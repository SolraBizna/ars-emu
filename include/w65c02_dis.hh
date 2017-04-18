#ifndef W65C02DIS_HH
#define W65C02DIS_HH

#include <inttypes.h>
#include <iostream>
#include <iomanip>

namespace W65C02 {
  template<class System> class Disassembler {
    System& system;
    void outImplied(std::ostream& out, uint16_t&, const char* op) {
      out << op;
    }
    void outImpliedA(std::ostream& out, uint16_t&, const char* op) {
      out << op << " A";
    }
    void outImmediate(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " #$" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << std::dec;
    }
    void outRelativeBitBranch(std::ostream& out, uint16_t& addr,const char*op){
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr) << "," << std::setfill('0')
          << std::setw(4)
          << ((addr+((int8_t)(int)system.peek_byte(addr+1)))&0xFFFF)
          << std::dec;
      addr += 2;
    }
    void outRelative(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(4)
          << ((addr+1+((int8_t)system.peek_byte(addr)))&0xFFFF)
          << std::dec;
      addr += 1;
    }
    void outZeroPage(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << std::dec;
    }
    void outAbsolute(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr+1)
          << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr) << std::dec;
      addr += 2;
    }
    void outAbsoluteIndirect(std::ostream& out, uint16_t& addr, const char*op){
      out << op << " ($" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr+1)
          << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr) << ")" << std::dec;
      addr += 2;
    }
    void outAbsoluteXIndirect(std::ostream& out, uint16_t& addr,const char*op){
      out << op << " ($" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr+1)
          << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr) << ",X)" << std::dec;
      addr += 2;
    }
    void outAbsoluteX(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr+1)
          << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr) << ",X" << std::dec;
      addr += 2;
    }
    void outAbsoluteY(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr+1)
          << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr) << ",Y" << std::dec;
      addr += 2;
    }
    void outZeroPageIndirect(std::ostream& out, uint16_t& addr, const char*op){
      out << op << " ($" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << ")" << std::dec;
    }
    void outZeroPageX(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << ",X" << std::dec;
    }
    void outZeroPageY(std::ostream& out, uint16_t& addr, const char* op) {
      out << op << " $" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << ",Y" << std::dec;
    }
    void outZeroPageXIndirect(std::ostream& out, uint16_t& addr,const char*op){
      out << op << " ($" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << ",X)" << std::dec;
    }
    void outZeroPageIndirectY(std::ostream& out, uint16_t& addr,const char*op){
      out << op << " ($" << std::hex << std::setfill('0') << std::setw(2)
          << (int)system.peek_byte(addr++) << "),Y" << std::dec;
    }
  public:
    Disassembler(System& system) : system(system) {}
    // returns next PC
    uint16_t disassemble(uint16_t addr, std::ostream& out) {
      uint8_t opcode = system.peek_opcode(addr++);
      switch(opcode) {
      case 0x00: outImmediate(out, addr, "BRK"); break;
      case 0x01: outZeroPageXIndirect(out, addr, "ORA"); break;
      case 0x02: outZeroPage(out, addr, "NOP"); break;
      case 0x03: outImplied(out, addr, "NOP"); break;
      case 0x04: outZeroPage(out, addr, "TSB"); break;
      case 0x05: outZeroPage(out, addr, "ORA"); break;
      case 0x06: outZeroPage(out, addr, "ASL"); break;
      case 0x07: outZeroPage(out, addr, "RMB0"); break;
      case 0x08: outImplied(out, addr, "PHP"); break;
      case 0x09: outImmediate(out, addr, "ORA"); break;
      case 0x0A: outImpliedA(out, addr, "ASL"); break;
      case 0x0B: outImplied(out, addr, "NOP"); break;
      case 0x0C: outAbsolute(out, addr, "TSB"); break;
      case 0x0D: outAbsolute(out, addr, "ORA"); break;
      case 0x0E: outAbsolute(out, addr, "ASL"); break;
      case 0x0F: outRelativeBitBranch(out, addr, "BBR0"); break;
      case 0x10: outRelative(out, addr, "BPL"); break;
      case 0x11: outZeroPageIndirectY(out, addr, "ORA"); break;
      case 0x12: outZeroPageIndirect(out, addr, "ORA"); break;
      case 0x13: outImplied(out, addr, "NOP"); break;
      case 0x14: outZeroPage(out, addr, "TRB"); break;
      case 0x15: outZeroPageX(out, addr, "ORA"); break;
      case 0x16: outZeroPageX(out, addr, "ASL"); break;
      case 0x17: outZeroPage(out, addr, "RMB1"); break;
      case 0x18: outImplied(out, addr, "CLC"); break;
      case 0x19: outAbsoluteY(out, addr, "ORA"); break;
      case 0x1A: outImpliedA(out, addr, "INC"); break;
      case 0x1B: outImplied(out, addr, "NOP"); break;
      case 0x1C: outAbsolute(out, addr, "TRB"); break;
      case 0x1D: outAbsoluteX(out, addr, "ORA"); break;
      case 0x1E: outAbsoluteX(out, addr, "ASL"); break;
      case 0x1F: outRelativeBitBranch(out, addr, "BBR1"); break;
      case 0x20: outAbsolute(out, addr, "JSR"); break;
      case 0x21: outZeroPageXIndirect(out, addr, "AND"); break;
      case 0x22: outZeroPage(out, addr, "NOP"); break;
      case 0x23: outImplied(out, addr, "NOP"); break;
      case 0x24: outZeroPage(out, addr, "BIT"); break;
      case 0x25: outZeroPage(out, addr, "AND"); break;
      case 0x26: outZeroPage(out, addr, "ROL"); break;
      case 0x27: outZeroPage(out, addr, "RMB2"); break;
      case 0x28: outImplied(out, addr, "PLP"); break;
      case 0x29: outImmediate(out, addr, "AND"); break;
      case 0x2A: outImpliedA(out, addr, "ROL"); break;
      case 0x2B: outImplied(out, addr, "NOP"); break;
      case 0x2C: outAbsolute(out, addr, "BIT"); break;
      case 0x2D: outAbsolute(out, addr, "AND"); break;
      case 0x2E: outAbsolute(out, addr, "ROL"); break;
      case 0x2F: outRelativeBitBranch(out, addr, "BBR2"); break;
      case 0x30: outRelative(out, addr, "BMI"); break;
      case 0x31: outZeroPageIndirectY(out, addr, "AND"); break;
      case 0x32: outZeroPageIndirect(out, addr, "AND"); break;
      case 0x33: outImplied(out, addr, "NOP"); break;
      case 0x34: outZeroPageX(out, addr, "BIT"); break;
      case 0x35: outZeroPageX(out, addr, "AND"); break;
      case 0x36: outZeroPageX(out, addr, "ROL"); break;
      case 0x37: outZeroPage(out, addr, "RMB3"); break;
      case 0x38: outImplied(out, addr, "SEC"); break;
      case 0x39: outAbsoluteY(out, addr, "AND"); break;
      case 0x3A: outImpliedA(out, addr, "DEC"); break;
      case 0x3B: outImplied(out, addr, "NOP"); break;
      case 0x3C: outAbsoluteX(out, addr, "BIT"); break;
      case 0x3D: outAbsoluteX(out, addr, "AND"); break;
      case 0x3E: outAbsoluteX(out, addr, "ROL"); break;
      case 0x3F: outRelativeBitBranch(out, addr, "BBR3"); break;
      case 0x40: outImplied(out, addr, "RTI"); break;
      case 0x41: outZeroPageXIndirect(out, addr, "EOR"); break;
      case 0x42: outZeroPage(out, addr, "NOP"); break;
      case 0x43: outImplied(out, addr, "NOP"); break;
      case 0x44: outZeroPage(out, addr, "NOP"); break;
      case 0x45: outZeroPage(out, addr, "EOR"); break;
      case 0x46: outZeroPage(out, addr, "LSR"); break;
      case 0x47: outZeroPage(out, addr, "RMB4"); break;
      case 0x48: outImplied(out, addr, "PHA"); break;
      case 0x49: outImmediate(out, addr, "EOR"); break;
      case 0x4A: outImpliedA(out, addr, "LSR"); break;
      case 0x4B: outImplied(out, addr, "NOP"); break;
      case 0x4C: outAbsolute(out, addr, "JMP"); break;
      case 0x4D: outAbsolute(out, addr, "EOR"); break;
      case 0x4E: outAbsolute(out, addr, "LSR"); break;
      case 0x4F: outRelativeBitBranch(out, addr, "BBR4"); break;
      case 0x50: outRelative(out, addr, "BVC"); break;
      case 0x51: outZeroPageIndirectY(out, addr, "EOR"); break;
      case 0x52: outZeroPageIndirect(out, addr, "EOR"); break;
      case 0x53: outImplied(out, addr, "NOP"); break;
      case 0x54: outZeroPageX(out, addr, "NOP"); break;
      case 0x55: outZeroPageX(out, addr, "EOR"); break;
      case 0x56: outZeroPageX(out, addr, "LSR"); break;
      case 0x57: outZeroPage(out, addr, "RMB5"); break;
      case 0x58: outImplied(out, addr, "CLI"); break;
      case 0x59: outAbsoluteY(out, addr, "EOR"); break;
      case 0x5A: outImplied(out, addr, "PHY"); break;
      case 0x5B: outImplied(out, addr, "NOP"); break;
      case 0x5C: outAbsolute(out, addr, "NOP"); break;
      case 0x5D: outAbsoluteX(out, addr, "EOR"); break;
      case 0x5E: outAbsoluteX(out, addr, "LSR"); break;
      case 0x5F: outRelativeBitBranch(out, addr, "BBR5"); break;
      case 0x60: outImplied(out, addr, "RTS"); break;
      case 0x61: outZeroPageXIndirect(out, addr, "ADC"); break;
      case 0x62: outZeroPage(out, addr, "NOP"); break;
      case 0x63: outImplied(out, addr, "NOP"); break;
      case 0x64: outZeroPage(out, addr, "STZ"); break;
      case 0x65: outZeroPage(out, addr, "ADC"); break;
      case 0x66: outZeroPage(out, addr, "ROR"); break;
      case 0x67: outZeroPage(out, addr, "RMB6"); break;
      case 0x68: outImplied(out, addr, "PLA"); break;
      case 0x69: outImmediate(out, addr, "ADC"); break;
      case 0x6A: outImpliedA(out, addr, "ROR"); break;
      case 0x6B: outImplied(out, addr, "NOP"); break;
      case 0x6C: outAbsoluteIndirect(out, addr, "JMP"); break;
      case 0x6D: outAbsolute(out, addr, "ADC"); break;
      case 0x6E: outAbsolute(out, addr, "ROR"); break;
      case 0x6F: outRelativeBitBranch(out, addr, "BBR6"); break;
      case 0x70: outRelative(out, addr, "BVS"); break;
      case 0x71: outZeroPageIndirectY(out, addr, "ADC"); break;
      case 0x72: outZeroPageIndirect(out, addr, "ADC"); break;
      case 0x73: outImplied(out, addr, "NOP"); break;
      case 0x74: outZeroPageX(out, addr, "STZ"); break;
      case 0x75: outZeroPageX(out, addr, "ADC"); break;
      case 0x76: outZeroPageX(out, addr, "ROR"); break;
      case 0x77: outZeroPage(out, addr, "RMB7"); break;
      case 0x78: outImplied(out, addr, "SEI"); break;
      case 0x79: outAbsoluteY(out, addr, "ADC"); break;
      case 0x7A: outImplied(out, addr, "PLY"); break;
      case 0x7B: outImplied(out, addr, "NOP"); break;
      case 0x7C: outAbsoluteXIndirect(out, addr, "JMP"); break;
      case 0x7D: outAbsoluteX(out, addr, "ADC"); break;
      case 0x7E: outAbsoluteX(out, addr, "ROR"); break;
      case 0x7F: outRelativeBitBranch(out, addr, "BBR7"); break;
      case 0x80: outRelative(out, addr, "BRA"); break;
      case 0x81: outZeroPageXIndirect(out, addr, "STA"); break;
      case 0x82: outZeroPage(out, addr, "NOP"); break;
      case 0x83: outImplied(out, addr, "NOP"); break;
      case 0x84: outZeroPage(out, addr, "STY"); break;
      case 0x85: outZeroPage(out, addr, "STA"); break;
      case 0x86: outZeroPage(out, addr, "STX"); break;
      case 0x87: outZeroPage(out, addr, "SMB0"); break;
      case 0x88: outImplied(out, addr, "DEY"); break;
      case 0x89: outImmediate(out, addr, "BIT"); break;
      case 0x8A: outImplied(out, addr, "TXA"); break;
      case 0x8B: outImplied(out, addr, "NOP"); break;
      case 0x8C: outAbsolute(out, addr, "STY"); break;
      case 0x8D: outAbsolute(out, addr, "STA"); break;
      case 0x8E: outAbsolute(out, addr, "STX"); break;
      case 0x8F: outRelativeBitBranch(out, addr, "BBS0"); break;
      case 0x90: outRelative(out, addr, "BCC"); break;
      case 0x91: outZeroPageIndirectY(out, addr, "STA"); break;
      case 0x92: outZeroPageIndirect(out, addr, "STA"); break;
      case 0x93: outImplied(out, addr, "NOP"); break;
      case 0x94: outZeroPageX(out, addr, "STY"); break;
      case 0x95: outZeroPageX(out, addr, "STA"); break;
      case 0x96: outZeroPageY(out, addr, "STX"); break;
      case 0x97: outZeroPage(out, addr, "SMB1"); break;
      case 0x98: outImplied(out, addr, "TYA"); break;
      case 0x99: outAbsoluteY(out, addr, "STA"); break;
      case 0x9A: outImplied(out, addr, "TXS"); break;
      case 0x9B: outImplied(out, addr, "NOP"); break;
      case 0x9C: outAbsolute(out, addr, "STZ"); break;
      case 0x9D: outAbsoluteX(out, addr, "STA"); break;
      case 0x9E: outAbsoluteX(out, addr, "STZ"); break;
      case 0x9F: outRelativeBitBranch(out, addr, "BBS1"); break;
      case 0xA0: outImmediate(out, addr, "LDY"); break;
      case 0xA1: outZeroPageXIndirect(out, addr, "LDA"); break;
      case 0xA2: outImmediate(out, addr, "LDX"); break;
      case 0xA3: outImplied(out, addr, "NOP"); break;
      case 0xA4: outZeroPage(out, addr, "LDY"); break;
      case 0xA5: outZeroPage(out, addr, "LDA"); break;
      case 0xA6: outZeroPage(out, addr, "LDX"); break;
      case 0xA7: outZeroPage(out, addr, "SMB2"); break;
      case 0xA8: outImplied(out, addr, "TAY"); break;
      case 0xA9: outImmediate(out, addr, "LDA"); break;
      case 0xAA: outImplied(out, addr, "TAX"); break;
      case 0xAB: outImplied(out, addr, "NOP"); break;
      case 0xAC: outAbsolute(out, addr, "LDY"); break;
      case 0xAD: outAbsolute(out, addr, "LDA"); break;
      case 0xAE: outAbsolute(out, addr, "LDX"); break;
      case 0xAF: outRelativeBitBranch(out, addr, "BBS2"); break;
      case 0xB0: outRelative(out, addr, "BCS"); break;
      case 0xB1: outZeroPageIndirectY(out, addr, "LDA"); break;
      case 0xB2: outZeroPageIndirect(out, addr, "LDA"); break;
      case 0xB3: outImplied(out, addr, "NOP"); break;
      case 0xB4: outZeroPageX(out, addr, "LDY"); break;
      case 0xB5: outZeroPageX(out, addr, "LDA"); break;
      case 0xB6: outZeroPageY(out, addr, "LDX"); break;
      case 0xB7: outZeroPage(out, addr, "SMB3"); break;
      case 0xB8: outImplied(out, addr, "CLV"); break;
      case 0xB9: outAbsoluteY(out, addr, "LDA"); break;
      case 0xBA: outImplied(out, addr, "TSX"); break;
      case 0xBB: outImplied(out, addr, "NOP"); break;
      case 0xBC: outAbsoluteX(out, addr, "LDY"); break;
      case 0xBD: outAbsoluteX(out, addr, "LDA"); break;
      case 0xBE: outAbsoluteY(out, addr, "LDX"); break;
      case 0xBF: outRelativeBitBranch(out, addr, "BBS3"); break;
      case 0xC0: outImmediate(out, addr, "CPY"); break;
      case 0xC1: outZeroPageXIndirect(out, addr, "CMP"); break;
      case 0xC2: outImmediate(out, addr, "NOP"); break;
      case 0xC3: outImplied(out, addr, "NOP"); break;
      case 0xC4: outZeroPage(out, addr, "CPY"); break;
      case 0xC5: outZeroPage(out, addr, "CMP"); break;
      case 0xC6: outZeroPage(out, addr, "DEC"); break;
      case 0xC7: outZeroPage(out, addr, "SMB4"); break;
      case 0xC8: outImplied(out, addr, "INY"); break;
      case 0xC9: outImmediate(out, addr, "CMP"); break;
      case 0xCA: outImplied(out, addr, "DEX"); break;
      case 0xCB: outImplied(out, addr, "WAI"); break;
      case 0xCC: outAbsolute(out, addr, "CPY"); break;
      case 0xCD: outAbsolute(out, addr, "CMP"); break;
      case 0xCE: outAbsolute(out, addr, "DEC"); break;
      case 0xCF: outRelativeBitBranch(out, addr, "BBS4"); break;
      case 0xD0: outRelative(out, addr, "BNE"); break;
      case 0xD1: outZeroPageIndirectY(out, addr, "CMP"); break;
      case 0xD2: outZeroPageIndirect(out, addr, "CMP"); break;
      case 0xD3: outImplied(out, addr, "NOP"); break;
      case 0xD4: outZeroPageX(out, addr, "NOP"); break;
      case 0xD5: outZeroPageX(out, addr, "CMP"); break;
      case 0xD6: outZeroPageX(out, addr, "DEC"); break;
      case 0xD7: outZeroPage(out, addr, "SMB5"); break;
      case 0xD8: outImplied(out, addr, "CLD"); break;
      case 0xD9: outAbsoluteY(out, addr, "CMP"); break;
      case 0xDA: outImplied(out, addr, "PHX"); break;
      case 0xDB: outImplied(out, addr, "STP"); break;
      case 0xDC: outAbsoluteX(out, addr, "NOP"); break;
      case 0xDD: outAbsoluteX(out, addr, "CMP"); break;
      case 0xDE: outAbsoluteX(out, addr, "DEC"); break;
      case 0xDF: outRelativeBitBranch(out, addr, "BBS5"); break;
      case 0xE0: outImmediate(out, addr, "CPX"); break;
      case 0xE1: outZeroPageXIndirect(out, addr, "SBC"); break;
      case 0xE2: outImmediate(out, addr, "NOP"); break;
      case 0xE3: outImplied(out, addr, "NOP"); break;
      case 0xE4: outZeroPage(out, addr, "CPX"); break;
      case 0xE5: outZeroPage(out, addr, "SBC"); break;
      case 0xE6: outZeroPage(out, addr, "INC"); break;
      case 0xE7: outZeroPage(out, addr, "SMB6"); break;
      case 0xE8: outImplied(out, addr, "INX"); break;
      case 0xE9: outImmediate(out, addr, "SBC"); break;
      case 0xEA: outImplied(out, addr, "NOP"); break;
      case 0xEB: outImplied(out, addr, "NOP"); break;
      case 0xEC: outAbsolute(out, addr, "CPX"); break;
      case 0xED: outAbsolute(out, addr, "SBC"); break;
      case 0xEE: outAbsolute(out, addr, "INC"); break;
      case 0xEF: outRelativeBitBranch(out, addr, "BBS6"); break;
      case 0xF0: outRelative(out, addr, "BEQ"); break;
      case 0xF1: outZeroPageIndirectY(out, addr, "SBC"); break;
      case 0xF2: outZeroPageIndirect(out, addr, "SBC"); break;
      case 0xF3: outImplied(out, addr, "NOP"); break;
      case 0xF4: outZeroPageX(out, addr, "NOP"); break;
      case 0xF5: outZeroPageX(out, addr, "SBC"); break;
      case 0xF6: outZeroPageX(out, addr, "INC"); break;
      case 0xF7: outZeroPage(out, addr, "SMB7"); break;
      case 0xF8: outImplied(out, addr, "SED"); break;
      case 0xF9: outAbsoluteY(out, addr, "SBC"); break;
      case 0xFA: outImplied(out, addr, "PLX"); break;
      case 0xFB: outImplied(out, addr, "NOP"); break;
      case 0xFC: outAbsoluteX(out, addr, "NOP"); break;
      case 0xFD: outAbsoluteX(out, addr, "SBC"); break;
      case 0xFE: outAbsoluteX(out, addr, "INC"); break;
      case 0xFF: outRelativeBitBranch(out, addr, "BBS7"); break;
      }
      return addr;
    }
  };
}

#endif
