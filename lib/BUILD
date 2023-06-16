package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "riscv",
    srcs = [
        "libriscv/cpu.cpp",
        "libriscv/debug.cpp",
        "libriscv/decode_bytecodes.cpp",
        "libriscv/decoder_cache.cpp",
        "libriscv/machine.cpp",
        "libriscv/memory.cpp",
        "libriscv/memory_elf.cpp",
        "libriscv/memory_rw.cpp",
        "libriscv/multiprocessing.cpp",
        "libriscv/native_libc.cpp",
        "libriscv/native_threads.cpp",
        "libriscv/posix/minimal.cpp",
        "libriscv/posix/signals.cpp",
        "libriscv/posix/threads.cpp",
        "libriscv/posix/socket_calls.cpp",
        "libriscv/rv32i.cpp",
        "libriscv/rv64i.cpp",
        "libriscv/serialize.cpp",
        "libriscv/util/crc32c.cpp",
    ] + select({
        "@platforms//os:linux": ["libriscv/linux/system_calls.cpp"],
        "@platforms//os:windows": ["libriscv/win32/system_calls.cpp"],
    }) + ["libriscv/threaded_dispatch.cpp"],
    hdrs = glob([
        "**/*.hpp",
        "**/*.h",
        "**/*.cpp",
        "**/*.inc",
    ]),
    includes = ["."],
)
