#include "menu.hh"

#include "prefs.hh"

std::shared_ptr<Menu> Menu::createMainMenu() {
  std::vector<std::shared_ptr<Menu::Item> > items;
#if !NO_KEYBOARD
  Menu::addText(items, sn.Get("MAIN_MENU_KEYBOARD_HELP_TEXT"_Key));
  items.emplace_back(new Menu::Divider());
#endif
  items.emplace_back(new Menu::Submenu(sn.Get("MAIN_MENU_AUDIO_SUBMENU"_Key),
                                       Menu::createAudioMenu));
  items.emplace_back(new Menu::Submenu(sn.Get("MAIN_MENU_VIDEO_SUBMENU"_Key),
                                       Menu::createFightMenu));
#if !NO_KEYBOARD
  items.emplace_back(new Menu::Divider());
  items.emplace_back(new Menu::Submenu(sn.Get("MAIN_MENU"
                                              "_KEYBOARD_SUBMENU"_Key),
                                       Menu::createKeyboardMenu));
#endif
  items.emplace_back(new Menu::Divider());
  items.emplace_back(new Menu::Button(sn.Get("MAIN_MENU_FINISHED_LABEL"_Key),
                                      []() {
                                        PrefsLogic::SaveAll();
                                        Menu::backOutOfMenu();
                                      }));
  items.emplace_back(new Menu::Button(sn.Get("MAIN_MENU_REVERT_LABEL"_Key),
                                      []() {
                                        PrefsLogic::LoadAll();
                                        Menu::backOutOfMenu();
                                      }));
  return std::make_shared<Menu>(sn.Get("MAIN_MENU_TITLE"_Key), items);
}
