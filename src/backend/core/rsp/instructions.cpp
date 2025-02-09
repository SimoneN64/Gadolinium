#include <Mem.hpp>
#include <RCP.hpp>
#include <RSP.hpp>
#include <RSQ.hpp>
#include <core/registers/Registers.hpp>
#include <log.hpp>

namespace n64 {
FORCE_INLINE bool AcquireSemaphore(RSP &rsp) {
  if (rsp.semaphore) {
    return true;
  }

  rsp.semaphore = true;
  return false;
}

FORCE_INLINE void ReleaseSemaphore(RSP &rsp) { rsp.semaphore = false; }

FORCE_INLINE int SignExt7bit(const u8 val, const int sa) {
  const s8 sval = val << 1 & 0x80 | val;

  const s32 sval32 = sval;
  const u32 val32 = sval32;
  return val32 << sa;
}

FORCE_INLINE auto GetCop0Reg(RSP &rsp, const RDP &rdp, const u8 index) -> u32 {
  switch (index) {
  case 0:
    return rsp.lastSuccessfulSPAddr.raw;
  case 1:
    return rsp.lastSuccessfulDRAMAddr.raw;
  case 2:
  case 3:
    return rsp.spDMALen.raw;
  case 4:
    return rsp.spStatus.raw;
  case 5:
    return rsp.spStatus.dmaFull;
  case 6:
    return rsp.spStatus.dmaBusy;
  case 7:
    return AcquireSemaphore(rsp);
  case 8:
    return rdp.dpc.start;
  case 9:
    return rdp.dpc.end;
  case 10:
    return rdp.dpc.current;
  case 11:
    return rdp.dpc.status.raw;
  case 12:
    return rdp.dpc.clock;
  case 13:
    return rdp.dpc.status.cmdBusy;
  case 14:
    return rdp.dpc.status.pipeBusy;
  case 15:
    return rdp.dpc.status.tmemBusy;
  default:
    Util::panic("Unhandled RSP COP0 register read at index {}", index);
    return 0;
  }
}

FORCE_INLINE void SetCop0Reg(Mem &mem, const u8 index, const u32 val) {
  MMIO &mmio = mem.mmio;
  RSP &rsp = mmio.rsp;
  RDP &rdp = mmio.rdp;
  switch (index) {
  case 0:
    rsp.spDMASPAddr.raw = val;
    break;
  case 1:
    rsp.spDMADRAMAddr.raw = val;
    break;
  case 2:
    rsp.spDMALen.raw = val;
    rsp.DMA<false>();
    break;
  case 3:
    rsp.spDMALen.raw = val;
    rsp.DMA<true>();
    break;
  case 4:
    rsp.WriteStatus(val);
    break;
  case 7:
    if (val == 0) {
      ReleaseSemaphore(rsp);
    } else {
      Util::panic("Write with non-zero value to RSP_COP0_RESERVED ({})", val);
    }
    break;
  case 8:
    rdp.WriteStart(val);
    break;
  case 9:
    rdp.WriteEnd(val);
    break;
  case 11:
    rdp.WriteStatus(val);
    break;
  default:
    Util::panic("Unhandled RSP COP0 register write at index {}", index);
  }
}

FORCE_INLINE VPR Broadcast(const VPR &vt, const int l0, const int l1, const int l2, const int l3, const int l4,
                           const int l5, const int l6, const int l7) {
  VPR vte{};
  vte.element[ELEMENT_INDEX(0)] = vt.element[ELEMENT_INDEX(l0)];
  vte.element[ELEMENT_INDEX(1)] = vt.element[ELEMENT_INDEX(l1)];
  vte.element[ELEMENT_INDEX(2)] = vt.element[ELEMENT_INDEX(l2)];
  vte.element[ELEMENT_INDEX(3)] = vt.element[ELEMENT_INDEX(l3)];
  vte.element[ELEMENT_INDEX(4)] = vt.element[ELEMENT_INDEX(l4)];
  vte.element[ELEMENT_INDEX(5)] = vt.element[ELEMENT_INDEX(l5)];
  vte.element[ELEMENT_INDEX(6)] = vt.element[ELEMENT_INDEX(l6)];
  vte.element[ELEMENT_INDEX(7)] = vt.element[ELEMENT_INDEX(l7)];
  return vte;
}

#ifdef SIMD_SUPPORT
void RSP::SetVTE(const VPR &vt, u8 e) {
  e &= 0xf;
  switch (e & 0xf) {
  case 0 ... 1:
    vte = vt;
    break;
  case 2:
    vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt.single, 0xF5), 0xF5);
    break;
  case 3:
    vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt.single, 0xA0), 0xA0);
    break;
  case 4:
    vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt.single, 0xFF), 0xFF);
    break;
  case 5:
    vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt.single, 0xAA), 0xAA);
    break;
  case 6:
    vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt.single, 0x55), 0x55);
    break;
  case 7:
    vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt.single, 0x00), 0x00);
    break;
  case 8 ... 15:
    {
      const int index = ELEMENT_INDEX(e - 8);
      vte.single = _mm_set1_epi16(vt.element[index]);
    }
    break;
  }
}
#else
void RSP::SetVTE(const VPR &vt, u8 e) {
  e &= 0xf;
  switch (e) {
  case 0 ... 1:
    vte = vt;
    break;
  case 2 ... 3:
    vte = Broadcast(vt, e - 2, e - 2, e, e, e + 2, e + 2, e + 4, e + 4);
    break;
  case 4 ... 7:
    vte = Broadcast(vt, e - 4, e - 4, e - 4, e - 4, e, e, e, e);
    break;
  case 8 ... 15:
    {
      int index = ELEMENT_INDEX(e - 8);
      for (int i = 0; i < 8; i++) {
        vte.element[i] = vt.element[index];
      }
    }
    break;
  }
}
#endif

void RSP::add(const u32 instr) { gpr[RD(instr)] = gpr[RS(instr)] + gpr[RT(instr)]; }

void RSP::addi(const u32 instr) {
  const s32 op1 = gpr[RS(instr)];
  const s16 op2 = instr;
  const s32 result = op1 + op2;
  gpr[RT(instr)] = result;
}

void RSP::and_(const u32 instr) { gpr[RD(instr)] = gpr[RT(instr)] & gpr[RS(instr)]; }

void RSP::andi(const u32 instr) { gpr[RT(instr)] = gpr[RS(instr)] & (u16)instr; }

void RSP::cfc2(const u32 instr) {
  s16 value = 0;
  switch (RD(instr) & 3) {
  case 0:
    value = GetVCO();
    break;
  case 1:
    value = GetVCC();
    break;
  case 2 ... 3:
    value = GetVCE();
    break;
  }

  gpr[RT(instr)] = value;
}

void RSP::ctc2(const u32 instr) {
  const u16 value = gpr[RT(instr)];
  switch (RD(instr) & 3) {
  case 0:
    for (int i = 0; i < 8; i++) {
      vco.h.element[ELEMENT_INDEX(i)] = ((value >> (i + 8)) & 1) == 1 ? 0xFFFF : 0;
      vco.l.element[ELEMENT_INDEX(i)] = ((value >> i) & 1) == 1 ? 0xFFFF : 0;
    }
    break;
  case 1:
    for (int i = 0; i < 8; i++) {
      vcc.h.element[ELEMENT_INDEX(i)] = ((value >> (i + 8)) & 1) == 1 ? 0xFFFF : 0;
      vcc.l.element[ELEMENT_INDEX(i)] = ((value >> i) & 1) == 1 ? 0xFFFF : 0;
    }
    break;
  case 2:
  case 3:
    for (int i = 0; i < 8; i++) {
      vce.element[ELEMENT_INDEX(i)] = ((value >> i) & 1) == 1 ? 0xFFFF : 0;
    }
    break;
  }
}

void RSP::b(const u32 instr, const bool cond) {
  const u32 address = ((instr & 0xFFFF) << 2) + pc;
  branch(address, cond);
}

void RSP::blink(const u32 instr, const bool cond) {
  b(instr, cond);
  gpr[31] = pc + 4;
}

void RSP::lb(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  gpr[RT(instr)] = (s32)(s8)ReadByte(address);
}

void RSP::lh(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  gpr[RT(instr)] = (s32)(s16)ReadHalf(address);
}

void RSP::lw(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  gpr[RT(instr)] = ReadWord(address);
}

void RSP::lbu(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  gpr[RT(instr)] = ReadByte(address);
}

void RSP::lhu(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  gpr[RT(instr)] = ReadHalf(address);
}

void RSP::lui(const u32 instr) {
  u32 imm = ((u16)instr) << 16;
  gpr[RT(instr)] = imm;
}

#define OFFSET(x) ((x) & 0x7F)

void RSP::lqv(const u32 instr) {
  int e = E1(instr);
  u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  u32 end = ((addr & ~15) + 15);

  for (int i = 0; addr + i <= end && i + e < 16; i++) {
    vpr[VT(instr)].byte[BYTE_INDEX(i + e)] = ReadByte(addr + i);
  }
}

void RSP::lpv(const u32 instr) {
  const int e = E1(instr);
  u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 3);

  const int addrOffset = addr & 7;
  addr &= ~7;

  for (int elem = 0; elem < 8; elem++) {
    const int elemOffset = (16 - e + (elem + addrOffset)) & 0xF;

    u16 value = ReadByte(addr + elemOffset);
    value <<= 8;
    vpr[VT(instr)].element[ELEMENT_INDEX(elem)] = value;
  }
}

void RSP::luv(const u32 instr) {
  u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 3);

  const int e = E1(instr);

  const int addrOffset = addr & 7;
  addr &= ~7;

  for (int elem = 0; elem < 8; elem++) {
    const int elemOffset = (16 - e + (elem + addrOffset)) & 0xF;

    u16 value = ReadByte(addr + elemOffset);
    value <<= 7;
    vpr[VT(instr)].element[ELEMENT_INDEX(elem)] = value;
  }
}

void RSP::suv(const u32 instr) {
  u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 3);

  const int start = E1(instr);
  const int end = start + 8;

  for (int offset = start; offset < end; offset++) {
    if ((offset & 15) < 8) {
      WriteByte(addr++, vpr[VT(instr)].element[ELEMENT_INDEX(offset & 7)] >> 7);
    } else {
      WriteByte(addr++, vpr[VT(instr)].byte[BYTE_INDEX((offset & 7) << 1)]);
    }
  }
}

void RSP::ldv(const u32 instr) {
  const int e = E1(instr);
  u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 3);
  const u32 end = e + 8 > 16 ? 16 : e + 8;

  for (int i = e; i < end; i++) {
    vpr[VT(instr)].byte[BYTE_INDEX(i)] = ReadByte(addr);
    addr++;
  }
}

void RSP::lsv(const u32 instr) {
  const u8 e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 1);
  const u16 val = ReadHalf(addr);
  vpr[VT(instr)].byte[BYTE_INDEX(e)] = val >> 8;
  if (e < 15) {
    vpr[VT(instr)].byte[BYTE_INDEX(e + 1)] = val;
  }
}

void RSP::lbv(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 0);
  vpr[VT(instr)].byte[BYTE_INDEX(E1(instr))] = ReadByte(address);
}

void RSP::llv(const u32 instr) {
  const int e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 2);

  for (int i = 0; i < 4; i++) {
    const int elem = i + e;
    if (elem > 15) {
      break;
    }

    vpr[VT(instr)].byte[BYTE_INDEX(elem)] = ReadByte(addr + i);
  }
}

void RSP::j(const u32 instr) {
  const u32 target = (instr & 0x3ffffff) << 2;
  nextPC = target;
}

void RSP::jal(const u32 instr) {
  j(instr);
  gpr[31] = pc + 4;
}

void RSP::jr(const u32 instr) { nextPC = gpr[RS(instr)]; }

void RSP::jalr(const u32 instr) {
  jr(instr);
  gpr[RD(instr)] = pc + 4;
}

void RSP::nor(const u32 instr) { gpr[RD(instr)] = ~(gpr[RT(instr)] | gpr[RS(instr)]); }

void RSP::ori(const u32 instr) {
  const s32 op1 = gpr[RS(instr)];
  const u32 op2 = instr & 0xffff;
  const s32 result = op1 | op2;
  gpr[RT(instr)] = result;
}

void RSP::xori(const u32 instr) {
  const s32 op1 = gpr[RS(instr)];
  const u32 op2 = instr & 0xffff;
  const s32 result = op1 ^ op2;
  gpr[RT(instr)] = result;
}

void RSP::or_(const u32 instr) { gpr[RD(instr)] = gpr[RT(instr)] | gpr[RS(instr)]; }

void RSP::xor_(const u32 instr) { gpr[RD(instr)] = gpr[RT(instr)] ^ gpr[RS(instr)]; }

void RSP::sb(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  WriteByte(address, gpr[RT(instr)]);
}

void RSP::sh(const u32 instr) {
  const s16 imm = s16(instr);
  const u32 address = gpr[RS(instr)] + imm;
  WriteHalf(address, gpr[RT(instr)]);
}

void RSP::sw(const u32 instr) {
  const u32 address = gpr[BASE(instr)] + (s16)instr;
  WriteWord(address, gpr[RT(instr)]);
}

void RSP::swv(const u32 instr) {
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  int base = address & 7;
  address &= ~7;

  for (int i = E1(instr); i < E1(instr) + 16; i++) {
    WriteByte(address + (base & 15), vpr[VT(instr)].byte[BYTE_INDEX(i & 15)]);
    base++;
  }
}

void RSP::sub(const u32 instr) { gpr[RD(instr)] = gpr[RS(instr)] - gpr[RT(instr)]; }

void RSP::sqv(const u32 instr) {
  const int e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  const u32 end = ((addr & ~15) + 15);

  for (int i = 0; addr + i <= end; i++) {
    WriteByte(addr + i, vpr[VT(instr)].byte[BYTE_INDEX((i + e) & 15)]);
  }
}

void RSP::spv(const u32 instr) {
  const int e = E1(instr);
  u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 3);

  const int start = e;
  const int end = start + 8;

  for (int offset = start; offset < end; offset++) {
    if ((offset & 15) < 8) {
      WriteByte(addr++, vpr[VT(instr)].byte[BYTE_INDEX((offset & 7) << 1)]);
    } else {
      WriteByte(addr++, vpr[VT(instr)].element[ELEMENT_INDEX(offset & 7)] >> 7);
    }
  }
}

void RSP::srv(const u32 instr) {
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  const int start = E1(instr);
  const int end = start + (address & 15);
  const int base = 16 - (address & 15);
  address &= ~15;
  for (int i = start; i < end; i++) {
    WriteByte(address++, vpr[VT(instr)].byte[BYTE_INDEX((i + base) & 0xF)]);
  }
}

void RSP::shv(const u32 instr) {
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);

  const u32 in_addr_offset = address & 0x7;
  address &= ~0x7;

  const int e = E1(instr);

  for (int i = 0; i < 8; i++) {
    const int byte_index = (i * 2) + e;
    u16 val = vpr[VT(instr)].byte[BYTE_INDEX(byte_index & 15)] << 1;
    val |= vpr[VT(instr)].byte[BYTE_INDEX((byte_index + 1) & 15)] >> 7;
    u8 b = val & 0xFF;

    const int ofs = in_addr_offset + (i * 2);
    WriteByte(address + (ofs & 0xF), b);
  }
}

void RSP::lhv(const u32 instr) {
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);

  const u32 in_addr_offset = address & 0x7;
  address &= ~0x7;

  const int e = E1(instr);

  for (int i = 0; i < 8; i++) {
    const int ofs = ((16 - e) + (i * 2) + in_addr_offset) & 0xF;
    u16 val = ReadByte(address + ofs);
    val <<= 7;
    vpr[VT(instr)].element[ELEMENT_INDEX(i)] = val;
  }
}

void RSP::lfv(const u32 instr) {
  VPR &vt = vpr[VT(instr)];
  const int start = E1(instr);
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  const u32 base = (address & 7) - start;
  address &= ~7;

  const int end = std::min(start + 8, 16);

  // TODO: should be possible to do with one loop
  VPR tmp{};
  for (u32 offset = 0; offset < 4; offset++) {
    tmp.element[ELEMENT_INDEX(offset + 0)] = ReadByte(address + (base + offset * 4 + 0 & 15)) << 7;
    tmp.element[ELEMENT_INDEX(offset + 4)] = ReadByte(address + (base + offset * 4 + 8 & 15)) << 7;
  }

  for (u32 offset = start; offset < end; offset++) {
    vt.byte[BYTE_INDEX(offset)] = tmp.byte[BYTE_INDEX(offset)];
  }
}

void RSP::lrv(const u32 instr) {
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  const int e = E1(instr);
  const int start = 16 - ((address & 15) - e);
  address &= ~15;

  for (int i = start; i < 16; i++) {
    vpr[VT(instr)].byte[BYTE_INDEX(i & 0xF)] = ReadByte(address++);
  }
}

void RSP::sfv(const u32 instr) {
  const VPR &vt = vpr[VT(instr)];
  u32 address = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  const int base = address & 7;
  address &= ~7;
  const int e = E1(instr);

  u8 values[4] = {0, 0, 0, 0};

  switch (e) {
  case 0:
  case 15:
    values[0] = vt.element[ELEMENT_INDEX(0)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(1)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(2)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(3)] >> 7;
    break;
  case 1:
    values[0] = vt.element[ELEMENT_INDEX(6)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(7)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(4)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(5)] >> 7;
    break;
  case 4:
    values[0] = vt.element[ELEMENT_INDEX(1)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(2)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(3)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(0)] >> 7;
    break;
  case 5:
    values[0] = vt.element[ELEMENT_INDEX(7)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(4)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(5)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(6)] >> 7;
    break;
  case 8:
    values[0] = vt.element[ELEMENT_INDEX(4)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(5)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(6)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(7)] >> 7;
    break;
  case 11:
    values[0] = vt.element[ELEMENT_INDEX(3)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(0)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(1)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(2)] >> 7;
    break;
  case 12:
    values[0] = vt.element[ELEMENT_INDEX(5)] >> 7;
    values[1] = vt.element[ELEMENT_INDEX(6)] >> 7;
    values[2] = vt.element[ELEMENT_INDEX(7)] >> 7;
    values[3] = vt.element[ELEMENT_INDEX(4)] >> 7;
    break;
  default:
    break;
  }

  for (int i = 0; i < 4; i++) {
    WriteByte(address + ((base + (i << 2)) & 15), values[i]);
  }
}

void RSP::sbv(const u32 instr) {
  const int e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 0);

  WriteByte(addr, vpr[VT(instr)].byte[BYTE_INDEX(e & 0xF)]);
}

void RSP::sdv(const u32 instr) {
  const int e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 3);

  for (int i = 0; i < 8; i++) {
    WriteByte(addr + i, vpr[VT(instr)].byte[BYTE_INDEX((i + e) & 0xF)]);
  }
}

void RSP::ssv(const u32 instr) {
  const int e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 1);

  const u8 hi = vpr[VT(instr)].byte[BYTE_INDEX(e & 15)];
  const u8 lo = vpr[VT(instr)].byte[BYTE_INDEX((e + 1) & 15)];
  const u16 val = (u16)hi << 8 | lo;

  WriteHalf(addr, val);
}

void RSP::slv(const u32 instr) {
  const int e = E1(instr);
  const u32 addr = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 2);

  for (int i = 0; i < 4; i++) {
    WriteByte(addr + i, vpr[VT(instr)].byte[BYTE_INDEX((i + e) & 0xF)]);
  }
}

void RSP::stv(const u32 instr) {
  u32 base = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  const u32 addrOffset = base & 0x7;
  base &= ~0x7;

  const u8 e = E1(instr) >> 1;

  for (int i = 0; i < 8; i++) {
    const u32 address = base;
    const u32 offset = (i << 1) + addrOffset;

    const int reg = (VT(instr) & 0x18) | ((i + e) & 0x7);

    const u16 val = vpr[reg].element[ELEMENT_INDEX(i & 0x7)];
    const u16 hi = (val >> 8) & 0xFF;
    const u16 lo = (val >> 0) & 0xFF;

    WriteByte(address + ((offset + 0) & 0xF), hi);
    WriteByte(address + ((offset + 1) & 0xF), lo);
  }
}

void RSP::ltv(const u32 instr) {
  u32 base = gpr[BASE(instr)] + SignExt7bit(OFFSET(instr), 4);
  base &= ~0x7;

  const u8 e = E1(instr);

  for (int i = 0; i < 8; i++) {
    const u32 address = base;
    const u32 offset = (i << 1) + e + (base & 8);

    const u16 hi = ReadByte(address + (offset & 0xF));
    const u16 lo = ReadByte(address + ((offset + 1) & 0xF));

    const int reg = (VT(instr) & 0x18) | ((i + (e >> 1)) & 0x7);

    vpr[reg].element[ELEMENT_INDEX(i & 0x7)] = (hi << 8) | lo;
  }
}

void RSP::sllv(const u32 instr) {
  const u8 sa = (gpr[RS(instr)]) & 0x1F;
  const u32 rt = gpr[RT(instr)];
  const u32 result = rt << sa;
  gpr[RD(instr)] = result;
}

void RSP::srlv(const u32 instr) {
  const u8 sa = (gpr[RS(instr)]) & 0x1F;
  const u32 rt = gpr[RT(instr)];
  const u32 result = rt >> sa;
  gpr[RD(instr)] = result;
}

void RSP::srav(const u32 instr) {
  const u8 sa = gpr[RS(instr)] & 0x1F;
  const s32 rt = gpr[RT(instr)];
  const s32 result = rt >> sa;
  gpr[RD(instr)] = result;
}

void RSP::sll(const u32 instr) {
  const u8 sa = (instr >> 6) & 0x1f;
  gpr[RD(instr)] = (u32)gpr[RT(instr)] << sa;
}

void RSP::srl(const u32 instr) {
  const u8 sa = (instr >> 6) & 0x1f;
  gpr[RD(instr)] = (u32)gpr[RT(instr)] >> sa;
}

void RSP::sra(const u32 instr) {
  const u8 sa = (instr >> 6) & 0x1f;
  gpr[RD(instr)] = gpr[RT(instr)] >> sa;
}

void RSP::slt(const u32 instr) { gpr[RD(instr)] = gpr[RS(instr)] < gpr[RT(instr)]; }

void RSP::sltu(const u32 instr) { gpr[RD(instr)] = (u32)gpr[RS(instr)] < (u32)gpr[RT(instr)]; }

void RSP::slti(const u32 instr) {
  const s32 imm = (s16)instr;
  gpr[RT(instr)] = gpr[RS(instr)] < imm;
}

void RSP::sltiu(const u32 instr) {
  const s32 imm = (s16)instr;
  gpr[RT(instr)] = (u32)gpr[RS(instr)] < imm;
}

FORCE_INLINE s16 signedClamp(const s64 val) {
  if (val < -32768)
    return -32768;
  if (val > 32767)
    return 32767;
  return val;
}

FORCE_INLINE u16 unsignedClamp(const s64 val) {
  if (val < 0)
    return 0;
  if (val > 32767)
    return 65535;
  return val;
}

#ifdef SIMD_SUPPORT
void RSP::vabs(const u32 instr) {
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  const m128i isZero = _mm_cmpeq_epi16(vs.single, m128i{});
  const m128i isNeg = _mm_srai_epi16(vs.single, 15);
  m128i temp = _mm_andnot_si128(isZero, vte.single);
  temp = _mm_xor_si128(temp, isNeg);
  acc.l.single = _mm_sub_epi16(temp, isNeg);
  vd.single = _mm_subs_epi16(temp, isNeg);
}
#else
void RSP::vabs(const u32 instr) {
  VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    if (vs.selement[i] < 0) {
      if (vte.element[i] == 0x8000) {
        vd.element[i] = 0x7FFF;
        acc.l.element[i] = 0x8000;
      } else {
        vd.element[i] = -vte.selement[i];
        acc.l.element[i] = -vte.selement[i];
      }
    } else if (vs.element[i] == 0) {
      vd.element[i] = 0;
      acc.l.element[i] = 0;
    } else {
      vd.element[i] = vte.element[i];
      acc.l.element[i] = vte.element[i];
    }
  }
}
#endif

void RSP::vadd(const u32 instr) {
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    const s32 result = vs.selement[i] + vte.selement[i] + (vco.l.selement[i] != 0);
    acc.l.element[i] = result;
    vd.element[i] = (u16)signedClamp(result);
    vco.l.element[i] = 0;
    vco.h.element[i] = 0;
  }
}

void RSP::vaddc(const u32 instr) {
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    const u32 result = vs.element[i] + vte.element[i];
    acc.l.element[i] = result;
    vd.element[i] = result;
    vco.l.element[i] = ((result >> 16) & 1) ? 0xffff : 0;
    vco.h.element[i] = 0;
  }
}

void RSP::vch(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    s16 vsElem = vs.selement[i];
    s16 vteElem = vte.selement[i];

    vco.l.element[i] = ((vsElem ^ vteElem) < 0) ? 0xffff : 0;
    if (vco.l.element[i]) {
      const s16 result = vsElem + vteElem;

      acc.l.selement[i] = (result <= 0) ? -vteElem : vsElem;
      vcc.l.element[i] = result <= 0 ? 0xffff : 0;
      vcc.h.element[i] = vteElem < 0 ? 0xffff : 0;
      vco.h.element[i] = (result != 0 && (vteElem != ~vsElem)) ? 0xffff : 0;
      vce.element[i] = result == -1 ? 0xffff : 0;
    } else {
      const s16 result = vsElem - vteElem;
      acc.l.element[i] = (result >= 0) ? vteElem : vsElem;
      vcc.l.element[i] = vteElem < 0 ? 0xffff : 0;
      vcc.h.element[i] = result >= 0 ? 0xffff : 0;
      vco.h.element[i] = result != 0 ? 0xffff : 0;
      vce.element[i] = 0;
    }

    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vcr(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    u16 vsE = vs.element[i];
    const u16 vteE = vte.element[i];

    const bool signDiff = (0x8000 & (vsE ^ vteE)) == 0x8000;

    u16 vtAbs = signDiff ? ~vteE : vteE;

    bool gte = s16(vteE) <= s16(signDiff ? 0xffff : vsE);
    bool lte = (((signDiff ? vsE : 0) + vteE) & 0x8000) == 0x8000;

    const bool check = signDiff ? lte : gte;
    const u16 result = check ? vtAbs : vsE;

    acc.l.element[i] = result;
    vd.element[i] = result;

    vcc.h.element[i] = gte ? 0xffff : 0;
    vcc.l.element[i] = lte ? 0xffff : 0;

    vco.l.element[i] = 0;
    vco.h.element[i] = 0;
    vce.element[i] = 0;
  }
}

void RSP::vcl(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    u16 vs_element = vs.element[i];
    u16 vte_element = vte.element[i];

    if (vco.l.element[i]) {
      if (!vco.h.element[i]) {
        const u16 clamped_sum = vs_element + vte_element;
        const bool overflow = (vs_element + vte_element) != clamped_sum;
        if (vce.element[i]) {
          vcc.l.element[i] = (!clamped_sum || !overflow) ? 0xffff : 0;
        } else {
          vcc.l.element[i] = (!clamped_sum && !overflow) ? 0xffff : 0;
        }
      }
      acc.l.element[i] = vcc.l.element[i] ? -vte_element : vs_element;
    } else {
      if (!vco.h.element[i]) {
        vcc.h.element[i] = ((s32)vs_element - (s32)vte_element >= 0) ? 0xffff : 0;
      }
      acc.l.element[i] = vcc.h.element[i] ? vte_element : vs_element;
    }

    vco.l.element[i] = 0;
    vco.h.element[i] = 0;
    vce.element[i] = 0;
    vd.element[i] = acc.l.element[i];
  }
}
void RSP::vmov(const u32 instr) {
  const u8 e = E2(instr), vs = VS(instr) & 7;
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  u8 se;
  switch (e) {
  case 0 ... 1:
    se = (e & 0b000) | (vs & 0b111);
    break;
  case 2 ... 3:
    se = (e & 0b001) | (vs & 0b110);
    break;
  case 4 ... 7:
    se = (e & 0b011) | (vs & 0b100);
    break;
  case 8 ... 15:
    se = (e & 0b111) | (vs & 0b000);
    break;
  default:
    Util::panic("VMOV: This should be unreachable!");
  }

  const u8 de = vs & 7;

  vd.element[ELEMENT_INDEX(de)] = vte.element[ELEMENT_INDEX(se)];
#ifdef SIMD_SUPPORT
  acc.l.single = vte.single;
#else
  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i];
  }
#endif
}

FORCE_INLINE bool IsSignExtension(const s16 hi, const s16 lo) {
  if (hi == 0) {
    return (lo & 0x8000) == 0;
  }
  if (hi == -1) {
    return (lo & 0x8000) == 0x8000;
  }
  return false;
}

void RSP::vmulf(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const s16 op1 = vte.element[i];
    const s16 op2 = vs.element[i];
    const s32 prod = op1 * op2;

    s64 accum = prod;
    accum = (accum * 2) + 0x8000;

    SetACC(i, accum);

    const s16 result = signedClamp(accum >> 16);
    vd.element[i] = result;
  }
}

void RSP::vmulq(const u32 instr) {
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));
  VPR &vd = vpr[VD(instr)];

  for (int i = 0; i < 8; i++) {
    const bool neg = vte.selement[i] < 0 && vs.selement[i] >= 0 || vs.selement[i] < 0 && vte.selement[i] >= 0;
    const s32 product = vs.selement[i] * vte.selement[i] + 31 * neg;

    acc.h.element[i] = product >> 16;
    acc.m.element[i] = product;
    acc.l.element[i] = 0;
    vd.element[i] = signedClamp(product >> 1) & ~15;
  }
}

void RSP::vmulu(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const s16 op1 = vte.element[i];
    const s16 op2 = vs.element[i];
    const s32 prod = op1 * op2;

    s64 accum = prod;
    accum = (accum * 2) + 0x8000;

    SetACC(i, accum);

    const u16 result = unsignedClamp(accum >> 16);
    vd.element[i] = result;
  }
}

void RSP::vmudl(const u32 instr) {
  const u8 e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);
  for (int i = 0; i < 8; i++) {
    const u64 op1 = vte.element[i];
    const u64 op2 = vs.element[i];
    const u64 prod = op1 * op2;
    const u64 accum = prod >> 16;

    SetACC(i, accum);

    u16 result;
    if (IsSignExtension(acc.h.selement[i], acc.m.selement[i])) {
      result = acc.l.element[i];
    } else if (acc.h.selement[i] < 0) {
      result = 0;
    } else {
      result = 0xffff;
    }

    vd.element[i] = result;
  }
}

void RSP::vmudh(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);
  for (int i = 0; i < 8; i++) {
    const s32 prod = vs.selement[i] * vte.selement[i];
    const s16 result = signedClamp(prod);
    const s64 accum = static_cast<s64>(prod) << 16;
    SetACC(i, accum);

    vd.element[i] = result;
  }
}

void RSP::vmudm(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);
  for (int i = 0; i < 8; i++) {
    const s32 prod = vs.selement[i] * vte.element[i];
    const s64 accum = prod;
    const s16 result = signedClamp(accum >> 16);
    SetACC(i, accum);

    vd.element[i] = result;
  }
}

void RSP::vmudn(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);
  for (int i = 0; i < 8; i++) {
    const s16 op1 = vte.element[i];
    const u16 op2 = vs.element[i];
    const s32 prod = op1 * op2;
    const s64 accum = prod;
    SetACC(i, accum);

    u16 result;
    if (IsSignExtension(acc.h.selement[i], acc.m.selement[i])) {
      result = acc.l.element[i];
    } else if (acc.h.selement[i] < 0) {
      result = 0;
    } else {
      result = 0xffff;
    }

    vd.element[i] = result;
  }
}

#ifdef SIMD_SUPPORT
void RSP::vmadh(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);
  m128i lo = _mm_mullo_epi16(vs.single, vte.single);
  m128i hi = _mm_mulhi_epi16(vs.single, vte.single);
  m128i omask = _mm_adds_epu16(acc.m.single, lo);
  acc.m.single = _mm_add_epi16(acc.m.single, lo);
  omask = _mm_cmpeq_epi16(acc.m.single, omask);
  omask = _mm_cmpeq_epi16(omask, m128i{});
  hi = _mm_sub_epi16(hi, omask);
  acc.h.single = _mm_add_epi16(acc.h.single, hi);
  lo = _mm_unpacklo_epi16(acc.m.single, acc.h.single);
  hi = _mm_unpackhi_epi16(acc.m.single, acc.h.single);
  vd.single = _mm_packs_epi32(lo, hi);
}
#else
void RSP::vmadh(const u32 instr) {
  int e = E2(instr);
  VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);
  for (int i = 0; i < 8; i++) {
    s16 op1 = vte.element[i];
    s16 op2 = vs.element[i];
    s32 prod = op1 * op2;
    u32 unsProd = prod;

    u64 accumDelta = (u64)unsProd << 16;
    s64 accum = GetACC(i) + accumDelta;
    SetACC(i, accum);
    accum = GetACC(i);

    s16 result = signedClamp(accum >> 16);

    vd.element[i] = result;
  }
}
#endif

void RSP::vmadl(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const u64 op1 = vte.element[i];
    const u64 op2 = vs.element[i];
    const u64 prod = op1 * op2;
    const u64 accDelta = prod >> 16;
    const u64 accum = GetACC(i) + accDelta;

    SetACC(i, accum);

    u16 result;
    if (IsSignExtension(acc.h.selement[i], acc.m.selement[i])) {
      result = acc.l.element[i];
    } else if (acc.h.selement[i] < 0) {
      result = 0;
    } else {
      result = 0xffff;
    }

    vd.element[i] = result;
  }
}
#ifdef SIMD_SUPPORT
void RSP::vmadm(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], e);

  m128i lo = _mm_mullo_epi16(vs.single, vte.single);
  m128i hi = _mm_mulhi_epu16(vs.single, vte.single);
  const m128i sign = _mm_srai_epi16(vs.single, 15);
  const m128i vta = _mm_and_si128(vte.single, sign);
  hi = _mm_sub_epi16(hi, vta);
  m128i omask = _mm_adds_epu16(acc.l.single, lo);
  acc.l.single = _mm_add_epi16(acc.l.single, lo);
  omask = _mm_cmpeq_epi16(acc.l.single, omask);
  omask = _mm_cmpeq_epi16(omask, m128i{});
  hi = _mm_sub_epi16(hi, omask);
  omask = _mm_adds_epu16(acc.m.single, hi);
  acc.m.single = _mm_add_epi16(acc.m.single, hi);
  omask = _mm_cmpeq_epi16(acc.m.single, omask);
  omask = _mm_cmpeq_epi16(omask, m128i{});
  hi = _mm_srai_epi16(hi, 15);
  acc.h.single = _mm_add_epi16(acc.h.single, hi);
  acc.h.single = _mm_sub_epi16(acc.h.single, omask);
  lo = _mm_unpacklo_epi16(acc.m.single, acc.h.single);
  hi = _mm_unpackhi_epi16(acc.m.single, acc.h.single);
  VPR &vd = vpr[VD(instr)];
  vd.single = _mm_packs_epi32(lo, hi);
}

void RSP::vmadn(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], e);

  const m128i lo = _mm_mullo_epi16(vs.single, vte.single);
  m128i hi = _mm_mulhi_epu16(vs.single, vte.single);
  const m128i sign = _mm_srai_epi16(vte.single, 15);
  const m128i vsa = _mm_and_si128(vs.single, sign);
  hi = _mm_sub_epi16(hi, vsa);
  m128i omask = _mm_adds_epu16(acc.l.single, lo);
  acc.l.single = _mm_add_epi16(acc.l.single, lo);
  omask = _mm_cmpeq_epi16(acc.l.single, omask);
  omask = _mm_cmpeq_epi16(omask, m128i{});
  hi = _mm_sub_epi16(hi, omask);
  omask = _mm_adds_epu16(acc.m.single, hi);
  acc.m.single = _mm_add_epi16(acc.m.single, hi);
  omask = _mm_cmpeq_epi16(acc.m.single, omask);
  omask = _mm_cmpeq_epi16(omask, m128i{});
  hi = _mm_srai_epi16(hi, 15);
  acc.h.single = _mm_add_epi16(acc.h.single, hi);
  acc.h.single = _mm_sub_epi16(acc.h.single, omask);
  const m128i nhi = _mm_srai_epi16(acc.h.single, 15);
  const m128i nmd = _mm_srai_epi16(acc.m.single, 15);
  const m128i shi = _mm_cmpeq_epi16(nhi, acc.h.single);
  const m128i smd = _mm_cmpeq_epi16(nhi, nmd);
  const m128i cmask = _mm_and_si128(smd, shi);
  const m128i cval = _mm_cmpeq_epi16(nhi, m128i{});
  VPR &vd = vpr[VD(instr)];
  vd.single = _mm_blendv_epi8(cval, acc.l.single, cmask);
}
#else
void RSP::vmadm(const u32 instr) {
  int e = E2(instr);
  VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    s32 prod = vs.selement[i] * vte.element[i];
    s64 accum = GetACC(i);
    accum += prod;
    SetACC(i, accum);
    accum = GetACC(i);

    s16 result = signedClamp(accum >> 16);

    vd.element[i] = result;
  }
}

void RSP::vmadn(const u32 instr) {
  int e = E2(instr);
  VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    s32 prod = vs.element[i] * vte.selement[i];
    s64 accum = GetACC(i) + prod;

    SetACC(i, accum);

    u16 result;
    if (IsSignExtension(acc.h.selement[i], acc.m.selement[i])) {
      result = acc.l.element[i];
    } else if (acc.h.selement[i] < 0) {
      result = 0;
    } else {
      result = 0xffff;
    }

    vd.element[i] = result;
  }
}
#endif

void RSP::vmacf(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    const s16 op1 = vte.element[i];
    const s16 op2 = vs.element[i];
    const s32 prod = op1 * op2;

    const s64 accDelta = static_cast<s64>(prod) * 2;
    s64 accum = GetACC(i) + accDelta;
    SetACC(i, accum);
    accum = GetACC(i);

    const s16 result = signedClamp(accum >> 16);
    vd.element[i] = result;
  }
}

void RSP::vmacu(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    const s16 op1 = vte.element[i];
    const s16 op2 = vs.element[i];
    const s32 prod = op1 * op2;
    const s64 accDelta = static_cast<s64>(prod) * 2;
    s64 accum = GetACC(i) + accDelta;
    SetACC(i, accum);
    accum = GetACC(i);

    const u16 result = unsignedClamp(accum >> 16);
    vd.element[i] = result;
  }
}

void RSP::vmacq(const u32 instr) {
  VPR &vd = vpr[VD(instr)];

  for (int i = 0; i < 8; i++) {
    s32 product = acc.h.element[i] << 16 | acc.m.element[i];
    if (product < 0 && !(product & 1 << 5)) {
      product += 32;
    } else if (product >= 32 && !(product & 1 << 5)) {
      product -= 32;
    }
    acc.h.element[i] = product >> 16;
    acc.m.element[i] = product & 0xFFFF;

    vd.element[i] = signedClamp(product >> 1) & ~15;
  }
}

void RSP::veq(const u32 instr) {
  const int e = E2(instr);
  VPR &vd = vpr[VD(instr)];
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    vcc.l.element[i] = vco.h.element[i] == 0 && vs.element[i] == vte.element[i] ? 0xffff : 0;
    acc.l.element[i] = vcc.l.element[i] ? vs.element[i] : vte.element[i];
    vd.element[i] = acc.l.element[i];
    vcc.h.element[i] = vco.h.element[i] = vco.l.element[i] = 0;
  }
}

void RSP::vne(const u32 instr) {
  const int e = E2(instr);
  VPR &vd = vpr[VD(instr)];
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    vcc.l.element[i] = vco.h.element[i] || vs.element[i] != vte.element[i] ? 0xffff : 0;
    acc.l.element[i] = vcc.l.element[i] ? vs.element[i] : vte.element[i];
    vd.element[i] = acc.l.element[i];
    vcc.h.element[i] = vco.h.element[i] = vco.l.element[i] = 0;
  }
}

void RSP::vge(const u32 instr) {
  const int e = E2(instr);
  VPR &vd = vpr[VD(instr)];
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const bool eql = vs.selement[i] == vte.selement[i];
    const bool neg = !(vco.h.element[i] && vco.l.element[i]) && eql;
    vcc.l.element[i] = neg || vs.selement[i] > vte.selement[i] ? 0xffff : 0;
    acc.l.element[i] = vcc.l.element[i] ? vs.element[i] : vte.element[i];
    vd.element[i] = acc.l.element[i];
    vcc.h.element[i] = vco.h.element[i] = vco.l.element[i] = 0;
  }
}

void RSP::vlt(const u32 instr) {
  const int e = E2(instr);
  VPR &vd = vpr[VD(instr)];
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const bool eql = vs.element[i] == vte.element[i];
    const bool neg = vco.h.element[i] && vco.l.element[i] && eql;
    vcc.l.element[i] = neg || vs.selement[i] < vte.selement[i] ? 0xffff : 0;
    acc.l.element[i] = vcc.l.element[i] ? vs.element[i] : vte.element[i];
    vd.element[i] = acc.l.element[i];
    vcc.h.element[i] = vco.h.element[i] = vco.l.element[i] = 0;
  }
}

FORCE_INLINE u32 rcp(const s32 sinput) {
  const s32 mask = sinput >> 31;
  s32 input = sinput ^ mask;
  if (sinput > INT16_MIN) {
    input -= mask;
  }
  if (input == 0) {
    return 0x7FFFFFFF;
  }
  if (sinput == INT16_MIN) {
    return 0xFFFF0000;
  }

  const u32 shift = std::countl_zero(static_cast<u32>(input));
  const u64 dinput = static_cast<u64>(input);
  const u32 index = (dinput << shift & 0x7FC00000) >> 22;

  s32 result = rcpRom[index];
  result = (0x10000 | result) << 14;
  result = result >> (31 - shift) ^ mask;
  return result;
}

FORCE_INLINE u32 rsq(u32 input) {
  if (input == 0) {
    return 0x7FFFFFFF;
  }

  if (input == 0xFFFF8000) {
    return 0xFFFF0000;
  }

  if (input > 0xFFFF8000) {
    input--;
  }

  const s32 sinput = input;
  const s32 mask = sinput >> 31;
  input ^= mask;

  const int shift = std::countl_zero(input) + 1;

  const int index = (((input << shift) >> 24) | ((shift & 1) << 8));
  const u32 rom = (((u32)rsqRom[index]) << 14);
  const int r_shift = ((32 - shift) >> 1);
  const u32 result = (0x40000000 | rom) >> r_shift;

  return result ^ mask;
}

void RSP::vrcpl(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  const VPR &vt = vpr[VT(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));
  const int e = E2(instr) & 7;
  const int de = DE(instr) & 7;

  s32 input;
  if (divInLoaded) {
    input = (s32(divIn) << 16) | vt.element[ELEMENT_INDEX(e)];
  } else {
    input = vt.selement[ELEMENT_INDEX(e)];
  }

  const s32 result = rcp(input);
  divOut = result >> 16;
  divIn = 0;
  divInLoaded = false;

#ifdef SIMD_SUPPORT
  acc.l.single = vte.single;
#else
  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i];
  }
#endif

  vd.element[ELEMENT_INDEX(de)] = result;
}

void RSP::vrcp(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  const VPR &vt = vpr[VT(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));
  const int e = E2(instr) & 7;
  const int de = DE(instr) & 7;
  const s32 input = vt.selement[ELEMENT_INDEX(e)];
  const s32 result = rcp(input);
  vd.element[ELEMENT_INDEX(de)] = result;
  divOut = result >> 16;
  divInLoaded = false;

#ifdef SIMD_SUPPORT
  acc.l.single = vte.single;
#else
  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i];
  }
#endif
}

void RSP::vrsq(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  const VPR &vt = vpr[VT(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));
  const int e = E2(instr) & 7;
  const int de = VS(instr) & 7;
  const s32 input = vt.selement[ELEMENT_INDEX(e)];
  const u32 result = rsq(input);
  vd.element[ELEMENT_INDEX(de)] = result & 0xFFFF;
  divOut = result >> 16;
  divInLoaded = false;

#ifdef SIMD_SUPPORT
  acc.l.single = vte.single;
#else
  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i];
  }
#endif
}

// from nall, in ares
static FORCE_INLINE s64 sclip(const s64 x, const u32 bits) {
  const u64 b = 1ull << (bits - 1);
  const u64 m = b * 2 - 1;
  return ((x & m) ^ b) - b;
}

void RSP::vrndn(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    s32 product = vte.selement[i];

    if (VS(instr) & 1) {
      product <<= 16;
    }

    s64 accum = 0;
    accum |= acc.h.element[i];
    accum <<= 16;
    accum |= acc.m.element[i];
    accum <<= 16;
    accum |= acc.l.element[i];
    accum <<= 16;
    accum >>= 16;

    if (accum < 0) {
      accum = sclip(accum + product, 48);
    }

    acc.h.element[i] = accum >> 32;
    acc.m.element[i] = accum >> 16;
    acc.l.element[i] = accum >> 0;
    vd.element[i] = signedClamp(accum >> 16);
  }
}

void RSP::vrndp(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

  for (int i = 0; i < 8; i++) {
    s32 product = vte.selement[i];

    if (VS(instr) & 1) {
      product <<= 16;
    }

    s64 accum = 0;
    accum |= acc.h.element[i];
    accum <<= 16;
    accum |= acc.m.element[i];
    accum <<= 16;
    accum |= acc.l.element[i];
    accum <<= 16;
    accum >>= 16;

    if (accum >= 0) {
      accum = sclip(accum + product, 48);
    }

    acc.h.element[i] = accum >> 32;
    acc.m.element[i] = accum >> 16;
    acc.l.element[i] = accum >> 0;
    vd.element[i] = signedClamp(accum >> 16);
  }
}

void RSP::vrsql(const u32 instr) {
  VPR &vd = vpr[VD(instr)];
  const VPR &vt = vpr[VT(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));
  const int e = E2(instr) & 7;
  const int de = DE(instr) & 7;

  s32 input;
  if (divInLoaded) {
    input = (divIn << 16) | vt.element[ELEMENT_INDEX(e)];
  } else {
    input = vt.selement[ELEMENT_INDEX(e)];
  }

  const u32 result = rsq(input);
  divOut = result >> 16;
  divInLoaded = false;

#ifdef SIMD_SUPPORT
  acc.l.single = vte.single;
#else
  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i];
  }
#endif

  vd.element[ELEMENT_INDEX(de)] = result;
}

void RSP::vrcph(const u32 instr) {
  const int e = E2(instr) & 7;
  const int de = DE(instr) & 7;
  VPR &vd = vpr[VD(instr)];
  const VPR &vt = vpr[VT(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));

#ifdef SIMD_SUPPORT
  acc.l.single = vte.single;
#else
  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i];
  }
#endif

  vd.element[ELEMENT_INDEX(de)] = divOut;
  divIn = vt.element[ELEMENT_INDEX(e)];
  divInLoaded = true;
}

void RSP::vsar(const u32 instr) {
  const u8 e = E2(instr);
  VPR &vd = vpr[VD(instr)];
  switch (e) {
  case 0x8:
#ifdef SIMD_SUPPORT
    vd.single = acc.h.single;
#else
    for (int i = 0; i < 8; i++) {
      vd.element[i] = acc.h.element[i];
    }
#endif
    break;
  case 0x9:
#ifdef SIMD_SUPPORT
    vd.single = acc.m.single;
#else
    for (int i = 0; i < 8; i++) {
      vd.element[i] = acc.m.element[i];
    }
#endif
    break;
  case 0xA:
#ifdef SIMD_SUPPORT
    vd.single = acc.l.single;
#else
    for (int i = 0; i < 8; i++) {
      vd.element[i] = acc.l.element[i];
    }
#endif
    break;
  default:
#ifdef SIMD_SUPPORT
    vd.single = _mm_set_epi16(0, 0, 0, 0, 0, 0, 0, 0);
#else
    for (int i = 0; i < 8; i++) {
      vd.element[i] = 0;
    }
#endif
    break;
  }
}

void RSP::vsubc(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const u32 result = vs.element[i] - vte.element[i];
    acc.l.element[i] = result;
    vd.element[i] = result;

    vco.l.element[i] = (result >> 16) & 1 ? 0xffff : 0;
    vco.h.element[i] = result != 0 ? 0xffff : 0;
  }
}

void RSP::vsub(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    const s32 result = vs.selement[i] - vte.selement[i] - (vco.l.element[i] != 0);
    acc.l.element[i] = result;
    vd.element[i] = signedClamp(result);

    vco.l.element[i] = 0;
    vco.h.element[i] = 0;
  }
}

void RSP::vmrg(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vcc.l.element[i] ? vs.element[i] : vte.element[i];
    vd.element[i] = acc.l.element[i];

    vco.l.element[i] = vco.h.element[i] = 0;
  }
}

void RSP::vxor(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i] ^ vs.element[i];
    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vnxor(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = ~(vte.element[i] ^ vs.element[i]);
    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vand(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i] & vs.element[i];
    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vnand(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = ~(vte.element[i] & vs.element[i]);
    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vnor(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = ~(vte.element[i] | vs.element[i]);
    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vor(const u32 instr) {
  const int e = E2(instr);
  const VPR &vs = vpr[VS(instr)];
  VPR &vd = vpr[VD(instr)];
  SetVTE(vpr[VT(instr)], e);

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i] | vs.element[i];
    vd.element[i] = acc.l.element[i];
  }
}

void RSP::vzero(const u32 instr) {
  const VPR &vs = vpr[VS(instr)];
  SetVTE(vpr[VT(instr)], E2(instr));
  VPR &vd = vpr[VD(instr)];

  for (int i = 0; i < 8; i++) {
    acc.l.element[i] = vte.element[i] + vs.element[i];
  }

  memset(&vd, 0, sizeof(VPR));
}

void RSP::mfc0(const RDP &rdp, const u32 instr) { gpr[RT(instr)] = GetCop0Reg(*this, rdp, RD(instr)); }

void RSP::mtc0(const u32 instr) const { SetCop0Reg(mem, RD(instr), gpr[RT(instr)]); }

void RSP::mfc2(const u32 instr) {
  const u8 hi = vpr[RD(instr)].byte[BYTE_INDEX(E1(instr))];
  const u8 lo = vpr[RD(instr)].byte[BYTE_INDEX((E1(instr) + 1) & 0xF)];
  const s16 elem = hi << 8 | lo;
  gpr[RT(instr)] = elem;
}

void RSP::mtc2(const u32 instr) {
  const u16 element = gpr[RT(instr)];
  const u8 lo = element;
  const u8 hi = element >> 8;
  vpr[RD(instr)].byte[BYTE_INDEX(E1(instr))] = hi;
  if (E1(instr) < 15) {
    vpr[RD(instr)].byte[BYTE_INDEX(E1(instr) + 1)] = lo;
  }
}
} // namespace n64
