#include "asm_trans.h"
#include "enc_i286.h"

namespace Engine
{
	namespace XN
	{
		enum class TokenClass { EOF = 0, EOL = 1, Name = 2, Symbol = 3, Number = 4, String = 5 };
		enum class ArgumentClass { Register = 0, Immediate = 1, Memory = 2 };
		struct Token
		{
			uint offset;
			TokenClass cls;
			string text;
			int number;
		};
		struct Label
		{
			bool offset_is_32;
			uint offset;
			uint segment_relocate;
		};
		struct OperationArgument
		{
			ArgumentClass cls;
			string label_ref;
			uint value_offset;
			union {
				struct {
					Reg reg;
				} reg;
				struct {
					Reg offset_reg;
					Reg segment_reg;
					int offset;
				} mem;
				struct {
					int value;
				} imm;
			};
		};
		struct JumpDefinition
		{
			string label;
			uint64 explicit_address;
			uint address_position, opcode_end_position;
			uint quant; // 0 - no jump, 1 - short local, 2 - full local, 4 - far
			bool relative, far, addr_mod_change;
		};
		AssemblyErrorCode EncodeOperation(const string & name, Array<OperationArgument> & args, i286_EncoderContext & ctx, JumpDefinition & jd)
		{
			jd.address_position = jd.opcode_end_position = jd.quant = 0;
			jd.relative = false;
			if (name[0] == L'A') {
				if (name == L"ADC") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::ADC, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::ADC, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::ADC, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::ADC, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"ADD") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::ADD, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::ADD, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::ADD, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::ADD, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"AND") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::AND, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::AND, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::AND, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::AND, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'C') {
				if (name == L"CALL") {
					bool address_32 = ctx.is_32_bit_mode() != jd.addr_mod_change;
					auto quant = address_32 ? (jd.far ? 6 : 4) : (jd.far ? 4 : 2);
					if (args.Length() == 0) {
						jd.quant = quant;
						ctx.encode_call(jd.quant);
						jd.opcode_end_position = ctx.get_current_position();
						jd.address_position = jd.opcode_end_position - jd.quant;
						jd.relative = !jd.far;
					} else if (args.Length() == 1) {
						if (args[0].cls == ArgumentClass::Register) {
							ctx.encode_call(ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
						} else if (args[0].cls == ArgumentClass::Memory) {
							ctx.encode_call(quant, args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset);
							args[0].value_offset = ctx.get_current_position();
						}
						jd.quant = 0;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"CBW") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_cbw();
					return AssemblyErrorCode::Success;
				} else if (name == L"CLI") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_cli();
					return AssemblyErrorCode::Success;
				} else if (name == L"CMP") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::CMP, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::CMP, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::CMP, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::CMP, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"CWD") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_cwd();
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'D') {
				if (name == L"DEC") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_uop_reg(UOP::DEC, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else if (name == L"DIV") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_mulop_reg(MULOP::DIV, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name == L"HLT") {
				if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
				ctx.encode_hlt();
				return AssemblyErrorCode::Success;
			} else if (name[0] == L'I') {
				if (name == L"IDIV") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_mulop_reg(MULOP::IDIV, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else if (name == L"IMUL") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_mulop_reg(MULOP::IMUL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else if (name == L"IN") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_in(args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_in(args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"INC") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_uop_reg(UOP::INC, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else if (name == L"INT3") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_int3();
					return AssemblyErrorCode::Success;
				} else if (name == L"INTO") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_into();
					return AssemblyErrorCode::Success;
				} else if (name == L"INT") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Immediate) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_int(args[0].imm.value);
					return AssemblyErrorCode::Success;
				} else if (name == L"IRET") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_iret();
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'J') {
				if (name == L"JMP") {
					bool address_32 = ctx.is_32_bit_mode() != jd.addr_mod_change;
					auto quant = address_32 ? (jd.far ? 6 : 4) : (jd.far ? 4 : 2);
					if (args.Length() == 0) {
						jd.quant = quant;
						ctx.encode_jmp(jd.quant);
						jd.opcode_end_position = ctx.get_current_position();
						jd.address_position = jd.opcode_end_position - jd.quant;
						jd.relative = !jd.far;
					} else if (args.Length() == 1) {
						if (args[0].cls == ArgumentClass::Register) {
							ctx.encode_jmp(ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
						} else if (args[0].cls == ArgumentClass::Memory) {
							ctx.encode_jmp(quant, args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset);
							args[0].value_offset = ctx.get_current_position();
						}
						jd.quant = 0;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else {
					if (args.Length() != 0 || jd.far) return AssemblyErrorCode::IllegalEncoding;
					if (ctx.is_i386_mode()) { jd.quant = ctx.is_32_bit_mode() ? 4 : 2; } else jd.quant = 1;
					uint dta = 0x0F;
					if (jd.quant > 1) ctx.encode_data(&dta, 1);
					if (name == L"JA") ctx.encode_jcc(JCC::JA);
					else if (name == L"JAE") ctx.encode_jcc(JCC::JAE);
					else if (name == L"JB") ctx.encode_jcc(JCC::JB);
					else if (name == L"JBE") ctx.encode_jcc(JCC::JBE);
					else if (name == L"JC") ctx.encode_jcc(JCC::JC);
					else if (name == L"JE") ctx.encode_jcc(JCC::JE);
					else if (name == L"JG") ctx.encode_jcc(JCC::JG);
					else if (name == L"JGE") ctx.encode_jcc(JCC::JGE);
					else if (name == L"JL") ctx.encode_jcc(JCC::JL);
					else if (name == L"JLE") ctx.encode_jcc(JCC::JLE);
					else if (name == L"JNA") ctx.encode_jcc(JCC::JNA);
					else if (name == L"JNAE") ctx.encode_jcc(JCC::JNAE);
					else if (name == L"JNB") ctx.encode_jcc(JCC::JNB);
					else if (name == L"JNBE") ctx.encode_jcc(JCC::JNBE);
					else if (name == L"JNC") ctx.encode_jcc(JCC::JNC);
					else if (name == L"JNE") ctx.encode_jcc(JCC::JNE);
					else if (name == L"JNG") ctx.encode_jcc(JCC::JNG);
					else if (name == L"JNGE") ctx.encode_jcc(JCC::JNGE);
					else if (name == L"JNL") ctx.encode_jcc(JCC::JNL);
					else if (name == L"JNLE") ctx.encode_jcc(JCC::JNLE);
					else if (name == L"JNO") ctx.encode_jcc(JCC::JNO);
					else if (name == L"JNP") ctx.encode_jcc(JCC::JNP);
					else if (name == L"JNS") ctx.encode_jcc(JCC::JNS);
					else if (name == L"JNZ") ctx.encode_jcc(JCC::JNZ);
					else if (name == L"JO") ctx.encode_jcc(JCC::JO);
					else if (name == L"JP") ctx.encode_jcc(JCC::JP);
					else if (name == L"JPE") ctx.encode_jcc(JCC::JPE);
					else if (name == L"JPO") ctx.encode_jcc(JCC::JPO);
					else if (name == L"JS") ctx.encode_jcc(JCC::JS);
					else if (name == L"JZ") ctx.encode_jcc(JCC::JZ);
					else return AssemblyErrorCode::UnknownInstruction;
					if (jd.quant > 1) {
						dta = 0;
						ctx.encode_data(&dta, jd.quant - 1);
						uint8 & opcode = ctx.get_destination()[ctx.get_current_position() - jd.quant - 1];
						opcode &= 0x0F;
						opcode |= 0x80;
					}
					jd.opcode_end_position = ctx.get_current_position();
					jd.address_position = jd.opcode_end_position - jd.quant;
					jd.relative = true;
					return AssemblyErrorCode::Success;
				}
			} else if (name[0] == L'L') {
				if (name == L"LAHF") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_lahf();
					return AssemblyErrorCode::Success;
				} else if (name == L"LDS") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls != ArgumentClass::Register || args[1].cls != ArgumentClass::Memory) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_lds(args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
					args[1].value_offset = ctx.get_current_position();
					return AssemblyErrorCode::Success;
				} else if (name == L"LES") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls != ArgumentClass::Register || args[1].cls != ArgumentClass::Memory) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_les(args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
					args[1].value_offset = ctx.get_current_position();
					return AssemblyErrorCode::Success;
				} else if (name == L"LEA") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls != ArgumentClass::Register || args[1].cls != ArgumentClass::Memory) return AssemblyErrorCode::IllegalEncoding;
					if (args[1].mem.segment_reg != Reg::NO) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_lea(args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.offset);
					args[1].value_offset = ctx.get_current_position();
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'M') {
				if (name == L"MOV") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_mov_reg_reg(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_mov_reg_mem(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_mov_reg_imm(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_mov_mem_reg(ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"MUL") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_mulop_reg(MULOP::MUL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'N') {
				if (name == L"NEG") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_mulop_reg(MULOP::NEG, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else if (name == L"NOP") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_nop();
					return AssemblyErrorCode::Success;
				} else if (name == L"NOT") {
					if (args.Length() != 1 || args[0].cls != ArgumentClass::Register) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_mulop_reg(MULOP::NOT, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'O') {
				if (name == L"OR") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::OR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::OR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::OR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::OR, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"OUT") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[1].cls == ArgumentClass::Register) {
							if (args[0].cls == ArgumentClass::Register) {
								ctx.encode_out(args[1].reg.reg, args[0].reg.reg);
							} else if (args[0].cls == ArgumentClass::Immediate) {
								ctx.encode_out(args[1].reg.reg, args[0].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'P') {
				if (name == L"POP") {
					if (args.Length() == 1 && args[0].cls == ArgumentClass::Register) {
						try { ctx.encode_pop(args[0].reg.reg); } catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						return AssemblyErrorCode::Success;
					} else return AssemblyErrorCode::IllegalEncoding;
				} else if (name == L"POPA") {
					if (args.Length() == 0) {
						ctx.encode_pop_all();
						return AssemblyErrorCode::Success;
					} else return AssemblyErrorCode::IllegalEncoding;
				} else if (name == L"POPF") {
					if (args.Length() == 0) {
						ctx.encode_pop_flags();
						return AssemblyErrorCode::Success;
					} else return AssemblyErrorCode::IllegalEncoding;
				} else if (name == L"PUSH") {
					if (args.Length() == 1 && args[0].cls == ArgumentClass::Register) {
						try { ctx.encode_push(args[0].reg.reg); } catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						return AssemblyErrorCode::Success;
					} else if (args.Length() == 1 && args[0].cls == ArgumentClass::Immediate) {
						try { ctx.encode_push(args[0].imm.value); } catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						return AssemblyErrorCode::Success;
					} else return AssemblyErrorCode::IllegalEncoding;
				} else if (name == L"PUSHA") {
					if (args.Length() == 0) {
						ctx.encode_push_all();
						return AssemblyErrorCode::Success;
					} else return AssemblyErrorCode::IllegalEncoding;
				} else if (name == L"PUSHF") {
					if (args.Length() == 0) {
						ctx.encode_push_flags();
						return AssemblyErrorCode::Success;
					} else return AssemblyErrorCode::IllegalEncoding;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'R') {
				if (name == L"RCL") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::RCL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::RCL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::RCL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"RCR") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::RCR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::RCR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::RCR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"ROL") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::ROL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::ROL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::ROL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"ROR") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::ROR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::ROR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::ROR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"RET") {
					if (args.Length() == 1 && args[0].cls == ArgumentClass::Immediate) {
						ctx.encode_ret(args[0].imm.value, jd.far);
					} else if (args.Length() == 0) {
						ctx.encode_ret(0, jd.far);
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name[0] == L'S') {
				if (name == L"SAHF") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_sahf();
					return AssemblyErrorCode::Success;
				} else if (name == L"SAL") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::SAL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::SAL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::SAL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"SAR") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::SAR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::SAR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::SAR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"SHL") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::SHL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::SHL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::SHL, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"SHR") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							try { ctx.encode_shop_reg(SHOP::SHR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg); }
							catch (...) { return AssemblyErrorCode::IllegalEncoding; }
						} else if (args[1].cls == ArgumentClass::Immediate) {
							if (args[1].imm.value == 1) ctx.encode_shop_reg(SHOP::SHR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg);
							else ctx.encode_shop_reg(SHOP::SHR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else return AssemblyErrorCode::IllegalEncoding;
					return AssemblyErrorCode::Success;
				} else if (name == L"SBB") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::SBB, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::SBB, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::SBB, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::SBB, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"STI") {
					if (args.Length()) return AssemblyErrorCode::IllegalEncoding;
					ctx.encode_sti();
					return AssemblyErrorCode::Success;
				} else if (name == L"SUB") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::SUB, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::SUB, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::SUB, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::SUB, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else if (name == L"TEST") {
				if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
				try {
					if (args[0].cls == ArgumentClass::Register) {
						if (args[1].cls == ArgumentClass::Register) {
							ctx.encode_test_reg_reg(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
						} else if (args[1].cls == ArgumentClass::Immediate) {
							ctx.encode_test_reg_imm(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
						} else return AssemblyErrorCode::IllegalEncoding;
					} else if (args[0].cls == ArgumentClass::Memory) {
						if (args[1].cls == ArgumentClass::Register) {
							ctx.encode_test_mem_reg(ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
						} else return AssemblyErrorCode::IllegalEncoding;
						args[0].value_offset = ctx.get_current_position();
					} else return AssemblyErrorCode::IllegalEncoding;
				} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
				return AssemblyErrorCode::Success;
			} else if (name[0] == L'X') {
				if (name == L"XCHG") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_xchg_reg_reg(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_xchg_reg_mem(ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_xchg_reg_mem(ctx.register_quant(args[1].reg.reg), args[1].reg.reg, args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else if (name == L"XOR") {
					if (args.Length() != 2) return AssemblyErrorCode::IllegalEncoding;
					try {
						if (args[0].cls == ArgumentClass::Register) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_reg_reg(AROP::XOR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].reg.reg);
							} else if (args[1].cls == ArgumentClass::Memory) {
								ctx.encode_arop_reg_mem(AROP::XOR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].mem.offset_reg, args[1].mem.segment_reg, args[1].mem.offset);
								args[1].value_offset = ctx.get_current_position();
							} else if (args[1].cls == ArgumentClass::Immediate) {
								ctx.encode_arop_reg_imm(AROP::XOR, ctx.register_quant(args[0].reg.reg), args[0].reg.reg, args[1].imm.value);
							} else return AssemblyErrorCode::IllegalEncoding;
						} else if (args[0].cls == ArgumentClass::Memory) {
							if (args[1].cls == ArgumentClass::Register) {
								ctx.encode_arop_mem_reg(AROP::XOR, ctx.register_quant(args[1].reg.reg), args[0].mem.offset_reg, args[0].mem.segment_reg, args[0].mem.offset, args[1].reg.reg);
							} else return AssemblyErrorCode::IllegalEncoding;
							args[0].value_offset = ctx.get_current_position();
						} else return AssemblyErrorCode::IllegalEncoding;
					} catch (...) { return AssemblyErrorCode::IllegalEncoding; }
					return AssemblyErrorCode::Success;
				} else return AssemblyErrorCode::UnknownInstruction;
			} else return AssemblyErrorCode::UnknownInstruction;
		}
		Reg AsRegisterName(const string & name)
		{
			if (name.Length() == 2) {
				if (name == L"AX") return Reg::AX;
				else if (name == L"CX") return Reg::CX;
				else if (name == L"DX") return Reg::DX;
				else if (name == L"BX") return Reg::BX;
				else if (name == L"SP") return Reg::SP;
				else if (name == L"BP") return Reg::BP;
				else if (name == L"SI") return Reg::SI;
				else if (name == L"DI") return Reg::DI;
				else if (name == L"AL") return Reg::AL;
				else if (name == L"CL") return Reg::CL;
				else if (name == L"DL") return Reg::DL;
				else if (name == L"BL") return Reg::BL;
				else if (name == L"AH") return Reg::AH;
				else if (name == L"CH") return Reg::CH;
				else if (name == L"DH") return Reg::DH;
				else if (name == L"BH") return Reg::BH;
				else if (name == L"CS") return Reg::CS;
				else if (name == L"DS") return Reg::DS;
				else if (name == L"ES") return Reg::ES;
				else if (name == L"SS") return Reg::SS;
				else return Reg::NO;
			} else if (name.Length() == 3) {
				if (name == L"EAX") return Reg::EAX;
				else if (name == L"ECX") return Reg::ECX;
				else if (name == L"EDX") return Reg::EDX;
				else if (name == L"EBX") return Reg::EBX;
				else if (name == L"ESP") return Reg::ESP;
				else if (name == L"EBP") return Reg::EBP;
				else if (name == L"ESI") return Reg::ESI;
				else if (name == L"EDI") return Reg::EDI;
				else if (name == L"CR0") return Reg::CR0;
				else if (name == L"CR1") return Reg::CR1;
				else if (name == L"CR2") return Reg::CR2;
				else if (name == L"CR3") return Reg::CR3;
				else if (name == L"CR4") return Reg::CR4;
				else if (name == L"CR5") return Reg::CR5;
				else if (name == L"CR6") return Reg::CR6;
				else if (name == L"CR7") return Reg::CR7;
				else return Reg::NO;
			} else return Reg::NO;
		}
		bool ReadToken(const string & input, uint length, uint & pos, Token & token)
		{
			while (true) {
				while (pos < length && (input[pos] == L' ' || input[pos] == L'\t' || input[pos] == L'\r')) pos++;
				if (pos == length) {
					token.cls = TokenClass::EOF;
					token.text = L"";
					token.number = 0;
					token.offset = pos;
					return true;
				} else {
					if (input[pos] == L'\n') {
						token.cls = TokenClass::EOL;
						token.text = L"";
						token.number = 0;
						token.offset = pos;
						pos++;
						return true;
					} else if (input[pos] == L'[' || input[pos] == L']' || input[pos] == L'+' || input[pos] == L':' || input[pos] == L'#' || input[pos] == L'@' || input[pos] == L',') {
						token.cls = TokenClass::Symbol;
						token.text = input.Fragment(pos, 1);
						token.number = 0;
						token.offset = pos;
						pos++;
						return true;
					} else if (input[pos] == L';') {
						while (pos < length && input[pos] != L'\n') pos++;
					} else if ((input[pos] >= L'0' && input[pos] <= L'9') || input[pos] == L'-') {
						uint as = pos;
						int sgn = 1;
						if (input[pos] == L'-') { sgn = -1; pos++; }
						uint s = pos;
						while (pos < length && ((input[pos] >= L'A' && input[pos] <= L'Z') ||
							(input[pos] >= L'a' && input[pos] <= L'z') || (input[pos] >= L'0' && input[pos] <= L'9') || input[pos] == L'_')) pos++;
						token.text = input.Fragment(s, pos - s).LowerCase();
						token.cls = TokenClass::Number;
						token.offset = as;
						try {
							if (token.text.Fragment(0, 2) == L"0x") {
								token.number = sgn * int(token.text.Fragment(2, -1).ToUInt32(HexadecimalBase));
							} else if (token.text.Fragment(0, 2) == L"0o") {
								token.number = sgn * int(token.text.Fragment(2, -1).ToUInt32(OctalBase));
							} else if (token.text.Fragment(0, 2) == L"0b") {
								token.number = sgn * int(token.text.Fragment(2, -1).ToUInt32(BinaryBase));
							} else {
								token.number = sgn * int(token.text.ToUInt32());
							}
						} catch (...) { pos = s; return false; }
						return true;
					} else if ((input[pos] >= L'A' && input[pos] <= L'Z') || (input[pos] >= L'a' && input[pos] <= L'z') || input[pos] == L'_') {
						uint s = pos;
						while (pos < length && ((input[pos] >= L'A' && input[pos] <= L'Z') ||
							(input[pos] >= L'a' && input[pos] <= L'z') || (input[pos] >= L'0' && input[pos] <= L'9') || input[pos] == L'_')) pos++;
						token.cls = TokenClass::Name;
						token.text = input.Fragment(s, pos - s).UpperCase();
						token.number = 0;
						token.offset = s;
						return true;
					} else if (input[pos] == L'\'') {
						Array<uint8> text;
						token.cls = TokenClass::String;
						token.number = 0;
						token.offset = pos;
						pos++;
						while (pos < length && input[pos] != L'\'') {
							if (input[pos] == L'\\' && pos + 1 < length) {
								pos++;
								if (input[pos] == L'0') text << 0;
								else if (input[pos] == L'a') text << '\a';
								else if (input[pos] == L'b') text << '\b';
								else if (input[pos] == L'e') text << 0x1B;
								else if (input[pos] == L'f') text << '\f';
								else if (input[pos] == L'v') text << '\v';
								else if (input[pos] == L't') text << '\t';
								else if (input[pos] == L'n') text << '\n';
								else if (input[pos] == L'r') text << '\r';
								else text << input[pos];
								pos++;
							} else {
								text << input[pos]; pos++;
							}
						}
						if (pos < length) pos++;
						token.text = ConvertToBase64(text.GetBuffer(), text.Length());
						return true;
					} else return false;
				}
			}
		}
		void SetError(AssemblyError & error, AssemblyErrorCode code, uint at) { error.code = code; error.position = at; }
		void Assembly(const string & input, const AssemblyState & initstate, AssemblyOutput & output, AssemblyError & error)
		{
			uint length = input.Length();
			uint pos = 0;
			Token token;
			AssemblyState state = initstate;
			output.data.Clear();
			output.relocate_segments.Clear();
			output.entry_point_offset = state.initial_offset;
			output.entry_point_segment_relocate = state.initial_segment;
			output.required_memory = output.desired_memory = output.required_stack = 0;
			Volumes::Dictionary<string, Label> labels;
			Volumes::Dictionary<string, Array<uint>> label_refs;
			Volumes::Dictionary<string, Array<uint>> label_seg_refs;
			Volumes::List<JumpDefinition> jumps;
			i286_EncoderContext ctx(output.data);
			if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
			while (true) {
				if (token.cls == TokenClass::EOF) break;
				if (token.cls == TokenClass::EOL) {
					if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					continue;
				}
				if (token.cls == TokenClass::Name) {
					auto name = token.text;
					auto offs = token.offset;
					if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					if (token.cls == TokenClass::Symbol && token.text == L":") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						Label label;
						label.segment_relocate = state.initial_segment;
						label.offset = state.initial_offset + int(ctx.get_current_position());
						label.offset_is_32 = ctx.is_32_bit_mode();
						labels.Update(name, label);
					} else {
						JumpDefinition jd;
						Array<OperationArgument> args(0x10);
						jd.far = jd.addr_mod_change = false;
						while (token.cls != TokenClass::EOF && token.cls != TokenClass::EOL) {
							OperationArgument arg;
							arg.label_ref = L"";
							arg.value_offset = 0;
							if (token.cls == TokenClass::Number) {
								arg.cls = ArgumentClass::Immediate;
								arg.imm.value = token.number;
								args << arg;
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
							} else if (token.cls == TokenClass::String) {
								SafePointer<DataBlock> dta = ConvertFromBase64(token.text);
								uint8 chr = dta->Length() ? dta->ElementAt(0) : 0;
								arg.cls = ArgumentClass::Immediate;
								arg.imm.value = chr;
								args << arg;
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
							} else if (token.cls == TokenClass::Name) {
								if (token.text == L"ULT") {
									jd.far = true;
								} else if (token.text == L"AA") {
									jd.addr_mod_change = true;
								} else {
									arg.cls = ArgumentClass::Register;
									arg.reg.reg = AsRegisterName(token.text);
									args << arg;
									if (arg.reg.reg == Reg::NO) { SetError(error, AssemblyErrorCode::UnknownRegister, token.offset); return; }
								}
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
							} else if (token.cls == TokenClass::Symbol && token.text == L"[") {
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
								arg.cls = ArgumentClass::Memory;
								arg.mem.segment_reg = Reg::NO;
								arg.mem.offset_reg = Reg::NO;
								arg.mem.offset = 0;
								do {
									if (token.cls == TokenClass::Name) {
										auto reg = AsRegisterName(token.text);
										if (ctx.is_regular_register(reg)) {
											arg.mem.offset_reg = reg;
											arg.mem.segment_reg = Reg::NO;
											if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
										} else if (ctx.is_segment_register(reg)) {
											arg.mem.offset_reg = Reg::NO;
											arg.mem.segment_reg = reg;
											if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
											if (token.cls == TokenClass::Symbol && token.text == L':') {
												if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
												if (token.cls != TokenClass::Name) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
												arg.mem.offset_reg = AsRegisterName(token.text);
												if (arg.mem.offset_reg == Reg::NO) { SetError(error, AssemblyErrorCode::UnknownRegister, token.offset); return; }
												if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
											}
										} else { SetError(error, AssemblyErrorCode::UnknownRegister, token.offset); return; }
									} else if (token.cls == TokenClass::Number) {
										arg.mem.offset = token.number;
										arg.label_ref = L"";
										if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
									} else if (token.cls == TokenClass::Symbol && token.text == L'@') {
										bool segment = false;
										if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
										if (token.cls == TokenClass::Symbol && token.text == L'@') {
											segment = true;
											if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
										}
										if (token.cls != TokenClass::Name) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
										arg.mem.offset = 0x10000;
										arg.label_ref = segment ? L"@" + token.text : token.text;
										if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
									} else { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
									if (token.cls == TokenClass::Symbol && token.text == L'+') {
										if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
									} else if (token.cls == TokenClass::Symbol && token.text == L']') {
										if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
										break;
									} else { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
								} while (true);
								args << arg;
							} else if (token.cls == TokenClass::Symbol && token.text == L'@') {
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
								if (token.cls == TokenClass::Name) {
									jd.label = token.text;
									jd.explicit_address = 0;
								} else if (token.cls == TokenClass::Number) {
									jd.label = L"";
									jd.explicit_address = token.number;
								} else { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
							} else { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
							if (token.cls == TokenClass::Symbol && token.text == L',') {
								if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
							}
						}
						auto status = EncodeOperation(name, args, ctx, jd);
						if (status != AssemblyErrorCode::Success) { SetError(error, status, offs); return; }
						for (auto & a : args) if (a.label_ref.Length()) {
							if (ctx.is_32_bit_mode()) a.value_offset |= 0x80000000;
							if (a.label_ref[0] == L'@') {
								auto label = a.label_ref.Fragment(1, -1);
								label_seg_refs.Append(label, Array<uint32>(0x100));
								auto ref = label_seg_refs.GetElementByKey(label);
								if (ref) ref->Append(a.value_offset);
							} else {
								label_refs.Append(a.label_ref, Array<uint32>(0x100));
								auto ref = label_refs.GetElementByKey(a.label_ref);
								if (ref) ref->Append(a.value_offset);
							}
						}
						if (jd.quant) {
							jd.opcode_end_position += state.initial_offset;
							if (jd.label.Length()) jumps.InsertLast(jd);
							else MemoryCopy(output.data.GetBuffer() + jd.address_position, &jd.explicit_address, jd.quant);
						}
					}
					if (state.initial_offset + int(ctx.get_current_position()) > 0x10000) {
						SetError(error, AssemblyErrorCode::SegmentOverflow, offs);
						return;
					}
				} else if (token.cls == TokenClass::Symbol && token.text == L"#") {
					auto offs = token.offset;
					if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					if (token.cls != TokenClass::Name) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
					if (token.text == L"DATA") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						auto quant = token.number;
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls == TokenClass::Number) {
							if (quant == 4) ctx.encode_data(&token.number, 4);
							else if (quant == 2) ctx.encode_data(&token.number, 2);
							else ctx.encode_data(&token.number, 1);
						} else if (token.cls == TokenClass::String) {
							SafePointer<DataBlock> acsii = ConvertFromBase64(token.text);
							ctx.encode_data(acsii->GetBuffer(), acsii->Length());
						} else { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					} else if (token.text == L"POLI") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						ctx.encode_padding_zeroes(token.number);
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					} else if (token.text == L"SEGMENTUM") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						ctx.encode_padding_zeroes(16);
						auto effective_offset = int(ctx.get_current_position()) + state.initial_offset;
						state.initial_segment += (effective_offset >> 4) & 0xFFFF;
						state.initial_offset = -int(ctx.get_current_position());
					} else if (token.text == L"RELOCA") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						auto rel_offs = token.number;
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						auto rel_seg = token.number;
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						state.initial_segment = rel_seg;
						state.initial_offset = rel_offs - int(ctx.get_current_position());
					} else if (token.text == L"ALLOCA") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						output.required_memory = token.number;
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					} else if (token.text == L"ALLOCA_MAXIMA") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						output.desired_memory = token.number;
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					} else if (token.text == L"ACERVUS") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Number) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						output.required_stack = token.number;
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					} else if (token.text == L"MODUS") {
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
						if (token.cls != TokenClass::Name) { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						if (token.text == L"I386_16") ctx.set_encoder_mode(EncoderMode::i386_16);
						else if (token.text == L"I386_32") ctx.set_encoder_mode(EncoderMode::i386_32);
						else { SetError(error, AssemblyErrorCode::AnotherTokenExpected, token.offset); return; }
						if (!ReadToken(input, length, pos, token)) { SetError(error, AssemblyErrorCode::InvalidTokenInput, pos); return; }
					} else { SetError(error, AssemblyErrorCode::UnknownTranslatorHint, token.offset); return; }
					if (state.initial_offset + int(ctx.get_current_position()) > 0x10000) {
						SetError(error, AssemblyErrorCode::SegmentOverflow, offs);
						return;
					}
				}
			}
			for (auto & cor : label_refs) {
				auto label = labels.GetElementByKey(cor.key);
				if (!label) { SetError(error, AssemblyErrorCode::UnknownLabel, 0); return; }
				for (auto & pos : cor.value) {
					auto p = pos & 0x7FFFFFFF;
					if (pos & 0x80000000) *reinterpret_cast<uint32 *>(output.data.GetBuffer() + p - 4) = label->offset;
					else *reinterpret_cast<uint16 *>(output.data.GetBuffer() + p - 2) = label->offset;
				}
			}
			for (auto & cor : label_seg_refs) {
				auto label = labels.GetElementByKey(cor.key);
				if (!label) { SetError(error, AssemblyErrorCode::UnknownLabel, -1); error.object = cor.key; return; }
				for (auto & pos : cor.value) {
					auto p = pos & 0x7FFFFFFF;
					if (pos & 0x80000000) {
						*reinterpret_cast<uint16 *>(output.data.GetBuffer() + p - 4) = label->segment_relocate;
						output.relocate_segments << (p - 4);
					} else {
						*reinterpret_cast<uint16 *>(output.data.GetBuffer() + p - 2) = label->segment_relocate;
						output.relocate_segments << (p - 2);
					}
				}
			}
			for (auto & cor : jumps) {
				auto label = labels.GetElementByKey(cor.label);
				if (!label) { SetError(error, AssemblyErrorCode::UnknownLabel, -1); error.object = cor.label; return; }
				if (cor.relative) {
					int64 diff = int(label->offset) - int(cor.opcode_end_position);
					if (cor.quant == 1 && abs(diff) >= 0x80) {
						SetError(error, AssemblyErrorCode::IllegalAddressMode, 0);
						return;
					} else if (cor.quant == 2 && abs(diff) >= 0x8000) {
						SetError(error, AssemblyErrorCode::IllegalAddressMode, 0);
						return;
					}
					MemoryCopy(output.data.GetBuffer() + cor.address_position, &diff, cor.quant);
				} else {
					if (label->offset_is_32) {
						if (cor.quant == 6) {
							uint64 addr = uint64(label->offset & 0xFFFFFFFF) | (uint64(label->segment_relocate) << 32);
							MemoryCopy(output.data.GetBuffer() + cor.address_position, &addr, 6);
						} else MemoryCopy(output.data.GetBuffer() + cor.address_position, &label->offset, cor.quant);
					} else {
						if (cor.quant == 4) {
							uint32 addr = (label->offset & 0xFFFF) | (label->segment_relocate << 16);
							MemoryCopy(output.data.GetBuffer() + cor.address_position, &addr, 4);
							output.relocate_segments << (cor.address_position + 2);
						} else MemoryCopy(output.data.GetBuffer() + cor.address_position, &label->offset, cor.quant);
					}
				}
			}
			auto entry = labels.GetElementByKey(L"INITUS");
			if (entry) {
				if (entry->offset_is_32) {
					output.entry_point_offset = entry->offset;
					output.entry_point_segment_relocate = 0;
				} else {
					output.entry_point_offset = entry->offset & 0xFFFF;
					output.entry_point_segment_relocate = entry->segment_relocate;
				}
			}
			error.code = AssemblyErrorCode::Success;
			error.position = 0;
		}
	}
}