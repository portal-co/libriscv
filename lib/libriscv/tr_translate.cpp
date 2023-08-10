#include <cmath>
#include <dlfcn.h>
#include <unistd.h>
#include "machine.hpp"
#include "decoder_cache.hpp"
#include "instruction_list.hpp"
#include "rv32i_instr.hpp"
#include "tr_api.hpp"
#include "tr_types.hpp"
#include "util/crc32.hpp"
#include <unordered_set>
//#define BINTR_TIMING

namespace riscv
{
	static constexpr bool VERBOSE_BLOCKS = false;
	static constexpr bool SCAN_FOR_GP = true;

	inline timespec time_now();
	inline long nanodiff(timespec, timespec);
#ifdef BINTR_TIMING
	#define TIME_POINT(x) \
		asm("" : : : "memory"); \
		auto x = time_now();    \
		asm("" : : : "memory");
#else
	#define TIME_POINT(x)  /* */
#endif

static const std::unordered_set<uint32_t> good_insn
{
	RV32I_LOAD,
	RV32I_STORE,
	RV32I_BRANCH,
	RV32I_JAL,
	//RV32I_JALR,
	RV32I_OP_IMM,
	RV32I_OP,
	RV32I_LUI,
	RV32I_AUIPC,
	RV32I_SYSTEM,
	RV32I_FENCE,
	RV64I_OP_IMM32,
	RV64I_OP32,
	RV32F_LOAD,
	RV32F_STORE,
	RV32F_FMADD,
	RV32F_FMSUB,
	RV32F_FNMADD,
	RV32F_FNMSUB,
	RV32F_FPFUNC,
	RV32A_ATOMIC,
	// TODO: Needs better integration
	//RV32V_OP
};

template <int W>
inline uint32_t opcode(const TransInstr<W>& ti) {
	return rv32i_instruction{ti.instr}.opcode();
}

template <int W>
inline DecoderData<W>& decoder_entry_at(const DecodedExecuteSegment<W>& exec, address_type<W> addr) {
	return exec.decoder_cache()[addr / DecoderCache<W>::DIVISOR];
}

template <int W>
struct NamedIPair {
	address_type<W> addr;
	std::string symbol;
};

template <int W>
int CPU<W>::load_translation(const MachineOptions<W>& options,
	std::string* filename) const
{
	// Disable translator with NO_TRANSLATE=1
	// or by setting max blocks to zero.
	if (0 == options.translate_blocks_max || getenv("NO_TRANSLATE")) {
		if (getenv("VERBOSE")) {
			printf("Binary translation disabled\n");
		}
		machine().memory.set_binary_translated(nullptr);
		return -1;
	}
	if (machine().memory.is_binary_translated()) {
		throw MachineException(ILLEGAL_OPERATION, "Machine already reports binary translation");
	}

	const auto& exec = this->machine().memory.main_execute_segment();
	auto* exec_data = exec.exec_data(exec.exec_begin());

	// Checksum the execute segment + compiler flags
	TIME_POINT(t5);
	extern std::string compile_command(int arch);
	const auto cc = compile_command(W);
	const uint32_t checksum =
		crc32c(exec_data, exec.exec_end() - exec.exec_begin())
		^ crc32c(cc.c_str(), cc.size());

	char filebuffer[256];
	int len = snprintf(filebuffer, sizeof(filebuffer),
		"/tmp/rvbintr-%08X", checksum);
	if (len <= 0)
		return -1;

	void* dylib = nullptr;
#ifdef BINTR_TIMING
	TIME_POINT(t6);
	printf(">> Execute segment hashing took %ld ns\n", nanodiff(t5, t6));
#endif

	// Always check if there is an existing file
	if (access(filebuffer, R_OK) == 0) {
		TIME_POINT(t7);
		dylib = dlopen(filebuffer, RTLD_LAZY);
	#ifdef BINTR_TIMING
		TIME_POINT(t8);
		printf(">> dlopen took %ld ns\n", nanodiff(t7, t8));
	#endif
	}

	// We must compile ourselves
	if (dylib == nullptr) {
		if (filename) *filename = std::string(filebuffer);
		return 1;
	}

	this->activate_dylib(dylib);

	// close dylib when machine is destructed
	machine().memory.set_binary_translated(dylib);
#ifdef BINTR_TIMING
	TIME_POINT(t10);
	printf(">> Loading binary translation took %ld ns\n", nanodiff(t5, t10));
#endif
	return 0;
}

template <int W>
void CPU<W>::try_translate(const MachineOptions<W>& options,
	const std::string& filename, address_t basepc, std::vector<TransInstr<W>> ipairs) const
{
	// Run with VERBOSE=1 to see command and output
	const bool verbose = (getenv("VERBOSE") != nullptr);

	address_t gp = 0;
	TIME_POINT(t0);
if constexpr (SCAN_FOR_GP) {
	// We assume that GP is initialized with AUIPC,
	// followed by OP_IMM (and maybe OP_IMM32)
	for (auto it = ipairs.begin(); it != ipairs.end(); ++it) {
		const rv32i_instruction instruction {it->instr};
		if (instruction.opcode() == RV32I_AUIPC) {
			const auto auipc = instruction;
			if (auipc.Utype.rd == 3) { // GP
				// calculate current PC for AUIPC
				const address_t pc = basepc + 4 * (it - ipairs.begin());
				const auto addi = rv32i_instruction {(it+1)->instr};
				if (addi.opcode() == RV32I_OP_IMM && addi.Itype.funct3 == 0x0) {
					//printf("Found OP_IMM: ADDI  rd=%d, rs1=%d\n", addi.Itype.rd, addi.Itype.rs1);
					if (addi.Itype.rd == 3 && addi.Itype.rs1 == 3) { // GP
						gp = pc + auipc.Utype.upper_imm() + addi.Itype.signed_imm();
						break;
					}
				}
			}
		} // opcode
	} // iterator
#ifdef BINTR_TIMING
	TIME_POINT(t1);
	printf(">> GP scan took %ld ns, GP=0x%lX\n", nanodiff(t0, t1), (long)gp);
#endif
} // SCAN_FOR_GP

	// Code block and loop detection
	TIME_POINT(t2);
	size_t icounter = 0;
	auto it = ipairs.begin();
	std::vector<std::pair<decltype(it), address_t>> loops;
	struct CodeBlock {
		TransInstr<W>& instr;
		size_t      length;
		address_t   addr;
		bool        has_branch;
		std::set<address_t> jump_locations;
	};
	std::vector<CodeBlock> blocks;
	std::set<address_t> jump_locations;

	while (it != ipairs.end() && icounter < options.translate_instr_max)
	{
		const auto block = it;
		bool has_branch = false;
		// Measure length of instructions that belong
		// together sequentially (a code block).
		auto current_pc = basepc;

		for (; it != ipairs.end(); ++it) {
			const rv32i_instruction instruction{it->instr};
			const auto opcode = instruction.opcode();

			// JALR is a show-stopper / code-blocker
			if (opcode == RV32I_JALR)
			{
				current_pc += 4;
				++it; break;
			}
			// loop detection (negative branch offsets)
			if (opcode == RV32I_BRANCH) {
				has_branch = true;
				// detect jump location
				const auto offset = instruction.Btype.signed_imm();
				jump_locations.insert(current_pc + offset);
			}
			if (opcode == RV32I_JAL) {
				has_branch = true;
				const auto offset = instruction.Jtype.jump_offset();
				jump_locations.insert(current_pc + offset);
			}

			current_pc += 4;
		} // find block

		// Process block and add it for emission
		const size_t length = it - block;
		if (length >= options.block_size_treshold
			&& icounter + length < options.translate_instr_max)
		{
			if constexpr (VERBOSE_BLOCKS) {
				printf("Block found at %#lX. Length: %zu\n", (long) basepc, length);
			}
			blocks.push_back({
				*block, length, basepc, has_branch,
				std::move(jump_locations)
			});
			icounter += length;
			// we can't translate beyond this estimate, otherwise
			// the compiler will never finish code generation
			if (blocks.size() >= options.translate_blocks_max)
				break;
		}
		basepc = current_pc;
	}
#ifdef BINTR_TIMING
	TIME_POINT(t3);
	printf(">> Code block detection %ld ns\n", nanodiff(t2, t3));
#endif

	// Code generation
	std::vector<NamedIPair<W>> dlmappings;
	extern const std::string bintr_code;
	std::string code = bintr_code;

	for (const auto& block : blocks)
	{
		std::string func =
			"f" + std::to_string(block.addr);
		emit(code, func, &block.instr, {
			block.addr, gp, (int)block.length,
			block.has_branch,
			true, // forward jumps
			std::move(block.jump_locations)
		});
		dlmappings.push_back({block.addr, std::move(func)});
	}
	// Append all instruction handler -> dl function mappings
	code += "const uint32_t no_mappings = "
		+ std::to_string(dlmappings.size()) + ";\n";
	code += R"V0G0N(
struct Mapping {
	addr_t addr;
	void (*handler)();
};
const struct Mapping mappings[] = {
)V0G0N";
	for (const auto& mapping : dlmappings)
	{
		char buffer[128];
		snprintf(buffer, sizeof(buffer), 
			"{0x%lX, %s},\n",
			(long)mapping.addr, mapping.symbol.c_str());
		code.append(buffer);
	}
	code += "};\n";

#ifdef BINTR_TIMING
	TIME_POINT(t4);
	printf(">> Code generation took %ld ns\n", nanodiff(t3, t4));
#endif

	if (verbose) {
		printf("Emitted %zu accelerated instructions and %zu functions. GP=0x%lX\n",
			icounter, dlmappings.size(), (long) gp);
	}
	// nothing to compile without mappings
	if (dlmappings.empty()) {
		if (verbose) {
			printf("Binary translator has nothing to compile! No mappings.\n");
		}
		return;
	}

	TIME_POINT(t9);
	extern void* compile(const std::string& code, int arch, const char*);
	void* dylib = compile(code, W, filename.c_str());
#ifdef BINTR_TIMING
	TIME_POINT(t10);
	printf(">> Code compilation took %.2f ms\n", nanodiff(t9, t10) / 1e6);
#endif
	// Check compilation result
	if (dylib == nullptr) {
		return;
	}

	this->activate_dylib(dylib);

#ifndef RISCV_TRANSLATION_CACHE
	// Delete the program if the shared ELF is unwanted
	unlink(filename.c_str());
#endif

	// close dylib when machine is destructed
	machine().memory.set_binary_translated(dylib);
#ifdef BINTR_TIMING
	TIME_POINT(t12);
	printf(">> Binary translation totals %.2f ms\n", nanodiff(t0, t12) / 1e6);
#endif
}

template <int W>
void CPU<W>::activate_dylib(void* dylib) const
{
	TIME_POINT(t11);
	// map the API callback table
	auto* ptr = dlsym(dylib, "init");
	if (ptr == nullptr) {
		// only warn when translation is not already disabled
		if (getenv("NO_TRANSLATE") == nullptr) {
			fprintf(stderr, "libriscv: Could not find dylib init function\n");
		}
		dlclose(dylib);
		return;
	}

	auto func = (void (*)(const CallbackTable<W>&, uint64_t*, uint64_t*)) ptr;
	func(CallbackTable<W>{
		.mem_read8 = [] (CPU<W>& cpu, address_type<W> addr) -> uint8_t {
			return cpu.machine().memory.template read<uint8_t> (addr);
		},
		.mem_read16 = [] (CPU<W>& cpu, address_type<W> addr) -> uint16_t {
			return cpu.machine().memory.template read<uint16_t> (addr);
		},
		.mem_read32 = [] (CPU<W>& cpu, address_type<W> addr) -> uint32_t {
			return cpu.machine().memory.template read<uint32_t> (addr);
		},
		.mem_read64 = [] (CPU<W>& cpu, address_type<W> addr) -> uint64_t {
			return cpu.machine().memory.template read<uint64_t> (addr);
		},
		.mem_write8 = [] (CPU<W>& cpu, address_type<W> addr, uint8_t val) {
			cpu.machine().memory.template write<uint8_t> (addr, val);
		},
		.mem_write16 = [] (CPU<W>& cpu, address_type<W> addr, uint16_t val) {
			cpu.machine().memory.template write<uint16_t> (addr, val);
		},
		.mem_write32 = [] (CPU<W>& cpu, address_type<W> addr, uint32_t val) {
			cpu.machine().memory.template write<uint32_t> (addr, val);
		},
		.mem_write64 = [] (CPU<W>& cpu, address_type<W> addr, uint64_t val) {
			cpu.machine().memory.template write<uint64_t> (addr, val);
		},
		.jump = [] (CPU<W>& cpu, address_type<W> addr) {
			cpu.jump(addr);
		},
		.syscall = [] (CPU<W>& cpu, address_type<W> n) -> int {
			auto old_pc = cpu.pc();
			cpu.machine().system_call(n);
			// if the system did not modify PC, return to bintr
			if (cpu.pc() == old_pc && !cpu.machine().stopped()) {
				cpu.registers().pc = old_pc;
				return 0;
			}
			// otherwise, update instruction counter and exit
			return 1;
		},
		.stop = [] (CPU<W>& cpu) {
			cpu.machine().stop();
		},
		.ebreak = [] (CPU<W>& cpu) {
			cpu.machine().ebreak();
		},
		.system = [] (CPU<W>& cpu, uint32_t instr) {
			cpu.machine().system(rv32i_instruction{instr});
		},
		.execute = [] (CPU<W>& cpu, uint32_t instr) {
			const rv32i_instruction rvi{instr};
			cpu.decode(rvi).handler(cpu, rvi);
		},
		.trigger_exception = [] (CPU<W>& cpu, int e) {
			cpu.trigger_exception(e);
		},
		.sqrtf32 = [] (float f) -> float {
			return std::sqrt(f);
		},
		.sqrtf64 = [] (double d) -> double {
			return std::sqrt(d);
		},
	},
	&m_machine.get_counters().first,
	&m_machine.get_counters().second);

	// Map all the functions to instruction handlers
	uint32_t* no_mappings = (uint32_t *)dlsym(dylib, "no_mappings");
	struct Mapping {
		address_t addr;
		instruction_handler<W> handler;
	};
	Mapping* mappings = (Mapping *)dlsym(dylib, "mappings");

	if (no_mappings == nullptr || mappings == nullptr) {
		throw MachineException(INVALID_PROGRAM, "Invalid mappings in binary translation program");
	}

	// Apply mappings to decoder cache
	auto& main_exec = machine().memory.main_execute_segment();
	const auto nmappings = *no_mappings;
	for (size_t i = 0; i < nmappings; i++) {
		if (mappings[i].handler != nullptr) {
			auto& entry = decoder_entry_at(main_exec, mappings[i].addr);
			entry.set_insn_handler((instruction_handler<W>) mappings[i].handler);
		}
	}

#ifdef BINTR_TIMING
	TIME_POINT(t12);
	printf(">> Binary translation activation %ld ns\n", nanodiff(t11, t12));
#endif
}

	template void CPU<4>::try_translate(const MachineOptions<4>&, const std::string&, address_t, std::vector<TransInstr<4>>) const;
	template void CPU<8>::try_translate(const MachineOptions<8>&, const std::string&, address_t, std::vector<TransInstr<8>>) const;
	template int CPU<4>::load_translation(const MachineOptions<4>&, std::string*) const;
	template int CPU<8>::load_translation(const MachineOptions<8>&, std::string*) const;
	template void CPU<4>::activate_dylib(void*) const;
	template void CPU<8>::activate_dylib(void*) const;

	static_assert(!compressed_enabled,
		"C-extension incompatible with binary translation");

	timespec time_now()
	{
		timespec t;
		clock_gettime(CLOCK_MONOTONIC, &t);
		return t;
	}
	long nanodiff(timespec start_time, timespec end_time)
	{
		return (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
	}
}
