#include "ars-emu.hh"
#include "w65c02.hh"

namespace {
  class CPU_Scanline : public ARS::CPU {
    int cycle_budget = 0;
    uint32_t audio_cycle_counter = 0;
    W65C02::Core<CPU_Scanline> core;
  public:
    CPU_Scanline() : core(*this) {}
    ~CPU_Scanline() {}
    void handleReset() override {
      cycle_budget = 0;
      audio_cycle_counter = 0;
      core.reset();
    }
    void eatCycles(int count) override {
      cycle_budget -= count;
    }
    void runCycles(int count) override {
      cycle_budget += count;
      audio_cycle_counter += count;
      while(core.in_productive_state() && cycle_budget > 0) {
        core.step();
      }
      if(cycle_budget > 0) {
        cycle_budget = 0;
      }
      while(audio_cycle_counter >= 256) {
        audio_cycle_counter -= 256;
        ARS::output_apu_sample();
      }
    }
    void setIRQ(bool irq) override { core.set_irq(irq); }
    void setSO(bool so) override { core.set_so(so); }
    void setNMI(bool nmi) override { core.set_nmi(nmi); }
    uint8_t read_byte(uint16_t addr, W65C02::ReadType) {
      --cycle_budget;
      return ARS::read(addr);
    }
    uint8_t read_opcode(uint16_t addr, W65C02::ReadType) {
      --cycle_budget;
      return ARS::read(addr, false, false, true);
    }
    uint8_t fetch_vector_byte(uint16_t addr) {
      --cycle_budget;
      return ARS::read(addr, false, true);
    }
    void write_byte(uint16_t addr, uint8_t byte, W65C02::WriteType) {
      --cycle_budget;
      ARS::write(addr, byte);
    }
    bool isStopped() override {
      return core.is_stopped();
    }
  };
}

std::unique_ptr<ARS::CPU> ARS::makeScanlineCPU(const std::string&) {
  return std::make_unique<CPU_Scanline>();
}
