/* This is the strangest implementation of this test I've written so far */

#include "menu.hh"
#include "teg.hh"

namespace {
  std::shared_ptr<Menu> makeFightMenu(int uhp, int upot, int mhp,
                                      std::string message) {
    std::vector<std::shared_ptr<Menu::Item> > items;
    items.emplace_back(new Menu::Label(std::move(message)));
    items.emplace_back(new Menu::Label(TEG::format("You: %i/50 HP, %i potion",
                                                   uhp, upot)));
    items.emplace_back(new Menu::Label(TEG::format("Him: %i/100 HP", mhp)));
    bool owari = false;
    if(uhp <= 0) {
      items.emplace_back(new Menu::Label("You die..."));
      owari = true;
    }
    else if(mhp <= 0) {
      items.emplace_back(new Menu::Label("You win!"));
      owari = true;
    }
    items.emplace_back(new Menu::Divider());
    if(!owari) {
      items.emplace_back(new Menu::Button("Attack! (10 dmg)",
        [uhp,upot,mhp]() {
          Menu::replaceActiveMenu(makeFightMenu(uhp-10, upot, mhp-10, "You attack each other!"));
        }));
      if(upot > 0)
        items.emplace_back(new Menu::Button("Potion! (+30 HP)",
          [uhp,upot,mhp]() {
            Menu::replaceActiveMenu(makeFightMenu(std::min(uhp+30,50)-10, upot-1, mhp, "You drink a potion!"));
          }));
      else
        items.emplace_back(new Menu::Label("(no more potions)"));
    }
    else {
      items.emplace_back(new Menu::Button("Restart",
        []() {
          Menu::replaceActiveMenu(Menu::createFightMenu());
        }));
    }
    items.emplace_back(new Menu::BackButton("Stop Fighting"));
    return std::make_shared<Menu>("Fight!", items);
  }
}

std::shared_ptr<Menu> Menu::createFightMenu() {
  return makeFightMenu(50, 3, 100, "Welcome to Fight!");
}
