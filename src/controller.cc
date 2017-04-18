#include "ars-emu.hh"

using namespace ARS;

namespace {
  enum Players { P1=0,P2,NUM_PLAYERS };
  enum Buttons { A,B,C,LEFT,UP,RIGHT,DOWN,PAUSE,NUM_BUTTONS };
  constexpr int MAX_KEYS_PER_BUTTON = 3;
  SDL_Keycode keymappings[NUM_PLAYERS][NUM_BUTTONS][MAX_KEYS_PER_BUTTON] = {
    /* P1 */
    {
      /* A */
      {SDLK_a, SDLK_z, -1},
      /* B */
      {SDLK_s, SDLK_x, -1},
      /* C */
      {SDLK_d, SDLK_c, -1},
      /* LEFT */
      {-1, -1, -1},
      /* UP */
      {-1, -1, -1},
      /* RIGHT */
      {-1, -1, -1},
      /* DOWN */
      {-1, -1, -1},
      /* PAUSE */
      {SDLK_TAB, -1, -1},
    },
    /* P2 */
    {
      /* A */
      {SDLK_KP_0, -1, -1},
      /* B */
      {SDLK_KP_ENTER, -1, -1},
      /* C */
      {SDLK_KP_PERIOD, -1, -1},
      /* LEFT */
      {SDLK_KP_4, -1, -1},
      /* UP */
      {SDLK_KP_8, -1, -1},
      /* RIGHT */
      {SDLK_KP_6, -1, -1},
      /* DOWN */
      {SDLK_KP_2, -1, -1},
      /* PAUSE */
      {SDLK_KP_5, SDLK_NUMLOCKCLEAR, -1},
    },
  };
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
}

std::unique_ptr<Controller> ARS::controller1, ARS::controller2;

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
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    switch(evt.key.keysym.sym) {
    case SDLK_RETURN:
      buttonsPressed[0][Buttons::A] = evt.type == SDL_KEYDOWN;
      return true;
    case SDLK_ESCAPE:
      buttonsPressed[0][Buttons::PAUSE] = evt.type == SDL_KEYDOWN;
      return true;
    case SDLK_LEFT:
      buttonsPressed[0][Buttons::LEFT] = evt.type == SDL_KEYDOWN;
      return true;
    case SDLK_UP:
      buttonsPressed[0][Buttons::UP] = evt.type == SDL_KEYDOWN;
      return true;
    case SDLK_RIGHT:
      buttonsPressed[0][Buttons::RIGHT] = evt.type == SDL_KEYDOWN;
      return true;
    case SDLK_DOWN:
      buttonsPressed[0][Buttons::DOWN] = evt.type == SDL_KEYDOWN;
      return true;
    default:
      for(int p = 0; p < NUM_PLAYERS; ++p) {
        for(int b = 0; b < NUM_BUTTONS; ++b) {
          for(int k = 0; k < MAX_KEYS_PER_BUTTON; ++k) {
            if(evt.key.keysym.sym == keymappings[p][b][k]) {
              buttonsPressed[p][b] = evt.type == SDL_KEYDOWN;
              return true;
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
  controller1 = std::make_unique<StandardController>(P1);
  controller2 = std::make_unique<StandardController>(P2);
}
