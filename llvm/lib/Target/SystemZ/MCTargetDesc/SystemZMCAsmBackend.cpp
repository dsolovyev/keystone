//===-- SystemZMCAsmBackend.cpp - SystemZ assembler backend ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SystemZMCTargetDesc.h"
#include "MCTargetDesc/SystemZMCFixups.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"

#include <keystone/keystone.h>

using namespace llvm_ks;

// Value is a fully-resolved relocation value: Symbol + Addend [- Pivot].
// Return the bits that should be installed in a relocation field for
// fixup kind Kind.
static uint64_t extractBitsForFixup(MCFixupKind Kind, uint64_t Value) {
  if (Kind < FirstTargetFixupKind)
    return Value;

  switch (unsigned(Kind)) {
  case SystemZ::FK_390_PC16DBL:
  case SystemZ::FK_390_PC32DBL:
    return (int64_t)Value / 2;

  case SystemZ::FK_390_TLS_CALL:
    return 0;
  }

  llvm_unreachable("Unknown fixup kind!");
}

namespace {
class SystemZMCAsmBackend : public MCAsmBackend {
  uint8_t OSABI;
public:
  SystemZMCAsmBackend(uint8_t osABI)
    : OSABI(osABI) {}

  // Override MCAsmBackend
  unsigned getNumFixupKinds() const override {
    return SystemZ::NumTargetFixupKinds;
  }
  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override;
  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsPCRel, unsigned int &KsError) const override;
  bool mayNeedRelaxation(const MCInst &Inst) const override {
    return false;
  }
  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *Fragment,
                            const MCAsmLayout &Layout, unsigned &KsError) const override {
    return false;
  }
  void relaxInstruction(const MCInst &Inst, MCInst &Res) const override {
    llvm_unreachable("SystemZ does do not have assembler relaxation");
  }
  bool writeNopData(uint64_t Count, MCObjectWriter *OW) const override;
  MCObjectWriter *createObjectWriter(raw_pwrite_stream &OS) const override {
    return createSystemZObjectWriter(OS, OSABI);
  }
};
} // end anonymous namespace

const MCFixupKindInfo &
SystemZMCAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[SystemZ::NumTargetFixupKinds] = {
    { "FK_390_PC16DBL",  0, 16, MCFixupKindInfo::FKF_IsPCRel },
    { "FK_390_PC32DBL",  0, 32, MCFixupKindInfo::FKF_IsPCRel },
    { "FK_390_TLS_CALL", 0, 0, 0 }
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

void SystemZMCAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                     const MCValue &Target, MutableArrayRef<char> Data,
                                     uint64_t Value, bool IsPCRel, unsigned int &KsError) const {
  MCFixupKind Kind = Fixup.getKind();
  unsigned Offset = Fixup.getOffset();
  unsigned Size = (getFixupKindInfo(Kind).TargetSize + 7) / 8;

  //assert(Offset + Size <= Data.size() && "Invalid fixup offset!");
  if (Offset + Size > Data.size()) {
      KsError = KS_ERR_ASM_FIXUP_INVALID;
      return;
  }

  // Big-endian insertion of Size bytes.
  Value = extractBitsForFixup(Kind, Value);
  unsigned ShiftValue = (Size * 8) - 8;
  for (unsigned I = 0; I != Size; ++I) {
    Data[Offset + I] |= uint8_t(Value >> ShiftValue);
    ShiftValue -= 8;
  }
}

bool SystemZMCAsmBackend::writeNopData(uint64_t Count,
                                       MCObjectWriter *OW) const {
  for (uint64_t I = 0; I != Count; ++I)
    OW->write8(7);
  return true;
}

MCAsmBackend *llvm_ks::createSystemZMCAsmBackend(const Target &T,
                                              const MCRegisterInfo &MRI,
                                              const Triple &TT, StringRef CPU) {
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  return new SystemZMCAsmBackend(OSABI);
}
