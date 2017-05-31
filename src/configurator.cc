#include "ars-emu.hh"
#include "utfit.hh"
#include "font.hh"
#include "menu.hh"

#include <array>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace {
  namespace Command {
    enum {
      ECHO_TWO = 0x00,
      INIT_MENU_SYSTEM = 0x01,
      CLEANUP_MENU_SYSTEM = 0x02,
      RENDER_MENU_TITLE = 0x03,
      GET_MENU_ITEM_COUNT = 0x04,
      GET_MENU_ITEM_TYPE = 0x05,
      RENDER_MENU_ITEM_LABEL = 0x06,
      ACTIVATE_BUTTON = 0x07,
      ESCAPE_MENU = 0x08,
      BIND_KEY = 0x09,
      REVERSE_SELECTION = 0x0A,
      GET_MENU_COOKIE = 0x0B,
      NOP = 0xFF
    };
  }
  constexpr int MAX_RENDERED_COLUMNS = 28;
  std::vector<uint8_t> reply, params;
  size_t reply_byte = 0;
  uint8_t command_in_progress = Command::NOP;
  uint8_t awaited_parameter_count = 0;
  void renderString(std::string str) {
    // std::cout << "renderString: \"" << str << "\"\n";
    auto length_index = reply.size();
    reply.push_back(0);
    int length = 0;
    auto cit = str.cbegin();
    while(cit != str.cend()) {
      uint32_t c = getNextCodePoint(cit, str.cend());
      auto& glyph = Font::GetGlyph(c);
      if(length >= MAX_RENDERED_COLUMNS - glyph.wide) break;
      reply.insert(reply.end(),
                   glyph.tiles, glyph.tiles + (glyph.wide ? 32 : 16));
      length += glyph.wide ? 2 : 1;
    }
    SDL_assert(length <= MAX_RENDERED_COLUMNS);
    reply[length_index] = length;
  }
  /* This is smaller and faster than a map... */
  uint8_t getParamCount(uint8_t cmd) {
    switch(cmd) {
    case Command::ECHO_TWO: return 2;
    case Command::GET_MENU_ITEM_TYPE: return 1;
    case Command::RENDER_MENU_ITEM_LABEL: return 1;
    case Command::ACTIVATE_BUTTON: return 1;
    case Command::BIND_KEY: return 1;
    case Command::REVERSE_SELECTION: return 1;
    }
    return 0;
  }
  void execute() {
    switch(command_in_progress) {
    case Command::ECHO_TWO:
      SDL_assert(params.size() == 2);
      reply.push_back(params[0]);
      reply.push_back(params[1]);
      break;
    case Command::INIT_MENU_SYSTEM:
      SDL_assert(params.size() == 0);
      Menu::tearDownMenus(false);
      Menu::addNewMenu(Menu::createMainMenu());
      break;
    case Command::CLEANUP_MENU_SYSTEM:
      SDL_assert(params.size() == 0);
      Menu::tearDownMenus(true);
      break;
    case Command::RENDER_MENU_TITLE:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 0);
      renderString(Menu::getTopMenu()->getTitle());
      break;
    case Command::GET_MENU_ITEM_COUNT:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 0);
      reply.push_back(Menu::getTopMenu()->getItemCount());
      break;
    case Command::GET_MENU_ITEM_TYPE:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 1);
      reply.push_back(Menu::getTopMenu()->getItem(params[0])->getType());
      break;
    case Command::RENDER_MENU_ITEM_LABEL:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 1);
      {
        auto labels = Menu::getTopMenu()->getItem(params[0])->getLabels();
        if(sn.Get("CONFIG_MENU_DIRECTION"_Key) == "RTL") {
          std::swap(labels[0], labels[1]);
          std::swap(labels[2], labels[3]);
        }
        reply.push_back(0); // placeholder
        for(int n = 0; n < 4; ++n) {
          if(!labels[n].empty()) {
            reply[0] |= 1<<n;
            renderString(labels[n]);
          }
        }
      }
      break;
    case Command::ACTIVATE_BUTTON:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 1);
      Menu::getTopMenu()->getItem(params[0])->activate();
      break;
    case Command::ESCAPE_MENU:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 0);
      Menu::weaklyBackOutOfMenu();
      break;
    case Command::BIND_KEY:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 1);
      {
        std::string binding_instructions = Menu::getTopMenu()->getItem(params[0])->getKeyBindingInstructions();
        std::vector<std::string> lines;
        auto sit = binding_instructions.cbegin();
        while(sit < binding_instructions.cend()) {
          auto eit = std::find(sit, binding_instructions.cend(), '\n');
          lines.emplace_back(sit, eit);
          sit = eit + 1;
        }
        uint8_t a, b;
        Menu::getTopMenu()->getItem(params[0])->beginBindingKey(a, b);
        reply.push_back(lines.size());
        for(auto& line : lines) {
        renderString(line);
        }
        reply.push_back(a);
        reply.push_back(b);
      }
      break;
    case Command::REVERSE_SELECTION:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 1);
      Menu::getTopMenu()->getItem(params[0])->reverseActivate();
      break;
    case Command::GET_MENU_COOKIE:
      if(Menu::getTopMenu() == nullptr) return;
      SDL_assert(params.size() == 0);
      reply.push_back(Menu::getCookie());
      break;
    case Command::NOP:
      /* do nothing */
      break;
    }
    command_in_progress = Command::NOP;
    awaited_parameter_count = 0;
  }
}

uint8_t ARS::Configurator::read() {
  if(reply_byte < reply.size())
    return reply[reply_byte++];
  else if(ARS::Controller::keyIsBeingBound())
    return 0;
  else
    return 0xEC;
}

void ARS::Configurator::write(uint8_t v) {
  reply.clear();
  reply_byte = 0;
  if(command_in_progress == Command::NOP) {
    command_in_progress = v;
    awaited_parameter_count = getParamCount(v);
    params.clear();
  }
  else {
    SDL_assert(params.size() < awaited_parameter_count);
    params.push_back(v);
  }
  if(params.size() == awaited_parameter_count)
    execute();
}
