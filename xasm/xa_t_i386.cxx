#include "xa_t_i386.h"
#include "xa_t_x86.h"
#include "xa_type_helper.h"

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
				static bool _is_fp_register(Reg reg) { return reg == Reg32::ST0; }
				static bool _is_gp_register(Reg reg) { return uint(reg) & 0xFF; }
				static bool _is_in_pass_by_reference(const ArgumentSpecification & spec) { return spec.semantics == ArgumentSemantics::Object; }
				static bool _is_out_pass_by_reference(const ArgumentSpecification & spec) { return _word_align(spec.size) > 8 || spec.semantics == ArgumentSemantics::Object; }
				static uint32 _word_align(const ObjectSize & size) { uint full_size = size.num_bytes + WordSize * size.num_words; return (uint64(full_size) + 3) / 4 * 4; }
				Array<ArgumentPassageInfo> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt, ABI * abiret)
				{
					SafePointer< Array<ArgumentPassageInfo> > result = new Array<ArgumentPassageInfo>(0x40);
					ABI abi = ABI::CDECL;
					if (_conv == CallingConvention::Windows) for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics == ArgumentSemantics::This) {
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
						bool skip = false;
						for (auto & i : _init_locals) if (i.bp_offset == l.bp_offset) { skip = true; break; }
						if (skip) continue;
						int stack_growth = 0;
						for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
							auto & arg = l.finalizer.final_args[i];
							encode_put_addr_of(Reg32::ECX, arg);
							encode_push(Reg32::ECX);
						}
						if (_conv == CallingConvention::Windows) {
							stack_growth = 0;
							encode_lea(Reg32::ECX, Reg32::EBP, l.bp_offset);
						} else if (_conv == CallingConvention::Unix) {
							stack_growth = (l.finalizer.final_args.Length() + 1) * 4;
							encode_lea(Reg32::ECX, Reg32::EBP, l.bp_offset);
							encode_push(Reg32::ECX);
						}
						encode_put_addr_of(Reg32::EAX, l.finalizer.final);
						encode_call(Reg32::EAX, false);
						if (stack_growth) encode_add(Reg32::ESP, stack_growth);
					}
					encode_restore(Reg32::EDX, reg_in_use, 0, true);
					encode_restore(Reg32::ECX, reg_in_use, 0, true);
					encode_restore(Reg32::EAX, reg_in_use, 0, true);
					if (scope.shift_sp) encode_add(Reg32::ESP, scope.frame_size);
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
								encode_add(Reg32::ESP, 16);
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
							*mem_load += 4;
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
					if (stack_usage && !idle) encode_add(Reg32::ESP, -int(stack_usage));
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
						if (_conv == CallingConvention::Unix && retval_byref) stack_usage -= 4;
						if (stack_usage && conv == ABI::CDECL) encode_add(Reg32::ESP, int(stack_usage));
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

					throw Exception();

					// TODO: IMPLEMENT
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
										encode_preserve(Reg32::EAX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::EDX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::ECX, reg_in_use, 0, !idle && scale > 1);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg32::EAX;
											offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
											if (offset.size != 4) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg32::ESI) | uint(Reg32::EAX) | uint(Reg32::EDX) | uint(Reg32::ECX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(4, Reg32::ECX, scale);
													encode_mul_div(4, mdOp::MUL, Reg32::ECX);
												}
												encode_operation(4, arOp::ADD, Reg32::ESI, Reg32::EAX);
											}
										}
										encode_restore(Reg32::ECX, reg_in_use, 0, !idle && scale > 1);
										encode_restore(Reg32::EDX, reg_in_use, 0, !idle);
										encode_restore(Reg32::EAX, reg_in_use, 0, !idle);
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										encode_preserve(Reg32::EAX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::EDX, reg_in_use, 0, !idle);
										encode_preserve(Reg32::ECX, reg_in_use, 0, !idle && scale > 1);
										if (scale) {
											InternalDisposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg32::EAX;
											offset.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
											if (offset.size != 4) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(Reg32::ESI) | uint(Reg32::EAX) | uint(Reg32::EDX) | uint(Reg32::ECX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(4, Reg32::ECX, scale);
													encode_mul_div(4, mdOp::MUL, Reg32::ECX);
												}
												encode_operation(4, arOp::ADD, Reg32::ESI, Reg32::EAX);
											}
										}
										encode_restore(Reg32::ECX, reg_in_use, 0, !idle && scale > 1);
										encode_restore(Reg32::EDX, reg_in_use, 0, !idle);
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
								} else if (node.self.index == TransformBlockTransfer) {
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
								} else throw InvalidArgumentException();
							} else if (node.self.index >= 0x010 && node.self.index < 0x013) {
								_encode_logics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x013 && node.self.index < 0x050) {
								_encode_arithmetics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x080 && node.self.index < 0x100) {
								_encode_floating_point(node, idle, mem_load, disp, reg_in_use);
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
				EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src) : X86::EncoderContext(conv, dest, src, false) {}
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
						if (size) encode_add(Reg32::ESP, -size);
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
					else if (_conv == CallingConvention::Unix && _retval.indirect) encode_pure_ret(4);
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
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							JumpRelocStruct rs;
							rs.machine_offset_at = _dest.code.Length() - 4;
							rs.machine_offset_relative_to = _dest.code.Length();
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
						} else if (inst.opcode == OpcodeConditionalJump) {
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
						} else if (inst.opcode == OpcodeControlReturn) {
							_encode_expression_evaluation(inst.tree, Reg32::NO);
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

			class TranslatorX86i386 : public IAssemblyTranslator
			{
				CallingConvention _conv;
			public:
				TranslatorX86i386(CallingConvention conv) : _conv(conv) {}
				virtual bool Translate(TranslatedFunction & dest, const Function & src) noexcept override
				{
					try {
						dest.Clear();
						dest.data = src.data;
						i386::EncoderContext ctx(_conv, dest, src);
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
				virtual CallingConvention GetCallingConvention(void) noexcept override { return _conv; }
				virtual string ToString(void) const override { return L"XA-80386"; }
			};
		}

		IAssemblyTranslator * CreateTranslatorX86i386(CallingConvention conv) { return new (std::nothrow) i386::TranslatorX86i386(conv); }
	}
}