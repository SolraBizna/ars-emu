#ifndef MENUHH
#define MENUHH

/* This header deals with the emulator side of the menu system of the Config
   "chip". */

#include "ars-emu.hh"

#include <memory>
#include <functional>

class Menu : public std::enable_shared_from_this<Menu> {
public:
  class Item;
private:
  const std::vector<std::shared_ptr<Item> > items;
  std::string title;
public:
  class Item : public std::enable_shared_from_this<Item> {
  public:
    virtual ~Item();
    virtual uint8_t getType() const = 0;
    virtual std::array<std::string,4> getLabels() const = 0;
    virtual void activate();
    virtual void reverseActivate();
    virtual void beginBindingKey(uint8_t& controller_type,
                                 uint8_t& button_index);
    virtual std::string getKeyBindingInstructions() const;
  };
  class Divider : public Item {
  public:
    uint8_t getType() const override;
    std::array<std::string,4> getLabels() const override;
  };
  class Label : public Item {
    std::string label;
  public:
    Label(const std::string& label) : label(label) {}
    Label(std::string&& label) : label(std::move(label)) {}
    uint8_t getType() const override;
    std::array<std::string,4> getLabels() const override;
  };
  class Button : public Item {
    std::string label;
    std::function<void()> callback;
  public:
    Button(std::string label, std::function<void()> callback)
      : label(std::move(label)), callback(callback) {}
    uint8_t getType() const override;
    std::array<std::string,4> getLabels() const override;
    void activate() override;
  };
  class BackButton : public Button {
  public:
    BackButton(std::string label);
    uint8_t getType() const override;
  };
  class ApplyButton : public Button {
  public:
    ApplyButton(std::string label, std::function<void()> callback)
      : Button(std::move(label), callback) {}
    uint8_t getType() const override;
  };
  class SaveButton : public Button {
  public:
    SaveButton(std::string label, std::function<void()> callback)
      : Button(std::move(label), callback) {}
    uint8_t getType() const override;
  };
  class Submenu : public Item {
  public:
    std::string label;
    std::function<std::shared_ptr<Menu>()> callback;
  public:
    Submenu(std::string label, std::function<std::shared_ptr<Menu>()>
            callback)
      : label(std::move(label)), callback(callback) {}
    uint8_t getType() const override;
    std::array<std::string,4> getLabels() const override;
    void activate() override;
  };
  class Selector : public Item {
    std::string label;
    const std::vector<std::string> options;
    size_t cur_option;
    std::function<void(size_t)> update_selection;
  public:
    Selector(std::string label,
             std::vector<std::string> options, int cur_option,
             std::function<void(size_t)> update_selection)
      : label(std::move(label)), options(std::move(options)),
        cur_option(cur_option), update_selection(update_selection) {}
    uint8_t getType() const override;
    std::array<std::string,4> getLabels() const override;
    void activate() override;
    void reverseActivate() override;
  };
  class KeyConfig : public Item {
    std::string label;
    int player_no;
    const uint8_t controller_type, button_index;
  public:
    KeyConfig(std::string label, int player_no,
              uint8_t controller_type, uint8_t button_index)
      : label(std::move(label)), player_no(player_no),
        controller_type(controller_type), button_index(button_index) {}
    uint8_t getType() const override;
    std::array<std::string,4> getLabels() const override;
    void beginBindingKey(uint8_t& controller_type,
                         uint8_t& button_index) override;
    std::string getKeyBindingInstructions() const override;
  };
  Menu();
  Menu(std::string title, std::vector<std::shared_ptr<Item> >);
  virtual ~Menu();
  const std::string& getTitle() const;
  int getItemCount() const;
  std::shared_ptr<Item> getItem(size_t n) const;
  static void addText(std::vector<std::shared_ptr<Menu::Item> >& items,
                      const std::string& text);
  static uint8_t getCookie();
  static void replaceActiveMenu(std::shared_ptr<Menu>);
  static void addNewMenu(std::shared_ptr<Menu>);
  static void backOutOfMenu();
  static void weaklyBackOutOfMenu();
  static void tearDownMenus(bool mayReset = true);
  static std::shared_ptr<Menu> getTopMenu();
  static std::shared_ptr<Menu> createMainMenu();
  static std::shared_ptr<Menu> createFightMenu();
  static std::shared_ptr<Menu> createKeyboardMenu();
};

#endif
