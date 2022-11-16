#pragma once
#include <array>
#include "common.hpp"
#include "types.hpp"
#define RISCV_DECODER_BASE_FUNC DecoderData::function

namespace riscv {

template <int W>
struct DecoderData {
	using Handler = instruction_handler<W>;
#if defined(RISCV_DECODER_COMPRESS) && defined(RISCV_EXT_COMPRESSED)
	int32_t m_handler = 0x0;
#elif !defined(RISCV_DECODER_COMPRESS)
	Handler m_handler = nullptr;
#endif
#ifdef RISCV_FAST_SIMULATOR
	uint32_t instr;
	uint16_t idxend;
	// NOTE: Original_opcode is only relevant during decoding.
	union {
		uint16_t original_opcode;
		struct {
			uint8_t opcode_length;
			uint8_t instr_count;
		};
#if defined(RISCV_DECODER_COMPRESS) && !defined(RISCV_EXT_COMPRESSED)
		uint16_t m_handler;
#endif
	};

	void execute(CPU<W>& cpu) {
		get_handler()(cpu, instruction_format{this->instr});
	}
#endif // RISCV_FAST_SIMULATOR

	template <typename... Args>
	void execute(CPU<W>& cpu, Args... args) {
		get_handler()(cpu, args...);
	}
	bool isset() const noexcept {
		return get_handler() != nullptr;
	}
	void set_handler(Instruction<W> insn) noexcept {
		this->set_insn_handler(insn.handler);
	}

#ifdef RISCV_DECODER_COMPRESS
	Handler get_handler() const noexcept {
		return instr_handlers[m_handler];
		//return (Handler)((uintptr_t)&RISCV_DECODER_BASE_FUNC + m_handler);
	}
	void set_insn_handler(instruction_handler<W> ih) noexcept {
		this->m_handler = index_for(ih);
		//this->m_handler = (uintptr_t)ih - (uintptr_t)&RISCV_DECODER_BASE_FUNC;
	}
#else
	Handler get_handler() const noexcept {
		return this->m_handler;
	}
	void set_insn_handler(instruction_handler<W> ih) noexcept {
		this->m_handler = ih;
	}
#endif

#ifdef RISCV_DECODER_COMPRESS
private:
	static size_t index_for(Handler new_handler) {
		for (size_t i = 1; i < instr_handlers.size(); i++) {
			auto& handler = instr_handlers[i];
			if (handler == new_handler)
				return i;
			else if (handler == nullptr) {
				handler = new_handler;
				return i;
			}
		}
		throw MachineException(MAX_INSTRUCTIONS_REACHED,
			"Not enough instruction handler space", instr_handlers.size());
	}
	static inline std::array<Handler, 4096> instr_handlers;
	static void function() {}
#endif
};

template <int W>
struct DecoderCache
{
	static constexpr size_t DIVISOR = (compressed_enabled) ? 2 : 4;

	inline auto& get(size_t idx) noexcept {
		return cache[idx];
	}

	inline auto* get_base() noexcept {
		return &cache[0];
	}

	std::array<DecoderData<W>, PageSize / DIVISOR> cache = {};
};

}
