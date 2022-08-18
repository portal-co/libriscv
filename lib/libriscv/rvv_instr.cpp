#include "rvv.hpp"
#include "instr_helpers.hpp"

namespace riscv
{
	static const char *VOPNAMES[3][64] = {
		{"VADD", "???", "VSUB", "VRSUB", "VMINU", "VMIN", "VMAXU", "VMAX", "???", "VAND", "VOR", "VXOR", "VRGATHER", "???", "VSLIDEUP", "VSLIDEDOWN",
		 "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???"
		 "VSADDU", "VSADD", "VSSUBU", "VSSUB", "???", "VSLL", "???", "VSMUL", "VSRL", "VSRA", "VSSRL", "VSSRA", "VNSLR", "VNSRA", "VNCLIPU", "VNCLIP",
		 "VWREDSUMU", "VWREDSUM", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???"},
		{"VREDSUM", "VREDAND", "VREDOR", "VREDXOR", "VREDMINU", "VREDMIN", "VREDMAXU", "VREDMAX", "VAADDU", "VAADD", "VASUBU", "VASUB", "???", "???", "VSLIDE1UP", "VSLIDE1DOWN",
		 "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???"
		 "VDIVU", "VDIV", "VREMU", "VREM", "VMULHU", "VMUL", "VMULHSU", "VMULH", "???", "VMADD", "???", "VNMSUB", "???", "VMACC", "VNMSAC",
		  "VWADDU", "VWADD", "VWSUBU", "VWSUB", "VWADDU.W", "VWADD.W", "VWSUBU.W", "VWSUB.W", "VWMULU", "???", "VWMULSU", "VWMUL", "VWMACCU", "VWMACC", "VWMACCUS", "VWMACCSU"},
		{"VFADD", "VFREDUSUM", "VFSUB", "VFREDOSUM", "VFMIN", "VFREDMIN", "VFMAX", "VFREDMAX", "VFSGNJ", "VFSGNJ.N", "VFSGNJ.X", "???", "???", "???", "VFSLIDE1UP", "VFSLIDE1DOWN",
		 "VWFUNARY0", "???", "VFUNARY0", "VFUNARY1", "???", "???", "???", "VFMERGE", "VMFEQ", "MVFLE", "???", "VMFLT", "VMFNE", "VMFGT", "???", "VMFGE",
		 "VFDIV", "VFRDIV", "???", "???", "VFMUL", "???", "???", "VFRSUB", "VFMADD", "VFNMADD", "VFMSUB", "VFNMSUB", "VFMACC", "VFNMACC", "VFMSAC", "VFNMSAC",
		 "VFWADD", "VFWREDUSUM", "VFWSUB", "VFWREDOSUM", "VFWADD.W", "???", "VFWSUB.W", "???", "VFWMUL", "???", "???", "???", "VFWMACC", "VFWNMACC", "VFWMSAC", "VFWNMSAC"},
		};

	VECTOR_INSTR(VSETVLI,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VSETVLI %s, %s, 0x%X",
						RISCV::regname(vi.VLI.rd),
						RISCV::regname(vi.VLI.rs1),
						vi.VLI.zimm);
	});

	VECTOR_INSTR(VSETIVLI,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VSETIVLI %s, 0x%X, 0x%X",
						RISCV::regname(vi.IVLI.rd),
						vi.IVLI.uimm,
						vi.IVLI.zimm);
	});

	VECTOR_INSTR(VSETVL,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VSETVL %s, %s, %s",
						RISCV::regname(vi.VSETVL.rd),
						RISCV::regname(vi.VSETVL.rs1),
						RISCV::regname(vi.VSETVL.rs2));
	});

	VECTOR_INSTR(VLE32,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		const auto addr = cpu.reg(vi.VLS.rs1);
		if (addr % VectorLane::size() == 0) {
			auto& rvv = cpu.registers().rvv();
			cpu.machine().memory.memcpy_out(&rvv.f32(vi.VLS.vd), addr, VectorLane::size());
		} else {
			cpu.trigger_exception(INVALID_ALIGNMENT, addr);
		}
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VLE32.V %s, %s, %s",
						RISCV::vecname(vi.VLS.vd),
						RISCV::regname(vi.VLS.rs1),
						RISCV::regname(vi.VLS.rs2));
	});

	VECTOR_INSTR(VSE32,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		const auto addr = cpu.reg(vi.VLS.rs1);
		if (addr % VectorLane::size() == 0) {
			auto& rvv = cpu.registers().rvv();
			cpu.machine().copy_to_guest(addr, &rvv.f32(vi.VLS.vd), VectorLane::size());
		} else {
			cpu.trigger_exception(INVALID_ALIGNMENT, addr);
		}
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VSE32.V %s, %s, %s",
						RISCV::vecname(vi.VLS.vd),
						RISCV::regname(vi.VLS.rs1),
						RISCV::regname(vi.VLS.rs2));
	});

	VECTOR_INSTR(VOPI_VV,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		auto& rvv = cpu.registers().rvv();
		switch (vi.OPVV.funct6) {
		case 0b000000: // VADD
			for (size_t i = 0; i < rvv.u32(0).size(); i++) {
				rvv.u32(vi.OPVV.vd)[i] = rvv.u32(vi.OPVV.vs1)[i] + rvv.u32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b000010: // VSUB
			for (size_t i = 0; i < rvv.u32(0).size(); i++) {
				rvv.u32(vi.OPVV.vd)[i] = rvv.u32(vi.OPVV.vs1)[i] - rvv.u32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b001001: // VAND
			for (size_t i = 0; i < rvv.u32(0).size(); i++) {
				rvv.u32(vi.OPVV.vd)[i] = rvv.u32(vi.OPVV.vs1)[i] & rvv.u32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b001010: // VOR
			for (size_t i = 0; i < rvv.u32(0).size(); i++) {
				rvv.u32(vi.OPVV.vd)[i] = rvv.u32(vi.OPVV.vs1)[i] | rvv.u32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b001011: // VXOR
			for (size_t i = 0; i < rvv.u32(0).size(); i++) {
				rvv.u32(vi.OPVV.vd)[i] = rvv.u32(vi.OPVV.vs1)[i] ^ rvv.u32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b001100: // VRGATHER
			for (size_t i = 0; i < rvv.u32(0).size(); i++) {
				const auto vs1 = rvv.u32(vi.OPVV.vs1)[i];
				rvv.u32(vi.OPVV.vd)[i] = (vs1 >= rvv.u32(0).size()) ? 0 : rvv.u32(vi.OPVV.vs2)[vs1];
			}
			break;
		default:
			cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
		}
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "%s %s, %s, %s",
						VOPNAMES[0][vi.OPVV.funct6],
						RISCV::vecname(vi.VLS.vd),
						RISCV::regname(vi.VLS.rs1),
						RISCV::regname(vi.VLS.rs2));
	});

	VECTOR_INSTR(VOPF_VV,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		auto& rvv = cpu.registers().rvv();
		switch (vi.OPVV.funct6) {
		case 0b000000: // VFADD
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				rvv.f32(vi.OPVV.vd)[i] = rvv.f32(vi.OPVV.vs1)[i] + rvv.f32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b000001:   // VFREDUSUM
		case 0b000011: { // VFREDOSUM
			float sum = 0.0f;
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				sum += rvv.f32(vi.OPVV.vs1)[i] + rvv.f32(vi.OPVV.vs2)[i];
			}
			rvv.f32(vi.OPVV.vd)[0] = sum;
			} break;
		case 0b000010: // VFSUB
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				rvv.f32(vi.OPVV.vd)[i] = rvv.f32(vi.OPVV.vs1)[i] - rvv.f32(vi.OPVV.vs2)[i];
			}
			break;
		case 0b100100: // VFMUL
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				rvv.f32(vi.OPVV.vd)[i] = rvv.f32(vi.OPVV.vs1)[i] * rvv.f32(vi.OPVV.vs2)[i];
			}
			break;
		default:
			cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
		}
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "%s.VV %s, %s, %s",
						VOPNAMES[2][vi.OPVV.funct6],
						RISCV::vecname(vi.OPVV.vd),
						RISCV::vecname(vi.OPVV.vs1),
						RISCV::vecname(vi.OPVV.vs2));
	});

	VECTOR_INSTR(VOPM_VV,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VOPM.VV %s, %s, %s",
						RISCV::vecname(vi.VLS.vd),
						RISCV::regname(vi.VLS.rs1),
						RISCV::regname(vi.VLS.rs2));
	});

	VECTOR_INSTR(VOPF_VF,
	[] (auto& cpu, rv32i_instruction instr)
	{
		const rv32v_instruction vi { instr };
		auto& rvv = cpu.registers().rvv();
		const float scalar = cpu.registers().getfl(vi.OPVV.vs1).f32[0];
		const auto vector = vi.OPVV.vs2;
		switch (vi.OPVV.funct6) {
		case 0b000000: // VFADD.VF
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				rvv.f32(vi.OPVV.vd)[i] = rvv.f32(vector)[i] + scalar;
			}
			break;
		case 0b000001:   // VFREDUSUM.VF
		case 0b000011: { // VFREDOSUM.VF
			float sum = 0.0f;
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				sum += rvv.f32(vector)[i] + scalar;
			}
			rvv.f32(vi.OPVV.vd)[0] = sum;
			} break;
		case 0b000010: // VFSUB.VF
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				rvv.f32(vi.OPVV.vd)[i] = rvv.f32(vector)[i] - scalar;
			}
			break;
		case 0b100100: // VFMUL.VF
			for (size_t i = 0; i < rvv.f32(0).size(); i++) {
				rvv.f32(vi.OPVV.vd)[i] = rvv.f32(vector)[i] * scalar;
			}
			break;
		default:
			cpu.trigger_exception(UNIMPLEMENTED_INSTRUCTION);
		}
	},
	[] (char* buffer, size_t len, auto&, rv32i_instruction instr) RVPRINTR_ATTR {
		const rv32v_instruction vi { instr };
		return snprintf(buffer, len, "VOPF.VF %s, %s, %s",
						RISCV::vecname(vi.VLS.vd),
						RISCV::regname(vi.VLS.rs1),
						RISCV::regname(vi.VLS.rs2));
	});
}
