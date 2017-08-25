#include "menu.hh"
#include "controller.hh"

#include <vector>
#include <algorithm>
#include <array>

namespace {
  uint8_t cookie = 0;
  class Dummy : public Menu::Item {
  public:
    uint8_t getType() const override { return 0xFF; }
    std::array<std::string,4> getLabels() const override { return {}; }
  };
  std::shared_ptr<Dummy> dummyItem = std::make_shared<Dummy>();
  std::shared_ptr<Menu> emptyMenu = std::make_shared<Menu>();
}

std::vector<std::shared_ptr<Menu> > Menu::menus;

Menu::Item::~Item() {}
void Menu::Item::activate() {}
void Menu::Item::reverseActivate() {}
void Menu::Item::beginBindingKey(uint8_t& a, uint8_t& b) {
  b = a = 0;
}
std::string Menu::Item::getKeyBindingInstructions() const { return ""; }

Menu::BackButton::BackButton(std::string label)
  : Button(std::move(label), []() {Menu::backOutOfMenu();}) {}

uint8_t Menu::Divider::getType() const { return 0x00; }
uint8_t Menu::Label::getType() const { return 0x01; }
uint8_t Menu::Button::getType() const { return 0x02; }
uint8_t Menu::BackButton::getType() const { return 0x03; }
uint8_t Menu::ApplyButton::getType() const { return 0x04; }
uint8_t Menu::SaveButton::getType() const { return 0x05; }
uint8_t Menu::Submenu::getType() const { return 0x06; }
uint8_t Menu::Selector::getType() const { return 0x07; }
uint8_t Menu::KeyConfig::getType() const { return 0x08; }

std::array<std::string,4> Menu::Divider::getLabels() const {
  return {{}};
}
std::array<std::string,4> Menu::Label::getLabels() const {
  return {{label}};
}
std::array<std::string,4> Menu::Button::getLabels() const {
  return {{label}};
}
std::array<std::string,4> Menu::Submenu::getLabels() const {
  return {{label}};
}
std::array<std::string,4> Menu::Selector::getLabels() const {
  return {{label, options[cur_option]}};
}
std::array<std::string,4> Menu::KeyConfig::getLabels() const {
  return {{label,ARS::Controller::getNamesOfBoundKeys(player_no,button_index)}};
}

void Menu::Button::activate() {
  callback();
}
void Menu::Submenu::activate() {
  Menu::addNewMenu(callback());
}
void Menu::Selector::activate() {
  cur_option = (cur_option + 1) % options.size();
  update_selection(cur_option);
}
void Menu::Selector::reverseActivate() {
  if(cur_option == 0) cur_option = options.size() - 1;
  else --cur_option;
  update_selection(cur_option);
}
void Menu::KeyConfig::beginBindingKey(uint8_t& controller_type,
                                      uint8_t& button_index) {
  controller_type = this->controller_type;
  button_index = this->button_index;
  ARS::Controller::startBindingKey(player_no, button_index);
}
std::string Menu::KeyConfig::getKeyBindingInstructions() const {
  return ARS::Controller::getKeyBindingInstructions(player_no, button_index);
}

Menu::Menu() {}
Menu::Menu(std::string title, std::vector<std::shared_ptr<Item> > items)
  : items(std::move(items)), title(std::move(title)) {}
Menu::~Menu() {}

const std::string& Menu::getTitle() const {
  return title;
}

int Menu::getItemCount() const {
  return items.size();
}

std::shared_ptr<Menu::Item> Menu::getItem(size_t n) const {
  return n >= items.size() ? dummyItem : items[n];
}

void Menu::replaceActiveMenu(std::shared_ptr<Menu> menu) {
  SDL_assert(!menus.empty());
  menus.back() = menu;
  ++cookie;
  if(cookie == 0xEC) ++cookie;
}

void Menu::addNewMenu(std::shared_ptr<Menu> menu) {
  menus.push_back(menu);
  ++cookie;
  if(cookie == 0xEC) ++cookie;
}

void Menu::backOutOfMenu() {
  SDL_assert(!menus.empty());
  menus.pop_back();
  ++cookie;
  if(cookie == 0xEC) ++cookie;
}

void Menu::weaklyBackOutOfMenu() {
  SDL_assert(!menus.empty());
  if(menus.size() > 1) {
    menus.pop_back();
    ++cookie;
    if(cookie == 0xEC) ++cookie;
  }
}

void Menu::tearDownMenus(bool) {
  menus.clear();
}

std::shared_ptr<Menu> Menu::getTopMenu() {
  if(menus.empty()) return emptyMenu;
  else return menus.back();
}

uint8_t Menu::getCookie() {
  return cookie;
}

void Menu::addText(std::vector<std::shared_ptr<Menu::Item> >& items,
                   const std::string& text) {
  auto sit = text.cbegin();
  while(sit < text.cend()) {
    auto eit = std::find(sit, text.cend(), '\n');
    items.emplace_back(new Menu::Label(std::string(sit, eit)));
    sit = eit + 1;
  }
}
