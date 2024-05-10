#include "xa_t_x64.h"
#include "xa_t_x86.h"
#include "xa_type_helper.h"

using namespace Engine::XA::X86;

namespace Engine
{
	namespace XA
	{
		namespace X64
		{
			constexpr int WordSize = 8;

			class EncoderContext : public X86::EncoderContext
			{
				static bool _is_xmm_register(Reg reg) { return uint(reg) & 0x00FF0000; }
				static bool _is_pass_by_reference(const ArgumentSpecification & spec) { return _word_align(spec.size) > 8 || spec.semantics == ArgumentSemantics::Object; }
				static bool _needs_stack_storage(const ArgumentSpecification & spec) { if (_word_align(spec.size) > WordSize) return true; return false; }
				static uint32 _word_align(const ObjectSize & size) { uint full_size = size.num_bytes + WordSize * size.num_words; return (uint64(full_size) + 7) / 8 * 8; }
				Array<ArgumentPassageInfo> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt)
				{
					SafePointer< Array<ArgumentPassageInfo> > result = new Array<ArgumentPassageInfo>(0x40);
					if (_conv == CallingConvention::Windows) {
						for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics == ArgumentSemantics::This) {
							ArgumentPassageInfo info;
							info.index = i;
							info.reg = Reg64::NO;
							info.indirect = _is_pass_by_reference(inputs[i]);
							result->Append(info);
						}
					}
					if (_is_pass_by_reference(output)) {
						ArgumentPassageInfo info;
						info.index = -1;
						info.reg = Reg64::NO;
						info.indirect = true;
						result->Append(info);
					}
					for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics != ArgumentSemantics::This || _conv == CallingConvention::Unix) {
						ArgumentPassageInfo info;
						info.index = i;
						info.reg = Reg64::NO;
						info.indirect = _is_pass_by_reference(inputs[i]);
						result->Append(info);
					}
					int cmr = 0, cfr = 0;
					Reg unix_mr[] = { Reg64::RDI, Reg64::RSI, Reg64::RDX, Reg64::RCX, Reg64::R8, Reg64::R9, Reg64::NO };
					Reg unix_fr[] = { Reg64::XMM0, Reg64::XMM1, Reg64::XMM2, Reg64::XMM3, Reg64::XMM4, Reg64::XMM5, Reg64::XMM6, Reg64::XMM7, Reg64::NO };
					for (int i = 0; i < result->Length(); i++) {
						auto & info = result->ElementAt(i);
						if (!info.indirect && inputs[info.index].semantics == ArgumentSemantics::FloatingPoint) {
							if (_conv == CallingConvention::Windows) {
								if (i == 0) info.reg = Reg64::XMM0;
								else if (i == 1) info.reg = Reg64::XMM1;
								else if (i == 2) info.reg = Reg64::XMM2;
								else if (i == 3) info.reg = Reg64::XMM3;
							} else if (_conv == CallingConvention::Unix) {
								info.reg = unix_fr[cfr];
								if (info.reg != Reg64::NO) cfr++;
							}
						} else {
							if (_conv == CallingConvention::Windows) {
								if (i == 0) info.reg = Reg64::RCX;
								else if (i == 1) info.reg = Reg64::RDX;
								else if (i == 2) info.reg = Reg64::R8;
								else if (i == 3) info.reg = Reg64::R9;
							} else if (_conv == CallingConvention::Unix) {
								info.reg = unix_mr[cmr];
								if (info.reg != Reg64::NO) cmr++;
							}
						}
					}
					result->Retain();
					return result;
				}
				virtual void encode_finalize_scope(const LocalScope & scope, uint reg_in_use = 0) override
				{
					encode_preserve(Reg64::RAX, reg_in_use, 0, true);
					if (_conv == CallingConvention::Windows) {
						encode_preserve(Reg64::RCX, reg_in_use, 0, true);
						encode_preserve(Reg64::RDX, reg_in_use, 0, true);
						encode_preserve(Reg64::R8, reg_in_use, 0, true);
						encode_preserve(Reg64::R9, reg_in_use, 0, true);
					} else if (_conv == CallingConvention::Unix) {
						encode_preserve(Reg64::RDI, reg_in_use, 0, true);
						encode_preserve(Reg64::RSI, reg_in_use, 0, true);
						encode_preserve(Reg64::RDX, reg_in_use, 0, true);
						encode_preserve(Reg64::RCX, reg_in_use, 0, true);
						encode_preserve(Reg64::R8, reg_in_use, 0, true);
						encode_preserve(Reg64::R9, reg_in_use, 0, true);
					}
					for (auto & l : scope.locals) if (l.finalizer.final.ref_class != ReferenceNull) {
						bool skip = false;
						for (auto & i : _init_locals) if (i.bp_offset == l.bp_offset) { skip = true; break; }
						if (skip) continue;
						int stack_growth = 0;
						if (_conv == CallingConvention::Windows) {
							stack_growth = max(1 + l.finalizer.final_args.Length(), 4) * 8;
							if ((stack_growth + _stack_oddity) & 0xF) {
								encode_add(Reg64::RSP, -8);
								stack_growth += 8;
							}
							for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
								auto & arg = l.finalizer.final_args[i];
								if (i == 0) encode_put_addr_of(Reg64::RDX, arg);
								else if (i == 1) encode_put_addr_of(Reg64::R8, arg);
								else if (i == 2) encode_put_addr_of(Reg64::R9, arg);
								else { encode_put_addr_of(Reg64::RCX, arg); encode_push(Reg64::RCX); }
							}
							encode_lea(Reg64::RCX, Reg64::RBP, l.bp_offset);
							encode_add(Reg64::RSP, -32);
						} else if (_conv == CallingConvention::Unix) {
							stack_growth = max(l.finalizer.final_args.Length() - 5, 0) * 8;
							if ((stack_growth + _stack_oddity) & 0xF) {
								encode_add(Reg64::RSP, -8);
								stack_growth += 8;
							}
							for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
								auto & arg = l.finalizer.final_args[i];
								if (i == 0) encode_put_addr_of(Reg64::RSI, arg);
								else if (i == 1) encode_put_addr_of(Reg64::RDX, arg);
								else if (i == 2) encode_put_addr_of(Reg64::RCX, arg);
								else if (i == 3) encode_put_addr_of(Reg64::R8, arg);
								else if (i == 4) encode_put_addr_of(Reg64::R9, arg);
								else { encode_put_addr_of(Reg64::RDI, arg); encode_push(Reg64::RDI); }
							}
							encode_lea(Reg64::RDI, Reg64::RBP, l.bp_offset);
						}
						encode_put_addr_of(Reg64::RAX, l.finalizer.final);
						encode_call(Reg64::RAX, false);
						if (stack_growth) encode_add(Reg64::RSP, stack_growth);
					}
					if (_conv == CallingConvention::Windows) {
						encode_restore(Reg64::R9, reg_in_use, 0, true);
						encode_restore(Reg64::R8, reg_in_use, 0, true);
						encode_restore(Reg64::RDX, reg_in_use, 0, true);
						encode_restore(Reg64::RCX, reg_in_use, 0, true);
					} else if (_conv == CallingConvention::Unix) {
						encode_restore(Reg64::R9, reg_in_use, 0, true);
						encode_restore(Reg64::R8, reg_in_use, 0, true);
						encode_restore(Reg64::RCX, reg_in_use, 0, true);
						encode_restore(Reg64::RDX, reg_in_use, 0, true);
						encode_restore(Reg64::RSI, reg_in_use, 0, true);
						encode_restore(Reg64::RDI, reg_in_use, 0, true);
					}
					encode_restore(Reg64::RAX, reg_in_use, 0, true);
					if (scope.shift_sp) encode_add(Reg64::RSP, scope.frame_size);
				}
				void _encode_arithmetics(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() > 2 || node.inputs.Length() < 1) throw InvalidArgumentException();
					auto size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					if (size != 1 && size != 2 && size != 4 && size != 8) throw InvalidArgumentException();
					auto opcode = node.self.index;
					InternalDisposition core;
					core.flags = DispositionRegister;
					core.size = size;
					core.reg = (disp->reg != Reg64::NO) ? disp->reg : Reg64::RAX;
					Array<Reg> fsave(2);
					Reg rreg = Reg64::NO;
					if (opcode == TransformIntegerSResize || opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR) {
						core.reg = Reg64::RAX;
						if (disp->reg != Reg64::RAX) fsave << Reg64::RAX;
					} if (opcode >= TransformIntegerUMul && opcode <= TransformIntegerSMod) {
						core.reg = Reg64::RAX;
						if (disp->reg != Reg64::RAX) fsave << Reg64::RAX;
						if (disp->reg != Reg64::RDX) fsave << Reg64::RDX;
					} else if (disp->reg == Reg64::NO) fsave << Reg64::RAX;
					for (auto & r : fsave.Elements()) encode_preserve(r, reg_in_use, 0, !idle);
					if (opcode == TransformVectorInverse || opcode == TransformVectorIsZero || opcode == TransformVectorNotZero || opcode == TransformIntegerUResize ||
						opcode == TransformIntegerSResize || opcode == TransformIntegerInverse || opcode == TransformIntegerAbs) {
						_encode_tree_node(node.inputs[0], idle, mem_load, &core, reg_in_use | uint(core.reg));
						if (!idle) {
							if (opcode == TransformVectorInverse) {
								encode_invert(size, core.reg);
							} else if (opcode == TransformVectorIsZero) {
								encode_test(size, core.reg, 0xFFFFFFFF);
								int addr;
								_dest.code << 0x75; // JNZ
								_dest.code << 0x00;
								addr = _dest.code.Length();
								encode_mov_reg_const(size, core.reg, 1);
								_dest.code << 0xEB;
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								encode_mov_reg_const(size, core.reg, 0);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
							} else if (opcode == TransformVectorNotZero) {
								encode_test(size, core.reg, 0xFFFFFFFF);
								int addr;
								_dest.code << 0x75; // JNZ
								_dest.code << 0x00;
								addr = _dest.code.Length();
								encode_mov_reg_const(size, core.reg, 0);
								_dest.code << 0xEB;
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								encode_mov_reg_const(size, core.reg, 1);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
							} else if (opcode == TransformIntegerUResize) {
								auto size_from = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
								if (size_from < size) {
									auto delta = (8 - size_from) * 8;
									encode_shl(core.reg, delta);
									encode_shr(core.reg, delta);
								}
							} else if (opcode == TransformIntegerSResize) {
								auto size_from = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
								if (size_from < size) {
									if (size_from < 2 && size >= 2) { _dest.code << 0x66; _dest.code << 0x98; }
									if (size_from < 4 && size >= 4) { _dest.code << 0x98; }
									if (size_from < 8 && size >= 8) { _dest.code << make_rex(true, false, false, false); _dest.code << 0x98; }
								}
								rreg = Reg64::RAX;
							} else if (opcode == TransformIntegerInverse) {
								encode_negative(size, core.reg);
							} else if (opcode == TransformIntegerAbs) {
								Reg sec = core.reg == Reg64::RDX ? Reg64::RCX : Reg64::RDX;
								encode_preserve(sec, reg_in_use, 0, true);
								encode_mov_reg_reg(size, sec, core.reg);
								if (size == 1) encode_shift(size, shOp::SAR, sec, 7);
								else if (size == 2) encode_shift(size, shOp::SAR, sec, 15);
								else if (size == 4) encode_shift(size, shOp::SAR, sec, 31);
								else if (size == 8) encode_shift(size, shOp::SAR, sec, 63);
								encode_operation(size, arOp::XOR, core.reg, sec);
								encode_operation(size, arOp::SUB, core.reg, sec);
								encode_restore(sec, reg_in_use, 0, true);
							}
						}
					} else {
						if (node.inputs.Length() != 2) throw InvalidArgumentException();
						InternalDisposition modifier;
						modifier.flags = DispositionAny;
						modifier.size = size;
						modifier.reg = core.reg == Reg64::RCX ? Reg64::RDX : Reg64::RCX;
						encode_preserve(modifier.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &core, reg_in_use | uint(core.reg) | uint(modifier.reg));
						_encode_tree_node(node.inputs[1], idle, mem_load, &modifier, reg_in_use | uint(core.reg) | uint(modifier.reg));
						if (!idle) {
							if (opcode == TransformLogicalSame) {
								if (modifier.flags & DispositionPointer) encode_mov_reg_mem(size, modifier.reg, modifier.reg);
								encode_test(size, core.reg, 0xFFFFFFFF);
								_dest.code << 0x74; // JZ
								_dest.code << 0x00;
								int addr = _dest.code.Length();
								encode_mov_reg_reg(size, core.reg, modifier.reg);
								_dest.code << 0xEB; // JMP
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								encode_test(size, modifier.reg, 0xFFFFFFFF);
								_dest.code << 0x75; // JNZ
								_dest.code << 0x00;
								int addr2 = _dest.code.Length();
								encode_mov_reg_const(size, core.reg, 1);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							} else if (opcode == TransformLogicalNotSame) {
								if (modifier.flags & DispositionPointer) encode_mov_reg_mem(size, modifier.reg, modifier.reg);
								encode_test(size, core.reg, 0xFFFFFFFF);
								_dest.code << 0x75; // JNZ
								_dest.code << 0x00;
								int addr = _dest.code.Length();
								encode_mov_reg_reg(size, core.reg, modifier.reg);
								_dest.code << 0xEB; // JMP
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								encode_test(size, modifier.reg, 0xFFFFFFFF);
								_dest.code << 0x74; // JZ
								_dest.code << 0x00;
								int addr2 = _dest.code.Length();
								encode_operation(size, arOp::XOR, core.reg, core.reg);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							} else if (opcode == TransformVectorAnd) {
								if (modifier.flags & DispositionPointer) encode_operation(size, arOp::AND, core.reg, modifier.reg, true);
								else encode_operation(size, arOp::AND, core.reg, modifier.reg);
							} else if (opcode == TransformVectorOr) {
								if (modifier.flags & DispositionPointer) encode_operation(size, arOp::OR, core.reg, modifier.reg, true);
								else encode_operation(size, arOp::OR, core.reg, modifier.reg);
							} else if (opcode == TransformVectorXor) {
								if (modifier.flags & DispositionPointer) encode_operation(size, arOp::XOR, core.reg, modifier.reg, true);
								else encode_operation(size, arOp::XOR, core.reg, modifier.reg);
							} else if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR) {
								if (modifier.flags & DispositionPointer) encode_mov_reg_mem(size, Reg64::RCX, Reg64::RCX);
								int mask, max_shift;
								shOp op;
								if (opcode == TransformVectorShiftL) op = shOp::SHL;
								else if (opcode == TransformVectorShiftR) op = shOp::SHR;
								else if (opcode == TransformVectorShiftAL) op = shOp::SAL;
								else if (opcode == TransformVectorShiftAR) op = shOp::SAR;
								if (size == 1) { mask = 0xFFFFFFF8; max_shift = 7; }
								else if (size == 2) { mask = 0xFFFFFFF0; max_shift = 15; }
								else if (size == 4) { mask = 0xFFFFFFE0; max_shift = 31; }
								else if (size == 8) { mask = 0xFFFFFFC0; max_shift = 63; }
								encode_test(size, Reg64::RCX, mask);
								int addr;
								_dest.code << 0x75; // JNZ
								_dest.code << 0x00;
								addr = _dest.code.Length();
								encode_shift(size, op, Reg64::RAX);
								_dest.code << 0xEB;
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								if (opcode == TransformVectorShiftAR) encode_shift(size, op, Reg64::RAX, max_shift);
								else encode_mov_reg_const(size, Reg64::RAX, 0);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								rreg = Reg64::RAX;
							} else if (opcode >= TransformIntegerEQ && opcode <= TransformIntegerSG) {
								if (modifier.flags & DispositionPointer) encode_operation(size, arOp::CMP, core.reg, modifier.reg, true);
								else encode_operation(size, arOp::CMP, core.reg, modifier.reg);
								uint8 jcc;
								if (opcode == TransformIntegerEQ) jcc = 0x74; // JZ
								else if (opcode == TransformIntegerNEQ) jcc = 0x75; // JNZ
								else if (opcode == TransformIntegerULE) jcc = 0x76; // JBE
								else if (opcode == TransformIntegerUGE) jcc = 0x73; // JAE
								else if (opcode == TransformIntegerUL) jcc = 0x72; // JB
								else if (opcode == TransformIntegerUG) jcc = 0x77; // JA
								else if (opcode == TransformIntegerSLE) jcc = 0x7E; // JLE
								else if (opcode == TransformIntegerSGE) jcc = 0x7D; // JGE
								else if (opcode == TransformIntegerSL) jcc = 0x7C; // JL
								else if (opcode == TransformIntegerSG) jcc = 0x7F; // JG
								_dest.code << jcc;
								_dest.code << 5;
								encode_operation(8, arOp::XOR, core.reg, core.reg);
								_dest.code << 0xEB;
								_dest.code << 10;
								encode_mov_reg_const(8, core.reg, 1);
							} else if (opcode == TransformIntegerAdd) {
								if (modifier.flags & DispositionPointer) encode_operation(size, arOp::ADD, core.reg, modifier.reg, true);
								else encode_operation(size, arOp::ADD, core.reg, modifier.reg);
							} else if (opcode == TransformIntegerSubt) {
								if (modifier.flags & DispositionPointer) encode_operation(size, arOp::SUB, core.reg, modifier.reg, true);
								else encode_operation(size, arOp::SUB, core.reg, modifier.reg);
							} else if (opcode == TransformIntegerUMul) {
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::MUL, modifier.reg, true);
								else encode_mul_div(size, mdOp::MUL, modifier.reg);
								rreg = Reg64::RAX;
							} else if (opcode == TransformIntegerSMul) {
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::IMUL, modifier.reg, true);
								else encode_mul_div(size, mdOp::IMUL, modifier.reg);
								rreg = Reg64::RAX;
							} else if (opcode == TransformIntegerUDiv) {
								if (size > 1) encode_operation(size, arOp::XOR, Reg64::RDX, Reg64::RDX);
								else { encode_shl(Reg64::RAX, 56); encode_shr(Reg64::RAX, 56); }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::DIV, modifier.reg, true);
								else encode_mul_div(size, mdOp::DIV, modifier.reg);
								rreg = Reg64::RAX;
							} else if (opcode == TransformIntegerSDiv) {
								if (size > 1) {
									encode_mov_reg_reg(size, Reg64::RDX, Reg64::RAX);
									encode_shift(size, shOp::SAR, Reg64::RDX, 63);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::IDIV, modifier.reg, true);
								else encode_mul_div(size, mdOp::IDIV, modifier.reg);
								rreg = Reg64::RAX;
							} else if (opcode == TransformIntegerUMod) {
								if (size > 1) encode_operation(size, arOp::XOR, Reg64::RDX, Reg64::RDX);
								else { encode_shl(Reg64::RAX, 56); encode_shr(Reg64::RAX, 56); }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::DIV, modifier.reg, true);
								else encode_mul_div(size, mdOp::DIV, modifier.reg);
								if (size == 1) {
									encode_shr(Reg64::RAX, 8);
									rreg = Reg64::RAX;
								} else rreg = Reg64::RDX;
							} else if (opcode == TransformIntegerSMod) {
								if (size > 1) {
									encode_mov_reg_reg(size, Reg64::RDX, Reg64::RAX);
									encode_shift(size, shOp::SAR, Reg64::RDX, 63);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::IDIV, modifier.reg, true);
								else encode_mul_div(size, mdOp::IDIV, modifier.reg);
								if (size == 1) {
									encode_shr(Reg64::RAX, 8);
									rreg = Reg64::RAX;
								} else rreg = Reg64::RDX;
							} else throw InvalidArgumentException();
						}
						encode_restore(modifier.reg, reg_in_use, 0, !idle);
					}
					if (rreg != Reg64::NO && rreg != disp->reg && !idle) encode_mov_reg_reg(size, disp->reg, rreg);
					for (auto & r : fsave.InversedElements()) encode_restore(r, reg_in_use, 0, !idle);
					if (disp->flags & DispositionRegister) {
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionPointer) {
						(*mem_load) += _word_align(TH::MakeSize(size, 0));
						if (!idle) {
							int offs = allocate_temporary(TH::MakeSize(size, 0));
							encode_mov_mem_reg(size, Reg64::RBP, offs, disp->reg);
							encode_lea(disp->reg, Reg64::RBP, offs);
						}
						disp->flags = DispositionPointer;
					}
				}
				void _encode_logics(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformLogicalFork) {
						if (node.inputs.Length() != 3) throw InvalidArgumentException();
						InternalDisposition cond, none;
						cond.reg = Reg64::RCX;
						cond.flags = DispositionRegister;
						cond.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						none.reg = Reg64::NO;
						none.flags = DispositionDiscard;
						none.size = 0;
						if (!cond.size || cond.size > 8) throw InvalidArgumentException();
						encode_preserve(cond.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &cond, reg_in_use | uint(cond.reg));
						if (!idle) {
							encode_test(cond.size, cond.reg, 0xFFFFFFFF);
							encode_restore(cond.reg, reg_in_use, 0, true);
							_dest.code << 0x0F; _dest.code << 0x84; // JZ
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
						}
						int addr = _dest.code.Length();
						if (!idle) {
							int local_mem_load = 0, ssoffs;
							_encode_tree_node(node.inputs[1], true, &local_mem_load, &none, reg_in_use);
							ssoffs = allocate_temporary(TH::MakeSize(local_mem_load, 0));
							encode_open_scope(local_mem_load, true, ssoffs);
							_encode_tree_node(node.inputs[1], false, &local_mem_load, &none, reg_in_use);
							encode_close_scope(reg_in_use);
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
						} else _encode_tree_node(node.inputs[1], true, mem_load, &none, reg_in_use);
						addr = _dest.code.Length();
						if (!idle) {
							int local_mem_load = 0, ssoffs;
							_encode_tree_node(node.inputs[2], true, &local_mem_load, &none, reg_in_use);
							ssoffs = allocate_temporary(TH::MakeSize(local_mem_load, 0));
							encode_open_scope(local_mem_load, true, ssoffs);
							_encode_tree_node(node.inputs[2], false, &local_mem_load, &none, reg_in_use);
							encode_close_scope(reg_in_use);
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
						} else _encode_tree_node(node.inputs[2], true, mem_load, &none, reg_in_use);
						if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					} else {
						if (!node.inputs.Length()) throw InvalidArgumentException();
						Reg local = disp->reg != Reg64::NO ? disp->reg : Reg64::RCX;
						encode_preserve(local, reg_in_use, disp->reg, !idle);
						Array<int> put_offs(0x10);
						uint min_size = 0;
						uint rv_size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
						if (!rv_size || rv_size > 8) throw InvalidArgumentException();
						for (int i = 0; i < node.inputs.Length(); i++) {
							auto & n = node.inputs[i];
							auto size = node.input_specs[i].size.num_bytes + WordSize * node.input_specs[i].size.num_words;
							if (!size || size > 8) throw InvalidArgumentException();
							if (size < min_size) min_size = size;
							InternalDisposition ld;
							ld.flags = DispositionRegister;
							ld.reg = local;
							ld.size = size;
							if (!idle) {
								int local_mem_load = 0, ssoffs;
								_encode_tree_node(n, true, &local_mem_load, &ld, reg_in_use | uint(local));
								ssoffs = allocate_temporary(TH::MakeSize(local_mem_load, 0));
								encode_open_scope(local_mem_load, true, ssoffs);
								_encode_tree_node(n, false, &local_mem_load, &ld, reg_in_use | uint(local));
								encode_close_scope(reg_in_use | uint(local));
							} else _encode_tree_node(n, true, mem_load, &ld, reg_in_use | uint(local));
							if (!idle && i < node.inputs.Length() - 1) {
								encode_test(size, local, 0xFFFFFFFF);
								if (node.self.index == TransformLogicalAnd) {
									_dest.code << 0x0F;
									_dest.code << 0x84; // JZ
								} else if (node.self.index == TransformLogicalOr) {
									_dest.code << 0x0F;
									_dest.code << 0x85; // JNZ
								} else throw InvalidArgumentException();
								_dest.code << 0x00;
								_dest.code << 0x00;
								_dest.code << 0x00;
								_dest.code << 0x00;
								put_offs << _dest.code.Length();
							}
						}
						if (!idle) for (auto & p : put_offs) *reinterpret_cast<int *>(_dest.code.GetBuffer() + p - 4) = _dest.code.Length() - p;
						if (rv_size > min_size && !idle) {
							uint cs = (8 - min_size) * 8;
							encode_shl(local, cs);
							encode_shr(local, cs);
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != local) encode_mov_reg_reg(8, disp->reg, local);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(0, 1));
								encode_mov_mem_reg(8, Reg64::RBP, offs, local);
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						encode_restore(local, reg_in_use, disp->reg, !idle);
					}
				}
				void _encode_general_call(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					bool indirect, retval_byref, preserve_rax, retval_final;
					int first_arg, arg_no;
					if (node.self.ref_class == ReferenceTransform && node.self.index == TransformInvoke) {
						indirect = true; first_arg = 1; arg_no = node.inputs.Length() - 1;
					} else {
						indirect = false; first_arg = 0; arg_no = node.inputs.Length();
					}
					preserve_rax = disp->reg != Reg64::RAX;
					retval_byref = _is_pass_by_reference(node.retval_spec);
					retval_final = node.retval_final.final.ref_class != ReferenceNull;
					SafePointer< Array<ArgumentPassageInfo> > layout = _make_interface_layout(node.retval_spec, node.input_specs.GetBuffer() + first_arg, arg_no);
					Array<Reg> preserve_regs(0x10);
					Array<int> argument_homes(1), argument_layout_index(1);
					if (_conv == CallingConvention::Windows) preserve_regs << Reg64::RBX << Reg64::RCX << Reg64::RDX << Reg64::R8 << Reg64::R9;
					else if (_conv == CallingConvention::Unix) preserve_regs << Reg64::RBX << Reg64::RDI << Reg64::RSI << Reg64::RDX << Reg64::RCX << Reg64::R8 << Reg64::R9;
					encode_preserve(Reg64::RAX, reg_in_use, 0, !idle && preserve_rax);
					for (auto & r : preserve_regs.Elements()) encode_preserve(r, reg_in_use, 0, !idle);
					uint reg_used_mask = 0;
					uint stack_usage = 0;
					uint num_args_by_stack = 0;
					uint num_args_by_xmm = 0;
					for (auto & info : *layout) {
						if (_is_xmm_register(info.reg)) num_args_by_xmm++;
						else if (info.reg == Reg64::NO) num_args_by_stack++;
					}
					if (_conv == CallingConvention::Windows) stack_usage = max(layout->Length(), 4) * 8;
					else if (_conv == CallingConvention::Unix) stack_usage = (num_args_by_stack + num_args_by_xmm) * 8;
					if ((_stack_oddity + stack_usage) & 0xF) stack_usage += 8;
					if (stack_usage && !idle) {
						_stack_oddity += stack_usage;
						encode_add(Reg64::RSP, -int(stack_usage));
					}
					argument_homes.SetLength(node.inputs.Length() - first_arg);
					argument_layout_index.SetLength(node.inputs.Length() - first_arg);
					uint current_stack_index = 0;
					int rv_offset = 0;
					Reg rv_reg = Reg64::NO;
					int rv_mem_index = -1;
					for (int i = 0; i < layout->Length(); i++) {
						auto & info = layout->ElementAt(i);
						if (info.index >= 0) {
							argument_layout_index[info.index] = i;
							if (_conv == CallingConvention::Windows) {
								argument_homes[info.index] = current_stack_index;
								current_stack_index += 8;
							} else if (_conv == CallingConvention::Unix) {
								if (info.reg == Reg64::NO) {
									argument_homes[info.index] = current_stack_index;
									current_stack_index += 8;
								} else argument_homes[info.index] = -1;
							}
						} else {
							if (_conv == CallingConvention::Windows) current_stack_index += 8;
							*mem_load += _word_align(node.retval_spec.size);
							rv_reg = info.reg;
							if (!idle) rv_offset = allocate_temporary(node.retval_spec.size, &rv_mem_index);
						}
					}
					if (_conv == CallingConvention::Unix) for (auto & info : layout->Elements()) if (info.index >= 0 && _is_xmm_register(info.reg)) {
						argument_homes[info.index] = current_stack_index;
						current_stack_index += 8;
					}
					if (indirect) {
						InternalDisposition ld;
						ld.flags = DispositionRegister;
						ld.reg = Reg64::RAX;
						ld.size = 8;
						reg_used_mask |= uint(ld.reg);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | reg_used_mask);
					}
					if (!idle && current_stack_index) {
						encode_mov_reg_reg(8, Reg64::RBX, Reg64::RSP);
						reg_used_mask |= uint(Reg64::RBX);
					}
					for (int i = 0; i < argument_homes.Length(); i++) {
						auto home = argument_homes[i];
						auto & info = layout->ElementAt(argument_layout_index[i]);
						auto & spec = node.input_specs[i + first_arg];
						InternalDisposition ld;
						ld.size = spec.size.num_bytes + WordSize * spec.size.num_words;
						ld.flags = info.indirect ? DispositionPointer : DispositionRegister;
						bool home_mode;
						if (info.reg == Reg64::NO || _is_xmm_register(info.reg)) {
							home_mode = true;
							ld.reg = Reg64::R14;
						} else {
							home_mode = false;
							ld.reg = info.reg;
							reg_used_mask |= uint(ld.reg);
						}
						if (home_mode) encode_preserve(ld.reg, reg_in_use | reg_used_mask, 0, !idle);
						_encode_tree_node(node.inputs[i + first_arg], idle, mem_load, &ld, reg_in_use | reg_used_mask | uint(ld.reg));
						if ((ld.flags & DispositionRegister) && !idle && ld.size < 4) {
							encode_shift(8, shOp::SHL, ld.reg, (8 - ld.size) * 8);
							encode_shift(8, node.input_specs[i + first_arg].semantics == ArgumentSemantics::SignedInteger ? shOp::SAR : shOp::SHR, ld.reg, (8 - ld.size) * 8);
						}
						if (home_mode) {
							if (!idle) encode_mov_mem_reg(8, Reg64::RBX, home, ld.reg);
							encode_restore(ld.reg, reg_in_use | reg_used_mask, 0, !idle);
						}
					}
					if (!idle && rv_reg != Reg64::NO) encode_lea(rv_reg, Reg64::RBP, rv_offset);
					if (!idle && num_args_by_xmm) {
						if (indirect) encode_push(Reg64::RAX);
						for (int i = 0; i < argument_homes.Length(); i++) {
							auto home = argument_homes[i];
							auto & info = layout->ElementAt(argument_layout_index[i]);
							if (_is_xmm_register(info.reg)) {
								encode_mov_reg_mem(8, Reg64::RAX, Reg64::RBX, home);
								encode_mov_xmm_reg(8, info.reg, Reg64::RAX);
							}
						}
						if (indirect) encode_pop(Reg64::RAX);
					}
					if (!indirect && !idle) encode_put_addr_of(Reg64::RAX, node.self);
					if (!idle) {
						encode_call(Reg64::RAX, false);
						_stack_oddity -= stack_usage;
						if (stack_usage) encode_add(Reg64::RSP, int(stack_usage));
						if (!retval_byref && node.retval_spec.semantics == ArgumentSemantics::FloatingPoint) encode_mov_reg_xmm(8, Reg64::RAX, Reg64::XMM0);
					}
					for (auto & r : preserve_regs.InversedElements()) encode_restore(r, reg_in_use, 0, !idle);
					if ((disp->flags & DispositionPointer) && retval_byref) {
						if (!idle && disp->reg != Reg64::RAX) encode_mov_reg_reg(8, disp->reg, Reg64::RAX);
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && !retval_byref) {
						if (retval_final) {
							*mem_load += WordSize;
							if (!idle) {
								int offs;
								offs = allocate_temporary(TH::MakeSize(0, 1), &rv_mem_index);
								encode_mov_mem_reg(8, Reg64::RBP, offs, Reg64::RAX);
							}
						}
						if (!idle && disp->reg != Reg64::RAX) encode_mov_reg_reg(8, disp->reg, Reg64::RAX);
						disp->flags = DispositionRegister;
					} else if ((disp->flags & DispositionPointer) && !retval_byref) {
						*mem_load += WordSize;
						if (!idle) {
							int offs;
							offs = allocate_temporary(TH::MakeSize(0, 1), &rv_mem_index);
							encode_mov_mem_reg(8, Reg64::RBP, offs, Reg64::RAX);
							encode_lea(disp->reg, Reg64::RBP, offs);
						}
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && retval_byref) {
						if (!idle) {
							encode_preserve(Reg64::RCX, reg_in_use, 0, !preserve_rax);
							Reg src = Reg64::RAX;
							if (disp->reg == Reg64::RAX) {
								src = Reg64::RCX;
								encode_mov_reg_reg(8, Reg64::RCX, Reg64::RAX);
							}
							uint size = _word_align(node.retval_spec.size);
							encode_blt(disp->reg, false, src, true, size, reg_in_use | uint(disp->reg) | uint(src));
							encode_restore(Reg64::RCX, reg_in_use, 0, !preserve_rax);
						}
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					}
					encode_restore(Reg64::RAX, reg_in_use, 0, !idle && preserve_rax);
					if (rv_mem_index >= 0) assign_finalizer(rv_mem_index, node.retval_final);
				}
				void _encode_floating_point_preserve(uint vreg_in_use, VectorDisposition * disp)
				{
					uint unmask = disp ? disp->reg_lo | disp->reg_hi : 0;
					encode_preserve(Reg64::XMM0, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM1, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM2, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM3, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM4, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM5, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM6, vreg_in_use, unmask, true);
					encode_preserve(Reg64::XMM7, vreg_in_use, unmask, true);
				}
				void _encode_floating_point_restore(uint vreg_in_use, VectorDisposition * disp)
				{
					uint unmask = disp ? disp->reg_lo | disp->reg_hi : 0;
					encode_restore(Reg64::XMM7, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM6, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM5, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM4, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM3, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM2, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM1, vreg_in_use, unmask, true);
					encode_restore(Reg64::XMM0, vreg_in_use, unmask, true);
				}
				void _encode_floating_point(const ExpressionTree & node, bool idle, int * mem_load, VectorDisposition * disp, uint reg_in_use, uint vreg_in_use)
				{
					if (node.self.ref_flags & ReferenceFlagInvoke) {
						if (node.self.ref_class == ReferenceTransform) {
							if (node.self.index >= 0x080 && node.self.index < 0x100) {
								if (node.self.ref_flags & ReferenceFlagShort) throw InvalidArgumentException();
								if (node.self.index == TransformFloatResize) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto ins = object_size(node.input_specs[0].size);
									auto ous = object_size(node.retval_spec.size);
									auto dim = (node.self.ref_flags & ReferenceFlagLong) ? ins / 8 : ins / 4;
									if (ins == ous) {
										VectorDisposition a;
										a.size = ins;
										a.reg_lo = disp->reg_lo;
										a.reg_hi = disp->reg_hi;
										if (a.reg_lo == Reg64::NO) allocate_xmm(vreg_in_use, 0, a);
										encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
										encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
										_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
										encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
										encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									} else {
										VectorDisposition in;
										in.size = ins;
										allocate_xmm(vreg_in_use, disp->reg_lo | disp->reg_hi, in);
										encode_preserve(in.reg_lo, vreg_in_use, 0, !idle);
										encode_preserve(in.reg_hi, vreg_in_use, 0, !idle);
										_encode_floating_point(node.inputs[0], idle, mem_load, &in, reg_in_use, vreg_in_use | uint(in.reg_lo) | uint(in.reg_hi));
										if (!idle && disp->reg_lo) {
											if (dim == 4) {
												if (ins == 32 && ous == 16) {
													// CVTPD2PS disp->reg_lo in.reg_lo
													_dest.code << 0x66 << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
													// CVTPD2PS in.reg_hi in.reg_hi
													_dest.code << 0x66 << 0x0F << 0x5A << make_mod(xmm_register_code(in.reg_hi), 3, xmm_register_code(in.reg_hi));
													// MOVLHPS disp->reg_lo in.reg_hi
													_dest.code << 0x0F << 0x16 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_hi));
												} else if (ins == 16 && ous == 32) {
													// CVTPS2PD disp->reg_lo in.reg_lo
													_dest.code << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
													// MOVHLPS disp->reg_hi in.reg_lo
													_dest.code << 0x0F << 0x12 << make_mod(xmm_register_code(disp->reg_hi), 3, xmm_register_code(in.reg_lo));
													// CVTPS2PD disp->reg_hi disp->reg_hi
													_dest.code << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_hi), 3, xmm_register_code(disp->reg_hi));
												} else throw InvalidArgumentException();
											} else if (dim == 3) {
												if (ins == 24 && ous == 12) {
													// CVTPD2PS disp->reg_lo in.reg_lo
													_dest.code << 0x66 << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
													// CVTSD2SS in.reg_hi in.reg_hi
													_dest.code << 0xF2 << 0x0F << 0x5A << make_mod(xmm_register_code(in.reg_hi), 3, xmm_register_code(in.reg_hi));
													// MOVLHPS disp->reg_lo in.reg_hi
													_dest.code << 0x0F << 0x16 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_hi));
												} else if (ins == 12 && ous == 24) {
													// CVTPS2PD disp->reg_lo in.reg_lo
													_dest.code << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
													// MOVHLPS disp->reg_hi in.reg_lo
													_dest.code << 0x0F << 0x12 << make_mod(xmm_register_code(disp->reg_hi), 3, xmm_register_code(in.reg_lo));
													// CVTSS2SD disp->reg_hi disp->reg_hi
													_dest.code << 0xF3 << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_hi), 3, xmm_register_code(disp->reg_hi));
												} else throw InvalidArgumentException();
											} else if (dim == 2) {
												if (ins == 16 && ous == 8) {
													// CVTPD2PS disp->reg_lo in.reg_lo
													_dest.code << 0x66 << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
												} else if (ins == 8 && ous == 16) {
													// CVTPS2PD disp->reg_lo in.reg_lo
													_dest.code << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
												} else throw InvalidArgumentException();
											} else if (dim == 1) {
												if (ins == 8 && ous == 4) {
													// CVTSD2SS disp->reg_lo in.reg_lo
													_dest.code << 0xF2 << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
												} else if (ins == 4 && ous == 8) {
													// CVTSS2SD disp->reg_lo in.reg_lo
													_dest.code << 0xF3 << 0x0F << 0x5A << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(in.reg_lo));
												} else throw InvalidArgumentException();
											} else throw InvalidArgumentException();
										}
										encode_restore(in.reg_hi, vreg_in_use, 0, !idle);
										encode_restore(in.reg_lo, vreg_in_use, 0, !idle);
									}
								} else if (node.self.index == TransformFloatGather) {
									int ous = object_size(node.retval_spec.size);
									int dim = (node.self.ref_flags & ReferenceFlagLong) ? ous / 8 : ous / 4;
									if (dim == 0 || dim > 4 || node.inputs.Length() != dim) throw InvalidArgumentException();
									Reg vin[4] = { 0, 0, 0, 0 };
									Reg xin[4] = { 0, 0, 0, 0 };
									Reg xalloc = Reg64::RAX;
									for (int i = 0; i < dim; i++) if (node.input_specs[i].semantics != ArgumentSemantics::FloatingPoint) {
										xin[i] = xalloc; xalloc <<= 1;
										encode_preserve(xin[i], reg_in_use, 0, !idle);
									}
									auto fppres = xalloc != Reg64::RAX && !idle;
									if (fppres) _encode_floating_point_preserve(vreg_in_use, disp);
									xalloc = 0;
									for (int i = 0; i < dim; i++) if (xin[i]) {
										InternalDisposition ld;
										ld.size = object_size(node.input_specs[i].size);
										ld.reg = xin[i];
										ld.flags = DispositionRegister;
										xalloc |= ld.reg;
										_encode_tree_node(node.inputs[i], idle, mem_load, &ld, reg_in_use | xalloc);
									}
									if (fppres) _encode_floating_point_restore(vreg_in_use, disp);
									auto reg_in_use_new = reg_in_use | xalloc;
									xalloc = disp->reg_lo | disp->reg_hi;
									for (int i = 0; i < dim; i++) {
										vin[i] = allocate_xmm(vreg_in_use | xalloc, xalloc);
										xalloc |= vin[i];
										encode_preserve(vin[i], vreg_in_use, 0, !idle);
										if (node.input_specs[i].semantics == ArgumentSemantics::FloatingPoint) {
											VectorDisposition ld;
											ld.size = object_size(node.input_specs[i].size);
											ld.reg_lo = vin[i];
											ld.reg_hi = Reg64::NO;
											_encode_floating_point(node.inputs[i], idle, mem_load, &ld, reg_in_use_new, vreg_in_use | xalloc);
										}
									}
									if (!idle) {
										for (int i = 0; i < dim; i++) {
											auto ins = object_size(node.input_specs[i].size);
											if (xin[i]) {
												if (ins == 1) {
													encode_shift(8, shOp::SHL, xin[i], 56);
													encode_shift(8, node.input_specs[i].semantics == ArgumentSemantics::SignedInteger ? shOp::SAR : shOp::SHR, xin[i], 56);
												} else if (ins == 2) {
													encode_shift(8, shOp::SHL, xin[i], 48);
													encode_shift(8, node.input_specs[i].semantics == ArgumentSemantics::SignedInteger ? shOp::SAR : shOp::SHR, xin[i], 48);
												} else if (ins != 4 && ins != 8) throw InvalidArgumentException();
												if (node.self.ref_flags & ReferenceFlagLong) {
													// CVTSI2SD vin[i] xin[i]
													_dest.code << 0xF2;
													if (ins == 8) _dest.code << make_rex(true, 0, 0, 0);
													_dest.code << 0x0F << 0x2A << make_mod(xmm_register_code(vin[i]), 3, regular_register_code(xin[i]));
													ins = 8;
												} else {
													// CVTSI2SS vin[i] xin[i]
													_dest.code << 0xF3;
													if (ins == 8) _dest.code << make_rex(true, 0, 0, 0);
													_dest.code << 0x0F << 0x2A << make_mod(xmm_register_code(vin[i]), 3, regular_register_code(xin[i]));
													ins = 4;
												}
											}
											if ((node.self.ref_flags & ReferenceFlagLong) && ins == 4) {
												// CVTSS2SD vin[i] vin[i]
												_dest.code << 0xF3 << 0x0F << 0x5A << make_mod(xmm_register_code(vin[i]), 3, xmm_register_code(vin[i]));
											} else if (!(node.self.ref_flags & ReferenceFlagLong) && ins == 8) {
												// CVTSD2SS vin[i] vin[i]
												_dest.code << 0xF2 << 0x0F << 0x5A << make_mod(xmm_register_code(vin[i]), 3, xmm_register_code(vin[i]));
											}
										}
										if (disp->reg_lo) {
											if (node.self.ref_flags & ReferenceFlagLong) {
												if (ous == 8) {
													// MOVSD disp->reg_lo vin[0]
													_dest.code << 0xF2 << 0x0F << 0x10 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
												} else if (ous == 16) {
													// SHUFPD vin[0] vin[1]
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[1])) << 0x00;
													// MOVAPD disp->reg_lo vin[0]
													_dest.code << 0x66 << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
												} else if (ous == 24) {
													// SHUFPD vin[0] vin[1]
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[1])) << 0x00;
													// MOVAPD disp->reg_lo vin[0]
													_dest.code << 0x66 << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
													// MOVSD disp->reg_hi vin[2]
													_dest.code << 0xF2 << 0x0F << 0x10 << make_mod(xmm_register_code(disp->reg_hi), 3, xmm_register_code(vin[2]));
												} else if (ous == 32) {
													// SHUFPD vin[0] vin[1]
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[1])) << 0x00;
													// MOVAPD disp->reg_lo vin[0]
													_dest.code << 0x66 << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
													// SHUFPD vin[2] vin[3]
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[2]), 3, xmm_register_code(vin[3])) << 0x00;
													// MOVAPD disp->reg_hi vin[2]
													_dest.code << 0x66 << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_hi), 3, xmm_register_code(vin[2]));
												} else throw InvalidArgumentException();
											} else {
												if (ous == 4) {
													// MOVSS disp->reg_lo vin[0]
													_dest.code << 0xF3 << 0x0F << 0x10 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
												} else if (ous == 8) {
													// SHUFPS vin[0] vin[1]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[1])) << 0x00;
													// SHUFPS vin[0] vin[0]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[0])) << 0x08;
													// MOVAPS disp->reg_lo vin[0]
													_dest.code << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
												} else if (ous == 12) {
													// SHUFPS vin[0] vin[1]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[1])) << 0x00;
													// SHUFPS vin[0] vin[2]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[2])) << 0x08;
													// MOVAPS disp->reg_lo vin[0]
													_dest.code << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
												} else if (ous == 16) {
													// SHUFPS vin[0] vin[1]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[1])) << 0x00;
													// SHUFPS vin[2] vin[3]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[2]), 3, xmm_register_code(vin[3])) << 0x00;
													// SHUFPS vin[0] vin[2]
													_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(vin[0]), 3, xmm_register_code(vin[2])) << 0x88;
													// MOVAPS disp->reg_lo vin[0]
													_dest.code << 0x0F << 0x28 << make_mod(xmm_register_code(disp->reg_lo), 3, xmm_register_code(vin[0]));
												} else throw InvalidArgumentException();
											}
										}
									}
									for (int i = dim - 1; i >= 0; i--) encode_restore(vin[i], vreg_in_use, 0, !idle);
									for (int i = dim - 1; i >= 0; i--) if (xin[i]) encode_restore(xin[i], reg_in_use, 0, !idle);
								} else if (node.self.index == TransformFloatScatter) {
									if (node.inputs.Length() < 1) throw InvalidArgumentException();
									int ins = object_size(node.input_specs[0].size);
									int dim = (node.self.ref_flags & ReferenceFlagLong) ? ins / 8 : ins / 4;
									if (dim == 0 || dim > 4 || node.inputs.Length() != dim + 1) throw InvalidArgumentException();
									VectorDisposition a;
									a.size = ins;
									a.reg_lo = disp->reg_lo;
									a.reg_hi = disp->reg_hi;
									if (a.reg_lo == Reg64::NO) allocate_xmm(vreg_in_use, 0, a);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
									Reg ar[4] = { Reg64::RAX, Reg64::RDX, Reg64::RCX, Reg64::RDI };
									Reg irx = Reg64::RBX;
									Reg irv = allocate_xmm(vreg_in_use | a.reg_lo | a.reg_hi, a.reg_lo | a.reg_hi);
									for (int i = 0; i < dim; i++) encode_preserve(ar[i], reg_in_use, 0, !idle);
									auto local_reg_in_use = reg_in_use;
									if (!idle) _encode_floating_point_preserve(vreg_in_use, 0);
									for (int i = 0; i < dim; i++) {
										InternalDisposition ld;
										ld.size = object_size(node.input_specs[1 + i].size);
										ld.reg = ar[i];
										ld.flags = DispositionPointer;
										_encode_tree_node(node.inputs[1 + i], idle, mem_load, &ld, local_reg_in_use);
										local_reg_in_use |= ld.reg;
									}
									if (!idle) _encode_floating_point_restore(vreg_in_use, 0);
									encode_preserve(irv, vreg_in_use, 0, !idle);
									encode_preserve(irx, reg_in_use, 0, !idle);
									if (!idle) for (int i = 0; i < dim; i++) {
										auto & spec = node.input_specs[1 + i];
										auto quant = object_size(spec.size);
										Reg addr = ar[i];
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (i == 0) {
												// MOVSD irv a.reg_lo
												_dest.code << 0xF2 << 0x0F << 0x10 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_lo));
											} else if (i == 1) {
												// MOVHLPS irv a.reg_lo
												_dest.code << 0x0F << 0x12 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_lo));
											} else if (i == 2) {
												// MOVSD irv a.reg_hi
												_dest.code << 0xF2 << 0x0F << 0x10 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_hi));
											} else if (i == 3) {
												// MOVHLPS irv a.reg_hi
												_dest.code << 0x0F << 0x12 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_hi));
											}
											if (spec.semantics == ArgumentSemantics::FloatingPoint) {
												if (quant == 4) {
													// CVTSD2SS irv irv
													_dest.code << 0xF2 << 0x0F << 0x5A << make_mod(xmm_register_code(irv), 3, xmm_register_code(irv));
												}
											} else {
												// CVTTSD2SI irx irv
												_dest.code << 0xF2;
												if (quant == 8) _dest.code << make_rex(true, 0, 0, 0);
												_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(irx) & 7, 3, xmm_register_code(irv));
											}
										} else {
											if (i == 0) {
												// MOVSS irv a.reg_lo
												_dest.code << 0xF3 << 0x0F << 0x10 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_lo));
											} else if (i == 1) {
												// MOVAPS irv a.reg_lo
												_dest.code << 0x0F << 0x28 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_lo));
												// SHUFPS irv irv
												_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(irv), 3, xmm_register_code(irv)) << 0x01;
											} else if (i == 2) {
												// MOVAPS irv a.reg_lo
												_dest.code << 0x0F << 0x28 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_lo));
												// SHUFPS irv irv
												_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(irv), 3, xmm_register_code(irv)) << 0x02;
											} else if (i == 3) {
												// MOVAPS irv a.reg_lo
												_dest.code << 0x0F << 0x28 << make_mod(xmm_register_code(irv), 3, xmm_register_code(a.reg_lo));
												// SHUFPS irv irv
												_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(irv), 3, xmm_register_code(irv)) << 0x03;
											}
											if (spec.semantics == ArgumentSemantics::FloatingPoint) {
												if (quant == 8) {
													// CVTSS2SD irv irv
													_dest.code << 0xF3 << 0x0F << 0x5A << make_mod(xmm_register_code(irv), 3, xmm_register_code(irv));
												}
											} else {
												// CVTTSS2SI irx irv
												_dest.code << 0xF3;
												if (quant == 8) _dest.code << make_rex(true, 0, 0, 0);
												_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(irx) & 7, 3, xmm_register_code(irv));
											}
										}
										if (spec.semantics != ArgumentSemantics::FloatingPoint) {
											encode_mov_mem_reg(quant, addr, irx);
										} else encode_mov_mem_xmm(quant, addr, 0, irv);
									}
									encode_restore(irx, reg_in_use, 0, !idle);
									encode_restore(irv, vreg_in_use, 0, !idle);
									for (int i = dim - 1; i >= 0; i--) encode_restore(ar[i], reg_in_use, 0, !idle);
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else if (node.self.index == TransformFloatRecombine) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									if (node.inputs[1].self.ref_class != ReferenceLiteral) throw InvalidArgumentException();
									auto ins = object_size(node.input_specs[0].size);
									auto ous = object_size(node.retval_spec.size);
									auto rec = node.input_specs[1].size.num_bytes;
									VectorDisposition a;
									a.size = ins;
									a.reg_lo = disp->reg_lo ? disp->reg_lo : allocate_xmm(vreg_in_use, 0);
									a.reg_hi = disp->reg_hi ? disp->reg_hi : (max(ins, ous) > 16 ? allocate_xmm(vreg_in_use, a.reg_lo) : Reg64::NO);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
									if ((node.self.ref_flags & ReferenceFlagLong) && ins > 16) *mem_load += 32;
									if (!idle && disp->reg_lo) {
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (ins <= 16) {
												if (ous <= 16) {
													// SHUFPD a.reg_lo a.reg_lo
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
													_dest.code << ((rec & 0x1) | ((rec & 0x10) >> 3));
												} else {
													// MOVAPD a.reg_hi a.reg_lo
													_dest.code << 0x66 << 0x0F << 0x28 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(a.reg_lo));
													// SHUFPD a.reg_lo a.reg_lo
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
													_dest.code << ((rec & 0x1) | ((rec & 0x10) >> 3));
													// SHUFPD a.reg_hi a.reg_hi
													_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(a.reg_hi));
													_dest.code << (((rec & 0x100) >> 8) | ((rec & 0x1000) >> 11));
												}
											} else {
												auto offset = allocate_temporary(XA::TH::MakeSize(32, 0));
												encode_mov_mem_xmm(16, Reg64::RBP, offset, a.reg_lo);
												encode_mov_mem_xmm(16, Reg64::RBP, offset + 16, a.reg_hi);
												if (ous >= 8) {
													encode_mov_xmm_mem(8, a.reg_lo, Reg64::RBP, offset + ((rec & 0x3) << 3));
													if (ous >= 16) {
														encode_mov_xmm_mem_hi(8, a.reg_lo, Reg64::RBP, offset + ((rec & 0x30) >> 1));
														if (ous >= 24) {
															encode_mov_xmm_mem(8, a.reg_hi, Reg64::RBP, offset + ((rec & 0x300) >> 5));
															if (ous == 32) encode_mov_xmm_mem_hi(8, a.reg_hi, Reg64::RBP, offset + ((rec & 0x3000) >> 9));
														}
													}
												}
											}
										} else {
											// SHUFPS a.reg_lo a.reg_lo
											_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											_dest.code << ((rec & 0x3) | ((rec & 0x30) >> 2) | ((rec & 0x300) >> 4) | ((rec & 0x3000) >> 6));
										}
									}
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else if (node.self.index == TransformFloatAbs || node.self.index == TransformFloatInverse) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									VectorDisposition a, b;
									a.size = b.size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									a.reg_lo = disp->reg_lo;
									a.reg_hi = disp->reg_hi;
									if (a.reg_lo == Reg64::NO) allocate_xmm(vreg_in_use, 0, a);
									allocate_xmm(vreg_in_use | a.reg_lo | a.reg_hi, a.reg_lo | a.reg_hi, b);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(b.reg_lo, vreg_in_use, 0, !idle);
									encode_preserve(b.reg_hi, vreg_in_use, 0, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &b, reg_in_use, vreg_in_use | uint(b.reg_lo) | uint(b.reg_hi));
									if (!idle) {
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (a.size == 32) {
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(a.reg_hi));
												_dest.code << 0x66 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0x66 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
												if (node.self.index == TransformFloatAbs) {
													_dest.code << 0x66 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
													_dest.code << 0x66 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
												}
											} else if (a.size == 24) {
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(a.reg_hi));
												_dest.code << 0x66 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0xF2 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
												if (node.self.index == TransformFloatAbs) {
													_dest.code << 0x66 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
													_dest.code << 0xF2 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
												}
											} else if (a.size == 16) {
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0x66 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												if (node.self.index == TransformFloatAbs) _dest.code << 0x66 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else if (a.size == 8) {
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0xF2 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												if (node.self.index == TransformFloatAbs) _dest.code << 0xF2 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else throw InvalidArgumentException();
										} else {
											if (a.size == 16 || a.size == 12 || a.size == 8) {
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												if (node.self.index == TransformFloatAbs) _dest.code << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else if (a.size == 4) {
												_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0xF3 << 0x0F << 0x5C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												if (node.self.index == TransformFloatAbs) _dest.code << 0xF3 << 0x0F << 0x5F << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else throw InvalidArgumentException();
										}
									}
									encode_restore(b.reg_hi, vreg_in_use, 0, !idle);
									encode_restore(b.reg_lo, vreg_in_use, 0, !idle);
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else if (node.self.index == TransformFloatSqrt) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									VectorDisposition a;
									a.size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									a.reg_lo = disp->reg_lo;
									a.reg_hi = disp->reg_hi;
									if (a.reg_lo == Reg64::NO) allocate_xmm(vreg_in_use, 0, a);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
									if (!idle) {
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (a.size == 32) {
												_dest.code << 0x66 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0x66 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(a.reg_hi));
											} else if (a.size == 24) {
												_dest.code << 0x66 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												_dest.code << 0xF2 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(a.reg_hi));
											} else if (a.size == 16) {
												_dest.code << 0x66 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else if (a.size == 8) {
												_dest.code << 0xF2 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else throw InvalidArgumentException();
										} else {
											if (a.size == 16 || a.size == 12 || a.size == 8) {
												_dest.code << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else if (a.size == 4) {
												_dest.code << 0xF3 << 0x0F << 0x51 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else throw InvalidArgumentException();
										}
									}
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else if (node.self.index == TransformFloatReduce) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									VectorDisposition a;
									a.size = object_size(node.input_specs[0].size);
									a.reg_lo = disp->reg_lo;
									a.reg_hi = disp->reg_hi;
									if (a.reg_lo == Reg64::NO) a.reg_lo = allocate_xmm(vreg_in_use, 0);
									if (a.reg_hi == Reg64::NO && a.size > 16) a.reg_hi = allocate_xmm(vreg_in_use | a.reg_lo, a.reg_lo);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
									if (!idle) {
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (a.size == 32) {
												// HADDPD a.reg_lo a.reg_hi
												_dest.code << 0x66 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_hi));
												// HADDPD a.reg_lo a.reg_lo
												_dest.code << 0x66 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else if (a.size == 24) {
												// HADDPD a.reg_lo a.reg_lo
												_dest.code << 0x66 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												// ADDSD a.reg_lo a.reg_hi
												_dest.code << 0xF2 << 0x0F << 0x58 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_hi));
											} else if (a.size == 16) {
												// HADDPD a.reg_lo a.reg_lo
												_dest.code << 0x66 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else if (a.size == 8) {
											} else throw InvalidArgumentException();
										} else {
											if (a.size == 16) {
												// HADDPS a.reg_lo a.reg_lo
												_dest.code << 0xF2 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
												// HADDPS a.reg_lo a.reg_lo
												_dest.code << 0xF2 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else if (a.size == 12) {
												auto aux = allocate_xmm(vreg_in_use | a.reg_lo, a.reg_lo);
												encode_preserve(aux, vreg_in_use, 0, true);
												// HADDPS aux a.reg_lo
												_dest.code << 0xF2 << 0x0F << 0x7C << make_mod(xmm_register_code(aux), 3, xmm_register_code(a.reg_lo));
												// ADDPS a.reg_lo aux
												_dest.code << 0x0F << 0x58 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(aux));
												// SHUFPS a.reg_lo a.reg_lo
												_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo)) << 0xAA;
												encode_restore(aux, vreg_in_use, 0, true);
											} else if (a.size == 8) {
												// HADDPS a.reg_lo a.reg_lo
												_dest.code << 0xF2 << 0x0F << 0x7C << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(a.reg_lo));
											} else if (a.size == 4) {
											} else throw InvalidArgumentException();
										}
									}
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else if (node.self.index == TransformFloatAdd || node.self.index == TransformFloatSubt || node.self.index == TransformFloatMul || node.self.index == TransformFloatDiv) {
									uint8 op;
									if (node.self.index == TransformFloatAdd) op = 0x58;
									else if (node.self.index == TransformFloatSubt) op = 0x5C;
									else if (node.self.index == TransformFloatMul) op = 0x59;
									else if (node.self.index == TransformFloatDiv) op = 0x5E;
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									VectorDisposition a, b;
									a.size = b.size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									a.reg_lo = disp->reg_lo;
									a.reg_hi = disp->reg_hi;
									if (a.reg_lo == Reg64::NO) allocate_xmm(vreg_in_use, 0, a);
									allocate_xmm(vreg_in_use | a.reg_lo | a.reg_hi, a.reg_lo | a.reg_hi, b);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(b.reg_lo, vreg_in_use, 0, !idle);
									encode_preserve(b.reg_hi, vreg_in_use, 0, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
									_encode_floating_point(node.inputs[1], idle, mem_load, &b, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi) | uint(b.reg_lo) | uint(b.reg_hi));
									if (!idle) {
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (a.size == 32) {
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
											} else if (a.size == 24) {
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0xF2 << 0x0F << op << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
											} else if (a.size == 16) {
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else if (a.size == 8) {
												_dest.code << 0xF2 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else throw InvalidArgumentException();
										} else {
											if (a.size == 16 || a.size == 12 || a.size == 8) {
												_dest.code << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else if (a.size == 4) {
												_dest.code << 0xF3 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
											} else throw InvalidArgumentException();
										}
									}
									encode_restore(b.reg_hi, vreg_in_use, 0, !idle);
									encode_restore(b.reg_lo, vreg_in_use, 0, !idle);
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else if (node.self.index == TransformFloatMulAdd || node.self.index == TransformFloatMulSubt) {
									uint8 op;
									uint8 opm = 0x59;
									if (node.self.index == TransformFloatMulAdd) op = 0x58;
									else if (node.self.index == TransformFloatMulSubt) op = 0x5C;
									if (node.inputs.Length() != 3) throw InvalidArgumentException();
									VectorDisposition a, b, c;
									a.size = b.size = c.size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									a.reg_lo = disp->reg_lo;
									a.reg_hi = disp->reg_hi;
									if (a.reg_lo == Reg64::NO) allocate_xmm(vreg_in_use, 0, a);
									allocate_xmm(vreg_in_use | a.reg_lo | a.reg_hi, a.reg_lo | a.reg_hi, b);
									allocate_xmm(vreg_in_use | a.reg_lo | a.reg_hi | b.reg_lo | b.reg_hi, a.reg_lo | a.reg_hi | b.reg_lo | b.reg_hi, c);
									encode_preserve(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_preserve(b.reg_lo, vreg_in_use, 0, !idle);
									encode_preserve(b.reg_hi, vreg_in_use, 0, !idle);
									encode_preserve(c.reg_lo, vreg_in_use, 0, !idle);
									encode_preserve(c.reg_hi, vreg_in_use, 0, !idle);
									_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi));
									_encode_floating_point(node.inputs[1], idle, mem_load, &b, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi) | uint(b.reg_lo) | uint(b.reg_hi));
									_encode_floating_point(node.inputs[2], idle, mem_load, &c, reg_in_use, vreg_in_use | uint(a.reg_lo) | uint(a.reg_hi) | uint(b.reg_lo) | uint(b.reg_hi) | uint(c.reg_lo) | uint(c.reg_hi));
									if (!idle) {
										if (node.self.ref_flags & ReferenceFlagLong) {
											if (a.size == 32) {
												_dest.code << 0x66 << 0x0F << opm << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0x66 << 0x0F << opm << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(c.reg_lo));
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(c.reg_hi));
											} else if (a.size == 24) {
												_dest.code << 0x66 << 0x0F << opm << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0xF2 << 0x0F << opm << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi));
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(c.reg_lo));
												_dest.code << 0xF2 << 0x0F << op << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(c.reg_hi));
											} else if (a.size == 16) {
												_dest.code << 0x66 << 0x0F << opm << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0x66 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(c.reg_lo));
											} else if (a.size == 8) {
												_dest.code << 0xF2 << 0x0F << opm << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0xF2 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(c.reg_lo));
											} else throw InvalidArgumentException();
										} else {
											if (a.size == 16 || a.size == 12 || a.size == 8) {
												_dest.code << 0x0F << opm << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(c.reg_lo));
											} else if (a.size == 4) {
												_dest.code << 0xF3 << 0x0F << opm << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo));
												_dest.code << 0xF3 << 0x0F << op << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(c.reg_lo));
											} else throw InvalidArgumentException();
										}
									}
									encode_restore(c.reg_hi, vreg_in_use, 0, !idle);
									encode_restore(c.reg_lo, vreg_in_use, 0, !idle);
									encode_restore(b.reg_hi, vreg_in_use, 0, !idle);
									encode_restore(b.reg_lo, vreg_in_use, 0, !idle);
									encode_restore(a.reg_hi, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
									encode_restore(a.reg_lo, vreg_in_use, disp->reg_lo | disp->reg_hi, !idle);
								} else throw InvalidArgumentException();
								return;
							}
						}
					}
					InternalDisposition idisp;
					idisp.size = disp->size;
					if (disp->reg_lo == Reg64::NO) {
						idisp.flags = DispositionDiscard;
						idisp.reg = Reg64::NO;
					} else {
						idisp.flags = DispositionPointer;
						idisp.reg = Reg64::RSI;
					}
					encode_preserve(idisp.reg, reg_in_use, 0, !idle);
					if (!idle && (node.self.ref_flags & ReferenceFlagInvoke)) _encode_floating_point_preserve(vreg_in_use, disp);
					_encode_tree_node(node, idle, mem_load, &idisp, reg_in_use | uint(idisp.reg));
					if (!idle && idisp.reg != Reg64::NO) {
						if (disp->size > 16) {
							encode_mov_xmm_mem(16, disp->reg_lo, idisp.reg, 0);
							encode_mov_xmm_mem(disp->size - 16, disp->reg_hi, idisp.reg, 16);
						} else encode_mov_xmm_mem(disp->size, disp->reg_lo, idisp.reg, 0);
					}
					if (!idle && (node.self.ref_flags & ReferenceFlagInvoke)) _encode_floating_point_restore(vreg_in_use, disp);
					encode_restore(idisp.reg, reg_in_use, 0, !idle);
				}
				void _encode_floating_point_ir(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformFloatInteger) {
						if (node.inputs.Length() != 1) throw InvalidArgumentException();
						uint8 cnv_prefix = (node.self.ref_flags & ReferenceFlagLong) ? 0xF2 : 0xF3;
						int nx;
						VectorDisposition a;
						a.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						allocate_xmm(0, 0, a);
						_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, uint(a.reg_lo) | uint(a.reg_hi));
						if (node.self.ref_flags & ReferenceFlagLong) nx = a.size / 8; else nx = a.size / 4;
						auto rvs = object_size(node.retval_spec.size);
						if (rvs % nx) throw InvalidArgumentException();
						if ((disp->flags & DispositionRegister) && nx == 1) {
							disp->flags = DispositionRegister;
							if (rvs == 8) {
								if (!idle) {
									_dest.code << cnv_prefix << make_rex(true, regular_register_code(disp->reg) & 8, 0, 0);
									_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(disp->reg) & 7, 3, xmm_register_code(a.reg_lo));
								}
							} else if (rvs == 4) {
								if (!idle) {
									_dest.code << cnv_prefix << make_rex(false, regular_register_code(disp->reg) & 8, 0, 0);
									_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(disp->reg) & 7, 3, xmm_register_code(a.reg_lo));
								}
							} else if (rvs == 2) {
								if (!idle) {
									_dest.code << cnv_prefix << make_rex(false, regular_register_code(disp->reg) & 8, 0, 0);
									_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(disp->reg) & 7, 3, xmm_register_code(a.reg_lo));
									encode_and(disp->reg, 0xFFFF);
								}
							} else if (rvs == 1) {
								if (!idle) {
									_dest.code << cnv_prefix << make_rex(false, regular_register_code(disp->reg) & 8, 0, 0);
									_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(disp->reg) & 7, 3, xmm_register_code(a.reg_lo));
									encode_and(disp->reg, 0xFF);
								}
							} else throw InvalidArgumentException();
						} else {
							auto rqs = rvs / nx;
							if (rqs != 8 && rqs != 4 && rqs != 2 && rqs != 1) throw InvalidArgumentException();
							auto rvs_padded = word_align(node.retval_spec.size);
							(*mem_load) += rvs_padded;
							int offs;
							if (!idle) {
								offs = allocate_temporary(node.retval_spec.size);
								Reg reg = disp->reg;
								if (reg == Reg64::NO) reg = Reg64::RAX;
								encode_preserve(reg, reg_in_use, disp->reg, true);
								for (int i = 0; i < nx; i++) {
									Reg xmm;
									if (node.self.ref_flags & ReferenceFlagLong) {
										xmm = i & 2 ? a.reg_hi : a.reg_lo;
										if (i & 1) {
											// SHUFPD xmm xmm
											_dest.code << 0x66 << 0x0F << 0xC6 << make_mod(xmm_register_code(xmm), 3, xmm_register_code(xmm)) << 0x01;
										}
									} else {
										xmm = a.reg_lo;
										if (i) {
											// SHUFPS xmm xmm
											_dest.code << 0x0F << 0xC6 << make_mod(xmm_register_code(xmm), 3, xmm_register_code(xmm)) << 0x39;
										}
									}
									_dest.code << cnv_prefix << make_rex(rqs == 8, regular_register_code(reg) & 8, 0, 0);
									_dest.code << 0x0F << 0x2C << make_mod(regular_register_code(reg) & 7, 3, xmm_register_code(xmm));
									encode_mov_mem_reg(rqs, Reg64::RBP, offs + i * rqs, reg);
								}
								encode_restore(reg, reg_in_use, disp->reg, true);
							}
							if (disp->flags & DispositionPointer) {
								disp->flags = DispositionPointer;
								if (!idle) encode_lea(disp->reg, Reg64::RBP, offs);
							} else if (disp->flags & DispositionRegister) {
								disp->flags = DispositionRegister;
								if (!idle) encode_mov_reg_mem(min(rvs, 8U), disp->reg, Reg64::RBP, offs);
							} else disp->flags = DispositionDiscard;
						}
					} else if (node.self.index == TransformFloatIsZero || node.self.index == TransformFloatNotZero) {
						uint8 result_mask;
						bool invert = (node.self.index == TransformFloatNotZero);
						if (node.inputs.Length() != 1) throw InvalidArgumentException();
						VectorDisposition a, b;
						Reg acc1, acc2;
						a.size = b.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						allocate_xmm(0, 0, a);
						allocate_xmm(a.reg_lo | a.reg_hi, a.reg_lo | a.reg_hi, b);
						_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, uint(a.reg_lo) | uint(a.reg_hi));
						if (disp->reg != Reg64::NO) acc1 = disp->reg; else acc1 = Reg64::RAX;
						if (a.reg_hi != Reg64::NO) { if (acc1 == Reg64::RAX) acc2 = Reg64::RDX; else acc2 = Reg64::RAX; }
						encode_preserve(acc1, reg_in_use, disp->reg, !idle);
						encode_preserve(acc2, reg_in_use, disp->reg, !idle);
						if (!idle) {
							_dest.code << 0x0F << 0x57 << make_mod(xmm_register_code(b.reg_lo), 3, xmm_register_code(b.reg_lo));
							if (node.self.ref_flags & ReferenceFlagLong) {
								if (a.size == 32) {
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc2) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc2) & 0x7, 3, xmm_register_code(a.reg_hi));
									encode_shl(acc2, 2);
									encode_operation(8, arOp::OR, acc1, acc2);
									result_mask = 0xF;
								} else if (a.size == 24) {
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << 0xF2 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc2) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc2) & 0x7, 3, xmm_register_code(a.reg_hi));
									encode_shl(acc2, 2);
									encode_operation(8, arOp::OR, acc1, acc2);
									encode_and(acc1, 7);
									result_mask = 0x7;
								} else if (a.size == 16) {
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									result_mask = 0x3;
								} else if (a.size == 8) {
									_dest.code << 0xF2 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									encode_and(acc1, 1);
									result_mask = 0x1;
								} else throw InvalidArgumentException();
							} else {
								if (a.size == 16 || a.size == 12 || a.size == 8) {
									_dest.code << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									if (a.size == 16) result_mask = 0xF;
									else if (a.size == 12) { encode_and(acc1, 7); result_mask = 0x7; }
									else if (a.size == 8) { encode_and(acc1, 3); result_mask = 0x3; }
								} else if (a.size == 4) {
									_dest.code << 0xF3 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << 0;
									_dest.code << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									encode_and(acc1, 1);
									result_mask = 0x1;
								} else throw InvalidArgumentException();
							}
							if ((node.self.ref_flags & ReferenceFlagVectorCom) || result_mask == 1) {
								if (invert) encode_xor(acc1, result_mask);
							} else {
								if (!invert) encode_xor(acc1, result_mask);
								encode_test(8, acc1, result_mask);
								_dest.code << 0x74; // JZ
								_dest.code << 0x00;
								int addr = _dest.code.Length();
								encode_mov_reg_const(8, acc1, 0);
								_dest.code << 0xEB; // JMP
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								encode_mov_reg_const(8, acc1, 1);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
							}
						}
						if (_word_align(node.retval_spec.size) != 8) throw InvalidArgumentException();
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += 8;
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size);
								encode_mov_mem_reg(8, Reg64::RBP, offs, acc1);
								encode_lea(acc1, Reg64::RBP, offs);
							}
						} else disp->flags = DispositionDiscard;
						encode_restore(acc2, reg_in_use, disp->reg, !idle);
						encode_restore(acc1, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformFloatEQ || node.self.index == TransformFloatNEQ ||
						node.self.index == TransformFloatLE || node.self.index == TransformFloatGE ||
						node.self.index == TransformFloatL || node.self.index == TransformFloatG) {
						uint8 result_mask;
						uint8 comparator;
						bool invert;
						if (node.self.index == TransformFloatEQ || node.self.index == TransformFloatNEQ) {
							comparator = 0;
							invert = (node.self.index == TransformFloatNEQ);
						} else if (node.self.index == TransformFloatL || node.self.index == TransformFloatGE) {
							comparator = 1;
							invert = (node.self.index == TransformFloatGE);
						} else if (node.self.index == TransformFloatLE || node.self.index == TransformFloatG) {
							comparator = 2;
							invert = (node.self.index == TransformFloatG);
						}
						if (node.inputs.Length() != 2) throw InvalidArgumentException();
						VectorDisposition a, b;
						Reg acc1, acc2;
						a.size = b.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						allocate_xmm(0, 0, a);
						allocate_xmm(a.reg_lo | a.reg_hi, a.reg_lo | a.reg_hi, b);
						_encode_floating_point(node.inputs[0], idle, mem_load, &a, reg_in_use, uint(a.reg_lo) | uint(a.reg_hi));
						_encode_floating_point(node.inputs[1], idle, mem_load, &b, reg_in_use, uint(a.reg_lo) | uint(a.reg_hi) | uint(b.reg_lo) | uint(b.reg_hi));
						if (disp->reg != Reg64::NO) acc1 = disp->reg; else acc1 = Reg64::RAX;
						if (a.reg_hi != Reg64::NO) { if (acc1 == Reg64::RAX) acc2 = Reg64::RDX; else acc2 = Reg64::RAX; }
						encode_preserve(acc1, reg_in_use, disp->reg, !idle);
						encode_preserve(acc2, reg_in_use, disp->reg, !idle);
						if (!idle) {
							if (node.self.ref_flags & ReferenceFlagLong) {
								if (a.size == 32) {
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << comparator;
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi)) << comparator;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc2) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc2) & 0x7, 3, xmm_register_code(a.reg_hi));
									encode_shl(acc2, 2);
									encode_operation(8, arOp::OR, acc1, acc2);
									result_mask = 0xF;
								} else if (a.size == 24) {
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << comparator;
									_dest.code << 0xF2 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_hi), 3, xmm_register_code(b.reg_hi)) << comparator;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc2) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc2) & 0x7, 3, xmm_register_code(a.reg_hi));
									encode_shl(acc2, 2);
									encode_operation(8, arOp::OR, acc1, acc2);
									encode_and(acc1, 7);
									result_mask = 0x7;
								} else if (a.size == 16) {
									_dest.code << 0x66 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << comparator;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									result_mask = 0x3;
								} else if (a.size == 8) {
									_dest.code << 0xF2 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << comparator;
									_dest.code << 0x66 << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									encode_and(acc1, 1);
									result_mask = 0x1;
								} else throw InvalidArgumentException();
							} else {
								if (a.size == 16 || a.size == 12 || a.size == 8) {
									_dest.code << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << comparator;
									_dest.code << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									if (a.size == 16) result_mask = 0xF;
									else if (a.size == 12) { encode_and(acc1, 7); result_mask = 0x7; }
									else if (a.size == 8) { encode_and(acc1, 3); result_mask = 0x3; }
								} else if (a.size == 4) {
									_dest.code << 0xF3 << 0x0F << 0xC2 << make_mod(xmm_register_code(a.reg_lo), 3, xmm_register_code(b.reg_lo)) << comparator;
									_dest.code << make_rex(true, regular_register_code(acc1) & 0x8, 0, 0) << 0x0F << 0x50 << make_mod(regular_register_code(acc1) & 0x7, 3, xmm_register_code(a.reg_lo));
									encode_and(acc1, 1);
									result_mask = 0x1;
								} else throw InvalidArgumentException();
							}
							if ((node.self.ref_flags & ReferenceFlagVectorCom) || result_mask == 1) {
								if (invert) encode_xor(acc1, result_mask);
							} else {
								if (!invert) encode_xor(acc1, result_mask);
								encode_test(8, acc1, result_mask);
								_dest.code << 0x74; // JZ
								_dest.code << 0x00;
								int addr = _dest.code.Length();
								encode_mov_reg_const(8, acc1, 0);
								_dest.code << 0xEB; // JMP
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								encode_mov_reg_const(8, acc1, 1);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
							}
						}
						if (_word_align(node.retval_spec.size) != 8) throw InvalidArgumentException();
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += 8;
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size);
								encode_mov_mem_reg(8, Reg64::RBP, offs, acc1);
								encode_lea(acc1, Reg64::RBP, offs);
							}
						} else disp->flags = DispositionDiscard;
						encode_restore(acc2, reg_in_use, disp->reg, !idle);
						encode_restore(acc1, reg_in_use, disp->reg, !idle);
					} else throw InvalidArgumentException();
				}
				void _encode_tree_node(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.ref_flags & ReferenceFlagInvoke) {
						if (node.self.ref_class == ReferenceTransform) {
							if (node.self.index >= 0x001 && node.self.index < 0x010) {
								if (node.self.index == TransformFollowPointer) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									if (disp->flags & DispositionPointer) {
										InternalDisposition src;
										src.flags = DispositionRegister;
										src.reg = disp->reg;
										src.size = 8;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										InternalDisposition src;
										src.flags = DispositionRegister;
										src.reg = disp->reg;
										src.size = 8;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										if (!idle) encode_mov_reg_mem(8, src.reg, src.reg);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										InternalDisposition src;
										src.flags = DispositionDiscard;
										src.reg = Reg64::NO;
										src.size = 0;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformTakePointer) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									if (disp->flags & DispositionRegister) {
										InternalDisposition src;
										src.flags = DispositionPointer;
										src.reg = disp->reg;
										src.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionPointer) {
										InternalDisposition src;
										src.flags = DispositionPointer;
										src.reg = disp->reg;
										src.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										*mem_load += _word_align(TH::MakeSize(0, 1));
										if (!idle) {
											int offset = allocate_temporary(TH::MakeSize(0, 1));
											encode_mov_mem_reg(8, Reg64::RBP, offset, src.reg);
											encode_lea(src.reg, Reg64::RBP, offset);
										}
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionDiscard) {
										InternalDisposition src;
										src.flags = DispositionDiscard;
										src.reg = Reg64::NO;
										src.size = 0;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformAddressOffset) {
									if (node.inputs.Length() < 2 || node.inputs.Length() > 3) throw InvalidArgumentException();
									InternalDisposition base;
									base.flags = DispositionPointer;
									base.reg = Reg64::RSI;
									base.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									encode_preserve(base.reg, reg_in_use, disp->reg, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &base, reg_in_use | uint(base.reg));
									if (node.inputs[1].self.ref_class == ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint offset = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (!idle && offset * scale) {
											if (scale != 0xFFFFFFFF) encode_add(Reg64::RSI, offset * scale);
											else encode_add(Reg64::RSI, -offset);
										}
									} else if (node.inputs[1].self.ref_class != ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										encode_preserve(Reg64::RAX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RDX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RCX, reg_in_use, 0, !idle && scale > 1);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg64::RAX;
											offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg64::RSI) | uint(Reg64::RAX) | uint(Reg64::RDX) | uint(Reg64::RCX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(8, Reg64::RCX, scale);
													encode_mul_div(8, mdOp::MUL, Reg64::RCX);
												}
												encode_operation(8, arOp::ADD, Reg64::RSI, Reg64::RAX);
											}
										}
										encode_restore(Reg64::RCX, reg_in_use, 0, !idle && scale > 1);
										encode_restore(Reg64::RDX, reg_in_use, 0, !idle);
										encode_restore(Reg64::RAX, reg_in_use, 0, !idle);
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										encode_preserve(Reg64::RAX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RDX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RCX, reg_in_use, 0, !idle && scale > 1);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg64::RAX;
											offset.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(Reg64::RSI) | uint(Reg64::RAX) | uint(Reg64::RDX) | uint(Reg64::RCX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(8, Reg64::RCX, scale);
													encode_mul_div(8, mdOp::MUL, Reg64::RCX);
												}
												encode_operation(8, arOp::ADD, Reg64::RSI, Reg64::RAX);
											}
										}
										encode_restore(Reg64::RCX, reg_in_use, 0, !idle && scale > 1);
										encode_restore(Reg64::RDX, reg_in_use, 0, !idle);
										encode_restore(Reg64::RAX, reg_in_use, 0, !idle);
									} else {
										encode_preserve(Reg64::RAX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RDX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RCX, reg_in_use, 0, !idle);
										InternalDisposition offset;
										offset.flags = DispositionRegister;
										offset.reg = Reg64::RAX;
										offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										InternalDisposition scale;
										scale.flags = DispositionRegister;
										scale.reg = Reg64::RCX;
										scale.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (offset.size != 8 || scale.size != 8) throw InvalidArgumentException();
										_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg64::RSI) | uint(Reg64::RAX) | uint(Reg64::RDX) | uint(Reg64::RCX));
										_encode_tree_node(node.inputs[2], idle, mem_load, &scale, reg_in_use | uint(Reg64::RSI) | uint(Reg64::RAX) | uint(Reg64::RDX) | uint(Reg64::RCX));
										if (!idle) {
											encode_mul_div(8, mdOp::MUL, Reg64::RCX);
											encode_operation(8, arOp::ADD, Reg64::RSI, Reg64::RAX);
										}
										encode_restore(Reg64::RCX, reg_in_use, 0, !idle);
										encode_restore(Reg64::RDX, reg_in_use, 0, !idle);
										encode_restore(Reg64::RAX, reg_in_use, 0, !idle);
									}
									if (disp->flags & DispositionPointer) {
										if (disp->reg != Reg64::RSI && !idle) encode_mov_reg_reg(8, disp->reg, Reg64::RSI);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) {
											if (disp->reg != Reg64::RSI) {
												encode_blt(disp->reg, false, Reg64::RSI, true, disp->size, reg_in_use | uint(Reg64::RSI));
											} else {
												encode_preserve(Reg64::RDI, reg_in_use, 0, true);
												encode_mov_reg_reg(8, Reg64::RDI, Reg64::RSI);
												encode_blt(Reg64::RSI, false, Reg64::RDI, true, disp->size, reg_in_use | uint(Reg64::RDI) | uint(Reg64::RSI));
												encode_restore(Reg64::RDI, reg_in_use, 0, true);
											}
										}
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
									encode_restore(base.reg, reg_in_use, disp->reg, !idle);
								} else if (node.self.index == TransformBlockTransfer) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									uint size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									InternalDisposition dest_d, src_d;
									dest_d = *disp;
									if (dest_d.reg == Reg64::NO) dest_d.reg = Reg64::RAX;
									dest_d.size = size;
									dest_d.flags = DispositionPointer;
									src_d.flags = DispositionAny;
									src_d.size = size;
									src_d.reg = Reg64::RBX;
									if (src_d.reg == dest_d.reg) src_d.reg = Reg64::RAX;
									if (!idle) {
										encode_preserve(dest_d.reg, reg_in_use, 0, disp->reg == Reg64::NO);
										encode_preserve(src_d.reg, reg_in_use, 0, true);
									}
									_encode_tree_node(node.inputs[0], idle, mem_load, &dest_d, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
									_encode_tree_node(node.inputs[1], idle, mem_load, &src_d, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
									if (!idle) encode_blt(dest_d.reg, true, src_d.reg, src_d.flags & DispositionPointer, size, reg_in_use);
									if ((disp->flags & DispositionRegister) && !(disp->flags & DispositionPointer)) {
										if (!idle) {
											if (src_d.flags & DispositionPointer) encode_blt(src_d.reg, false, dest_d.reg, true, size, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
											encode_mov_reg_reg(8, dest_d.reg, src_d.reg);
										}
										disp->flags = DispositionRegister;
										disp->reg = dest_d.reg;
										disp->size = size;
									} else {
										if (disp->flags & DispositionAny) *disp = dest_d;
									}
									if (!idle) {
										encode_restore(src_d.reg, reg_in_use, 0, true);
										encode_restore(dest_d.reg, reg_in_use, 0, disp->reg == Reg64::NO);
									}
								} else if (node.self.index == TransformInvoke) {
									_encode_general_call(node, idle, mem_load, disp, reg_in_use);
								} else if (node.self.index == TransformTemporary) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									*mem_load += _word_align(node.retval_spec.size);
									InternalDisposition ld;
									ld.flags = DispositionDiscard;
									ld.reg = Reg64::NO;
									ld.size = 0;
									int offset, index;
									if (!idle) {
										offset = allocate_temporary(node.retval_spec.size, &index);
										LocalDisposition local;
										local.bp_offset = offset;
										local.size = size;
										_init_locals.Push(local);
									}
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use);
									if (!idle) {
										assign_finalizer(index, node.retval_final);
										_init_locals.Pop();
									}
									if (disp->flags & DispositionPointer) {
										if (!idle) encode_lea(disp->reg, Reg64::RBP, offset);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) encode_mov_reg_mem(8, disp->reg, Reg64::RBP, offset);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformBreakIf) {
									if (node.inputs.Length() != 3) throw InvalidArgumentException();
									_encode_tree_node(node.inputs[0], idle, mem_load, disp, reg_in_use);
									InternalDisposition ld;
									ld.flags = DispositionRegister;
									ld.reg = Reg64::RCX;
									ld.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
									encode_preserve(ld.reg, reg_in_use, 0, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									if (!idle) {
										encode_test(ld.size, ld.reg, 0xFFFFFFFF);
										_dest.code << 0x0F; _dest.code << 0x84; // JZ
										_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
										int addr = _dest.code.Length();
										auto scope = _scopes.GetLast();
										while (scope && !scope->GetValue().shift_sp) scope = scope->GetPrevious();
										if (scope) encode_lea(Reg64::RSP, Reg64::RBP, scope->GetValue().frame_base);
										else encode_lea(Reg64::RSP, Reg64::RBP, _scope_frame_base);
										int _preserve_oddity = _stack_oddity;
										_stack_oddity = 0;
										encode_scope_unroll(_current_instruction, _current_instruction + 1 + int(node.input_specs[2].size.num_bytes));
										_stack_oddity = _preserve_oddity;
										_dest.code << 0xE9; // JMP
										_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
										JumpRelocStruct rs;
										rs.machine_offset_at = _dest.code.Length() - 4;
										rs.machine_offset_relative_to = _dest.code.Length();
										rs.xasm_offset_jump_to = _current_instruction + 1 + int(node.input_specs[2].size.num_bytes);
										_jump_reloc << rs;
										*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
									}
									encode_restore(ld.reg, reg_in_use, 0, !idle);
								} else if (node.self.index == TransformSplit) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									InternalDisposition ld = *disp;
									if (ld.flags & DispositionDiscard) {
										ld.flags = DispositionPointer;
										ld.reg = Reg64::RSI;
										ld.size = size;
									}
									encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									*mem_load += _word_align(node.input_specs[0].size);
									Reg dest = ld.reg == Reg64::RDI ? Reg64::RDX : Reg64::RDI;
									if (!idle) {
										int offset = allocate_temporary(node.input_specs[0].size);
										_scopes.GetLast()->GetValue().current_split_offset = offset;
										encode_preserve(dest, reg_in_use | uint(ld.reg), 0, true);
										encode_lea(dest, Reg64::RBP, offset);
										encode_blt(dest, true, ld.reg, ld.flags & DispositionPointer, size, reg_in_use | uint(ld.reg) | uint(dest));
										encode_restore(dest, reg_in_use | uint(ld.reg), 0, true);
									}
									encode_restore(ld.reg, reg_in_use, disp->reg, !idle);
									if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									} else *disp = ld;
								} else throw InvalidArgumentException();
							} else if (node.self.index >= 0x010 && node.self.index < 0x013) {
								_encode_logics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x013 && node.self.index < 0x050) {
								_encode_arithmetics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x080 && node.self.index < 0x100) {
								if (is_vector_retval_transform(node.self.index)) {
									VectorDisposition vdisp;
									vdisp.size = node.retval_spec.size.num_bytes + node.retval_spec.size.num_words * WordSize;
									if (disp->flags & DispositionDiscard) vdisp.reg_lo = vdisp.reg_hi = Reg64::NO; else {
										vdisp.reg_lo = Reg64::XMM0;
										vdisp.reg_hi = vdisp.size > 16 ? Reg64::XMM1 : Reg64::NO;
									}
									_encode_floating_point(node, idle, mem_load, &vdisp, reg_in_use, uint(vdisp.reg_lo) | uint(vdisp.reg_hi));
									if (disp->flags & DispositionPointer) {
										disp->flags = DispositionPointer;
										int offs;
										*mem_load += _word_align(node.retval_spec.size);
										if (!idle) {
											Reg freg;
											if (uint(disp->reg) < 8) freg = disp->reg; else freg = Reg64::RDI;
											offs = allocate_temporary(node.retval_spec.size);
											encode_preserve(freg, reg_in_use, disp->reg, true);
											encode_lea(freg, Reg64::RBP, offs);
											if (vdisp.size > 16) {
												encode_mov_mem_xmm(16, freg, 0, vdisp.reg_lo);
												encode_mov_mem_xmm(vdisp.size - 16, freg, 16, vdisp.reg_hi);
											} else encode_mov_mem_xmm(vdisp.size, freg, 0, vdisp.reg_lo);
											if (disp->reg != freg) encode_mov_reg_reg(8, disp->reg, freg);
											encode_restore(freg, reg_in_use, disp->reg, true);
										}
									} else if (disp->flags & DispositionRegister) {
										disp->flags = DispositionRegister;
										if (!idle) encode_mov_reg_xmm(8, disp->reg, vdisp.reg_lo);
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									} else throw InvalidArgumentException();
								} else _encode_floating_point_ir(node, idle, mem_load, disp, reg_in_use);
							} else throw InvalidArgumentException();
						} else {
							_encode_general_call(node, idle, mem_load, disp, reg_in_use);
						}
					} else {
						if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							if (!idle) encode_put_addr_of(disp->reg, node.self);
						} else if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
							if (!idle) {
								Reg ld = Reg64::RAX;
								if (ld == disp->reg) ld = Reg64::R15;
								encode_preserve(ld, reg_in_use, 0, true);
								encode_put_addr_of(ld, node.self);
								encode_blt(disp->reg, false, ld, true, disp->size, reg_in_use | uint(ld));
								encode_restore(ld, reg_in_use, 0, true);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					}
				}
				void _encode_expression_evaluation(const ExpressionTree & tree, Reg retval_copy)
				{
					if (retval_copy != Reg64::NO && (tree.self.ref_flags & ReferenceFlagInvoke) && _needs_stack_storage(tree.retval_spec)) throw InvalidArgumentException();
					int _temp_storage = 0;
					InternalDisposition disp;
					disp.reg = retval_copy;
					if (retval_copy == Reg64::NO) {
						disp.flags = DispositionDiscard;
						disp.size = 0;
					} else {
						disp.flags = DispositionRegister;
						disp.size = 1;
					}
					_encode_tree_node(tree, true, &_temp_storage, &disp, uint(retval_copy));
					encode_open_scope(_temp_storage, true, 0);
					_encode_tree_node(tree, false, &_temp_storage, &disp, uint(retval_copy));
					encode_close_scope(uint(retval_copy));
				}
			public:
				EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src) : X86::EncoderContext(conv, dest, src, true) {}
				virtual void encode_function_prologue(void) override
				{
					SafePointer< Array<ArgumentPassageInfo> > api = _make_interface_layout(_src.retval, _src.inputs.GetBuffer(), _src.inputs.Length());
					_inputs.SetLength(_src.inputs.Length());
					if (_conv == CallingConvention::Windows) {
						int align = WordSize * 9;
						_unroll_base = -WordSize * 9;
						encode_push(Reg64::RBP);
						encode_mov_reg_reg(8, Reg64::RBP, Reg64::RSP);
						encode_push(Reg64::RBX);
						encode_push(Reg64::RDI);
						encode_push(Reg64::RSI);
						encode_push(Reg64::R10);
						encode_push(Reg64::R11);
						encode_push(Reg64::R12);
						encode_push(Reg64::R13);
						encode_push(Reg64::R14);
						encode_push(Reg64::R15);
						_retval.bp_offset = 0;
						for (int i = 0; i < api->Length(); i++) {
							auto & info = api->ElementAt(i);
							if (info.index >= 0) {
								_inputs[info.index].bound_to = info.reg;
								_inputs[info.index].bp_offset = WordSize * (2 + i);
								_inputs[info.index].indirect = info.indirect;
							} else {
								_retval.bound_to = info.reg;
								_retval.indirect = info.indirect;
								_retval.bp_offset = WordSize * (2 + i);
							}
							if (info.reg != Reg64::NO) {
								if (_is_xmm_register(info.reg)) {
									encode_mov_reg_xmm(8, Reg64::RAX, info.reg);
									encode_mov_mem_reg(8, Reg64::RBP, WordSize * (2 + i), Reg64::RAX);
								} else encode_mov_mem_reg(8, Reg64::RBP, WordSize * (2 + i), info.reg);
							}
						}
						if (!_retval.bp_offset) {
							_retval.bound_to = _src.retval.semantics == ArgumentSemantics::FloatingPoint ? Reg64::XMM0 : Reg64::RAX;
							_retval.indirect = false;
							_retval.bp_offset = -align - WordSize;
							align += WordSize;
							_unroll_base -= WordSize;
							encode_add(Reg64::RSP, -WordSize);
						}
						if (align & 0xF) {
							align += WordSize;
							encode_add(Reg64::RSP, -WordSize);
						}
						_scope_frame_base = -align;
					} else if (_conv == CallingConvention::Unix) {
						int align = WordSize * 7;
						_unroll_base = -WordSize * 7;
						encode_push(Reg64::RBP);
						encode_mov_reg_reg(8, Reg64::RBP, Reg64::RSP);
						encode_push(Reg64::RBX);
						encode_push(Reg64::R10);
						encode_push(Reg64::R11);
						encode_push(Reg64::R12);
						encode_push(Reg64::R13);
						encode_push(Reg64::R14);
						encode_push(Reg64::R15);
						if (!_is_pass_by_reference(_src.retval)) {
							_retval.bound_to = _src.retval.semantics == ArgumentSemantics::FloatingPoint ? Reg64::XMM0 : Reg64::RAX;
							_retval.indirect = false;
							_retval.bp_offset = -align - WordSize;
							align += WordSize;
							_unroll_base -= WordSize;
							encode_add(Reg64::RSP, -WordSize);
						}
						int stack_space_offset = 2 * WordSize;
						for (auto & info : *api) {
							ArgumentStorageSpec * spec;
							if (info.index >= 0) spec = &_inputs[info.index];
							else spec = &_retval;
							spec->bound_to = info.reg;
							spec->indirect = info.indirect;
							if (info.reg == Reg64::NO) {
								spec->bp_offset = stack_space_offset;
								stack_space_offset += WordSize;
							} else {
								if (_is_xmm_register(info.reg)) {
									encode_mov_reg_xmm(8, Reg64::RAX, info.reg);
									encode_push(Reg64::RAX);
								} else encode_push(info.reg);
								spec->bp_offset = -align - WordSize;
								align += WordSize;
							}	
						}
						if (align & 0xF) {
							align += WordSize;
							encode_add(Reg64::RSP, -WordSize);
						}
						_scope_frame_base = -align;
					}
				}
				virtual void encode_function_epilogue(void) override
				{
					if (_conv == CallingConvention::Windows) {
						if (_unroll_base != _scope_frame_base) encode_lea(Reg64::RSP, Reg64::RBP, _unroll_base);
						if (!_is_pass_by_reference(_src.retval)) {
							encode_pop(Reg64::RAX);
							if (_retval.bound_to == Reg64::XMM0) {
								auto quant = _src.retval.size.num_bytes + WordSize * _src.retval.size.num_words;
								encode_mov_xmm_reg(quant, Reg64::XMM0, Reg64::RAX);
							}
						} else encode_mov_reg_mem(8, Reg64::RAX, Reg64::RBP, _retval.bp_offset);
						encode_pop(Reg64::R15);
						encode_pop(Reg64::R14);
						encode_pop(Reg64::R13);
						encode_pop(Reg64::R12);
						encode_pop(Reg64::R11);
						encode_pop(Reg64::R10);
						encode_pop(Reg64::RSI);
						encode_pop(Reg64::RDI);
						encode_pop(Reg64::RBX);
						encode_pop(Reg64::RBP);
						encode_pure_ret();
					} else if (_conv == CallingConvention::Unix) {
						if (_unroll_base != _scope_frame_base) encode_lea(Reg64::RSP, Reg64::RBP, _unroll_base);
						if (!_is_pass_by_reference(_src.retval)) {
							encode_pop(Reg64::RAX);
							if (_retval.bound_to == Reg64::XMM0) {
								auto quant = _src.retval.size.num_bytes + WordSize * _src.retval.size.num_words;
								encode_mov_xmm_reg(quant, Reg64::XMM0, Reg64::RAX);
							}
						} else encode_mov_reg_mem(8, Reg64::RAX, Reg64::RBP, _retval.bp_offset);
						encode_pop(Reg64::R15);
						encode_pop(Reg64::R14);
						encode_pop(Reg64::R13);
						encode_pop(Reg64::R12);
						encode_pop(Reg64::R11);
						encode_pop(Reg64::R10);
						encode_pop(Reg64::RBX);
						encode_pop(Reg64::RBP);
						encode_pure_ret();
					}
				}
				virtual void encode_scope_unroll(int inst_current, int inst_jump_to) override
				{
					int current_level = 0;
					int ref_level = 0;
					auto current_scope = _scopes.GetLast();
					while (current_scope && current_scope->GetValue().temporary) {
						encode_finalize_scope(current_scope->GetValue());
						current_scope = current_scope->GetPrevious();
					}
					if (inst_jump_to > inst_current) {
						for (int i = inst_current + 1; i < inst_jump_to; i++) {
							if (_src.instset[i].opcode == OpcodeOpenScope) {
								current_level++;
							} else if (_src.instset[i].opcode == OpcodeCloseScope) {
								current_level--;
								if (current_level < ref_level) {
									ref_level--;
									if (!current_scope) throw InvalidStateException();
									encode_finalize_scope(current_scope->GetValue());
									current_scope = current_scope->GetPrevious();
								}
							}
						}
					} else if (inst_jump_to < inst_current) {
						for (int i = inst_current - 1; i >= inst_jump_to; i--) {
							if (_src.instset[i].opcode == OpcodeOpenScope) {
								current_level--;
								if (current_level < ref_level) {
									ref_level--;
									if (!current_scope) throw InvalidStateException();
									encode_finalize_scope(current_scope->GetValue());
									current_scope = current_scope->GetPrevious();
								}
							} else if (_src.instset[i].opcode == OpcodeCloseScope) {
								current_level++;
							}
						}
					}
				}
				virtual void process_encoding(void) override
				{
					for (int i = 0; i < _src.instset.Length(); i++) {
						_current_instruction = i;
						_org_inst_offsets[i] = _dest.code.Length();
						auto & inst = _src.instset[i];
						if (inst.opcode == OpcodeNOP) {
						} else if (inst.opcode == OpcodeTrap) {
							encode_debugger_trap();
						} else if (inst.opcode == OpcodeOpenScope) {
							int required_size = 0;
							int current_scope_depth = _scopes.Count() + 1;
							int future_scope_depth = current_scope_depth;
							for (int j = i + 1; j < _src.instset.Length(); j++) {
								if (_src.instset[j].opcode == OpcodeOpenScope) {
									future_scope_depth++;
								} else if (_src.instset[j].opcode == OpcodeCloseScope) {
									if (future_scope_depth == current_scope_depth) break;
									future_scope_depth--;
								} else if (_src.instset[j].opcode == OpcodeNewLocal) {
									if (future_scope_depth == current_scope_depth) required_size += _word_align(_src.instset[j].attachment);
								}
							}
							encode_open_scope(required_size, false, 0);
						} else if (inst.opcode == OpcodeCloseScope) {
							encode_close_scope();
						} else if (inst.opcode == OpcodeExpression) {
							_encode_expression_evaluation(inst.tree, Reg64::NO);
						} else if (inst.opcode == OpcodeNewLocal) {
							auto current_scope_ptr = _scopes.GetLast();
							if (!current_scope_ptr) throw InvalidArgumentException();
							auto & scope = current_scope_ptr->GetValue();
							LocalDisposition new_var;
							ZeroMemory(&new_var.finalizer.final, sizeof(new_var.finalizer.final));
							new_var.size = inst.attachment.num_bytes + WordSize * inst.attachment.num_words;
							auto size_padded = _word_align(inst.attachment);
							new_var.bp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
							scope.frame_size_unused -= size_padded;
							if (scope.frame_size_unused < 0) throw InvalidArgumentException();
							_init_locals.Push(new_var);
							_encode_expression_evaluation(inst.tree, Reg64::NO);
							_init_locals.Pop();
							new_var.finalizer = inst.attachment_final;
							scope.locals << new_var;
						} else if (inst.opcode == OpcodeUnconditionalJump) {
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							JumpRelocStruct rs;
							rs.machine_offset_at = _dest.code.Length() - 4;
							rs.machine_offset_relative_to = _dest.code.Length();
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
						} else if (inst.opcode == OpcodeConditionalJump) {
							_encode_expression_evaluation(inst.tree, Reg64::R15);
							encode_test(1, Reg64::R15, 0xFF);
							_dest.code << 0x0F; _dest.code << 0x84; // JZ
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							int addr = _dest.code.Length();
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
							JumpRelocStruct rs;
							rs.machine_offset_at = _dest.code.Length() - 4;
							rs.machine_offset_relative_to = _dest.code.Length();
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
						} else if (inst.opcode == OpcodeControlReturn) {
							_encode_expression_evaluation(inst.tree, Reg64::NO);
							for (auto & scp : _scopes.InversedElements()) encode_finalize_scope(scp);
							encode_function_epilogue();
						} else InvalidArgumentException();
					}
				}
				virtual void finalize_encoding(void) override
				{
					for (auto & rs : _jump_reloc) {
						int offset_to = _org_inst_offsets[rs.xasm_offset_jump_to];
						int offset_from = rs.machine_offset_relative_to;
						*reinterpret_cast<int *>(_dest.code.GetBuffer() + rs.machine_offset_at) = offset_to - offset_from;
					}
				}
			};

			class TranslatorX64 : public IAssemblyTranslator
			{
				CallingConvention _conv;
			public:
				TranslatorX64(CallingConvention conv) : _conv(conv) {}
				virtual ~TranslatorX64(void) override {}
				virtual bool Translate(TranslatedFunction & dest, const Function & src) noexcept override
				{
					try {
						dest.Clear();
						dest.data = src.data;
						X64::EncoderContext ctx(_conv, dest, src);
						ctx.encode_function_prologue();
						ctx.process_encoding();
						ctx.finalize_encoding();
						ctx.encode_debugger_trap();
						while (dest.code.Length() & 0xF) ctx.encode_debugger_trap();
						return true;
					} catch (...) { dest.Clear(); return false; }
				}
				virtual uint GetWordSize(void) noexcept override { return 8; }
				virtual Platform GetPlatform(void) noexcept override { return Platform::X64; }
				virtual CallingConvention GetCallingConvention(void) noexcept override { return _conv; }
				virtual string ToString(void) const override { return L"XA-x86-64"; }
			};
		}
		
		IAssemblyTranslator * CreateTranslatorX64(CallingConvention conv) { return new X64::TranslatorX64(conv); }
	}
}