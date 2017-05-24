// not config.hh to avoid confusion with TEG's
#ifndef PREFSHH
#define PREFSHH

#include "ars-emu.hh"

class PrefsLogic {
protected:
  PrefsLogic();
  ~PrefsLogic();
  virtual void Load() = 0;
  virtual void Save() = 0;
  virtual void Defaults() = 0;
public:
  static void LoadAll();
  static void SaveAll();
  static void DefaultsAll();
};

#endif
