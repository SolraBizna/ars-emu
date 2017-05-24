#include "prefs.hh"

#include <algorithm>

namespace {
  std::vector<PrefsLogic*>& getLogicList() {
    static std::vector<PrefsLogic*> logics;
    return logics;
  }
}

PrefsLogic::PrefsLogic() {
  getLogicList().push_back(this);
}

PrefsLogic::~PrefsLogic() {
  auto it = std::find(getLogicList().begin(), getLogicList().end(), this);
  if(it != getLogicList().end()) getLogicList().erase(it);
}

void PrefsLogic::LoadAll() {
  for(auto p : getLogicList()) p->Load();
}

void PrefsLogic::SaveAll() {
  for(auto p : getLogicList()) p->Save();
}

void PrefsLogic::DefaultsAll() {
  for(auto p : getLogicList()) { p->Defaults(); }
}

