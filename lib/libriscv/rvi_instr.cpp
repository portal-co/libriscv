#include "instr_helpers.hpp"
#include "rvc.hpp"
#include <atomic>
#include <inttypes.h>

namespace riscv
{
	INSTRUCTION(NOP,
	[] (auto& /* cpu */, rv32i_instruction /* instr */) RVINSTR_COLDATTR {
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction) RVPRINTR_ATTR {
		return snprintf(buffer, len, "NOP");
	});

	INSTRUCTION(UNIMPLEMENTED,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_COLDATTR {
		if (instr.length() == 4)
			cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION, instr.whole);
		else
			cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION, instr.half[0]);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		if (instr.length() == 4) {
			return snprintf(buffer, len, "UNIMPLEMENTED: 4-byte 0x%X (0x%X)",
							instr.opcode(), instr.whole);
		} else {
			return snprintf(buffer, len, "UNIMPLEMENTED: 2-byte %#hx F%#hx (%#hx)",
							rv32c_instruction { instr }.opcode(),
							rv32c_instruction { instr }.funct3(),
							instr.half[0]);
		}
	});

	INSTRUCTION(ILLEGAL,
	[] (auto& cpu, rv32i_instruction /* instr */) RVINSTR_COLDATTR {
		cpu.trigger_exception(ILLEGAL_OPCODE);
	}, DECODED_INSTR(UNIMPLEMENTED).printer);

	INSTRUCTION(LOAD_I8,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (int8_t) cpu.machine().memory.template read<uint8_t>(addr);
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		static std::array<const char*, 8> f3 = {"LD.B", "LD.H", "LD.W", "LD.D", "LD.BU", "LD.HU", "LD.WU", "LD.Q"};
		return snprintf(buffer, len, "%s %s, [%s%+" PRId32 " = 0x%" PRIX64 "]",
						f3[instr.Itype.funct3], RISCV::regname(instr.Itype.rd),
						RISCV::regname(instr.Itype.rs1), instr.Itype.signed_imm(),
						uint64_t(cpu.reg(instr.Itype.rs1) + instr.Itype.signed_imm()));
	});

	INSTRUCTION(LOAD_I16,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (int16_t) cpu.machine().memory.template read<uint16_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_I32,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (int32_t) cpu.machine().memory.template read<uint32_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_I64,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (int64_t) cpu.machine().memory.template read<uint64_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_U8,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (RVSIGNTYPE(cpu)) cpu.machine().memory.template read<uint8_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_U16,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (RVSIGNTYPE(cpu)) cpu.machine().memory.template read<uint16_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_U32,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (RVSIGNTYPE(cpu)) cpu.machine().memory.template read<uint32_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_U64,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& reg = cpu.reg(instr.Itype.rd);
		const auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		reg = (RVSIGNTYPE(cpu)) cpu.machine().memory.template read<uint64_t>(addr);
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(LOAD_X_DUMMY,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_COLDATTR
	{
		auto addr = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		switch (instr.Itype.funct3) {
		case 0x0:
			cpu.machine().memory.template read<uint8_t>(addr);
			return;
		case 0x1:
			cpu.machine().memory.template read<uint16_t>(addr);
			return;
		case 0x2:
			cpu.machine().memory.template read<uint32_t>(addr);
			return;
		case 0x3:
			if constexpr (RVISGE64BIT(cpu)) {
				cpu.machine().memory.template read<uint64_t>(addr);
				return;
			}
			cpu.trigger_exception(ILLEGAL_OPCODE);
		case 0x7:
#ifdef RISCV_128BIT_ISA
			if constexpr (RVIS128BIT(cpu)) {
				addr &= ~(__uint128_t)0xF;
				cpu.machine().memory.template read<__uint128_t>(addr);
				return;
			}
#endif
			cpu.trigger_exception(ILLEGAL_OPCODE);
		}
	}, DECODED_INSTR(LOAD_I8).printer);

	INSTRUCTION(STORE_I8_IMM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		const auto& value = cpu.reg(instr.Stype.rs2);
		const auto addr  = cpu.reg(instr.Stype.rs1) + RVIMM(cpu, instr.Stype);
		cpu.machine().memory.template write<uint8_t>(addr, value);
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		static std::array<const char*, 8> f3 = {"ST.B", "ST.H", "ST.W", "ST.D", "ST.Q", "???", "???", "???"};
		return snprintf(buffer, len, "%s %s, [%s%+d] (0x%" PRIX64 ")",
						f3[instr.Stype.funct3], RISCV::regname(instr.Stype.rs2),
						RISCV::regname(instr.Stype.rs1), instr.Stype.signed_imm(),
						uint64_t(cpu.reg(instr.Stype.rs1) + instr.Stype.signed_imm()));
	});

	INSTRUCTION(STORE_I8,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		const auto& addr  = cpu.reg(instr.Stype.rs1);
		const auto& value = cpu.reg(instr.Stype.rs2);
		cpu.machine().memory.template write<uint8_t>(addr, value);
	}, DECODED_INSTR(STORE_I8_IMM).printer);

	INSTRUCTION(STORE_I16_IMM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		const auto& value = cpu.reg(instr.Stype.rs2);
		const auto addr  = cpu.reg(instr.Stype.rs1) + RVIMM(cpu, instr.Stype);
		cpu.machine().memory.template write<uint16_t>(addr, value);
	}, DECODED_INSTR(STORE_I8_IMM).printer);

	INSTRUCTION(STORE_I32_IMM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		const auto& value = cpu.reg(instr.Stype.rs2);
		const auto addr  = cpu.reg(instr.Stype.rs1) + RVIMM(cpu, instr.Stype);
		cpu.machine().memory.template write<uint32_t>(addr, value);
	}, DECODED_INSTR(STORE_I8_IMM).printer);

	INSTRUCTION(STORE_I64_IMM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		const auto& value = cpu.reg(instr.Stype.rs2);
		const auto addr  = cpu.reg(instr.Stype.rs1) + RVIMM(cpu, instr.Stype);
		cpu.machine().memory.template write<uint64_t>(addr, value);
	}, DECODED_INSTR(STORE_I8_IMM).printer);

	INSTRUCTION(STORE_I128_IMM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		const auto& value = cpu.reg(instr.Stype.rs2);
		auto addr = cpu.reg(instr.Stype.rs1) + RVIMM(cpu, instr.Stype);
		addr &= ~(__uint128_t)0xF;
		cpu.machine().memory.template write<__uint128_t>(addr, value);
	}, DECODED_INSTR(STORE_I8_IMM).printer);

#define VERBOSE_BRANCH() \
	if constexpr (verbose_branches_enabled) { \
		printf(">>> BRANCH jump to 0x%" PRIX64 "\n", uint64_t(cpu.pc() + 4)); \
	}

	INSTRUCTION(BRANCH_EQ,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		const auto reg1 = cpu.reg(instr.Btype.rs1);
		const auto reg2 = cpu.reg(instr.Btype.rs2);
		if (reg1 == reg2) {
			cpu.jump(cpu.pc() + RVIMM(cpu, instr.Btype) - 4);
			VERBOSE_BRANCH()
		}
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		// BRANCH compares two registers, BQE = equal taken, BNE = notequal taken
		static std::array<const char*, 8> f3 = {"BEQ", "BNE", "???", "???", "BLT", "BGE", "BLTU", "BGEU"};
		static std::array<const char*, 8> f1z = {"BEQ", "BNE", "???", "???", "BGTZ", "BLEZ", "BLTU", "BGEU"};
		static std::array<const char*, 8> f2z = {"BEQZ", "BNEZ", "???", "???", "BLTZ", "BGEZ", "BLTU", "BGEU"};
		if (instr.Btype.rs1 != 0 && instr.Btype.rs2) {
			return snprintf(buffer, len, "%s %s (0x%" PRIX64 "), %s (0x%" PRIX64 ") => PC%+d (0x%" PRIX64 ")",
							f3[instr.Btype.funct3],
							RISCV::regname(instr.Btype.rs1), uint64_t(cpu.reg(instr.Btype.rs1)),
							RISCV::regname(instr.Btype.rs2), uint64_t(cpu.reg(instr.Btype.rs2)),
							instr.Btype.signed_imm(),
							uint64_t(cpu.pc() + instr.Btype.signed_imm()));
		} else {
			auto& array = (instr.Btype.rs1) ? f2z : f1z;
			auto  reg   = (instr.Btype.rs1) ? instr.Btype.rs1 : instr.Btype.rs2;
			return snprintf(buffer, len, "%s %s (0x%" PRIX64 ") => PC%+d (0x%" PRIX64 ")",
							array[instr.Btype.funct3],
							RISCV::regname(reg), uint64_t(cpu.reg(reg)),
							instr.Btype.signed_imm(),
							uint64_t(cpu.pc() + instr.Btype.signed_imm()));
		}
	});

	INSTRUCTION(BRANCH_NE,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		const auto reg1 = cpu.reg(instr.Btype.rs1);
		const auto reg2 = cpu.reg(instr.Btype.rs2);
		if (reg1 != reg2) {
			cpu.jump(cpu.pc() + RVIMM(cpu, instr.Btype) - 4);
			VERBOSE_BRANCH()
		}
	}, DECODED_INSTR(BRANCH_EQ).printer);

	INSTRUCTION(BRANCH_LT,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		const auto reg1 = cpu.reg(instr.Btype.rs1);
		const auto reg2 = cpu.reg(instr.Btype.rs2);
		if (RVTOSIGNED(reg1) < RVTOSIGNED(reg2)) {
			cpu.jump(cpu.pc() + RVIMM(cpu, instr.Btype) - 4);
			VERBOSE_BRANCH()
		}
	}, DECODED_INSTR(BRANCH_EQ).printer);

	INSTRUCTION(BRANCH_GE,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		const auto reg1 = cpu.reg(instr.Btype.rs1);
		const auto reg2 = cpu.reg(instr.Btype.rs2);
		if (RVTOSIGNED(reg1) >= RVTOSIGNED(reg2)) {
			cpu.jump(cpu.pc() + RVIMM(cpu, instr.Btype) - 4);
			VERBOSE_BRANCH()
		}
	}, DECODED_INSTR(BRANCH_EQ).printer);

	INSTRUCTION(BRANCH_LTU,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		const auto& reg1 = cpu.reg(instr.Btype.rs1);
		const auto& reg2 = cpu.reg(instr.Btype.rs2);
		if (reg1 < reg2) {
			cpu.jump(cpu.pc() + RVIMM(cpu, instr.Btype) - 4);
			VERBOSE_BRANCH()
		}
	}, DECODED_INSTR(BRANCH_EQ).printer);

	INSTRUCTION(BRANCH_GEU,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		const auto& reg1 = cpu.reg(instr.Btype.rs1);
		const auto& reg2 = cpu.reg(instr.Btype.rs2);
		if (reg1 >= reg2) {
			cpu.jump(cpu.pc() + RVIMM(cpu, instr.Btype) - 4);
			VERBOSE_BRANCH()
		}
	}, DECODED_INSTR(BRANCH_EQ).printer);

	INSTRUCTION(JALR,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		// jump to register + immediate
		// NOTE: if rs1 == rd, avoid clobber by storing address first
		const auto address = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		// Link *next* instruction (rd = PC + 4)
		if (LIKELY(instr.Itype.rd != 0)) {
			cpu.reg(instr.Itype.rd) = cpu.pc() + 4;
		}
		cpu.jump(address - 4);
		if constexpr (verbose_branches_enabled) {
		printf(">>> JMP 0x%" PRIX64 " <-- %s = 0x%" PRIX64 "%+d\n",
				uint64_t(address),
				RISCV::regname(instr.Itype.rs1),
				uint64_t(cpu.reg(instr.Itype.rs1)),
				instr.Itype.signed_imm());
		}
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		// RISC-V's RET instruction: return to register + immediate
		const char* variant = (instr.Itype.rs1 == REG_RA) ? "RET" : "JMP";
		const auto address = cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
		return snprintf(buffer, len, "%s %s%+d (0x%" PRIX64 ")", variant,
						RISCV::regname(instr.Itype.rs1),
						instr.Itype.signed_imm(), uint64_t(address));
	});

	INSTRUCTION(JAL,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		// Link *next* instruction (rd = PC + 4)
		cpu.reg(instr.Jtype.rd) = cpu.pc() + 4;
		// And jump relative
		cpu.jump(cpu.pc() + instr.Jtype.jump_offset() - 4);
		if constexpr (verbose_branches_enabled) {
			printf(">>> CALL 0x%" PRIX64 " <-- %s = 0x%" PRIX64 "\n",
					uint64_t(cpu.pc()),
					RISCV::regname(instr.Jtype.rd),
					uint64_t(cpu.reg(instr.Jtype.rd)));
		}
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		if (instr.Jtype.rd != 0) {
		return snprintf(buffer, len, "JAL %s, PC%+d (0x%" PRIX64 ")",
						RISCV::regname(instr.Jtype.rd), instr.Jtype.jump_offset(),
						uint64_t(cpu.pc() + instr.Jtype.jump_offset()));
		}
		return snprintf(buffer, len, "JMP PC%+d (0x%" PRIX64 ")",
						instr.Jtype.jump_offset(),
						uint64_t(cpu.pc() + instr.Jtype.jump_offset()));
	});

	INSTRUCTION(JMPI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		// Jump relative
		cpu.jump(cpu.pc() + instr.Jtype.jump_offset() - 4);
		if constexpr (verbose_branches_enabled) {
			printf(">>> JMP 0x%" PRIX64 " <-- %s = 0x%" PRIX64 "\n",
					uint64_t(cpu.pc()),
					RISCV::regname(instr.Jtype.rd),
					uint64_t(cpu.reg(instr.Jtype.rd)));
		}
	}, DECODED_INSTR(JAL).printer);

	INSTRUCTION(OP_IMM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& dst = cpu.reg(instr.Itype.rd);
		const auto src = cpu.reg(instr.Itype.rs1);
		switch (instr.Itype.funct3) {
		case 0x1: // *NOT* SLLI, SEXT.B, SEXT.H
			switch (instr.Itype.imm) {
			case 0b011000000100: // SEXT.B
				dst = RVSIGNTYPE(cpu)(int8_t(src));
				return;
			case 0b011000000101: // SEXT.H
				dst = RVSIGNTYPE(cpu)(int16_t(src));
				return;
			}
			break;
		case 0x2: // SLTI: Set less than immediate
			dst = (RVTOSIGNED(src) < RVIMM(cpu, instr.Itype));
			return;
		case 0x3: // SLTIU: Sign-extend, then treat as unsigned
			dst = (src < (RVREGTYPE(cpu)) RVIMM(cpu, instr.Itype));
			return;
		case 0x4: // XORI:
			dst = src ^ RVIMM(cpu, instr.Itype);
			return;
		case 0x5: // SRLI / SRAI / ORC.B
			if (instr.Itype.is_srai()) {
				// SRAI: Preserve the sign bit
				dst = (RVSIGNTYPE(cpu))src >> (instr.Itype.imm & (RVXLEN(cpu)-1));
				return;
			}
			else if (instr.Itype.is_rori()) {
				// RORI: Rotate right
				const auto shift = instr.Itype.imm & (RVXLEN(cpu) - 1);
				dst = (src >> shift) | (src << (RVXLEN(cpu) - shift));
				return;
			}
			else if (instr.Itype.imm == 0x287) {
				// ORC.B: Bitwise OR-combine
				auto* bytes = (uint8_t *)&src;
				for (size_t i = 0; i < sizeof(src); i++)
					dst = bytes[i] ? 0xFF : 0x0;
				return;
			}
			break;
		case 0x6: // ORI: Or sign-extended 12-bit immediate
			dst = src | RVIMM(cpu, instr.Itype);
			return;
		}
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR
	{
		if (instr.Itype.imm == 0)
		{
			// this is the official NOP instruction (ADDI x0, x0, 0)
			if (instr.Itype.rd == 0 && instr.Itype.rs1 == 0) {
				return snprintf(buffer, len, "NOP");
			}
			static std::array<const char*, 8> func3 = {"MV", "SLL", "SLT", "SLT", "XOR", "SRL", "OR", "AND"};
			return snprintf(buffer, len, "%s %s, %s (= 0x%" PRIx64 ")",
							func3[instr.Itype.funct3],
							RISCV::regname(instr.Itype.rd),
							RISCV::regname(instr.Itype.rs1),
							uint64_t(cpu.reg(instr.Itype.rs1)));
		}
		else if (instr.Itype.rs1 != 0 && instr.Itype.funct3 == 1) {
			const auto shift = (RVIS64BIT(cpu)) ? instr.Itype.shift64_imm() : instr.Itype.shift_imm();
			return snprintf(buffer, len, "SLLI %s, %s << %u (0x%" PRIX64 ")",
							RISCV::regname(instr.Itype.rd),
							RISCV::regname(instr.Itype.rs1),
							shift,
							uint64_t(cpu.reg(instr.Itype.rs1) << shift));
		} else if (instr.Itype.rs1 != 0 && instr.Itype.funct3 == 5) {
			const auto shift = (RVIS64BIT(cpu)) ? instr.Itype.shift64_imm() : instr.Itype.shift_imm();
			return snprintf(buffer, len, "%s %s, %s >> %u (0x%" PRIX64 ")",
							(instr.Itype.is_srai() ? "SRAI" : "SRLI"),
							RISCV::regname(instr.Itype.rd),
							RISCV::regname(instr.Itype.rs1),
							shift,
							uint64_t(cpu.reg(instr.Itype.rs1) >> shift));
		} else if (instr.Itype.rs1 != 0) {
			static std::array<const char*, 8> func3 = {"ADDI", "SLLI", "SLTI", "SLTU", "XORI", "SRLI", "ORI", "ANDI"};
			if (!(instr.Itype.funct3 == 4 && instr.Itype.signed_imm() == -1)) {
				return snprintf(buffer, len, "%s %s, %s%+d (0x%" PRIX64 ")",
								func3[instr.Itype.funct3],
								RISCV::regname(instr.Itype.rd),
								RISCV::regname(instr.Itype.rs1),
								instr.Itype.signed_imm(),
								uint64_t(cpu.reg(instr.Itype.rs1)));
			} else {
				return snprintf(buffer, len, "NOT %s, %s",
								RISCV::regname(instr.Itype.rd),
								RISCV::regname(instr.Itype.rs1));
			}
		}
		static std::array<const char*, 8> func3 = {"LINT", "SLLI", "SLTI", "SLTU", "XORI", "SRLI", "ORI", "ANDI"};
		return snprintf(buffer, len, "%s %s, %d (0x%X)",
						func3[instr.Itype.funct3],
						RISCV::regname(instr.Itype.rd),
						instr.Itype.signed_imm(), instr.Itype.signed_imm());
	});

	INSTRUCTION(OP_IMM_ADDI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		// ADDI: Add sign-extended 12-bit immediate
		cpu.reg(instr.Itype.rd) =
			cpu.reg(instr.Itype.rs1) + RVIMM(cpu, instr.Itype);
	}, DECODED_INSTR(OP_IMM).printer);

	INSTRUCTION(OP_IMM_LI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		// LI: Load sign-extended 12-bit immediate
		cpu.reg(instr.Itype.rd) = (RVSIGNTYPE(cpu)) RVIMM(cpu, instr.Itype);
	}, DECODED_INSTR(OP_IMM).printer);

	INSTRUCTION(OP_MV,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		cpu.reg(instr.Itype.rd) = cpu.reg(instr.Itype.rs1);
	}, DECODED_INSTR(OP_IMM).printer);

	INSTRUCTION(OP_IMM_SLLI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const auto src = cpu.reg(instr.Itype.rs1);
		// SLLI: Logical left-shift 5/6/7-bit immediate
		dst = src << (instr.Itype.imm & (RVXLEN(cpu)-1));
	}, DECODED_INSTR(OP_IMM).printer);

	INSTRUCTION(OP_IMM_SRLI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const auto src = cpu.reg(instr.Itype.rs1);
		// SRLI: Shift-right logical 5/6/7-bit immediate
		dst = src >> (instr.Itype.imm & (RVXLEN(cpu)-1));
	}, DECODED_INSTR(OP_IMM).printer);

	INSTRUCTION(OP_IMM_ANDI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		// ANDI: And sign-extended 12-bit immediate
		dst = cpu.reg(instr.Itype.rs1) & RVIMM(cpu, instr.Itype);
	}, DECODED_INSTR(OP_IMM).printer);

	INSTRUCTION(OP,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR
	{
		auto& dst = cpu.reg(instr.Rtype.rd);
		const auto src1 = cpu.reg(instr.Rtype.rs1);
		const auto src2 = cpu.reg(instr.Rtype.rs2);

		switch (instr.Rtype.jumptable_friendly_op()) {
		case 0x1: // SLL
			dst = src1 << (src2 & (RVXLEN(cpu)-1));
			return;
		case 0x2: // SLT
			dst = (RVTOSIGNED(src1) < RVTOSIGNED(src2));
			return;
		case 0x3: // SLTU
			dst = (src1 < src2);
			return;
		case 0x4: // XOR
			if (instr.Rtype.funct7 == 0x0) {
				dst = src1 ^ src2;
				return;
			} else if (instr.Rtype.funct7 == 0x4) {
				dst = uint16_t(src1);
				return;
			}
			break;
		case 0x5: // SRL / ROR
			if (instr.Itype.high_bits() == 0x0) {
				// SRL: Logical right shift
				dst = src1 >> (src2 & (RVXLEN(cpu)-1));
				return;
			}
			else if (instr.Itype.is_rori()) {
				// ROR: Rotate right
				const auto shift = src2 & (RVXLEN(cpu) - 1);
				dst = (src1 >> shift) | (src1 << (RVXLEN(cpu) - shift));
				return;
			}
			break;
		case 0x6: // OR
			dst = src1 | src2;
			return;
		case 0x7: // AND
			dst = src1 & src2;
			return;
		// extension RV32M / RV64M
		case 0x10: // MUL
			dst = RVTOSIGNED(src1) * RVTOSIGNED(src2);
			return;
		case 0x11: // MULH (signed x signed)
			if constexpr (RVIS32BIT(cpu)) {
				dst = uint64_t((int64_t)RVTOSIGNED(src1) * (int64_t)RVTOSIGNED(src2)) >> 32u;
			} else if constexpr (RVIS64BIT(cpu)) {
				dst = ((__int128_t) src1 * (__int128_t) src2) >> 64u;
			} else {
				dst = 0;
			}
			return;
		case 0x12: // MULHSU (signed x unsigned)
			if constexpr (RVIS32BIT(cpu)) {
				dst = uint64_t((int64_t)RVTOSIGNED(src1) * (uint64_t)src2) >> 32u;
			} else if constexpr (RVIS64BIT(cpu)) {
				dst = ((__int128_t) src1 * (__int128_t) src2) >> 64u;
			} else {
				dst = 0;
			}
			return;
		case 0x13: // MULHU (unsigned x unsigned)
			if constexpr (RVIS32BIT(cpu)) {
				dst = uint64_t((uint64_t)src1 * (uint64_t)src2) >> 32u;
			} else if constexpr (RVIS64BIT(cpu)) {
				dst = ((__int128_t) src1 * (__int128_t) src2) >> 64u;
			} else {
				dst = 0;
			}
			return;
		case 0x14: // DIV
			// division by zero is not an exception
			if (LIKELY(RVTOSIGNED(src2) != 0)) {
				if constexpr (RVIS64BIT(cpu)) {
					// vi_instr.cpp:444:2: runtime error:
					// division of -9223372036854775808 by -1 cannot be represented in type 'long'
					if (LIKELY(!((int64_t)src1 == INT64_MIN && (int64_t)src2 == -1ll)))
						dst = RVTOSIGNED(src1) / RVTOSIGNED(src2);
				} else {
					// rv32i_instr.cpp:301:2: runtime error:
					// division of -2147483648 by -1 cannot be represented in type 'int'
					if (LIKELY(!(src1 == 2147483648 && src2 == 4294967295)))
						dst = RVTOSIGNED(src1) / RVTOSIGNED(src2);
				}
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x15: // DIVU
			if (LIKELY(src2 != 0)) {
				dst = src1 / src2;
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x16: // REM
			if (LIKELY(src2 != 0)) {
				if constexpr(RVIS32BIT(cpu)) {
					if (LIKELY(!(src1 == 2147483648 && src2 == 4294967295)))
						dst = RVTOSIGNED(src1) % RVTOSIGNED(src2);
				} else if constexpr (RVIS64BIT(cpu)) {
					if (LIKELY(!((int64_t)src1 == INT64_MIN && (int64_t)src2 == -1ll)))
						dst = RVTOSIGNED(src1) % RVTOSIGNED(src2);
				} else {
					dst = RVTOSIGNED(src1) % RVTOSIGNED(src2);
				}
			}
			return;
		case 0x17: // REMU
			if (LIKELY(src2 != 0)) {
				dst = src1 % src2;
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x54: // MIN
			dst = (RVSIGNTYPE(cpu)(src1) < RVSIGNTYPE(cpu)(src2)) ? src1 : src2;
			return;
		case 0x55: // MINU
			dst = (src1 < src2) ? src1 : src2;
			return;
		case 0x56: // MAX
			dst = (RVSIGNTYPE(cpu)(src1) > RVSIGNTYPE(cpu)(src2)) ? src1 : src2;
			return;
		case 0x57: // MAXU
			dst = (src1 > src2) ? src1 : src2;
			return;
		case 0x102: // SH1ADD
			dst = src2 + (src1 << 1);
			return;
		case 0x104: // SH2ADD
			dst = src2 + (src1 << 2);
			return;
		case 0x106: // SH3ADD
			dst = src2 + (src1 << 3);
			return;
		case 0x204: // XNOR
			dst = ~(src1 ^ src2);
			return;
		case 0x205: // SRA
			dst = (RVSIGNTYPE(cpu))src1 >> (src2 & (RVXLEN(cpu)-1));
			return;
		case 0x206: // ORN
			dst = src1 | ~src2;
			return;
		case 0x207: // ANDN
			dst = src1 & ~src2;
			return;
		case 0x301: { // ROL: Rotate left
			const auto shift = src2 & (RVXLEN(cpu) - 1);
			dst = (src1 << shift) | (src1 >> (RVXLEN(cpu) - shift));
			} return;
		}
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION, instr.whole);
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR
	{
		if (!instr.Rtype.is_32M())
		{
			static std::array<const char*, 8*2> func3 = {
				"ADD", "SLL", "SLT", "SLTU", "XOR", "SRL", "OR", "AND",
				"SUB", "SLL", "SLT", "SLTU", "XOR", "SRA", "OR", "AND"};
			const int EX = instr.Rtype.is_f7() ? 8 : 0;
			return snprintf(buffer, len, "%s %s %s, %s (0x%" PRIX64 ")",
							RISCV::regname(instr.Rtype.rs1),
							func3[instr.Rtype.funct3 + EX],
							RISCV::regname(instr.Rtype.rs2),
							RISCV::regname(instr.Rtype.rd),
							uint64_t(cpu.reg(instr.Rtype.rd)));
		}
		else {
			static std::array<const char*, 8> func3 = {
				"MUL", "MULH", "MULHSU", "MULHU", "DIV", "DIVU", "REM", "REMU"};
			return snprintf(buffer, len, "%s %s %s, %s (0x%" PRIX64 ")",
							RISCV::regname(instr.Rtype.rs1),
							func3[instr.Rtype.funct3],
							RISCV::regname(instr.Rtype.rs2),
							RISCV::regname(instr.Rtype.rd),
							uint64_t(cpu.reg(instr.Rtype.rd)));
		}
	});

	INSTRUCTION(SYSTEM,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_COLDATTR {
		cpu.machine().system(instr);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		// system functions
		static std::array<const char*, 2> etype = {"ECALL", "EBREAK"};
		if (instr.Itype.imm < 2 && instr.Itype.funct3 == 0) {
			return snprintf(buffer, len, "SYS %s", etype.at(instr.Itype.imm));
		} else if (instr.Itype.imm == 0x102 && instr.Itype.funct3 == 0) {
			return snprintf(buffer, len, "SYS SRET");
		} else if (instr.Itype.imm == 0x105 && instr.Itype.funct3 == 0) {
			return snprintf(buffer, len, "SYS WFI");
		} else if (instr.Itype.imm == 0x7FF && instr.Itype.funct3 == 0) {
			return snprintf(buffer, len, "SYS STOP");
		} else if (instr.Itype.funct3 == 0x1 || instr.Itype.funct3 == 0x2) {
			// CSRRW / CSRRS
			switch (instr.Itype.imm) {
				case 0x001:
					return snprintf(buffer, len, "RDCSR FFLAGS %s", RISCV::regname(instr.Itype.rd));
				case 0x002:
					return snprintf(buffer, len, "RDCSR FRM %s", RISCV::regname(instr.Itype.rd));
				case 0x003:
					return snprintf(buffer, len, "RDCSR FCSR %s", RISCV::regname(instr.Itype.rd));
				case 0xC00:
					if (instr.Itype.rd == 0 && instr.Itype.rs1 == 0)
						return snprintf(buffer, len, "UNIMP");
					else
						return snprintf(buffer, len, "RDCYCLE.L %s", RISCV::regname(instr.Itype.rd));
				case 0xC01:
					return snprintf(buffer, len, "RDINSTRET.L %s", RISCV::regname(instr.Itype.rd));
				case 0xC80:
					return snprintf(buffer, len, "RDCYCLE.U %s", RISCV::regname(instr.Itype.rd));
				case 0xC81:
					return snprintf(buffer, len, "RDINSTRET.U %s", RISCV::regname(instr.Itype.rd));
			}
			return snprintf(buffer, len, "CSRRS (unknown), %s", RISCV::regname(instr.Itype.rd));
		} else {
			return snprintf(buffer, len, "SYS ???");
		}
	});

	INSTRUCTION(OP_ADD,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Rtype.rd);
		dst = cpu.reg(instr.Rtype.rs1) + cpu.reg(instr.Rtype.rs2);
	}, DECODED_INSTR(OP).printer);

	INSTRUCTION(OP_SUB,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Rtype.rd);
		dst = cpu.reg(instr.Rtype.rs1) - cpu.reg(instr.Rtype.rs2);
	}, DECODED_INSTR(OP).printer);

	INSTRUCTION(SYSCALL,
	[] (auto& cpu, rv32i_instruction) RVINSTR_ATTR {
		cpu.machine().system_call(cpu.reg(REG_ECALL));
	}, DECODED_INSTR(SYSTEM).printer);

	INSTRUCTION(WFI,
	[] (auto& cpu, rv32i_instruction) RVINSTR_ATTR {
		cpu.machine().stop();
	}, DECODED_INSTR(SYSTEM).printer);

	INSTRUCTION(LUI,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		cpu.reg(instr.Utype.rd) = instr.Utype.upper_imm();
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		return snprintf(buffer, len, "LUI %s, 0x%X",
						RISCV::regname(instr.Utype.rd),
						instr.Utype.upper_imm());
	});

	INSTRUCTION(AUIPC,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		cpu.reg(instr.Utype.rd) = cpu.pc() + instr.Utype.upper_imm();
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		return snprintf(buffer, len, "AUIPC %s, PC+0x%X (0x%" PRIX64 ")",
						RISCV::regname(instr.Utype.rd),
						instr.Utype.upper_imm(),
						uint64_t(cpu.pc() + instr.Utype.upper_imm()));
	});

	INSTRUCTION(OP_IMM32_ADDIW,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const uint32_t src = cpu.reg(instr.Itype.rs1);
		// ADDIW: Add 32-bit sign-extended 12-bit immediate
		dst = (int32_t) (src + RVIMM(cpu, instr.Itype));
	},
	[] (char* buffer, size_t len, auto& cpu, rv32i_instruction instr) RVPRINTR_ATTR {
		if (instr.Itype.imm == 0)
		{
			// this is the official NOP instruction (ADDI x0, x0, 0)
			if (instr.Itype.rd == 0 && instr.Itype.rs1 == 0) {
				return snprintf(buffer, len, "NOP");
			}
			static std::array<const char*, 8> func3 = {"MV", "SLL", "SLT", "SLT", "XOR", "SRL", "OR", "AND"};
			return snprintf(buffer, len, "%sW %s, %s (0x%X)",
							func3[instr.Itype.funct3],
							RISCV::regname(instr.Itype.rd),
							RISCV::regname(instr.Itype.rs1),
							int32_t(cpu.reg(instr.Itype.rs1)));
		}
		else if (instr.Itype.rs1 != 0 && instr.Itype.funct3 == 1) {
			return snprintf(buffer, len, "SLLIW %s, %s << %u (0x%" PRIX64 ")",
							RISCV::regname(instr.Itype.rd),
							RISCV::regname(instr.Itype.rs1),
							instr.Itype.shift_imm(),
							uint64_t(cpu.reg(instr.Itype.rs1) << instr.Itype.shift_imm()));
		} else if (instr.Itype.rs1 != 0 && instr.Itype.funct3 == 5) {
			return snprintf(buffer, len, "%sW %s, %s >> %u (0x%" PRIX64 ")",
							(instr.Itype.is_srai() ? "SRAI" : "SRLI"),
							RISCV::regname(instr.Itype.rd),
							RISCV::regname(instr.Itype.rs1),
							instr.Itype.shift_imm(),
							uint64_t(cpu.reg(instr.Itype.rs1) >> instr.Itype.shift_imm()));
		} else if (instr.Itype.rs1 != 0) {
			static std::array<const char*, 8> func3 = {"ADDI", "SLLI", "SLTI", "SLTU", "XORI", "SRLI", "ORI", "ANDI"};
			if (!(instr.Itype.funct3 == 4 && instr.Itype.signed_imm() == -1)) {
				return snprintf(buffer, len, "%sW %s, %s%+d (0x%" PRIX64 ")",
								func3[instr.Itype.funct3],
								RISCV::regname(instr.Itype.rd),
								RISCV::regname(instr.Itype.rs1),
								instr.Itype.signed_imm(),
								uint64_t(cpu.reg(instr.Itype.rs1) + instr.Itype.signed_imm()));
			} else {
				return snprintf(buffer, len, "NOTW %s, %s",
								RISCV::regname(instr.Itype.rd),
								RISCV::regname(instr.Itype.rs1));
			}
		}
		static std::array<const char*, 8> func3 = {"LINT", "SLLI", "SLTI", "SLTU", "XORI", "SRLI", "ORI", "ANDI"};
		return snprintf(buffer, len, "%sW %s, %d (0x%X)",
						func3[instr.Itype.funct3],
						RISCV::regname(instr.Itype.rd),
						instr.Itype.signed_imm(), instr.Itype.signed_imm());
	});

	INSTRUCTION(OP_IMM32_SLLIW,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const uint32_t src = cpu.reg(instr.Itype.rs1);
		// SLLIW: Shift-Left Logical 0-31 immediate
		dst = (int32_t) (src << instr.Itype.shift_imm());
	}, DECODED_INSTR(OP_IMM32_ADDIW).printer);

	INSTRUCTION(OP_IMM32_SRLIW,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const uint32_t src = cpu.reg(instr.Itype.rs1);
		// SRLIW: Shift-Right Logical 0-31 immediate
		dst = (int32_t) (src >> instr.Itype.shift_imm());
	}, DECODED_INSTR(OP_IMM32_ADDIW).printer);

	INSTRUCTION(OP_IMM32_SRAIW,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const uint32_t src = cpu.reg(instr.Itype.rs1);
		// SRAIW: Arithmetic right shift, preserve the sign bit
		dst = (int32_t)src >> instr.Itype.shift_imm();
	}, DECODED_INSTR(OP_IMM32_ADDIW).printer);


	INSTRUCTION(OP_IMM32_SLLI_UW,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Itype.rd);
		const uint32_t src = cpu.reg(instr.Itype.rs1);
		// SLLI.UW: Shift-left Unsigned Word (Immediate)
		dst = src << instr.Itype.shift_imm();
	}, DECODED_INSTR(OP_IMM32_ADDIW).printer);

	INSTRUCTION(OP32,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Rtype.rd);
		const uint32_t src1 = cpu.reg(instr.Rtype.rs1);
		const uint32_t src2 = cpu.reg(instr.Rtype.rs2);

		switch (instr.Rtype.jumptable_friendly_op()) {
		case 0x1: // SLLW
			dst = (int32_t) ((uint32_t)src1 << (src2 & 31));
			return;
		case 0x5: // SRLW
			dst = (int32_t) ((uint32_t)src1 >> (src2 & 31));
			return;
		// M-extension
		case 0x10: // MULW (signed 32-bit multiply, sign-extended)
			dst = (int32_t) ((int32_t)src1 * (int32_t)src2);
			return;
		case 0x14: // DIVW
			// division by zero is not an exception
			if (LIKELY(src2 != 0)) {
				// division of -2147483648 by -1 cannot be represented in type 'int'
				if (LIKELY(!((int32_t)src1 == -2147483648 && (int32_t)src2 == -1))) {
					dst = (int32_t) ((int32_t)src1 / (int32_t)src2);
				}
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x15: // DIVUW
			if (LIKELY(src2 != 0)) {
				dst = (int32_t) (src1 / src2);
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x16: // REMW
			if (LIKELY(src2 != 0)) {
				if (LIKELY(!((int32_t)src1 == -2147483648 && (int32_t)src2 == -1))) {
					dst = (int32_t) ((int32_t)src1 % (int32_t)src2);
				}
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x17: // REMUW
			if (LIKELY(src2 != 0)) {
				dst = (int32_t) (src1 % src2);
			} else {
				dst = (RVREGTYPE(cpu)) -1;
			}
			return;
		case 0x40: // ADDUW
			dst = cpu.reg(instr.Rtype.rs2) + src1;
			return;
		case 0x44: // ZEXT.H (imm=0x40):
			dst = uint16_t(src1);
			return;
		case 0x102: // SH1ADD.UW
			dst = cpu.reg(instr.Rtype.rs2) + (src1 << 1);
			return;
		case 0x104: // SH2ADD.UW
			dst = cpu.reg(instr.Rtype.rs2) + (src1 << 2);
			return;
		case 0x106: // SH3ADD.UW
			dst = cpu.reg(instr.Rtype.rs2) + (src1 << 3);
			return;
		case 0x200: // SUBW
			dst = (int32_t) (src1 - src2);
			return;
		case 0x205: // SRAW
			dst = (int32_t)src1 >> (src2 & 31);
			return;
		}
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION, instr.whole);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		if (!instr.Rtype.is_32M())
		{
			static std::array<const char*, 8*2> func3 = {
				"ADD", "SLL", "SLT", "SLTU", "XOR", "SRL", "OR", "AND",
				"SUB", "SLL", "SLT", "SLTU", "XOR", "SRA", "OR", "AND"};
			const int EX = instr.Rtype.is_f7() ? 8 : 0;
			return snprintf(buffer, len, "%s %sW %s, %s",
							RISCV::regname(instr.Rtype.rs1),
							func3[instr.Rtype.funct3 + EX],
							RISCV::regname(instr.Rtype.rs2),
							RISCV::regname(instr.Rtype.rd));
		}
		else {
			static std::array<const char*, 8> func3 = {
				"MUL", "MULH", "MULHSU", "MULHU", "DIV", "DIVU", "REM", "REMU"};
			return snprintf(buffer, len, "%s %sW %s, %s",
							RISCV::regname(instr.Rtype.rs1),
							func3[instr.Rtype.funct3],
							RISCV::regname(instr.Rtype.rs2),
							RISCV::regname(instr.Rtype.rd));
		}
	});

	INSTRUCTION(OP32_ADDW,
	[] (auto& cpu, rv32i_instruction instr) RVINSTR_ATTR {
		auto& dst = cpu.reg(instr.Rtype.rd);
		const uint32_t src1 = cpu.reg(instr.Rtype.rs1);
		const uint32_t src2 = cpu.reg(instr.Rtype.rs2);
		dst = (int32_t) (src1 + src2);
	}, DECODED_INSTR(OP32).printer);

	INSTRUCTION(FENCE,
	[] (auto&, rv32i_instruction /* instr */) RVINSTR_COLDATTR {
		// Do a full barrier, for now
		std::atomic_thread_fence(std::memory_order_seq_cst);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction) RVPRINTR_ATTR {
		// printer
		return snprintf(buffer, len, "FENCE");
	});
}
