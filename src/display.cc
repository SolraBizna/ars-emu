#include "display.hh"
#include "prefs.hh"
#include "config.hh"
#include "menu.hh"

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

bool Display::filterEvent(SDL_Event&) { return false; }

std::unique_ptr<Display>
Display::makeConfiguredDisplay() {
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
        ret = preferred_dd->constructor();
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
        ret = dd->constructor();
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
                                     ()> constructor,
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

const DisplayDescriptor* Display::getActiveDisplay() {
  auto& displays = getAvailableDisplays();
  for(auto dd : displays) {
      if(dd->identifier == preferred_display_id) {
        return dd;
      }
  }
  return displays[0];
}

std::shared_ptr<Menu> Menu::createVideoMenu() {
  std::vector<std::shared_ptr<Menu::Item> > items;
  std::vector<std::string> driver_names;
  auto& displays = Display::getAvailableDisplays();
  driver_names.reserve(displays.size());
  std::shared_ptr<size_t> selected_driver = std::make_shared<size_t>(0);
  for(size_t n = 0; n < displays.size(); ++n) {
    auto dd = displays[n];
    if(preferred_display_id == dd->identifier) {
      *selected_driver = n;
    }
    driver_names.emplace_back(sn.Get(dd->pretty_name_key));
  }
  items.emplace_back(new Menu::Selector(sn.Get("VIDEO_DRIVER"_Key),
                                        driver_names,
                                        *selected_driver,
                                        [selected_driver](size_t opt) {
                                          *selected_driver = opt;
                                          Display::setActiveDisplay(getAvailableDisplays()[*selected_driver]);
                                          display.reset();
                                          display = Display::makeConfiguredDisplay();
                                        }));
  bool have_had_divider = false;
  for(auto dd : displays) {
    if(dd->configurator) {
      if(!have_had_divider) {
        items.emplace_back(new Menu::Divider());
        have_had_divider = true;
      }
      items.emplace_back(new Menu::Submenu(sn.Get("CONFIGURE_VIDEO_DRIVER"_Key,
                                                  {sn.Get(dd->pretty_name_key)}
                                                  ),
                                           dd->configurator));

    }
  }
  items.emplace_back(new Menu::Divider());
  items.emplace_back(new Menu::BackButton(sn.Get("GENERIC_MENU_FINISHED_LABEL"_Key)));
  return std::make_shared<Menu>(sn.Get("VIDEO_MENU_TITLE"_Key), items);
}
