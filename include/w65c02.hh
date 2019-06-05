#ifndef W65C02_HH
#define W65C02_HH

// define MORE_COMPATIBLE_WITH_6502 to non-zero before including this file if
// you want to emulate certain key operations the way NMOS 6502s handled them
// (useful for passing Klauss Dormann's functional tests)
#ifndef MORE_COMPATIBLE_WITH_6502
#define MORE_COMPATIBLE_WITH_6502 0
#endif

#include <inttypes.h>

namespace W65C02 {
  // these enums are intended only be used to facilitate debuggers
  enum class ReadType {
    // Fetching the opcode for an instruction
    // (only passed to `read_opcode`)
    OPCODE,
    // Fetching the opcode for an instruction which has been preempted by an
    // interrupt
    // (only passed to `read_opcode`)
    PREEMPTED,
    // (all remaining values passed to `read_byte` only)
    // Fetching the "second" byte of a one-byte instruction
    UNUSED,
    // Fetching the operand for an instruction
    OPERAND,
    // Fetching a pointer to be used for indirection
    POINTER,
    // Fetching data
    DATA,
    // Fetching data for a RMW instruction
    DATA_LOCKED,
    // Stack operation
    POP,
    // Internal operation
    IOP,
    // Internal operation during a RMW instruction
    IOP_LOCKED,
    // WAI has been executed (waiting for interrupt)
    WAI,
    // STP has been executed (waiting for reset)
    STP,
  };
  enum class WriteType {
    // Writing data
    DATA,
    // Writing the result of a RMW operation
    DATA_LOCKED,
    // Stack operation
    PUSH
  };
  namespace AM {
    /*
       Each addressing mode is a template class, with the following methods:

       constructor: Fetch the opcode, if any, and calculate the effective
       address, if any.

       read(): Fetches data according to this addressing mode.

       perform_spurious_rmw_read(): Perform the extra read that occurs during
       read-modify-write instructions. (This does not always read the effective
       address.)

       write(): Stores data according to this addressing mode.

       get_effective_address(): Return the effective address for this
       addressing mode. (Not implemented for implied modes.)

       get_branch_target_address(): Return the target address for this
       relative jump. (Only used for branch instructions, not JMP instructions)

    */
    template<class Core> class HasEA {
    protected:
      uint16_t ea;
      Core& core;
      HasEA(Core& core) : core(core) {}
    public:
      uint8_t read(bool locked = false) {
        return core.read_byte(ea, locked ? ReadType::DATA_LOCKED
                              : ReadType::DATA);
      }
      uint8_t read_iop(bool locked = false) {
        return core.read_byte(ea, locked ? ReadType::IOP_LOCKED
                              : ReadType::IOP);
      }
      void perform_spurious_rmw_read() {
        core.read_byte(this->ea, ReadType::IOP_LOCKED);
      }
      void write(uint8_t v, bool locked = false) {
        return core.write_byte(ea, v, locked ? WriteType::DATA_LOCKED
                               : WriteType::DATA);
      }
      uint16_t get_effective_address() { return ea; }
    };
    // 8a, 8b, 8c
    template<class Core> class Implied {
    protected:
      Core& core;
    public:
      Implied(Core& core) : core(core) {
        core.read_byte(core.read_pc(), ReadType::UNUSED);
      }
      void read_iop() {}
    };
    // all implied NOPs are like this (except $EA for some reason)
    template<class Core> class SuperImplied {
    protected:
      Core& core;
    public:
      SuperImplied(Core& core) : core(core) {
      }
      void read_iop() {}
    };
    // 1a, 1b, 1c
    template<class Core> class Absolute : public HasEA<Core> {
    public:
      Absolute(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        this->ea = core.read_byte(core.read_pc_postincrement(),
                                  ReadType::OPERAND);
        // Cycle 3
        this->ea |= core.read_byte(core.read_pc_postincrement(),
                                   ReadType::OPERAND) << 8;
      }
    };
    // 2
    template<class Core> class AbsoluteXIndirect : public HasEA<Core> {
    public:
      AbsoluteXIndirect(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::OPERAND);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc(),
                                    ReadType::OPERAND) << 8;
        // Cycle 4
        uint16_t indexed_addr = base_addr + core.read_x();
        core.read_byte(core.read_pc_postincrement(), ReadType::IOP);
        // Cycle 5
        this->ea = core.read_byte(indexed_addr, ReadType::POINTER);
        // Cycle 6+
        this->ea |= core.read_byte(indexed_addr+1, ReadType::POINTER)<<8;
      }
    };
    // 3a
    template<class Core> class AbsoluteX : public HasEA<Core> {
    public:
      AbsoluteX(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::DATA);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc_postincrement(),
                                    ReadType::DATA) << 8;
        // Cycle 4?
        this->ea = base_addr + core.read_x();
        if((this->ea>>8) != (base_addr>>8))
          core.read_byte(core.read_pc()-1, ReadType::IOP);
      }
    };
    // a couple instructions have this version...
    template<class Core> class AbsoluteXBug : public HasEA<Core> {
    public:
      AbsoluteXBug(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::DATA);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc_postincrement(),
                                    ReadType::DATA) << 8;
        // Cycle 4
        this->ea = base_addr + core.read_x();
        if((this->ea>>8) != (base_addr>>8))
          core.read_byte(core.read_pc()-1, ReadType::IOP);
        else
          core.read_byte(this->ea, ReadType::IOP);
      }
    };
    // 3b
    template<class Core> class AbsoluteX_RMW : public HasEA<Core> {
    public:
      AbsoluteX_RMW(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::OPERAND);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc_postincrement(),
                                    ReadType::OPERAND) << 8;
        // Cycle 4
        this->ea = base_addr + core.read_x();
        core.read_byte((this->ea&0xFF)|(base_addr&0xFF00), ReadType::IOP);
      }
      void perform_spurious_rmw_read(Core& core) {
        core.read_byte(this->ea+1, ReadType::IOP);
      }
    };
    // 4
    template<class Core> class AbsoluteY : public HasEA<Core> {
    public:
      AbsoluteY(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::OPERAND);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc_postincrement(),
                                    ReadType::OPERAND) << 8;
        // Cycle 4?
        this->ea = base_addr + core.read_y();
        if((this->ea>>8) != (base_addr>>8))
          core.read_byte(core.read_pc()-1, ReadType::IOP);
      }
    };
    // a couple instructions have this version
    template<class Core> class AbsoluteYBug : public HasEA<Core> {
    public:
      AbsoluteYBug(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::OPERAND);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc_postincrement(),
                                    ReadType::OPERAND) << 8;
        // Cycle 4?
        this->ea = base_addr + core.read_y();
        if((this->ea>>8) != (base_addr>>8))
          core.read_byte(core.read_pc()-1, ReadType::IOP);
        else
          core.read_byte(this->ea, ReadType::IOP);
      }
    };
    // 5
    template<class Core> class AbsoluteIndirect : public HasEA<Core> {
    public:
      AbsoluteIndirect(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint16_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                            ReadType::OPERAND);
        // Cycle 3
        base_addr |= core.read_byte(core.read_pc(), ReadType::OPERAND) << 8;
        // Cycle 4
        core.read_byte(core.read_pc_postincrement(), ReadType::IOP);
        // Cycle 5
        this->ea = core.read_byte(base_addr, ReadType::POINTER);
        // Cycle 6
        this->ea |= core.read_byte(base_addr+1, ReadType::POINTER) << 8;
      }
    };
    // 6
    template<class Core> class ImpliedA : public Implied<Core> {
    public:
      ImpliedA(Core& core) : Implied<Core>(core) {}
      uint8_t read(bool = false) { return this->core.read_a(); }
      void perform_spurious_rmw_read() {}
      void write(uint8_t v, bool = false) { return this->core.write_a(v); }
    };
    // no number...
    template<class Core> class ImpliedX : public Implied<Core> {
    public:
      ImpliedX(Core& core) : Implied<Core>(core) {}
      uint8_t read(bool = false) { return this->core.read_x(); }
      void perform_spurious_rmw_read() {}
      void write(uint8_t v, bool = false) { return this->core.write_x(v); }
    };
    template<class Core> class ImpliedY : public Implied<Core> {
    public:
      ImpliedY(Core& core) : Implied<Core>(core) {}
      uint8_t read(bool = false) { return this->core.read_y(); }
      void perform_spurious_rmw_read() {}
      void write(uint8_t v, bool = false) { return this->core.write_y(v); }
    };
    // 7
    template<class Core> class Immediate {
      uint8_t value;
    public:
      Immediate(Core& core) {
        value = core.read_byte(core.read_pc_postincrement(),
                               ReadType::OPERAND);
      }
      uint8_t read(bool = false) { return value; }
      void read_iop() {}
    };
    // 9a
    template<class Core> class Relative : public HasEA<Core> {
    public:
      Relative(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        int8_t branch_offset = core.read_byte(core.read_pc_postincrement(),
                                              ReadType::OPERAND);
        // Cycle 3 (pipelined)
        this->ea = core.read_pc()+branch_offset;
      }
      uint16_t get_branch_target_address() {
        // burn a cycle
        this->core.read_byte(this->core.read_pc(), ReadType::IOP);
        if(this->core.read_pc()>>8 != this->ea>>8) {
          bool old_irq_edge = this->core.irq_edge;
          this->core.check_irq_edge();
          this->core.irq_edge = this->core.irq_edge || old_irq_edge;
          // burn another cycle
          this->core.read_byte(this->core.read_pc(), ReadType::IOP);
        }
        return this->ea;
      }
    };
    // 9b
    template<class Core> class RelativeBitBranch {
      Core& core;
      uint16_t target_pc;
      uint8_t data;
    public:
      RelativeBitBranch(Core& core) : core(core) {
        // Cycle 2
        uint8_t addr = core.read_byte(core.read_pc_postincrement(), ReadType::OPERAND);
        // Cycle 3
        data = core.read_byte(addr, ReadType::DATA);
        // Cycle 4
        data = core.read_byte(addr, ReadType::IOP);
        // Cycle 5
        int8_t branch_offset = core.read_byte(core.read_pc_postincrement(),
                                              ReadType::OPERAND);
        target_pc = core.read_pc()+branch_offset;
      }
      uint8_t read(bool = false) { return data; }
      uint16_t get_branch_target_address() {
        this->core.read_byte(this->core.read_pc(), ReadType::IOP);
        if(this->core.read_pc()>>8 != target_pc>>8) {
          bool old_irq_edge = this->core.irq_edge;
          this->core.check_irq_edge();
          this->core.irq_edge = this->core.irq_edge || old_irq_edge;
          // burn another cycle
          this->core.read_byte(this->core.read_pc(), ReadType::IOP);
        }
        return target_pc;
      }
    };
    // 11a, 11b, 11c
    template<class Core> class ZeroPage : public HasEA<Core> {
    public:
      ZeroPage(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        this->ea = core.read_byte(core.read_pc_postincrement(),
                                  ReadType::OPERAND);
      }
    };
    // 12
    template<class Core> class ZeroPageXIndirect : public HasEA<Core> {
    public:
      ZeroPageXIndirect(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint8_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                           ReadType::OPERAND);
        // Cycle 3
        uint8_t indexed_addr = base_addr + core.read_x();
        core.read_byte(core.read_pc()-1, ReadType::IOP);
        // Cycle 4
        this->ea = core.read_byte(indexed_addr, ReadType::POINTER);
        // Cycle 5
        this->ea |= core.read_byte((uint8_t)(indexed_addr+1),
                                   ReadType::POINTER) << 8;
        // TODO: is this right, or can we read $00FF,$0100 like this?
      }
    };
    // 13a, 13b
    template<class Core> class ZeroPageX : public HasEA<Core> {
    public:
      ZeroPageX(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint8_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                           ReadType::OPERAND);
        // Cycle 3
        this->ea = (base_addr + core.read_x()) & 0xFF;
        core.read_byte(core.read_pc()-1, ReadType::IOP);
      }
    };
    // 14
    template<class Core> class ZeroPageY : public HasEA<Core> {
    public:
      ZeroPageY(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint8_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                           ReadType::OPERAND);
        // Cycle 3
        this->ea = (base_addr + core.read_y()) & 0xFF;
        core.read_byte(core.read_pc()-1, ReadType::IOP);
      }
    };
    // 15
    template<class Core> class ZeroPageIndirect : public HasEA<Core> {
    public:
      ZeroPageIndirect(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint8_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                           ReadType::OPERAND);
        // Cycle 3
        this->ea = core.read_byte(base_addr, ReadType::POINTER);
        // Cycle 4
        this->ea |= core.read_byte((uint8_t)(base_addr+1),
                                   ReadType::POINTER) << 8;
      }
    };
    // 16
    template<class Core> class ZeroPageIndirectY : public HasEA<Core> {
    public:
      ZeroPageIndirectY(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint8_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                           ReadType::OPERAND);
        // Cycle 3
        uint16_t indirect_addr = core.read_byte(base_addr, ReadType::POINTER);
        // Cycle 4
        indirect_addr |= core.read_byte((uint8_t)(base_addr+1),
                                        ReadType::POINTER) << 8;
        // Cycle 5?
        this->ea = indirect_addr + core.read_y();
        if(this->ea>>8 != indirect_addr>>8)
          core.read_byte((uint8_t)(base_addr+1), ReadType::IOP);
      }
    };
    // obligatory needlessly different version
    template<class Core> class ZeroPageIndirectYBug : public HasEA<Core> {
    public:
      ZeroPageIndirectYBug(Core& core) : HasEA<Core>(core) {
        // Cycle 2
        uint8_t base_addr = core.read_byte(core.read_pc_postincrement(),
                                           ReadType::OPERAND);
        // Cycle 3
        uint16_t indirect_addr = core.read_byte(base_addr, ReadType::POINTER);
        // Cycle 4
        indirect_addr |= core.read_byte((uint8_t)(base_addr+1),
                                        ReadType::POINTER) << 8;
        // Cycle 5
        this->ea = indirect_addr + core.read_y();
        core.read_byte((uint8_t)(base_addr+1), ReadType::IOP);
      }
    };
    // special: 1d, 10a, 10b, 10c, 10d, 10e, 10f
    // uhhh: 11d?!!
  }
  template<class System> class Core {
    friend class AM::RelativeBitBranch<Core<System>>;
    friend class AM::Relative<Core<System>>;
    System& system;
    uint8_t a, x, y, p, s;
    uint16_t pc;
    uint8_t irq:1, irq_edge:1, nmi:1, nmi_edge:1, nmi_pending:1, so:1, so_edge:1;
    enum class State {
      INVALID_STATE, AWAITING_INTERRUPT, RECOVERING_FROM_RESET, RUNNING,
        STOPPED
    } state;
    void nz_p(uint8_t result) {
      if(result == 0) p |= P_Z;
      else p &= ~P_Z;
      if(result & 128) p |= P_N;
      else p &= ~P_N;
    }
    void cnz_p(uint16_t result) {
      if((result&255) == 0) p |= P_Z;
      else p &= ~P_Z;
      if(result & 128) p |= P_N;
      else p &= ~P_N;
      if(result & 256) p |= P_C;
      else p &= ~P_C;
    }
    void BRK() {
      system.read_byte(read_pc_postincrement(), ReadType::UNUSED);
      push(pc>>8);
      push(pc);
      push(p|P_B);
      p &= ~P_D;
      p |= P_I;
      pc = system.fetch_vector_byte(IRQ_VECTOR);
      check_irq_edge();
      pc |= system.fetch_vector_byte(IRQ_VECTOR+1)<<8;
    }
    template<class AM> void ORA() {
      check_irq_edge();
      nz_p(a |= AM(*this).read());
    }
    template<class AM> void AND() {
      check_irq_edge();
      nz_p(a &= AM(*this).read());
    }
    template<class AM> void EOR() {
      check_irq_edge();
      nz_p(a ^= AM(*this).read());
    }
    template<class AM> void BIT() {
      check_irq_edge();
      uint8_t q = AM(*this).read();
      if(q&a) p &= ~P_Z;
      else p |= P_Z;
      p = (p&0x3F) | (q&0xC0);
    }
    template<class AM> void BIT_I() {
      check_irq_edge();
      uint8_t q = AM(*this).read();
      if(q&a) p &= ~P_Z;
      else p |= P_Z;
    }
    template<class AM> void NOP() {
      AM am(*this);
      check_irq_edge();
      am.read_iop();
    }
    template<class AM> void NOP_5C() {
      AM am(*this);
      // okay, this one's a little weird...
      read_byte(am.get_effective_address() | 0xFF00, ReadType::IOP);
      read_byte(0xFFFF, ReadType::IOP);
      read_byte(0xFFFF, ReadType::IOP);
      read_byte(0xFFFF, ReadType::IOP);
      check_irq_edge();
      read_byte(0xFFFF, ReadType::IOP);
    }
    template<class AM> void TRB() {
      AM am(*this);
      uint8_t mem = am.read();
      am.perform_spurious_rmw_read();
      check_irq_edge();
      am.write(mem & ~a, true);
      if(mem & a) p &= ~P_Z;
      else p |= P_Z;
    }
    template<class AM> void TSB() {
      AM am(*this);
      uint8_t mem = am.read();
      am.perform_spurious_rmw_read();
      check_irq_edge();
      am.write(mem | a, true);
      if(mem & a) p &= ~P_Z;
      else p |= P_Z;
    }
    template<class AM> void ASL() {
      AM am(*this);
      uint16_t q = am.read();
      am.perform_spurious_rmw_read();
      q = q << 1;
      check_irq_edge();
      am.write(q, true);
      cnz_p(q);
    }
    template<class AM> void ROL() {
      AM am(*this);
      uint16_t q = am.read();
      am.perform_spurious_rmw_read();
      q = (q << 1) | ((p&P_C)?1:0);
      check_irq_edge();
      am.write(q, true);
      cnz_p(q);
    }
    template<class AM> void ROR() {
      AM am(*this);
      uint16_t q = am.read();
      am.perform_spurious_rmw_read();
      uint16_t q_ = (q >> 1) | ((p&P_C)?0x80:0);
      check_irq_edge();
      am.write(q_, true);
      nz_p(q_);
      if(q&1) p |= P_C;
      else p &= ~P_C;
    }
    template<class AM> void LSR() {
      AM am(*this);
      uint16_t q = am.read();
      am.perform_spurious_rmw_read();
      uint8_t q_ = q >> 1;
      check_irq_edge();
      am.write(q_, true);
      nz_p(q_);
      if(q&1) p |= P_C;
      else p &= ~P_C;
    }
    template<class AM, int bit> void RMB() {
      AM am(*this);
      uint8_t q = am.read(true);
      am.perform_spurious_rmw_read();
      q = q & ~(1<<bit);
      check_irq_edge();
      am.write(q, true);
    }
    template<class AM, int bit> void SMB() {
      AM am(*this);
      uint8_t q = am.read(true);
      am.perform_spurious_rmw_read();
      q = q | (1<<bit);
      check_irq_edge();
      am.write(q, true);
    }
    template<class AM, int bit> void BBR() {
      AM am(*this);
      check_irq_edge();
      if((am.read()&(1<<bit)) == 0)
        write_pc(am.get_branch_target_address());
    }
    template<class AM, int bit> void BBS() {
      AM am(*this);
      check_irq_edge();
      if((am.read()&(1<<bit)) != 0)
        write_pc(am.get_branch_target_address());
    }
    template<class AM, uint8_t mask, uint8_t cmp> void Branch() {
      AM am(*this);
      check_irq_edge();
      if((p&mask) == cmp)
        write_pc(am.get_branch_target_address());
    }
    template<class AM> void INC() {
      AM am(*this);
      uint8_t q = am.read();
      am.perform_spurious_rmw_read();
      check_irq_edge();
      am.write(++q, true);
      nz_p(q);
    }
    template<class AM> void DEC() {
      AM am(*this);
      uint8_t q = am.read();
      am.perform_spurious_rmw_read();
      check_irq_edge();
      am.write(--q, true);
      nz_p(q);
    }
    template<class AM> void JMP() {
      AM am(*this);
      check_irq_edge();
      write_pc(am.get_effective_address());
    }
    template<class AM> void ADC() {
      AM am(*this);
      check_irq_edge();
      uint8_t red = am.read();
      uint16_t val;
      if(p & P_D) {
        check_irq_edge();
        am.read(); // spurious!
        // TODO: make this less clunky but still pass tests
        unsigned int al = (a & 0xF) + (red & 0xF) + ((p & P_C) ? 1 : 0);
        if(al > 9) al = ((al + 6) & 0xF) | 0x10;
        val = (int8_t(a) & ~0xF) + (int8_t(red) & ~0xF) + al;
        if(val >= 0x80 && val < 0xFF80) p |= P_V;
        else p &= ~P_V;
        val = (a & ~0xF) + (red & ~0xF) + al;
        if(val > 0x9F) val = (val + 0x60) | 0x100;
      }
      else {
        val = a + red;
        if(p & P_C) ++val;
        if((a ^ val) & (red ^ val) & 0x80) p |= P_V;
        else p &= ~P_V;
      }
      a = val;
      cnz_p(val);
    }
    template<class AM> void SBC() {
      AM am(*this);
      check_irq_edge();
      uint8_t red = am.read();
      uint16_t val;
      if(p & P_D) {
        check_irq_edge();
        am.read(); // spurious!
        // TODO: make this less clunky but still pass tests
        int al = (a & 0xF) - (red & 0xF) - ((p & P_C) ? 0 : 1);
        val = a - red - ((p & P_C) ? 0 : 1);
        if((a ^ val) & (red ^ 0xFF ^ val) & 0x80) p |= P_V;
        else p &= ~P_V;
        if(val & 0x8000) {
          val -= 0x60;
          p &= ~P_C;
        }
        else {
          p |= P_C;
        }
        if(al < 0) val -= 0x06;
        nz_p(val);
      }
      else {
        red ^= 0xFF;
        val = a + red;
        if(p & P_C) ++val;
        if((a ^ val) & (red ^ val) & 0x80) p |= P_V;
        else p &= ~P_V;
        cnz_p(val);
      }
      a = val;
    }
    template<class AM> void CMP() {
      AM am(*this);
      check_irq_edge();
      uint8_t red = am.read();
      uint16_t val = (red^0xFF) + a + 1;
      cnz_p(val);
    }
    template<class AM> void CPX() {
      AM am(*this);
      check_irq_edge();
      uint8_t red = am.read();
      uint16_t val = (red^0xFF) + x + 1;
      cnz_p(val);
    }
    template<class AM> void CPY() {
      AM am(*this);
      check_irq_edge();
      uint8_t red = am.read();
      uint16_t val = (red^0xFF) + y + 1;
      cnz_p(val);
    }
    template<class AM> void STZ() {
      AM am(*this);
      check_irq_edge();
      am.write(0);
    }
    template<class AM> void STA() {
      AM am(*this);
      check_irq_edge();
      am.write(a);
    }
    template<class AM> void STX() {
      AM am(*this);
      check_irq_edge();
      am.write(x);
    }
    template<class AM> void STY() {
      AM am(*this);
      check_irq_edge();
      am.write(y);
    }
    template<class AM> void LDA() {
      AM am(*this);
      check_irq_edge();
      a = am.read();
      nz_p(a);
    }
    template<class AM> void LDX() {
      AM am(*this);
      check_irq_edge();
      x = am.read();
      nz_p(x);
    }
    template<class AM> void LDY() {
      AM am(*this);
      check_irq_edge();
      y = am.read();
      nz_p(y);
    }
    void JSR() {
      uint16_t target = read_byte(read_pc_postincrement(), ReadType::OPERAND);
      read_byte(0x100 | s, ReadType::IOP);
      push(pc>>8);
      push(pc);
      check_irq_edge();
      target |= read_byte(read_pc(), ReadType::OPERAND)<<8;
      pc = target;
    }
    void RTI() {
      uint16_t io;
      read_byte(io = read_pc(), ReadType::UNUSED);
      read_byte(read_s() | 0x0100, ReadType::IOP);
      write_p(pop());
      pc = pop();
      check_irq_edge();
      pc |= pop()<<8;
    }
    void RTS() {
      uint16_t io;
      read_byte(io = read_pc(), ReadType::UNUSED);
      read_byte(0x100 | read_s(), ReadType::IOP);
      pc = pop();
      pc |= pop()<<8;
      check_irq_edge();
      read_byte(pc, ReadType::IOP);
      ++pc;
    }
    void PHP() {
      system.read_byte(pc, ReadType::UNUSED);
      check_irq_edge();
      push(p|P_B|P_1);
    }
    void PLP() {
      system.read_byte(pc, ReadType::UNUSED);
      system.read_byte(0x100|read_s(), ReadType::IOP);
      check_irq_edge();
      write_p(pop());
    }
    void PHA() {
      system.read_byte(pc, ReadType::UNUSED);
      check_irq_edge();
      push(a);
    }
    void PLA() {
      system.read_byte(pc, ReadType::UNUSED);
      system.read_byte(0x100|read_s(), ReadType::IOP);
      check_irq_edge();
      nz_p(a = pop());
    }
    void PHX() {
      system.read_byte(pc, ReadType::UNUSED);
      check_irq_edge();
      push(x);
    }
    void PLX() {
      system.read_byte(pc, ReadType::UNUSED);
      system.read_byte(0x100|read_s(), ReadType::IOP);
      check_irq_edge();
      nz_p(x = pop());
    }
    void PHY() {
      system.read_byte(pc, ReadType::UNUSED);
      check_irq_edge();
      push(y);
    }
    void PLY() {
      system.read_byte(pc, ReadType::UNUSED);
      system.read_byte(0x100|read_s(), ReadType::IOP);
      check_irq_edge();
      nz_p(y = pop());
    }
    void CLC() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p &= ~P_C;
    }
    void SEC() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p |= P_C;
    }
    void CLV() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p &= ~P_V;
    }
    void CLD() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p &= ~P_D;
    }
    void SED() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p |= P_D;
    }
    void CLI() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p &= ~P_I;
    }
    void SEI() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      p |= P_I;
    }
    void TXA() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      nz_p(a = x);
    }
    void TAX() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      nz_p(x = a);
    }
    void TYA() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      nz_p(a = y);
    }
    void TAY() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      nz_p(y = a);
    }
    void TXS() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      s = x;
    }
    void TSX() {
      check_irq_edge();
      system.read_byte(pc, ReadType::UNUSED);
      nz_p(x = s);
    }
    void WAI() {
      state = State::AWAITING_INTERRUPT;
    }
    void STP() {
      // don't bother checking IRQ edge
      system.read_byte(pc, ReadType::UNUSED);
      state = State::STOPPED;
    }
    inline void check_irq_edge() {
      irq_edge = irq && !(p & P_I);
      nmi_pending = nmi_edge;
    }
  public:
    static constexpr uint16_t IRQ_VECTOR = 0xfffe;
    static constexpr uint16_t RESET_VECTOR = 0xfffc;
    static constexpr uint16_t NMI_VECTOR = 0xfffa;
    static constexpr uint8_t P_C = 0x01;
    static constexpr uint8_t P_Z = 0x02;
    static constexpr uint8_t P_I = 0x04;
    static constexpr uint8_t P_D = 0x08;
    static constexpr uint8_t P_B = 0x10;
    // The datasheet claims you can use this as a flag for whatever. This can
    // easily be taken to mean that you can set and clear it freely. Well, ya
    // can't. Just like in the NMOS 6502s, the B and 1 flags don't physically
    // exist in the processor.
    static constexpr uint8_t P_1 = 0x20;
    static constexpr uint8_t P_V = 0x40;
    static constexpr uint8_t P_N = 0x80;
    Core(System& system)
      : system(system), a(0), x(0), y(0), p(0), s(0), pc(0),
        irq(false), irq_edge(false), nmi(false), nmi_edge(false),
        nmi_pending(false), so(false), so_edge(false),
        state(State::INVALID_STATE) {}
    uint8_t read_a() { return a; }
    void write_a(uint8_t v) { a = v; }
    uint8_t read_x() { return x; }
    void write_x(uint8_t v) { x = v; }
    uint8_t read_y() { return y; }
    void write_y(uint8_t v) { y = v;  }
    uint8_t read_p() { return p; }
    void write_p(uint8_t v) { p = (v|P_1); }
    uint8_t read_s() { return s; }
    void write_s(uint8_t v) { s = v; }
    uint16_t read_pc() { return pc; }
    uint16_t read_pc_postincrement() { return pc++; }
    void write_pc(uint16_t v) { pc = v; }
    void set_irq(bool nu) { irq = nu; }
    void set_nmi(bool nu) {
      if(!nmi && nu) nmi_edge = true;
      nmi = nu;
    }
    void set_so(bool nu) {
      if(!so && nu) so_edge = true;
      so = nu;
    }
    void reset() {
      state = State::RECOVERING_FROM_RESET;
      s = 0;
      p = P_1|P_I;
    }
    // returns true if awaiting an interrupt, or stopped
    bool step() {
      if(so_edge) {
        so_edge = false;
        // technically we should filter out less-than-one-cycle SO pulses...
        p |= P_V;
      }
      switch(state) {
      case State::INVALID_STATE:
        abort();
        break;
      case State::STOPPED:
        read_byte(pc, ReadType::STP);
        break;
      case State::RECOVERING_FROM_RESET:
        system.read_opcode(read_pc(), ReadType::PREEMPTED);
        read_byte(read_pc(), ReadType::OPERAND);
        // if this were a real interrupt, something would be written here
        fake_push();
        fake_push();
        fake_push();
        p &= ~P_D;
        p |= P_I;
        pc = system.fetch_vector_byte(RESET_VECTOR);
        pc |= system.fetch_vector_byte(RESET_VECTOR+1)<<8;
        state = State::RUNNING;
        break;
      case State::AWAITING_INTERRUPT:
        if(irq || nmi_edge) {
          state = State::RUNNING;
          read_byte(pc, ReadType::WAI);
        }
        check_irq_edge();
        read_byte(pc, ReadType::WAI);
        break;
      case State::RUNNING: {
        if(nmi_pending) {
          nmi_pending = false;
          nmi_edge = false;
          irq_edge = false;
          system.read_opcode(read_pc(), ReadType::PREEMPTED);
          read_byte(read_pc(), ReadType::OPERAND);
          push(pc>>8);
          push(pc);
          push(p);
          p &= ~P_D;
          pc = system.fetch_vector_byte(NMI_VECTOR);
          pc |= system.fetch_vector_byte(NMI_VECTOR+1)<<8;
        }
        else if(irq_edge) {
          irq_edge = false;
          system.read_opcode(read_pc(), ReadType::PREEMPTED);
          read_byte(read_pc(), ReadType::OPERAND);
          push(pc>>8);
          push(pc);
          push(p&~P_B);
          p &= ~P_D;
          p |= P_I;
          pc = system.fetch_vector_byte(IRQ_VECTOR);
          pc |= system.fetch_vector_byte(IRQ_VECTOR+1)<<8;
        }
        else {
          uint8_t opcode = system.read_opcode(read_pc_postincrement(),
                                              ReadType::OPCODE);
          switch(opcode) {
          case 0x00: BRK(); break;
          case 0x01: ORA<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0x02: NOP<AM::Immediate<Core<System>>>(); break;
          case 0x03: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x04: TSB<AM::ZeroPage<Core<System>>>(); break;
          case 0x05: ORA<AM::ZeroPage<Core<System>>>(); break;
          case 0x06: ASL<AM::ZeroPage<Core<System>>>(); break;
          case 0x07: RMB<AM::ZeroPage<Core<System>>,0>(); break;
          case 0x08: PHP(); break;
          case 0x09: ORA<AM::Immediate<Core<System>>>(); break;
          case 0x0A: ASL<AM::ImpliedA<Core<System>>>(); break;
          case 0x0B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x0C: TSB<AM::Absolute<Core<System>>>(); break;
          case 0x0D: ORA<AM::Absolute<Core<System>>>(); break;
          case 0x0E: ASL<AM::Absolute<Core<System>>>(); break;
          case 0x0F: BBR<AM::RelativeBitBranch<Core<System>>,0>(); break;
          case 0x10: Branch<AM::Relative<Core<System>>,P_N,0>(); break;
          case 0x11: ORA<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0x12: ORA<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0x13: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x14: TRB<AM::ZeroPage<Core<System>>>(); break;
          case 0x15: ORA<AM::ZeroPageX<Core<System>>>(); break;
          case 0x16: ASL<AM::ZeroPageX<Core<System>>>(); break;
          case 0x17: RMB<AM::ZeroPage<Core<System>>,1>(); break;
          case 0x18: CLC(); break;
          case 0x19: ORA<AM::AbsoluteY<Core<System>>>(); break;
          case 0x1A: INC<AM::ImpliedA<Core<System>>>(); break;
          case 0x1B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x1C: TRB<AM::Absolute<Core<System>>>(); break;
          case 0x1D: ORA<AM::AbsoluteX<Core<System>>>(); break;
          case 0x1E: ASL<AM::AbsoluteX<Core<System>>>(); break;
          case 0x1F: BBR<AM::RelativeBitBranch<Core<System>>,1>(); break;
          case 0x20: JSR(); break;
          case 0x21: AND<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0x22: NOP<AM::Immediate<Core<System>>>(); break;
          case 0x23: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x24: BIT<AM::ZeroPage<Core<System>>>(); break;
          case 0x25: AND<AM::ZeroPage<Core<System>>>(); break;
          case 0x26: ROL<AM::ZeroPage<Core<System>>>(); break;
          case 0x27: RMB<AM::ZeroPage<Core<System>>,2>(); break;
          case 0x28: PLP(); break;
          case 0x29: AND<AM::Immediate<Core<System>>>(); break;
          case 0x2A: ROL<AM::ImpliedA<Core<System>>>(); break;
          case 0x2B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x2C: BIT<AM::Absolute<Core<System>>>(); break;
          case 0x2D: AND<AM::Absolute<Core<System>>>(); break;
          case 0x2E: ROL<AM::Absolute<Core<System>>>(); break;
          case 0x2F: BBR<AM::RelativeBitBranch<Core<System>>,2>(); break;
          case 0x30: Branch<AM::Relative<Core<System>>,P_N,P_N>(); break;
          case 0x31: AND<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0x32: AND<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0x33: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x34: BIT<AM::ZeroPageX<Core<System>>>(); break;
          case 0x35: AND<AM::ZeroPageX<Core<System>>>(); break;
          case 0x36: ROL<AM::ZeroPageX<Core<System>>>(); break;
          case 0x37: RMB<AM::ZeroPage<Core<System>>,3>(); break;
          case 0x38: SEC(); break;
          case 0x39: AND<AM::AbsoluteY<Core<System>>>(); break;
          case 0x3A: DEC<AM::ImpliedA<Core<System>>>(); break;
          case 0x3B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x3C: BIT<AM::AbsoluteX<Core<System>>>(); break;
          case 0x3D: AND<AM::AbsoluteX<Core<System>>>(); break;
          case 0x3E: ROL<AM::AbsoluteX<Core<System>>>(); break;
          case 0x3F: BBR<AM::RelativeBitBranch<Core<System>>,3>(); break;
          case 0x40: RTI(); break;
          case 0x41: EOR<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0x42: NOP<AM::Immediate<Core<System>>>(); break;
          case 0x43: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x44: NOP<AM::ZeroPage<Core<System>>>(); break;
          case 0x45: EOR<AM::ZeroPage<Core<System>>>(); break;
          case 0x46: LSR<AM::ZeroPage<Core<System>>>(); break;
          case 0x47: RMB<AM::ZeroPage<Core<System>>,4>(); break;
          case 0x48: PHA(); break;
          case 0x49: EOR<AM::Immediate<Core<System>>>(); break;
          case 0x4A: LSR<AM::ImpliedA<Core<System>>>(); break;
          case 0x4B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x4C: JMP<AM::Absolute<Core<System>>>(); break;
          case 0x4D: EOR<AM::Absolute<Core<System>>>(); break;
          case 0x4E: LSR<AM::Absolute<Core<System>>>(); break;
          case 0x4F: BBR<AM::RelativeBitBranch<Core<System>>,4>(); break;
          case 0x50: Branch<AM::Relative<Core<System>>,P_V,0>(); break;
          case 0x51: EOR<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0x52: EOR<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0x53: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x54: NOP<AM::ZeroPageX<Core<System>>>(); break;
          case 0x55: EOR<AM::ZeroPageX<Core<System>>>(); break;
          case 0x56: LSR<AM::ZeroPageX<Core<System>>>(); break;
          case 0x57: RMB<AM::ZeroPage<Core<System>>,5>(); break;
          case 0x58: CLI(); break;
          case 0x59: EOR<AM::AbsoluteY<Core<System>>>(); break;
          case 0x5A: PHY(); break;
          case 0x5B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x5C: NOP_5C<AM::Absolute<Core<System>>>(); break;
          case 0x5D: EOR<AM::AbsoluteX<Core<System>>>(); break;
          case 0x5E: LSR<AM::AbsoluteX<Core<System>>>(); break;
          case 0x5F: BBR<AM::RelativeBitBranch<Core<System>>,5>(); break;
          case 0x60: RTS(); break;
          case 0x61: ADC<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0x62: NOP<AM::Immediate<Core<System>>>(); break;
          case 0x63: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x64: STZ<AM::ZeroPage<Core<System>>>(); break;
          case 0x65: ADC<AM::ZeroPage<Core<System>>>(); break;
          case 0x66: ROR<AM::ZeroPage<Core<System>>>(); break;
          case 0x67: RMB<AM::ZeroPage<Core<System>>,6>(); break;
          case 0x68: PLA(); break;
          case 0x69: ADC<AM::Immediate<Core<System>>>(); break;
          case 0x6A: ROR<AM::ImpliedA<Core<System>>>(); break;
          case 0x6B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x6C: JMP<AM::AbsoluteIndirect<Core<System>>>(); break;
          case 0x6D: ADC<AM::Absolute<Core<System>>>(); break;
          case 0x6E: ROR<AM::Absolute<Core<System>>>(); break;
          case 0x6F: BBR<AM::RelativeBitBranch<Core<System>>,6>(); break;
          case 0x70: Branch<AM::Relative<Core<System>>,P_V,P_V>(); break;
          case 0x71: ADC<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0x72: ADC<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0x73: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x74: STZ<AM::ZeroPageX<Core<System>>>(); break;
          case 0x75: ADC<AM::ZeroPageX<Core<System>>>(); break;
          case 0x76: ROR<AM::ZeroPageX<Core<System>>>(); break;
          case 0x77: RMB<AM::ZeroPage<Core<System>>,7>(); break;
          case 0x78: SEI(); break;
          case 0x79: ADC<AM::AbsoluteY<Core<System>>>(); break;
          case 0x7A: PLY(); break;
          case 0x7B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x7C: JMP<AM::AbsoluteXIndirect<Core<System>>>(); break;
          case 0x7D: ADC<AM::AbsoluteX<Core<System>>>(); break;
          case 0x7E: ROR<AM::AbsoluteX<Core<System>>>(); break;
          case 0x7F: BBR<AM::RelativeBitBranch<Core<System>>,7>(); break;
          case 0x80: Branch<AM::Relative<Core<System>>,0,0>(); break;
          case 0x81: STA<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0x82: NOP<AM::Immediate<Core<System>>>(); break;
          case 0x83: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x84: STY<AM::ZeroPage<Core<System>>>(); break;
          case 0x85: STA<AM::ZeroPage<Core<System>>>(); break;
          case 0x86: STX<AM::ZeroPage<Core<System>>>(); break;
          case 0x87: SMB<AM::ZeroPage<Core<System>>,0>(); break;
          case 0x88: DEC<AM::ImpliedY<Core<System>>>(); break;
          case 0x89: BIT_I<AM::Immediate<Core<System>>>(); break;
          case 0x8A: TXA(); break;
          case 0x8B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x8C: STY<AM::Absolute<Core<System>>>(); break;
          case 0x8D: STA<AM::Absolute<Core<System>>>(); break;
          case 0x8E: STX<AM::Absolute<Core<System>>>(); break;
          case 0x8F: BBS<AM::RelativeBitBranch<Core<System>>,0>(); break;
          case 0x90: Branch<AM::Relative<Core<System>>,P_C,0>(); break;
          case 0x91: STA<AM::ZeroPageIndirectYBug<Core<System>>>(); break;
          case 0x92: STA<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0x93: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x94: STY<AM::ZeroPageX<Core<System>>>(); break;
          case 0x95: STA<AM::ZeroPageX<Core<System>>>(); break;
          case 0x96: STX<AM::ZeroPageY<Core<System>>>(); break;
          case 0x97: SMB<AM::ZeroPage<Core<System>>,1>(); break;
          case 0x98: TYA(); break;
          case 0x99: STA<AM::AbsoluteYBug<Core<System>>>(); break;
          case 0x9A: TXS(); break;
          case 0x9B: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0x9C: STZ<AM::Absolute<Core<System>>>(); break;
          case 0x9D: STA<AM::AbsoluteXBug<Core<System>>>(); break;
          case 0x9E: STZ<AM::AbsoluteXBug<Core<System>>>(); break;
          case 0x9F: BBS<AM::RelativeBitBranch<Core<System>>,1>(); break;
          case 0xA0: LDY<AM::Immediate<Core<System>>>(); break;
          case 0xA1: LDA<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0xA2: LDX<AM::Immediate<Core<System>>>(); break;
          case 0xA3: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xA4: LDY<AM::ZeroPage<Core<System>>>(); break;
          case 0xA5: LDA<AM::ZeroPage<Core<System>>>(); break;
          case 0xA6: LDX<AM::ZeroPage<Core<System>>>(); break;
          case 0xA7: SMB<AM::ZeroPage<Core<System>>,2>(); break;
          case 0xA8: TAY(); break;
          case 0xA9: LDA<AM::Immediate<Core<System>>>(); break;
          case 0xAA: TAX(); break;
          case 0xAB: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xAC: LDY<AM::Absolute<Core<System>>>(); break;
          case 0xAD: LDA<AM::Absolute<Core<System>>>(); break;
          case 0xAE: LDX<AM::Absolute<Core<System>>>(); break;
          case 0xAF: BBS<AM::RelativeBitBranch<Core<System>>,2>(); break;
          case 0xB0: Branch<AM::Relative<Core<System>>,P_C,P_C>(); break;
          case 0xB1: LDA<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0xB2: LDA<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0xB3: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xB4: LDY<AM::ZeroPageX<Core<System>>>(); break;
          case 0xB5: LDA<AM::ZeroPageX<Core<System>>>(); break;
          case 0xB6: LDX<AM::ZeroPageY<Core<System>>>(); break;
          case 0xB7: SMB<AM::ZeroPage<Core<System>>,3>(); break;
          case 0xB8: CLV(); break;
          case 0xB9: LDA<AM::AbsoluteY<Core<System>>>(); break;
          case 0xBA: TSX(); break;
          case 0xBB: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xBC: LDY<AM::AbsoluteX<Core<System>>>(); break;
          case 0xBD: LDA<AM::AbsoluteX<Core<System>>>(); break;
          case 0xBE: LDX<AM::AbsoluteY<Core<System>>>(); break;
          case 0xBF: BBS<AM::RelativeBitBranch<Core<System>>,3>(); break;
          case 0xC0: CPY<AM::Immediate<Core<System>>>(); break;
          case 0xC1: CMP<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0xC2: NOP<AM::Immediate<Core<System>>>(); break;
          case 0xC3: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xC4: CPY<AM::ZeroPage<Core<System>>>(); break;
          case 0xC5: CMP<AM::ZeroPage<Core<System>>>(); break;
          case 0xC6: DEC<AM::ZeroPage<Core<System>>>(); break;
          case 0xC7: SMB<AM::ZeroPage<Core<System>>,4>(); break;
          case 0xC8: INC<AM::ImpliedY<Core<System>>>(); break;
          case 0xC9: CMP<AM::Immediate<Core<System>>>(); break;
          case 0xCA: DEC<AM::ImpliedX<Core<System>>>(); break;
          case 0xCB: WAI(); break;
          case 0xCC: CPY<AM::Absolute<Core<System>>>(); break;
          case 0xCD: CMP<AM::Absolute<Core<System>>>(); break;
          case 0xCE: DEC<AM::Absolute<Core<System>>>(); break;
          case 0xCF: BBS<AM::RelativeBitBranch<Core<System>>,4>(); break;
          case 0xD0: Branch<AM::Relative<Core<System>>,P_Z,0>(); break;
          case 0xD1: CMP<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0xD2: CMP<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0xD3: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xD4: NOP<AM::ZeroPageX<Core<System>>>(); break;
          case 0xD5: CMP<AM::ZeroPageX<Core<System>>>(); break;
          case 0xD6: DEC<AM::ZeroPageX<Core<System>>>(); break;
          case 0xD7: SMB<AM::ZeroPage<Core<System>>,5>(); break;
          case 0xD8: CLD(); break;
          case 0xD9: CMP<AM::AbsoluteY<Core<System>>>(); break;
          case 0xDA: PHX(); break;
          case 0xDB: STP(); break;
          case 0xDC: NOP<AM::Absolute<Core<System>>>(); break;
          case 0xDD: CMP<AM::AbsoluteX<Core<System>>>(); break;
          case 0xDE: DEC<AM::AbsoluteXBug<Core<System>>>(); break;
          case 0xDF: BBS<AM::RelativeBitBranch<Core<System>>,5>(); break;
          case 0xE0: CPX<AM::Immediate<Core<System>>>(); break;
          case 0xE1: SBC<AM::ZeroPageXIndirect<Core<System>>>(); break;
          case 0xE2: NOP<AM::Immediate<Core<System>>>(); break;
          case 0xE3: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xE4: CPX<AM::ZeroPage<Core<System>>>(); break;
          case 0xE5: SBC<AM::ZeroPage<Core<System>>>(); break;
          case 0xE6: INC<AM::ZeroPage<Core<System>>>(); break;
          case 0xE7: SMB<AM::ZeroPage<Core<System>>,6>(); break;
          case 0xE8: INC<AM::ImpliedX<Core<System>>>(); break;
          case 0xE9: SBC<AM::Immediate<Core<System>>>(); break;
          case 0xEA: NOP<AM::Implied<Core<System>>>(); break;
          case 0xEB: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xEC: CPX<AM::Absolute<Core<System>>>(); break;
          case 0xED: SBC<AM::Absolute<Core<System>>>(); break;
          case 0xEE: INC<AM::Absolute<Core<System>>>(); break;
          case 0xEF: BBS<AM::RelativeBitBranch<Core<System>>,6>(); break;
          case 0xF0: Branch<AM::Relative<Core<System>>,P_Z,P_Z>(); break;
          case 0xF1: SBC<AM::ZeroPageIndirectY<Core<System>>>(); break;
          case 0xF2: SBC<AM::ZeroPageIndirect<Core<System>>>(); break;
          case 0xF3: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xF4: NOP<AM::ZeroPageX<Core<System>>>(); break;
          case 0xF5: SBC<AM::ZeroPageX<Core<System>>>(); break;
          case 0xF6: INC<AM::ZeroPageX<Core<System>>>(); break;
          case 0xF7: SMB<AM::ZeroPage<Core<System>>,7>(); break;
          case 0xF8: SED(); break;
          case 0xF9: SBC<AM::AbsoluteY<Core<System>>>(); break;
          case 0xFA: PLX(); break;
          case 0xFB: NOP<AM::SuperImplied<Core<System>>>(); break;
          case 0xFC: NOP<AM::Absolute<Core<System>>>(); break;
          case 0xFD: SBC<AM::AbsoluteX<Core<System>>>(); break;
          case 0xFE: INC<AM::AbsoluteXBug<Core<System>>>(); break;
          case 0xFF: BBS<AM::RelativeBitBranch<Core<System>>,7>(); break;
          }
        }
      } break;
      }
      return state == State::AWAITING_INTERRUPT || state == State::STOPPED;
    }
    void push(uint8_t value) {
      system.write_byte(0x100 | s, value, WriteType::PUSH);
      --s;
    }
    uint8_t pop() {
      ++s;
      return system.read_byte(0x100 | s, ReadType::POP);
    }
    // ...kay, thanks RESET handler
    void fake_push() {
      system.read_byte(0x100 | s, ReadType::IOP);
      --s;
    }
    uint8_t read_byte(uint16_t addr, ReadType rt) {
      return system.read_byte(addr, rt);
    }
    void write_byte(uint16_t addr, uint8_t v, WriteType wt) {
      system.write_byte(addr, v, wt);
    }
    // false -> the CPU will not do useful work if step() is called
    bool in_productive_state() {
      switch(state) {
      default:
      case State::INVALID_STATE: return false;
      case State::AWAITING_INTERRUPT: return irq || nmi_edge;
      case State::RECOVERING_FROM_RESET: return true;
      case State::RUNNING: return true;
      case State::STOPPED: return false;
      }
    }
    bool is_stopped() {
      return state == State::STOPPED;
    }
  };
}

#endif
