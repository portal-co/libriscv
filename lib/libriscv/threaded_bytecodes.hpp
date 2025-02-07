#pragma once

namespace riscv
{
	// Bytecodes for threaded simulation
	enum
	{
		RV32I_BC_INVALID = 0,
		RV32I_BC_ADDI,
		RV32I_BC_LI,
		RV32I_BC_MV,

		RV32I_BC_SLLI,
		RV32I_BC_SLTI,
		RV32I_BC_SLTIU,
		RV32I_BC_XORI,
		RV32I_BC_SRLI,
		RV32I_BC_SRAI,
		RV32I_BC_ORI,
		RV32I_BC_ANDI,

		RV32I_BC_LUI,
		RV32I_BC_AUIPC,

		RV32I_BC_LDB,
		RV32I_BC_LDBU,
		RV32I_BC_LDH,
		RV32I_BC_LDHU,
		RV32I_BC_LDW,
		RV32I_BC_LDWU,
		RV32I_BC_LDD,

		RV32I_BC_STB,
		RV32I_BC_STH,
		RV32I_BC_STW,
		RV32I_BC_STD,

		RV32I_BC_BEQ,
		RV32I_BC_BNE,
		RV32I_BC_BLT,
		RV32I_BC_BGE,
		RV32I_BC_BLTU,
		RV32I_BC_BGEU,
		RV32I_BC_BEQ_FW,
		RV32I_BC_BNE_FW,

		RV32I_BC_JAL,
		RV32I_BC_JALR,
		RV32I_BC_FAST_JAL,
		RV32I_BC_FAST_CALL,

		RV32I_BC_OP_ADD,
		RV32I_BC_OP_SUB,
		RV32I_BC_OP_SLL,
		RV32I_BC_OP_SLT,
		RV32I_BC_OP_SLTU,
		RV32I_BC_OP_XOR,
		RV32I_BC_OP_SRL,
		RV32I_BC_OP_OR,
		RV32I_BC_OP_AND,
		RV32I_BC_OP_MUL,
		RV32I_BC_OP_MULH,
		RV32I_BC_OP_MULHSU,
		RV32I_BC_OP_MULHU,
		RV32I_BC_OP_DIV,
		RV32I_BC_OP_DIVU,
		RV32I_BC_OP_REM,
		RV32I_BC_OP_REMU,
		RV32I_BC_OP_SRA,
		RV32I_BC_OP_SH1ADD,
		RV32I_BC_OP_SH2ADD,
		RV32I_BC_OP_SH3ADD,

		RV64I_BC_ADDIW,

#ifdef RISCV_EXT_COMPRESSED
		RV32C_BC_ADDI,
		RV32C_BC_LI,
		RV32C_BC_MV,
		RV32C_BC_BNEZ,
		RV32C_BC_LDD,
		RV32C_BC_STD,
		RV32C_BC_FUNCTION,
		RV32C_BC_JUMPFUNC,
#endif

		RV32I_BC_SYSCALL,
		RV32I_BC_STOP,
		RV32I_BC_NOP,

		RV32F_BC_FLW,
		RV32F_BC_FLD,
		RV32F_BC_FSW,
		RV32F_BC_FSD,
		RV32F_BC_FADD,
		RV32F_BC_FSUB,
		RV32F_BC_FMUL,
		RV32F_BC_FDIV,
		RV32F_BC_FMADD,
#ifdef RISCV_EXT_VECTOR
		RV32V_BC_VLE32,
		RV32V_BC_VSE32,
		RV32V_BC_VFADD_VV,
#endif
		RV32I_BC_FUNCTION,
#ifdef RISCV_BINARY_TRANSLATION
		RV32I_BC_TRANSLATOR,
#endif
		RV32I_BC_SYSTEM,
		BYTECODES_MAX
	};

	union FasterItype
	{
		uint32_t whole;

		struct
		{
			uint32_t  imm : 16;
			uint32_t  rs2 : 8;
			uint32_t  rs1 : 8;
		};

		RISCV_ALWAYS_INLINE
		auto get_rs1() const noexcept {
			return rs1;
		}
		RISCV_ALWAYS_INLINE
		auto get_rs2() const noexcept {
			return rs2;
		}
		RISCV_ALWAYS_INLINE
		int32_t signed_imm() const noexcept {
			return (int16_t)imm;
		}
		RISCV_ALWAYS_INLINE
		auto unsigned_imm() const noexcept {
			return (uint16_t)imm;
		}
	};

	union FasterOpType
	{
		uint32_t whole;

		struct
		{
			uint32_t rd  : 16;
			uint32_t rs2 : 8;
			uint32_t rs1 : 8;
		};

		RISCV_ALWAYS_INLINE
		auto get_rd() const noexcept {
			return rd;
		}
		RISCV_ALWAYS_INLINE
		auto get_rs1() const noexcept {
			return rs1;
		}
		RISCV_ALWAYS_INLINE
		auto get_rs2() const noexcept {
			return rs2;
		}
	};

	union FasterImmediate
	{
		uint32_t whole;

		struct
		{
			uint8_t  rd;
			uint8_t  zeroes;
			int16_t  imm;
		};

		RISCV_ALWAYS_INLINE
		auto get_rd() const noexcept {
			return whole & 0xFF;
		}

		RISCV_ALWAYS_INLINE
		int32_t signed_imm() const noexcept {
			return imm;
		}
	};

	union FasterMove
	{
		uint32_t whole;

		struct
		{
			uint32_t rd  : 16;
			uint32_t rs1 : 16;
		};

		RISCV_ALWAYS_INLINE
		auto get_rd() const noexcept {
			return rd;
		}
		RISCV_ALWAYS_INLINE
		auto get_rs1() const noexcept {
			return rs1;
		}
	};

	union FasterJtype
	{
		uint32_t whole;

		struct
		{
			int32_t offset : 24;
			int32_t rd     : 8;
		};
	};

} // riscv
