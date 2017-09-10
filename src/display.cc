#include "display.hh"
#include "prefs.hh"
#include "config.hh"

#include <assert.h>
#include <algorithm>

using namespace ARS;

namespace {
  std::vector<const DisplayDescriptor*>& getAvailableDisplays() {
    static std::vector<const DisplayDescriptor*> ret;
    return ret;
  }
  bool displays_sorted = false;
  std::string preferred_display_id;
  const Config::Element elements[] = {
    {"preferred_display_id", preferred_display_id},
  };
  class DisplaySystemPrefsLogic : public PrefsLogic {
  protected:
    void Load() override {
      Config::Read("Display.utxt",
                   elements, elementcount(elements));
    }
    void Save() override {
      Config::Write("Display.utxt",
                    elements, elementcount(elements));
    }
    void Defaults() override {
      preferred_display_id = "";
    }
  } displaySystemPrefsLogic;
  void sortDisplays() {
    assert(!displays_sorted);
    std::sort(getAvailableDisplays().begin(),
              getAvailableDisplays().end(),
              [](const DisplayDescriptor* left,
                 const DisplayDescriptor* right) -> bool {
                if(left->priority > right->priority) return true;
                else if(left->priority < right->priority) return false;
                else return left->identifier < right->identifier;
              });
    displays_sorted = true;
  }
}

Display::~Display() {}

std::unique_ptr<Display>
Display::makeConfiguredDisplay(const std::string& window_title) {
  if(!displays_sorted) sortDisplays();
  bool errors_were_made = false;
  std::unique_ptr<Display> ret;
  std::string display_name;
  if(!preferred_display_id.empty()) {
    // Attempt to set up the preferred display type, if we have one
    const DisplayDescriptor* preferred_dd = nullptr;
    for(auto dd : getAvailableDisplays()) {
      if(dd->identifier == preferred_display_id) {
        preferred_dd = dd;
        break;
      }
    }
    if(preferred_dd != nullptr) {
      try {
        ret = preferred_dd->constructor(window_title);
        assert(ret);
        display_name = sn.Get(preferred_dd->pretty_name_key);
      }
      catch(std::string& e) {
        std::cerr << sn.Get("DISPLAY_FAILURE"_Key,
                            {sn.Get(preferred_dd->pretty_name_key), e});
      }
    }
    else {
      std::cerr << sn.Get("UNKNOWN_DISPLAY_TYPE"_Key);
      errors_were_made = true;
    }
  }
  if(!ret) {
    // Try setting up every display in the list
    for(auto& dd : getAvailableDisplays()) {
      // we will already have tried the preferred display if one is set
      // so skip it
      if(preferred_display_id == dd->identifier) continue;
      try {
        ret = dd->constructor(window_title);
        assert(ret);
        display_name = sn.Get(dd->pretty_name_key);
        break;
      }
      catch(std::string& e) {
        std::cerr << sn.Get("DISPLAY_FAILURE"_Key,
                            {sn.Get(dd->pretty_name_key), e});
      }
    }
  }
  if(!ret) {
    die("%s",sn.Get("TOTAL_DISPLAY_FAILURE"_Key).c_str());
  }
  if(errors_were_made) {
    ui << sn.Get("DISPLAY_SETUP_ERRORS"_Key, {display_name}) << ui;
  }
  return ret;
}

DisplayDescriptor::DisplayDescriptor(const std::string& identifier,
                                     const SN::ConstKey& pretty_name_key,
                                     int priority,
                                     std::function<std::unique_ptr<Display>
                                     (const std::string&)> constructor,
                                     std::function<std::shared_ptr<Menu>()>
                                     configurator)
  : identifier(identifier), pretty_name_key(pretty_name_key),
    priority(priority), constructor(constructor), configurator(configurator) {
  assert(!displays_sorted);
  getAvailableDisplays().push_back(this);
}

const std::vector<const DisplayDescriptor*>& Display::getAvailableDisplays() {
  if(!displays_sorted) sortDisplays();
  return ::getAvailableDisplays();
}

void Display::setActiveDisplay(const DisplayDescriptor* nu) {
  if(nu == nullptr) preferred_display_id = "";
  else preferred_display_id = nu->identifier;
}
