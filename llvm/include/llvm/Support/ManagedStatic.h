//===-- llvm/Support/ManagedStatic.h - Static Global wrapper ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the ManagedStatic class and the llvm_shutdown() function.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_MANAGEDSTATIC_H
#define LLVM_SUPPORT_MANAGEDSTATIC_H

#include "llvm/Support/Compiler.h"
#include <atomic>
#include <cstddef>

namespace llvm_ks {

/// object_creator - Helper method for ManagedStatic.
template<class C>
LLVM_LIBRARY_VISIBILITY void* object_creator() {
  return new C();
}

/// object_deleter - Helper method for ManagedStatic.
///
template <typename T> struct LLVM_LIBRARY_VISIBILITY object_deleter {
  static void call(void *Ptr) { delete (T *)Ptr; }
};
template <typename T, size_t N>
struct LLVM_LIBRARY_VISIBILITY object_deleter<T[N]> {
  static void call(void *Ptr) { delete[](T *)Ptr; }
};

/// ManagedStaticBase - Common base class for ManagedStatic instances.
class ManagedStaticBase {
protected:
  // This should only be used as a static variable, which guarantees that this
  // will be zero initialized.
  mutable std::atomic<void *> Ptr;
  mutable void (*DeleterFn)(void*);
  mutable const ManagedStaticBase *Next;

  void RegisterManagedStatic(void *(*creator)(), void (*deleter)(void*)) const;
public:
  /// isConstructed - Return true if this object has not been created yet.
  bool isConstructed() const { return Ptr != nullptr; }

  void destroy() const;
};

/// ManagedStatic - This transparently changes the behavior of global statics to
/// be lazily constructed on demand (good for reducing startup times of dynamic
/// libraries that link in LLVM components) and for making destruction be
/// explicit through the llvm_shutdown() function call.
///
template<class C>
class ManagedStatic : public ManagedStaticBase {
public:
  // Accessors.
  C &operator*() {
    void *Tmp = Ptr.load(std::memory_order_acquire);
    if (!Tmp)
      RegisterManagedStatic(object_creator<C>, object_deleter<C>::call);

    return *static_cast<C *>(Ptr.load(std::memory_order_relaxed));
  }

  C *operator->() { return &**this; }

  const C &operator*() const {
    void *Tmp = Ptr.load(std::memory_order_acquire);
    if (!Tmp)
      RegisterManagedStatic(object_creator<C>, object_deleter<C>::call);

    return *static_cast<C *>(Ptr.load(std::memory_order_relaxed));
  }

  const C *operator->() const { return &**this; }
};

/// llvm_shutdown - Deallocate and destroy all ManagedStatic variables.
void llvm_shutdown();

/// llvm_shutdown_obj - This is a simple helper class that calls
/// llvm_shutdown() when it is destroyed.
struct llvm_shutdown_obj {
  llvm_shutdown_obj() { }
  ~llvm_shutdown_obj() { llvm_shutdown(); }
};

}

#endif
