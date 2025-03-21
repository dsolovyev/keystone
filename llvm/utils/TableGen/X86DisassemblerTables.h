//===- X86DisassemblerTables.h - Disassembler tables ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is part of the X86 Disassembler Emitter.
// It contains the interface of the disassembler tables.
// Documentation for the disassembler emitter in general can be found in
//  X86DisasemblerEmitter.h.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UTILS_TABLEGEN_X86DISASSEMBLERTABLES_H
#define LLVM_UTILS_TABLEGEN_X86DISASSEMBLERTABLES_H

#include "X86DisassemblerShared.h"
#include "X86ModRMFilters.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>

namespace llvm_ks {

namespace X86Disassembler {

/// DisassemblerTables - Encapsulates all the decode tables being generated by
///   the table emitter.  Contains functions to populate the tables as well as
///   to emit them as hierarchical C structures suitable for consumption by the
///   runtime.
class DisassemblerTables {
private:
  /// The decoder tables.  There is one for each opcode type:
  /// [0] one-byte opcodes
  /// [1] two-byte opcodes of the form 0f __
  /// [2] three-byte opcodes of the form 0f 38 __
  /// [3] three-byte opcodes of the form 0f 3a __
  /// [4] XOP8 map opcode
  /// [5] XOP9 map opcode
  /// [6] XOPA map opcode
  ContextDecision* Tables[7];

  // Table of ModRM encodings.
  typedef std::map<std::vector<unsigned>, unsigned> ModRMMapTy;
  mutable ModRMMapTy ModRMTable;

  /// The instruction information table
  std::vector<InstructionSpecifier> InstructionSpecifiers;

  /// True if there are primary decode conflicts in the instruction set
  bool HasConflicts;

  /// emitModRMDecision - Emits a table of entries corresponding to a single
  ///   ModR/M decision.  Compacts the ModR/M decision if possible.  ModR/M
  ///   decisions are printed as:
  ///
  ///   { /* struct ModRMDecision */
  ///     TYPE,
  ///     modRMTablennnn
  ///   }
  ///
  ///   where nnnn is a unique ID for the corresponding table of IDs.
  ///   TYPE indicates whether the table has one entry that is the same
  ///   regardless of ModR/M byte, two entries - one for bytes 0x00-0xbf and one
  ///   for bytes 0xc0-0xff -, or 256 entries, one for each possible byte.
  ///   nnnn is the number of a table for looking up these values.  The tables
  ///   are written separately so that tables consisting entirely of zeros will
  ///   not be duplicated.  (These all have the name modRMEmptyTable.)  A table
  ///   is printed as:
  ///
  ///   InstrUID modRMTablennnn[k] = {
  ///     nnnn, /* MNEMONIC */
  ///     ...
  ///     nnnn /* MNEMONIC */
  ///   };
  ///
  /// @param o1       - The output stream to print the ID table to.
  /// @param o2       - The output stream to print the decision structure to.
  /// @param i1       - The indentation level to use with stream o1.
  /// @param i2       - The indentation level to use with stream o2.
  /// @param ModRMTableNum - next table number for adding to ModRMTable.
  /// @param decision - The ModR/M decision to emit.  This decision has 256
  ///                   entries - emitModRMDecision decides how to compact it.
  void emitModRMDecision(raw_ostream &o1, raw_ostream &o2,
                         unsigned &i1, unsigned &i2, unsigned &ModRMTableNum,
                         ModRMDecision &decision) const;

  /// emitOpcodeDecision - Emits an OpcodeDecision and all its subsidiary ModR/M
  ///   decisions.  An OpcodeDecision is printed as:
  ///
  ///   { /* struct OpcodeDecision */
  ///     /* 0x00 */
  ///     { /* struct ModRMDecision */
  ///       ...
  ///     }
  ///     ...
  ///   }
  ///
  ///   where the ModRMDecision structure is printed as described in the
  ///   documentation for emitModRMDecision().  emitOpcodeDecision() passes on a
  ///   stream and indent level for the UID tables generated by
  ///   emitModRMDecision(), but does not use them itself.
  ///
  /// @param o1       - The output stream to print the ID tables generated by
  ///                   emitModRMDecision() to.
  /// @param o2       - The output stream for the decision structure itself.
  /// @param i1       - The indent level to use with stream o1.
  /// @param i2       - The indent level to use with stream o2.
  /// @param ModRMTableNum - next table number for adding to ModRMTable.
  /// @param decision - The OpcodeDecision to emit along with its subsidiary
  ///                    structures.
  void emitOpcodeDecision(raw_ostream &o1, raw_ostream &o2,
                          unsigned &i1, unsigned &i2, unsigned &ModRMTableNum,
                          OpcodeDecision &decision) const;

  /// emitContextDecision - Emits a ContextDecision and all its subsidiary
  ///   Opcode and ModRMDecisions.  A ContextDecision is printed as:
  ///
  ///   struct ContextDecision NAME = {
  ///     { /* OpcodeDecisions */
  ///       /* IC */
  ///       { /* struct OpcodeDecision */
  ///         ...
  ///       },
  ///       ...
  ///     }
  ///   }
  ///
  ///   NAME is the name of the ContextDecision (typically one of the four names
  ///   ONEBYTE_SYM, TWOBYTE_SYM, THREEBYTE38_SYM, THREEBYTE3A_SYM from
  ///   X86DisassemblerDecoderCommon.h).
  ///   IC is one of the contexts in InstructionContext.  There is an opcode
  ///   decision for each possible context.
  ///   The OpcodeDecision structures are printed as described in the
  ///   documentation for emitOpcodeDecision.
  ///
  /// @param o1       - The output stream to print the ID tables generated by
  ///                   emitModRMDecision() to.
  /// @param o2       - The output stream to print the decision structure to.
  /// @param i1       - The indent level to use with stream o1.
  /// @param i2       - The indent level to use with stream o2.
  /// @param ModRMTableNum - next table number for adding to ModRMTable.
  /// @param decision - The ContextDecision to emit along with its subsidiary
  ///                   structures.
  /// @param name     - The name for the ContextDecision.
  void emitContextDecision(raw_ostream &o1, raw_ostream &o2,
                           unsigned &i1, unsigned &i2, unsigned &ModRMTableNum,
                           ContextDecision &decision, const char* name) const;

  /// emitInstructionInfo - Prints the instruction specifier table, which has
  ///   one entry for each instruction, and contains name and operand
  ///   information.  This table is printed as:
  ///
  ///   struct InstructionSpecifier CONTEXTS_SYM[k] = {
  ///     {
  ///       /* nnnn */
  ///       "MNEMONIC",
  ///       0xnn,
  ///       {
  ///         {
  ///           ENCODING,
  ///           TYPE
  ///         },
  ///         ...
  ///       }
  ///     },
  ///   };
  ///
  ///   k is the total number of instructions.
  ///   nnnn is the ID of the current instruction (0-based).  This table
  ///   includes entries for non-instructions like PHINODE.
  ///   0xnn is the lowest possible opcode for the current instruction, used for
  ///   AddRegFrm instructions to compute the operand's value.
  ///   ENCODING and TYPE describe the encoding and type for a single operand.
  ///
  /// @param o  - The output stream to which the instruction table should be
  ///             written.
  /// @param i  - The indent level for use with the stream.
  void emitInstructionInfo(raw_ostream &o, unsigned &i) const;

  /// emitContextTable - Prints the table that is used to translate from an
  ///   instruction attribute mask to an instruction context.  This table is
  ///   printed as:
  ///
  ///   InstructionContext CONTEXTS_STR[256] = {
  ///     IC, /* 0x00 */
  ///     ...
  ///   };
  ///
  ///   IC is the context corresponding to the mask 0x00, and there are 256
  ///   possible masks.
  ///
  /// @param o  - The output stream to which the context table should be written.
  /// @param i  - The indent level for use with the stream.
  void emitContextTable(raw_ostream &o, uint32_t &i) const;

  /// emitContextDecisions - Prints all four ContextDecision structures using
  ///   emitContextDecision().
  ///
  /// @param o1 - The output stream to print the ID tables generated by
  ///             emitModRMDecision() to.
  /// @param o2 - The output stream to print the decision structures to.
  /// @param i1 - The indent level to use with stream o1.
  /// @param i2 - The indent level to use with stream o2.
  /// @param ModRMTableNum - next table number for adding to ModRMTable.
  void emitContextDecisions(raw_ostream &o1, raw_ostream &o2,
                            unsigned &i1, unsigned &i2,
                            unsigned &ModRMTableNum) const;

  /// setTableFields - Uses a ModRMFilter to set the appropriate entries in a
  ///   ModRMDecision to refer to a particular instruction ID.
  ///
  /// @param decision - The ModRMDecision to populate.
  /// @param filter   - The filter to use in deciding which entries to populate.
  /// @param uid      - The unique ID to set matching entries to.
  /// @param opcode   - The opcode of the instruction, for error reporting.
  void setTableFields(ModRMDecision &decision,
                      const ModRMFilter &filter,
                      InstrUID uid,
                      uint8_t opcode);
public:
  /// Constructor - Allocates space for the class decisions and clears them.
  DisassemblerTables();

  ~DisassemblerTables();

  /// emit - Emits the instruction table, context table, and class decisions.
  ///
  /// @param o  - The output stream to print the tables to.
  void emit(raw_ostream &o) const;

  /// setTableFields - Uses the opcode type, instruction context, opcode, and a
  ///   ModRMFilter as criteria to set a particular set of entries in the
  ///   decode tables to point to a specific uid.
  ///
  /// @param type         - The opcode type (ONEBYTE, TWOBYTE, etc.)
  /// @param insnContext  - The context to use (IC, IC_64BIT, etc.)
  /// @param opcode       - The last byte of the opcode (not counting any escape
  ///                       or extended opcodes).
  /// @param filter       - The ModRMFilter that decides which ModR/M byte values
  ///                       correspond to the desired instruction.
  /// @param uid          - The unique ID of the instruction.
  /// @param is32bit      - Instructon is only 32-bit
  /// @param ignoresVEX_L - Instruction ignores VEX.L
  /// @param AddrSize     - Instructions address size 16/32/64. 0 is unspecified
  void setTableFields(OpcodeType type,
                      InstructionContext insnContext,
                      uint8_t opcode,
                      const ModRMFilter &filter,
                      InstrUID uid,
                      bool is32bit,
                      bool ignoresVEX_L,
                      unsigned AddrSize);

  /// specForUID - Returns the instruction specifier for a given unique
  ///   instruction ID.  Used when resolving collisions.
  ///
  /// @param uid  - The unique ID of the instruction.
  /// @return     - A reference to the instruction specifier.
  InstructionSpecifier& specForUID(InstrUID uid) {
    if (uid >= InstructionSpecifiers.size())
      InstructionSpecifiers.resize(uid + 1);

    return InstructionSpecifiers[uid];
  }

  // hasConflicts - Reports whether there were primary decode conflicts
  //   from any instructions added to the tables.
  // @return  - true if there were; false otherwise.

  bool hasConflicts() {
    return HasConflicts;
  }
};

} // namespace X86Disassembler

} // namespace llvm_ks

#endif
