#include "machine.hpp"
#include "decoder_cache.hpp"
#include "riscvbase.hpp"
#include "rv32i_instr.hpp"
#include "rv32i.hpp"
#include "rv64i.hpp"
#include "rv128i.hpp"

#define INSTRUCTION_LOGGING()	\
	if (machine().verbose_instructions) { \
		const auto string = isa_type<W>::to_string(*this, instruction, decode(instruction)); \
		printf("%s\n", string.c_str()); \
	}


namespace riscv
{
	template <int W>
	CPU<W>::CPU(Machine<W>& machine, const Machine<W>& other)
		: m_machine { machine }
	{
		this->m_exec_data  = other.cpu.m_exec_data;
		this->m_exec_begin = other.cpu.m_exec_begin;
		this->m_exec_end   = other.cpu.m_exec_end;

		this->registers() = other.cpu.registers();
#ifdef RISCV_EXT_ATOMICS
		this->m_atomics = other.cpu.m_atomics;
#endif
	}
	template <int W>
	void CPU<W>::reset()
	{
		static_assert(offsetof(CPU, m_regs) == 0, "Registers must be first");
		this->m_regs = {};
		this->reset_stack_pointer();
		// We can't jump if there's been no ELF loader
		if (!machine().memory.binary().empty()) {
			this->jump(machine().memory.start_address());
		}
		// reset the page cache
		this->m_cache = {};
	}

	template <int W>
	void CPU<W>::init_execute_area(const uint8_t* data, address_t begin, address_t length)
	{
		this->initialize_exec_segs(data - begin, begin, length);
	#ifdef RISCV_INSTR_CACHE
		machine().memory.generate_decoder_cache({}, begin, begin, length);
	#endif
	}

	template <int W> __attribute__((noinline))
	typename CPU<W>::format_t CPU<W>::read_next_instruction_slowpath()
	{
		// Fallback: Read directly from page memory
		const auto pageno = this->pc() >> Page::SHIFT;
		// Page cache
		auto& entry = this->m_cache;
		if (entry.pageno != pageno || entry.page == nullptr) {
			auto e = decltype(m_cache){pageno, &machine().memory.get_exec_pageno(pageno)};
			if (!e.page->attr.exec) {
				trigger_exception(EXECUTION_SPACE_PROTECTION_FAULT, this->pc());
			}
			// delay setting entry until we know it's good!
			entry = e;
		}
		const auto& page = *entry.page;
		const auto offset = this->pc() & (Page::size()-1);
		format_t instruction;

		if (LIKELY(offset <= Page::size()-4)) {
			instruction.whole = *(uint32_t*) (page.data() + offset);
			return instruction;
		}
		// It's not possible to jump to a misaligned address,
		// so there is necessarily 16-bit left of the page now.
		instruction.whole = *(uint16_t*) (page.data() + offset);

		// If it's a 32-bit instruction at a page border, we need
		// to get the next page, and then read the upper half
		if (UNLIKELY(instruction.is_long()))
		{
			const auto& page = machine().memory.get_exec_pageno(pageno+1);
			instruction.half[1] = *(uint16_t*) page.data();
		}

		return instruction;
	}

	template <int W>
	typename CPU<W>::format_t CPU<W>::read_next_instruction()
	{
		if (LIKELY(this->pc() >= m_exec_begin && this->pc() < m_exec_end)) {
			return format_t { *(uint32_t*) &m_exec_data[this->pc()] };
		}

		return read_next_instruction_slowpath();
	}

	template<int W> __attribute__((hot))
	void CPU<W>::simulate(uint64_t max)
	{
		// Calculate the instruction limit
#ifndef RISCV_BINARY_TRANSLATION
		machine().set_max_instructions(max);
		uint64_t counter = 0;

		for (; counter < machine().max_instructions(); counter++) {
#else
		/* With binary translation we need to modify the counter from anywhere */ ;;
		if (max != UINT64_MAX)
			machine().set_max_instructions(machine().instruction_counter() + max);
		else
			machine().set_max_instructions(UINT64_MAX);

		for (; machine().instruction_counter() < machine().max_instructions();
			machine().increment_counter(1)) {
#endif

			format_t instruction;
#ifdef RISCV_DEBUG
			this->break_checks();
#endif

# ifdef RISCV_INSTR_CACHE
#  ifndef RISCV_INBOUND_JUMPS_ONLY
		if (LIKELY(this->pc() >= m_exec_begin && this->pc() < m_exec_end)) {
#  endif
			instruction = format_t { *(uint32_t*) &m_exec_data[this->pc()] };
			// retrieve instructions directly from the constant cache
			auto& cache_entry =
				machine().memory.get_decoder_cache()[this->pc() / DecoderCache<W>::DIVISOR];
		#ifndef RISCV_INSTR_CACHE_PREGEN
			if (UNLIKELY(!DecoderCache<W>::isset(cache_entry))) {
				DecoderCache<W>::convert(this->decode(instruction), cache_entry);
			}
		#endif
		#ifdef RISCV_DEBUG
			INSTRUCTION_LOGGING();
			// execute instruction
			cache_entry.handler(*this, instruction);
		#else
			// execute instruction
			cache_entry(*this, instruction);
		#endif
#  ifndef RISCV_INBOUND_JUMPS_ONLY
		} else {
			instruction = read_next_instruction_slowpath();
	#ifdef RISCV_DEBUG
			INSTRUCTION_LOGGING();
	#endif
			// decode & execute instruction directly
			this->execute(instruction);
		}
#  endif
# else
			instruction = this->read_next_instruction();
	#ifdef RISCV_DEBUG
			INSTRUCTION_LOGGING();
	#endif
			// decode & execute instruction directly
			this->execute(instruction);
# endif

#ifdef RISCV_DEBUG
			if (UNLIKELY(machine().verbose_registers))
			{
				auto regs = this->registers().to_string();
				printf("\n%s\n\n", regs.c_str());
				if (UNLIKELY(machine().verbose_fp_registers)) {
					printf("%s\n", registers().flp_to_string().c_str());
				}
			}
#endif
			// increment PC
			if constexpr (compressed_enabled)
				registers().pc += instruction.length();
			else
				registers().pc += 4;
		} // while not stopped
	#ifndef RISCV_BINARY_TRANSLATION
		machine().increment_counter(counter);
	#endif
	} // CPU::simulate

	template<int W>
	void CPU<W>::step_one()
	{
		this->simulate(1);
	}

	template<int W> __attribute__((cold))
	void CPU<W>::trigger_exception(int intr, address_t data)
	{
		switch (intr)
		{
		case ILLEGAL_OPCODE:
			throw MachineException(ILLEGAL_OPCODE,
					"Illegal opcode executed", data);
		case ILLEGAL_OPERATION:
			throw MachineException(ILLEGAL_OPERATION,
					"Illegal operation during instruction decoding", data);
		case PROTECTION_FAULT:
			throw MachineException(PROTECTION_FAULT,
					"Protection fault", data);
		case EXECUTION_SPACE_PROTECTION_FAULT:
			throw MachineException(EXECUTION_SPACE_PROTECTION_FAULT,
					"Execution space protection fault", data);
		case MISALIGNED_INSTRUCTION:
			// NOTE: only check for this when jumping or branching
			throw MachineException(MISALIGNED_INSTRUCTION,
					"Misaligned instruction executed", data);
		case UNIMPLEMENTED_INSTRUCTION:
			throw MachineException(UNIMPLEMENTED_INSTRUCTION,
					"Unimplemented instruction executed", data);
		default:
			throw MachineException(UNKNOWN_EXCEPTION,
					"Unknown exception", intr);
		}
	}

	template <int W> __attribute__((cold))
	std::string CPU<W>::to_string(format_t format, const instruction_t& instr) const
	{
		if constexpr (W == 4)
			return RV32I::to_string(*this, format, instr);
		else if constexpr (W == 8)
			return RV64I::to_string(*this, format, instr);
		else if constexpr (W == 16)
			return RV128I::to_string(*this, format, instr);
		return "Unknown architecture";
	}

	template <int W> __attribute__((cold))
	std::string Registers<W>::flp_to_string() const
	{
		char buffer[800];
		int  len = 0;
		for (int i = 0; i < 32; i++) {
			auto& src = this->getfl(i);
			const char T = (src.i32[1] == -1) ? 'S' : 'D';
			double val = (src.i32[1] == -1) ? src.f32[0] : src.f64;
			len += snprintf(buffer+len, sizeof(buffer) - len,
					"[%s\t%c%+.2f] ", RISCV::flpname(i), T, val);
			if (i % 5 == 4) {
				len += snprintf(buffer+len, sizeof(buffer)-len, "\n");
			}
		}
		return std::string(buffer, len);
	}

	template struct CPU<4>;
	template struct Registers<4>;
	template struct CPU<8>;
	template struct Registers<8>;
	template struct CPU<16>;
	template struct Registers<16>;
}
