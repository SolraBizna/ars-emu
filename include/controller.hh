#ifndef CONTROLLERHH
#define CONTROLLERHH

#include "ars-emu.hh"

#include "expansions.hh"

namespace ARS {
  class Controller : public Expansion {
    uint8_t dOut, dIn;
    bool dataIsFresh, strobeIsHigh;
  protected:
    Controller() : strobeIsHigh(true) {}
    virtual uint8_t onStrobeFall(uint8_t curData) = 0;
  public:
    virtual ~Controller() {}
    virtual void output(uint8_t d);
    virtual uint8_t input();
    static void initControllers();
    /* returns true if the event was fully handled, false if the event needs
       further handling (it may have been modified in the mean time) */
    static bool filterEvent(SDL_Event&);
    static std::string getNameOfHardKey(int player, int button);
    static std::string getNamesOfBoundKeys(int player, int button);
    static void startBindingKey(int player, int button);
    static bool keyIsBeingBound();
    static std::string getKeyBindingInstructions(int player, int button);
  };
}

#endif
