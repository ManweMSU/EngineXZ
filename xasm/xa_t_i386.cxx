#include "xa_t_i386.h"
#include "xa_t_x86.h"
#include "xa_type_helper.h"
#include "xa_sha2_commons.h"

using namespace Engine::XA::X86;

namespace Engine
{
	namespace XA
	{
		namespace i386
		{
			constexpr int WordSize = 4;

			class EncoderContext : public X86::EncoderContext
			{
				translator_sha2_state _sha_state;
			private:
				static bool _is_fp_register(Reg reg) { return reg == Reg32::ST0; }
				static bool _is_gp_register(Reg reg) { return uint(reg) & 0xFF; }
				static bool _is_in_pass_by_reference(const ArgumentSpecification & spec) { return spec.semantics == ArgumentSemantics::Object; }
				static bool _is_out_pass_by_reference(const ArgumentSpecification & spec) { return _word_align(spec.size) > 8 || spec.semantics == ArgumentSemantics::Object; }
				static uint32 _word_align(const ObjectSize & size) { uint full_size = size.num_bytes + WordSize * size.num_words; return (uint64(full_size) + 3) / 4 * 4; }
				static Reg _fph_allocate(uint used, uint protect)
				{
					if (!(Reg32::ECX & used) && !(Reg32::ECX & protect)) return Reg32::ECX;
					if (!(Reg32::EDX & used) && !(Reg32::EDX & protect)) return Reg32::EDX;
					if (!(Reg32::EBX & used) && !(Reg32::EBX & protect)) return Reg32::EBX;
					if (!(Reg32::ESI & used) && !(Reg32::ESI & protect)) return Reg32::ESI;
					if (!(Reg32::EDI & used) && !(Reg32::EDI & protect)) return Reg32::EDI;
					if (!(Reg32::ECX & protect)) return Reg32::ECX;
					if (!(Reg32::EDX & protect)) return Reg32::EDX;
					if (!(Reg32::EBX & protect)) return Reg32::EBX;
					if (!(Reg32::ESI & protect)) return Reg32::ESI;
					if (!(Reg32::EDI & protect)) return Reg32::EDI;
					throw InvalidStateException();
				}
				Array<ArgumentPassageInfo> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt, ABI * abiret)
				{
					SafePointer< Array<ArgumentPassageInfo> > result = new Array<ArgumentPassageInfo>(0x40);
					ABI abi = ABI::CDECL;
					if (_osenv == Environment::Windows) for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics == ArgumentSemantics::This) {
						abi = ABI::THISCALL;
						ArgumentPassageInfo info;
						info.index = i;
						info.reg = Reg32::ECX;
						info.indirect = _is_in_pass_by_reference(inputs[i]);
						result->Append(info);
						break;
					}
					if (_is_out_pass_by_reference(output)) {
						ArgumentPassageInfo info;
						info.index = -1;
						info.reg = Reg32::NO;
						info.indirect = true;
						result->Append(info);
					}
					for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics != ArgumentSemantics::This || abi != ABI::THISCALL) {
						ArgumentPassageInfo info;
						info.index = i;
						info.reg = Reg32::NO;
						info.indirect = _is_in_pass_by_reference(inputs[i]);
						result->Append(info);
					}
					*abiret = abi;
					result->Retain();
					return result;
				}
				virtual void encode_finalize_scope(const LocalScope & scope, uint reg_in_use = 0) override
				{
					encode_preserve(Reg32::EAX, reg_in_use, 0, true);
					encode_preserve(Reg32::ECX, reg_in_use, 0, true);
					encode_preserve(Reg32::EDX, reg_in_use, 0, true);
					for (auto & l : scope.locals) if (l.finalizer.final.ref_class != ReferenceNull) {
						auto effective_frame_size = _allocated_frame_size;
						bool skip = false;
						for (auto & i : _init_locals) if (i.bp_offset == l.bp_offset) { skip = true; break; }
						if (skip) continue;
						int stack_growth = 0;
						for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
							auto & arg = l.finalizer.final_args[i];
							encode_put_addr_of(Reg32::ECX, arg);
							encode_push(Reg32::ECX);
						}
						if (_osenv == Environment::Windows) {
							stack_growth = 0;
							encode_lea(Reg32::ECX, Reg32::EBP, l.bp_offset);
						} else {
							stack_growth = (l.finalizer.final_args.Length() + 1) * 4;
							encode_lea(Reg32::ECX, Reg32::EBP, l.bp_offset);
							encode_push(Reg32::ECX);
						}
						encode_put_addr_of(Reg32::EAX, l.finalizer.final);
						encode_call(Reg32::EAX, false);
						if (stack_growth) encode_stack_dealloc(stack_growth);
						_allocated_frame_size = effective_frame_size;
					}
					encode_restore(Reg32::EDX, reg_in_use, 0, true);
					encode_restore(Reg32::ECX, reg_in_use, 0, true);
					encode_restore(Reg32::EAX, reg_in_use, 0, true);
					if (scope.shift_sp) encode_stack_dealloc(scope.frame_size);
				}
				void _encode_arithmetics_shift(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
					auto opcode = node.self.index;
					int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
					encode_preserve(Reg32::ECX, reg_in_use, disp->reg, !idle);
					encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle && size == 8);
					InternalDisposition a1, a2;
					a1.reg = Reg32::EAX; a2.reg = Reg32::ECX;
					a1.size = a2.size = size;
					if (size == 8) a1.flags = a2.flags = DispositionPointer;
					else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
					_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg32::EAX));
					_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg32::EAX) | uint(Reg32::ECX));
					if (size == 8) {
						if (!idle) {
							encode_preserve(Reg32::ESI, reg_in_use, 0, !idle);
							encode_preserve(Reg32::EDI, reg_in_use, 0, !idle);
							encode_mov_reg_mem(4, Reg32::EDX, Reg32::EAX, 4);
							encode_mov_reg_mem(4, Reg32::EAX, Reg32::EAX); // EAX:EDX holds the base argument
							encode_mov_reg_mem(4, Reg32::EDI, Reg32::ECX, 4);
							encode_mov_reg_mem(4, Reg32::ESI, Reg32::ECX); // ESI:EDI holds the shift argument
							int addr1, addr2, addr3, addr4;
							encode_test(4, Reg32::EDI, 0xFFFFFFFF);
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							addr1 = _dest.code.Length();
							encode_test(4, Reg32::ESI, 0xFFFFFFC0);
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							addr2 = _dest.code.Length();
							encode_mov_reg_const(4, Reg32::ECX, 1); // ESI is pure shift now, ECX is 1
							addr3 = _dest.code.Length();
							encode_test(4, Reg32::ESI, 0x3F);
							_dest.code << 0x74; _dest.code << 0x00; // JZ
							addr4 = _dest.code.Length();
							if (opcode == TransformVectorShiftL) {
								encode_shift(4, shOp::SHL, Reg32::EAX);
								encode_shift(4, shOp::RCL, Reg32::EDX);
							} else if (opcode == TransformVectorShiftR) {
								encode_shift(4, shOp::SHR, Reg32::EDX);
								encode_shift(4, shOp::RCR, Reg32::EAX);
							} else if (opcode == TransformVectorShiftAL) {
								encode_shift(4, shOp::SAL, Reg32::EAX);
								encode_shift(4, shOp::RCL, Reg32::EDX);
							} else if (opcode == TransformVectorShiftAR) {
								encode_shift(4, shOp::SAR, Reg32::EDX);
								encode_shift(4, shOp::RCR, Reg32::EAX);
							}
							encode_add(Reg32::ESI, -1);
							_dest.code << 0xEB;
							_dest.code << addr3 - _dest.code.Length() - 1; // LOOP
							_dest.code[addr1 - 1] = _dest.code.Length() - addr1;
							_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL) {
								encode_operation(4, arOp::XOR, Reg32::EAX, Reg32::EAX);
								encode_operation(4, arOp::XOR, Reg32::EDX, Reg32::EDX);
							} else if (opcode == TransformVectorShiftAR) {
								encode_shift(4, shOp::SAR, Reg32::EDX, 31);
								encode_mov_reg_reg(4, Reg32::EAX, Reg32::EDX);
							}
							_dest.code[addr4 - 1] = _dest.code.Length() - addr4;
							encode_restore(Reg32::EDI, reg_in_use, 0, !idle);
							encode_restore(Reg32::ESI, reg_in_use, 0, !idle);
						}
						if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(8, 0));
								encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
								encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::EDX);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionRegister) {
							if (!idle) {
								if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
								if (disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
							}
							disp->flags = DispositionRegister;
						}
					} else {
						if (!idle) {
							if (a2.flags & DispositionPointer) encode_mov_reg_mem(size, Reg32::ECX, Reg32::ECX);
							int mask, max_shift;
							shOp op;
							if (opcode == TransformVectorShiftL) op = shOp::SHL;
							else if (opcode == TransformVectorShiftR) op = shOp::SHR;
							else if (opcode == TransformVectorShiftAL) op = shOp::SAL;
							else if (opcode == TransformVectorShiftAR) op = shOp::SAR;
							if (size == 1) { mask = 0xFFFFFFF8; max_shift = 7; }
							else if (size == 2) { mask = 0xFFFFFFF0; max_shift = 15; }
							else if (size == 4) { mask = 0xFFFFFFE0; max_shift = 31; }
							encode_test(size, Reg32::ECX, mask);
							int addr;
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							addr = _dest.code.Length();
							encode_shift(size, op, Reg32::EAX);
							_dest.code << 0xEB; _dest.code << 0x00;
							_dest.code[addr - 1] = _dest.code.Length() - addr;
							addr = _dest.code.Length();
							if (opcode == TransformVectorShiftAR) encode_shift(size, op, Reg32::EAX, max_shift);
							else encode_mov_reg_const(size, Reg32::EAX, 0);
							_dest.code[addr - 1] = _dest.code.Length() - addr;
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != Reg32::EAX) encode_mov_reg_reg(size, disp->reg, Reg32::EAX);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 4;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(size, 0));
								encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
							disp->flags = DispositionPointer;
						}
					}
					encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle && size == 8);
					encode_restore(Reg32::ECX, reg_in_use, disp->reg, !idle);
					encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
				}
				void _encode_arithmetics_compare(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
					auto opcode = node.self.index;
					int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
					encode_preserve(Reg32::ECX, reg_in_use, disp->reg, !idle);
					encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle && size == 8);
					InternalDisposition a1, a2;
					a1.reg = Reg32::EAX; a2.reg = Reg32::ECX;
					a1.size = a2.size = size;
					if (size == 8) a1.flags = a2.flags = DispositionPointer;
					else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
					_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg32::EAX));
					_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg32::EAX) | uint(Reg32::ECX));
					if (size == 8) {
						bool wants_signed = (opcode == TransformIntegerSLE || opcode == TransformIntegerSGE || opcode == TransformIntegerSL || opcode == TransformIntegerSG);
						bool wants_lesser = (opcode == TransformIntegerULE || opcode == TransformIntegerUL || opcode == TransformIntegerSLE || opcode == TransformIntegerSL);
						bool wants_greater = (opcode == TransformIntegerUGE || opcode == TransformIntegerUG || opcode == TransformIntegerSGE || opcode == TransformIntegerSG);
						bool wants_equal = (opcode == TransformIntegerULE || opcode == TransformIntegerUGE || opcode == TransformIntegerSLE || opcode == TransformIntegerSGE);
						if (!idle) {
							encode_preserve(Reg32::ESI, reg_in_use, 0, true);
							encode_mov_reg_mem(4, Reg32::EDX, Reg32::EAX, 4);
							encode_mov_reg_mem(4, Reg32::EAX, Reg32::EAX); // EAX:EDX - first
							encode_mov_reg_mem(4, Reg32::ESI, Reg32::ECX, 4);
							encode_mov_reg_mem(4, Reg32::ECX, Reg32::ECX); // ESI:ECX - second
							encode_operation(4, arOp::SUB, Reg32::EAX, Reg32::ECX);
							encode_operation(4, arOp::SBB, Reg32::EDX, Reg32::ESI); // EAX:EDX - the difference
							_dest.code << (wants_signed ? 0x7C : 0x72) << 0x00;
							int addr = _dest.code.Length();
							encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							int addr2 = _dest.code.Length();
							encode_mov_reg_const(4, Reg32::EAX, wants_equal ? 1 : 0);
							_dest.code << 0xEB; _dest.code << 0x00;
							int addr3 = _dest.code.Length();
							_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							encode_mov_reg_const(4, Reg32::EAX, wants_greater ? 1 : 0);
							_dest.code << 0xEB; _dest.code << 0x00;
							int addr4 = _dest.code.Length();
							_dest.code[addr - 1] = _dest.code.Length() - addr;
							encode_mov_reg_const(4, Reg32::EAX, wants_lesser ? 1 : 0);
							_dest.code[addr3 - 1] = _dest.code.Length() - addr3;
							_dest.code[addr4 - 1] = _dest.code.Length() - addr4;
							encode_restore(Reg32::ESI, reg_in_use, 0, true);
							encode_operation(4, arOp::XOR, Reg32::EDX, Reg32::EDX);
						}
						if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(8, 0));
								encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
								encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::EDX);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionRegister) {
							if (!idle) {
								if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
								if (disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
							}
							disp->flags = DispositionRegister;
						}
					} else {
						if (!idle) {
							encode_operation(size, arOp::CMP, Reg32::EAX, Reg32::ECX, a2.flags & DispositionPointer);
							uint8 jcc;
							if (opcode == TransformIntegerULE) jcc = 0x76; // JBE
							else if (opcode == TransformIntegerUGE) jcc = 0x73; // JAE
							else if (opcode == TransformIntegerUL) jcc = 0x72; // JB
							else if (opcode == TransformIntegerUG) jcc = 0x77; // JA
							else if (opcode == TransformIntegerSLE) jcc = 0x7E; // JLE
							else if (opcode == TransformIntegerSGE) jcc = 0x7D; // JGE
							else if (opcode == TransformIntegerSL) jcc = 0x7C; // JL
							else if (opcode == TransformIntegerSG) jcc = 0x7F; // JG
							_dest.code << jcc; _dest.code << 4;
							encode_operation(4, arOp::XOR, Reg32::EAX, Reg32::EAX);
							_dest.code << 0xEB; _dest.code << 5;
							encode_mov_reg_const(4, Reg32::EAX, 1);
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != Reg32::EAX) encode_mov_reg_reg(size, disp->reg, Reg32::EAX);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 4;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(size, 0));
								encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
							disp->flags = DispositionPointer;
						}
					}
					encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle && size == 8);
					encode_restore(Reg32::ECX, reg_in_use, disp->reg, !idle);
					encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
				}
				void _encode_arithmetics_core(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					auto opcode = node.self.index;
					if (opcode == TransformIntegerUResize || opcode == TransformIntegerSResize) {
						if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
						int in_size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						int out_size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
						InternalDisposition ld;
						ld.reg = Reg32::EAX;
						ld.size = in_size;
						if (out_size <= 4 || in_size <= 4) ld.flags = DispositionRegister; else ld.flags = DispositionPointer;
						encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
						if (ld.flags & DispositionRegister) {
							encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle && out_size > 4);
							if (!idle) {
								if (opcode == TransformIntegerUResize) {
									if (in_size < 4 && in_size < out_size) {
										auto delta = (4 - in_size) * 8;
										encode_shl(Reg32::EAX, delta);
										encode_shr(Reg32::EAX, delta);
									}
									if (out_size > 4) encode_operation(4, arOp::XOR, Reg32::EDX, Reg32::EDX);
								} else if (opcode == TransformIntegerSResize) {
									if (in_size < 2 && out_size >= 2) { _dest.code << 0x66; _dest.code << 0x98; }
									if (in_size < 4 && out_size >= 4) { _dest.code << 0x98; }
									if (in_size < 8 && out_size >= 8) { encode_mov_reg_reg(4, Reg32::EDX, Reg32::EAX); encode_shift(4, shOp::SAR, Reg32::EDX, 31); }
								}
							}
							if (disp->flags & DispositionPointer) {
								*mem_load += _word_align(TH::MakeSize(out_size, 0));
								if (!idle) {
									int offs = allocate_temporary(TH::MakeSize(out_size, 0));
									if (out_size == 1) encode_mov_mem_reg(1, Reg32::EBP, offs, Reg32::AL);
									else if (out_size == 2) encode_mov_mem_reg(2, Reg32::EBP, offs, Reg32::AX);
									else if (out_size == 4) encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
									else if (out_size == 8) {
										encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
										encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::EDX);
									}
									encode_lea(disp->reg, Reg32::EBP, offs);
								}
								disp->flags = DispositionPointer;
							} else if (disp->flags & DispositionRegister) {
								if (!idle) {
									if ((disp->flags & DispositionCompress) && out_size > 4) encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
									if (disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
								}
								disp->flags = DispositionRegister;
							}
							encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle && out_size > 4);
						} else if (ld.flags & DispositionPointer) {
							if (disp->flags & DispositionPointer) {
								if (disp->reg != Reg32::EAX && !idle) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
								disp->flags = DispositionPointer;
							} else if (disp->flags & DispositionRegister) {
								if (!idle) encode_reg_load_32(disp->reg, Reg32::EAX, 0, 8, disp->flags & DispositionCompress, reg_in_use);
								disp->flags = DispositionRegister;
							}
						}
						encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
					} else {
						int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
						if (opcode == TransformVectorInverse || opcode == TransformVectorIsZero || opcode == TransformVectorNotZero ||
							opcode == TransformIntegerInverse || opcode == TransformIntegerAbs) {
							if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
							encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
							encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle && size > 4);
							InternalDisposition ld;
							ld.reg = Reg32::EAX;
							ld.size = size;
							if (size == 8) ld.flags = DispositionPointer; else ld.flags = DispositionRegister;
							_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(Reg32::EAX));
							if (size == 8) {
								if (!idle) {
									encode_mov_reg_mem(4, Reg32::EDX, Reg32::EAX, 4);
									encode_mov_reg_mem(4, Reg32::EAX, Reg32::EAX); // EDX:EAX - first
									if (opcode == TransformVectorInverse) {
										encode_invert(4, Reg32::EAX);
										encode_invert(4, Reg32::EDX);
									} else if (opcode == TransformVectorIsZero) {
										encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg32::EAX, 1);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg32::EAX, 0);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										encode_operation(4, arOp::XOR, Reg32::EDX, Reg32::EDX);
									} else if (opcode == TransformVectorNotZero) {
										encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg32::EAX, 0);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg32::EAX, 1);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										encode_operation(4, arOp::XOR, Reg32::EDX, Reg32::EDX);
									} else if (opcode == TransformIntegerInverse) {
										encode_preserve(Reg32::ESI, reg_in_use, 0, true);
										encode_preserve(Reg32::EDI, reg_in_use, 0, true);
										encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::ESI);
										encode_operation(4, arOp::XOR, Reg32::EDI, Reg32::EDI);
										encode_operation(4, arOp::SUB, Reg32::ESI, Reg32::EAX);
										encode_operation(4, arOp::SBB, Reg32::EDI, Reg32::EDX);
										encode_mov_reg_reg(4, Reg32::EAX, Reg32::ESI);
										encode_mov_reg_reg(4, Reg32::EDX, Reg32::EDI);
										encode_restore(Reg32::EDI, reg_in_use, 0, true);
										encode_restore(Reg32::ESI, reg_in_use, 0, true);
									} else if (opcode == TransformIntegerAbs) {
										encode_test(4, Reg32::EDX, 0x80000000);
										_dest.code << 0x74; // JZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_preserve(Reg32::ESI, reg_in_use, 0, true);
										encode_preserve(Reg32::EDI, reg_in_use, 0, true);
										encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::ESI);
										encode_operation(4, arOp::XOR, Reg32::EDI, Reg32::EDI);
										encode_operation(4, arOp::SUB, Reg32::ESI, Reg32::EAX);
										encode_operation(4, arOp::SBB, Reg32::EDI, Reg32::EDX);
										encode_mov_reg_reg(4, Reg32::EAX, Reg32::ESI);
										encode_mov_reg_reg(4, Reg32::EDX, Reg32::EDI);
										encode_restore(Reg32::EDI, reg_in_use, 0, true);
										encode_restore(Reg32::ESI, reg_in_use, 0, true);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									}
								}
								if (disp->flags & DispositionPointer) {
									*mem_load += 8;
									if (!idle) {
										int offs = allocate_temporary(TH::MakeSize(8, 0));
										encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
										encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::EDX);
										encode_lea(disp->reg, Reg32::EBP, offs);
									}
									disp->flags = DispositionPointer;
								} else if (disp->flags & DispositionRegister) {
									if (!idle) {
										if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
										if (disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
									}
									disp->flags = DispositionRegister;
								}
							} else {
								if (!idle) {
									if (opcode == TransformVectorInverse) {
										encode_invert(size, Reg32::EAX);
									} else if (opcode == TransformVectorIsZero) {
										encode_test(size, Reg32::EAX, 0xFFFFFFFF);
										int addr;
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg32::EAX, 1);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg32::EAX, 0);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									} else if (opcode == TransformVectorNotZero) {
										encode_test(size, Reg32::EAX, 0xFFFFFFFF);
										int addr;
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg32::EAX, 0);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg32::EAX, 1);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									} else if (opcode == TransformIntegerInverse) {
										encode_negative(size, Reg32::EAX);
									} else if (opcode == TransformIntegerAbs) {
										encode_preserve(Reg32::EDX, reg_in_use, 0, true);
										encode_mov_reg_reg(size, Reg32::EDX, Reg32::EAX);
										if (size == 1) encode_shift(size, shOp::SAR, Reg32::EDX, 7);
										else if (size == 2) encode_shift(size, shOp::SAR, Reg32::EDX, 15);
										else if (size == 4) encode_shift(size, shOp::SAR, Reg32::EDX, 31);
										encode_operation(size, arOp::XOR, Reg32::EAX, Reg32::EDX);
										encode_operation(size, arOp::SUB, Reg32::EAX, Reg32::EDX);
										encode_restore(Reg32::EDX, reg_in_use, 0, true);
									}
								}
								if (disp->flags & DispositionRegister) {
									if (!idle && disp->reg != Reg32::EAX) encode_mov_reg_reg(size, disp->reg, Reg32::EAX);
									disp->flags = DispositionRegister;
								} else if (disp->flags & DispositionPointer) {
									*mem_load += 4;
									if (!idle) {
										int offs = allocate_temporary(TH::MakeSize(size, 0));
										encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
										encode_lea(disp->reg, Reg32::EBP, offs);
									}
									disp->flags = DispositionPointer;
								}
							}
							encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle && size > 4);
							encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
						} else {
							if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
							encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
							encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle);
							encode_preserve(Reg32::ESI, reg_in_use, disp->reg, !idle && size > 4);
							encode_preserve(Reg32::EDI, reg_in_use, disp->reg, !idle && size > 4);
							InternalDisposition a1, a2;
							a1.reg = Reg32::EAX;
							a2.reg = Reg32::EDX;
							a1.size = a2.size = size;
							if (size == 8) a1.flags = a2.flags = DispositionPointer;
							else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
							_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg32::EAX));
							_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg32::EAX) | uint(Reg32::EDX));
							if (size == 8) {
								if (!idle) {
									encode_mov_reg_mem(4, Reg32::ESI, Reg32::EAX, 4);
									encode_mov_reg_mem(4, Reg32::EAX, Reg32::EAX); // ESI:EAX - first
									encode_mov_reg_mem(4, Reg32::EDI, Reg32::EDX, 4);
									encode_mov_reg_mem(4, Reg32::EDX, Reg32::EDX); // EDI:EDX - second
									if (opcode == TransformLogicalSame || opcode == TransformLogicalNotSame) {
										encode_operation(4, arOp::OR, Reg32::EAX, Reg32::ESI);
										encode_operation(4, arOp::OR, Reg32::EDX, Reg32::EDI);
										encode_test(4, Reg32::EAX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x74 : 0x75) << 0x00; // JZ / JNZ
										int addr = _dest.code.Length();
										encode_mov_reg_reg(4, Reg32::EAX, Reg32::EDX);
										_dest.code << 0xEB << 0x00; // JMP
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_test(4, Reg32::EDX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x75 : 0x74) << 0x00; // JNZ / JZ
										int addr2 = _dest.code.Length();
										if (opcode == TransformLogicalSame) encode_mov_reg_const(4, Reg32::EAX, 1);
										else encode_operation(4, arOp::XOR, Reg32::EAX, Reg32::EAX);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
										encode_operation(4, arOp::XOR, Reg32::EDX, Reg32::EDX);
									} else if (opcode == TransformIntegerEQ || opcode == TransformIntegerNEQ) {
										encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::EDI);
										encode_operation(4, arOp::XOR, Reg32::EAX, Reg32::EDX);
										encode_operation(4, arOp::OR, Reg32::EAX, Reg32::ESI);
										_dest.code << 0x74; // JZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg32::EAX, opcode == TransformIntegerEQ ? 0 : 1); // IF NON-ZERO
										_dest.code << 0xEB; // JMP
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg32::EAX, opcode == TransformIntegerEQ ? 1 : 0); // IF ZERO
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::ESI);
									} else if (opcode == TransformVectorAnd) {
										encode_operation(4, arOp::AND, Reg32::EAX, Reg32::EDX);
										encode_operation(4, arOp::AND, Reg32::ESI, Reg32::EDI);
									} else if (opcode == TransformVectorOr) {
										encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
										encode_operation(4, arOp::OR, Reg32::ESI, Reg32::EDI);
									} else if (opcode == TransformVectorXor) {
										encode_operation(4, arOp::XOR, Reg32::EAX, Reg32::EDX);
										encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::EDI);
									} else if (opcode == TransformIntegerAdd) {
										encode_operation(4, arOp::ADD, Reg32::EAX, Reg32::EDX);
										encode_operation(4, arOp::ADC, Reg32::ESI, Reg32::EDI);
									} else if (opcode == TransformIntegerSubt) {
										encode_operation(4, arOp::SUB, Reg32::EAX, Reg32::EDX);
										encode_operation(4, arOp::SBB, Reg32::ESI, Reg32::EDI);
									}
								}
								if (disp->flags & DispositionPointer) {
									*mem_load += 8;
									if (!idle) {
										int offs = allocate_temporary(TH::MakeSize(8, 0));
										encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
										encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::ESI);
										encode_lea(disp->reg, Reg32::EBP, offs);
									}
									disp->flags = DispositionPointer;
								} else if (disp->flags & DispositionRegister) {
									if (!idle) {
										if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg32::EAX, Reg32::ESI);
										if (disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
									}
									disp->flags = DispositionRegister;
								}
							} else {
								if (!idle) {
									if (opcode == TransformLogicalSame || opcode == TransformLogicalNotSame) {
										if (a2.flags & DispositionPointer) encode_mov_reg_mem(size, Reg32::EDX, Reg32::EDX);
										encode_test(size, Reg32::EAX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x74 : 0x75); // JZ / JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_reg(size, Reg32::EAX, Reg32::EDX);
										_dest.code << 0xEB; // JMP
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_test(size, Reg32::EDX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x75 : 0x74); // JNZ / JZ
										_dest.code << 0x00;
										int addr2 = _dest.code.Length();
										if (opcode == TransformLogicalSame) encode_mov_reg_const(size, Reg32::EAX, 1);
										else encode_operation(size, arOp::XOR, Reg32::EAX, Reg32::EAX);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
									} else if (opcode == TransformIntegerEQ || opcode == TransformIntegerNEQ) {
										encode_operation(size, arOp::CMP, Reg32::EAX, Reg32::EDX, a2.flags & DispositionPointer);
										_dest.code << 0x74 << 0x00; // JZ
										int addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg32::EAX, opcode == TransformIntegerEQ ? 0 : 1); // IF NON-ZERO
										_dest.code << 0xEB << 0x00; // JMP
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg32::EAX, opcode == TransformIntegerEQ ? 1 : 0); // IF ZERO
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									} else if (opcode == TransformVectorAnd) encode_operation(size, arOp::AND, Reg32::EAX, Reg32::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformVectorOr) encode_operation(size, arOp::OR, Reg32::EAX, Reg32::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformVectorXor) encode_operation(size, arOp::XOR, Reg32::EAX, Reg32::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformIntegerAdd) encode_operation(size, arOp::ADD, Reg32::EAX, Reg32::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformIntegerSubt) encode_operation(size, arOp::SUB, Reg32::EAX, Reg32::EDX, a2.flags & DispositionPointer);
								}
								if (disp->flags & DispositionRegister) {
									if (!idle && disp->reg != Reg32::EAX) encode_mov_reg_reg(size, disp->reg, Reg32::EAX);
									disp->flags = DispositionRegister;
								} else if (disp->flags & DispositionPointer) {
									*mem_load += 4;
									if (!idle) {
										int offs = allocate_temporary(TH::MakeSize(size, 0));
										encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
										encode_lea(disp->reg, Reg32::EBP, offs);
									}
									disp->flags = DispositionPointer;
								}
							}
							encode_restore(Reg32::EDI, reg_in_use, disp->reg, !idle && size > 4);
							encode_restore(Reg32::ESI, reg_in_use, disp->reg, !idle && size > 4);
							encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle);
							encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
						}
					}
				}
				void _encode_arithmetics_mul_div(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
					auto opcode = node.self.index;
					int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
					encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle);
					encode_preserve(Reg32::ECX, reg_in_use, disp->reg, !idle);
					InternalDisposition a1, a2;
					a1.reg = Reg32::EAX; a2.reg = Reg32::ECX;
					a1.size = a2.size = size;
					if (size == 8) a1.flags = a2.flags = DispositionPointer;
					else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
					_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg32::EAX));
					_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg32::EAX) | uint(Reg32::ECX));
					if (size == 8) {
						if (!idle) {
							encode_preserve(Reg32::EBX, reg_in_use, 0, true);
							encode_preserve(Reg32::ESI, reg_in_use, 0, true);
							encode_preserve(Reg32::EDI, reg_in_use, 0, true);
							if (opcode == TransformIntegerUMul || opcode == TransformIntegerSMul) {
								encode_emulate_mul_64();
								encode_mov_reg_reg(4, Reg32::EAX, Reg32::ECX);
								encode_mov_reg_reg(4, Reg32::EDX, Reg32::EBX);
							} else if (opcode == TransformIntegerUDiv) {
								encode_emulate_div_64();
							} else if (opcode == TransformIntegerSDiv) {
								encode_operation(4, arOp::XOR, Reg32::EBX, Reg32::EBX);
								encode_emulate_collect_signum(Reg32::EAX, Reg32::EBX);
								encode_emulate_collect_signum(Reg32::ECX, Reg32::EBX);
								encode_push(Reg32::EBX);
								encode_emulate_div_64();
								encode_pop(Reg32::EBX);
								encode_stack_dealloc(16);
								encode_emulate_set_signum(Reg32::EAX, Reg32::EDX, Reg32::EBX);
							} else if (opcode == TransformIntegerUMod) {
								encode_emulate_div_64();
								encode_mov_reg_reg(4, Reg32::EAX, Reg32::ESI);
								encode_mov_reg_reg(4, Reg32::EDX, Reg32::EDI);
							} else if (opcode == TransformIntegerSMod) {
								encode_operation(4, arOp::XOR, Reg32::EBX, Reg32::EBX);
								encode_emulate_collect_signum(Reg32::EAX, Reg32::EBX);
								encode_emulate_collect_signum(Reg32::ECX, Reg32::NO);
								encode_push(Reg32::EBX);
								encode_emulate_div_64();
								encode_pop(Reg32::EBX);
								encode_add(Reg32::ESP, 16);
								encode_mov_reg_reg(4, Reg32::EAX, Reg32::ESI);
								encode_mov_reg_reg(4, Reg32::EDX, Reg32::EDI);
								encode_emulate_set_signum(Reg32::EAX, Reg32::EDX, Reg32::EBX);
							}
							encode_restore(Reg32::EDI, reg_in_use, 0, true);
							encode_restore(Reg32::ESI, reg_in_use, 0, true);
							encode_restore(Reg32::EBX, reg_in_use, 0, true);
						}
						if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(8, 0));
								encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
								encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::EDX);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionRegister) {
							if (!idle) {
								if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg32::EAX, Reg32::EDX);
								if (disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
							}
							disp->flags = DispositionRegister;
						}
					} else {
						Reg result;
						if (!idle) {
							if (opcode == TransformIntegerUMul) {
								result = Reg32::EAX;
								encode_mul_div(size, mdOp::MUL, Reg32::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerSMul) {
								result = Reg32::EAX;
								encode_mul_div(size, mdOp::IMUL, Reg32::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerUDiv) {
								result = Reg32::EAX;
								if (size == 1) encode_operation(1, arOp::XOR, Reg32::AH, Reg32::AH);
								else encode_operation(size, arOp::XOR, Reg32::EDX, Reg32::EDX);
								encode_mul_div(size, mdOp::DIV, Reg32::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerSDiv) {
								result = Reg32::EAX;
								if (size > 1) {
									encode_mov_reg_reg(size, Reg32::EDX, Reg32::EAX);
									encode_shift(size, shOp::SAR, Reg32::EDX, 31);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								encode_mul_div(size, mdOp::IDIV, Reg32::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerUMod) {
								result = Reg32::EDX;
								if (size == 1) encode_operation(1, arOp::XOR, Reg32::AH, Reg32::AH);
								else encode_operation(size, arOp::XOR, Reg32::EDX, Reg32::EDX);
								encode_mul_div(size, mdOp::DIV, Reg32::ECX, a2.flags & DispositionPointer);
								if (size == 1) encode_mov_reg_reg(1, Reg32::DL, Reg32::AH);
							} else if (opcode == TransformIntegerSMod) {
								result = Reg32::EDX;
								if (size > 1) {
									encode_mov_reg_reg(size, Reg32::EDX, Reg32::EAX);
									encode_shift(size, shOp::SAR, Reg32::EDX, 31);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								encode_mul_div(size, mdOp::IDIV, Reg32::ECX, a2.flags & DispositionPointer);
								if (size == 1) encode_mov_reg_reg(1, Reg32::DL, Reg32::AH);
							}
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != result) encode_mov_reg_reg(size, disp->reg, result);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 4;
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(size, 0));
								encode_mov_mem_reg(4, Reg32::EBP, offs, result);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
							disp->flags = DispositionPointer;
						}
					}
					encode_restore(Reg32::ECX, reg_in_use, disp->reg, !idle);
					encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle);
					encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
				}
				void _encode_arithmetics(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					auto opcode = node.self.index;
					if (opcode == TransformVectorAnd || opcode == TransformVectorOr || opcode == TransformVectorXor || opcode == TransformVectorInverse ||
						opcode == TransformVectorIsZero || opcode == TransformVectorNotZero || opcode == TransformIntegerEQ || opcode == TransformIntegerNEQ ||
						opcode == TransformIntegerUResize || opcode == TransformIntegerSResize || opcode == TransformIntegerInverse || opcode == TransformIntegerAbs ||
						opcode == TransformIntegerAdd || opcode == TransformIntegerSubt || opcode == TransformLogicalSame || opcode == TransformLogicalNotSame) {
						_encode_arithmetics_core(node, idle, mem_load, disp, reg_in_use);
					} else if (opcode == TransformIntegerULE || opcode == TransformIntegerUGE || opcode == TransformIntegerUL || opcode == TransformIntegerUG ||
						opcode == TransformIntegerSLE || opcode == TransformIntegerSGE || opcode == TransformIntegerSL || opcode == TransformIntegerSG) {
						_encode_arithmetics_compare(node, idle, mem_load, disp, reg_in_use);
					} else if (opcode == TransformIntegerUMul || opcode == TransformIntegerSMul || opcode == TransformIntegerUDiv || opcode == TransformIntegerSDiv ||
						opcode == TransformIntegerUMod || opcode == TransformIntegerSMod) {
						_encode_arithmetics_mul_div(node, idle, mem_load, disp, reg_in_use);
					} else if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR) {
						_encode_arithmetics_shift(node, idle, mem_load, disp, reg_in_use);
					} else throw InvalidArgumentException();
				}
				void _encode_logics(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformLogicalFork) {
						if (node.inputs.Length() != 3) throw InvalidArgumentException();
						InternalDisposition cond, none;
						cond.reg = Reg32::ECX;
						cond.flags = DispositionRegister | DispositionCompress;
						cond.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						none.reg = Reg32::NO;
						none.flags = DispositionDiscard;
						none.size = 0;
						if (!cond.size || cond.size > 8) throw InvalidArgumentException();
						encode_preserve(cond.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &cond, reg_in_use | uint(cond.reg));
						if (!idle) {
							encode_test(min(cond.size, 4), cond.reg, 0xFFFFFFFF);
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
						Reg local = disp->reg != Reg32::NO ? disp->reg : Reg32::ECX;
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
							ld.flags = DispositionRegister | DispositionCompress;
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
						if (rv_size > min_size && min_size < 4 && !idle) {
							uint cs = (4 - min_size) * 8;
							encode_shl(local, cs);
							encode_shr(local, cs);
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != local) encode_mov_reg_reg(4, disp->reg, local);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += word_align(node.retval_spec.size);
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size, &offs);
								encode_mov_mem_reg(4, Reg32::EBP, offs, local);
								if (rv_size > 4) encode_mov_mem_reg(4, Reg32::EBP, offs + 4, local);
								encode_lea(disp->reg, Reg32::EBP, offs);
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
					ABI conv;
					bool indirect, retval_byref, preserve_eax, retval_final;
					int first_arg, arg_no;
					if (node.self.ref_class == ReferenceTransform && node.self.index == TransformInvoke) {
						indirect = true; first_arg = 1; arg_no = node.inputs.Length() - 1;
					} else {
						indirect = false; first_arg = 0; arg_no = node.inputs.Length();
					}
					preserve_eax = disp->reg != Reg32::EAX;
					retval_byref = _is_out_pass_by_reference(node.retval_spec);
					retval_final = node.retval_final.final.ref_class != ReferenceNull;
					SafePointer< Array<ArgumentPassageInfo> > layout = _make_interface_layout(node.retval_spec, node.input_specs.GetBuffer() + first_arg, arg_no, &conv);
					Array<Reg> preserve_regs(0x10);
					Array<int> argument_homes(1), argument_layout_index(1);
					preserve_regs << Reg32::ECX << Reg32::EDX;
					encode_preserve(Reg32::EAX, reg_in_use, 0, !idle && preserve_eax);
					for (auto & r : preserve_regs.Elements()) encode_preserve(r, reg_in_use, 0, !idle);
					uint reg_used_mask = 0;
					uint stack_usage = 0;
					uint num_args_by_stack = 0;
					for (auto & info : *layout) if (info.reg == Reg32::NO) {
						const XA::ArgumentSpecification * spec;
						if (info.index >= 0) spec = &node.input_specs[info.index + first_arg];
						else spec = &node.retval_spec;
						if (info.indirect) stack_usage += 4; else stack_usage += _word_align(spec->size);
						num_args_by_stack++;
					}
					if (stack_usage && !idle) encode_stack_alloc(stack_usage);
					argument_homes.SetLength(node.inputs.Length() - first_arg);
					argument_layout_index.SetLength(node.inputs.Length() - first_arg);
					uint current_stack_index = 0;
					int rv_offset = 0;
					int rv_home = -1;
					int rv_mem_index = -1;
					for (int i = 0; i < layout->Length(); i++) {
						auto & info = layout->ElementAt(i);
						if (info.index >= 0) {
							argument_layout_index[info.index] = i;
							if (info.reg == Reg32::NO) {
								argument_homes[info.index] = current_stack_index;
								if (info.indirect) current_stack_index += 4;
								else current_stack_index += _word_align(node.input_specs[info.index + first_arg].size);
							} else argument_homes[info.index] = -1;
						} else {
							*mem_load += _word_align(node.retval_spec.size);
							rv_home = current_stack_index;
							current_stack_index += 4;
							if (!idle) rv_offset = allocate_temporary(node.retval_spec.size, &rv_mem_index);
						}
					}
					if (!idle && current_stack_index) {
						encode_mov_reg_reg(4, Reg32::EDX, Reg32::ESP);
						reg_used_mask |= uint(Reg32::EDX);
					}
					if (indirect) {
						InternalDisposition ld;
						ld.flags = DispositionRegister;
						ld.reg = Reg32::EAX;
						ld.size = 4;
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | reg_used_mask | uint(ld.reg));
						if (!idle) encode_push(ld.reg);
					}
					for (int i = 0; i < argument_homes.Length(); i++) {
						auto home = argument_homes[i];
						auto & info = layout->ElementAt(argument_layout_index[i]);
						auto & spec = node.input_specs[i + first_arg];
						if (info.reg == Reg32::NO) {
							InternalDisposition ld;
							ld.reg = Reg32::EAX;
							ld.size = spec.size.num_bytes + WordSize * spec.size.num_words;
							ld.flags = info.indirect ? DispositionPointer : DispositionAny;
							_encode_tree_node(node.inputs[i + first_arg], idle, mem_load, &ld, reg_in_use | reg_used_mask | uint(Reg32::EAX));
							if (info.indirect) {
								if (!idle) encode_mov_mem_reg(4, Reg32::EDX, home, ld.reg);
							} else {
								if (ld.flags & DispositionPointer) {
									if (ld.size == 4) {
										if (!idle) {
											encode_mov_reg_mem(4, ld.reg, ld.reg);
											encode_mov_mem_reg(4, Reg32::EDX, home, ld.reg);
										}
									} else {
										if (!idle) {
											if (home) encode_add(Reg32::EDX, home);
											encode_blt(Reg32::EDX, ld.reg, ld.size, reg_in_use | reg_used_mask | uint(Reg32::EAX));
											if (home) encode_add(Reg32::EDX, -home);
										}
									}
								} else if (ld.flags & DispositionRegister) {
									if (ld.size > 0) { if (!idle) encode_mov_mem_reg(4, Reg32::EDX, home, ld.reg); }
								}
							}
						} else {
							InternalDisposition ld;
							ld.reg = info.reg;
							ld.size = spec.size.num_bytes + WordSize * spec.size.num_words;
							ld.flags = info.indirect ? DispositionPointer : DispositionRegister;
							reg_used_mask |= uint(ld.reg);
							_encode_tree_node(node.inputs[i + first_arg], idle, mem_load, &ld, reg_in_use | reg_used_mask);
						}
					}
					if (!idle && rv_home != -1) {
						encode_lea(Reg32::EAX, Reg32::EBP, rv_offset);
						encode_mov_mem_reg(4, Reg32::EDX, rv_home, Reg32::EAX);
					}
					if (!idle) {
						if (indirect) encode_pop(Reg32::EAX);
						else encode_put_addr_of(Reg32::EAX, node.self);
						encode_call(Reg32::EAX, false);
						if (_osenv != Environment::Windows && retval_byref) stack_usage -= 4;
						if (stack_usage && conv == ABI::CDECL) encode_stack_dealloc(stack_usage);
						else _allocated_frame_size -= stack_usage;
					}
					int quant = _word_align(node.retval_spec.size);
					if (!retval_byref && (node.retval_spec.semantics == ArgumentSemantics::FloatingPoint || quant > 4 || retval_final)) {
						*mem_load += quant;
						if (!idle) {
							int offs = allocate_temporary(node.retval_spec.size);
							if (node.retval_spec.semantics == ArgumentSemantics::FloatingPoint) {
								encode_fstp(quant, Reg32::EBP, offs);
							} else {
								encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
								if (quant > 4) encode_mov_mem_reg(4, Reg32::EBP, offs + 4, Reg32::EDX);
							}
							encode_lea(Reg32::EAX, Reg32::EBP, offs);
						}
						retval_byref = true;
						retval_final = false;
					}
					for (auto & r : preserve_regs.InversedElements()) encode_restore(r, reg_in_use, 0, !idle);
					if ((disp->flags & DispositionPointer) && retval_byref) {
						if (!idle && disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && !retval_byref) {
						if (!idle && disp->reg != Reg32::EAX) encode_mov_reg_reg(4, disp->reg, Reg32::EAX);
						disp->flags = DispositionRegister;
					} else if ((disp->flags & DispositionPointer) && !retval_byref) {
						*mem_load += WordSize;
						if (!idle) {
							int offs = allocate_temporary(TH::MakeSize(0, 1));
							encode_mov_mem_reg(4, Reg32::EBP, offs, Reg32::EAX);
							encode_lea(disp->reg, Reg32::EBP, offs);
						}
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && retval_byref) {
						if (!idle) {
							if (quant == 8 && (disp->flags & DispositionCompress)) {
								Reg local = disp->reg == Reg32::EAX ? Reg32::EDX : disp->reg;
								encode_preserve(local, reg_in_use, disp->reg, true);
								encode_mov_reg_mem(4, local, Reg32::EAX);
								encode_operation(4, arOp::OR, local, Reg32::EAX, true, 4);
								if (local != disp->reg) encode_mov_reg_reg(4, disp->reg, local);
								encode_restore(local, reg_in_use, disp->reg, true);
							} else encode_mov_reg_mem(4, disp->reg, Reg32::EAX);
						}
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					}
					if (rv_mem_index >= 0) assign_finalizer(rv_mem_index, node.retval_final);
					encode_restore(Reg32::EAX, reg_in_use, 0, !idle && preserve_eax);
				}
				void _encode_floating_point(const ExpressionTree & node, bool idle, int * mem_load, InternalDisposition * disp, uint reg_in_use)
				{
					if (node.self.ref_flags & ReferenceFlagShort) throw InvalidArgumentException();
					Reg rv;
					uint sysdisp;
					if (disp->flags & DispositionDiscard) rv = _fph_allocate(reg_in_use, 0);
					else if (disp->reg == Reg32::EAX) rv = _fph_allocate(reg_in_use, 0);
					else rv = disp->reg;
					encode_preserve(rv, reg_in_use, disp->reg, !idle);
					if (node.self.index == TransformFloatResize) {
						sysdisp = DispositionPointer;
						if (node.inputs.Length() != 1) throw InvalidArgumentException();
						uint in_quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int ins = object_size(node.input_specs[0].size);
						int ous = object_size(node.retval_spec.size);
						int dim = ins / in_quant;
						if (dim < 1 || dim > 4) throw InvalidArgumentException();
						if (ins % in_quant) throw InvalidArgumentException();
						if (ous % dim) throw InvalidArgumentException();
						uint out_quant = ous / dim;
						if (out_quant != 4 && out_quant != 8) throw InvalidArgumentException();
						InternalDisposition ld;
						ld.size = object_size(node.input_specs[0].size);
						ld.flags = DispositionPointer;
						ld.reg = _fph_allocate(reg_in_use, rv);
						encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, (reg_in_use & ~rv) | ld.reg);
						if ((ld.flags & DispositionReuse) && (ld.size >= ous)) {
							if (!idle) {
								if (ins != ous) for (int i = 0; i < dim; i++) {
									encode_fld(in_quant, ld.reg, in_quant * i);
									encode_fstp(out_quant, ld.reg, out_quant * i);
								}
								encode_mov_reg_reg(4, rv, ld.reg);
							}
						} else {
							*mem_load += word_align(node.retval_spec.size);
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size);
								encode_lea(rv, Reg32::EBP, offs);
								for (int i = 0; i < dim; i++) {
									encode_fld(in_quant, ld.reg, in_quant * i);
									encode_fstp(out_quant, rv, out_quant * i);
								}
							}
						}
						encode_restore(ld.reg, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformFloatGather) {
						sysdisp = DispositionPointer;
						uint quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int dim = node.inputs.Length();
						int ous = object_size(node.retval_spec.size);
						if (dim < 1 || dim > 4) throw InvalidArgumentException();
						if (ous != quant * dim) throw InvalidArgumentException();
						*mem_load += word_align(node.retval_spec.size);
						int rvp;
						if (!idle) rvp = allocate_temporary(node.retval_spec.size);
						for (int i = 0; i < dim; i++) {
							InternalDisposition ld;
							ld.size = object_size(node.input_specs[i].size);
							ld.reg = rv;
							if (node.input_specs[i].semantics != ArgumentSemantics::FloatingPoint && ld.size == 1) ld.flags = DispositionRegister;
							else ld.flags = DispositionPointer;
							_encode_tree_node(node.inputs[i], idle, mem_load, &ld, reg_in_use | rv);
							if (!idle) {
								if (node.input_specs[i].semantics == ArgumentSemantics::FloatingPoint) {
									encode_fld(ld.size, ld.reg, 0);
									encode_fstp(quant, Reg32::EBP, rvp + i * quant);
								} else if (ld.size == 1) {
									encode_shl(ld.reg, 24);
									encode_shift(4, node.input_specs[i].semantics == ArgumentSemantics::SignedInteger ? shOp::SAR : shOp::SHR, ld.reg, 24);
									encode_mov_mem_reg(4, Reg32::EBP, rvp + i * quant, ld.reg);
									encode_fild(4, Reg32::EBP, rvp + i * quant);
									encode_fstp(quant, Reg32::EBP, rvp + i * quant);
								} else {
									encode_fild(ld.size, ld.reg, 0);
									encode_fstp(quant, Reg32::EBP, rvp + i * quant);
								}
							}
						}
						if (!idle) encode_lea(rv, Reg32::EBP, rvp);
					} else if (node.self.index == TransformFloatScatter) {
						sysdisp = DispositionPointer;
						uint quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int dim = node.inputs.Length() - 1;
						int ous = object_size(node.retval_spec.size);
						int ins = object_size(node.input_specs[0].size);
						if (dim < 1 || dim > 4) throw InvalidArgumentException();
						if (ous != ins || ous != quant * dim) throw InvalidArgumentException();
						InternalDisposition ld;
						ld.size = object_size(node.input_specs[0].size);
						ld.flags = DispositionPointer;
						ld.reg = rv;
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | rv);
						Reg ir = _fph_allocate(reg_in_use, rv);
						encode_preserve(ir, reg_in_use, disp->reg, !idle);
						for (int i = 0; i < dim; i++) {
							InternalDisposition sd;
							sd.size = object_size(node.input_specs[1 + i].size);
							sd.flags = DispositionPointer;
							sd.reg = ir;
							_encode_tree_node(node.inputs[1 + i], idle, mem_load, &sd, reg_in_use | rv | ir);
							if (sd.size == 1) *mem_load += WordSize;
							if (!idle) {
								encode_fld(quant, rv, i * quant);
								if (node.input_specs[1 + i].semantics == ArgumentSemantics::FloatingPoint) {
									encode_fstp(sd.size, ir, 0);
								} else {
									if (sd.size == 1) {
										int ioffs = allocate_temporary(XA::TH::MakeSize(0, 1));
										Reg hr = _fph_allocate(reg_in_use, rv | ir);
										encode_preserve(hr, reg_in_use, disp->reg, true);
										encode_fisttp(4, Reg32::EBP, ioffs);
										encode_mov_reg_mem(1, hr, Reg32::EBP, ioffs);
										encode_mov_mem_reg(1, ir, hr);
										encode_restore(hr, reg_in_use, disp->reg, true);
									} else encode_fisttp(sd.size, ir, 0);
								}
							}
						}
						encode_restore(ir, reg_in_use, disp->reg, !idle);
						if (!((ld.flags & DispositionReuse) || (disp->flags & DispositionDiscard))) {
							*mem_load += word_align(node.retval_spec.size);
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size);
								for (int i = 0; i < dim; i++) {
									encode_fld(quant, rv, i * quant);
									encode_fstp(quant, Reg32::EBP, offs + i * quant);
								}
								encode_lea(rv, Reg32::EBP, offs);
							}
						}
					} else if (node.self.index == TransformFloatRecombine) {
						sysdisp = DispositionPointer;
						if (node.inputs.Length() != 2) throw InvalidArgumentException();
						if (node.inputs[1].self.ref_class != ReferenceLiteral) throw InvalidArgumentException();
						uint quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int ins = object_size(node.input_specs[0].size);
						int ous = object_size(node.retval_spec.size);
						int idim = ins / quant;
						int odim = ous / quant;
						if (idim < 1 || idim > 4 || odim < 1 || odim > 4) throw InvalidArgumentException();
						if (ins % quant || ous % quant) throw InvalidArgumentException();
						InternalDisposition ld;
						ld.size = object_size(node.input_specs[0].size);
						ld.flags = DispositionPointer;
						ld.reg = _fph_allocate(reg_in_use, rv);
						encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, (reg_in_use & ~rv) | ld.reg);
						if ((ld.flags & DispositionReuse) && idim == 1 && odim == 1) {
							if (!idle) encode_mov_reg_reg(4, rv, ld.reg);
						} else {
							*mem_load += word_align(node.retval_spec.size);
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size);
								encode_lea(rv, Reg32::EBP, offs);
								for (int i = 0; i < odim; i++) {
									int j = ((node.input_specs[1].size.num_bytes >> (i * 4)) & 0xF) % idim;
									encode_fld(quant, ld.reg, j * quant);
									encode_fstp(quant, rv, i * quant);
								}
							}
						}
						encode_restore(ld.reg, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformFloatInteger) {
						sysdisp = DispositionPointer;
						if (node.inputs.Length() != 1) throw InvalidArgumentException();
						uint quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int ins = object_size(node.input_specs[0].size);
						int ous = object_size(node.retval_spec.size);
						int dim = ins / quant;
						if (dim < 1 || dim > 4) throw InvalidArgumentException();
						if (ins % quant) throw InvalidArgumentException();
						if (ous % dim) throw InvalidArgumentException();
						uint oquant = ous / dim;
						if (oquant != 1 && oquant != 2 && oquant != 4 && oquant != 8) throw InvalidArgumentException();
						InternalDisposition ld;
						ld.size = ins;
						ld.flags = DispositionPointer;
						ld.reg = _fph_allocate(reg_in_use, rv);
						encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, (reg_in_use & ~rv) | ld.reg);
						*mem_load += word_align(node.retval_spec.size);
						if (oquant == 1) *mem_load += WordSize;
						if (!idle) {
							int offs = allocate_temporary(node.retval_spec.size);
							int ioffs = 0;
							if (oquant == 1) {
								ioffs = allocate_temporary(XA::TH::MakeSize(0, 1));
								encode_preserve(Reg32::EAX, reg_in_use, 0, true);
							}
							encode_lea(rv, Reg32::EBP, offs);
							for (int i = 0; i < dim; i++) {
								encode_fld(quant, ld.reg, quant * i);
								if (oquant == 1) {
									encode_fisttp(2, Reg32::EBP, ioffs);
									encode_mov_reg_mem(1, Reg32::EAX, Reg32::EBP, ioffs);
									encode_mov_mem_reg(1, rv, oquant * i, Reg32::EAX);
								} else encode_fisttp(oquant, rv, oquant * i);
							}
							if (oquant == 1) encode_restore(Reg32::EAX, reg_in_use, 0, true);
						}
						encode_restore(ld.reg, reg_in_use, disp->reg, !idle);
					} else if (node.self.index >= TransformFloatIsZero && node.self.index <= TransformFloatG) {
						sysdisp = DispositionRegister;
						bool cz = (node.self.index == TransformFloatIsZero) || (node.self.index == TransformFloatNotZero);
						if (cz && node.inputs.Length() != 1) throw InvalidArgumentException();
						if (!cz && node.inputs.Length() != 2) throw InvalidArgumentException();
						uint quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int ins = object_size(node.input_specs[0].size);
						int dim = ins / quant;
						if (dim < 1 || dim > 4 || ins % quant) throw InvalidArgumentException();
						InternalDisposition a1, a2;
						a1.size = ins;
						a1.flags = DispositionPointer;
						a1.reg = _fph_allocate(reg_in_use, rv);
						encode_preserve(a1.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &a1, (reg_in_use & ~rv) | a1.reg);
						if (!cz) {
							if (object_size(node.input_specs[1].size) != ins) throw InvalidArgumentException();
							a2.size = ins;
							a2.flags = DispositionPointer;
							a2.reg = _fph_allocate(reg_in_use, rv | a1.reg);
							encode_preserve(a2.reg, reg_in_use, disp->reg, !idle);
							_encode_tree_node(node.inputs[1], idle, mem_load, &a2, (reg_in_use & ~rv) | a1.reg | a2.reg);
						}
						encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
						if (!idle) {
							if (node.self.ref_flags & ReferenceFlagVectorCom) encode_operation(4, arOp::XOR, rv, rv);
							else encode_mov_reg_const(4, rv, 1);
							for (int i = 0; i < dim; i++) {
								if (cz) {
									encode_fldz();
									encode_fld(quant, a1.reg, i * quant);
									encode_fcompp();
								} else {
									encode_fld(quant, a1.reg, i * quant);
									encode_fcomp(quant, a2.reg, i * quant);
								}
								encode_fstsw();
								_dest.code << 0x9E; // SAHF: L ~ CF, E ~ ZF, NaN ~ PF
								int jp_offs, jcc_offs, jmp = 0;
								_dest.code << 0x7A << 0x00; // JP
								jp_offs = _dest.code.Length();
								if (node.self.index == TransformFloatIsZero || node.self.index == TransformFloatEQ) {
									_dest.code << 0x75 << 0x00; // JNZ
								} else if (node.self.index == TransformFloatNotZero || node.self.index == TransformFloatNEQ) {
									_dest.code << 0x74 << 0x00; // JZ
								} else if (node.self.index == TransformFloatLE) {
									_dest.code << 0x77 << 0x00; // JA
								} else if (node.self.index == TransformFloatGE) {
									_dest.code << 0x72 << 0x00; // JB
								} else if (node.self.index == TransformFloatL) {
									_dest.code << 0x73 << 0x00; // JNB
								} else if (node.self.index == TransformFloatG) {
									_dest.code << 0x76 << 0x00; // JNA
								}
								jcc_offs = _dest.code.Length();
								if (node.self.ref_flags & ReferenceFlagVectorCom) encode_xor(rv, 1 << i);
								else { _dest.code << 0xEB << 0x00; jmp = _dest.code.Length(); }
								_dest.code[jp_offs - 1] = _dest.code.Length() - jp_offs;
								_dest.code[jcc_offs - 1] = _dest.code.Length() - jcc_offs;
								if (jmp) {
									encode_operation(4, arOp::XOR, rv, rv);
									_dest.code[jmp - 1] = _dest.code.Length() - jmp;
								}
							}
						}
						encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
						if (!cz) encode_restore(a2.reg, reg_in_use, disp->reg, !idle);
						encode_restore(a1.reg, reg_in_use, disp->reg, !idle);
					} else if (node.self.index >= TransformFloatAdd && node.self.index <= TransformFloatReduce) {
						sysdisp = DispositionPointer;
						if (node.inputs.Length() < 1) throw InvalidArgumentException();
						uint quant = (node.self.ref_flags & ReferenceFlagLong) ? 8 : 4;
						int ins = object_size(node.input_specs[0].size);
						int dim = ins / quant;
						if (dim < 1 || dim > 4 || ins % quant) throw InvalidArgumentException();
						InternalDisposition a1;
						a1.size = ins;
						a1.flags = DispositionPointer;
						a1.reg = rv;
						_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | rv);
						Reg store_reg;
						int store_offs;
						if (a1.flags & DispositionReuse) {
							store_reg = rv;
							store_offs = 0;
						} else {
							*mem_load += word_align(node.retval_spec.size);
							if (!idle) {
								store_reg = Reg32::EBP;
								store_offs = allocate_temporary(node.retval_spec.size);
							}
						}
						if (node.self.index == TransformFloatAbs || node.self.index == TransformFloatInverse || node.self.index == TransformFloatSqrt) {
							if (node.inputs.Length() != 1) throw InvalidArgumentException();
							if (object_size(node.retval_spec.size) != ins) throw InvalidArgumentException();
							if (!idle) for (int i = 0; i < dim; i++) {
								encode_fld(quant, rv, i * quant);
								if (node.self.index == TransformFloatAbs) encode_fabs();
								else if (node.self.index == TransformFloatInverse) encode_fneg();
								else if (node.self.index == TransformFloatSqrt) encode_fsqrt();
								encode_fstp(quant, store_reg, store_offs + i * quant);
							}
						} else if (node.self.index == TransformFloatReduce) {
							if (node.inputs.Length() != 1) throw InvalidArgumentException();
							if (object_size(node.retval_spec.size) != quant) throw InvalidArgumentException();
							if (!idle) {
								encode_fld(quant, rv, 0);
								for (int i = 1; i < dim; i++) encode_fadd(quant, rv, i * quant);
								encode_fstp(quant, store_reg, store_offs);
							}
						} else if (node.self.index == TransformFloatAdd || node.self.index == TransformFloatSubt || node.self.index == TransformFloatMul || node.self.index == TransformFloatDiv) {
							if (node.inputs.Length() != 2) throw InvalidArgumentException();
							if (object_size(node.retval_spec.size) != ins) throw InvalidArgumentException();
							if (object_size(node.input_specs[1].size) != ins) throw InvalidArgumentException();
							InternalDisposition a2;
							a2.size = ins;
							a2.flags = DispositionPointer;
							a2.reg = _fph_allocate(reg_in_use, rv);
							encode_preserve(a2.reg, reg_in_use, disp->reg, !idle);
							_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | rv | a2.reg);
							if (!idle) for (int i = 0; i < dim; i++) {
								encode_fld(quant, rv, i * quant);
								if (node.self.index == TransformFloatAdd) encode_fadd(quant, a2.reg, i * quant);
								else if (node.self.index == TransformFloatSubt) encode_fsub(quant, a2.reg, i * quant);
								else if (node.self.index == TransformFloatMul) encode_fmul(quant, a2.reg, i * quant);
								else if (node.self.index == TransformFloatDiv) encode_fdiv(quant, a2.reg, i * quant);
								encode_fstp(quant, store_reg, store_offs + i * quant);
							}
							encode_restore(a2.reg, reg_in_use, disp->reg, !idle);
						} else if (node.self.index == TransformFloatMulAdd || node.self.index == TransformFloatMulSubt) {
							if (node.inputs.Length() != 3) throw InvalidArgumentException();
							if (object_size(node.retval_spec.size) != ins) throw InvalidArgumentException();
							if (object_size(node.input_specs[1].size) != ins) throw InvalidArgumentException();
							if (object_size(node.input_specs[2].size) != ins) throw InvalidArgumentException();
							InternalDisposition a2, a3;
							a2.size = a3.size = ins;
							a2.flags = a3.flags = DispositionPointer;
							a2.reg = _fph_allocate(reg_in_use, rv);
							a3.reg = _fph_allocate(reg_in_use, rv | a2.reg);
							encode_preserve(a2.reg, reg_in_use, disp->reg, !idle);
							encode_preserve(a3.reg, reg_in_use, disp->reg, !idle);
							_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | rv | a2.reg);
							_encode_tree_node(node.inputs[2], idle, mem_load, &a3, reg_in_use | rv | a2.reg | a3.reg);
							if (!idle) for (int i = 0; i < dim; i++) {
								encode_fld(quant, rv, i * quant);
								encode_fmul(quant, a2.reg, i * quant);
								if (node.self.index == TransformFloatMulAdd) encode_fadd(quant, a3.reg, i * quant);
								else if (node.self.index == TransformFloatMulSubt) encode_fsub(quant, a3.reg, i * quant);
								encode_fstp(quant, store_reg, store_offs + i * quant);
							}
							encode_restore(a3.reg, reg_in_use, disp->reg, !idle);
							encode_restore(a2.reg, reg_in_use, disp->reg, !idle);
						}
						if (!(a1.flags & DispositionReuse) && !idle) encode_lea(rv, store_reg, store_offs);
					} else throw InvalidArgumentException();
					if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					} else if (disp->flags & DispositionPointer) {
						disp->flags = DispositionPointer;
						if (sysdisp == DispositionPointer) {
							disp->flags |= DispositionReuse;
							if (!idle && rv != disp->reg) encode_mov_reg_reg(4, disp->reg, rv);
						} else if (sysdisp == DispositionRegister) {
							*mem_load += word_align(node.retval_spec.size);
							if (!idle) {
								int offs = allocate_temporary(node.retval_spec.size, &offs);
								encode_mov_mem_reg(4, Reg32::EBP, offs, rv);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
						}
					} else if (disp->flags & DispositionRegister) {
						disp->flags = DispositionRegister;
						if (sysdisp == DispositionPointer) {
							if (!idle) encode_reg_load_32(disp->reg, rv, 0, disp->size, disp->flags & DispositionCompress, reg_in_use);
						} else if (sysdisp == DispositionRegister) {
							if (!idle && rv != disp->reg) encode_mov_reg_reg(4, disp->reg, rv);
						}
					} else throw InvalidArgumentException();
					encode_restore(rv, reg_in_use, disp->reg, !idle);
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
						if (staging == Reg32::NO) staging = Reg32::EAX;
						InternalDisposition c1, c2;
						c1.reg = staging != Reg32::EDI ? Reg32::EDI : Reg32::EBX;
						c2.reg = staging != Reg32::ESI ? Reg32::ESI : Reg32::EBX;
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
								if (remaining >= 4) quant = 4;
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
								if (out_size == 8) {
									encode_mov_mem_reg(4, Reg32::EBP, offs, disp->reg);
									encode_operation(4, arOp::XOR, disp->reg, disp->reg);
									encode_mov_mem_reg(4, Reg32::EBP, offs + 4, disp->reg);
								} else encode_mov_mem_reg(out_size, Reg32::EBP, offs, disp->reg);
								encode_lea(disp->reg, Reg32::EBP, offs);
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
						if (staging == Reg32::NO) staging = Reg32::EAX;
						InternalDisposition c1, c2;
						c1.reg = staging != Reg32::EDI ? Reg32::EDI : Reg32::EBX;
						c2.reg = staging != Reg32::ESI ? Reg32::ESI : Reg32::EBX;
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
								if (remaining >= 4) quant = 4;
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
								if (out_size == 8) {
									encode_mov_mem_reg(4, Reg32::EBP, offs, disp->reg);
									encode_operation(4, arOp::XOR, disp->reg, disp->reg);
									encode_mov_mem_reg(4, Reg32::EBP, offs + 4, disp->reg);
								} else if (out_size) encode_mov_mem_reg(out_size, Reg32::EBP, offs, disp->reg);
								encode_lea(disp->reg, Reg32::EBP, offs);
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
						if ((in_size & 3) == 0) quant = 4;
						else if ((in_size & 1) == 0) quant = 2;
						else quant = 1;
						uint num_quants = in_size / quant;
						InternalDisposition ld;
						ld.reg = Reg32::EDI;
						ld.flags = DispositionPointer;
						ld.size = in_size;
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
						if (shift && !idle) {
							uint shift_quants = shift / (quant * 8);
							uint shift_bits = shift % (quant * 8);
							if (shift_bits == 0) {
								encode_preserve(Reg32::EAX, reg_in_use, 0, true);
								uint quant_rem = shift_quants > num_quants ? 0 : num_quants - shift_quants;
								uint quant_del = num_quants - quant_rem;
								if (node.self.index == TransformLongIntShiftL) {
									for (int i = num_quants - 1; i >= quant_del; i--) {
										encode_mov_reg_mem(quant, Reg32::EAX, ld.reg, quant * (i - quant_del));
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
									}
									encode_operation(quant, arOp::XOR, Reg32::EAX, Reg32::EAX);
									for (int i = quant_del - 1; i >= 0; i--) {
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
									}
								} else if (node.self.index == TransformLongIntShiftR) {
									for (int i = 0; i < quant_rem; i++) {
										encode_mov_reg_mem(quant, Reg32::EAX, ld.reg, quant * (i + quant_del));
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
									}
									encode_operation(quant, arOp::XOR, Reg32::EAX, Reg32::EAX);
									for (int i = quant_rem; i < num_quants; i++) {
										encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
									}
								}
								encode_restore(Reg32::EAX, reg_in_use, 0, true);
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
								encode_preserve(Reg32::EAX, reg_in_use, 0, true);
								encode_preserve(Reg32::ECX, reg_in_use, 0, true);
								int quant_rem = shift_quants > num_quants ? 0 : num_quants - shift_quants;
								int quant_del = num_quants - quant_rem;
								if (node.self.index == TransformLongIntShiftL) {
									for (int i = num_quants - 1; i >= quant_del; i--) {
										if (i > quant_del) {
											encode_mov_reg_mem(quant, Reg32::EAX, ld.reg, quant * (i - quant_del));
											encode_mov_reg_mem(quant, Reg32::ECX, ld.reg, quant * (i - quant_del - 1));
											encode_shift(quant, shOp::SHL, Reg32::EAX, shift_bits);
											encode_shift(quant, shOp::SHR, Reg32::ECX, 8 * quant - shift_bits);
											encode_operation(quant, arOp::OR, Reg32::EAX, Reg32::ECX);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
										} else {
											encode_mov_reg_mem(quant, Reg32::EAX, ld.reg, quant * (i - quant_del));
											encode_shift(quant, shOp::SHL, Reg32::EAX, shift_bits);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
										}
									}
									if (quant_del) {
										encode_operation(quant, arOp::XOR, Reg32::EAX, Reg32::EAX);
										for (int i = quant_del - 1; i >= 0; i--) {
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
										}
									}
								} else if (node.self.index == TransformLongIntShiftR) {
									for (int i = 0; i < quant_rem; i++) {
										if (i + 1 < quant_rem) {
											encode_mov_reg_mem(quant, Reg32::EAX, ld.reg, quant * (i + quant_del));
											encode_mov_reg_mem(quant, Reg32::ECX, ld.reg, quant * (i + quant_del + 1));
											encode_shift(quant, shOp::SHR, Reg32::EAX, shift_bits);
											encode_shift(quant, shOp::SHL, Reg32::ECX, 8 * quant - shift_bits);
											encode_operation(quant, arOp::OR, Reg32::EAX, Reg32::ECX);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
										} else {
											encode_mov_reg_mem(quant, Reg32::EAX, ld.reg, quant * (i + quant_del));
											encode_shift(quant, shOp::SHR, Reg32::EAX, shift_bits);
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
										}
									}
									if (quant_del) {
										encode_operation(quant, arOp::XOR, Reg32::EAX, Reg32::EAX);
										for (int i = quant_rem; i < num_quants; i++) {
											encode_mov_mem_reg(quant, ld.reg, quant * i, Reg32::EAX);
										}
									}
								}
								encode_restore(Reg32::ECX, reg_in_use, 0, true);
								encode_restore(Reg32::EAX, reg_in_use, 0, true);
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
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
						}
					} else if (node.self.index == TransformLongIntMul || node.self.index == TransformLongIntDivMod || node.self.index == TransformLongIntMod) {
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						auto io_size = object_size(node.input_specs[0].size);
						auto quant = object_size(node.input_specs[1].size);
						auto out_size = object_size(node.retval_spec.size);
						if (io_size == 0 || io_size % quant) throw InvalidArgumentException();
						if (quant != 1 && quant != 2 && quant != 4) throw InvalidArgumentException();
						if (out_size != 0 && out_size != quant) throw InvalidArgumentException();
						auto num_words = io_size / quant;
						InternalDisposition ld, m;
						ld.reg = Reg32::EDI;
						ld.size = io_size;
						ld.flags = DispositionPointer;
						m.reg = Reg32::ECX;
						m.size = quant;
						m.flags = DispositionRegister;
						Reg staging_low = Reg32::EAX;
						Reg staging_high = Reg32::EDX;
						Reg carry = Reg32::EBX;
						Reg zero = Reg32::ESI;
						Reg result = Reg32::NO;
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
										encode_mov_reg_reg(1, Reg32::BL, Reg32::AH);
										encode_mov_mem_reg(1, ld.reg, i, Reg32::AL);
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
								if (quant == 1) result = Reg32::AH;
								else result = staging_high;
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
								if (out_size) encode_mov_mem_reg(out_size, Reg32::EBP, offs, result);
								encode_lea(disp->reg, Reg32::EBP, offs);
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
						X86::Reg accumulator = Reg32::EAX;
						InternalDisposition dld, sld;
						dld.reg = Reg32::EDI;
						sld.reg = Reg32::ESI;
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
							if (pos_1 && zero_blt) {
								encode_operation(WordSize, arOp::XOR, accumulator, accumulator);
								uint pos = 0;
								while (pos < pos_1) {
									uint rem = pos_1 - pos;
									uint quant;
									if ((rem & 3) == 0) quant = 4;
									else if ((rem & 1) == 0) quant = 2;
									else quant = 1;
									encode_mov_mem_reg(quant, dld.reg, pos, accumulator);
									pos += quant;
								}
							}
							if (pos_2 > pos_1) {
								uint pos = pos_1;
								while (pos < pos_2) {
									uint rem = pos_2 - pos;
									uint pos_s = pos - dest_offset + src_offset;
									uint quant;
									if ((rem & 3) == 0 && (pos & 3) == 0) quant = 4;
									else if ((rem & 1) == 0 && (pos & 1) == 0) quant = 2;
									else quant = 1;
									encode_mov_reg_mem(quant, accumulator, sld.reg, pos_s);
									encode_mov_mem_reg(quant, dld.reg, pos, accumulator);
									pos += quant;
								}
							}
							if (dest_size > pos_2 && zero_blt) {
								if (pos_2 > pos_1 || !pos_1) encode_operation(WordSize, arOp::XOR, accumulator, accumulator);
								uint pos = pos_2;
								while (pos < dest_size) {
									uint rem = dest_size - pos;
									uint quant;
									if ((rem & 3) == 0 && (pos & 3) == 0) quant = 4;
									else if ((rem & 1) == 0 && (pos & 1) == 0) quant = 2;
									else quant = 1;
									encode_mov_mem_reg(quant, dld.reg, pos, accumulator);
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
								encode_lea(disp->reg, Reg32::EBP, offs);
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
						Reg result = Reg32::EAX;
						InternalDisposition ld, sel;
						ld.reg = Reg32::EDI;
						sel.reg = Reg32::ECX;
						ld.flags = DispositionPointer;
						ld.size = io_size;
						if (selector_size == 8) {
							sel.flags = DispositionPointer;
							sel.size = 8;
						} else {
							sel.flags = DispositionRegister;
							sel.size = selector_size;
						}
						encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | ld.reg);
						encode_preserve(sel.reg, reg_in_use, disp->reg, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &sel, reg_in_use | ld.reg | sel.reg);
						encode_preserve(result, reg_in_use, disp->reg, !idle);
						if (!idle) {
							if (selector_size == 8) {
								encode_mov_reg_mem(4, result, sel.reg, 4);
								encode_shl(result, 29);
								encode_operation(4, arOp::ADD, ld.reg, result);
								encode_mov_reg_mem(4, sel.reg, sel.reg);
								encode_mov_reg_reg(4, result, sel.reg);
							} else if (selector_size == 4) {
								encode_mov_reg_reg(4, result, sel.reg);
							} else {
								encode_mov_zx(4, result, selector_size, sel.reg);
							}
							encode_shr(result, 3);
							encode_operation(4, arOp::ADD, ld.reg, result);
							encode_mov_reg_mem(1, result, ld.reg);
							encode_and(sel.reg, 0x7);
							encode_shift(1, shOp::SHR, result);
							encode_and(result, 1);
						}
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
							if (!idle && result != disp->reg) encode_mov_reg_reg(4, disp->reg, result);
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(out_size, 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(out_size, 0));
								if (out_size == 8) {
									encode_mov_mem_reg(4, Reg32::EBP, offs, result);
									encode_operation(4, arOp::XOR, result, result);
									encode_mov_mem_reg(4, Reg32::EBP, offs + 4, result);
								} else encode_mov_mem_reg(out_size, Reg32::EBP, offs, result);
								encode_lea(disp->reg, Reg32::EBP, offs);
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
						Reg acc = Reg32::EAX;
						InternalDisposition ld, sel, bit;
						ld.reg = Reg32::EDI;
						sel.reg = Reg32::ECX;
						bit.reg = Reg32::EDX;
						ld.flags = DispositionPointer;
						ld.size = io_size;
						if (selector_size == 8) {
							sel.flags = DispositionPointer;
							sel.size = 8;
						} else {
							sel.flags = DispositionRegister;
							sel.size = selector_size;
						}
						if (bit_size == 8) {
							bit.flags = DispositionPointer;
							bit.size = 8;
						} else {
							bit.flags = DispositionRegister;
							bit.size = bit_size;
						}
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | ld.reg);
						encode_preserve(sel.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &sel, reg_in_use | ld.reg | sel.reg);
						encode_preserve(bit.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[2], idle, mem_load, &bit, reg_in_use | ld.reg | sel.reg | bit.reg);
						encode_preserve(acc, reg_in_use, 0, !idle);
						if (!idle) {
							if (selector_size == 8) {
								encode_mov_reg_mem(4, acc, sel.reg, 4);
								encode_shl(acc, 29);
								encode_operation(4, arOp::ADD, ld.reg, acc);
								encode_mov_reg_mem(4, sel.reg, sel.reg);
								encode_mov_reg_reg(4, acc, sel.reg);
							} else if (selector_size == 4) {
								encode_mov_reg_reg(4, acc, sel.reg);
							} else {
								encode_mov_zx(4, acc, selector_size, sel.reg);
							}
							encode_shr(acc, 3);
							encode_operation(4, arOp::ADD, ld.reg, acc);
							encode_mov_reg_mem(1, acc, ld.reg);
							encode_and(sel.reg, 0x7);
							encode_shift(1, shOp::ROR, acc);
							if (bit_size == 8) encode_mov_reg_mem(4, bit.reg, bit.reg);
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
								encode_lea(disp->reg, Reg32::EBP, offs);
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
							encode_lea(disp->reg, Reg32::EBP, offs);
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
						Reg result = Reg32::EAX;
						encode_preserve(Reg32::EAX, reg_in_use, disp->reg, !idle);
						encode_preserve(Reg32::ECX, reg_in_use, disp->reg, !idle);
						encode_preserve(Reg32::EDX, reg_in_use, disp->reg, !idle);
						encode_preserve(Reg32::EBX, reg_in_use, disp->reg, !idle);
						// OPERATION
						if (!idle) {
							Reg test = Reg32::NO;
							uint eax_code = 0, ecx_code = 0xFFFFFFFF;
							uint bit = 0;
							if (opcode == TransformCryptFeature) {
								encode_mov_reg_const(4, result, 1);
							} else if (opcode == TransformCryptRandom) {
								eax_code = 0x01; // RDRAND
								bit = 30;
								test = Reg32::ECX;
							} else if (opcode == TransformCryptAesEncECB || opcode == TransformCryptAesDecECB || opcode == TransformCryptAesEncCBC || opcode == TransformCryptAesDecCBC) {
								eax_code = 0x01; // AES-NI
								bit = 25;
								test = Reg32::ECX;
							} else if (opcode == TransformCryptSha224I || opcode == TransformCryptSha256I || opcode == TransformCryptSha384I || opcode == TransformCryptSha512I) {
								eax_code = 0x01; // SSE3
								bit = 0;
								test = Reg32::ECX;
							} else if (opcode == TransformCryptSha224F || opcode == TransformCryptSha256F || opcode == TransformCryptSha384F || opcode == TransformCryptSha512F) {
								eax_code = 0x01; // SSE3
								bit = 0;
								test = Reg32::ECX;
							} else if (opcode == TransformCryptSha224S || opcode == TransformCryptSha256S) {
								if (flags & ReferenceFlagLong) {
									eax_code = 0x01; // SSE3
									bit = 0;
									test = Reg32::ECX;
								} else {
									eax_code = 0x07; // SHA-NI
									ecx_code = 0x00;
									bit = 29;
									test = Reg32::EBX;
								}
							} else if (opcode == TransformCryptSha384S || opcode == TransformCryptSha512S) {
								if (flags & ReferenceFlagLong) {
									eax_code = 0x01; // SSE3
									bit = 0;
									test = Reg32::ECX;
								} else {
									encode_mov_reg_const(4, result, 0); // NOT IMPLEMENTED
									// eax_code = 0x07; // SHA512
									// ecx_code = 0x01;
									// bit = 0;
									// test = Reg32::EAX;
								}
							} else {
								encode_mov_reg_const(4, result, 0);
							}
							if (test) {
								encode_mov_reg_const(4, Reg32::EAX, eax_code);
								if (ecx_code != 0xFFFFFFFF) encode_mov_reg_const(4, Reg32::ECX, ecx_code);
								encode_cpuid();
								encode_test(4, test, 1U << bit);
								_dest.code << 0x74 << 0x00; // JZ
								int jz = _dest.code.Length();
								encode_mov_reg_const(4, result, 1);
								_dest.code << 0xEB << 0x00; // JMP
								int jmp = _dest.code.Length();
								_dest.code[jz - 1] = _dest.code.Length() - jz;
								encode_mov_reg_const(4, result, 0);
								_dest.code[jmp - 1] = _dest.code.Length() - jmp;
							}
						}
						// FINALIZATION
						if (disp->flags & DispositionRegister) {
							disp->flags = DispositionRegister;
							if (!idle && result != disp->reg) encode_mov_reg_reg(4, disp->reg, result);
						} else if (disp->flags & DispositionPointer) {
							disp->flags = DispositionPointer;
							(*mem_load) += _word_align(TH::MakeSize(retval_size, 0));
							if (!idle) {
								int offs = allocate_temporary(TH::MakeSize(retval_size, 0));
								if (retval_size == 8) {
									encode_mov_mem_reg(4, Reg32::EBP, offs, result);
									encode_operation(4, arOp::XOR, result, result);
									encode_mov_mem_reg(4, Reg32::EBP, offs + 4, result);
								} else encode_mov_mem_reg(retval_size, Reg32::EBP, offs, result);
								encode_lea(disp->reg, Reg32::EBP, offs);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						encode_restore(Reg32::EBX, reg_in_use, disp->reg, !idle);
						encode_restore(Reg32::EDX, reg_in_use, disp->reg, !idle);
						encode_restore(Reg32::ECX, reg_in_use, disp->reg, !idle);
						encode_restore(Reg32::EAX, reg_in_use, disp->reg, !idle);
					} else if (node.self.index == TransformCryptRandom) {
						// ARGUMENT VALIDATION
						if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
						if (object_size(node.input_specs[0].size) != WordSize || object_size(node.retval_spec.size)) throw InvalidArgumentException();
						auto length_size = object_size(node.input_specs[1].size);
						if (length_size != 1 && length_size != 2 && length_size != 4 && length_size != 8) throw InvalidArgumentException();
						// ARGUMENT EVALUATION
						Reg acc = Reg32::EAX;
						InternalDisposition ld, len;
						ld.reg = Reg32::EDI;
						len.reg = Reg32::ECX;
						ld.flags = DispositionRegister;
						ld.size = WordSize;
						if (length_size == 8) {
							len.flags = DispositionPointer;
							len.size = 8;
						} else {
							len.flags = DispositionRegister;
							len.size = length_size;
						}
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | ld.reg);
						encode_preserve(len.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &len, reg_in_use | ld.reg | len.reg);
						encode_preserve(acc, reg_in_use, 0, !idle);
						// OPERATION
						if (!idle) {
							if (length_size == 8) encode_mov_reg_mem(4, len.reg, len.reg);
							else if (length_size != 4) encode_mov_zx(4, len.reg, length_size, len.reg);
							int test = _dest.code.Length();
							encode_test(4, len.reg, int(0xFFFFFFFC));
							_dest.code << 0x74 << 0x00; // JZ
							int jz = _dest.code.Length();
							encode_read_random(4, acc);
							_dest.code << 0x73 << 0x00; // JNC
							_dest.code[_dest.code.Length() - 1] = jz - _dest.code.Length();
							encode_mov_mem_reg(4, ld.reg, acc);
							encode_add(ld.reg, 4);
							encode_add(len.reg, -4);
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
						Reg staging_1 = Reg64::XMM0;
						Reg staging_2 = Reg64::XMM1;
						Reg staging_3 = Reg64::XMM2;
						Reg round_key_buffer_reg = Reg32::EAX;
						InternalDisposition state, ld, len, key;
						state.reg = Reg32::ESI;
						ld.reg = Reg32::EDI;
						len.reg = Reg32::ECX;
						key.reg = Reg32::EBX;
						state.flags = key.flags = DispositionPointer;
						ld.flags = DispositionRegister;
						state.size = 16;
						ld.size = WordSize;
						key.size = key_size;
						if (length_size == 8) {
							len.flags = DispositionPointer;
							len.size = 8;
						} else {
							len.flags = DispositionRegister;
							len.size = length_size;
						}
						encode_preserve(state.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &state, reg_in_use | state.reg);
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | state.reg | ld.reg);
						encode_preserve(len.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[2], idle, mem_load, &len, reg_in_use | state.reg | ld.reg | len.reg);
						encode_preserve(key.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[3], idle, mem_load, &key, reg_in_use | state.reg | ld.reg | len.reg | key.reg);
						encode_preserve(round_key_buffer_reg, reg_in_use, 0, !idle);
						// OPERATION
						uint round_key_buffer_size = 17 * (rounds + 1);
						*mem_load += round_key_buffer_size;
						if (!idle) {
							int round_key_buffer_offset = allocate_temporary(TH::MakeSize(round_key_buffer_size, 0));
							encode_lea(round_key_buffer_reg, Reg32::EBP, round_key_buffer_offset);
							encode_add(round_key_buffer_reg, 0x0000000F);
							encode_and(round_key_buffer_reg, 0xFFFFFFF0);
							encode_mov_xmm_mem(16, staging_1, key.reg, 0);
							encode_mov_mem_xmm(16, round_key_buffer_reg, 0, staging_1);
							if (key_size > 16) {
								if (key_size == 24) encode_mov_xmm_mem(8, staging_1, key.reg, 16);
								else if (key_size == 32) encode_mov_xmm_mem(16, staging_1, key.reg, 16);
								encode_mov_mem_xmm(16, round_key_buffer_reg, 16, staging_1);
							}
							uint base_round_words = key_size / 4;
							uint needs_round_words = (rounds + 1) * 4;
							for (uint w = base_round_words; w < needs_round_words; w += 2) {
								int dest_offset = w * 4;
								int prev1_offset = dest_offset - 4;
								int prev2_offset = dest_offset - base_round_words * 4;
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
									if (w % base_round_words == 0) w1 = 1; else w1 = 0;
									encode_mov_xmm_mem(8, staging_1, round_key_buffer_reg, prev1_offset - 4);
									encode_aes_keygen(staging_1, staging_1, rc);
									if (w1) encode_simd_shuffle(4, staging_1, staging_1, w1, w1, w1, w1);
								} else encode_mov_xmm_mem(4, staging_1, round_key_buffer_reg, prev1_offset);
								encode_mov_xmm_mem(4, staging_2, round_key_buffer_reg, prev2_offset);
								encode_simd_xor(staging_1, staging_2);
								encode_mov_xmm_mem(4, staging_2, round_key_buffer_reg, prev2_offset + 4);
								encode_simd_xor(staging_2, staging_1);
								encode_simd_shuffle(4, staging_1, staging_2, 0, 0, 0, 0);
								encode_simd_shuffle(4, staging_1, staging_1, 0, 2, 0, 2);
								encode_mov_mem_xmm(8, round_key_buffer_reg, dest_offset, staging_1);
							}
							if (decrypt) for (int i = 1; i < rounds; i++) {
								encode_aes_inversed_mix_columns(staging_1, round_key_buffer_reg, 16 * i);
								encode_mov_mem_xmm(16, round_key_buffer_reg, 16 * i, staging_1);
							}
							if (chain_cbc) encode_mov_xmm_mem(16, staging_2, state.reg, 0);
							if (length_size == 8) encode_mov_reg_mem(4, len.reg, len.reg);
							else if (length_size != 4) encode_mov_zx(4, len.reg, length_size, len.reg);
							int test = _dest.code.Length();
							encode_test(4, len.reg, int(0xFFFFFFF0));
							_dest.code << 0x0F << 0x84 << 0x00 << 0x00 << 0x00 << 0x00; // JZ
							int jz = _dest.code.Length();
							if (decrypt) {
								encode_mov_xmm_mem(16, staging_1, ld.reg, 0);
								if (chain_cbc) encode_mov_xmm_xmm(staging_3, staging_1);
								encode_simd_xor(staging_1, round_key_buffer_reg, 16 * rounds);
								for (int i = rounds - 1; i > 0; i--) encode_aes_dec(staging_1, round_key_buffer_reg, 16 * i);
								encode_aes_dec_last(staging_1, round_key_buffer_reg, 0);
								if (chain_cbc) {
									encode_simd_xor(staging_1, staging_2);
									encode_mov_xmm_xmm(staging_2, staging_3);
								}
								encode_mov_mem_xmm(16, ld.reg, 0, staging_1);
							} else {
								encode_mov_xmm_mem(16, staging_1, ld.reg, 0);
								if (chain_cbc) encode_simd_xor(staging_1, staging_2);
								encode_simd_xor(staging_1, round_key_buffer_reg, 0);
								for (int i = 1; i < rounds; i++) encode_aes_enc(staging_1, round_key_buffer_reg, 16 * i);
								encode_aes_enc_last(staging_1, round_key_buffer_reg, 16 * rounds);
								encode_mov_mem_xmm(16, ld.reg, 0, staging_1);
								if (chain_cbc) encode_mov_xmm_xmm(staging_2, staging_1);
							}
							encode_add(ld.reg, 16);
							encode_add(len.reg, -16);
							_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + _dest.code.Length() - 4) = test - _dest.code.Length();
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + jz - 4) = _dest.code.Length() - jz;
							if (chain_cbc) encode_mov_mem_xmm(16, state.reg, 0, staging_2);
						}
						// FINALIZATION
						encode_restore(round_key_buffer_reg, reg_in_use, 0, !idle);
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
						Reg src_ptr = Reg32::ESI;
						InternalDisposition dest;
						dest.reg = Reg32::EDI;
						dest.flags = DispositionPointer;
						dest.size = state_size;
						encode_preserve(dest.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &dest, reg_in_use | dest.reg);
						encode_preserve(src_ptr, reg_in_use, 0, !idle);
						// OPERATION
						if (!idle) {
							encode_put_addr_of(src_ptr, TH::MakeRef(ReferenceData, data_index));
							if (state_size == 32) {
								Reg staging = Reg32::EAX;
								encode_preserve(staging, reg_in_use, 0, true); // Intel's canonical order: febahgdc
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
								encode_restore(staging, reg_in_use, 0, true);
							} else {
								Reg staging = Reg64::XMM0;
								encode_mov_xmm_mem(8, staging, src_ptr, 40);
								encode_mov_mem_xmm(8, dest.reg, 0, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 32);
								encode_mov_mem_xmm(8, dest.reg, 8, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 8);
								encode_mov_mem_xmm(8, dest.reg, 16, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 0);
								encode_mov_mem_xmm(8, dest.reg, 24, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 56);
								encode_mov_mem_xmm(8, dest.reg, 32, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 48);
								encode_mov_mem_xmm(8, dest.reg, 40, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 24);
								encode_mov_mem_xmm(8, dest.reg, 48, staging);
								encode_mov_xmm_mem(8, staging, src_ptr, 16);
								encode_mov_mem_xmm(8, dest.reg, 56, staging);
							}
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
						state.reg = Reg32::ESI;
						ld.reg = Reg32::EDI;
						len.reg = Reg32::ECX;
						state.flags = DispositionPointer;
						ld.flags = DispositionRegister;
						state.size = state_size;
						ld.size = WordSize;
						if (length_size == 8) {
							len.flags = DispositionPointer;
							len.size = 8;
						} else {
							len.flags = DispositionRegister;
							len.size = length_size;
						}
						encode_preserve(state.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &state, reg_in_use | state.reg);
						encode_preserve(ld.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | state.reg | ld.reg);
						encode_preserve(len.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[2], idle, mem_load, &len, reg_in_use | state.reg | ld.reg | len.reg);
						uint swap_space_size = 0;
						if (software) {
							if (sha512) swap_space_size = 16 * 8 + 16 + 16; else swap_space_size = 16 * 4 + 16;
							*mem_load += swap_space_size;
						}
						// OPERATION
						if (!idle) {
							Reg constant_pointer = Reg32::EBX;
							Reg swap_space_pointer = Reg32::EDX;
							encode_preserve(constant_pointer, reg_in_use, 0, true);
							encode_preserve(swap_space_pointer, reg_in_use, 0, software);
							if (software) {
								auto swap_space_offset = allocate_temporary(TH::MakeSize(swap_space_size, 0));
								encode_lea(swap_space_pointer, Reg32::EBP, swap_space_offset);
								encode_add(swap_space_pointer, 0x0000000F);
								encode_and(swap_space_pointer, 0xFFFFFFF0);
							}
							encode_put_addr_of(constant_pointer, TH::MakeRef(ReferenceData, constant_index));
							if (length_size == 8) encode_mov_reg_mem(4, len.reg, len.reg);
							else if (length_size != 4) encode_mov_zx(4, len.reg, length_size, len.reg);
							int test = _dest.code.Length();
							if (sha512) encode_test(4, len.reg, int(0xFFFFFF80));
							else encode_test(4, len.reg, int(0xFFFFFFC0));
							_dest.code << 0x0F << 0x84 << 0x00 << 0x00 << 0x00 << 0x00; // JZ
							int jz = _dest.code.Length();
							if (sha512) {
								if (software) {
									// Loading the state into fe:ba:hg:dc
									Reg staging_0 = Reg64::XMM0, staging_1 = Reg64::XMM1, staging_2 = Reg64::XMM2, staging_3 = Reg64::XMM3;
									Reg fe = Reg64::XMM4, ba = Reg64::XMM5, hg = Reg64::XMM6, dc = Reg64::XMM7;
									encode_mov_xmm_mem(16, fe, state.reg, 0);
									encode_mov_xmm_mem(16, ba, state.reg, 16);
									encode_mov_xmm_mem(16, hg, state.reg, 32);
									encode_mov_xmm_mem(16, dc, state.reg, 48);
									// Loading the block into [swap_space_pointer] === words; changing the endianess
									for (int i = 0; i < 8; i++) {
										encode_mov_xmm_mem(16, staging_1, ld.reg, 16 * i);
										encode_simd_shuffle_dwords(staging_1, staging_1, 1, 0, 3, 2);
										encode_mov_xmm_xmm(staging_0, staging_1);
										encode_simd_shift_left(4, staging_1, 16);
										encode_simd_shift_right(4, staging_0, 16);
										encode_simd_or(staging_1, staging_0);
										encode_mov_xmm_xmm(staging_0, staging_1);
										encode_simd_shift_left(2, staging_1, 8);
										encode_simd_shift_right(2, staging_0, 8);
										encode_simd_or(staging_1, staging_0);
										encode_mov_mem_xmm(16, swap_space_pointer, i * 16, staging_1);
									}
									// Running the rounds
									for (int r = 0; r < 80; r += 2) {
										int wri = r / 2, w0o;
										if (r >= 16) {
											w0o = (wri % 8) * 16;
											auto w1o = ((wri + 1) % 8) * 16;
											auto w4o = ((wri + 4) % 8) * 16;
											auto w5o = ((wri + 5) % 8) * 16;
											auto w7o = ((wri + 7) % 8) * 16;
											encode_mov_mem_xmm(16, swap_space_pointer, 16 * 8, fe); // Unloading fe into the swap to use as the w[r-16]/w[r] register
											encode_mov_xmm_mem(16, fe, swap_space_pointer, w0o);
											encode_mov_xmm_mem(16, staging_0, swap_space_pointer, w1o);
											encode_simd_extract(staging_0, fe, 8); // staging_0 = w[r-15]
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
											encode_simd_int_add(8, fe, staging_1); // fe = S0(w[r-15]) + w[r-16]
											encode_mov_xmm_mem(16, staging_0, swap_space_pointer, w5o);
											encode_simd_extract(staging_0, swap_space_pointer, w4o, 8); // staging_0 = w[r-7]
											encode_simd_int_add(8, fe, staging_0); // fe = S0(w[r-15]) + w[r-16] + w[r-7]
											encode_mov_xmm_mem(16, staging_1, swap_space_pointer, w7o);
											encode_mov_xmm_xmm(staging_2, staging_1);
											encode_mov_xmm_xmm(staging_3, staging_1);
											encode_simd_shift_right(8, staging_1, 19);
											encode_simd_shift_left(8, staging_2, 45);
											encode_simd_shift_right(8, staging_3, 6);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3);
											encode_mov_xmm_mem(16, staging_2, swap_space_pointer, w7o);
											encode_mov_xmm_xmm(staging_3, staging_2);
											encode_simd_shift_right(8, staging_2, 61);
											encode_simd_shift_left(8, staging_3, 3);
											encode_simd_xor(staging_1, staging_2);
											encode_simd_xor(staging_1, staging_3); // staging_1 = S1(w[r-2])
											encode_simd_int_add(8, fe, staging_1); // fe = S0(w[r-15]) + w[r-16] + w[r-7] + S1(w[r-2])
											encode_mov_mem_xmm(16, swap_space_pointer, w0o, fe);
											encode_mov_xmm_mem(16, fe, swap_space_pointer, 16 * 8);
										} else w0o = wri * 16;
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
											encode_mov_xmm_mem(16, staging_2, swap_space_pointer, w0o);
											if (i) encode_simd_shuffle_dwords(staging_2, staging_2, 2, 3, 2, 3);
											else encode_simd_shuffle_dwords(staging_2, staging_2, 0, 1, 0, 1); // staging_2[1] = w[i]
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
									encode_mov_xmm_mem(16, staging_0, state.reg, 0);
									encode_mov_xmm_mem(16, staging_1, state.reg, 16);
									encode_mov_xmm_mem(16, staging_2, state.reg, 32);
									encode_mov_xmm_mem(16, staging_3, state.reg, 48);
									encode_simd_int_add(8, fe, staging_0);
									encode_simd_int_add(8, ba, staging_1);
									encode_simd_int_add(8, hg, staging_2);
									encode_simd_int_add(8, dc, staging_3);
									encode_mov_mem_xmm(16, state.reg, 0, fe);
									encode_mov_mem_xmm(16, state.reg, 16, ba);
									encode_mov_mem_xmm(16, state.reg, 32, hg);
									encode_mov_mem_xmm(16, state.reg, 48, dc);
								} else {
									encode_debugger_trap(); // Not implemented by now
								}
							} else {
								if (software) {
									// Loading the state into feba:hgdc
									Reg staging_0 = Reg64::XMM0;
									Reg staging_sw_1 = Reg64::XMM3, staging_sw_2 = Reg64::XMM4, staging_sw_3 = Reg64::XMM5, staging_sw_4 = Reg64::XMM6, staging_sw_5 = Reg64::XMM7;
									Reg feba = Reg64::XMM1, hgdc = Reg64::XMM2;
									encode_mov_xmm_mem(16, feba, state.reg, 0);
									encode_mov_xmm_mem(16, hgdc, state.reg, 16);
									// Loading the block into [swap_space_pointer] === words; changing the endianess
									for (int i = 0; i < 4; i++) {
										encode_mov_xmm_mem(16, staging_sw_1, ld.reg, 16 * i);
										encode_mov_xmm_xmm(staging_0, staging_sw_1);
										encode_simd_shift_left(4, staging_sw_1, 16);
										encode_simd_shift_right(4, staging_0, 16);
										encode_simd_or(staging_sw_1, staging_0);
										encode_mov_xmm_xmm(staging_0, staging_sw_1);
										encode_simd_shift_left(2, staging_sw_1, 8);
										encode_simd_shift_right(2, staging_0, 8);
										encode_simd_or(staging_sw_1, staging_0);
										encode_mov_mem_xmm(16, swap_space_pointer, 16 * i, staging_sw_1);
									}
									// Running the rounds
									for (int r = 0; r < 64; r += 4) {
										int wri = r / 4, w0o;
										if (r >= 16) {
											w0o = (wri % 4) * 16;
											auto w1o = ((wri + 1) % 4) * 16;
											auto w2o = ((wri + 2) % 4) * 16;
											auto w3o = ((wri + 3) % 4) * 16;
											encode_mov_xmm_mem(16, staging_sw_5, swap_space_pointer, w0o);
											encode_mov_xmm_mem(16, staging_0, swap_space_pointer, w1o);
											encode_simd_extract(staging_0, staging_sw_5, 4);
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
											encode_simd_int_add(4, staging_sw_5, staging_0);
											// staging_sw_5 = S0(w[r-15]) + w[r-16]
											encode_mov_xmm_mem(16, staging_0, swap_space_pointer, w3o);
											encode_simd_extract(staging_0, swap_space_pointer, w2o, 4);
											// staging_0 = w[r-7]
											encode_simd_int_add(4, staging_sw_5, staging_0);
											// staging_sw_5 = S0(w[r-15]) + w[r-16] + w[r-7]
											encode_mov_xmm_mem(16, staging_0, swap_space_pointer, w3o);
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
											encode_simd_int_add(4, staging_sw_5, staging_0);
											encode_mov_xmm_xmm(staging_0, staging_sw_5);
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
											encode_simd_int_add(4, staging_sw_5, staging_0);
											// staging_sw_5 = S0(w[r-15]) + w[r-16] + w[r-7] + S1(w[r-2])
											encode_mov_mem_xmm(16, swap_space_pointer, w0o, staging_sw_5);
										} else w0o = wri * 16;
										encode_mov_xmm_mem(16, staging_0, constant_pointer, 4 * r);
										encode_simd_int_add(4, staging_0, swap_space_pointer, w0o);
										// staging_0 = w[r..r+3] + k[r..r+3]
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
									}
									encode_mov_xmm_mem(16, staging_sw_1, state.reg, 0);
									encode_mov_xmm_mem(16, staging_sw_2, state.reg, 16);
									encode_simd_int_add(4, feba, staging_sw_1);
									encode_simd_int_add(4, hgdc, staging_sw_2);
									encode_mov_mem_xmm(16, state.reg, 0, feba);
									encode_mov_mem_xmm(16, state.reg, 16, hgdc);
								} else {
									// Loading the state into feba:hgdc
									Reg staging_0 = Reg64::XMM0;
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
											encode_sha256_msg_words4_part1(w, w1);
											encode_mov_xmm_xmm(staging_0, w3);
											encode_simd_extract(staging_0, w2, 4);
											encode_simd_int_add(4, w, staging_0);
											encode_sha256_msg_words4_part2(w, w3);
										} else w = words[wri];
										encode_mov_xmm_mem(16, staging_0, constant_pointer, 4 * r);
										encode_simd_int_add(4, staging_0, w);
										encode_sha256_rounds2(hgdc, feba, staging_0);
										encode_simd_xmm_shift_right(staging_0, 8);
										encode_sha256_rounds2(feba, hgdc, staging_0);
									}
									encode_mov_xmm_mem(16, words[0], state.reg, 0);
									encode_mov_xmm_mem(16, words[1], state.reg, 16);
									encode_simd_int_add(4, feba, words[0]);
									encode_simd_int_add(4, hgdc, words[1]);
									encode_mov_mem_xmm(16, state.reg, 0, feba);
									encode_mov_mem_xmm(16, state.reg, 16, hgdc);
								}
							}
							encode_add(ld.reg, block_size);
							encode_add(len.reg, -int(block_size));
							_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + _dest.code.Length() - 4) = test - _dest.code.Length();
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + jz - 4) = _dest.code.Length() - jz;
							encode_restore(swap_space_pointer, reg_in_use, 0, software);
							encode_restore(constant_pointer, reg_in_use, 0, true);
						}
						// FINALIZATION
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
						InternalDisposition dest;
						dest.reg = Reg32::EDI;
						dest.flags = DispositionPointer;
						dest.size = state_size;
						encode_preserve(dest.reg, reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &dest, reg_in_use | dest.reg);
						// OPERATION
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
										src.size = 4;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										InternalDisposition src;
										src.flags = DispositionRegister;
										src.reg = disp->reg;
										src.size = 4;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										if (!idle) encode_reg_load_32(disp->reg, disp->reg, 0, disp->size, disp->flags & DispositionCompress, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										InternalDisposition src;
										src.flags = DispositionDiscard;
										src.reg = Reg32::NO;
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
											encode_mov_mem_reg(4, Reg32::EBP, offset, src.reg);
											encode_lea(src.reg, Reg32::EBP, offset);
										}
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionDiscard) {
										InternalDisposition src;
										src.flags = DispositionDiscard;
										src.reg = Reg32::NO;
										src.size = 0;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformAddressOffset) {
									if (node.inputs.Length() < 2 || node.inputs.Length() > 3) throw InvalidArgumentException();
									InternalDisposition base;
									base.flags = DispositionPointer;
									base.reg = Reg32::ESI;
									base.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									encode_preserve(base.reg, reg_in_use, disp->reg, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &base, reg_in_use | uint(base.reg));
									if (node.inputs[1].self.ref_class == ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint offset = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (!idle && offset * scale) {
											if (scale != 0xFFFFFFFF) encode_add(Reg32::ESI, offset * scale);
											else encode_add(Reg32::ESI, -offset);
										}
									} else if (node.inputs[1].self.ref_class != ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										uint scale_lb = logarithm(scale);
										encode_preserve(Reg32::EAX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::EDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_preserve(Reg32::ECX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg32::EAX;
											offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
											if (offset.size != 4) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg32::ESI) | uint(Reg32::EAX));
											if (!idle) {
												if (scale > 1) {
													if (scale_lb != InvalidLogarithm) {
														encode_shl(Reg32::EAX, scale_lb);
													} else {
														encode_mov_reg_const(4, Reg32::ECX, scale);
														encode_mul_div(4, mdOp::MUL, Reg32::ECX);
													}
												}
												encode_operation(4, arOp::ADD, Reg32::ESI, Reg32::EAX);
											}
										}
										encode_restore(Reg32::ECX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg32::EDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg32::EAX, reg_in_use, 0, !idle);
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										uint scale_lb = logarithm(scale);
										encode_preserve(Reg32::EAX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::EDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_preserve(Reg32::ECX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg32::EAX;
											offset.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
											if (offset.size != 4) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(Reg32::ESI) | uint(Reg32::EAX));
											if (!idle) {
												if (scale > 1) {
													if (scale_lb != InvalidLogarithm) {
														encode_shl(Reg32::EAX, scale_lb);
													} else {
														encode_mov_reg_const(4, Reg32::ECX, scale);
														encode_mul_div(4, mdOp::MUL, Reg32::ECX);
													}
												}
												encode_operation(4, arOp::ADD, Reg32::ESI, Reg32::EAX);
											}
										}
										encode_restore(Reg32::ECX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg32::EDX, reg_in_use, 0, !idle && scale > 1 && scale_lb == InvalidLogarithm);
										encode_restore(Reg32::EAX, reg_in_use, 0, !idle);
									} else {
										encode_preserve(Reg32::EAX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::EDX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::ECX, reg_in_use, 0, !idle);
										InternalDisposition offset;
										offset.flags = DispositionRegister;
										offset.reg = Reg32::EAX;
										offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										InternalDisposition scale;
										scale.flags = DispositionRegister;
										scale.reg = Reg32::ECX;
										scale.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (offset.size != 4 || scale.size != 4) throw InvalidArgumentException();
										_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg32::ESI) | uint(Reg32::EAX) | uint(Reg32::EDX) | uint(Reg32::ECX));
										_encode_tree_node(node.inputs[2], idle, mem_load, &scale, reg_in_use | uint(Reg32::ESI) | uint(Reg32::EAX) | uint(Reg32::EDX) | uint(Reg32::ECX));
										if (!idle) {
											encode_mul_div(4, mdOp::MUL, Reg32::ECX);
											encode_operation(4, arOp::ADD, Reg32::ESI, Reg32::EAX);
										}
										encode_restore(Reg32::ECX, reg_in_use, 0, !idle);
										encode_restore(Reg32::EDX, reg_in_use, 0, !idle);
										encode_restore(Reg32::EAX, reg_in_use, 0, !idle);
									}
									if (disp->flags & DispositionPointer) {
										if (disp->reg != Reg32::ESI && !idle) encode_mov_reg_reg(4, disp->reg, Reg32::ESI);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) encode_reg_load_32(disp->reg, Reg32::ESI, 0, disp->size, disp->flags & DispositionCompress, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
									encode_restore(base.reg, reg_in_use, disp->reg, !idle);
								} else if (node.self.index == TransformMove) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									uint size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									InternalDisposition dest_d, src_d;
									dest_d.reg = Reg32::EDI;
									dest_d.size = size;
									dest_d.flags = DispositionPointer;
									src_d.flags = size > 4 ? DispositionPointer : DispositionAny;
									src_d.size = size;
									src_d.reg = Reg32::EAX;
									encode_preserve(Reg32::EDI, reg_in_use, disp->reg, !idle);
									encode_preserve(Reg32::EAX, reg_in_use, 0, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &dest_d, reg_in_use | uint(Reg32::EDI) | uint(Reg32::EAX));
									_encode_tree_node(node.inputs[1], idle, mem_load, &src_d, reg_in_use | uint(Reg32::EDI) | uint(Reg32::EAX));
									if (src_d.flags & DispositionPointer) {
										if (!idle) encode_blt(dest_d.reg, src_d.reg, size, reg_in_use | uint(Reg32::EDI) | uint(Reg32::EAX));
									} else if (!idle)  {
										if (size == 1) encode_mov_mem_reg(1, Reg32::EDI, Reg32::AL);
										else if (size == 2) encode_mov_mem_reg(2, Reg32::EDI, Reg32::AX);
										else if (size == 3) {
											encode_mov_mem_reg(2, Reg32::EDI, Reg32::AX);
											encode_shr(Reg32::EAX, 16);
											encode_mov_mem_reg(1, Reg32::EDI, 2, Reg32::AL);
										} else if (size == 4) encode_mov_mem_reg(4, Reg32::EDI, Reg32::EAX);
									}
									encode_restore(Reg32::EAX, reg_in_use, 0, !idle);
									if ((disp->flags & DispositionRegister) && !(disp->flags & DispositionPointer)) {
										if (!idle) encode_reg_load_32(disp->reg, Reg32::EDI, 0, disp->size, disp->flags & DispositionCompress, reg_in_use | uint(Reg32::EDI));
										disp->flags = DispositionRegister;
									} else {
										if (disp->flags & DispositionAny) {
											disp->flags = DispositionPointer;
											if (disp->reg != Reg32::EDI) encode_mov_reg_reg(4, disp->reg, Reg32::EDI);
										}
									}
									encode_restore(Reg32::EDI, reg_in_use, disp->reg, !idle);
								} else if (node.self.index == TransformInvoke) {
									_encode_general_call(node, idle, mem_load, disp, reg_in_use);
								} else if (node.self.index == TransformTemporary) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									*mem_load += _word_align(node.retval_spec.size);
									InternalDisposition ld;
									ld.flags = DispositionDiscard;
									ld.reg = Reg32::NO;
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
										if (!idle) encode_lea(disp->reg, Reg32::EBP, offset);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) encode_reg_load_32(disp->reg, Reg32::EBP, offset, disp->size, disp->flags & DispositionCompress, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformBreakIf) {
									if (node.inputs.Length() != 3) throw InvalidArgumentException();
									_encode_tree_node(node.inputs[0], idle, mem_load, disp, reg_in_use);
									InternalDisposition ld;
									ld.flags = DispositionRegister | DispositionCompress;
									ld.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
									ld.reg = Reg32::ECX;
									encode_preserve(ld.reg, reg_in_use, 0, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									if (!idle) {
										auto effective_frame_size = _allocated_frame_size;
										encode_test(min(ld.size, 4), ld.reg, 0xFFFFFFFF);
										_dest.code << 0x0F; _dest.code << 0x84; // JZ
										_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
										int addr = _dest.code.Length();
										auto scope = _scopes.GetLast();
										while (scope && !scope->GetValue().shift_sp) scope = scope->GetPrevious();
										if (scope) encode_lea(Reg32::ESP, Reg32::EBP, scope->GetValue().frame_base);
										else encode_lea(Reg32::ESP, Reg32::EBP, _scope_frame_base);
										encode_scope_unroll(_current_instruction, _current_instruction + 1 + int(node.input_specs[2].size.num_bytes));
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
										ld.reg = Reg32::ESI;
										ld.size = size;
									}
									encode_preserve(ld.reg, reg_in_use, disp->reg, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									*mem_load += _word_align(node.input_specs[0].size);
									Reg dest = ld.reg == Reg32::EDI ? Reg32::EDX : Reg32::EDI;
									if (!idle) {
										int offset = allocate_temporary(node.input_specs[0].size);
										_scopes.GetLast()->GetValue().current_split_offset = offset;
										encode_preserve(dest, reg_in_use | uint(ld.reg), 0, true);
										encode_lea(dest, Reg32::EBP, offset);
										if (ld.flags & DispositionPointer) encode_blt(dest, ld.reg, size, reg_in_use | uint(ld.reg) | uint(dest));
										else encode_mov_mem_reg(4, dest, ld.reg);
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
									if (copy == Reg32::NO) copy = Reg32::EAX;
									addend = (copy == Reg32::ECX) ? Reg32::EAX : Reg32::ECX;
									ptr = (copy == Reg32::EDI) ? Reg32::EBX : Reg32::EDI;
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
											encode_mov_mem_reg(size, Reg32::EBP, offset, copy);
											encode_lea(copy, Reg32::EBP, offset);
										}
									} else disp->flags = DispositionDiscard;
								} else if (node.self.index == TransformAtomicSet) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									auto size = object_size(node.input_specs[0].size);
									Reg ptr, value;
									value = disp->reg;
									if (value == Reg32::NO) value = Reg32::EAX;
									ptr = (value == Reg32::EDI) ? Reg32::EBX : Reg32::EDI;
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
											encode_mov_mem_reg(size, Reg32::EBP, offset, value);
											encode_lea(value, Reg32::EBP, offset);
										}
									} else disp->flags = DispositionDiscard;
								} else throw InvalidArgumentException();
							} else if (node.self.index >= 0x010 && node.self.index < 0x013) {
								_encode_logics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x013 && node.self.index < 0x050) {
								_encode_arithmetics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x080 && node.self.index < 0x100) {
								_encode_floating_point(node, idle, mem_load, disp, reg_in_use);
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
								Reg ld = Reg32::EAX;
								if (ld == disp->reg) ld = Reg32::EDX;
								encode_preserve(ld, reg_in_use, 0, true);
								encode_put_addr_of(ld, node.self);
								encode_reg_load_32(disp->reg, ld, 0, disp->size, disp->flags & DispositionCompress, reg_in_use | uint(ld));
								encode_restore(ld, reg_in_use, 0, true);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					}
				}
				void _encode_expression_evaluation(const ExpressionTree & tree, Reg retval_copy)
				{
					if (retval_copy != Reg32::NO && (tree.self.ref_flags & ReferenceFlagInvoke) && _is_out_pass_by_reference(tree.retval_spec)) throw InvalidArgumentException();
					int _temp_storage = 0;
					InternalDisposition disp;
					disp.reg = retval_copy;
					if (retval_copy == Reg32::NO) {
						disp.flags = DispositionDiscard;
						disp.size = 0;
					} else {
						disp.flags = DispositionRegister | DispositionCompress;
						disp.size = 1;
					}
					_encode_tree_node(tree, true, &_temp_storage, &disp, uint(retval_copy));
					encode_open_scope(_temp_storage, true, 0);
					_encode_tree_node(tree, false, &_temp_storage, &disp, uint(retval_copy));
					encode_close_scope(uint(retval_copy));
				}
			public:
				EncoderContext(Environment osenv, TranslatedFunction & dest, const Function & src) : X86::EncoderContext(osenv, dest, src, false) { if (osenv != Environment::Windows && osenv != Environment::Linux) throw InvalidArgumentException(); translator_sha2_state_init(_sha_state); }
				virtual void encode_function_prologue(void) override
				{
					_stack_clear_size = 0;
					SafePointer< Array<ArgumentPassageInfo> > api = _make_interface_layout(_src.retval, _src.inputs.GetBuffer(), _src.inputs.Length(), &_abi);
					_scope_frame_base = _unroll_base = -WordSize * 3;
					encode_push(Reg32::EBP);
					encode_mov_reg_reg(4, Reg32::EBP, Reg32::ESP);
					encode_push(Reg32::EBX);
					encode_push(Reg32::EDI);
					encode_push(Reg32::ESI);
					if (!_is_out_pass_by_reference(_src.retval)) {
						int size = _word_align(_src.retval.size);
						if (_src.retval.semantics == ArgumentSemantics::FloatingPoint && size > 0) {
							_retval.bound_to = Reg32::ST0;
							_retval.bound_to_hi_dword = Reg32::NO;
						} else {
							_retval.bound_to = Reg32::NO;
							_retval.bound_to_hi_dword = Reg32::NO;
							if (size > 0) _retval.bound_to = Reg32::EAX;
							if (size > WordSize) _retval.bound_to_hi_dword = Reg32::EDX;
						}
						_retval.indirect = false;
						_scope_frame_base -= size;
						_unroll_base -= size;
						_retval.bp_offset = _scope_frame_base;
						if (size) encode_stack_alloc(size);
					}
					int arg_offs = 2 * WordSize;
					for (int i = 0; i < api->Length(); i++) {
						auto & info = api->ElementAt(i);
						if (info.index >= 0) {
							if (info.reg != Reg32::NO) {
								_scope_frame_base -= WordSize;
								_inputs[info.index].bound_to = info.reg;
								_inputs[info.index].bound_to_hi_dword = Reg32::NO;
								_inputs[info.index].bp_offset = _scope_frame_base;
								_inputs[info.index].indirect = info.indirect;
								encode_push(info.reg);
							} else {
								_inputs[info.index].bound_to = Reg32::NO;
								_inputs[info.index].bound_to_hi_dword = Reg32::NO;
								_inputs[info.index].bp_offset = arg_offs;
								_inputs[info.index].indirect = info.indirect;
								auto size = info.indirect ? WordSize : _word_align(_src.inputs[info.index].size);
								arg_offs += size;
								_stack_clear_size += size;
							}
						} else {
							_retval.bound_to = Reg32::NO;
							_retval.bound_to_hi_dword = Reg32::NO;
							_retval.bp_offset = arg_offs;
							_retval.indirect = true;
							arg_offs += WordSize;
							_stack_clear_size += WordSize;
						}
					}
				}
				virtual void encode_function_epilogue(void) override
				{
					if (_unroll_base != _scope_frame_base) encode_lea(Reg32::ESP, Reg32::EBP, _unroll_base);
					if (_retval.indirect) {
						encode_mov_reg_mem(4, Reg32::EAX, Reg32::EBP, _retval.bp_offset);
					} else {
						if (_retval.bound_to == Reg32::ST0) {
							auto quant = _word_align(_src.retval.size);
							encode_fld(quant, Reg32::EBP, _retval.bp_offset);
							encode_add(Reg32::ESP, quant);
						} else {
							if (_retval.bound_to != Reg32::NO) encode_pop(_retval.bound_to);
							if (_retval.bound_to_hi_dword != Reg32::NO) encode_pop(_retval.bound_to_hi_dword);
						}
					}
					encode_pop(Reg32::ESI);
					encode_pop(Reg32::EDI);
					encode_pop(Reg32::EBX);
					encode_pop(Reg32::EBP);
					if (_abi == ABI::THISCALL) encode_pure_ret(_stack_clear_size);
					else if (_osenv != Environment::Windows && _retval.indirect) encode_pure_ret(4);
					else encode_pure_ret(0);
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
							_encode_expression_evaluation(inst.tree, Reg32::NO);
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
							_encode_expression_evaluation(inst.tree, Reg32::NO);
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
							_encode_expression_evaluation(inst.tree, Reg32::ECX);
							encode_test(1, Reg32::ECX, 0xFF);
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
							_encode_expression_evaluation(inst.tree, Reg32::NO);
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

			class TranslatorX86i386 : public IAssemblyTranslator
			{
				Environment _osenv;
			public:
				TranslatorX86i386(Environment osenv) : _osenv(osenv) {}
				virtual bool Translate(TranslatedFunction & dest, const Function & src) noexcept override
				{
					try {
						dest.Clear();
						dest.data = src.data;
						i386::EncoderContext ctx(_osenv, dest, src);
						ctx.encode_function_prologue();
						ctx.process_encoding();
						ctx.finalize_encoding();
						ctx.encode_debugger_trap();
						while (dest.code.Length() & 0x3) ctx.encode_debugger_trap();
						return true;
					} catch (...) { dest.Clear(); return false; }
				}
				virtual uint GetWordSize(void) noexcept override { return WordSize; }
				virtual Platform GetPlatform(void) noexcept override { return Platform::X86; }
				virtual Environment GetEnvironment(void) noexcept override { return _osenv; }
				virtual string ToString(void) const override { return L"XA-80386"; }
			};
		}

		IAssemblyTranslator * CreateTranslatorX86i386(Environment osenv) { return new (std::nothrow) i386::TranslatorX86i386(osenv); }
	}
}