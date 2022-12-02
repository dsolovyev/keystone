set(system_libs)
if( MSVC OR MINGW )
  # libuuid required for FOLDERID_Profile usage in lib/Support/Windows/Path.inc.
  set(system_libs ${system_libs} psapi shell32 ole32 uuid)
elseif( CMAKE_HOST_UNIX )
  if( HAVE_LIBRT )
    set(system_libs ${system_libs} rt)
  endif()
  if( HAVE_LIBDL )
    set(system_libs ${system_libs} ${CMAKE_DL_LIBS})
  endif()
  if(LLVM_ENABLE_TERMINFO)
    if(HAVE_TERMINFO)
      set(system_libs ${system_libs} ${TERMINFO_LIBS})
    endif()
  endif()
  if( LLVM_ENABLE_THREADS AND HAVE_LIBATOMIC )
    set(system_libs ${system_libs} atomic)
  endif()
  set(system_libs ${system_libs} ${PTHREAD_LIB})
  if ( LLVM_ENABLE_ZLIB AND HAVE_LIBZ )
    set(system_libs ${system_libs} z)
  endif()
  if( UNIX AND NOT (BEOS OR HAIKU) )
    set(system_libs ${system_libs} m)
  endif()
endif( MSVC OR MINGW )

add_llvm_library(LLVMSupport
  APFloat.cpp
  APInt.cpp
  APSInt.cpp
  ARMBuildAttrs.cpp
  circular_raw_ostream.cpp
  CommandLine.cpp
  ConvertUTF.c
  ConvertUTFWrapper.cpp
  Debug.cpp
  ErrorHandling.cpp
  FoldingSet.cpp
  FormattedStream.cpp
  Hashing.cpp
  IntEqClasses.cpp
  LEB128.cpp
  ManagedStatic.cpp
  MemoryBuffer.cpp
  PrettyStackTrace.cpp
  Regex.cpp
  ScaledNumber.cpp
  SmallPtrSet.cpp
  SmallVector.cpp
  SourceMgr.cpp
  StringExtras.cpp
  StringMap.cpp
  StringPool.cpp
  StringSaver.cpp
  StringRef.cpp
  TargetParser.cpp
  ToolOutputFile.cpp
  Triple.cpp
  Twine.cpp
  raw_ostream.cpp
  regcomp.c
  regerror.c
  regexec.c
  regfree.c
  regstrlcpy.c

# System
  Errno.cpp
  Host.cpp
  Memory.cpp
  Mutex.cpp
  Path.cpp
  Program.cpp
  Signals.cpp
  TargetRegistry.cpp
  Threading.cpp
  Watchdog.cpp

  ADDITIONAL_HEADER_DIRS
  Unix
  Windows
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Support

  LINK_LIBS ${system_libs}

  STATIC
  )

set_property(TARGET LLVMSupport PROPERTY LLVM_SYSTEM_LIBS "${system_libs}")
