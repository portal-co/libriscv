#include "machine.hpp"
#include "decoder_cache.hpp"
#include "instruction_counter.hpp"
#include "threaded_bytecodes.hpp"
#include "rv32i_instr.hpp"
#include "rvfd.hpp"
#ifdef RISCV_EXT_COMPRESSED
#include "rvc.hpp"
#endif
#ifdef RISCV_EXT_VECTOR
#include "rvv.hpp"
#endif

namespace riscv
{
	static constexpr bool VERBOSE_JUMPS = false;
#define VIEW_INSTR() \
	auto instr = *(rv32i_instruction *)&decoder->instr;
#define VIEW_INSTR_AS(name, x) \
	auto &&name = *(x *)&decoder->instr;
#define NEXT_INSTR()                  \
	if constexpr (compressed_enabled) \
		decoder += 2;                 \
	else                              \
		decoder += 1;                 \
	EXECUTE_INSTR();
#define NEXT_C_INSTR() \
	decoder += 1;      \
	EXECUTE_INSTR();

#define NEXT_BLOCK(len)               \
	pc += len;                        \
	if constexpr (compressed_enabled) \
		decoder += len / 2;           \
	else                              \
		decoder += 1;                 \
	pc += decoder->block_bytes(); \
	counter.increment_counter(decoder->instruction_count()); \
	EXECUTE_INSTR();

#define NEXT_SEGMENT() \
	decoder = &exec_decoder[pc / DecoderCache<W>::DIVISOR]; \
	pc += decoder->block_bytes(); \
	counter.increment_counter(decoder->instruction_count()); \
	EXECUTE_INSTR();

#define PERFORM_BRANCH()                \
	if constexpr (VERBOSE_JUMPS) printf("Branch 0x%lX >= 0x%lX\n", pc, pc + fi.signed_imm()); \
	pc += fi.signed_imm();              \
	if (LIKELY(!counter.overflowed())) { \
		decoder += fi.signed_imm() / (compressed_enabled ? 2 : 4); \
		counter.increment_counter(decoder->instruction_count()); \
		pc += decoder->block_bytes(); \
		EXECUTE_INSTR(); \
	} \
	goto check_jump;

#define PERFORM_FORWARD_BRANCH()        \
	if constexpr (VERBOSE_JUMPS) printf("Fw.Branch 0x%lX >= 0x%lX\n", pc, pc + fi.signed_imm()); \
	pc += fi.signed_imm();              \
	NEXT_SEGMENT();

template <int W> DISPATCH_ATTR
void CPU<W>::DISPATCH_FUNC(uint64_t imax)
{
	static constexpr uint32_t XLEN = W * 8;
	using addr_t  = address_type<W>;
	using saddr_t = signed_address_type<W>;

#ifdef DISPATCH_MODE_THREADED
	static constexpr void *computed_opcode[] = {
		[RV32I_BC_INVALID] = &&execute_invalid,
		[RV32I_BC_ADDI]    = &&rv32i_addi,
		[RV32I_BC_LI]      = &&rv32i_li,
		[RV32I_BC_MV]      = &&rv32i_mv,
		[RV32I_BC_SLLI]    = &&rv32i_slli,
		[RV32I_BC_SLTI]    = &&rv32i_slti,
		[RV32I_BC_SLTIU]   = &&rv32i_sltiu,
		[RV32I_BC_XORI]    = &&rv32i_xori,
		[RV32I_BC_SRLI]    = &&rv32i_srli,
		[RV32I_BC_SRAI]    = &&rv32i_srai,
		[RV32I_BC_ORI]     = &&rv32i_ori,
		[RV32I_BC_ANDI]    = &&rv32i_andi,

		[RV32I_BC_LUI]     = &&rv32i_lui,
		[RV32I_BC_AUIPC]   = &&rv32i_auipc,

		[RV32I_BC_LDB]     = &&rv32i_ldb,
		[RV32I_BC_LDBU]    = &&rv32i_ldbu,
		[RV32I_BC_LDH]     = &&rv32i_ldh,
		[RV32I_BC_LDHU]    = &&rv32i_ldhu,
		[RV32I_BC_LDW]     = &&rv32i_ldw,
		[RV32I_BC_LDWU]    = &&rv32i_ldwu,
		[RV32I_BC_LDD]     = &&rv32i_ldd,

		[RV32I_BC_STB]     = &&rv32i_stb,
		[RV32I_BC_STH]     = &&rv32i_sth,
		[RV32I_BC_STW]     = &&rv32i_stw,
		[RV32I_BC_STD]     = &&rv32i_std,

		[RV32I_BC_BEQ]     = &&rv32i_beq,
		[RV32I_BC_BNE]     = &&rv32i_bne,
		[RV32I_BC_BLT]     = &&rv32i_blt,
		[RV32I_BC_BGE]     = &&rv32i_bge,
		[RV32I_BC_BLTU]    = &&rv32i_bltu,
		[RV32I_BC_BGEU]    = &&rv32i_bgeu,
		[RV32I_BC_BEQ_FW]  = &&rv32i_beq_fw,
		[RV32I_BC_BNE_FW]  = &&rv32i_bne_fw,

		[RV32I_BC_JAL]     = &&rv32i_jal,
		[RV32I_BC_JALR]    = &&rv32i_jalr,
		[RV32I_BC_FAST_JAL] = &&rv32i_fast_jal,
		[RV32I_BC_FAST_CALL] = &&rv32i_fast_call,

		[RV32I_BC_OP_ADD]  = &&rv32i_op_add,
		[RV32I_BC_OP_SUB]  = &&rv32i_op_sub,
		[RV32I_BC_OP_SLL]  = &&rv32i_op_sll,
		[RV32I_BC_OP_SLT]  = &&rv32i_op_slt,
		[RV32I_BC_OP_SLTU] = &&rv32i_op_sltu,
		[RV32I_BC_OP_XOR]  = &&rv32i_op_xor,
		[RV32I_BC_OP_SRL]  = &&rv32i_op_srl,
		[RV32I_BC_OP_OR]   = &&rv32i_op_or,
		[RV32I_BC_OP_AND]  = &&rv32i_op_and,
		[RV32I_BC_OP_MUL]  = &&rv32i_op_mul,
		[RV32I_BC_OP_MULH] = &&rv32i_op_mulh,
		[RV32I_BC_OP_MULHSU] = &&rv32i_op_mulhsu,
		[RV32I_BC_OP_MULHU]= &&rv32i_op_mulhu,
		[RV32I_BC_OP_DIV]  = &&rv32i_op_div,
		[RV32I_BC_OP_DIVU] = &&rv32i_op_divu,
		[RV32I_BC_OP_REM]  = &&rv32i_op_rem,
		[RV32I_BC_OP_REMU] = &&rv32i_op_remu,
		[RV32I_BC_OP_SRA]  = &&rv32i_op_sra,
		[RV32I_BC_OP_SH1ADD] = &&rv32i_op_sh1add,
		[RV32I_BC_OP_SH2ADD] = &&rv32i_op_sh2add,
		[RV32I_BC_OP_SH3ADD] = &&rv32i_op_sh3add,

		[RV64I_BC_ADDIW] = &&rv64i_addiw,

#ifdef RISCV_EXT_COMPRESSED
		[RV32C_BC_ADDI]     = &&rv32c_addi,
		[RV32C_BC_LI]       = &&rv32c_addi,
		[RV32C_BC_MV]       = &&rv32c_mv,
		[RV32C_BC_BNEZ]     = &&rv32c_bnez,
		[RV32C_BC_LDD]      = &&rv32c_ldd,
		[RV32C_BC_STD]      = &&rv32c_std,
		[RV32C_BC_FUNCTION] = &&rv32c_func,
		[RV32C_BC_JUMPFUNC] = &&rv32c_jfunc,
#endif

		[RV32I_BC_SYSCALL] = &&rv32i_syscall,
		[RV32I_BC_STOP]    = &&rv32i_stop,
		[RV32I_BC_NOP]     = &&rv32i_nop,

		[RV32F_BC_FLW]     = &&rv32i_flw,
		[RV32F_BC_FLD]     = &&rv32i_fld,
		[RV32F_BC_FSW]     = &&rv32i_fsw,
		[RV32F_BC_FSD]     = &&rv32i_fsd,
		[RV32F_BC_FADD]    = &&rv32f_fadd,
		[RV32F_BC_FSUB]    = &&rv32f_fsub,
		[RV32F_BC_FMUL]    = &&rv32f_fmul,
		[RV32F_BC_FDIV]    = &&rv32f_fdiv,
		[RV32F_BC_FMADD]   = &&rv32f_fmadd,
#ifdef RISCV_EXT_VECTOR
		[RV32V_BC_VLE32]   = &&rv32v_vle32,
		[RV32V_BC_VSE32]   = &&rv32v_vse32,
		[RV32V_BC_VFADD_VV] = &&rv32v_vfadd_vv,
#endif
		[RV32I_BC_FUNCTION] = &&execute_decoded_function,
#ifdef RISCV_BINARY_TRANSLATION
		[RV32I_BC_TRANSLATOR] = &&translated_function,
#endif
		[RV32I_BC_SYSTEM]  = &&rv32i_system,
	};
#endif

	// We need an execute segment matching current PC
	if (UNLIKELY(!is_executable(this->pc())))
	{
		this->next_execute_segment();
	}

	// Calculate the instruction limit
	if (imax != UINT64_MAX)
		machine().set_max_instructions(machine().instruction_counter() + imax);
	else
		machine().set_max_instructions(UINT64_MAX);

	InstrCounter counter{machine()};

	DecodedExecuteSegment<W>* exec = this->m_exec;
	DecoderData<W>* exec_decoder = exec->decoder_cache();
	address_t current_begin = exec->exec_begin();
	address_t current_end = exec->exec_end();
	address_t pc = this->pc();

continue_segment:
	DecoderData<W>* decoder = &exec_decoder[pc / DecoderCache<W>::DIVISOR];
	pc += decoder->block_bytes();
	counter.increment_counter(decoder->instruction_count());

#ifdef DISPATCH_MODE_SWITCH_BASED

while (true) {
	switch (decoder->get_bytecode()) {
	#define INSTRUCTION(bc, lbl) case bc:

#else

	goto *computed_opcode[decoder->get_bytecode()];
	#define INSTRUCTION(bc, lbl) lbl:

#endif

#define CPU()       (*this)
#define REG(x)      registers().get()[x]
#define REGISTERS() registers()
#define MACHINE()   machine()

	/** Instruction handlers **/

#define BYTECODES_OP_IMM
#  include "bytecode_impl.cpp"
#undef BYTECODES_OP_IMM

#define BYTECODES_LOAD_STORE
#  include "bytecode_impl.cpp"
#undef BYTECODES_LOAD_STORE

#define BYTECODES_BRANCH
#  include "bytecode_impl.cpp"
#undef BYTECODES_BRANCH

INSTRUCTION(RV32I_BC_FAST_JAL, rv32i_fast_jal) {
	VIEW_INSTR();
	pc = instr.whole;
	if constexpr (VERBOSE_JUMPS) {
		printf("FAST_JAL PC 0x%lX => 0x%lX\n", pc, pc + instr.whole);
	}
	if (UNLIKELY(counter.overflowed()))
		goto check_jump;
	NEXT_SEGMENT();
}
INSTRUCTION(RV32I_BC_FAST_CALL, rv32i_fast_call) {
	VIEW_INSTR();
	reg(REG_RA) = pc + 4;
	pc = instr.whole;
	if constexpr (VERBOSE_JUMPS) {
		printf("FAST_CALL PC 0x%lX => 0x%lX\n", pc, pc + instr.whole);
	}
	if (UNLIKELY(counter.overflowed()))
		goto check_jump;
	NEXT_SEGMENT();
}
INSTRUCTION(RV32I_BC_JALR, rv32i_jalr) {
	VIEW_INSTR();
	// jump to register + immediate
	// NOTE: if rs1 == rd, avoid clobber by storing address first
	const auto address = reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	// Link *next* instruction (rd = PC + 4)
	if (instr.Itype.rd != 0) {
		reg(instr.Itype.rd) = pc + 4;
	}
	if constexpr (VERBOSE_JUMPS) {
		printf("JALR PC 0x%lX => 0x%lX\n", pc, address);
	}
	pc = address;
	goto check_unaligned_jump;
}

#ifdef RISCV_EXT_COMPRESSED
INSTRUCTION(RV32C_BC_FUNCTION, rv32c_func) {
	VIEW_INSTR();
	auto handler = decoder->get_handler();
	handler(*this, instr);
	NEXT_C_INSTR();
}
INSTRUCTION(RV32C_BC_JUMPFUNC, rv32c_jfunc) {
	VIEW_INSTR();
	registers().pc = pc;
	auto handler = decoder->get_handler();
	handler(*this, instr);
	if constexpr (VERBOSE_JUMPS) {
		printf("Compressed jump from 0x%lX to 0x%lX\n",
			pc, registers().pc + 2);
	}
	pc = registers().pc + 2;
	goto check_unaligned_jump;
}
#endif

#define BYTECODES_OP
#  include "bytecode_impl.cpp"
#undef BYTECODES_OP

INSTRUCTION(RV32I_BC_SYSCALL, rv32i_syscall) {
	// Make the current PC visible
	this->registers().pc = pc;
	// Make the instruction counter(s) visible
	counter.apply_counter();
	// Invoke system call
	machine().system_call(this->reg(REG_ECALL));
	// Restore max counter
	counter.retrieve_max_counter();
	if (UNLIKELY(counter.overflowed() || pc != this->registers().pc))
	{
		// System calls are always full-length instructions
		pc = registers().pc + 4;
		goto check_jump;
	}
	NEXT_BLOCK(4);
}

#define BYTECODES_FLP
#  include "bytecode_impl.cpp"
#undef BYTECODES_FLP

/** UNLIKELY INSTRUCTIONS **/
/** UNLIKELY INSTRUCTIONS **/

INSTRUCTION(RV32I_BC_FUNCTION, execute_decoded_function) {
	VIEW_INSTR();
	auto handler = decoder->get_handler();
	handler(*this, instr);
	NEXT_INSTR();
}
INSTRUCTION(RV32I_BC_STOP, rv32i_stop) {
	REGISTERS().pc = pc + 4;
	counter.stop();
	return;
}

INSTRUCTION(RV32I_BC_JAL, rv32i_jal) {
	VIEW_INSTR_AS(fi, FasterJtype);
	if (fi.rd != 0)
		reg(fi.rd) = pc + 4;
	if constexpr (VERBOSE_JUMPS) {
		printf("JAL PC 0x%lX => 0x%lX\n", pc, pc+fi.offset);
	}
	pc += fi.offset;
	goto check_jump;
}

/** UNLIKELY INSTRUCTIONS **/
/** UNLIKELY INSTRUCTIONS **/

#ifdef RISCV_BINARY_TRANSLATION
INSTRUCTION(RV32I_BC_TRANSLATOR, translated_function) {
	VIEW_INSTR();
	// Make the current PC visible
	this->registers().pc = pc;
	// Make the instruction counter visible
	counter.apply_counter();
	// Invoke translated code
	auto handler = decoder->get_handler();
	handler(*this, instr);
	// Restore counter
	counter.retrieve();
	// Translations are always full-length instructions (?)
	pc = registers().pc + 4;
	goto check_jump;
}
#endif

INSTRUCTION(RV32I_BC_SYSTEM, rv32i_system) {
	VIEW_INSTR();
	// Make the current PC visible
	this->registers().pc = pc;
	// Make the instruction counter visible
	counter.apply_counter();
	// Invoke SYSTEM
	machine().system(instr);
	// Restore PC in case it changed (supervisor)
	pc = registers().pc + 4;
	goto check_jump;
}

#ifdef DISPATCH_MODE_SWITCH_BASED
	default:
		goto execute_invalid;
	} // switch case
} // while loop

#endif

execute_invalid:
	this->trigger_exception(ILLEGAL_OPCODE, decoder->instr);

check_unaligned_jump:
	if constexpr (!compressed_enabled) {
		if (UNLIKELY(pc & 0x3)) {
			registers().pc = pc;
			trigger_exception(MISALIGNED_INSTRUCTION, this->pc());
		}
	} else {
		if (UNLIKELY(pc & 0x1)) {
			registers().pc = pc;
			trigger_exception(MISALIGNED_INSTRUCTION, this->pc());
		}
	}
check_jump: {
	if (UNLIKELY(counter.overflowed())) {
		registers().pc = pc;
		return;
	}
	if (UNLIKELY(!(pc >= current_begin && pc < current_end)))
	{
		// We have to store and restore PC here as there are
		// custom callbacks when changing segments that can
		// jump around.
		registers().pc = pc;
		// Change to a new execute segment
		exec = this->next_execute_segment();
		exec_decoder = exec->decoder_cache();
		current_begin = exec->exec_begin();
		current_end = exec->exec_end();
		pc = registers().pc;
	}
	goto continue_segment;
}

} // CPU::simulate_XXX()

} // riscv
