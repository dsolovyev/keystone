//===-- Host.cpp - Implement OS Host Concept --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the operating system Host concept.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/Host.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Config/config.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <string.h>

// Include the platform-specific parts of this class.
#ifdef LLVM_ON_UNIX
#include "Unix/Host.inc"
#endif
#ifdef LLVM_ON_WIN32
#include "Windows/Host.inc"
#endif
#ifdef _MSC_VER
#include <intrin.h>
#endif
#if defined(__APPLE__) && (defined(__ppc__) || defined(__powerpc__))
#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/machine.h>
#endif

#define DEBUG_TYPE "host-detection"

//===----------------------------------------------------------------------===//
//
//  Implementations of the CPU detection routines
//
//===----------------------------------------------------------------------===//

using namespace llvm_ks;

#if defined(__linux__)
static ssize_t LLVM_ATTRIBUTE_UNUSED readCpuInfo(void *Buf, size_t Size) {
  // Note: We cannot mmap /proc/cpuinfo here and then process the resulting
  // memory buffer because the 'file' has 0 size (it can be read from only
  // as a stream).

  int FD;
  std::error_code EC = sys::fs::openFileForRead("/proc/cpuinfo", FD);
  if (EC) {
    DEBUG(dbgs() << "Unable to open /proc/cpuinfo: " << EC.message() << "\n");
    return -1;
  }
  int Ret = read(FD, Buf, Size);
  int CloseStatus = close(FD);
  if (CloseStatus)
    return -1;
  return Ret;
}
#endif

#if defined(i386) || defined(__i386__) || defined(__x86__) ||                  \
    defined(_M_IX86) || defined(__x86_64__) || defined(_M_AMD64) ||            \
    defined(_M_X64)

enum VendorSignatures {
  SIG_INTEL = 0x756e6547 /* Genu */,
  SIG_AMD = 0x68747541 /* Auth */
};

enum ProcessorVendors {
  VENDOR_INTEL = 1,
  VENDOR_AMD,
  VENDOR_OTHER,
  VENDOR_MAX
};

enum ProcessorTypes {
  INTEL_ATOM = 1,
  INTEL_CORE2,
  INTEL_COREI7,
  AMDFAM10H,
  AMDFAM15H,
  INTEL_i386,
  INTEL_i486,
  INTEL_PENTIUM,
  INTEL_PENTIUM_PRO,
  INTEL_PENTIUM_II,
  INTEL_PENTIUM_III,
  INTEL_PENTIUM_IV,
  INTEL_PENTIUM_M,
  INTEL_CORE_DUO,
  INTEL_XEONPHI,
  INTEL_X86_64,
  INTEL_NOCONA,
  INTEL_PRESCOTT,
  AMD_i486,
  AMDPENTIUM,
  AMDATHLON,
  AMDFAM14H,
  AMDFAM16H,
  CPU_TYPE_MAX
};

enum ProcessorSubtypes {
  INTEL_COREI7_NEHALEM = 1,
  INTEL_COREI7_WESTMERE,
  INTEL_COREI7_SANDYBRIDGE,
  AMDFAM10H_BARCELONA,
  AMDFAM10H_SHANGHAI,
  AMDFAM10H_ISTANBUL,
  AMDFAM15H_BDVER1,
  AMDFAM15H_BDVER2,
  INTEL_PENTIUM_MMX,
  INTEL_CORE2_65,
  INTEL_CORE2_45,
  INTEL_COREI7_IVYBRIDGE,
  INTEL_COREI7_HASWELL,
  INTEL_COREI7_BROADWELL,
  INTEL_COREI7_SKYLAKE,
  INTEL_COREI7_SKYLAKE_AVX512,
  INTEL_ATOM_BONNELL,
  INTEL_ATOM_SILVERMONT,
  INTEL_KNIGHTS_LANDING,
  AMDPENTIUM_K6,
  AMDPENTIUM_K62,
  AMDPENTIUM_K63,
  AMDPENTIUM_GEODE,
  AMDATHLON_TBIRD,
  AMDATHLON_MP,
  AMDATHLON_XP,
  AMDATHLON_K8SSE3,
  AMDATHLON_OPTERON,
  AMDATHLON_FX,
  AMDATHLON_64,
  AMD_BTVER1,
  AMD_BTVER2,
  AMDFAM15H_BDVER3,
  AMDFAM15H_BDVER4,
  CPU_SUBTYPE_MAX
};

enum ProcessorFeatures {
  FEATURE_CMOV = 0,
  FEATURE_MMX,
  FEATURE_POPCNT,
  FEATURE_SSE,
  FEATURE_SSE2,
  FEATURE_SSE3,
  FEATURE_SSSE3,
  FEATURE_SSE4_1,
  FEATURE_SSE4_2,
  FEATURE_AVX,
  FEATURE_AVX2,
  FEATURE_AVX512,
  FEATURE_AVX512SAVE,
  FEATURE_MOVBE,
  FEATURE_ADX,
  FEATURE_EM64T
};

/// getX86CpuIDAndInfo - Execute the specified cpuid and return the 4 values in
/// the specified arguments.  If we can't run cpuid on the host, return true.
static bool getX86CpuIDAndInfo(unsigned value, unsigned *rEAX, unsigned *rEBX,
                               unsigned *rECX, unsigned *rEDX) {
#if defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
  // gcc doesn't know cpuid would clobber ebx/rbx. Preseve it manually.
  asm("movq\t%%rbx, %%rsi\n\t"
      "cpuid\n\t"
      "xchgq\t%%rbx, %%rsi\n\t"
      : "=a"(*rEAX), "=S"(*rEBX), "=c"(*rECX), "=d"(*rEDX)
      : "a"(value));
  return false;
#elif defined(i386) || defined(__i386__) || defined(__x86__) || defined(_M_IX86)
  asm("movl\t%%ebx, %%esi\n\t"
      "cpuid\n\t"
      "xchgl\t%%ebx, %%esi\n\t"
      : "=a"(*rEAX), "=S"(*rEBX), "=c"(*rECX), "=d"(*rEDX)
      : "a"(value));
  return false;
// pedantic #else returns to appease -Wunreachable-code (so we don't generate
// postprocessed code that looks like "return true; return false;")
#else
  return true;
#endif
#elif defined(_MSC_VER)
  // The MSVC intrinsic is portable across x86 and x64.
  int registers[4];
  __cpuid(registers, value);
  *rEAX = registers[0];
  *rEBX = registers[1];
  *rECX = registers[2];
  *rEDX = registers[3];
  return false;
#else
  return true;
#endif
}

/// getX86CpuIDAndInfoEx - Execute the specified cpuid with subleaf and return
/// the 4 values in the specified arguments.  If we can't run cpuid on the host,
/// return true.
static bool getX86CpuIDAndInfoEx(unsigned value, unsigned subleaf,
                                 unsigned *rEAX, unsigned *rEBX, unsigned *rECX,
                                 unsigned *rEDX) {
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#if defined(__GNUC__)
  // gcc doesn't know cpuid would clobber ebx/rbx. Preseve it manually.
  asm("movq\t%%rbx, %%rsi\n\t"
      "cpuid\n\t"
      "xchgq\t%%rbx, %%rsi\n\t"
      : "=a"(*rEAX), "=S"(*rEBX), "=c"(*rECX), "=d"(*rEDX)
      : "a"(value), "c"(subleaf));
  return false;
#elif defined(_MSC_VER)
  int registers[4];
  __cpuidex(registers, value, subleaf);
  *rEAX = registers[0];
  *rEBX = registers[1];
  *rECX = registers[2];
  *rEDX = registers[3];
  return false;
#else
  return true;
#endif
#elif defined(i386) || defined(__i386__) || defined(__x86__) || defined(_M_IX86)
#if defined(__GNUC__)
  asm("movl\t%%ebx, %%esi\n\t"
      "cpuid\n\t"
      "xchgl\t%%ebx, %%esi\n\t"
      : "=a"(*rEAX), "=S"(*rEBX), "=c"(*rECX), "=d"(*rEDX)
      : "a"(value), "c"(subleaf));
  return false;
#elif defined(_MSC_VER)
  __asm {
      mov   eax,value
      mov   ecx,subleaf
      cpuid
      mov   esi,rEAX
      mov   dword ptr [esi],eax
      mov   esi,rEBX
      mov   dword ptr [esi],ebx
      mov   esi,rECX
      mov   dword ptr [esi],ecx
      mov   esi,rEDX
      mov   dword ptr [esi],edx
  }
  return false;
#else
  return true;
#endif
#else
  return true;
#endif
}

static bool getX86XCR0(unsigned *rEAX, unsigned *rEDX) {
#if defined(__GNUC__)
  // Check xgetbv; this uses a .byte sequence instead of the instruction
  // directly because older assemblers do not include support for xgetbv and
  // there is no easy way to conditionally compile based on the assembler used.
  __asm__(".byte 0x0f, 0x01, 0xd0" : "=a"(*rEAX), "=d"(*rEDX) : "c"(0));
  return false;
#elif defined(_MSC_FULL_VER) && defined(_XCR_XFEATURE_ENABLED_MASK)
  unsigned long long Result = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
  *rEAX = Result;
  *rEDX = Result >> 32;
  return false;
#else
  return true;
#endif
}

static void detectX86FamilyModel(unsigned EAX, unsigned *Family,
                                 unsigned *Model) {
  *Family = (EAX >> 8) & 0xf; // Bits 8 - 11
  *Model = (EAX >> 4) & 0xf;  // Bits 4 - 7
  if (*Family == 6 || *Family == 0xf) {
    if (*Family == 0xf)
      // Examine extended family ID if family ID is F.
      *Family += (EAX >> 20) & 0xff; // Bits 20 - 27
    // Examine extended model ID if family ID is 6 or F.
    *Model += ((EAX >> 16) & 0xf) << 4; // Bits 16 - 19
  }
}

static void
getIntelProcessorTypeAndSubtype(unsigned int Family, unsigned int Model,
                                unsigned int Brand_id, unsigned int Features,
                                unsigned *Type, unsigned *Subtype) {
  if (Brand_id != 0)
    return;
  switch (Family) {
  case 3:
    *Type = INTEL_i386;
    break;
  case 4:
    switch (Model) {
    case 0: // Intel486 DX processors
    case 1: // Intel486 DX processors
    case 2: // Intel486 SX processors
    case 3: // Intel487 processors, IntelDX2 OverDrive processors,
            // IntelDX2 processors
    case 4: // Intel486 SL processor
    case 5: // IntelSX2 processors
    case 7: // Write-Back Enhanced IntelDX2 processors
    case 8: // IntelDX4 OverDrive processors, IntelDX4 processors
    default:
      *Type = INTEL_i486;
      break;
    }
    break;
  case 5:
    switch (Model) {
    case 1: // Pentium OverDrive processor for Pentium processor (60, 66),
            // Pentium processors (60, 66)
    case 2: // Pentium OverDrive processor for Pentium processor (75, 90,
            // 100, 120, 133), Pentium processors (75, 90, 100, 120, 133,
            // 150, 166, 200)
    case 3: // Pentium OverDrive processors for Intel486 processor-based
            // systems
      *Type = INTEL_PENTIUM;
      break;
    case 4: // Pentium OverDrive processor with MMX technology for Pentium
            // processor (75, 90, 100, 120, 133), Pentium processor with
            // MMX technology (166, 200)
      *Type = INTEL_PENTIUM;
      *Subtype = INTEL_PENTIUM_MMX;
      break;
    default:
      *Type = INTEL_PENTIUM;
      break;
    }
    break;
  case 6:
    switch (Model) {
    case 0x01: // Pentium Pro processor
      *Type = INTEL_PENTIUM_PRO;
      break;
    case 0x03: // Intel Pentium II OverDrive processor, Pentium II processor,
               // model 03
    case 0x05: // Pentium II processor, model 05, Pentium II Xeon processor,
               // model 05, and Intel Celeron processor, model 05
    case 0x06: // Celeron processor, model 06
      *Type = INTEL_PENTIUM_II;
      break;
    case 0x07: // Pentium III processor, model 07, and Pentium III Xeon
               // processor, model 07
    case 0x08: // Pentium III processor, model 08, Pentium III Xeon processor,
               // model 08, and Celeron processor, model 08
    case 0x0a: // Pentium III Xeon processor, model 0Ah
    case 0x0b: // Pentium III processor, model 0Bh
      *Type = INTEL_PENTIUM_III;
      break;
    case 0x09: // Intel Pentium M processor, Intel Celeron M processor model 09.
    case 0x0d: // Intel Pentium M processor, Intel Celeron M processor, model
               // 0Dh. All processors are manufactured using the 90 nm process.
    case 0x15: // Intel EP80579 Integrated Processor and Intel EP80579
               // Integrated Processor with Intel QuickAssist Technology
      *Type = INTEL_PENTIUM_M;
      break;
    case 0x0e: // Intel Core Duo processor, Intel Core Solo processor, model
               // 0Eh. All processors are manufactured using the 65 nm process.
      *Type = INTEL_CORE_DUO;
      break;   // yonah
    case 0x0f: // Intel Core 2 Duo processor, Intel Core 2 Duo mobile
               // processor, Intel Core 2 Quad processor, Intel Core 2 Quad
               // mobile processor, Intel Core 2 Extreme processor, Intel
               // Pentium Dual-Core processor, Intel Xeon processor, model
               // 0Fh. All processors are manufactured using the 65 nm process.
    case 0x16: // Intel Celeron processor model 16h. All processors are
               // manufactured using the 65 nm process
      *Type = INTEL_CORE2; // "core2"
      *Subtype = INTEL_CORE2_65;
      break;
    case 0x17: // Intel Core 2 Extreme processor, Intel Xeon processor, model
               // 17h. All processors are manufactured using the 45 nm process.
               //
               // 45nm: Penryn , Wolfdale, Yorkfield (XE)
    case 0x1d: // Intel Xeon processor MP. All processors are manufactured using
               // the 45 nm process.
      *Type = INTEL_CORE2; // "penryn"
      *Subtype = INTEL_CORE2_45;
      break;
    case 0x1a: // Intel Core i7 processor and Intel Xeon processor. All
               // processors are manufactured using the 45 nm process.
    case 0x1e: // Intel(R) Core(TM) i7 CPU         870  @ 2.93GHz.
               // As found in a Summer 2010 model iMac.
    case 0x1f:
    case 0x2e:             // Nehalem EX
      *Type = INTEL_COREI7; // "nehalem"
      *Subtype = INTEL_COREI7_NEHALEM;
      break;
    case 0x25: // Intel Core i7, laptop version.
    case 0x2c: // Intel Core i7 processor and Intel Xeon processor. All
               // processors are manufactured using the 32 nm process.
    case 0x2f: // Westmere EX
      *Type = INTEL_COREI7; // "westmere"
      *Subtype = INTEL_COREI7_WESTMERE;
      break;
    case 0x2a: // Intel Core i7 processor. All processors are manufactured
               // using the 32 nm process.
    case 0x2d:
      *Type = INTEL_COREI7; //"sandybridge"
      *Subtype = INTEL_COREI7_SANDYBRIDGE;
      break;
    case 0x3a:
    case 0x3e:             // Ivy Bridge EP
      *Type = INTEL_COREI7; // "ivybridge"
      *Subtype = INTEL_COREI7_IVYBRIDGE;
      break;

    // Haswell:
    case 0x3c:
    case 0x3f:
    case 0x45:
    case 0x46:
      *Type = INTEL_COREI7; // "haswell"
      *Subtype = INTEL_COREI7_HASWELL;
      break;

    // Broadwell:
    case 0x3d:
    case 0x47:
    case 0x4f:
    case 0x56:
      *Type = INTEL_COREI7; // "broadwell"
      *Subtype = INTEL_COREI7_BROADWELL;
      break;

    // Skylake:
    case 0x4e:
      *Type = INTEL_COREI7; // "skylake-avx512"
      *Subtype = INTEL_COREI7_SKYLAKE_AVX512;
      break;
    case 0x5e:
      *Type = INTEL_COREI7; // "skylake"
      *Subtype = INTEL_COREI7_SKYLAKE;
      break;

    case 0x1c: // Most 45 nm Intel Atom processors
    case 0x26: // 45 nm Atom Lincroft
    case 0x27: // 32 nm Atom Medfield
    case 0x35: // 32 nm Atom Midview
    case 0x36: // 32 nm Atom Midview
      *Type = INTEL_ATOM;
      *Subtype = INTEL_ATOM_BONNELL;
      break; // "bonnell"

    // Atom Silvermont codes from the Intel software optimization guide.
    case 0x37:
    case 0x4a:
    case 0x4d:
    case 0x5a:
    case 0x5d:
    case 0x4c: // really airmont
      *Type = INTEL_ATOM;
      *Subtype = INTEL_ATOM_SILVERMONT;
      break; // "silvermont"

    case 0x57:
      *Type = INTEL_XEONPHI; // knl
      *Subtype = INTEL_KNIGHTS_LANDING;
      break;

    default: // Unknown family 6 CPU, try to guess.
      if (Features & (1 << FEATURE_AVX512)) {
        *Type = INTEL_XEONPHI; // knl
        *Subtype = INTEL_KNIGHTS_LANDING;
        break;
      }
      if (Features & (1 << FEATURE_ADX)) {
        *Type = INTEL_COREI7;
        *Subtype = INTEL_COREI7_BROADWELL;
        break;
      }
      if (Features & (1 << FEATURE_AVX2)) {
        *Type = INTEL_COREI7;
        *Subtype = INTEL_COREI7_HASWELL;
        break;
      }
      if (Features & (1 << FEATURE_AVX)) {
        *Type = INTEL_COREI7;
        *Subtype = INTEL_COREI7_SANDYBRIDGE;
        break;
      }
      if (Features & (1 << FEATURE_SSE4_2)) {
        if (Features & (1 << FEATURE_MOVBE)) {
          *Type = INTEL_ATOM;
          *Subtype = INTEL_ATOM_SILVERMONT;
        } else {
          *Type = INTEL_COREI7;
          *Subtype = INTEL_COREI7_NEHALEM;
        }
        break;
      }
      if (Features & (1 << FEATURE_SSE4_1)) {
        *Type = INTEL_CORE2; // "penryn"
        *Subtype = INTEL_CORE2_45;
        break;
      }
      if (Features & (1 << FEATURE_SSSE3)) {
        if (Features & (1 << FEATURE_MOVBE)) {
          *Type = INTEL_ATOM;
          *Subtype = INTEL_ATOM_BONNELL; // "bonnell"
        } else {
          *Type = INTEL_CORE2; // "core2"
          *Subtype = INTEL_CORE2_65;
        }
        break;
      }
      if (Features & (1 << FEATURE_EM64T)) {
        *Type = INTEL_X86_64;
        break; // x86-64
      }
      if (Features & (1 << FEATURE_SSE2)) {
        *Type = INTEL_PENTIUM_M;
        break;
      }
      if (Features & (1 << FEATURE_SSE)) {
        *Type = INTEL_PENTIUM_III;
        break;
      }
      if (Features & (1 << FEATURE_MMX)) {
        *Type = INTEL_PENTIUM_II;
        break;
      }
      *Type = INTEL_PENTIUM_PRO;
      break;
    }
    break;
  case 15: {
    switch (Model) {
    case 0: // Pentium 4 processor, Intel Xeon processor. All processors are
            // model 00h and manufactured using the 0.18 micron process.
    case 1: // Pentium 4 processor, Intel Xeon processor, Intel Xeon
            // processor MP, and Intel Celeron processor. All processors are
            // model 01h and manufactured using the 0.18 micron process.
    case 2: // Pentium 4 processor, Mobile Intel Pentium 4 processor - M,
            // Intel Xeon processor, Intel Xeon processor MP, Intel Celeron
            // processor, and Mobile Intel Celeron processor. All processors
            // are model 02h and manufactured using the 0.13 micron process.
      *Type =
          ((Features & (1 << FEATURE_EM64T)) ? INTEL_X86_64 : INTEL_PENTIUM_IV);
      break;

    case 3: // Pentium 4 processor, Intel Xeon processor, Intel Celeron D
            // processor. All processors are model 03h and manufactured using
            // the 90 nm process.
    case 4: // Pentium 4 processor, Pentium 4 processor Extreme Edition,
            // Pentium D processor, Intel Xeon processor, Intel Xeon
            // processor MP, Intel Celeron D processor. All processors are
            // model 04h and manufactured using the 90 nm process.
    case 6: // Pentium 4 processor, Pentium D processor, Pentium processor
            // Extreme Edition, Intel Xeon processor, Intel Xeon processor
            // MP, Intel Celeron D processor. All processors are model 06h
            // and manufactured using the 65 nm process.
      *Type =
          ((Features & (1 << FEATURE_EM64T)) ? INTEL_NOCONA : INTEL_PRESCOTT);
      break;

    default:
      *Type =
          ((Features & (1 << FEATURE_EM64T)) ? INTEL_X86_64 : INTEL_PENTIUM_IV);
      break;
    }
    break;
  }
  default:
    break; /*"generic"*/
  }
}

static void getAMDProcessorTypeAndSubtype(unsigned int Family,
                                          unsigned int Model,
                                          unsigned int Features,
                                          unsigned *Type,
                                          unsigned *Subtype) {
  // FIXME: this poorly matches the generated SubtargetFeatureKV table.  There
  // appears to be no way to generate the wide variety of AMD-specific targets
  // from the information returned from CPUID.
  switch (Family) {
  case 4:
    *Type = AMD_i486;
    break;
  case 5:
    *Type = AMDPENTIUM;
    switch (Model) {
    case 6:
    case 7:
      *Subtype = AMDPENTIUM_K6;
      break; // "k6"
    case 8:
      *Subtype = AMDPENTIUM_K62;
      break; // "k6-2"
    case 9:
    case 13:
      *Subtype = AMDPENTIUM_K63;
      break; // "k6-3"
    case 10:
      *Subtype = AMDPENTIUM_GEODE;
      break; // "geode"
    }
    break;
  case 6:
    *Type = AMDATHLON;
    switch (Model) {
    case 4:
      *Subtype = AMDATHLON_TBIRD;
      break; // "athlon-tbird"
    case 6:
    case 7:
    case 8:
      *Subtype = AMDATHLON_MP;
      break; // "athlon-mp"
    case 10:
      *Subtype = AMDATHLON_XP;
      break; // "athlon-xp"
    }
    break;
  case 15:
    *Type = AMDATHLON;
    if (Features & (1 << FEATURE_SSE3)) {
      *Subtype = AMDATHLON_K8SSE3;
      break; // "k8-sse3"
    }
    switch (Model) {
    case 1:
      *Subtype = AMDATHLON_OPTERON;
      break; // "opteron"
    case 5:
      *Subtype = AMDATHLON_FX;
      break; // "athlon-fx"; also opteron
    default:
      *Subtype = AMDATHLON_64;
      break; // "athlon64"
    }
    break;
  case 16:
    *Type = AMDFAM10H; // "amdfam10"
    switch (Model) {
    case 2:
      *Subtype = AMDFAM10H_BARCELONA;
      break;
    case 4:
      *Subtype = AMDFAM10H_SHANGHAI;
      break;
    case 8:
      *Subtype = AMDFAM10H_ISTANBUL;
      break;
    }
    break;
  case 20:
    *Type = AMDFAM14H;
    *Subtype = AMD_BTVER1;
    break; // "btver1";
  case 21:
    *Type = AMDFAM15H;
    if (!(Features &
          (1 << FEATURE_AVX))) { // If no AVX support, provide a sane fallback.
      *Subtype = AMD_BTVER1;
      break; // "btver1"
    }
    if (Model >= 0x50 && Model <= 0x6f) {
      *Subtype = AMDFAM15H_BDVER4;
      break; // "bdver4"; 50h-6Fh: Excavator
    }
    if (Model >= 0x30 && Model <= 0x3f) {
      *Subtype = AMDFAM15H_BDVER3;
      break; // "bdver3"; 30h-3Fh: Steamroller
    }
    if (Model >= 0x10 && Model <= 0x1f) {
      *Subtype = AMDFAM15H_BDVER2;
      break; // "bdver2"; 10h-1Fh: Piledriver
    }
    if (Model <= 0x0f) {
      *Subtype = AMDFAM15H_BDVER1;
      break; // "bdver1"; 00h-0Fh: Bulldozer
    }
    break;
  case 22:
    *Type = AMDFAM16H;
    if (!(Features &
          (1 << FEATURE_AVX))) { // If no AVX support provide a sane fallback.
      *Subtype = AMD_BTVER1;
      break; // "btver1";
    }
    *Subtype = AMD_BTVER2;
    break; // "btver2"
  default:
    break; // "generic"
  }
}

static unsigned getAvailableFeatures(unsigned int ECX, unsigned int EDX,
                                     unsigned MaxLeaf) {
  unsigned Features = 0;
  unsigned int EAX, EBX;
  Features |= (((EDX >> 23) & 1) << FEATURE_MMX);
  Features |= (((EDX >> 25) & 1) << FEATURE_SSE);
  Features |= (((EDX >> 26) & 1) << FEATURE_SSE2);
  Features |= (((ECX >> 0) & 1) << FEATURE_SSE3);
  Features |= (((ECX >> 9) & 1) << FEATURE_SSSE3);
  Features |= (((ECX >> 19) & 1) << FEATURE_SSE4_1);
  Features |= (((ECX >> 20) & 1) << FEATURE_SSE4_2);
  Features |= (((ECX >> 22) & 1) << FEATURE_MOVBE);

  // If CPUID indicates support for XSAVE, XRESTORE and AVX, and XGETBV
  // indicates that the AVX registers will be saved and restored on context
  // switch, then we have full AVX support.
  const unsigned AVXBits = (1 << 27) | (1 << 28);
  bool HasAVX = ((ECX & AVXBits) == AVXBits) && !getX86XCR0(&EAX, &EDX) &&
                ((EAX & 0x6) == 0x6);
  bool HasAVX512Save = HasAVX && ((EAX & 0xe0) == 0xe0);
  bool HasLeaf7 =
      MaxLeaf >= 0x7 && !getX86CpuIDAndInfoEx(0x7, 0x0, &EAX, &EBX, &ECX, &EDX);
  bool HasADX = HasLeaf7 && ((EBX >> 19) & 1);
  bool HasAVX2 = HasAVX && HasLeaf7 && (EBX & 0x20);
  bool HasAVX512 = HasLeaf7 && HasAVX512Save && ((EBX >> 16) & 1);
  Features |= (HasAVX << FEATURE_AVX);
  Features |= (HasAVX2 << FEATURE_AVX2);
  Features |= (HasAVX512 << FEATURE_AVX512);
  Features |= (HasAVX512Save << FEATURE_AVX512SAVE);
  Features |= (HasADX << FEATURE_ADX);

  getX86CpuIDAndInfo(0x80000001, &EAX, &EBX, &ECX, &EDX);
  Features |= (((EDX >> 29) & 0x1) << FEATURE_EM64T);
  return Features;
}

StringRef sys::getHostCPUName() {
  unsigned EAX = 0, EBX = 0, ECX = 0, EDX = 0;
  unsigned MaxLeaf, Vendor;

  if (getX86CpuIDAndInfo(0, &MaxLeaf, &Vendor, &ECX, &EDX))
    return "generic";
  if (getX86CpuIDAndInfo(0x1, &EAX, &EBX, &ECX, &EDX))
    return "generic";

  unsigned Brand_id = EBX & 0xff;
  unsigned Family = 0, Model = 0;
  unsigned Features = 0;
  detectX86FamilyModel(EAX, &Family, &Model);
  Features = getAvailableFeatures(ECX, EDX, MaxLeaf);

  unsigned Type;
  unsigned Subtype;

  if (Vendor == SIG_INTEL) {
    getIntelProcessorTypeAndSubtype(Family, Model, Brand_id, Features, &Type,
                                    &Subtype);
    switch (Type) {
    case INTEL_i386:
      return "i386";
    case INTEL_i486:
      return "i486";
    case INTEL_PENTIUM:
      if (Subtype == INTEL_PENTIUM_MMX)
        return "pentium-mmx";
      return "pentium";
    case INTEL_PENTIUM_PRO:
      return "pentiumpro";
    case INTEL_PENTIUM_II:
      return "pentium2";
    case INTEL_PENTIUM_III:
      return "pentium3";
    case INTEL_PENTIUM_IV:
      return "pentium4";
    case INTEL_PENTIUM_M:
      return "pentium-m";
    case INTEL_CORE_DUO:
      return "yonah";
    case INTEL_CORE2:
      switch (Subtype) {
      case INTEL_CORE2_65:
        return "core2";
      case INTEL_CORE2_45:
        return "penryn";
      default:
        return "core2";
      }
    case INTEL_COREI7:
      switch (Subtype) {
      case INTEL_COREI7_NEHALEM:
        return "nehalem";
      case INTEL_COREI7_WESTMERE:
        return "westmere";
      case INTEL_COREI7_SANDYBRIDGE:
        return "sandybridge";
      case INTEL_COREI7_IVYBRIDGE:
        return "ivybridge";
      case INTEL_COREI7_HASWELL:
        return "haswell";
      case INTEL_COREI7_BROADWELL:
        return "broadwell";
      case INTEL_COREI7_SKYLAKE:
        return "skylake";
      case INTEL_COREI7_SKYLAKE_AVX512:
        return "skylake-avx512";
      default:
        return "corei7";
      }
    case INTEL_ATOM:
      switch (Subtype) {
      case INTEL_ATOM_BONNELL:
        return "bonnell";
      case INTEL_ATOM_SILVERMONT:
        return "silvermont";
      default:
        return "atom";
      }
    case INTEL_XEONPHI:
      return "knl"; /*update for more variants added*/
    case INTEL_X86_64:
      return "x86-64";
    case INTEL_NOCONA:
      return "nocona";
    case INTEL_PRESCOTT:
      return "prescott";
    default:
      return "generic";
    }
  } else if (Vendor == SIG_AMD) {
    getAMDProcessorTypeAndSubtype(Family, Model, Features, &Type, &Subtype);
    switch (Type) {
    case AMD_i486:
      return "i486";
    case AMDPENTIUM:
      switch (Subtype) {
      case AMDPENTIUM_K6:
        return "k6";
      case AMDPENTIUM_K62:
        return "k6-2";
      case AMDPENTIUM_K63:
        return "k6-3";
      case AMDPENTIUM_GEODE:
        return "geode";
      default:
        return "pentium";
      }
    case AMDATHLON:
      switch (Subtype) {
      case AMDATHLON_TBIRD:
        return "athlon-tbird";
      case AMDATHLON_MP:
        return "athlon-mp";
      case AMDATHLON_XP:
        return "athlon-xp";
      case AMDATHLON_K8SSE3:
        return "k8-sse3";
      case AMDATHLON_OPTERON:
        return "opteron";
      case AMDATHLON_FX:
        return "athlon-fx";
      case AMDATHLON_64:
        return "athlon64";
      default:
        return "athlon";
      }
    case AMDFAM10H:
      if(Subtype == AMDFAM10H_BARCELONA)
        return "barcelona";
      return "amdfam10";
    case AMDFAM14H:
      return "btver1";
    case AMDFAM15H:
      switch (Subtype) {
      case AMDFAM15H_BDVER1:
        return "bdver1";
      case AMDFAM15H_BDVER2:
        return "bdver2";
      case AMDFAM15H_BDVER3:
        return "bdver3";
      case AMDFAM15H_BDVER4:
        return "bdver4";
      case AMD_BTVER1:
        return "btver1";
      default:
        return "amdfam15";
      }
    case AMDFAM16H:
      switch (Subtype) {
      case AMD_BTVER1:
        return "btver1";
      case AMD_BTVER2:
        return "btver2";
      default:
        return "amdfam16";
      }
    default:
      return "generic";
    }
  }
  return "generic";
}

#elif defined(__APPLE__) && (defined(__ppc__) || defined(__powerpc__))
StringRef sys::getHostCPUName() {
  host_basic_info_data_t hostInfo;
  mach_msg_type_number_t infoCount;

  infoCount = HOST_BASIC_INFO_COUNT;
  host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&hostInfo,
            &infoCount);

  if (hostInfo.cpu_type != CPU_TYPE_POWERPC)
    return "generic";

  switch (hostInfo.cpu_subtype) {
  case CPU_SUBTYPE_POWERPC_601:
    return "601";
  case CPU_SUBTYPE_POWERPC_602:
    return "602";
  case CPU_SUBTYPE_POWERPC_603:
    return "603";
  case CPU_SUBTYPE_POWERPC_603e:
    return "603e";
  case CPU_SUBTYPE_POWERPC_603ev:
    return "603ev";
  case CPU_SUBTYPE_POWERPC_604:
    return "604";
  case CPU_SUBTYPE_POWERPC_604e:
    return "604e";
  case CPU_SUBTYPE_POWERPC_620:
    return "620";
  case CPU_SUBTYPE_POWERPC_750:
    return "750";
  case CPU_SUBTYPE_POWERPC_7400:
    return "7400";
  case CPU_SUBTYPE_POWERPC_7450:
    return "7450";
  case CPU_SUBTYPE_POWERPC_970:
    return "970";
  default:;
  }

  return "generic";
}
#elif defined(__linux__) && (defined(__ppc__) || defined(__powerpc__))
StringRef sys::getHostCPUName() {
  // Access to the Processor Version Register (PVR) on PowerPC is privileged,
  // and so we must use an operating-system interface to determine the current
  // processor type. On Linux, this is exposed through the /proc/cpuinfo file.
  const char *generic = "generic";

  // The cpu line is second (after the 'processor: 0' line), so if this
  // buffer is too small then something has changed (or is wrong).
  char buffer[1024];
  ssize_t CPUInfoSize = readCpuInfo(buffer, sizeof(buffer));
  if (CPUInfoSize == -1)
    return generic;

  const char *CPUInfoStart = buffer;
  const char *CPUInfoEnd = buffer + CPUInfoSize;

  const char *CIP = CPUInfoStart;

  const char *CPUStart = 0;
  size_t CPULen = 0;

  // We need to find the first line which starts with cpu, spaces, and a colon.
  // After the colon, there may be some additional spaces and then the cpu type.
  while (CIP < CPUInfoEnd && CPUStart == 0) {
    if (CIP < CPUInfoEnd && *CIP == '\n')
      ++CIP;

    if (CIP < CPUInfoEnd && *CIP == 'c') {
      ++CIP;
      if (CIP < CPUInfoEnd && *CIP == 'p') {
        ++CIP;
        if (CIP < CPUInfoEnd && *CIP == 'u') {
          ++CIP;
          while (CIP < CPUInfoEnd && (*CIP == ' ' || *CIP == '\t'))
            ++CIP;

          if (CIP < CPUInfoEnd && *CIP == ':') {
            ++CIP;
            while (CIP < CPUInfoEnd && (*CIP == ' ' || *CIP == '\t'))
              ++CIP;

            if (CIP < CPUInfoEnd) {
              CPUStart = CIP;
              while (CIP < CPUInfoEnd && (*CIP != ' ' && *CIP != '\t' &&
                                          *CIP != ',' && *CIP != '\n'))
                ++CIP;
              CPULen = CIP - CPUStart;
            }
          }
        }
      }
    }

    if (CPUStart == 0)
      while (CIP < CPUInfoEnd && *CIP != '\n')
        ++CIP;
  }

  if (CPUStart == 0)
    return generic;

  return StringSwitch<const char *>(StringRef(CPUStart, CPULen))
      .Case("604e", "604e")
      .Case("604", "604")
      .Case("7400", "7400")
      .Case("7410", "7400")
      .Case("7447", "7400")
      .Case("7455", "7450")
      .Case("G4", "g4")
      .Case("POWER4", "970")
      .Case("PPC970FX", "970")
      .Case("PPC970MP", "970")
      .Case("G5", "g5")
      .Case("POWER5", "g5")
      .Case("A2", "a2")
      .Case("POWER6", "pwr6")
      .Case("POWER7", "pwr7")
      .Case("POWER8", "pwr8")
      .Case("POWER8E", "pwr8")
      .Case("POWER9", "pwr9")
      .Default(generic);
}
#elif defined(__linux__) && defined(__arm__)
StringRef sys::getHostCPUName() {
  // The cpuid register on arm is not accessible from user space. On Linux,
  // it is exposed through the /proc/cpuinfo file.

  // Read 1024 bytes from /proc/cpuinfo, which should contain the CPU part line
  // in all cases.
  char buffer[1024];
  ssize_t CPUInfoSize = readCpuInfo(buffer, sizeof(buffer));
  if (CPUInfoSize == -1)
    return "generic";

  StringRef Str(buffer, CPUInfoSize);

  SmallVector<StringRef, 32> Lines;
  Str.split(Lines, "\n");

  // Look for the CPU implementer line.
  StringRef Implementer;
  for (unsigned I = 0, E = Lines.size(); I != E; ++I)
    if (Lines[I].startswith("CPU implementer"))
      Implementer = Lines[I].substr(15).ltrim("\t :");

  if (Implementer == "0x41") // ARM Ltd.
    // Look for the CPU part line.
    for (unsigned I = 0, E = Lines.size(); I != E; ++I)
      if (Lines[I].startswith("CPU part"))
        // The CPU part is a 3 digit hexadecimal number with a 0x prefix. The
        // values correspond to the "Part number" in the CP15/c0 register. The
        // contents are specified in the various processor manuals.
        return StringSwitch<const char *>(Lines[I].substr(8).ltrim("\t :"))
            .Case("0x926", "arm926ej-s")
            .Case("0xb02", "mpcore")
            .Case("0xb36", "arm1136j-s")
            .Case("0xb56", "arm1156t2-s")
            .Case("0xb76", "arm1176jz-s")
            .Case("0xc08", "cortex-a8")
            .Case("0xc09", "cortex-a9")
            .Case("0xc0f", "cortex-a15")
            .Case("0xc20", "cortex-m0")
            .Case("0xc23", "cortex-m3")
            .Case("0xc24", "cortex-m4")
            .Default("generic");

  if (Implementer == "0x51") // Qualcomm Technologies, Inc.
    // Look for the CPU part line.
    for (unsigned I = 0, E = Lines.size(); I != E; ++I)
      if (Lines[I].startswith("CPU part"))
        // The CPU part is a 3 digit hexadecimal number with a 0x prefix. The
        // values correspond to the "Part number" in the CP15/c0 register. The
        // contents are specified in the various processor manuals.
        return StringSwitch<const char *>(Lines[I].substr(8).ltrim("\t :"))
            .Case("0x06f", "krait") // APQ8064
            .Default("generic");

  return "generic";
}
#elif defined(__linux__) && defined(__s390x__)
StringRef sys::getHostCPUName() {
  // STIDP is a privileged operation, so use /proc/cpuinfo instead.

  // The "processor 0:" line comes after a fair amount of other information,
  // including a cache breakdown, but this should be plenty.
  char buffer[2048];
  ssize_t CPUInfoSize = readCpuInfo(buffer, sizeof(buffer));
  if (CPUInfoSize == -1)
    return "generic";

  StringRef Str(buffer, CPUInfoSize);
  SmallVector<StringRef, 32> Lines;
  Str.split(Lines, "\n");

  // Look for the CPU features.
  SmallVector<StringRef, 32> CPUFeatures;
  for (unsigned I = 0, E = Lines.size(); I != E; ++I)
    if (Lines[I].startswith("features")) {
      size_t Pos = Lines[I].find(":");
      if (Pos != StringRef::npos) {
        Lines[I].drop_front(Pos + 1).split(CPUFeatures, ' ');
        break;
      }
    }

  // We need to check for the presence of vector support independently of
  // the machine type, since we may only use the vector register set when
  // supported by the kernel (and hypervisor).
  bool HaveVectorSupport = false;
  for (unsigned I = 0, E = CPUFeatures.size(); I != E; ++I) {
    if (CPUFeatures[I] == "vx")
      HaveVectorSupport = true;
  }

  // Now check the processor machine type.
  for (unsigned I = 0, E = Lines.size(); I != E; ++I) {
    if (Lines[I].startswith("processor ")) {
      size_t Pos = Lines[I].find("machine = ");
      if (Pos != StringRef::npos) {
        Pos += sizeof("machine = ") - 1;
        unsigned int Id;
        if (!Lines[I].drop_front(Pos).getAsInteger(10, Id)) {
          if (Id >= 2964 && HaveVectorSupport)
            return "z13";
          if (Id >= 2827)
            return "zEC12";
          if (Id >= 2817)
            return "z196";
        }
      }
      break;
    }
  }

  return "generic";
}
#else
StringRef sys::getHostCPUName() { return "generic"; }
#endif

#if defined(i386) || defined(__i386__) || defined(__x86__) ||                  \
    defined(_M_IX86) || defined(__x86_64__) || defined(_M_AMD64) ||            \
    defined(_M_X64)
bool sys::getHostCPUFeatures(StringMap<bool> &Features) {
  unsigned EAX = 0, EBX = 0, ECX = 0, EDX = 0;
  unsigned MaxLevel;
  union {
    unsigned u[3];
    char c[12];
  } text;

  if (getX86CpuIDAndInfo(0, &MaxLevel, text.u + 0, text.u + 2, text.u + 1) ||
      MaxLevel < 1)
    return false;

  getX86CpuIDAndInfo(1, &EAX, &EBX, &ECX, &EDX);

  Features["cmov"] = (EDX >> 15) & 1;
  Features["mmx"] = (EDX >> 23) & 1;
  Features["sse"] = (EDX >> 25) & 1;
  Features["sse2"] = (EDX >> 26) & 1;
  Features["sse3"] = (ECX >> 0) & 1;
  Features["ssse3"] = (ECX >> 9) & 1;
  Features["sse4.1"] = (ECX >> 19) & 1;
  Features["sse4.2"] = (ECX >> 20) & 1;

  Features["pclmul"] = (ECX >> 1) & 1;
  Features["cx16"] = (ECX >> 13) & 1;
  Features["movbe"] = (ECX >> 22) & 1;
  Features["popcnt"] = (ECX >> 23) & 1;
  Features["aes"] = (ECX >> 25) & 1;
  Features["rdrnd"] = (ECX >> 30) & 1;

  // If CPUID indicates support for XSAVE, XRESTORE and AVX, and XGETBV
  // indicates that the AVX registers will be saved and restored on context
  // switch, then we have full AVX support.
  bool HasAVXSave = ((ECX >> 27) & 1) && ((ECX >> 28) & 1) &&
                    !getX86XCR0(&EAX, &EDX) && ((EAX & 0x6) == 0x6);
  Features["avx"] = HasAVXSave;
  Features["fma"] = HasAVXSave && (ECX >> 12) & 1;
  Features["f16c"] = HasAVXSave && (ECX >> 29) & 1;

  // Only enable XSAVE if OS has enabled support for saving YMM state.
  Features["xsave"] = HasAVXSave && (ECX >> 26) & 1;

  // AVX512 requires additional context to be saved by the OS.
  bool HasAVX512Save = HasAVXSave && ((EAX & 0xe0) == 0xe0);

  unsigned MaxExtLevel;
  getX86CpuIDAndInfo(0x80000000, &MaxExtLevel, &EBX, &ECX, &EDX);

  bool HasExtLeaf1 = MaxExtLevel >= 0x80000001 &&
                     !getX86CpuIDAndInfo(0x80000001, &EAX, &EBX, &ECX, &EDX);
  Features["lzcnt"] = HasExtLeaf1 && ((ECX >> 5) & 1);
  Features["sse4a"] = HasExtLeaf1 && ((ECX >> 6) & 1);
  Features["prfchw"] = HasExtLeaf1 && ((ECX >> 8) & 1);
  Features["xop"] = HasExtLeaf1 && ((ECX >> 11) & 1) && HasAVXSave;
  Features["fma4"] = HasExtLeaf1 && ((ECX >> 16) & 1) && HasAVXSave;
  Features["tbm"] = HasExtLeaf1 && ((ECX >> 21) & 1);
  Features["mwaitx"] = HasExtLeaf1 && ((ECX >> 29) & 1);

  bool HasLeaf7 =
      MaxLevel >= 7 && !getX86CpuIDAndInfoEx(0x7, 0x0, &EAX, &EBX, &ECX, &EDX);

  // AVX2 is only supported if we have the OS save support from AVX.
  Features["avx2"] = HasAVXSave && HasLeaf7 && ((EBX >> 5) & 1);

  Features["fsgsbase"] = HasLeaf7 && ((EBX >> 0) & 1);
  Features["sgx"] = HasLeaf7 && ((EBX >> 2) & 1);
  Features["bmi"] = HasLeaf7 && ((EBX >> 3) & 1);
  Features["hle"] = HasLeaf7 && ((EBX >> 4) & 1);
  Features["bmi2"] = HasLeaf7 && ((EBX >> 8) & 1);
  Features["invpcid"] = HasLeaf7 && ((EBX >> 10) & 1);
  Features["rtm"] = HasLeaf7 && ((EBX >> 11) & 1);
  Features["rdseed"] = HasLeaf7 && ((EBX >> 18) & 1);
  Features["adx"] = HasLeaf7 && ((EBX >> 19) & 1);
  Features["smap"] = HasLeaf7 && ((EBX >> 20) & 1);
  Features["pcommit"] = HasLeaf7 && ((EBX >> 22) & 1);
  Features["clflushopt"] = HasLeaf7 && ((EBX >> 23) & 1);
  Features["clwb"] = HasLeaf7 && ((EBX >> 24) & 1);
  Features["sha"] = HasLeaf7 && ((EBX >> 29) & 1);

  // AVX512 is only supported if the OS supports the context save for it.
  Features["avx512f"] = HasLeaf7 && ((EBX >> 16) & 1) && HasAVX512Save;
  Features["avx512dq"] = HasLeaf7 && ((EBX >> 17) & 1) && HasAVX512Save;
  Features["avx512ifma"] = HasLeaf7 && ((EBX >> 21) & 1) && HasAVX512Save;
  Features["avx512pf"] = HasLeaf7 && ((EBX >> 26) & 1) && HasAVX512Save;
  Features["avx512er"] = HasLeaf7 && ((EBX >> 27) & 1) && HasAVX512Save;
  Features["avx512cd"] = HasLeaf7 && ((EBX >> 28) & 1) && HasAVX512Save;
  Features["avx512bw"] = HasLeaf7 && ((EBX >> 30) & 1) && HasAVX512Save;
  Features["avx512vl"] = HasLeaf7 && ((EBX >> 31) & 1) && HasAVX512Save;

  Features["prefetchwt1"] = HasLeaf7 && (ECX & 1);
  Features["avx512vbmi"] = HasLeaf7 && ((ECX >> 1) & 1) && HasAVX512Save;
  // Enable protection keys
  Features["pku"] = HasLeaf7 && ((ECX >> 4) & 1);

  bool HasLeafD = MaxLevel >= 0xd &&
                  !getX86CpuIDAndInfoEx(0xd, 0x1, &EAX, &EBX, &ECX, &EDX);

  // Only enable XSAVE if OS has enabled support for saving YMM state.
  Features["xsaveopt"] = HasAVXSave && HasLeafD && ((EAX >> 0) & 1);
  Features["xsavec"] = HasAVXSave && HasLeafD && ((EAX >> 1) & 1);
  Features["xsaves"] = HasAVXSave && HasLeafD && ((EAX >> 3) & 1);

  return true;
}
#elif defined(__linux__) && (defined(__arm__) || defined(__aarch64__))
bool sys::getHostCPUFeatures(StringMap<bool> &Features) {
  // Read 1024 bytes from /proc/cpuinfo, which should contain the Features line
  // in all cases.
  char buffer[1024];
  ssize_t CPUInfoSize = readCpuInfo(buffer, sizeof(buffer));
  if (CPUInfoSize == -1)
    return false;

  StringRef Str(buffer, CPUInfoSize);

  SmallVector<StringRef, 32> Lines;
  Str.split(Lines, "\n");

  SmallVector<StringRef, 32> CPUFeatures;

  // Look for the CPU features.
  for (unsigned I = 0, E = Lines.size(); I != E; ++I)
    if (Lines[I].startswith("Features")) {
      Lines[I].split(CPUFeatures, ' ');
      break;
    }

#if defined(__aarch64__)
  // Keep track of which crypto features we have seen
  enum { CAP_AES = 0x1, CAP_PMULL = 0x2, CAP_SHA1 = 0x4, CAP_SHA2 = 0x8 };
  uint32_t crypto = 0;
#endif

  for (unsigned I = 0, E = CPUFeatures.size(); I != E; ++I) {
    StringRef LLVMFeatureStr = StringSwitch<StringRef>(CPUFeatures[I])
#if defined(__aarch64__)
                                   .Case("asimd", "neon")
                                   .Case("fp", "fp-armv8")
                                   .Case("crc32", "crc")
#else
                                   .Case("half", "fp16")
                                   .Case("neon", "neon")
                                   .Case("vfpv3", "vfp3")
                                   .Case("vfpv3d16", "d16")
                                   .Case("vfpv4", "vfp4")
                                   .Case("idiva", "hwdiv-arm")
                                   .Case("idivt", "hwdiv")
#endif
                                   .Default("");

#if defined(__aarch64__)
    // We need to check crypto separately since we need all of the crypto
    // extensions to enable the subtarget feature
    if (CPUFeatures[I] == "aes")
      crypto |= CAP_AES;
    else if (CPUFeatures[I] == "pmull")
      crypto |= CAP_PMULL;
    else if (CPUFeatures[I] == "sha1")
      crypto |= CAP_SHA1;
    else if (CPUFeatures[I] == "sha2")
      crypto |= CAP_SHA2;
#endif

    if (LLVMFeatureStr != "")
      Features[LLVMFeatureStr] = true;
  }

#if defined(__aarch64__)
  // If we have all crypto bits we can add the feature
  if (crypto == (CAP_AES | CAP_PMULL | CAP_SHA1 | CAP_SHA2))
    Features["crypto"] = true;
#endif

  return true;
}
#else
bool sys::getHostCPUFeatures(StringMap<bool> &Features) { return false; }
#endif

std::string sys::getProcessTriple() {
  Triple PT(Triple::normalize(LLVM_HOST_TRIPLE));

  if (sizeof(void *) == 8 && PT.isArch32Bit())
    PT = PT.get64BitArchVariant();
  if (sizeof(void *) == 4 && PT.isArch64Bit())
    PT = PT.get32BitArchVariant();

  return PT.str();
}
