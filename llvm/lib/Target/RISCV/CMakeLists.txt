set(LLVM_TARGET_DEFINITIONS RISCV.td)

tablegen(LLVM RISCVGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM RISCVGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM RISCVGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM RISCVGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM RISCVGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(RISCVCommonTableGen)

add_llvm_target(RISCVCodeGen
  RISCVTargetMachine.cpp

  STATIC
  )

add_subdirectory(AsmParser)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)

# Fix order of *.inc/*.td dependencies
add_dependencies(LLVMRISCVAsmParser RISCVCommonTableGen)
add_dependencies(LLVMRISCVDesc RISCVCommonTableGen)
add_dependencies(LLVMRISCVInfo RISCVCommonTableGen)
