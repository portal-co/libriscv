# RISC-V userspace emulator library

_libriscv_ is a simple and slim RISC-V userspace emulator library that is highly embeddable and configurable. There are several [CMake options](lib/CMakeLists.txt) that control RISC-V extensions and how the emulator behaves.

There is also a CLI that you can use to run RISC-V programs and step through instructions one by one, like a simulator, or connect with GDB.

[![Build configuration matrix](https://github.com/fwsGonzo/libriscv/actions/workflows/buildconfig.yml/badge.svg)](https://github.com/fwsGonzo/libriscv/actions/workflows/buildconfig.yml) [![Unit Tests](https://github.com/fwsGonzo/libriscv/actions/workflows/unittests.yml/badge.svg)](https://github.com/fwsGonzo/libriscv/actions/workflows/unittests.yml) [![Experimental Unit Tests](https://github.com/fwsGonzo/libriscv/actions/workflows/unittests_exp.yml/badge.svg)](https://github.com/fwsGonzo/libriscv/actions/workflows/unittests_exp.yml) [![Linux emulator](https://github.com/fwsGonzo/libriscv/actions/workflows/emulator.yml/badge.svg)](https://github.com/fwsGonzo/libriscv/actions/workflows/emulator.yml) [![MinGW 64-bit emulator build](https://github.com/fwsGonzo/libriscv/actions/workflows/mingw.yml/badge.svg)](https://github.com/fwsGonzo/libriscv/actions/workflows/mingw.yml) [![Verify example programs](https://github.com/fwsGonzo/libriscv/actions/workflows/verify_examples.yml/badge.svg)](https://github.com/fwsGonzo/libriscv/actions/workflows/verify_examples.yml)

## Benchmarks

[STREAM memory benchmark](https://gist.github.com/fwsGonzo/a594727a9429cb29f2012652ad43fb37)

[CoreMark on a ThinkPad Z13](https://gist.github.com/fwsGonzo/571fde065e6aa73010bc2f948640bdc5) 2847 vs 31791 native.

Run [D00M 1 in libriscv](/examples/doom) and see for yourself. It should use around 15% CPU at 60 fps.

Benchmark between [libriscv binary translation and LuaJIT](https://gist.github.com/fwsGonzo/9132f0ef7d3f009baa5b222eedf392da). Most benchmarks are hand-picked for the purposes of game engine scripting, but there are still some classic benchmarks.

## What is userspace emulation?

Userspace emulation means running regular ELF programs in a sandbox, trapping and emulating system calls in order to provide the Linux environment the program expects, but also make sure the program is not doing anything wrong. There is fairly good support for Linux system calls, however anyone can implement support for other OSes, and ultimately even ELF loading is optional.

Instruction counting is used to limit the time spent executing code and can be used to prevent infinite loops. It can also help keep frame budgets for long running background scripting tasks as running out of instructions simply halts execution, and it can be resumed from where it stopped.

The virtual address space is implemented using pages, which means you can copy code into memory, make it executable, and then jump to it. It should Just Work. It also makes it possible to run more complex language runtimes like Go.


## Embedding the emulator in a project

See [example project](/examples/embed) for embedding on Linux.

On Windows you can use Clang-cl in Visual Studio. See the [example CMake project](/examples/msvc). It requires Clang and Git installed.


## Installing a RISC-V GCC compiler

On Ubuntu and Linux distributions like it, you can install a 64-bit RISC-V GCC compiler for running Linux programs with a one-liner:

```
sudo apt install gcc-11-riscv64-linux-gnu g++-11-riscv64-linux-gnu
```

Depending on your distro you may have access to GCC versions 10, 11 and 12. Now you have a full Linux C/C++ compiler for RISC-V. It is typically configured to use the C-extension, so make sure you have that enabled.

To build smaller and leaner programs you will need a (limited) Linux userspace environment. You sometimes need to build this cross-compiler yourself:

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv --with-arch=rv32g --with-abi=ilp32d
make
```
This will build a newlib cross-compiler with C++ exception support. The ABI is ilp32d, which is for 32-bit and 64-bit floating-point instruction set support. It is much faster than software implementations of binary IEEE floating-point arithmetic.

Note that if you want a full glibc cross-compiler instead, simply appending `linux` to the make command will suffice, like so: `make linux`. Glibc produces larger binaries but has more features, like sockets and threads.

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv --with-arch=rv64g --with-abi=lp64d
make
```
The incantation for 64-bit RISC-V. Not enabling the C-extension for compressed instructions results in faster emulation.

The last step is to add your compiler to PATH so that it becomes visible to build systems. So, add this at the bottom of your `.bashrc` file in the home (~) directory:

```
export PATH=$PATH:$HOME/riscv/bin
```

## Running a RISC-V program

```sh
cd emulator
./build.sh
./rvlinux <path to RISC-V ELF binary>
```

The emulator is built 3 times for different purposes. `rvmicro` is built for micro-environments with custom heap and threads. `rvnewlib` has hooked up enough system calls to run newlib programs. `rvlinux` has all the system calls necessary to run a normal userspace linux program. Each emulator is capable of running both 32- and 64-bit RISC-V programs. `rvlinux` can be used for most programs.

You can step through programs instruction by instruction by running the emulator with `DEBUG=1`:
```sh
cd emulator
DEBUG=1 ./rvlinux <path to RISC-V ELF binary>
```

You can use GDB remotely by starting the emulator with `GDB=1`:
```sh
cd emulator
GDB=1 ./rvlinux <path to RISC-V ELF binary>
```
Connect from `gdb-multiarch` with `target remote localhost:2159` after loading the program with `file <path>`.


## Example RISC-V programs

The [binaries folder](/binaries/) contains several example programs.

The [newlib](/binaries/newlib) and [newlib64](/binaries/newlib64) example projects have much more C and C++ support, but still misses things like environment variables and such. This is a deliberate design as newlib is intended for embedded development. It supports C++ RTTI and exceptions, and is the best middle-ground for running a fuller C++ environment that still produces small binaries. You can run these programs with rvnewlib.

The [linux](/binaries/linux) and [linux64](/binaries/linux64) example projects require a Linux-configured cross compiler. You can run these programs with rvlinux.

The [Go](/binaries/go) examples only require Go installed. Go produces complex RV64G ELF executables.

There are also examples for [Nim](/binaries/nim), [Zig](/binaries/zig) and [Rust](/binaries/rust).

## Remote debugging using GDB

If you have built the emulator, you can use `GDB=1 ./rvlinux /path/to/program` to enable GDB to connect. Most distros have `gdb-multiarch`, which is a separate program from the default gdb. It will have RISC-V support already built in. Start your GDB like so: `gdb-multiarch /path/to/program`. Make sure your program is built with -O0 and with debuginfo present. Then, once in GDB connect with `target remote localhost:2159`. Now you can step through the code.

Most modern languages embed their own pretty printers for debuginfo which enables you to go line by line in your favorite language.

## Instruction set support

The emulator currently supports RV32GC, RV64GC (IMAFDC) and RV128G.
The F and D-extensions should be 100% supported (32- and 64-bit floating point instructions). Atomics support is present and has been tested with multiprocessing, but there is no extensive test suite. The Golang runtime uses atomics extensively.

B-extension support is currently being implemented. Zba and Zbb is experimentally supported. In order to test this, build with `-march=rv64g_zba_zbb`. There is an [ELF verification test](/tests/unit/verify_elf.cpp).

The 128-bit ISA support is experimental, and the specification is not yet complete. There is neither toolchain support, nor is there an ELF format for 128-bit machines. There is an emulator that specifically runs a custom crafted 128-bit program in the [emu128 folder](/emu128/).

## Example usage when embedded into a project

Load a Linux program built for RISC-V and run through main:
```C++
#include <libriscv/machine.hpp>

int main(int /*argc*/, const char** /*argv*/)
{
	// Load ELF binary from file
	const std::vector<uint8_t> binary /* = ... */;

	using namespace riscv;

	// Create a 64-bit machine (with default options, see: libriscv/common.hpp)
	Machine<RISCV64> machine { binary };

	// Add program arguments on the stack, and set a few basic
	// environment variables.
	machine.setup_linux(
		{"myprogram", "1st argument!", "2nd argument!"},
		{"LC_TYPE=C", "LC_ALL=C", "USER=root"});

	// Add all the basic Linux system calls.
	// This includes `exit` and `exit_group` which we will override below.
	machine.setup_linux_syscalls();

	// Install our own `exit` system call handler (for all 64-bit machines).
	Machine<RISCV64>::install_syscall_handler(93, // exit
		[] (Machine<RISCV64>& machine) {
			const int code = machine.sysarg <int> (0);
			printf(">>> Program exited, exit code = %d\n", code);
			machine.stop();
		});
	// We also use the same system call handler again for `exit_group`,
	// which is another way that C libraries will use to end the process.
	Machine<RISCV64>::install_syscall_handler(94, // exit_group
		Machine<RISCV64>::syscall_handlers.at(93));

	// This function will run until the exit syscall has stopped the
	// machine, an exception happens which stops execution, or the
	// instruction counter reaches the given 1M instruction limit:
	try {
		machine.simulate(1'000'000UL);
	} catch (const std::exception& e) {
		fprintf(stderr, ">>> Runtime exception: %s\n", e.what());
	}
}
```

In order to have the machine not throw an exception when the instruction limit is reached, you can call simulate with the template argument false, instead:

```C++
machine.simulate<false>(1'000'000UL);
```
If the machine runs out of instructions, it will simply stop running. Use `machine.instruction_limit_reached()` to check if the machine stopped running because it hit the instruction limit.

You can limit the amount of (virtual) memory the machine can use like so:
```C++
	const uint32_t memsize = 1024 * 1024 * 64u;
	riscv::Machine<riscv::RISCV32> machine { binary, { .memory_max = memsize } };
```
You can find the `MachineOptions` structure in [common.hpp](/lib/libriscv/common.hpp).

You can find details on the Linux system call ABI online as well as in [the docs](/docs/SYSCALLS.md). You can use these examples to handle system calls in your RISC-V programs. The system calls emulate normal Linux system calls, and is compatible with a normal Linux RISC-V compiler.

## Handling instructions one by one

You can create your own custom instruction loop if you want to do things manually by yourself:

```C++
#include <libriscv/machine.hpp>
#include <libriscv/rv32i_instr.hpp>
...
Machine<RISCV64> machine{binary};
machine.setup_linux(
	{"myprogram"},
	{"LC_TYPE=C", "LC_ALL=C", "USER=root"});
machine.setup_linux_syscalls();

// Instruction limit is used to keep running
machine.set_max_instructions(1'000'000UL);

while (!machine.stopped()) {
	auto& cpu = machine.cpu;
	// Read next instruction
	const auto instruction = cpu.read_next_instruction();
	// Print the instruction to terminal
	printf("%s\n", cpu.to_string(instruction).c_str());
	// Execute instruction directly
	cpu.execute(instruction);
	// Increment PC to next instruction, and increment instruction counter
	cpu.increment_pc(instruction.length());
	machine.increment_counter(1);
}
```

## Executing the program in small increments

If we only want to run for a small amount of time and then leave the simulation, we can use the same example as above with an outer loop to keep it running as long as we want to until the machine stops normally.
```C++
	do {
		// Only execute 1000 instructions at a time
		machine.reset_instruction_counter();
		machine.set_max_instructions(1'000);

		while (!machine.stopped())
		{
			auto& cpu = machine.cpu;
			// Read next instruction
			const auto instruction = cpu.read_next_instruction();
			// Print the instruction to terminal
			printf("%s\n", cpu.to_string(instruction).c_str());
			// Execute instruction directly
			cpu.execute(instruction);
			// Increment PC to next instruction, and increment instruction counter
			cpu.increment_pc(instruction.length());
			machine.increment_counter(1);
		}

	} while (machine.instruction_limit_reached());
```
The function `machine.instruction_limit_reached()` only returns true when the instruction limit was reached, and not if the machine stops normally. Using that we can keep going until either the machine stops, or an exception is thrown.

## Setting up your own machine environment

You can create a 64kb machine without a binary, and no ELF loader will be invoked.

```C++
	Machine<RISCV32> machine;
	machine.setup_minimal_syscalls();

	std::vector<uint32_t> my_program {
		0x29a00513, //        li      a0,666
		0x05d00893, //        li      a7,93
		0x00000073, //        ecall
	};

	// Set main execute segment (12 instruction bytes)
	const uint32_t dst = 0x1000;
	machine.cpu.init_execute_area(my_program.data(), dst, 12);

	// Jump to the start instruction
	machine.cpu.jump(dst);

	// Geronimo!
	machine.simulate(1'000ul);
```

The fuzzing program does this, so have a look at that. There is also [a unit test](/tests/unit/micro.cpp).

## Adding your own instructions

See [this unit test](/tests/unit/custom.cpp) for an example on how to add your own instructions. They work in all simulation modes.

## Documentation

[System calls](docs/SYSCALLS.md)

[Freestanding environments](docs/FREESTANDING.md)

[Function calls into the VM](docs/VMCALL.md)

[Debugging with libriscv](docs/DEBUGGING.md)

[Example programs](/examples)

## Why a RISC-V library

It's a drop-in sandbox. Perhaps you want someone to be able to execute C/C++ code on a website, safely? It can step through RISC-V programs line by line showing registers and memory locations. It also has some extra features that allow you to make function calls into the guest program. I think it's pretty cool stuff.

## What to use for performance

Building the fastest possible RISC-V binaries for libriscv is a hard problem, but I am working on that in my [rvscript](https://github.com/fwsGonzo/rvscript) repository. It's a complex topic that cannot be explained in one paragraph.

If you have arenas available you can replace the default page fault handler with your own that allocates faster than regular heap. If you intend to use many (read hundreds, thousands) of machines in parallel, you absolutely must use the Machine forking constructor. It will apply copy-on-write to all pages on the newly created machine and share text, rodata and the instruction cache. It's also possible to use fault handlers while foregoing even the copy-on-write process in order to reduce the forking time.

## Multiprocessing

There is multiprocessing support, but it is in its early stages. It is achieved by calling a (C/SYSV ABI) function on many machines, with differing CPU IDs. The input data to be processed should exist beforehand. It is not well tested, and potential page table races are not well understood. That said, it passes manual testing and there is a unit test for the basic cases.

## Binary translation

Instead of JIT, the emulator supports translating binaries to native code using any local C compiler. You can control compilation by passing CC and CFLAGS environment variables to the program that runs the emulator. You can show the compiler arguments using VERBOSE=1. Example: `CFLAGS=-O2 VERBOSE=1 ./myemulator`.

The binary translation feature (accessible by enabling the RISCV_EXPERIMENTAL CMake option) can greatly improve performance in some cases, but requires compiling the program on the first run. The RISC-V binary is scanned for code blocks that are safe to translate, and then a C compiler is invoked on the generated code. This step takes a long time. The resulting code is then dynamically loaded and ready to use. The feature is a work in progress.
