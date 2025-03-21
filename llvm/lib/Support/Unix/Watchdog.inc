//===--- Unix/Watchdog.inc - Unix Watchdog Implementation -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the generic Unix implementation of the Watchdog class.
//
//===----------------------------------------------------------------------===//

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace llvm_ks {
  namespace sys {
    Watchdog::Watchdog(unsigned int seconds) {
#ifdef HAVE_UNISTD_H
      alarm(seconds);
#endif
    }

    Watchdog::~Watchdog() {
#ifdef HAVE_UNISTD_H
      alarm(0);
#endif
    }
  }
}
