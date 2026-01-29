#include "xa_t_x64.h"
#include "xa_t_x86.h"
#include "xa_type_helper.h"
#include "xa_sha2_commons.h"

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
				translator_sha2_state _sha_state;
			private:
				static bool _is_xmm_register(Reg reg) { return uint(reg) & 0x00FF0000; }
				static bool _is_pass_by_reference(const ArgumentSpecification & spec) { return _word_align(spec.size) > 8 || spec.semantics == ArgumentSemantics::Object; }
				static bool _needs_stack_storage(const ArgumentSpecification & spec) { if (_word_align(spec.size) > WordSize) return true; return false; }
				static uint32 _word_align(const ObjectSize & size) { uint full_size = size.num_bytes + WordSize * size.num_words; return (uint64(full_size) + 7) / 8 * 8; }
				Array<ArgumentPassageInfo> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt)
				{
					SafePointer< Array<ArgumentPassageInfo> > result = new Array<ArgumentPassageInfo>(0x40);
					if (_abi == ABI::WindowsX64) {
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
					for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics != ArgumentSemantics::This || _abi == ABI::UnixX64) {
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
							if (_abi == ABI::WindowsX64) {
								if (i == 0) info.reg = Reg64::XMM0;
								else if (i == 1) info.reg = Reg64::XMM1;
								else if (i == 2) info.reg = Reg64::XMM2;
								else if (i == 3) info.reg = Reg64::XMM3;
							} else if (_abi == ABI::UnixX64) {
								info.reg = unix_fr[cfr];
								if (info.reg != Reg64::NO) cfr++;
							}
						} else {
							if (_abi == ABI::WindowsX64) {
								if (i == 0) info.reg = Reg64::RCX;
								else if (i == 1) info.reg = Reg64::RDX;
								else if (i == 2) info.reg = Reg64::R8;
								else if (i == 3) info.reg = Reg64::R9;
							} else if (_abi == ABI::UnixX64) {
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
					if (_abi == ABI::WindowsX64) {
						encode_preserve(Reg64::RCX, reg_in_use, 0, true);
						encode_preserve(Reg64::RDX, reg_in_use, 0, true);
						encode_preserve(Reg64::R8, reg_in_use, 0, true);
						encode_preserve(Reg64::R9, reg_in_use, 0, true);
					} else if (_abi == ABI::UnixX64) {
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
						if (_abi == ABI::WindowsX64) {
							stack_growth = max(1 + l.finalizer.final_args.Length(), 4) * 8;
							if ((stack_growth + _stack_oddity) & 0xF) {
								encode_stack_alloc(8);
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
							encode_stack_alloc(32);
						} else if (_abi == ABI::UnixX64) {
							stack_growth = max(l.finalizer.final_args.Length() - 5, 0) * 8;
							if ((stack_growth + _stack_oddity) & 0xF) {
								encode_stack_alloc(8);
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
						if (stack_growth) encode_stack_dealloc(stack_growth);
					}
					if (_abi == ABI::WindowsX64) {
						encode_restore(Reg64::R9, reg_in_use, 0, true);
						encode_restore(Reg64::R8, reg_in_use, 0, true);
						encode_restore(Reg64::RDX, reg_in_use, 0, true);
						encode_restore(Reg64::RCX, reg_in_use, 0, true);
					} else if (_abi == ABI::UnixX64) {
						encode_restore(Reg64::R9, reg_in_use, 0, true);
						encode_restore(Reg64::R8, reg_in_use, 0, true);
						encode_restore(Reg64::RCX, reg_in_use, 0, true);
						encode_restore(Reg64::RDX, reg_in_use, 0, true);
						encode_restore(Reg64::RSI, reg_in_use, 0, true);
						encode_restore(Reg64::RDI, reg_in_use, 0, true);
					}
					encode_restore(Reg64::RAX, reg_in_use, 0, true);
					if (scope.shift_sp) encode_stack_dealloc(scope.frame_size);
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
					if (_abi == ABI::WindowsX64) preserve_regs << Reg64::RBX << Reg64::RCX << Reg64::RDX << Reg64::R8 << Reg64::R9;
					else if (_abi == ABI::UnixX64) preserve_regs << Reg64::RBX << Reg64::RDI << Reg64::RSI << Reg64::RDX << Reg64::RCX << Reg64::R8 << Reg64::R9;
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
					if (_abi == ABI::WindowsX64) stack_usage = max(layout->Length(), 4) * 8;
					else if (_abi == ABI::UnixX64) stack_usage = (num_args_by_stack + num_args_by_xmm) * 8;
					if ((_stack_oddity + stack_usage) & 0xF) stack_usage += 8;
					if (stack_usage && !idle) {
						_stack_oddity += stack_usage;
						encode_stack_alloc(stack_usage);
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
							if (_abi == ABI::WindowsX64) {
								argument_homes[info.index] = current_stack_index;
								current_stack_index += 8;
							} else if (_abi == ABI::UnixX64) {
								if (info.reg == Reg64::NO) {
									argument_homes[info.index] = current_stack_index;
									current_stack_index += 8;
								} else argument_homes[info.index] = -1;
							}
						} else {
							if (_abi == ABI::WindowsX64) current_stack_index += 8;
							*mem_load += _word_align(node.retval_spec.size);
							rv_reg = info.reg;
							if (!idle) rv_offset = allocate_temporary(node.retval_spec.size, &rv_mem_index);
						}
					}
					if (_abi == ABI::UnixX64) for (auto & info : layout->Elements()) if (info.index >= 0 && _is_xmm_register(info.reg)) {
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
						if (stack_usage) encode_stack_dealloc(stack_usage);
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
						Reg acc1, acc2 = Reg64::NO;
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
						Reg acc1, acc2 = Reg64::NO;
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
				void _encode_long_arithmetics(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformLongIntCmpEQ || node.self.index == TransformLongIntCmpNEQ || node.self.index == TransformLongIntCmpL ||
						node.self.index == TransformLongIntCmpLE || node.self.index == TransformLongIntCmpG || node.self.index == TransformLongIntCmpGE) {
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						auto in1_size = object_size(node.input_specs[0].size);
						auto in2_size = object_size(node.input_specs[1].size);
						auto out_size = object_size(node.retval_spec.size);
						if (in1_size != in2_size || in1_size == 0) throw InvalidArgumentException();
						if (out_size != 1 && out_size != 2 && out_size != 4 && out_size != 8) throw InvalidArgumentException();
						X86::Reg staging = disp->reg;
						if (staging == Reg64::NO) staging = Reg64::RAX;
						InternalDisposition c1, c2;
						c1.reg = staging != Reg64::RDI ? Reg64::RDI : Reg64::RBX;
						c2.reg = staging != Reg64::RSI ? Reg64::RSI : Reg64::RBX;
						c1.flags = c2.flags = DispositionPointer;
						c1.size = c2.size = in1_size;
						encode_preserve(c1.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &c1, reg_in_use | uint(c1.reg));
						encode_preserve(c2.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &c2, reg_in_use | uint(c1.reg) | uint(c2.reg));
						encode_preserve(staging, reg_in_use, uint(disp->reg), !idle);
						if (!idle) {
							bool swap_arguments = false;
							uint result_in_case_of_skip = 0xFF, result_in_case_of_pass = 0xFF;
							uint final_jcc_opcode = 0, intermediate_jcc_opcode = 0;
							arOp first_op, further_op;
							Array<int> skip_addresses(0x100);
							int pass_address;
							if (node.self.index == TransformLongIntCmpEQ) {
								result_in_case_of_skip = 0;
								result_in_case_of_pass = 1;
								final_jcc_opcode = intermediate_jcc_opcode = 0x85; // JNE
								first_op = further_op = arOp::CMP;
							} else if (node.self.index == TransformLongIntCmpNEQ) {
								result_in_case_of_skip = 1;
								result_in_case_of_pass = 0;
								final_jcc_opcode = intermediate_jcc_opcode = 0x85; // JNE
								first_op = further_op = arOp::CMP;
							} else if (node.self.index == TransformLongIntCmpLE) {
								swap_arguments = true;
								result_in_case_of_skip = 0;
								result_in_case_of_pass = 1;
								final_jcc_opcode = 0x82; // JC
								intermediate_jcc_opcode = 0; // don't interrupt
								first_op = arOp::SUB;
								further_op = arOp::SBB;
							} else if (node.self.index == TransformLongIntCmpL) {
								result_in_case_of_skip = 1;
								result_in_case_of_pass = 0;
								final_jcc_opcode = 0x82; // JC
								intermediate_jcc_opcode = 0; // don't interrupt
								first_op = arOp::SUB;
								further_op = arOp::SBB;
							} else if (node.self.index == TransformLongIntCmpGE) {
								result_in_case_of_skip = 0;
								result_in_case_of_pass = 1;
								final_jcc_opcode = 0x82; // JC
								intermediate_jcc_opcode = 0; // don't interrupt
								first_op = arOp::SUB;
								further_op = arOp::SBB;
							} else if (node.self.index == TransformLongIntCmpG) {
								swap_arguments = true;
								result_in_case_of_skip = 1;
								result_in_case_of_pass = 0;
								final_jcc_opcode = 0x82; // JC
								intermediate_jcc_opcode = 0; // don't interrupt
								first_op = arOp::SUB;
								further_op = arOp::SBB;
							}
							uint remaining = in1_size;
							while (remaining) {
								uint offset = in1_size - remaining;
								uint quant;
								if (remaining >= 8) quant = 8;
								else if (remaining >= 4) quant = 4;
								else if (remaining >= 2) quant = 2;
								else quant = 1;
								remaining -= quant;
								encode_mov_reg_mem(quant, staging, swap_arguments ? c2.reg : c1.reg, offset);
								encode_operation(quant, offset ? further_op : first_op, staging, swap_arguments ? c1.reg : c2.reg, true, offset);
								if (remaining) {
									if (intermediate_jcc_opcode) {
										_dest.code << 0x0F << uint8(intermediate_jcc_opcode) << 0x00 << 0x00 << 0x00 << 0x00;
										skip_addresses << _dest.code.Length();
									}
								} else {
									if (final_jcc_opcode) {
										_dest.code << 0x0F << uint8(final_jcc_opcode) << 0x00 << 0x00 << 0x00 << 0x00;
										skip_addresses << _dest.code.Length();
									}
								}
							}
							encode_mov_reg_const(WordSize, staging, result_in_case_of_pass);
							_dest.code << 0xEB << 0x00; // JMP
							pass_address = _dest.code.Length();
							for (auto & addr : skip_addresses) {
								*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
							}
							encode_mov_reg_const(WordSize, staging, result_in_case_of_skip);
							_dest.code[pass_address - 1] = _dest.code.Length() - pass_address;
						}
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							(*mem_load) += _word_align(TH::MakeSize(out_size, 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(out_size, 0));
								encode_mov_mem_reg(out_size, Reg64::RBP, offs, disp->reg);
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
							disp->flags = DispositionPointer;
						}
						encode_restore(staging, reg_in_use, uint(disp->reg), !idle);
						encode_restore(c2.reg, reg_in_use, 0, !idle);
						encode_restore(c1.reg, reg_in_use, 0, !idle);
					} else if (node.self.index == TransformLongIntAdd || node.self.index == TransformLongIntSubt) {
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						auto in1_size = object_size(node.input_specs[0].size);
						auto in2_size = object_size(node.input_specs[1].size);
						auto out_size = object_size(node.retval_spec.size);
						if (in1_size != in2_size || in1_size == 0) throw InvalidArgumentException();
						if (out_size != 0 && out_size != 1 && out_size != 2 && out_size != 4 && out_size != 8) throw InvalidArgumentException();
						X86::Reg staging = disp->reg;
						if (staging == Reg64::NO) staging = Reg64::RAX;
						InternalDisposition c1, c2;
						c1.reg = staging != Reg64::RDI ? Reg64::RDI : Reg64::RBX;
						c2.reg = staging != Reg64::RSI ? Reg64::RSI : Reg64::RBX;
						c1.flags = c2.flags = DispositionPointer;
						c1.size = c2.size = in1_size;
						encode_preserve(c1.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &c1, reg_in_use | uint(c1.reg));
						encode_preserve(c2.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &c2, reg_in_use | uint(c1.reg) | uint(c2.reg));
						encode_preserve(staging, reg_in_use, uint(disp->reg), !idle);
						if (!idle) {
							uint remaining = in1_size;
							while (remaining) {
								uint offset = in1_size - remaining;
								uint quant;
								if (remaining >= 8) quant = 8;
								else if (remaining >= 4) quant = 4;
								else if (remaining >= 2) quant = 2;
								else quant = 1;
								remaining -= quant;
								encode_mov_reg_mem(quant, staging, c1.reg, offset);
								if (node.self.index == TransformLongIntAdd) encode_operation(quant, offset ? arOp::ADC : arOp::ADD, staging, c2.reg, true, offset);
								else if (node.self.index == TransformLongIntSubt) encode_operation(quant, offset ? arOp::SBB : arOp::SUB, staging, c2.reg, true, offset);
								encode_mov_mem_reg(quant, c1.reg, offset, staging);
							}
							if (out_size && (disp->flags & (DispositionRegister | DispositionPointer))) {
								_dest.code << 0x72 << 0x00; // JC
								int jc = _dest.code.Length();
								encode_mov_reg_const(WordSize, staging, 0);
								_dest.code << 0xEB << 0x00; // JMP
								int jmp = _dest.code.Length();
								_dest.code[jc - 1] = _dest.code.Length() - jc;
								encode_mov_reg_const(WordSize, staging, 1);
								_dest.code[jmp - 1] = _dest.code.Length() - jmp;
							}
						}
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							(*mem_load) += _word_align(TH::MakeSize(max(out_size, 1U), 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(max(out_size, 1U), 0));
								if (out_size) encode_mov_mem_reg(out_size, Reg64::RBP, offs, disp->reg);
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						encode_restore(staging, reg_in_use, uint(disp->reg), !idle);
						encode_restore(c2.reg, reg_in_use, 0, !idle);
						encode_restore(c1.reg, reg_in_use, 0, !idle);
					} else if (node.self.index == TransformLongIntShiftL || node.self.index == TransformLongIntShiftR) {
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						if (node.inputs[1].self.ref_class != ReferenceLiteral) throw InvalidArgumentException();
						auto in_size = object_size(node.input_specs[0].size);
						auto shift = object_size(node.input_specs[1].size);
						auto out_size = object_size(node.retval_spec.size);
						if (out_size || in_size == 0) throw InvalidArgumentException();
						uint quant;
						if ((in_size & 7) == 0) quant = 8;
						else if ((in_size & 3) == 0) quant = 4;
						else if ((in_size & 1) == 0) quant = 2;
						else quant = 1;
						uint num_quants = in_size / quant;
						InternalDisposition ld;
						ld.reg = Reg64::RDI;
						ld.flags = DispositionPointer;
						ld.size = in_size;
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
						if (shift && !idle) {
							uint shift_quants = shift / (quant * 8);
							uint shift_bits = shift % (quant * 8);
							if (shift_bits == 0) {
								encode_preserve(Reg64::RAX, reg_in_use, 0, true);
								uint quant_rem = shift_quants > num_quants ? 0 : num_quants - shift_quants;
								uint quant_del = num_quants - quant_rem;
								if (node.self.index == TransformLongIntShiftL) {
									for (int i = num_quants - 1; i >= quant_del; i--) {
										encode_mov_reg_mem(quant, Reg64::RAX, ld.reg, quant * (i - quant_del));
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
									}
									encode_operation(quant, arOp::XOR, Reg64::RAX, Reg64::RAX);
									for (int i = quant_del - 1; i >= 0; i--) {
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
									}
								} else if (node.self.index == TransformLongIntShiftR) {
									for (int i = 0; i < quant_rem; i++) {
										encode_mov_reg_mem(quant, Reg64::RAX, ld.reg, quant * (i + quant_del));
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
									}
									encode_operation(quant, arOp::XOR, Reg64::RAX, Reg64::RAX);
									for (int i = quant_rem; i < num_quants; i++) {
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
									}
								}
								encode_restore(Reg64::RAX, reg_in_use, 0, true);
							} else if (shift == 1) {
								if (node.self.index == TransformLongIntShiftL) {
									for (int i = 0; i < num_quants; i++) {
										encode_shift(quant, i ? shOp::RCL : shOp::SHL, ld.reg, 1, true, quant * i);
									}
								} else if (node.self.index == TransformLongIntShiftR) {
									for (int i = num_quants - 1; i >= 0; i--) {
										encode_shift(quant, i == num_quants - 1 ? shOp::SHR : shOp::RCR, ld.reg, 1, true, quant * i);
									}
								}
							} else {
								encode_preserve(Reg64::RAX, reg_in_use, 0, true);
								encode_preserve(Reg64::RCX, reg_in_use, 0, true);
								int quant_rem = shift_quants > num_quants ? 0 : num_quants - shift_quants;
								int quant_del = num_quants - quant_rem;
								if (node.self.index == TransformLongIntShiftL) {
									for (int i = num_quants - 1; i >= quant_del; i--) {
										if (i > quant_del) {
											encode_mov_reg_mem(quant, Reg64::RAX, ld.reg, quant * (i - quant_del));
											encode_mov_reg_mem(quant, Reg64::RCX, ld.reg, quant * (i - quant_del - 1));
											encode_shift(quant, shOp::SHL, Reg64::RAX, shift_bits);
											encode_shift(quant, shOp::SHR, Reg64::RCX, 8 * quant - shift_bits);
											encode_operation(quant, arOp::OR, Reg64::RAX, Reg64::RCX);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
										} else {
											encode_mov_reg_mem(quant, Reg64::RAX, ld.reg, quant * (i - quant_del));
											encode_shift(quant, shOp::SHL, Reg64::RAX, shift_bits);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
										}
									}
									if (quant_del) {
										encode_operation(quant, arOp::XOR, Reg64::RAX, Reg64::RAX);
										for (int i = quant_del - 1; i >= 0; i--) {
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
										}
									}
								} else if (node.self.index == TransformLongIntShiftR) {
									for (int i = 0; i < quant_rem; i++) {
										if (i + 1 < quant_rem) {
											encode_mov_reg_mem(quant, Reg64::RAX, ld.reg, quant * (i + quant_del));
											encode_mov_reg_mem(quant, Reg64::RCX, ld.reg, quant * (i + quant_del + 1));
											encode_shift(quant, shOp::SHR, Reg64::RAX, shift_bits);
											encode_shift(quant, shOp::SHL, Reg64::RCX, 8 * quant - shift_bits);
											encode_operation(quant, arOp::OR, Reg64::RAX, Reg64::RCX);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
										} else {
											encode_mov_reg_mem(quant, Reg64::RAX, ld.reg, quant * (i + quant_del));
											encode_shift(quant, shOp::SHR, Reg64::RAX, shift_bits);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
										}
									}
									if (quant_del) {
										encode_operation(quant, arOp::XOR, Reg64::RAX, Reg64::RAX);
										for (int i = quant_rem; i < num_quants; i++) {
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg64::RAX);
										}
									}
								}
								encode_restore(Reg64::RCX, reg_in_use, 0, true);
								encode_restore(Reg64::RAX, reg_in_use, 0, true);
							}
						}
						encode_restore(ld.reg, reg_in_use, 0, !idle);
						if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(1, 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(1, 0));
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
						}
					} else if (node.self.index == TransformLongIntMul || node.self.index == TransformLongIntDivMod || node.self.index == TransformLongIntMod) {
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						auto io_size = object_size(node.input_specs[0].size);
						auto quant = object_size(node.input_specs[1].size);
						auto out_size = object_size(node.retval_spec.size);
						if (io_size == 0 || io_size % quant) throw InvalidArgumentException();
						if (quant != 1 && quant != 2 && quant != 4 && quant != 8) throw InvalidArgumentException();
						if (out_size != 0 && out_size != quant) throw InvalidArgumentException();
						auto num_words = io_size / quant;
						InternalDisposition ld, m;
						ld.reg = Reg64::RDI;
						ld.size = io_size;
						ld.flags = DispositionPointer;
						m.reg = Reg64::RCX;
						m.size = quant;
						m.flags = DispositionRegister;
						Reg staging_low = Reg64::RAX;
						Reg staging_high = Reg64::RDX;
						Reg carry = Reg64::RBX;
						Reg zero = Reg64::RSI;
						Reg result = Reg64::NO;
						auto alt_reg_in_use = reg_in_use & ~disp->reg;
						encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, alt_reg_in_use | uint(ld.reg));
						encode_preserve(m.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &m, alt_reg_in_use | uint(ld.reg) | uint(m.reg));
						encode_preserve(staging_low, reg_in_use, disp->reg, !idle);
						encode_preserve(staging_high, reg_in_use, disp->reg, !idle);
						if (!idle) {
							if (node.self.index == TransformLongIntMul) {
								encode_preserve(carry, reg_in_use, disp->reg, true);
								encode_preserve(zero, reg_in_use, disp->reg, quant != 1);
								encode_operation(WordSize, arOp::XOR, carry, carry);
								if (quant != 1) encode_operation(WordSize, arOp::XOR, zero, zero);
								for (int i = 0; i < num_words; i++) {
									encode_mov_reg_mem(quant, staging_low, ld.reg, quant * i);
									encode_mul_div(quant, mdOp::MUL, m.reg);
									if (quant == 1) {
										encode_operation(2, arOp::ADD, staging_low, carry);
										encode_mov_mem_reg(1, ld.reg, i, staging_low);
										encode_shr(staging_low, 8);
										encode_mov_reg_reg(1, carry, staging_low);
									} else {
										encode_operation(quant, arOp::ADD, staging_low, carry);
										encode_operation(quant, arOp::ADC, staging_high, zero);
										encode_mov_reg_reg(quant, carry, staging_high);
										encode_mov_mem_reg(quant, ld.reg, quant * i, staging_low);
									}
								}
								result = carry;
								encode_restore(zero, reg_in_use, disp->reg, quant != 1);
								encode_restore(carry, reg_in_use, disp->reg, true);
							} else {
								bool write_back = node.self.index == TransformLongIntDivMod;
								if (quant == 1) encode_operation(WordSize, arOp::XOR, staging_low, staging_low);
								else encode_operation(WordSize, arOp::XOR, staging_high, staging_high);
								for (int i = num_words - 1; i >= 0; i--) {
									encode_mov_reg_mem(quant, staging_low, ld.reg, quant * i);
									encode_mul_div(quant, mdOp::DIV, m.reg);
									if (write_back) encode_mov_mem_reg(quant, ld.reg, quant * i, staging_low);
								}
								if (quant == 1) {
									encode_shr(staging_low, 8);
									result = staging_low;
								} else result = staging_high;
							}
						}
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
							if (result != disp->reg && !idle) encode_mov_reg_reg(quant, disp->reg, result);
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(max(out_size, 1U), 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(max(out_size, 1U), 0));
								if (out_size) encode_mov_mem_reg(out_size, Reg64::RBP, offs, result);
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						encode_restore(staging_high, reg_in_use, disp->reg, !idle);
						encode_restore(staging_low, reg_in_use, disp->reg, !idle);
						encode_restore(m.reg, reg_in_use, disp->reg, !idle);
						encode_restore(ld.reg, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformLongIntZero || node.self.index == TransformLongIntCopy) {
						if (node.inputs.Length() < 1 || node.inputs.Length() > 5 || node.input_specs.Length() != node.inputs.Length()) throw InvalidArgumentException();
						uint dest_size, src_size, out_size, dest_offset, src_offset, blt_length;
						bool zero_blt = node.self.index == TransformLongIntZero;
						dest_size = object_size(node.input_specs[0].size);
						out_size = object_size(node.retval_spec.size);
						if (out_size != 0 || dest_size == 0) throw InvalidArgumentException();
						if (node.inputs.Length() >= 2) src_size = object_size(node.input_specs[1].size); else src_size = 0;
						if (node.inputs.Length() >= 3) {
							if (node.inputs[2].self.ref_class != ReferenceLiteral) throw InvalidArgumentException();
							dest_offset = object_size(node.input_specs[2].size);
							if (dest_offset > dest_size) dest_offset = dest_size;
						} else dest_offset = 0;
						if (node.inputs.Length() >= 4) {
							if (node.inputs[3].self.ref_class != ReferenceLiteral) throw InvalidArgumentException();
							src_offset = object_size(node.input_specs[3].size);
							if (src_offset > src_size) src_offset = src_size;
						} else src_offset = 0;
						if (node.inputs.Length() >= 5) {
							if (node.inputs[4].self.ref_class != ReferenceLiteral) throw InvalidArgumentException();
							blt_length = object_size(node.input_specs[4].size);
						} else blt_length = src_size;
						if (blt_length > src_size - src_offset) blt_length = src_size - src_offset;
						if (blt_length > dest_size - dest_offset) blt_length = dest_size - dest_offset;
						X86::Reg accumulator = Reg64::RAX;
						X86::Reg accumulator16 = allocate_xmm(0, 0);
						InternalDisposition dld, sld;
						dld.reg = Reg64::RDI;
						sld.reg = Reg64::RSI;
						dld.flags = sld.flags = DispositionPointer;
						dld.size = dest_size;
						sld.size = src_size;
						encode_preserve(dld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &dld, reg_in_use | uint(dld.reg));
						if (src_size && blt_length) {
							encode_preserve(sld.reg, reg_in_use, 0, !idle);
							_encode_tree_node(node.inputs[1], idle, mem_load, &sld, reg_in_use | uint(dld.reg) | uint(sld.reg));
						}
						encode_preserve(accumulator, reg_in_use, 0, !idle);
						if (!idle) {
							uint pos_1 = dest_offset;
							uint pos_2 = dest_offset + blt_length;
							bool reg_zeroed = false;
							bool xmm_zeroed = false;
							if (pos_1 && zero_blt) {
								uint pos = 0;
								while (pos < pos_1) {
									uint rem = pos_1 - pos;
									uint quant;
									if ((rem & 0xF) == 0) quant = 16;
									else if ((rem & 7) == 0) quant = 8;
									else if ((rem & 3) == 0) quant = 4;
									else if ((rem & 1) == 0) quant = 2;
									else quant = 1;
									if (quant == 16) {
										if (!xmm_zeroed) {
											encode_simd_xor(accumulator16, accumulator16);
											xmm_zeroed = true;
										}
										encode_mov_mem_xmm(quant, dld.reg, pos, accumulator16);
									} else {
										if (!reg_zeroed) {
											encode_operation(WordSize, arOp::XOR, accumulator, accumulator);
											reg_zeroed = true;
										}
										encode_mov_mem_reg(quant, dld.reg, pos, accumulator);
									}
									pos += quant;
								}
							}
							if (pos_2 > pos_1) {
								uint pos = pos_1;
								while (pos < pos_2) {
									uint rem = pos_2 - pos;
									uint pos_s = pos - dest_offset + src_offset;
									uint quant;
									if ((rem & 0xF) == 0 && (pos & 0xF) == 0) quant = 16;
									else if ((rem & 7) == 0 && (pos & 7) == 0) quant = 8;
									else if ((rem & 3) == 0 && (pos & 3) == 0) quant = 4;
									else if ((rem & 1) == 0 && (pos & 1) == 0) quant = 2;
									else quant = 1;
									if (quant == 16) {
										encode_mov_xmm_mem(quant, accumulator16, sld.reg, pos_s);
										encode_mov_mem_xmm(quant, dld.reg, pos, accumulator16);
										xmm_zeroed = false;
									} else {
										encode_mov_reg_mem(quant, accumulator, sld.reg, pos_s);
										encode_mov_mem_reg(quant, dld.reg, pos, accumulator);
										reg_zeroed = false;
									}
									pos += quant;
								}
							}
							if (dest_size > pos_2 && zero_blt) {
								uint pos = pos_2;
								while (pos < dest_size) {
									uint rem = dest_size - pos;
									uint quant;
									if ((rem & 0xF) == 0 && (pos & 0xF) == 0) quant = 16;
									else if ((rem & 7) == 0 && (pos & 7) == 0) quant = 8;
									else if ((rem & 3) == 0 && (pos & 3) == 0) quant = 4;
									else if ((rem & 1) == 0 && (pos & 1) == 0) quant = 2;
									else quant = 1;
									if (quant == 16) {
										if (!xmm_zeroed) {
											encode_simd_xor(accumulator16, accumulator16);
											xmm_zeroed = true;
										}
										encode_mov_mem_xmm(quant, dld.reg, pos, accumulator16);
									} else {
										if (!reg_zeroed) {
											encode_operation(WordSize, arOp::XOR, accumulator, accumulator);
											reg_zeroed = true;
										}
										encode_mov_mem_reg(quant, dld.reg, pos, accumulator);
									}
									pos += quant;
								}
							}
						}
						encode_restore(accumulator, reg_in_use, 0, !idle);
						if (src_size && blt_length) encode_restore(sld.reg, reg_in_use, 0, !idle);
						encode_restore(dld.reg, reg_in_use, 0, !idle);
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(1, 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(1, 0));
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
					} else if (node.self.index == TransformLongIntGetBit) {
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						auto io_size = object_size(node.input_specs[0].size);
						auto selector_size = object_size(node.input_specs[1].size);
						auto out_size = object_size(node.retval_spec.size);
						if (io_size == 0) throw InvalidArgumentException();
						if (selector_size != 1 && selector_size != 2 && selector_size != 4 && selector_size != 8) throw InvalidArgumentException();
						if (out_size != 1 && out_size != 2 && out_size != 4 && out_size != 8) throw InvalidArgumentException();
						Reg result = Reg64::RAX;
						InternalDisposition ld, sel;
						ld.reg = Reg64::RDI;
						sel.reg = Reg64::RCX;
						ld.flags = DispositionPointer;
						sel.flags = DispositionRegister;
						ld.size = io_size;
						sel.size = selector_size;
						encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | ld.reg);
						encode_preserve(sel.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &sel, reg_in_use | ld.reg | sel.reg);
						encode_preserve(result, reg_in_use, disp->reg, !idle);
						if (!idle) {
							if (selector_size == 8) encode_mov_reg_reg(8, result, sel.reg);
							else encode_mov_zx(8, result, selector_size, sel.reg);
							encode_shr(result, 3);
							encode_operation(8, arOp::ADD, ld.reg, result);
							encode_mov_reg_mem(1, result, ld.reg);
							encode_and(sel.reg, 0x7);
							encode_shift(1, shOp::SHR, result);
							encode_and(result, 1);
						}
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
							if (!idle && result != disp->reg) encode_mov_reg_reg(8, disp->reg, result);
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(0, 1));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(0, 1));
								encode_mov_mem_reg(out_size, Reg64::RBP, offs, result);
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						encode_restore(result, reg_in_use, disp->reg, !idle);
						encode_restore(sel.reg, reg_in_use, disp->reg, !idle);
						encode_restore(ld.reg, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformLongIntSetBit) {
						if (node.inputs.Length() != 3 || node.input_specs.Length() != 3) throw InvalidArgumentException();
						auto io_size = object_size(node.input_specs[0].size);
						auto selector_size = object_size(node.input_specs[1].size);
						auto bit_size = object_size(node.input_specs[2].size);
						auto out_size = object_size(node.retval_spec.size);
						if (io_size == 0 || out_size) throw InvalidArgumentException();
						if (selector_size != 1 && selector_size != 2 && selector_size != 4 && selector_size != 8) throw InvalidArgumentException();
						if (bit_size != 1 && bit_size != 2 && bit_size != 4 && bit_size != 8) throw InvalidArgumentException();
						Reg acc = Reg64::RAX;
						InternalDisposition ld, sel, bit;
						ld.reg = Reg64::RDI;
						sel.reg = Reg64::RCX;
						bit.reg = Reg64::RDX;
						ld.flags = DispositionPointer;
						sel.flags = bit.flags = DispositionRegister;
						ld.size = io_size;
						sel.size = selector_size;
						bit.size = bit_size;
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | ld.reg);
						encode_preserve(sel.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &sel, reg_in_use | ld.reg | sel.reg);
						encode_preserve(bit.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[2], idle, mem_load, &bit, reg_in_use | ld.reg | sel.reg | bit.reg);
						encode_preserve(acc, reg_in_use, 0, !idle);
						if (!idle) {
							if (selector_size == 8) encode_mov_reg_reg(8, acc, sel.reg);
							else encode_mov_zx(8, acc, selector_size, sel.reg);
							encode_shr(acc, 3);
							encode_operation(8, arOp::ADD, ld.reg, acc);
							encode_mov_reg_mem(1, acc, ld.reg);
							encode_and(sel.reg, 0x7);
							encode_shift(1, shOp::ROR, acc);
							encode_and(bit.reg, 0x01);
							encode_and(acc, 0xFE);
							encode_operation(1, arOp::OR, acc, bit.reg);
							encode_shift(1, shOp::ROL, acc);
							encode_mov_mem_reg(1, ld.reg, acc);
						}
						encode_restore(acc, reg_in_use, 0, !idle);
						encode_restore(bit.reg, reg_in_use, 0, !idle);
						encode_restore(sel.reg, reg_in_use, 0, !idle);
						encode_restore(ld.reg, reg_in_use, 0, !idle);
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(0, 1));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(0, 1));
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
					} else throw InvalidArgumentException();
				}
				void _encode_void_result(bool idle, int * mem_load, InternalDisposition * disp)
				{
					if (disp->flags & DispositionRegister) {
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionPointer) {
						disp->flags = DispositionPointer;
						(*mem_load) += _word_align(TH::MakeSize(0, 1));
						if (!idle) {
							int offs = allocate_temporary(TH::MakeSize(0, 1));
							encode_lea(disp->reg, Reg64::RBP, offs);
						}
					} else if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					}
				}
				void _encode_cryptography(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformCryptFeature) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
						if (node.inputs[0].self.ref_flags & ReferenceFlagInvoke) throw InvalidArgumentException();
						if (node.inputs[0].self.ref_class != ReferenceTransform) throw InvalidArgumentException();
						auto opcode = node.inputs[0].self.index;
						auto flags = node.inputs[0].self.ref_flags;
						auto retval_size = object_size(node.retval_spec.size);
						if (retval_size != 1 && retval_size != 2 && retval_size != 4 && retval_size != 8) throw InvalidArgumentException();
						// ARGUMENT EVALUATION
						Reg result = Reg64::RAX;
						encode_preserve(Reg64::RAX, reg_in_use, disp->reg, !idle);
						encode_preserve(Reg64::RCX, reg_in_use, disp->reg, !idle);
						encode_preserve(Reg64::RDX, reg_in_use, disp->reg, !idle);
						encode_preserve(Reg64::RBX, reg_in_use, disp->reg, !idle);
						// OPERATION
						if (!idle) {
							Reg test = Reg64::NO;
							uint rax_code = 0, rcx_code = 0xFFFFFFFF;
							uint bit = 0;
							if (opcode == TransformCryptFeature) {
								encode_mov_reg_const(8, result, 1);
							} else if (opcode == TransformCryptRandom) {
								rax_code = 0x01; // RDRAND
								bit = 30;
								test = Reg64::RCX;
							} else if (opcode == TransformCryptAesEncECB || opcode == TransformCryptAesDecECB || opcode == TransformCryptAesEncCBC || opcode == TransformCryptAesDecCBC) {
								rax_code = 0x01; // AES-NI
								bit = 25;
								test = Reg64::RCX;
							} else if (opcode == TransformCryptSha224I || opcode == TransformCryptSha256I || opcode == TransformCryptSha384I || opcode == TransformCryptSha512I) {
								rax_code = 0x01; // SSE3
								bit = 0;
								test = Reg64::RCX;
							} else if (opcode == TransformCryptSha224F || opcode == TransformCryptSha256F || opcode == TransformCryptSha384F || opcode == TransformCryptSha512F) {
								rax_code = 0x01; // SSE3
								bit = 0;
								test = Reg64::RCX;
							} else if (opcode == TransformCryptSha224S || opcode == TransformCryptSha256S) {
								if (flags & ReferenceFlagLong) {
									rax_code = 0x01; // SSE3
									bit = 0;
									test = Reg64::RCX;
								} else {
									rax_code = 0x07; // SHA-NI
									rcx_code = 0x00;
									bit = 29;
									test = Reg64::RBX;
								}
							} else if (opcode == TransformCryptSha384S || opcode == TransformCryptSha512S) {
								if (flags & ReferenceFlagLong) {
									rax_code = 0x01; // SSE3
									bit = 0;
									test = Reg64::RCX;
								} else {
									encode_mov_reg_const(8, result, 0); // NOT IMPLEMENTED
									// rax_code = 0x07; // SHA512
									// rcx_code = 0x01;
									// bit = 0;
									// test = Reg64::RAX;
								}
							} else {
								encode_mov_reg_const(8, result, 0);
							}
							if (test) {
								encode_mov_reg_const(8, Reg64::RAX, rax_code);
								if (rcx_code != 0xFFFFFFFF) encode_mov_reg_const(8, Reg64::RCX, rcx_code);
								encode_cpuid();
								encode_test(4, test, 1U << bit);
								_dest.code << 0x74 << 0x00; // JZ
								int jz = _dest.code.Length();
								encode_mov_reg_const(8, result, 1);
								_dest.code << 0xEB << 0x00; // JMP
								int jmp = _dest.code.Length();
								_dest.code[jz - 1] = _dest.code.Length() - jz;
								encode_mov_reg_const(8, result, 0);
								_dest.code[jmp - 1] = _dest.code.Length() - jmp;
							}
						}
						// FINALIZATION
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
							if (!idle && result != disp->reg) encode_mov_reg_reg(8, disp->reg, result);
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(0, 1));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(0, 1));
								encode_mov_mem_reg(retval_size, Reg64::RBP, offs, result);
								encode_lea(disp->reg, Reg64::RBP, offs);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						encode_restore(Reg64::RBX, reg_in_use, disp->reg, !idle);
						encode_restore(Reg64::RDX, reg_in_use, disp->reg, !idle);
						encode_restore(Reg64::RCX, reg_in_use, disp->reg, !idle);
						encode_restore(Reg64::RAX, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformCryptRandom) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						if (object_size(node.input_specs[0].size) != WordSize || object_size(node.retval_spec.size)) throw InvalidArgumentException();
						auto length_size = object_size(node.input_specs[1].size);
						if (length_size != 1 && length_size != 2 && length_size != 4 && length_size != 8) throw InvalidArgumentException();
						// ARGUMENT EVALUATION
						Reg acc = Reg64::RAX;
						InternalDisposition ld, len;
						ld.reg = Reg64::RDI;
						len.reg = Reg64::RCX;
						ld.flags = len.flags = DispositionRegister;
						ld.size = WordSize;
						len.size = length_size;
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | ld.reg);
						encode_preserve(len.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &len, reg_in_use | ld.reg | len.reg);
						encode_preserve(acc, reg_in_use, 0, !idle);
						// OPERATION
						if (!idle) {
							if (length_size != 8) encode_mov_zx(8, len.reg, length_size, len.reg);
							int test = _dest.code.Length();
							encode_test(8, len.reg, int(0xFFFFFFF8));
							_dest.code << 0x74 << 0x00; // JZ
							int jz = _dest.code.Length();
							encode_read_random(8, acc);
							_dest.code << 0x73 << 0x00; // JNC
							_dest.code[_dest.code.Length() - 1] = jz - _dest.code.Length();
							encode_mov_mem_reg(8, ld.reg, acc);
							encode_add(ld.reg, 8);
							encode_add(len.reg, -8);
							_dest.code << 0xEB << 0x00; // JMP
							_dest.code[_dest.code.Length() - 1] = test - _dest.code.Length();
							_dest.code[jz - 1] = _dest.code.Length() - jz;
						}
						// FINALIZATION
						encode_restore(acc, reg_in_use, 0, !idle);
						encode_restore(len.reg, reg_in_use, 0, !idle);
						encode_restore(ld.reg, reg_in_use, 0, !idle);
						_encode_void_result(idle, mem_load, disp);
					} else if (node.self.index == TransformCryptAesEncECB || node.self.index == TransformCryptAesDecECB || node.self.index == TransformCryptAesEncCBC || node.self.index == TransformCryptAesDecCBC) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 4 || node.input_specs.Length() != 4) throw InvalidArgumentException();
						if (object_size(node.input_specs[0].size) != 16 || object_size(node.input_specs[1].size) != WordSize || object_size(node.retval_spec.size)) throw InvalidArgumentException();
						auto length_size = object_size(node.input_specs[2].size);
						auto key_size = object_size(node.input_specs[3].size);
						if (length_size != 1 && length_size != 2 && length_size != 4 && length_size != 8) throw InvalidArgumentException();
						if (key_size != 16 && key_size != 24 && key_size != 32) throw InvalidArgumentException();
						uint rounds;
						if (key_size == 16) rounds = 10;
						else if (key_size == 24) rounds = 12;
						else if (key_size == 32) rounds = 14;
						bool decrypt = node.self.index == TransformCryptAesDecECB || node.self.index == TransformCryptAesDecCBC;
						bool chain_cbc = node.self.index == TransformCryptAesEncCBC || node.self.index == TransformCryptAesDecCBC;
						// ARGUMENT EVALUATION
						Reg staging, round_keys[15];
						InternalDisposition state, ld, len, key;
						staging = Reg64::XMM0;
						for (uint i = 0; i < rounds + 1; i++) round_keys[i] = Reg64::XMM1 << i;
						for (uint i = rounds + 1; i < 15; i++) round_keys[i] = Reg64::NO;
						state.reg = Reg64::RSI;
						ld.reg = Reg64::RDI;
						len.reg = Reg64::RCX;
						key.reg = Reg64::RBX;
						state.flags = key.flags = DispositionPointer;
						ld.flags = len.flags = DispositionRegister;
						state.size = 16;
						ld.size = WordSize;
						len.size = length_size;
						key.size = key_size;
						encode_preserve(state.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &state, reg_in_use | state.reg);
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | state.reg | ld.reg);
						encode_preserve(len.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[2], idle, mem_load, &len, reg_in_use | state.reg | ld.reg | len.reg);
						encode_preserve(key.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[3], idle, mem_load, &key, reg_in_use | state.reg | ld.reg | len.reg | key.reg);
						encode_preserve(staging, uint(-1), 0, !idle);
						for (int i = 0; i < 15; i++) if (round_keys[i]) encode_preserve(round_keys[i], uint(-1), 0, !idle);
						// OPERATION
						if (chain_cbc) *mem_load += 16;
						if (!idle) {
							int auxoffs = (chain_cbc) ? allocate_temporary(TH::MakeSize(16, 0)) : 0;
							encode_mov_xmm_mem(16, round_keys[rounds], key.reg, 0);
							if (key_size == 24) encode_mov_xmm_mem(8, round_keys[1], key.reg, 16);
							else if (key_size == 32) encode_mov_xmm_mem(16, round_keys[1], key.reg, 16);
							uint base_round_words = key_size / 4;
							uint needs_round_words = (rounds + 1) * 4;
							for (uint w = base_round_words; w < needs_round_words; w += 2) {
								auto dest_reg = round_keys[w / 4];
								auto prev1_reg = round_keys[(w - 1) / 4];
								auto prev2_reg = round_keys[(w - base_round_words) / 4];
								if (prev1_reg == round_keys[0]) prev1_reg = round_keys[rounds];
								if (prev2_reg == round_keys[0]) prev2_reg = round_keys[rounds];
								if (w % base_round_words == 0 || (w % base_round_words == 4 && base_round_words > 6)) {
									uint rc_i = w / base_round_words;
									uint rc, w1;
									switch (rc_i)
									{
										case 1: rc = 0x01; break;
										case 2: rc = 0x02; break;
										case 3: rc = 0x04; break;
										case 4: rc = 0x08; break;
										case 5: rc = 0x10; break;
										case 6: rc = 0x20; break;
										case 7: rc = 0x40; break;
										case 8: rc = 0x80; break;
										case 9: rc = 0x1B; break;
										case 10: rc = 0x36; break;
										default: rc = 0x00; break;
									}
									if (w % base_round_words == 0) w1 = (w + 3) % 4;
									else w1 = (w + 3) % 4 - 1;
									encode_aes_keygen(staging, prev1_reg, rc);
									encode_simd_shuffle(4, staging, staging, w1, w1, w1, w1);
								} else {
									auto w1 = (w + 3) % 4;
									encode_mov_xmm_xmm(staging, prev1_reg);
									encode_simd_shuffle(4, staging, staging, w1, w1, w1, w1);
								}
								uint w11 = (w - base_round_words + 1) % 4;
								uint w0 = (w - base_round_words) % 4;
								encode_simd_xor(staging, prev2_reg);
								encode_mov_xmm_xmm(round_keys[0], prev2_reg);
								encode_simd_shuffle(4, round_keys[0], round_keys[0], w11, w11, w11, w11);
								encode_simd_xor(round_keys[0], staging);
								encode_simd_shuffle(4, staging, round_keys[0], w0, w0, w0, w0);
								if (w % 4 == 0) {
									encode_mov_xmm_xmm(dest_reg, staging);
									encode_simd_shuffle(4, dest_reg, dest_reg, 0, 2, 0, 2);
								} else encode_simd_shuffle(4, dest_reg, staging, 0, 1, 0, 2);
							}
							encode_mov_xmm_mem(16, round_keys[0], key.reg, 0);
							if (decrypt) for (int i = 0; i < rounds - 1; i++) encode_aes_inversed_mix_columns(round_keys[i + 1], round_keys[i + 1]);
							if (chain_cbc) {
								encode_mov_xmm_mem(16, staging, state.reg, 0);
								encode_mov_mem_xmm(16, Reg64::RBP, auxoffs, staging);
							}
							if (length_size != 8) encode_mov_zx(8, len.reg, length_size, len.reg);
							int test = _dest.code.Length();
							encode_test(8, len.reg, int(0xFFFFFFF0));
							_dest.code << 0x0F << 0x84 << 0x00 << 0x00 << 0x00 << 0x00; // JZ
							int jz = _dest.code.Length();
							if (decrypt) {
								encode_mov_xmm_mem(16, staging, ld.reg, 0);
								if (chain_cbc) encode_mov_mem_xmm(16, state.reg, 0, staging);
								encode_simd_xor(staging, round_keys[rounds]);
								for (int i = rounds - 1; i > 0; i--) encode_aes_dec(staging, round_keys[i]);
								encode_aes_dec_last(staging, round_keys[0]);
								if (chain_cbc) encode_simd_xor(staging, Reg64::RBP, auxoffs);
								encode_mov_mem_xmm(16, ld.reg, 0, staging);
								if (chain_cbc) {
									encode_mov_xmm_mem(16, staging, state.reg, 0);
									encode_mov_mem_xmm(16, Reg64::RBP, auxoffs, staging);
								}
							} else {
								encode_mov_xmm_mem(16, staging, ld.reg, 0);
								if (chain_cbc) encode_simd_xor(staging, Reg64::RBP, auxoffs);
								encode_simd_xor(staging, round_keys[0]);
								for (int i = 0; i < rounds - 1; i++) encode_aes_enc(staging, round_keys[i + 1]);
								encode_aes_enc_last(staging, round_keys[rounds]);
								encode_mov_mem_xmm(16, ld.reg, 0, staging);
								encode_mov_mem_xmm(16, Reg64::RBP, auxoffs, staging);
							}
							encode_add(ld.reg, 16);
							encode_add(len.reg, -16);
							_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + _dest.code.Length() - 4) = test - _dest.code.Length();
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + jz - 4) = _dest.code.Length() - jz;
							if (chain_cbc) {
								encode_mov_xmm_mem(16, staging, Reg64::RBP, auxoffs);
								encode_mov_mem_xmm(16, state.reg, 0, staging);
							}
						}
						// FINALIZATION
						for (int i = 14; i >= 0; i--) if (round_keys[i]) encode_restore(round_keys[i], uint(-1), 0, !idle);
						encode_restore(staging, uint(-1), 0, !idle);
						encode_restore(key.reg, reg_in_use, 0, !idle);
						encode_restore(len.reg, reg_in_use, 0, !idle);
						encode_restore(ld.reg, reg_in_use, 0, !idle);
						encode_restore(state.reg, reg_in_use, 0, !idle);
						_encode_void_result(idle, mem_load, disp);
					} else if (node.self.index == TransformCryptSha224I || node.self.index == TransformCryptSha256I || node.self.index == TransformCryptSha384I || node.self.index == TransformCryptSha512I) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
						if (object_size(node.retval_spec.size)) throw InvalidArgumentException();
						auto state_size = object_size(node.input_specs[0].size);
						uint data_index;
						if (node.self.index == TransformCryptSha224I) {
							if (state_size != 32) throw InvalidArgumentException();
							data_index = translator_sha2_get_sha224_init_state(_sha_state, _dest);
						} else if (node.self.index == TransformCryptSha256I) {
							if (state_size != 32) throw InvalidArgumentException();
							data_index = translator_sha2_get_sha256_init_state(_sha_state, _dest);
						} else if (node.self.index == TransformCryptSha384I) {
							if (state_size != 64) throw InvalidArgumentException();
							data_index = translator_sha2_get_sha384_init_state(_sha_state, _dest);
						} else if (node.self.index == TransformCryptSha512I) {
							if (state_size != 64) throw InvalidArgumentException();
							data_index = translator_sha2_get_sha512_init_state(_sha_state, _dest);
						}
						// ARGUMENT EVALUATION
						Reg src_ptr = Reg64::RSI;
						InternalDisposition dest;
						dest.reg = Reg64::RDI;
						dest.flags = DispositionPointer;
						dest.size = state_size;
						encode_preserve(dest.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &dest, reg_in_use | dest.reg);
						encode_preserve(src_ptr, reg_in_use, 0, !idle);
						// OPERATION
						if (!idle) {
							encode_put_addr_of(src_ptr, TH::MakeRef(ReferenceData, data_index));
							Reg staging = Reg64::RAX;
							encode_preserve(staging, reg_in_use, 0, true); // Intel's canonical order: febahgdc
							if (state_size == 32) {
								encode_mov_reg_mem(4, staging, src_ptr, 20);
								encode_mov_mem_reg(4, dest.reg, 0, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 16);
								encode_mov_mem_reg(4, dest.reg, 4, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 4);
								encode_mov_mem_reg(4, dest.reg, 8, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 0);
								encode_mov_mem_reg(4, dest.reg, 12, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 28);
								encode_mov_mem_reg(4, dest.reg, 16, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 24);
								encode_mov_mem_reg(4, dest.reg, 20, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 12);
								encode_mov_mem_reg(4, dest.reg, 24, staging);
								encode_mov_reg_mem(4, staging, src_ptr, 8);
								encode_mov_mem_reg(4, dest.reg, 28, staging);
							} else {
								encode_mov_reg_mem(8, staging, src_ptr, 40);
								encode_mov_mem_reg(8, dest.reg, 0, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 32);
								encode_mov_mem_reg(8, dest.reg, 8, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 8);
								encode_mov_mem_reg(8, dest.reg, 16, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 0);
								encode_mov_mem_reg(8, dest.reg, 24, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 56);
								encode_mov_mem_reg(8, dest.reg, 32, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 48);
								encode_mov_mem_reg(8, dest.reg, 40, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 24);
								encode_mov_mem_reg(8, dest.reg, 48, staging);
								encode_mov_reg_mem(8, staging, src_ptr, 16);
								encode_mov_mem_reg(8, dest.reg, 56, staging);
							}
							encode_restore(staging, reg_in_use, 0, true);
						}
						// FINALIZATION
						encode_restore(src_ptr, reg_in_use, 0, !idle);
						encode_restore(dest.reg, reg_in_use, 0, !idle);
						_encode_void_result(idle, mem_load, disp);
					} else if (node.self.index == TransformCryptSha224S || node.self.index == TransformCryptSha256S || node.self.index == TransformCryptSha384S || node.self.index == TransformCryptSha512S) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 3 || node.input_specs.Length() != 3) throw InvalidArgumentException();
						if (object_size(node.input_specs[1].size) != WordSize || object_size(node.retval_spec.size)) throw InvalidArgumentException();
						bool software = (node.self.ref_flags & ReferenceFlagLong) != 0;
						bool sha512;
						auto state_size = object_size(node.input_specs[0].size);
						auto length_size = object_size(node.input_specs[2].size);
						if (length_size != 1 && length_size != 2 && length_size != 4 && length_size != 8) throw InvalidArgumentException();
						uint constant_index, block_size;
						if (node.self.index == TransformCryptSha224S || node.self.index == TransformCryptSha256S) {
							if (state_size != 32) throw InvalidArgumentException();
							constant_index = translator_sha2_get_sha256_words(_sha_state, _dest);
							sha512 = false;
							block_size = 64;
						} else if (node.self.index == TransformCryptSha384S || node.self.index == TransformCryptSha512S) {
							if (state_size != 64) throw InvalidArgumentException();
							constant_index = translator_sha2_get_sha512_words(_sha_state, _dest);
							sha512 = true;
							block_size = 128;
						}
						// ARGUMENT EVALUATION
						InternalDisposition state, ld, len;
						state.reg = Reg64::RSI;
						ld.reg = Reg64::RDI;
						len.reg = Reg64::RCX;
						state.flags = DispositionPointer;
						ld.flags = len.flags = DispositionRegister;
						state.size = state_size;
						ld.size = WordSize;
						len.size = length_size;
						encode_preserve(state.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &state, reg_in_use | state.reg);
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | state.reg | ld.reg);
						encode_preserve(len.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[2], idle, mem_load, &len, reg_in_use | state.reg | ld.reg | len.reg);
						int num_xmm = 0;
						if (sha512) { if (software) num_xmm = 16; } else { if (software) num_xmm = 12; else num_xmm = 7; }
						for (int i = 0; i < num_xmm; i++) encode_preserve(0x10000 << i, uint(-1), 0, !idle);
						// OPERATION
						if (!idle) {
							Reg constant_pointer = Reg64::RBX;
							encode_preserve(constant_pointer, reg_in_use, 0, true);
							encode_put_addr_of(constant_pointer, TH::MakeRef(ReferenceData, constant_index));
							if (length_size != 8) encode_mov_zx(8, len.reg, length_size, len.reg);
							int test = _dest.code.Length();
							if (sha512) encode_test(8, len.reg, int(0xFFFFFF80));
							else encode_test(8, len.reg, int(0xFFFFFFC0));
							_dest.code << 0x0F << 0x84 << 0x00 << 0x00 << 0x00 << 0x00; // JZ
							int jz = _dest.code.Length();
							if (sha512) {
								if (software) {
									// Loading the state into fe:ba:hg:dc
									Reg staging_0 = Reg64::XMM0, staging_1 = Reg64::XMM1, staging_2 = Reg64::XMM2, staging_3 = Reg64::XMM3;
									Reg fe = Reg64::XMM4, ba = Reg64::XMM5, hg = Reg64::XMM6, dc = Reg64::XMM7;
									Reg words[8] = { Reg64::XMM8, Reg64::XMM9, Reg64::XMM10, Reg64::XMM11, Reg64::XMM12, Reg64::XMM13, Reg64::XMM14, Reg64::XMM15 };
									encode_mov_xmm_mem(16, fe, state.reg, 0);
									encode_mov_xmm_mem(16, ba, state.reg, 16);
									encode_mov_xmm_mem(16, hg, state.reg, 32);
									encode_mov_xmm_mem(16, dc, state.reg, 48);
									// Loading the block into XMM8::XMM15 === words; changing the endianess
									for (int i = 0; i < 8; i++) {
										auto w = words[i];
										encode_mov_xmm_mem(16, w, ld.reg, 16 * i);
										encode_simd_shuffle_dwords(w, w, 1, 0, 3, 2);
										encode_mov_xmm_xmm(staging_0, w);
										encode_simd_shift_left(4, w, 16);
										encode_simd_shift_right(4, staging_0, 16);
										encode_simd_or(w, staging_0);
										encode_mov_xmm_xmm(staging_0, w);
										encode_simd_shift_left(2, w, 8);
										encode_simd_shift_right(2, staging_0, 8);
										encode_simd_or(w, staging_0);
									}
									// Running the rounds
									for (int r = 0; r < 80; r += 2) {
										int wri = r / 2;
										Reg w;
										if (r >= 16) {
											w = words[wri % 8];
											Reg w1 = words[(wri + 1) % 8];
											Reg w4 = words[(wri + 4) % 8];
											Reg w5 = words[(wri + 5) % 8];
											Reg w7 = words[(wri + 7) % 8];
											encode_mov_xmm_xmm(staging_0, w1);
											encode_simd_extract(staging_0, w, 8); // staging_0 = w[r-15]
											encode_mov_xmm_xmm(staging_1, staging_0);
											encode_mov_xmm_xmm(staging_2, staging_0);
											encode_mov_xmm_xmm(staging_3, staging_0);
											encode_simd_shift_right(8, staging_1, 1);
											encode_simd_shift_left(8, staging_2, 63);
											encode_simd_shift_right(8, staging_3, 7);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3);
											encode_mov_xmm_xmm(staging_2, staging_0);
											encode_mov_xmm_xmm(staging_3, staging_0);
											encode_simd_shift_right(8, staging_2, 8);
											encode_simd_shift_left(8, staging_3, 56);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1 = S0(w[r-15])
											encode_simd_int_add(8, w, staging_1); // w = S0(w[r-15]) + w[r-16]
											encode_mov_xmm_xmm(staging_0, w5);
											encode_simd_extract(staging_0, w4, 8); // staging_0 = w[r-7]
											encode_simd_int_add(8, w, staging_0); // w = S0(w[r-15]) + w[r-16] + w[r-7]
											encode_mov_xmm_xmm(staging_1, w7);
											encode_mov_xmm_xmm(staging_2, w7);
											encode_mov_xmm_xmm(staging_3, w7);
											encode_simd_shift_right(8, staging_1, 19);
											encode_simd_shift_left(8, staging_2, 45);
											encode_simd_shift_right(8, staging_3, 6);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3);
											encode_mov_xmm_xmm(staging_2, w7);
											encode_mov_xmm_xmm(staging_3, w7);
											encode_simd_shift_right(8, staging_2, 61);
											encode_simd_shift_left(8, staging_3, 3);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1 = S1(w[r-2])
											encode_simd_int_add(8, w, staging_1); // w = S0(w[r-15]) + w[r-16] + w[r-7] + S1(w[r-2])
										} else w = words[wri];
										for (int i = 0; i < 2; i++) {
											encode_mov_xmm_xmm(staging_1, fe);
											encode_mov_xmm_xmm(staging_2, fe);
											encode_simd_shift_right(8, staging_1, 14);
											encode_simd_shift_left(8, staging_2, 50);
											encode_simd_xor(staging_1, staging_2); // staging_1[1] = e >>> 14
											encode_mov_xmm_xmm(staging_2, fe);
											encode_mov_xmm_xmm(staging_3, fe);
											encode_simd_shift_right(8, staging_2, 18);
											encode_simd_shift_left(8, staging_3, 46);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1[1] ^= e >>> 18
											encode_mov_xmm_xmm(staging_2, fe);
											encode_mov_xmm_xmm(staging_3, fe);
											encode_simd_shift_right(8, staging_2, 41);
											encode_simd_shift_left(8, staging_3, 23);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1[1] ^= e >>> 41 === S1
											encode_simd_shuffle_dwords(staging_2, fe, 0, 1, 0, 1); // staging_2[*] = f
											encode_simd_and(staging_2, fe); // staging_2[1] = e AND f
											encode_mov_xmm_xmm(staging_3, fe);
											encode_simd_not_and(staging_3, hg); // staging_3[1] = NOT e AND g
											encode_simd_xor(staging_2, staging_3); // staging_2[1] = ch
											encode_simd_int_add(8, staging_1, staging_2); // staging_1[1] = S1 + ch
											encode_simd_shuffle_dwords(staging_2, hg, 0, 1, 0, 1); // staging_2[*] = h
											encode_simd_int_add(8, staging_1, staging_2); // staging_1[1] = S1 + ch + h
											encode_mov_xmm_mem_hi(8, staging_0, constant_pointer, 8 * (r + i)); // staging_0[1] = k[i]
											if (i) encode_simd_shuffle_dwords(staging_2, w, 2, 3, 2, 3);
											else encode_simd_shuffle_dwords(staging_2, w, 0, 1, 0, 1); // staging_2[1] = w[i]
											encode_simd_int_add(8, staging_0, staging_2);
											encode_simd_int_add(8, staging_0, staging_1); // staging_0[1] = T1
											encode_mov_xmm_xmm(staging_1, ba);
											encode_mov_xmm_xmm(staging_2, ba);
											encode_simd_shift_right(8, staging_1, 28);
											encode_simd_shift_left(8, staging_2, 36);
											encode_simd_xor(staging_1, staging_2); // staging_1[1] = a >>> 28
											encode_mov_xmm_xmm(staging_2, ba);
											encode_mov_xmm_xmm(staging_3, ba);
											encode_simd_shift_right(8, staging_2, 34);
											encode_simd_shift_left(8, staging_3, 30);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1[1] ^= a >>> 34
											encode_mov_xmm_xmm(staging_2, ba);
											encode_mov_xmm_xmm(staging_3, ba);
											encode_simd_shift_right(8, staging_2, 39);
											encode_simd_shift_left(8, staging_3, 25);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1[1] ^= a >>> 39 === S0
											encode_mov_xmm_xmm(staging_2, ba);
											encode_simd_extract(staging_2, dc, 8); // staging_2 = cb
											encode_simd_and(staging_2, ba); // staging_2 = (c AND b)(b AND a)
											encode_mov_xmm_xmm(staging_3, ba);
											encode_simd_and(staging_3, dc);
											encode_simd_xmm_shift_right(staging_3, 8); // staging_3 = (a AND c)0
											encode_simd_xor(staging_2, staging_3); // staging_2 = ( (c AND b) XOR (a AND c) )(b AND a)
											encode_simd_shuffle_dwords(staging_3, staging_2, 2, 3, 2, 3);
											encode_simd_xor(staging_2, staging_3); // staging_2[0] = (c AND b) XOR (a AND c) XOR (b AND a) === med(a, b, c)
											encode_simd_shuffle_dwords(staging_1, staging_1, 2, 3, 2, 3);
											encode_simd_int_add(8, staging_1, staging_2); // staging_1[0] = T2
											encode_simd_shuffle_dwords(staging_0, staging_0, 2, 3, 2, 3); // staging_0[*] = T1
											encode_simd_int_add(8, staging_1, staging_0); // staging_1[0] = T1 + T2
											encode_mov_xmm_xmm(staging_2, fe);
											encode_simd_extract(staging_2, hg, 8);
											encode_mov_xmm_xmm(hg, staging_2);
											encode_mov_xmm_xmm(staging_2, dc);
											encode_simd_extract(staging_2, fe, 8);
											encode_mov_xmm_xmm(fe, staging_2);
											encode_mov_xmm_xmm(staging_2, ba);
											encode_simd_extract(staging_2, dc, 8);
											encode_mov_xmm_xmm(dc, staging_2);
											encode_mov_xmm_xmm(staging_2, staging_1);
											encode_simd_extract(staging_2, ba, 8);
											encode_mov_xmm_xmm(ba, staging_2); // ?(T1+T2):ab -> (T1+T2)a
											encode_simd_xmm_shift_left(staging_0, 8);
											encode_simd_int_add(8, fe, staging_0);
										}
									}
									encode_mov_xmm_mem(16, words[0], state.reg, 0);
									encode_mov_xmm_mem(16, words[1], state.reg, 16);
									encode_mov_xmm_mem(16, words[2], state.reg, 32);
									encode_mov_xmm_mem(16, words[3], state.reg, 48);
									encode_simd_int_add(8, fe, words[0]);
									encode_simd_int_add(8, ba, words[1]);
									encode_simd_int_add(8, hg, words[2]);
									encode_simd_int_add(8, dc, words[3]);
									encode_mov_mem_xmm(16, state.reg, 0, fe);
									encode_mov_mem_xmm(16, state.reg, 16, ba);
									encode_mov_mem_xmm(16, state.reg, 32, hg);
									encode_mov_mem_xmm(16, state.reg, 48, dc);
								} else {
									encode_debugger_trap(); // Not implemented by now
								}
							} else {
								// Loading the state into feba:hgdc
								Reg staging_0 = Reg64::XMM0;
								Reg staging_sw_1 = Reg64::XMM7, staging_sw_2 = Reg64::XMM8, staging_sw_3 = Reg64::XMM9, staging_sw_4 = Reg64::XMM10, staging_sw_5 = Reg64::XMM11;
								Reg feba = Reg64::XMM1, hgdc = Reg64::XMM2;
								Reg words[4] = { Reg64::XMM3, Reg64::XMM4, Reg64::XMM5, Reg64::XMM6 };
								encode_mov_xmm_mem(16, feba, state.reg, 0);
								encode_mov_xmm_mem(16, hgdc, state.reg, 16);
								// Loading the block into XMM3:XMM4:XMM5:XMM6 === words; changing the endianess
								for (int i = 0; i < 4; i++) {
									auto w = words[i];
									encode_mov_xmm_mem(16, w, ld.reg, 16 * i);
									encode_mov_xmm_xmm(staging_0, w);
									encode_simd_shift_left(4, w, 16);
									encode_simd_shift_right(4, staging_0, 16);
									encode_simd_or(w, staging_0);
									encode_mov_xmm_xmm(staging_0, w);
									encode_simd_shift_left(2, w, 8);
									encode_simd_shift_right(2, staging_0, 8);
									encode_simd_or(w, staging_0);
								}
								// Running the rounds
								for (int r = 0; r < 64; r += 4) {
									int wri = r / 4;
									Reg w;
									if (r >= 16) {
										w = words[wri % 4];
										Reg w1 = words[(wri + 1) % 4];
										Reg w2 = words[(wri + 2) % 4];
										Reg w3 = words[(wri + 3) % 4];
										if (software) {
											encode_mov_xmm_xmm(staging_0, w1);
											encode_simd_extract(staging_0, w, 4);
											encode_mov_xmm_xmm(staging_sw_4, staging_0);
											encode_mov_xmm_xmm(staging_sw_1, staging_0);
											encode_mov_xmm_xmm(staging_sw_2, staging_0);
											encode_mov_xmm_xmm(staging_sw_3, staging_0);
											encode_simd_shift_right(4, staging_sw_4, 7);
											encode_simd_shift_left(4, staging_sw_2, 25);
											encode_simd_shift_right(4, staging_sw_1, 18);
											encode_simd_shift_left(4, staging_sw_3, 14);
											encode_simd_shift_right(4, staging_0, 3);
											encode_simd_xor(staging_0, staging_sw_4);
											encode_simd_xor(staging_0, staging_sw_1);
											encode_simd_xor(staging_0, staging_sw_2);
											encode_simd_xor(staging_0, staging_sw_3);
											encode_simd_int_add(4, w, staging_0);
										} else encode_sha256_msg_words4_part1(w, w1);
										// w = S0(w[r-15]) + w[r-16]
										encode_mov_xmm_xmm(staging_0, w3);
										encode_simd_extract(staging_0, w2, 4);
										// staging_0 = w[r-7]
										encode_simd_int_add(4, w, staging_0);
										// w = S0(w[r-15]) + w[r-16] + w[r-7]
										if (software) {
											encode_mov_xmm_xmm(staging_0, w3);
											encode_simd_xmm_shift_right(staging_0, 8);
											encode_mov_xmm_xmm(staging_sw_4, staging_0);
											encode_mov_xmm_xmm(staging_sw_1, staging_0);
											encode_mov_xmm_xmm(staging_sw_2, staging_0);
											encode_mov_xmm_xmm(staging_sw_3, staging_0);
											encode_simd_shift_right(4, staging_sw_4, 17);
											encode_simd_shift_left(4, staging_sw_2, 15);
											encode_simd_shift_right(4, staging_sw_1, 19);
											encode_simd_shift_left(4, staging_sw_3, 13);
											encode_simd_shift_right(4, staging_0, 10);
											encode_simd_xor(staging_0, staging_sw_4);
											encode_simd_xor(staging_0, staging_sw_1);
											encode_simd_xor(staging_0, staging_sw_2);
											encode_simd_xor(staging_0, staging_sw_3);
											encode_simd_int_add(4, w, staging_0);
											encode_mov_xmm_xmm(staging_0, w);
											encode_simd_xmm_shift_left(staging_0, 8);
											encode_mov_xmm_xmm(staging_sw_4, staging_0);
											encode_mov_xmm_xmm(staging_sw_1, staging_0);
											encode_mov_xmm_xmm(staging_sw_2, staging_0);
											encode_mov_xmm_xmm(staging_sw_3, staging_0);
											encode_simd_shift_right(4, staging_sw_4, 17);
											encode_simd_shift_left(4, staging_sw_2, 15);
											encode_simd_shift_right(4, staging_sw_1, 19);
											encode_simd_shift_left(4, staging_sw_3, 13);
											encode_simd_shift_right(4, staging_0, 10);
											encode_simd_xor(staging_0, staging_sw_4);
											encode_simd_xor(staging_0, staging_sw_1);
											encode_simd_xor(staging_0, staging_sw_2);
											encode_simd_xor(staging_0, staging_sw_3);
											encode_simd_int_add(4, w, staging_0);
										} else encode_sha256_msg_words4_part2(w, w3);
										// w = S0(w[r-15]) + w[r-16] + w[r-7] + S1(w[r-2])
									} else w = words[wri];
									encode_mov_xmm_mem(16, staging_0, constant_pointer, 4 * r);
									encode_simd_int_add(4, staging_0, w);
									// staging_0 = w[r..r+3] + k[r..r+3]
									if (software) {
										for (int i = 0; i < 4; i++) {
											encode_mov_xmm_xmm(staging_sw_4, feba); // staging_0[1] = e
											encode_mov_xmm_xmm(staging_sw_1, staging_sw_4);
											encode_mov_xmm_xmm(staging_sw_2, staging_sw_4);
											encode_simd_shift_right(4, staging_sw_1, 6);
											encode_simd_shift_left(4, staging_sw_2, 26);
											encode_simd_xor(staging_sw_1, staging_sw_2); // staging_sw_1[1] = e >>> 6
											encode_mov_xmm_xmm(staging_sw_2, staging_sw_4);
											encode_mov_xmm_xmm(staging_sw_3, staging_sw_4);
											encode_simd_shift_right(4, staging_sw_2, 11);
											encode_simd_shift_left(4, staging_sw_3, 21);
											encode_simd_xor(staging_sw_2, staging_sw_3); // staging_sw_2[1] = e >>> 11
											encode_simd_xor(staging_sw_1, staging_sw_2);
											encode_mov_xmm_xmm(staging_sw_2, staging_sw_4);
											encode_mov_xmm_xmm(staging_sw_3, staging_sw_4);
											encode_simd_shift_right(4, staging_sw_2, 25);
											encode_simd_shift_left(4, staging_sw_3, 7);
											encode_simd_xor(staging_sw_2, staging_sw_3); // staging_sw_2[1] = e >>> 25
											encode_simd_xor(staging_sw_1, staging_sw_2); // staging_sw_1[1] = S1
											encode_simd_shuffle_dwords(staging_sw_2, feba, 0, 0, 0, 0);
											encode_simd_and(staging_sw_2, staging_sw_4);
											encode_simd_not_and(staging_sw_4, hgdc);
											encode_simd_xor(staging_sw_2, staging_sw_4); // staging_sw_2[1] = ch
											encode_simd_int_add(4, staging_sw_1, staging_sw_2);
											encode_simd_shuffle_dwords(staging_sw_1, staging_sw_1, 1, 1, 1, 1);
											encode_simd_shuffle_dwords(staging_sw_2, staging_0, 0, 0, 0, 0);
											encode_simd_shuffle_dwords(staging_sw_3, hgdc, 0, 0, 0, 0);
											encode_simd_int_add(4, staging_sw_1, staging_sw_2);
											encode_simd_int_add(4, staging_sw_1, staging_sw_3); // staging_sw_1[*] = T1
											encode_mov_xmm_xmm(staging_sw_2, feba); // staging_sw_2[3] = a
											encode_mov_xmm_xmm(staging_sw_3, staging_sw_2);
											encode_mov_xmm_xmm(staging_sw_4, staging_sw_2);
											encode_simd_shift_right(4, staging_sw_3, 2);
											encode_simd_shift_left(4, staging_sw_4, 30);
											encode_simd_xor(staging_sw_3, staging_sw_4); // staging_sw_3[3] = a >>> 2
											encode_mov_xmm_xmm(staging_sw_4, staging_sw_2);
											encode_mov_xmm_xmm(staging_sw_5, staging_sw_2);
											encode_simd_shift_right(4, staging_sw_4, 13);
											encode_simd_shift_left(4, staging_sw_5, 19);
											encode_simd_xor(staging_sw_4, staging_sw_5); // staging_sw_4[3] = a >>> 13
											encode_simd_xor(staging_sw_3, staging_sw_4);
											encode_mov_xmm_xmm(staging_sw_4, staging_sw_2);
											encode_mov_xmm_xmm(staging_sw_5, staging_sw_2);
											encode_simd_shift_right(4, staging_sw_4, 22);
											encode_simd_shift_left(4, staging_sw_5, 10);
											encode_simd_xor(staging_sw_4, staging_sw_5); // staging_sw_4[3] = a >>> 22
											encode_simd_xor(staging_sw_3, staging_sw_4); // staging_sw_3[3] = S0
											encode_simd_shuffle_dwords(staging_sw_4, feba, 2, 2, 2, 2);
											encode_simd_shuffle_dwords(staging_sw_5, feba, 2, 2, 2, 2);
											encode_simd_and(staging_sw_4, feba);
											encode_simd_and(staging_sw_5, hgdc);
											encode_simd_and(staging_sw_2, hgdc);
											encode_simd_xor(staging_sw_2, staging_sw_4);
											encode_simd_xor(staging_sw_2, staging_sw_5); // staging_sw_2[3] = med(a, b, c)
											encode_simd_int_add(4, staging_sw_2, staging_sw_3); // staging_sw_2[3] = T2
											encode_mov_xmm_xmm(staging_sw_3, feba);
											encode_simd_shuffle(4, feba, hgdc, 3, 2, 3, 2);
											encode_simd_shuffle(4, staging_sw_3, hgdc, 1, 0, 1, 0); // feba:staging_sw_3 = abcdefgh
											encode_simd_extract(staging_sw_3, feba, 12);
											encode_simd_xmm_shift_left(feba, 4); // feba:staging_sw_3 = 0abcdefg
											encode_simd_xmm_shift_right(staging_sw_2, 12);
											encode_simd_xmm_shift_right(staging_sw_1, 12);
											encode_simd_int_add(4, feba, staging_sw_1);
											encode_simd_int_add(4, feba, staging_sw_2);
											encode_simd_int_add(4, staging_sw_3, staging_sw_1); // feba:staging_sw_3 = abcdefgh (new)
											encode_mov_xmm_xmm(hgdc, staging_sw_3);
											encode_simd_shuffle(4, hgdc, feba, 3, 2, 3, 2);
											encode_simd_shuffle(4, staging_sw_3, feba, 1, 0, 1, 0);
											encode_mov_xmm_xmm(feba, staging_sw_3);
											if (i < 3) encode_simd_xmm_shift_right(staging_0, 4);
										}
									} else {
										encode_sha256_rounds2(hgdc, feba, staging_0);
										encode_simd_xmm_shift_right(staging_0, 8);
										encode_sha256_rounds2(feba, hgdc, staging_0);
									}
								}
								encode_mov_xmm_mem(16, words[0], state.reg, 0);
								encode_mov_xmm_mem(16, words[1], state.reg, 16);
								encode_simd_int_add(4, feba, words[0]);
								encode_simd_int_add(4, hgdc, words[1]);
								encode_mov_mem_xmm(16, state.reg, 0, feba);
								encode_mov_mem_xmm(16, state.reg, 16, hgdc);
							}
							encode_add(ld.reg, block_size);
							encode_add(len.reg, -int(block_size));
							_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + _dest.code.Length() - 4) = test - _dest.code.Length();
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + jz - 4) = _dest.code.Length() - jz;
							encode_restore(constant_pointer, reg_in_use, 0, true);
						}
						// FINALIZATION
						for (int i = num_xmm - 1; i >= 0; i--) encode_restore(0x10000 << i, uint(-1), 0, !idle);
						encode_restore(len.reg, reg_in_use, 0, !idle);
						encode_restore(ld.reg, reg_in_use, 0, !idle);
						encode_restore(state.reg, reg_in_use, 0, !idle);
						_encode_void_result(idle, mem_load, disp);
					} else if (node.self.index == TransformCryptSha224F || node.self.index == TransformCryptSha256F || node.self.index == TransformCryptSha384F || node.self.index == TransformCryptSha512F) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
						if (object_size(node.retval_spec.size)) throw InvalidArgumentException();
						auto state_size = object_size(node.input_specs[0].size);
						bool sha512;
						if (node.self.index == TransformCryptSha224F || node.self.index == TransformCryptSha256F) {
							if (state_size != 32) throw InvalidArgumentException();
							sha512 = false;
						} else if (node.self.index == TransformCryptSha384F || node.self.index == TransformCryptSha512F) {
							if (state_size != 64) throw InvalidArgumentException();
							sha512 = true;
						}
						// ARGUMENT EVALUATION
						Reg src_ptr = Reg64::RSI;
						InternalDisposition dest;
						dest.reg = Reg64::RDI;
						dest.flags = DispositionPointer;
						dest.size = state_size;
						encode_preserve(dest.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &dest, reg_in_use | dest.reg);
						encode_preserve(src_ptr, reg_in_use, 0, !idle);
						// OPERATION
						int num_xmm = 0;
						if (sha512) num_xmm = 5; else num_xmm = 3;
						for (int i = 0; i < num_xmm; i++) encode_preserve(0x10000 << i, uint(-1), 0, !idle);
						if (!idle) {
							if (state_size == 32) {
								Reg feba = Reg64::XMM0, hgdc = Reg64::XMM1;
								Reg staging = Reg64::XMM2;
								// Loading and ordering the words
								encode_mov_xmm_mem(16, feba, dest.reg, 0);
								encode_mov_xmm_mem(16, hgdc, dest.reg, 16);
								encode_mov_xmm_xmm(staging, feba);
								encode_simd_shuffle(4, staging, hgdc, 3, 2, 3, 2);	// staging	= abcd
								encode_simd_shuffle(4, feba, hgdc, 1, 0, 1, 0);		// feba		= efgh
								// Byte order swap on staging
								encode_mov_xmm_xmm(hgdc, staging);
								encode_simd_shift_left(4, hgdc, 16);
								encode_simd_shift_right(4, staging, 16);
								encode_simd_or(staging, hgdc);
								encode_mov_xmm_xmm(hgdc, staging);
								encode_simd_shift_left(2, hgdc, 8);
								encode_simd_shift_right(2, staging, 8);
								encode_simd_or(staging, hgdc);
								// Byte order swap on feba
								encode_mov_xmm_xmm(hgdc, feba);
								encode_simd_shift_left(4, hgdc, 16);
								encode_simd_shift_right(4, feba, 16);
								encode_simd_or(feba, hgdc);
								encode_mov_xmm_xmm(hgdc, feba);
								encode_simd_shift_left(2, hgdc, 8);
								encode_simd_shift_right(2, feba, 8);
								encode_simd_or(feba, hgdc);
								// Saving the hash
								encode_mov_mem_xmm(16, dest.reg, 0, staging);
								encode_mov_mem_xmm(16, dest.reg, 16, feba);
							} else {
								Reg words[4] = { Reg64::XMM0, Reg64::XMM1, Reg64::XMM2, Reg64::XMM3 }; // fe ba hg dc
								Reg staging = Reg64::XMM4;
								// Loading the words
								for (int i = 0; i < 4; i++) encode_mov_xmm_mem(16, words[i], dest.reg, 16 * i);
								for (int i = 0; i < 4; i++) {
									// Selecting the word pair
									Reg inw;
									if (i == 0) inw = words[1];
									else if (i == 1) inw = words[3];
									else if (i == 2) inw = words[0];
									else inw = words[2];
									// Inverting word order and 32-bit word suborder
									encode_simd_shuffle_dwords(inw, inw, 3, 2, 1, 0);
									// Inverting byte order
									encode_mov_xmm_xmm(staging, inw);
									encode_simd_shift_left(4, staging, 16);
									encode_simd_shift_right(4, inw, 16);
									encode_simd_or(inw, staging);
									encode_mov_xmm_xmm(staging, inw);
									encode_simd_shift_left(2, staging, 8);
									encode_simd_shift_right(2, inw, 8);
									encode_simd_or(inw, staging);
									// Saving the hash
									encode_mov_mem_xmm(16, dest.reg, 16 * i, inw);
								}
							}
						}
						// FINALIZATION
						for (int i = num_xmm - 1; i >= 0; i--) encode_restore(0x10000 << i, uint(-1), 0, !idle);
						encode_restore(src_ptr, reg_in_use, 0, !idle);
						encode_restore(dest.reg, reg_in_use, 0, !idle);
						_encode_void_result(idle, mem_load, disp);
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
										uint scale_lb = logarithm(scale);
										encode_preserve(Reg64::RAX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_preserve(Reg64::RCX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg64::RAX;
											offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg64::RSI) | uint(Reg64::RAX));
											if (!idle) {
												if (scale > 1) {
													if (scale_lb != InvalidLogarithm) {
														encode_shl(Reg64::RAX, scale_lb);
													} else {
														encode_mov_reg_const(8, Reg64::RCX, scale);
														encode_mul_div(8, mdOp::MUL, Reg64::RCX);
													}
												}
												encode_operation(8, arOp::ADD, Reg64::RSI, Reg64::RAX);
											}
										}
										encode_restore(Reg64::RCX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg64::RDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg64::RAX, reg_in_use, 0, !idle);
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										uint scale_lb = logarithm(scale);
										encode_preserve(Reg64::RAX, reg_in_use, 0, !idle);
										encode_preserve(Reg64::RDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_preserve(Reg64::RCX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg64::RAX;
											offset.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(Reg64::RSI) | uint(Reg64::RAX));
											if (!idle) {
												if (scale > 1) {
													if (scale_lb != InvalidLogarithm) {
														encode_shl(Reg64::RAX, scale_lb);
													} else {
														encode_mov_reg_const(8, Reg64::RCX, scale);
														encode_mul_div(8, mdOp::MUL, Reg64::RCX);
													}
												}
												encode_operation(8, arOp::ADD, Reg64::RSI, Reg64::RAX);
											}
										}
										encode_restore(Reg64::RCX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg64::RDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
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
								} else if (node.self.index == TransformMove) {
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
										auto effective_frame_size = _allocated_frame_size;
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
										_allocated_frame_size = effective_frame_size;
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
								} else if (node.self.index == TransformAtomicAdd) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									auto size = object_size(node.input_specs[0].size);
									Reg ptr, addend, copy;
									copy = disp->reg;
									if (copy == Reg64::NO) copy = Reg64::RAX;
									addend = (copy == Reg64::RCX) ? Reg64::RAX : Reg64::RCX;
									ptr = (copy == Reg64::RDI) ? Reg64::RBX : Reg64::RDI;
									InternalDisposition a1, a2;
									a1.flags = DispositionPointer;
									a1.reg = ptr;
									a1.size = a2.size = size;
									a2.flags = DispositionRegister;
									a2.reg = addend;
									encode_preserve(ptr, reg_in_use, 0, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | ptr);
									encode_preserve(addend, reg_in_use | ptr, 0, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | ptr | addend);
									encode_preserve(copy, reg_in_use | ptr | addend, disp->reg, !idle);
									if (!idle) {
										encode_mov_reg_reg(size, copy, addend);
										// LOCK
										_dest.code << 0xF0;
										encode_xadd(size, ptr, addend);
										encode_operation(size, arOp::ADD, copy, addend);
									}
									encode_restore(copy, reg_in_use | ptr | addend, disp->reg, !idle);
									encode_restore(addend, reg_in_use | ptr, 0, !idle);
									encode_restore(ptr, reg_in_use, 0, !idle);
									if (disp->flags & DispositionRegister) {
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionPointer) {
										disp->flags = DispositionPointer;
										*mem_load += word_align(node.input_specs[0].size);
										if (!idle) {
											int offset = allocate_temporary(node.input_specs[0].size);
											encode_mov_mem_reg(size, Reg64::RBP, offset, copy);
											encode_lea(copy, Reg64::RBP, offset);
										}
									} else disp->flags = DispositionDiscard;
								} else if (node.self.index == TransformAtomicSet) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									auto size = object_size(node.input_specs[0].size);
									Reg ptr, value;
									value = disp->reg;
									if (value == Reg64::NO) value = Reg64::RAX;
									ptr = (value == Reg64::RDI) ? Reg64::RBX : Reg64::RDI;
									InternalDisposition a1, a2;
									a1.flags = DispositionPointer;
									a1.reg = ptr;
									a1.size = a2.size = size;
									a2.flags = DispositionRegister;
									a2.reg = value;
									encode_preserve(ptr, reg_in_use, 0, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | ptr);
									encode_preserve(value, reg_in_use | ptr, disp->reg, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | ptr | value);
									if (!idle) encode_xchg(size, ptr, value);
									encode_restore(value, reg_in_use | ptr, disp->reg, !idle);
									encode_restore(ptr, reg_in_use, 0, !idle);
									if (disp->flags & DispositionRegister) {
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionPointer) {
										disp->flags = DispositionPointer;
										*mem_load += word_align(node.input_specs[0].size);
										if (!idle) {
											int offset = allocate_temporary(node.input_specs[0].size);
											encode_mov_mem_reg(size, Reg64::RBP, offset, value);
											encode_lea(value, Reg64::RBP, offset);
										}
									} else disp->flags = DispositionDiscard;
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
							} else if (node.self.index >= 0x100 && node.self.index < 0x180) {
								throw InvalidArgumentException();
							} else if (node.self.index >= 0x200 && node.self.index < 0x2FF) {
								_encode_long_arithmetics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x300 && node.self.index < 0x3FF) {
								_encode_cryptography(node, idle, mem_load, disp, reg_in_use);
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
				EncoderContext(Environment osenv, TranslatedFunction & dest, const Function & src) : X86::EncoderContext(osenv, dest, src, true)
				{
					if (osenv == Environment::Windows || osenv == Environment::EFI) _abi = ABI::WindowsX64;
					else if (osenv == Environment::MacOSX || osenv == Environment::Linux) _abi = ABI::UnixX64;
					else throw InvalidArgumentException();
					translator_sha2_state_init(_sha_state);
				}
				virtual void encode_function_prologue(void) override
				{
					SafePointer< Array<ArgumentPassageInfo> > api = _make_interface_layout(_src.retval, _src.inputs.GetBuffer(), _src.inputs.Length());
					_inputs.SetLength(_src.inputs.Length());
					if (_abi == ABI::WindowsX64) {
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
							encode_stack_alloc(WordSize);
						}
						if (align & 0xF) {
							align += WordSize;
							encode_stack_alloc(WordSize);
						}
						_scope_frame_base = -align;
					} else if (_abi == ABI::UnixX64) {
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
							encode_stack_alloc(WordSize);
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
							encode_stack_alloc(WordSize);
						}
						_scope_frame_base = -align;
					}
				}
				virtual void encode_function_epilogue(void) override
				{
					if (_abi == ABI::WindowsX64) {
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
					} else if (_abi == ABI::UnixX64) {
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
							auto effective_frame_size = _allocated_frame_size;
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							JumpRelocStruct rs;
							rs.machine_offset_at = _dest.code.Length() - 4;
							rs.machine_offset_relative_to = _dest.code.Length();
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
							_allocated_frame_size = effective_frame_size;
						} else if (inst.opcode == OpcodeConditionalJump) {
							auto effective_frame_size = _allocated_frame_size;
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
							_allocated_frame_size = effective_frame_size;
						} else if (inst.opcode == OpcodeControlReturn) {
							auto effective_frame_size = _allocated_frame_size;
							_encode_expression_evaluation(inst.tree, Reg64::NO);
							for (auto & scp : _scopes.InversedElements()) encode_finalize_scope(scp);
							encode_function_epilogue();
							_allocated_frame_size = effective_frame_size;
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
				Environment _osenv;
			public:
				TranslatorX64(Environment osenv) : _osenv(osenv) {}
				virtual ~TranslatorX64(void) override {}
				virtual bool Translate(TranslatedFunction & dest, const Function & src) noexcept override
				{
					try {
						dest.Clear();
						dest.data = src.data;
						X64::EncoderContext ctx(_osenv, dest, src);
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
				virtual Environment GetEnvironment(void) noexcept override { return _osenv; }
				virtual string ToString(void) const override { return L"XA-x86-64"; }
			};
		}
		
		IAssemblyTranslator * CreateTranslatorX64(Environment osenv) { return new X64::TranslatorX64(osenv); }
	}
}