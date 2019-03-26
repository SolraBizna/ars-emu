#ifndef DISPLAYHH
#define DISPLAYHH

#include "ppu.hh"

class Menu;

namespace ARS {
  class DisplayDescriptor;
  class Display {
    Display(const Display&) = delete;
    Display(Display&&) = delete;
    Display& operator=(const Display&) = delete;
    Display& operator=(Display&&) = delete;
  protected:
    Display() {}
  public:
    virtual ~Display();
    virtual SDL_Window* getWindow() const = 0;
    virtual void update(const ARS::PPU::raw_screen& src) = 0;
    // return true if the event was fully handled
    virtual bool filterEvent(SDL_Event&);
    // return true if the coordinates are "in frame"
    virtual bool windowSpaceToVirtualScreenSpace(int& x, int& y) = 0;
    static std::unique_ptr<Display> makeConfiguredDisplay();
    static std::unique_ptr<Display> makeSafeModeDisplay();
    static const std::vector<const DisplayDescriptor*>& getAvailableDisplays();
    static void setActiveDisplay(const DisplayDescriptor*);
    static const DisplayDescriptor* getActiveDisplay();
  };
  // Safe Mode has 0 priority. Highly hardware-accelerated modes should have
  // >100 priority. Dangerous modes should have negative priority.
  class DisplayDescriptor {
    DisplayDescriptor& operator=(const DisplayDescriptor&) = delete;
    DisplayDescriptor& operator=(DisplayDescriptor&&) = delete;
    DisplayDescriptor(const DisplayDescriptor&) = delete;
    DisplayDescriptor(DisplayDescriptor&&) = delete;
  public:
    const std::string identifier;
    const SN::ConstKey pretty_name_key;
    const int priority;
    const std::function<std::unique_ptr<Display>()> constructor;
    const std::function<std::shared_ptr<Menu>()> configurator;
    DisplayDescriptor(const std::string& identifier,
                      const SN::ConstKey& pretty_name_key,
                      int priority,
                      std::function<std::unique_ptr<Display>()> constructor,
                      std::function<std::shared_ptr<Menu>()>
                      configurator = nullptr);
  };
}

#endif
