#include "jit/helpers.hpp"


#include <core/registers/Registers.hpp>
#include <core/JIT.hpp>

namespace n64 {
Registers::Registers(JIT *jit) : jit(jit), cop0(*this), cop1(*this) { Reset(); }

void Registers::Reset() {
  hi = 0;
  lo = 0;
  delaySlot = false;
  prevDelaySlot = false;
  gpr.fill(0);
  gprIsConstant.fill(false);
  gprIsConstant[0] = true;

  cop0.Reset();
  cop1.Reset();

  steps = 0;
  extraCycles = 0;
}

void Registers::SetPC64(s64 val) {
  oldPC = pc;
  pc = val;
  nextPC = pc + 4;
}

void Registers::SetPC32(s32 val) {
  oldPC = pc;
  pc = s64(val);
  nextPC = pc + 4;
}

template <>
u64 Registers::Read<u64>(size_t idx) {
  return idx == 0 ? 0 : gpr[idx];
}

template <>
s64 Registers::Read<s64>(const size_t idx) {
  return static_cast<s64>(Read<u64>(idx));
}

template <>
u32 Registers::Read<u32>(size_t idx) {
  return idx == 0 ? 0 : gpr[idx];
}

template <>
s32 Registers::Read<s32>(size_t idx) {
  return static_cast<s32>(Read<u32>(idx));
}

template <>
u16 Registers::Read<u16>(size_t idx) {
  return idx == 0 ? 0 : gpr[idx];
}

template <>
s16 Registers::Read<s16>(size_t idx) {
  return static_cast<s16>(Read<u16>(idx));
}

template <>
u8 Registers::Read<u8>(size_t idx) {
  return idx == 0 ? 0 : gpr[idx];
}

template <>
s8 Registers::Read<s8>(size_t idx) {
  return static_cast<s8>(Read<u8>(idx));
}

#ifndef __aarch64__
template <>
void Registers::Read<u64>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt64(), jit->GPR<u64>(idx));
}

template <>
void Registers::Read<s64>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt64(), jit->GPR<u64>(idx));
}

template <>
void Registers::Read<u32>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt32(), jit->GPR<u32>(idx));
}

template <>
void Registers::Read<s32>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt32(), jit->GPR<s32>(idx));
}

template <>
void Registers::Read<u16>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt16(), jit->GPR<u16>(idx));
}

template <>
void Registers::Read<s16>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt16(), jit->GPR<u16>(idx));
}

template <>
void Registers::Read<u8>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt8(), jit->GPR<u8>(idx));
}

template <>
void Registers::Read<s8>(size_t idx, Xbyak::Reg reg) {
  jit->code.mov(reg.cvt8(), jit->GPR<s8>(idx));
}
#endif

template <>
void Registers::Write<bool>(size_t idx, bool v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}

template <>
void Registers::Write<u64>(size_t idx, u64 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}

template <>
void Registers::Write<s64>(size_t idx, s64 v) {
  Write<u64>(idx, v);
}

template <>
void Registers::Write<u32>(size_t idx, u32 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}


template <>
void Registers::Write<s32>(size_t idx, s32 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}

template <>
void Registers::Write<u16>(size_t idx, u16 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}


template <>
void Registers::Write<s16>(size_t idx, s16 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}

template <>
void Registers::Write<u8>(size_t idx, u8 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}


template <>
void Registers::Write<s8>(size_t idx, s8 v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = true;
  gpr[idx] = v;
}

#ifndef __aarch64__
template <>
void Registers::Write<bool>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movsx(v.cvt64(), v.cvt8());
  jit->code.mov(jit->GPR<u64>(idx), v);
}

template <>
void Registers::Write<s8>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movsx(v.cvt64(), v.cvt8());
  jit->code.mov(jit->GPR<u64>(idx), v);
}

template <>
void Registers::Write<u8>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movzx(v.cvt64(), v.cvt8());
  jit->code.mov(jit->GPR<u64>(idx), v.cvt64());
}

template <>
void Registers::Write<s16>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movsx(v.cvt64(), v.cvt16());
  jit->code.mov(jit->GPR<u64>(idx), v.cvt64());
}

template <>
void Registers::Write<u16>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movzx(v.cvt64(), v.cvt16());
  jit->code.mov(jit->GPR<u64>(idx), v.cvt64());
}

template <>
void Registers::Write<s32>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movsxd(v.cvt64(), v.cvt32());
  jit->code.mov(jit->GPR<u64>(idx), v.cvt64());
}

template <>
void Registers::Write<u32>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.movzx(v.cvt64(), v.cvt32());
  jit->code.mov(jit->GPR<u64>(idx), v.cvt64());
}

template <>
void Registers::Write<u64>(size_t idx, Xbyak::Reg v) {
  if (idx == 0)
    return;

  gprIsConstant[idx] = false;

  if (!jit)
    Util::panic("Did you try to call Registers::Write(size_t, *Xbyak::Reg*) from the interpreter?");

  jit->code.mov(jit->GPR<u64>(idx), v.cvt64());
}

template <>
void Registers::Write<s64>(size_t idx, Xbyak::Reg v) {
  Write<u64>(idx, v);
}
#endif
} // namespace n64
