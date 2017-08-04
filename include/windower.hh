#ifndef WINDOWERHH
#define WINDOWERHH

#include "teg.hh"

#include "SDL.h"

namespace Windower {
  extern Uint32 UPDATE_EVENT;
  void Register(Uint32 windowid, void(*)(SDL_Event&));
  void Unregister(Uint32 windowid);
  // Returns true if Windower handled the event
  bool HandleEvent(SDL_Event&);
  void Update();
}

#endif
