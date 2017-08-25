#ifndef CPUHH
#define CPUHH

#include "ars-emu.hh"

namespace ARS {
  class CPU {
  protected:
    CPU() {}
  public:
    virtual ~CPU() {}
    virtual void handleReset() = 0;
    virtual void eatCycles(int count) = 0;
    virtual void runCycles(int count) = 0;
    virtual void setIRQ(bool irq) = 0;
    virtual void setSO(bool so) = 0;
    virtual void setNMI(bool nmi) = 0;
    virtual bool isStopped() = 0;
    virtual void frameBoundary() {}
    // don't forget, NMI active = masked IRQ
  };
  extern std::unique_ptr<CPU> cpu;
  std::unique_ptr<CPU> makeScanlineCPU(const std::string& rom_path);
  std::unique_ptr<CPU> makeScanlineIntProfCPU(const std::string& rom_path);
  std::unique_ptr<CPU> makeScanlineDebugCPU(const std::string& rom_path);
}

#endif
