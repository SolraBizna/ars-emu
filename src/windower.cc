#include "windower.hh"

#include <unordered_map>

namespace {
  std::unordered_map<Uint32, void(*)(SDL_Event&)> handlers;
}

Uint32 Windower::UPDATE_EVENT = 0;

void Windower::Register(Uint32 windowid, void(*handler)(SDL_Event&)) {
  handlers[windowid] = handler;
}

void Windower::Unregister(Uint32 windowid) {
  handlers.erase(windowid);
}

bool Windower::HandleEvent(SDL_Event& evt) {
  Uint32 target;
  switch(evt.type) {
  default:
    return false;
  case SDL_DROPFILE:
  case SDL_DROPTEXT:
  case SDL_DROPBEGIN:
  case SDL_DROPCOMPLETE:
    target = evt.drop.windowID;
    break;
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    target = evt.key.windowID;
    break;
  case SDL_MOUSEMOTION:
    target = evt.motion.windowID;
    break;
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    target = evt.button.windowID;
    break;
  case SDL_MOUSEWHEEL:
    target = evt.button.windowID;
    break;
  case SDL_TEXTEDITING:
  case SDL_TEXTINPUT:
    target = evt.text.windowID;
    break;
  case SDL_WINDOWEVENT:
    target = evt.window.windowID;
    break;
  }
  auto it = handlers.find(target);
  if(it != handlers.end()) {
    it->second(evt);
    return true;
  }
  else if(evt.type == SDL_WINDOWEVENT
          && evt.window.event == SDL_WINDOWEVENT_CLOSE) {
    evt.type = SDL_QUIT;
    // fall through
  }
  return false;
}

void Windower::Update() {
  if(UPDATE_EVENT == 0) UPDATE_EVENT = SDL_RegisterEvents(1); // assume success
  for(auto pair : handlers) {
    SDL_Event evt;
    evt.type = UPDATE_EVENT;
    evt.user.timestamp = SDL_GetTicks();
    pair.second(evt);
  }
}
