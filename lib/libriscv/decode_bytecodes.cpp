#include "machine.hpp"
#include "instruction_list.hpp"
#include "threaded_bytecodes.hpp"
#include "rv32i_instr.hpp"
#include "rvfd.hpp"
#ifdef RISCV_EXT_COMPRESSED
#include "rvc.hpp"
#endif
#ifdef RISCV_EXT_VECTOR
#include "rvv.hpp"
#endif

namespace riscv {

template <int W>
size_t CPU<W>::computed_index_for(rv32i_instruction instr)
{
#ifdef RISCV_BINARY_TRANSLATION
	if (instr.whole == FASTSIM_BLOCK_END)
		return RV32I_BC_TRANSLATOR;
#endif

#ifdef RISCV_EXT_COMPRESSED
	if (instr.length() == 2)
	{
		// RISC-V Compressed Extension
		const rv32c_instruction ci{instr};
		#define CI_CODE(x, y) ((x << 13) | (y))
		switch (ci.opcode())
		{
			case CI_CODE(0b000, 0b00):
				// if all bits are zero, it's an illegal instruction
				if (ci.whole != 0x0) {
					return RV32C_BC_ADDI; // C.ADDI4SPN
				}
				return RV32I_BC_INVALID;
			case CI_CODE(0b001, 0b00):
			case CI_CODE(0b010, 0b00):
			case CI_CODE(0b011, 0b00): {
				if (ci.CL.funct3 == 0x1) {
					return RV32C_BC_FUNCTION; // C.FLD
				}
				else if (ci.CL.funct3 == 0x2) {
					return RV32C_BC_FUNCTION; // C.LW
				}
				else if (ci.CL.funct3 == 0x3) {
					if constexpr (sizeof(address_t) == 8) {
						return RV32C_BC_FUNCTION; // C.LD
					} else {
						return RV32C_BC_FUNCTION; // C.FLW
					}
				}
				return RV32C_BC_FUNCTION; // C.UNIMP
			}
			// RESERVED: 0b100, 0b00
			case CI_CODE(0b101, 0b00):
			case CI_CODE(0b110, 0b00):
			case CI_CODE(0b111, 0b00): {
				switch (ci.CS.funct3) {
				case 4:
					return RV32C_BC_FUNCTION; // C.UNIMP
				case 5:
					return RV32C_BC_FUNCTION; // C.FSD
				case 6:
					return RV32C_BC_FUNCTION; // C.SW
				case 7: // C.SD / C.FSW
					if constexpr (sizeof(address_t) == 8) {
						return RV32C_BC_STD; // C.SD
					} else {
						return RV32C_BC_FUNCTION; // C.FSW
					}
				}
				return RV32C_BC_FUNCTION; // C.UNIMP?
			}
			case CI_CODE(0b000, 0b01):
				if (ci.CI.rd != 0) {
					return RV32C_BC_ADDI; // C.ADDI
				}
				return RV32C_BC_FUNCTION; // C.NOP
			case CI_CODE(0b010, 0b01):
				if (ci.CI.rd != 0) {
					return RV32C_BC_LI; // C.LI
				}
				return RV32C_BC_FUNCTION; // C.NOP
			case CI_CODE(0b011, 0b01):
				if (ci.CI.rd == 2) {
					return RV32C_BC_ADDI; // C.ADDI16SP
				}
				else if (ci.CI.rd != 0) {
					return RV32C_BC_FUNCTION; // C.LUI
				}
				return RV32C_BC_FUNCTION; // ILLEGAL
			case CI_CODE(0b001, 0b01):
				if constexpr (W == 8) {
					return RV32C_BC_FUNCTION; // C.ADDIW
				} else {
					return RV32C_BC_JUMPFUNC; // C.JAL
				}
			case CI_CODE(0b101, 0b01): // C.JAL
				return RV32C_BC_JUMPFUNC;
			case CI_CODE(0b110, 0b01): // C.BEQZ
				return RV32C_BC_JUMPFUNC;
			case CI_CODE(0b111, 0b01): // C.BNEZ
				return RV32C_BC_BNEZ;
			// Quadrant 2
			case CI_CODE(0b000, 0b10):
			case CI_CODE(0b001, 0b10):
			case CI_CODE(0b010, 0b10):
			case CI_CODE(0b011, 0b10): {
				if (ci.CI.funct3 == 0x0 && ci.CI.rd != 0) {
					return RV32C_BC_FUNCTION; // C.SLLI
				}
				else if (ci.CI2.funct3 == 0x1) {
					return RV32C_BC_FUNCTION; // C.FLDSP
				}
				else if (ci.CI2.funct3 == 0x2 && ci.CI2.rd != 0) {
					return RV32C_BC_FUNCTION; // C.LWSP
				}
				else if (ci.CI2.funct3 == 0x3) {
					if constexpr (sizeof(address_t) == 8) {
						return RV32C_BC_LDD; // C.LDSP
					} else {
						return RV32C_BC_FUNCTION; // C.FLWSP
					}
				}
				else if (ci.CI.rd == 0) {
					return RV32C_BC_FUNCTION; // C.HINT
				}
				return RV32C_BC_FUNCTION; // C.UNIMP?
			}
			case CI_CODE(0b100, 0b10): {
				const bool topbit = ci.whole & (1 << 12);
				if (!topbit && ci.CR.rd != 0 && ci.CR.rs2 == 0)
				{
					return RV32C_BC_JUMPFUNC; // C.JR rd
				}
				else if (topbit && ci.CR.rd != 0 && ci.CR.rs2 == 0)
				{
					return RV32C_BC_JUMPFUNC; // C.JALR ra, rd+0
				}
				else if (!topbit && ci.CR.rd != 0 && ci.CR.rs2 != 0)
				{	// MV rd, rs2
					return RV32C_BC_MV; // C.MV
				}
				else if (ci.CR.rd != 0)
				{	// ADD rd, rd + rs2
					return RV32C_BC_FUNCTION; // C.ADD
				}
				else if (topbit && ci.CR.rd == 0 && ci.CR.rs2 == 0)
				{	// EBREAK
					return RV32C_BC_FUNCTION; // C.EBREAK
				}
				return RV32C_BC_FUNCTION; // C.UNIMP?
			}
			case CI_CODE(0b101, 0b10):
			case CI_CODE(0b110, 0b10):
			case CI_CODE(0b111, 0b10): {
				if (ci.CSS.funct3 == 5) {
					return RV32C_BC_FUNCTION; // FSDSP
				}
				else if (ci.CSS.funct3 == 6) {
					return RV32C_BC_FUNCTION; // SWSP
				}
				else if (ci.CSS.funct3 == 7) {
					if constexpr (W == 8) {
						return RV32C_BC_STD; // SDSP
					} else {
						return RV32C_BC_FUNCTION; // FSWSP
					}
				}
				return RV32C_BC_FUNCTION; // C.UNIMP?
			}
			default:
				return RV32C_BC_FUNCTION;
		}
	}
#endif

	switch (instr.opcode())
	{
		case RV32I_LOAD:
			// XXX: Support dummy loads
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Itype.funct3) {
			case 0x0: // LD.B
				return RV32I_BC_LDB;
			case 0x1: // LD.H
				return RV32I_BC_LDH;
			case 0x2: // LD.W
				return RV32I_BC_LDW;
			case 0x3:
				if constexpr (W >= 8) {
					return RV32I_BC_LDD;
				}
				return RV32I_BC_INVALID;
			case 0x4: // LD.BU
				return RV32I_BC_LDBU;
			case 0x5: // LD.HU
				return RV32I_BC_LDHU;
			case 0x6: // LD.WU
				return RV32I_BC_LDWU;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_STORE:
			switch (instr.Stype.funct3)
			{
			case 0x0: // SD.B
				return RV32I_BC_STB;
			case 0x1: // SD.H
				return RV32I_BC_STH;
			case 0x2: // SD.W
				return RV32I_BC_STW;
			case 0x3:
				if constexpr (W >= 8) {
					return RV32I_BC_STD;
				}
				return RV32I_BC_INVALID;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_BRANCH:
			switch (instr.Btype.funct3) {
			case 0x0: // BEQ
				return RV32I_BC_BEQ;
			case 0x1: // BNE
				return RV32I_BC_BNE;
			case 0x4: // BLT
				return RV32I_BC_BLT;
			case 0x5: // BGE
				return RV32I_BC_BGE;
			case 0x6: // BLTU
				return RV32I_BC_BLTU;
			case 0x7: // BGEU
				return RV32I_BC_BGEU;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_LUI:
			if (instr.Utype.rd == 0)
				return RV32I_BC_NOP;
			return RV32I_BC_LUI;
		case RV32I_AUIPC:
			if (instr.Utype.rd == 0)
				return RV32I_BC_NOP;
			return RV32I_BC_AUIPC;
		case RV32I_JAL:
			return RV32I_BC_JAL;
		case RV32I_JALR:
			return RV32I_BC_JALR;
		case RV32I_OP_IMM:
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Itype.funct3)
			{
			case 0x0:
				if (instr.Itype.rs1 == 0)
					return RV32I_BC_LI;
				else if (instr.Itype.signed_imm() == 0)
					return RV32I_BC_MV;
				else
					return RV32I_BC_ADDI;
			case 0x1: // SLLI
				if (instr.Itype.imm < 128)
					return RV32I_BC_SLLI;
				else
					return RV32I_BC_FUNCTION;
			case 0x2: // SLTI
				return RV32I_BC_SLTI;
			case 0x3: // SLTIU
				return RV32I_BC_SLTIU;
			case 0x4: // XORI
				return RV32I_BC_XORI;
			case 0x5:
				if (instr.Itype.high_bits() == 0x0)
					return RV32I_BC_SRLI;
				else if (instr.Itype.is_srai())
					return RV32I_BC_SRAI;
				else
					return RV32I_BC_FUNCTION;
			case 0x6:
				return RV32I_BC_ORI;
			case 0x7:
				return RV32I_BC_ANDI;
			default:
				return RV32I_BC_FUNCTION;
			}
		case RV32I_OP:
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Rtype.jumptable_friendly_op())
			{
			case 0x0:
				return RV32I_BC_OP_ADD;
			case 0x200:
				return RV32I_BC_OP_SUB;
			case 0x1:
				return RV32I_BC_OP_SLL;
			case 0x2:
				return RV32I_BC_OP_SLT;
			case 0x3:
				return RV32I_BC_OP_SLTU;
			case 0x4:
				return RV32I_BC_OP_XOR;
			case 0x5:
				if (instr.Itype.high_bits() == 0x0)
					return RV32I_BC_OP_SRL;
				else
					return RV32I_BC_FUNCTION;
			case 0x6:
				return RV32I_BC_OP_OR;
			case 0x7:
				return RV32I_BC_OP_AND;
			case 0x10:
				return RV32I_BC_OP_MUL;
			case 0x11:
				return RV32I_BC_OP_MULH;
			case 0x12:
				return RV32I_BC_OP_MULHSU;
			case 0x13:
				return RV32I_BC_OP_MULHU;
			case 0x14:
				return RV32I_BC_OP_DIV;
			case 0x15:
				return RV32I_BC_OP_DIVU;
			case 0x16:
				return RV32I_BC_OP_REM;
			case 0x17:
				return RV32I_BC_OP_REMU;
			case 0x102:
				return RV32I_BC_OP_SH1ADD;
			case 0x104:
				return RV32I_BC_OP_SH2ADD;
			case 0x106:
				return RV32I_BC_OP_SH3ADD;
			case 0x205:
				return RV32I_BC_OP_SRA;
			case 0x204: // XNOR
			case 0x206: // ORN
			case 0x207: // ANDN
			case 0x54: // MIN
			case 0x55: // MINU
			case 0x56: // MAX
			case 0x57: // MAXU
			case 0x301: // ROL
				return RV32I_BC_FUNCTION;
			default:
				return RV32I_BC_INVALID;
			}
		case RV64I_OP32:
			return RV32I_BC_FUNCTION;
		case RV64I_OP_IMM32:
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Itype.funct3)
			{
			case 0x0:
				return RV64I_BC_ADDIW;
			}
			return RV32I_BC_FUNCTION;
		case RV32I_SYSTEM:
			if (LIKELY(instr.Itype.funct3 == 0))
			{
				if (instr.Itype.imm == 0) {
					return RV32I_BC_SYSCALL;
				}
				// WFI and STOP
				if (instr.Itype.imm == 0x105 || instr.Itype.imm == 0x7ff) {
					return RV32I_BC_STOP;
				}
			}
			return RV32I_BC_SYSTEM;
		case RV32I_FENCE:
			return RV32I_BC_NOP;
		case RV32F_LOAD: {
			const rv32f_instruction fi{instr};
			switch (fi.Itype.funct3) {
			case 0x2: // FLW
				return RV32F_BC_FLW;
			case 0x3: // FLD
				return RV32F_BC_FLD;
#ifdef RISCV_EXT_VECTOR
			case 0x6: // VLE32
				return RV32V_BC_VLE32;
#endif
			default:
				return RV32I_BC_INVALID;
			}
		}
		case RV32F_STORE: {
			const rv32f_instruction fi{instr};
			switch (fi.Itype.funct3) {
			case 0x2: // FSW
				return RV32F_BC_FSW;
			case 0x3: // FSD
				return RV32F_BC_FSD;
#ifdef RISCV_EXT_VECTOR
			case 0x6: // VSE32
				return RV32V_BC_VSE32;
#endif
			default:
				return RV32I_BC_INVALID;
			}
		}
		case RV32F_FMADD:
			return RV32F_BC_FMADD;
		case RV32F_FMSUB:
		case RV32F_FNMADD:
		case RV32F_FNMSUB:
			return RV32I_BC_FUNCTION;
		case RV32F_FPFUNC:
			switch (instr.fpfunc())
			{
				case 0b00000: // FADD
					return RV32F_BC_FADD;
				case 0b00001: // FSUB
					return RV32F_BC_FSUB;
				case 0b00010: // FMUL
					return RV32F_BC_FMUL;
				case 0b00011: // FDIV
					return RV32F_BC_FDIV;
				default:
					return RV32I_BC_FUNCTION;
				}
#ifdef RISCV_EXT_VECTOR
		case RV32V_OP:
			switch (instr.vwidth())
			{
			case 0x1: {
				const rv32v_instruction vi{instr};
				switch (vi.OPVV.funct6)
				{
					case 0b000000: // VFADD.VV
						return RV32V_BC_VFADD_VV;
				}
				[[fallthrough]];
			}
			default:
				return RV32I_BC_FUNCTION;
			}
#endif
#ifdef RISCV_EXT_ATOMICS
		case RV32A_ATOMIC:
			return RV32I_BC_FUNCTION;
#endif
		default:
			// Unknown instructions can be custom-handled
			return RV32I_BC_FUNCTION;
	}
} // computed_index_for()

	template struct CPU<4>;
	template struct CPU<8>;
	INSTANTIATE_128_IF_ENABLED(CPU);
} // riscv
