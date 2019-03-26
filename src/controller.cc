#include "ars-emu.hh"
#include "teg.hh"
#include "controller.hh"
#include "display.hh"

#include "prefs.hh"
#include "config.hh"

using namespace ARS;

namespace {
  enum Players { P1=0,P2,NUM_PLAYERS };
  enum Buttons { A,B,C,LEFT,UP,RIGHT,DOWN,PAUSE,NUM_BUTTONS };
  constexpr int MAX_KEYS_PER_BUTTON = 2;
  constexpr int NO_SCANCODE = -1;
  constexpr SDL_JoystickID NO_JOYSTICK = -1;
  struct BoundGamepad {
    SDL_JoystickID jid = NO_JOYSTICK;
    SDL_GameController* g;
    int oid;
  } bound_gamepads[NUM_PLAYERS];
  int binding_player = -1, binding_button;
  uint8_t mouseButtons = 0;
  uint8_t mouseButtonStick = 0; // to ensure that every press lasts at least one poll
  uint16_t mouseXRel = 0x80;
  uint16_t mouseYRel = 0x80;
  uint8_t mouseX = 255, mouseY = 255;
  int mouseScroll = 0;
  bool mouseInFrame = false;
  const std::unordered_map<SDL_Scancode, uint8_t> scancodemap = {
    {SDL_SCANCODE_LSHIFT, 0x01},
    {SDL_SCANCODE_LALT, 0x02},
    {SDL_SCANCODE_LGUI, 0x03},
    {SDL_SCANCODE_LCTRL, 0x04},
    {SDL_SCANCODE_RCTRL, 0x04},
    {SDL_SCANCODE_RSHIFT, 0x05},
    {SDL_SCANCODE_RALT, 0x06},
    {SDL_SCANCODE_RGUI, 0x07},
    {SDL_SCANCODE_ESCAPE, 0x08},
    {SDL_SCANCODE_BACKSPACE, 0x09},
    {SDL_SCANCODE_DELETE, 0x0A},
    {SDL_SCANCODE_INSERT, 0x0B},
    {SDL_SCANCODE_PAUSE, 0x0C},
    {SDL_SCANCODE_CLEAR, 0x0C},
    {SDL_SCANCODE_HOME, 0x0D},
    {SDL_SCANCODE_END, 0x0E},
    {SDL_SCANCODE_PAGEUP, 0x0F},
    {SDL_SCANCODE_PAGEDOWN, 0x10},
    {SDL_SCANCODE_LEFT, 0x11},
    {SDL_SCANCODE_UP, 0x12},
    {SDL_SCANCODE_RIGHT, 0x13},
    {SDL_SCANCODE_DOWN, 0x14},
    {SDL_SCANCODE_RETURN, 0x15},
    {SDL_SCANCODE_TAB, 0x16},
    {SDL_SCANCODE_SPACE, 0x17},
    {SDL_SCANCODE_GRAVE, 0x18},
    {SDL_SCANCODE_1, 0x19},
    {SDL_SCANCODE_Q, 0x1A},
    {SDL_SCANCODE_A, 0x1B},
    {SDL_SCANCODE_Z, 0x1C},
    {SDL_SCANCODE_2, 0x1D},
    {SDL_SCANCODE_W, 0x1E},
    {SDL_SCANCODE_S, 0x1F},
    {SDL_SCANCODE_X, 0x20},
    {SDL_SCANCODE_3, 0x21},
    {SDL_SCANCODE_E, 0x22},
    {SDL_SCANCODE_D, 0x23},
    {SDL_SCANCODE_C, 0x24},
    {SDL_SCANCODE_4, 0x25},
    {SDL_SCANCODE_R, 0x26},
    {SDL_SCANCODE_F, 0x27},
    {SDL_SCANCODE_V, 0x28},
    {SDL_SCANCODE_5, 0x29},
    {SDL_SCANCODE_T, 0x2A},
    {SDL_SCANCODE_G, 0x2B},
    {SDL_SCANCODE_B, 0x2C},
    {SDL_SCANCODE_6, 0x2D},
    {SDL_SCANCODE_Y, 0x2E},
    {SDL_SCANCODE_H, 0x2F},
    {SDL_SCANCODE_N, 0x30},
    {SDL_SCANCODE_7, 0x31},
    {SDL_SCANCODE_U, 0x32},
    {SDL_SCANCODE_J, 0x33},
    {SDL_SCANCODE_M, 0x34},
    {SDL_SCANCODE_8, 0x35},
    {SDL_SCANCODE_I, 0x36},
    {SDL_SCANCODE_K, 0x37},
    {SDL_SCANCODE_COMMA, 0x38},
    {SDL_SCANCODE_9, 0x39},
    {SDL_SCANCODE_O, 0x3A},
    {SDL_SCANCODE_L, 0x3B},
    {SDL_SCANCODE_PERIOD, 0x3C},
    {SDL_SCANCODE_0, 0x3D},
    {SDL_SCANCODE_P, 0x3E},
    {SDL_SCANCODE_SEMICOLON, 0x3F},
    {SDL_SCANCODE_SLASH, 0x40},
    {SDL_SCANCODE_MINUS, 0x41},
    {SDL_SCANCODE_LEFTBRACKET, 0x42},
    {SDL_SCANCODE_APOSTROPHE, 0x43},
    {SDL_SCANCODE_BACKSLASH, 0x44},
    {SDL_SCANCODE_NONUSBACKSLASH, 0x44},
    {SDL_SCANCODE_EQUALS, 0x45},
    {SDL_SCANCODE_RIGHTBRACKET, 0x46},
  };
  const int NUM_SCANCODES = 0x46;
  const int MIN_SCANCODE = 0x01;
  uint8_t scancode_statuses[NUM_SCANCODES] = {};
  std::vector<uint8_t> keyboard_events;
  size_t keyboard_event_front = 0;
  const int DEFAULT_KEYBINDINGS[NUM_PLAYERS][NUM_BUTTONS][MAX_KEYS_PER_BUTTON]
  = {
    /* P1 */
    {
      /* A */
      {SDL_SCANCODE_A, SDL_SCANCODE_Z},
      /* B */
      {SDL_SCANCODE_S, SDL_SCANCODE_X},
      /* C */
      {SDL_SCANCODE_D, SDL_SCANCODE_C},
      /* LEFT */
      {NO_SCANCODE, NO_SCANCODE},
      /* UP */
      {NO_SCANCODE, NO_SCANCODE},
      /* RIGHT */
      {NO_SCANCODE, NO_SCANCODE},
      /* DOWN */
      {NO_SCANCODE, NO_SCANCODE},
      /* PAUSE */
      {SDL_SCANCODE_TAB, NO_SCANCODE},
    },
    /* P2 */
    {
      /* A */
      {SDL_SCANCODE_KP_0, NO_SCANCODE},
      /* B */
      {SDL_SCANCODE_KP_ENTER, NO_SCANCODE},
      /* C */
      {SDL_SCANCODE_KP_PERIOD, NO_SCANCODE},
      /* LEFT */
      {SDL_SCANCODE_KP_4, NO_SCANCODE},
      /* UP */
      {SDL_SCANCODE_KP_8, NO_SCANCODE},
      /* RIGHT */
      {SDL_SCANCODE_KP_6, NO_SCANCODE},
      /* DOWN */
      {SDL_SCANCODE_KP_2, NO_SCANCODE},
      /* PAUSE */
      {SDL_SCANCODE_KP_5, SDL_SCANCODE_NUMLOCKCLEAR},
    },
  };
  const int DEFAULT_EMUKEYBINDINGS[NUM_EMULATOR_BUTTONS][MAX_KEYS_PER_BUTTON]={
    /* Reset */
    {SDL_SCANCODE_F8, NO_SCANCODE},
    /* Toggle Background */
    {SDL_SCANCODE_F1, NO_SCANCODE},
    /* Toggle Sprites */
    {SDL_SCANCODE_F2, NO_SCANCODE},
    /* Toggle Overlay */
    {SDL_SCANCODE_F3, NO_SCANCODE},
    /* Take Screenshot */
    {SDL_SCANCODE_F12, SDL_SCANCODE_PRINTSCREEN},
  };
  int keybindings[NUM_PLAYERS][NUM_BUTTONS][MAX_KEYS_PER_BUTTON];
  int emukeybindings[NUM_EMULATOR_BUTTONS][MAX_KEYS_PER_BUTTON];
  const Config::Element keybinding_elements[] = {
    {"P1_A",         keybindings[0][0][0]},
    {"P1_A_alt",     keybindings[0][0][1]},
    {"P1_B",         keybindings[0][1][0]},
    {"P1_B_alt",     keybindings[0][1][1]},
    {"P1_C",         keybindings[0][2][0]},
    {"P1_C_alt",     keybindings[0][2][1]},
    {"P1_left",      keybindings[0][3][0]},
    {"P1_left_alt",  keybindings[0][3][1]},
    {"P1_up",        keybindings[0][4][0]},
    {"P1_up_alt",    keybindings[0][4][1]},
    {"P1_right",     keybindings[0][5][0]},
    {"P1_right_alt", keybindings[0][5][1]},
    {"P1_down",      keybindings[0][6][0]},
    {"P1_down_alt",  keybindings[0][6][1]},
    {"P1_pause",     keybindings[0][7][0]},
    {"P1_pause_alt", keybindings[0][7][1]},
    {"P2_A",         keybindings[1][0][0]},
    {"P2_A_alt",     keybindings[1][0][1]},
    {"P2_B",         keybindings[1][1][0]},
    {"P2_B_alt",     keybindings[1][1][1]},
    {"P2_C",         keybindings[1][2][0]},
    {"P2_C_alt",     keybindings[1][2][1]},
    {"P2_left",      keybindings[1][3][0]},
    {"P2_left_alt",  keybindings[1][3][1]},
    {"P2_up",        keybindings[1][4][0]},
    {"P2_up_alt",    keybindings[1][4][1]},
    {"P2_right",     keybindings[1][5][0]},
    {"P2_right_alt", keybindings[1][5][1]},
    {"P2_down",      keybindings[1][6][0]},
    {"P2_down_alt",  keybindings[1][6][1]},
    {"P2_pause",     keybindings[1][7][0]},
    {"P2_pause_alt", keybindings[1][7][1]},
    {"EMU_reset",          emukeybindings[0][0]},
    {"EMU_reset_alt",      emukeybindings[0][1]},
    {"EMU_toggle_bg",      emukeybindings[1][0]},
    {"EMU_toggle_bg_alt",  emukeybindings[1][1]},
    {"EMU_toggle_sp",      emukeybindings[2][0]},
    {"EMU_toggle_sp_alt",  emukeybindings[2][1]},
    {"EMU_toggle_ol",      emukeybindings[3][0]},
    {"EMU_toggle_ol_alt",  emukeybindings[3][1]},
    {"EMU_screenshot",     emukeybindings[4][0]},
    {"EMU_screenshot_alt", emukeybindings[4][1]},
  };
  class KBPrefsLogic : public PrefsLogic {
  protected:
    void Load() override {
      Config::Read("Keybindings.utxt",
                   keybinding_elements, elementcount(keybinding_elements));
    }
    void Save() override {
      Config::Write("Keybindings.utxt",
                    keybinding_elements, elementcount(keybinding_elements));
    }
    void Defaults() override {
      memcpy(keybindings, DEFAULT_KEYBINDINGS, sizeof(DEFAULT_KEYBINDINGS));
      memcpy(emukeybindings, DEFAULT_EMUKEYBINDINGS,
             sizeof(DEFAULT_EMUKEYBINDINGS));
    }
  } kbPrefsLogic;
  bool buttonsPressed[NUM_PLAYERS][NUM_BUTTONS] = {};
  class UnpluggedController : public Controller {
    uint8_t onStrobeFall(uint8_t) override { return 0; }
  public:
    UnpluggedController() {}
  };
  class StandardController : public Controller {
    const int player;
    uint8_t onStrobeFall(uint8_t curData) override {
      switch(curData&0x80) {
      case 0x00: {
        uint8_t ret = 0;
        if(buttonsPressed[player][A]) ret |= 1;
        if(buttonsPressed[player][B]) ret |= 2;
        if(buttonsPressed[player][C]) ret |= 4;
        if(buttonsPressed[player][LEFT]) ret |= 8;
        if(buttonsPressed[player][UP]) ret |= 16;
        if(buttonsPressed[player][RIGHT]) ret |= 32;
        if(buttonsPressed[player][DOWN]) ret |= 64;
        if(buttonsPressed[player][PAUSE]) ret |= 120;
        return ret;
      }
      default:
        /* NOTREACHED */
      case 0x80:
        return 0x01;
      }
    }
  public:
    StandardController(int player) : player(player) {}
  };
  class KeyboardController : public Controller {
    uint8_t onStrobeFall(uint8_t curData) override {
      switch(curData&0x80) {
      case 0x00: {
        if(!keyboard_events.empty()) {
          uint8_t ret = keyboard_events[keyboard_event_front++];
          if(keyboard_event_front >= keyboard_events.size()) {
            keyboard_event_front = 0;
            keyboard_events.clear();
          }
          return ret;
        }
        else return 0;
      }
      default:
        /* NOTREACHED */
      case 0x80:
        return 0x02;
      }
    }
  public:
    KeyboardController(int) {}
  };
  class MouseController : public Controller {
    uint8_t onStrobeFall(uint8_t curData) override {
      switch(curData&0xC0) {
      case 0x00: {
        uint8_t ret = (mouseButtons | mouseButtonStick) & 3;
        mouseButtonStick = 0;
        return ret;
      }
      default:
        /* NOTREACHED */
      case 0x80:
        return 0x03;
      case 0x40: {
        uint8_t ret = mouseXRel >> 8;
        mouseXRel &= 255;
        return ret;
      }
      case 0xC0: {
        uint8_t ret = mouseYRel >> 8;
        mouseYRel &= 255;
        return ret;
      }
      }
    }
  public:
    MouseController(int) {
      if(SDL_SetRelativeMouseMode(SDL_TRUE)) {
        // TODO: warn when this failure happens
      }
    }
  };
  class LightPenController : public Controller {
    uint8_t onStrobeFall(uint8_t curData) override {
      switch(curData&0xC0) {
      case 0x00: {
        uint8_t ret = ((mouseButtons | mouseButtonStick) & 3) | (mouseInFrame ? 128 : 0);
        mouseButtonStick = 0;
        return ret;
      }
      default:
        /* NOTREACHED */
      case 0x80:
        return 0x04;
      case 0x40: return mouseX;
      case 0xC0: return mouseY;
      }
    }
  public:
    LightPenController(int) {}
  };
  class LightGunController : public Controller {
    const int player;
    int selector = 3;
    uint8_t onStrobeFall(uint8_t curData) override {
      int newSelector = selector + mouseScroll;
      mouseScroll = 0;
      if(newSelector > 3) newSelector = 3;
      else if(newSelector < 0) newSelector = 0;
      if(newSelector != selector) {
        selector = newSelector;
          std::string player_name = TEG::format("CONTROLLER_P%i_NAME",player+1);
          SN::ConstKey player_key(player_name.data(), player_name.length());
          std::string selector_name = TEG::format("LIGHT_GUN_SELECTOR_%i",selector);
          SN::ConstKey selector_key(selector_name.data(), selector_name.length());
          ui << sn.Get("LIGHT_GUN_SELECTOR_CHANGED"_Key, {sn.Get(player_key), sn.Get(selector_key)}) << ui;
      }
      switch(curData&0xC0) {
      case 0x00: {
        uint8_t ret = ((mouseButtons | mouseButtonStick) & 7) | (8 << selector) | (mouseInFrame ? 128 : 0);
        mouseButtonStick = 0;
        return ret;
      }
      default:
        /* NOTREACHED */
      case 0x80:
        return 0x04;
      case 0x40: return mouseX;
      case 0xC0: return mouseY;
      }
    }
  public:
    LightGunController(int player) : player(player) {}
  };
  std::string getScancodeName(int code) {
    SDL_Keycode key = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(code));
    if(key == SDLK_UNKNOWN) return sn.Get("UNKNOWN_SCANCODE"_Key,
                                          {TEG::format("%04X", code)});
    auto name = SDL_GetKeyName(key);
    if(*name == 0) return sn.Get("UNKNOWN_SCANCODE"_Key,
                                 {TEG::format("%04X", code)});
    else return name;
  }
}

void Controller::output(uint8_t d) {
  dOut = d;
  strobeIsHigh = true;
}

uint8_t Controller::input() {
  if(strobeIsHigh) {
    strobeIsHigh = false;
    dIn = onStrobeFall(dOut);
    dataIsFresh = true;
  }
  if(dataIsFresh) {
    dataIsFresh = false;
    return dOut|dIn;
  }
  else return dIn;
}

bool Controller::filterEvent(SDL_Event& evt) {
  switch(evt.type) {
  case SDL_CONTROLLERDEVICEADDED:
    if(!SDL_IsGameController(evt.cdevice.which))
      ui << sn.Get("GAMEPAD_INVALID"_Key) << ui;
    else {
      for(int n = 0; n < NUM_PLAYERS; ++n) {
        if(bound_gamepads[n].jid != NO_JOYSTICK
           && bound_gamepads[n].oid == evt.cdevice.which)
          return true; // already bound
      }
      for(int n = 0; n < NUM_PLAYERS; ++n) {
        if(bound_gamepads[n].jid == NO_JOYSTICK) {
          bound_gamepads[n].g = SDL_GameControllerOpen(evt.cdevice.which);
          if(bound_gamepads[n].g == nullptr) {
            ui << sn.Get("GAMEPAD_INVALID"_Key) << ui;
            return true;
          }
          bound_gamepads[n].jid = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(bound_gamepads[n].g));
          bound_gamepads[n].oid = evt.cdevice.which;
          std::string player_name = TEG::format("CONTROLLER_P%i_NAME",n+1);
          SN::ConstKey player_key(player_name.data(), player_name.length());
          ui << sn.Get("GAMEPAD_BOUND"_Key, {sn.Get(player_key)}) << ui;
          return true;
        }
      }
      ui << sn.Get("GAMEPAD_IGNORED"_Key) << ui;
    }
    return true;
  case SDL_CONTROLLERDEVICEREMOVED:
    for(int n = 0; n < NUM_PLAYERS; ++n) {
      if(bound_gamepads[n].jid == evt.cdevice.which) {
        bound_gamepads[n].jid = NO_JOYSTICK;
        SDL_GameControllerClose(bound_gamepads[n].g);
        std::string player_name = TEG::format("CONTROLLER_P%i_NAME",n+1);
        SN::ConstKey player_key(player_name.data(), player_name.length());
        ui << sn.Get("GAMEPAD_UNBOUND"_Key, {sn.Get(player_key)}) << ui;
      }
    }
    break;
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
    for(int n = 0; n < NUM_PLAYERS; ++n) {
      if(bound_gamepads[n].jid == evt.cbutton.which) {
        switch(evt.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A:
          buttonsPressed[n][A] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_X:
          buttonsPressed[n][B] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_Y:
          buttonsPressed[n][C] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_START:
          buttonsPressed[n][PAUSE] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
          buttonsPressed[n][UP] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
          buttonsPressed[n][LEFT] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
          buttonsPressed[n][DOWN] = evt.cbutton.state == SDL_PRESSED;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
          buttonsPressed[n][RIGHT] = evt.cbutton.state == SDL_PRESSED;
          break;
        }
      }
    }
    break;
  case SDL_WINDOWEVENT:
    switch(evt.window.event) {
    case SDL_WINDOWEVENT_LEAVE: mouseInFrame = false; break;
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    switch(evt.button.button) {
    case SDL_BUTTON_LEFT: mouseButtons |= 1; mouseButtonStick |= 1; break;
    case SDL_BUTTON_RIGHT: mouseButtons |= 2; mouseButtonStick |= 2; break;
    case SDL_BUTTON_MIDDLE: mouseButtons |= 4; mouseButtonStick |= 4; break;
    case SDL_BUTTON_X1: mouseScroll -= 1; break;
    case SDL_BUTTON_X2: mouseScroll += 1; break;
    }
    break;
  case SDL_MOUSEBUTTONUP:
    switch(evt.button.button) {
    case SDL_BUTTON_LEFT: mouseButtons &= ~1; break;
    case SDL_BUTTON_RIGHT: mouseButtons &= ~2; break;
    case SDL_BUTTON_MIDDLE: mouseButtons &= ~4; break;
    }
    break;
  case SDL_MOUSEMOTION: {
    int x = evt.motion.x;
    int y = evt.motion.y;
    mouseInFrame = display->windowSpaceToVirtualScreenSpace(x, y);
    if(mouseInFrame) {
      mouseX = x;
      mouseY = y;
    }
    mouseXRel += evt.motion.xrel * 64;
    mouseYRel += evt.motion.yrel * 64;
    break;
  }
  case SDL_MOUSEWHEEL: {
    mouseScroll += evt.wheel.y;
    break;
  }
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    if(keyIsBeingBound()) {
      int* binding = binding_player < NUM_PLAYERS
        ? keybindings[binding_player][binding_button]
        : emukeybindings[binding_button];
      switch(evt.key.keysym.scancode) {
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_LEFT:
      case SDL_SCANCODE_UP:
      case SDL_SCANCODE_RIGHT:
      case SDL_SCANCODE_DOWN:
        /* No effect */
        return true;
      case SDL_SCANCODE_ESCAPE:
        binding_player = -1;
        return true;
      case SDL_SCANCODE_DELETE:
      case SDL_SCANCODE_BACKSPACE:
        for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
          binding[k] = NO_SCANCODE;
        }
        binding_player = -1;
        return true;
      default:
        for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
          if(evt.key.keysym.scancode == binding[k]) {
            binding[0] = evt.key.keysym.scancode;
            for(k = 1; k < MAX_KEYS_PER_BUTTON; ++k)
              binding[k] = NO_SCANCODE;
            binding_player = -1;
            return true;
          }
        }
        for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
          if(binding[k] == NO_SCANCODE) {
            binding[k] = evt.key.keysym.scancode;
            binding_player = -1;
            return true;
          }
        }
        for(int k = 0; k < MAX_KEYS_PER_BUTTON - 1; ++k) {
          binding[k] = binding[k+1];
        }
        binding[MAX_KEYS_PER_BUTTON-1] = evt.key.keysym.scancode;
        binding_player = -1;
        return true;
      }
    }
    else {
      if(!evt.key.repeat) {
        auto it = scancodemap.find(evt.key.keysym.scancode);
        if(it != scancodemap.end()) {
          int ent = it->second - MIN_SCANCODE;
          if(evt.key.state == SDL_PRESSED) {
            if(scancode_statuses[ent]++ == 0) {
              keyboard_events.push_back(it->second);
            }
          }
          else {
            if(--scancode_statuses[ent] == 0) {
              keyboard_events.push_back(it->second | 0x80);
            }
          }
        }
      }
      switch(evt.key.keysym.scancode) {
      case SDL_SCANCODE_RETURN:
        buttonsPressed[0][Buttons::A] = evt.type == SDL_KEYDOWN;
        return true;
      case SDL_SCANCODE_ESCAPE:
        buttonsPressed[0][Buttons::PAUSE] = evt.type == SDL_KEYDOWN;
        return true;
      case SDL_SCANCODE_LEFT:
        buttonsPressed[0][Buttons::LEFT] = evt.type == SDL_KEYDOWN;
        return true;
      case SDL_SCANCODE_UP:
        buttonsPressed[0][Buttons::UP] = evt.type == SDL_KEYDOWN;
        return true;
      case SDL_SCANCODE_RIGHT:
        buttonsPressed[0][Buttons::RIGHT] = evt.type == SDL_KEYDOWN;
        return true;
      case SDL_SCANCODE_DOWN:
        buttonsPressed[0][Buttons::DOWN] = evt.type == SDL_KEYDOWN;
        return true;
      default:
        for(int p = 0; p < NUM_PLAYERS; ++p) {
          for(int b = 0; b < NUM_BUTTONS; ++b) {
            for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
              if(evt.key.keysym.scancode == keybindings[p][b][k]) {
                buttonsPressed[p][b] = evt.type == SDL_KEYDOWN;
                return true;
              }
            }
          }
        }
        if(evt.type == SDL_KEYDOWN) {
          for(int b = 0; b < NUM_EMULATOR_BUTTONS; ++b) {
            for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
              if(evt.key.keysym.scancode == emukeybindings[b][k]) {
                handleEmulatorButtonPress(static_cast<EmulatorButton>(b));
                return true;
              }
            }
          }
        }
      }
    }
    break;
  }
  return false;
}

std::unique_ptr<Controller> make_controller(int player, Controller::Type type) {
  switch(type) {
  case Controller::Type::NONE:
    return std::make_unique<UnpluggedController>();
  case Controller::Type::AUTO: case Controller::Type::GAMEPAD:
    return std::make_unique<StandardController>(player);
  case Controller::Type::KEYBOARD:
    return std::make_unique<KeyboardController>(player);
  case Controller::Type::MOUSE:
    return std::make_unique<MouseController>(player);
  case Controller::Type::LIGHT_PEN:
    return std::make_unique<LightPenController>(player);
  case Controller::Type::LIGHT_GUN:
    return std::make_unique<LightGunController>(player);
  default:
    die("Controller type not implemented yet");
  }
}

void Controller::initControllers(Type port1, Type port2) {
  map_expansion(0x240, make_controller(P1, port1));
  map_expansion(0x241, make_controller(P2, port2));
  for(int jid = 0; jid < SDL_NumJoysticks(); ++jid) {
    if(SDL_IsGameController(jid)) {
      SDL_Event evt;
      evt.type = SDL_CONTROLLERDEVICEADDED;
      evt.cdevice.which = jid;
      filterEvent(evt);
    }
  }
}

std::string ARS::Controller::getNameOfHardKey(int player, int button) {
  /* TODO: less-hardcode the hardcoded keys */
  if(player != 0) return "";
  switch(button) {
  case A: return getScancodeName(SDL_SCANCODE_RETURN);
  case PAUSE: return getScancodeName(SDL_SCANCODE_ESCAPE);
  case LEFT: return getScancodeName(SDL_SCANCODE_LEFT);
  case RIGHT: return getScancodeName(SDL_SCANCODE_RIGHT);
  case UP: return getScancodeName(SDL_SCANCODE_UP);
  case DOWN: return getScancodeName(SDL_SCANCODE_DOWN);
  default: return "";
  }
}

std::string ARS::Controller::getNamesOfBoundKeys(int player, int button) {
  std::string ret = getNameOfHardKey(player, button);
  for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
    int& binding = player < NUM_PLAYERS ? keybindings[player][button][k]
      : emukeybindings[button][k];
    if(binding != NO_SCANCODE) {
      if(!ret.empty()) ret += sn.Get("MULTI_KEY_BINDING_SEP"_Key);
      ret += getScancodeName(binding);
    }
  }
  if(ret.empty()) return sn.Get("NO_KEY_BINDING"_Key);
  else return ret;
}

void ARS::Controller::startBindingKey(int player, int button) {
  binding_player = player;
  binding_button = button;
}

bool ARS::Controller::keyIsBeingBound() {
  return binding_player >= 0;
}

Controller::Type Controller::TypeFromString(const std::string& str) {
  if(str == "none") return Type::NONE;
  else if(str == "auto") return Type::AUTO;
  else if(str == "gamepad" || str == "controller") return Type::GAMEPAD;
  else if(str == "keyboard") return Type::KEYBOARD;
  else if(str == "mouse") return Type::MOUSE;
  else if(str == "light-pen") return Type::LIGHT_PEN;
  else if(str == "light-gun") return Type::LIGHT_GUN;
  else return Type::INVALID;
}
