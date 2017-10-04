#include "ars-emu.hh"
#include "teg.hh"
#include "controller.hh"

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
  int keybindings[NUM_PLAYERS][NUM_BUTTONS][MAX_KEYS_PER_BUTTON];
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
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    if(keyIsBeingBound()) {
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
          keybindings[binding_player][binding_button][k] = NO_SCANCODE;
        }
        binding_player = -1;
        return true;
      default:
        for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
          if(evt.key.keysym.scancode == keybindings[binding_player][binding_button][k]) {
            keybindings[binding_player][binding_button][0] = evt.key.keysym.scancode;
            for(k = 1; k < MAX_KEYS_PER_BUTTON; ++k)
              keybindings[binding_player][binding_button][k] = NO_SCANCODE;
            binding_player = -1;
            return true;
          }
        }
        for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
          if(keybindings[binding_player][binding_button][k] == NO_SCANCODE) {
            keybindings[binding_player][binding_button][k] = evt.key.keysym.scancode;
            binding_player = -1;
            return true;
          }
        }
        for(int k = 0; k < MAX_KEYS_PER_BUTTON - 1; ++k) {
          keybindings[binding_player][binding_button][k]
            = keybindings[binding_player][binding_button][k+1];
        }
        keybindings[binding_player][binding_button][MAX_KEYS_PER_BUTTON-1]
          = evt.key.keysym.scancode;
        binding_player = -1;
        return true;
      }
    }
    else {
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
      }
    }
    break;
  }
  return false;
}

void Controller::initControllers() {
  map_expansion(0x240, std::make_unique<StandardController>(P1));
  map_expansion(0x241, std::make_unique<StandardController>(P2));
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
    if(keybindings[player][button][k] != NO_SCANCODE) {
      if(!ret.empty()) ret += sn.Get("MULTI_KEY_BINDING_SEP"_Key);
      ret += getScancodeName(keybindings[player][button][k]);
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
