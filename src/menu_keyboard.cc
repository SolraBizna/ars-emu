#if !NO_KEYBOARD
#include "menu.hh"
#include "controller.hh"

#include "teg.hh"

namespace {
  const SN::ConstKey PLAYER_IDS[2] = {
    "CONTROLLER_P1_NAME"_Key, "CONTROLLER_P2_NAME"_Key
  };
  const SN::ConstKey CONTROLLER_BUTTONS[] = {
    "CONTROLLER_A_BUTTON"_Key,
    "CONTROLLER_B_BUTTON"_Key,
    "CONTROLLER_C_BUTTON"_Key,
    "CONTROLLER_HAT_LEFT"_Key,
    "CONTROLLER_HAT_UP"_Key,
    "CONTROLLER_HAT_RIGHT"_Key,
    "CONTROLLER_HAT_DOWN"_Key,
    "CONTROLLER_PAUSE_BUTTON"_Key,
  };
  const SN::ConstKey EMULATOR_BUTTONS[] = {
    "EMULATOR_RESET"_Key,
    "EMULATOR_TOGGLE_BG"_Key,
    "EMULATOR_TOGGLE_SP"_Key,
    "EMULATOR_TOGGLE_OL"_Key,
  };
  std::shared_ptr<Menu> createPlayerKeyboardMenu(size_t player) {
    std::vector<std::shared_ptr<Menu::Item> > items;
    for(size_t m = 0; m < elementcount(CONTROLLER_BUTTONS); ++m) {
      items.emplace_back(new Menu::KeyConfig
                         (sn.Get("KEYBOARD_MENU_INPUT_BINDING_LABEL"_Key,
                                 {sn.Get(CONTROLLER_BUTTONS[m])}),
                          player, 0, m));
    }
    items.emplace_back(new Menu::Divider());
    items.emplace_back(new Menu::BackButton(sn.Get("GENERIC_MENU_FINISHED_LABEL"_Key)));
    return std::make_shared<Menu>(sn.Get("KEYBOARD_PLAYER_CONTROLLER_MENU_TITLE"_Key, {sn.Get(PLAYER_IDS[player])}), items);
  }
  std::shared_ptr<Menu> createNonplayerKeyboardSubmenu() {
    std::vector<std::shared_ptr<Menu::Item> > items;
    for(size_t m = 0; m < elementcount(EMULATOR_BUTTONS); ++m) {
      items.emplace_back(new Menu::KeyConfig
                         (sn.Get("KEYBOARD_MENU_INPUT_BINDING_LABEL"_Key,
                                 {sn.Get(EMULATOR_BUTTONS[m])}),
                          2, 0, m));
    }
    items.emplace_back(new Menu::Divider());
    items.emplace_back(new Menu::BackButton(sn.Get("GENERIC_MENU_FINISHED_LABEL"_Key)));
    return std::make_shared<Menu>(sn.Get("KEYBOARD_NONPLAYER_MENU_TITLE"_Key), items);
  }
}

std::string ARS::Controller::getKeyBindingInstructions(int player, int button){
  std::string button_name =
    player < 2
    ? sn.Get("CONTROLLER_INPUT_NAME"_Key,
             {sn.Get(PLAYER_IDS[player]), sn.Get(CONTROLLER_BUTTONS[button])})
    : sn.Get(EMULATOR_BUTTONS[button]);
  return sn.Get("CONTROLLER_KEY_BINDING_INSTRUCTIONS"_Key,
                {button_name});
}

std::shared_ptr<Menu> Menu::createKeyboardMenu() {
  std::vector<std::shared_ptr<Menu::Item> > items;
  for(size_t n = 0; n < elementcount(PLAYER_IDS); ++n) {
    items.emplace_back(new Menu::Submenu(sn.Get("KEYBOARD_PLAYER_CONTROLLER_SUBMENU_BUTTON_LABEL"_Key, {sn.Get(PLAYER_IDS[n])}), [n]() { return createPlayerKeyboardMenu(n); }));
  }
  items.emplace_back(new Menu::Submenu(sn.Get("KEYBOARD_NONPLAYER_SUBMENU_BUTTON_LABEL"_Key), createNonplayerKeyboardSubmenu));
  items.emplace_back(new Menu::Divider());
  items.emplace_back(new Menu::BackButton(sn.Get("GENERIC_MENU_FINISHED_LABEL"_Key)));
  return std::make_shared<Menu>(sn.Get("KEYBOARD_MENU_TITLE"_Key), items);
}
#endif
