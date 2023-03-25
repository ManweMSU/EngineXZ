#include "xa_t_i386.h"
#include "xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace i386
		{
			constexpr int WordSize = 4;

			enum class Reg : uint {
				NO = 0, ST0 = 0x0100,
				EAX = 0x0001, ECX = 0x0002, EDX = 0x0004, EBX = 0x0008,
				ESP = 0x0010, EBP = 0x0020, ESI = 0x0040, EDI = 0x0080,
				AX = 0x0001, CX = 0x0002, DX = 0x0004, BX = 0x0008,
				SP = 0x0010, BP = 0x0020, SI = 0x0040, DI = 0x0080,
				AL = 0x0001, CL = 0x0002, DL = 0x0004, BL = 0x0008,
				AH = 0x0010, CH = 0x0020, DH = 0x0040, BH = 0x0080,
			};
			enum class CC { CDECL, THISCALL };
			enum class arOp : uint8 { ADD = 0x02, ADC = 0x12, SBB = 0x1A, SUB = 0x2A, AND = 0x22, OR = 0x0A, XOR = 0x32, CMP = 0x3A };
			enum class mdOp : uint8 { MUL = 0x4, DIV = 0x6, IMUL = 0x5, IDIV = 0x7 };
			enum class shOp : uint8 { ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR };

			enum DispositionFlags {
				DispositionRegister	= 0x01,
				DispositionPointer	= 0x02,
				DispositionDiscard	= 0x04,
				DispositionCompress	= 0x10,
				DispositionAny		= DispositionRegister | DispositionPointer
			};

			class EncoderContext
			{
				struct _argument_passage_info {
					int index;		// 0, 1, ... - real arguments; -1 - retval
					Reg reg;		// holder register; NO for stack storage;
					bool indirect;	// pass-by-reference
				};
				struct _argument_storage_spec {
					Reg bound_to;
					Reg bound_to_hi_dword;
					int rbp_offset;
					bool indirect;
				};
				struct _jump_reloc_struct {
					uint machine_offset_at;
					uint machine_offset_relative_to;
					uint xasm_offset_jump_to;
				};
				struct _internal_disposition {
					int flags;
					Reg reg;
					int size;
				};
				struct _local_disposition {
					int ebp_offset;
					int size;
					FinalizerReference finalizer;
				};
				struct _local_scope {
					int frame_base; // EBP + frame_base = real frame base
					int frame_size;
					int frame_size_unused;
					int first_local_no;
					Array<_local_disposition> locals = Array<_local_disposition>(0x20);
					bool shift_esp, temporary;
				};

				CallingConvention _conv;
				CC _conv_eval;
				TranslatedFunction & _dest;
				const Function & _src;
				Array<uint> _org_inst_offsets;
				Array<_jump_reloc_struct> _jump_reloc;
				_argument_storage_spec _retval;
				Array<_argument_storage_spec> _inputs;
				int _scope_frame_base, _unroll_base, _current_instruction, _stack_clear_size;
				Volumes::Stack<_local_scope> _scopes;
				Volumes::Stack<_local_disposition> _init_locals;

				static bool _is_fp_register(Reg reg) { return reg == Reg::ST0; }
				static bool _is_gp_register(Reg reg) { return uint(reg) & 0xFF; }
				static bool _is_in_pass_by_reference(const ArgumentSpecification & spec) { return spec.semantics == ArgumentSemantics::Object; }
				static bool _is_out_pass_by_reference(const ArgumentSpecification & spec) { return _word_align(spec.size) > 8 || spec.semantics == ArgumentSemantics::Object; }
				static uint8 _regular_register_code(Reg reg)
				{
					if (reg == Reg::EAX) return 0;
					else if (reg == Reg::ECX) return 1;
					else if (reg == Reg::EDX) return 2;
					else if (reg == Reg::EBX) return 3;
					else if (reg == Reg::ESP) return 4;
					else if (reg == Reg::EBP) return 5;
					else if (reg == Reg::ESI) return 6;
					else if (reg == Reg::EDI) return 7;
					else return 16;
				}
				static uint8 _make_mod(uint8 reg_lo_3, uint8 mod, uint8 reg_mem_lo_3) { return (mod << 6) | (reg_lo_3 << 3) | reg_mem_lo_3; }
				static uint32 _word_align(const ObjectSize & size) { uint full_size = size.num_bytes + WordSize * size.num_words; return (uint64(full_size) + 3) / 4 * 4; }
				Array<_argument_passage_info> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt, CC * ccout)
				{
					SafePointer< Array<_argument_passage_info> > result = new Array<_argument_passage_info>(0x40);
					CC cc = CC::CDECL;
					if (_conv == CallingConvention::Windows) for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics == ArgumentSemantics::This) {
						cc = CC::THISCALL;
						_argument_passage_info info;
						info.index = i;
						info.reg = Reg::ECX;
						info.indirect = _is_in_pass_by_reference(inputs[i]);
						result->Append(info);
						break;
					}
					if (_is_out_pass_by_reference(output)) {
						_argument_passage_info info;
						info.index = -1;
						info.reg = Reg::NO;
						info.indirect = true;
						result->Append(info);
					}
					for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics != ArgumentSemantics::This || cc != CC::THISCALL) {
						_argument_passage_info info;
						info.index = i;
						info.reg = Reg::NO;
						info.indirect = _is_in_pass_by_reference(inputs[i]);
						result->Append(info);
					}
					*ccout = cc;
					result->Retain();
					return result;
				}
				int _allocate_temporary(const ObjectSize & size, const FinalizerReference & final, int * ebp_offs = 0)
				{
					auto current_scope_ptr = _scopes.GetLast();
					if (!current_scope_ptr) throw InvalidArgumentException();
					auto & scope = current_scope_ptr->GetValue();
					auto size_padded = _word_align(size);
					_local_disposition new_var;
					new_var.size = size.num_bytes + WordSize * size.num_words;
					new_var.ebp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
					new_var.finalizer = final;
					scope.frame_size_unused -= size_padded;
					if (scope.frame_size_unused < 0) throw InvalidArgumentException();
					auto index = scope.first_local_no + scope.locals.Length();
					scope.locals << new_var;
					if (ebp_offs) *ebp_offs = new_var.ebp_offset;
					return index;
				}
				void _relocate_code_at(int offset) { _dest.code_reloc << offset; }
				void _relocate_data_at(int offset) { _dest.data_reloc << offset; }
				void _refer_object_at(const string & name, int offset)
				{
					auto ent = _dest.extrefs[name];
					if (!ent) {
						_dest.extrefs.Append(name, Array<uint32>(0x100));
						ent = _dest.extrefs[name];
						if (!ent) return;
					}
					ent->Append(offset);
				}
				void _encode_preserve(Reg reg, uint reg_in_use, bool cond_if) { if (cond_if && (reg_in_use & uint(reg))) encode_push(reg); }
				void _encode_restore(Reg reg, uint reg_in_use, bool cond_if) { if (cond_if && (reg_in_use & uint(reg))) encode_pop(reg); }
				void _encode_open_scope(int of_size, bool temporary, int enf_base)
				{
					if (!enf_base) of_size = _word_align(TH::MakeSize(of_size, 0));
					auto prev = _scopes.GetLast();
					_local_scope ls;
					ls.frame_base = enf_base ? enf_base : (prev ? prev->GetValue().frame_base - of_size : _scope_frame_base - of_size);
					ls.frame_size = of_size;
					ls.frame_size_unused = of_size;
					ls.first_local_no = prev ? prev->GetValue().first_local_no + prev->GetValue().locals.Length() : 0;
					ls.shift_esp = of_size && !enf_base;
					ls.temporary = temporary;
					_scopes.Push(ls);
					if (ls.shift_esp) encode_add(Reg::ESP, -of_size);
				}
				void _encode_finalize_scope(const _local_scope & scope, uint reg_in_use = 0)
				{
					_encode_preserve(Reg::EAX, reg_in_use, true);
					_encode_preserve(Reg::ECX, reg_in_use, true);
					_encode_preserve(Reg::EDX, reg_in_use, true);
					for (auto & l : scope.locals) if (l.finalizer.final.ref_class != ReferenceNull) {
						bool skip = false;
						for (auto & i : _init_locals) if (i.ebp_offset == l.ebp_offset) { skip = true; break; }
						if (skip) continue;
						int stack_growth = 0;
						for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
							auto & arg = l.finalizer.final_args[i];
							encode_put_addr_of(Reg::ECX, arg);
							encode_push(Reg::ECX);
						}
						if (_conv == CallingConvention::Windows) {
							stack_growth = 0;
							encode_lea(Reg::ECX, Reg::EBP, l.ebp_offset);
						} else if (_conv == CallingConvention::Unix) {
							stack_growth = (l.finalizer.final_args.Length() + 1) * 4;
							encode_lea(Reg::ECX, Reg::EBP, l.ebp_offset);
							encode_push(Reg::ECX);
						}
						encode_put_addr_of(Reg::EAX, l.finalizer.final);
						encode_call(Reg::EAX, false);
						if (stack_growth) encode_add(Reg::ESP, stack_growth);
					}
					_encode_restore(Reg::EDX, reg_in_use, true);
					_encode_restore(Reg::ECX, reg_in_use, true);
					_encode_restore(Reg::EAX, reg_in_use, true);
					if (scope.shift_esp) encode_add(Reg::ESP, scope.frame_size);
				}
				void _encode_close_scope(uint reg_in_use = 0)
				{
					auto scope = _scopes.Pop();
					_encode_finalize_scope(scope, reg_in_use);
				}
				void _encode_blt(Reg dest_ptr, Reg src_ptr, int size, uint reg_in_use)
				{
					Reg ir;
					if (dest_ptr != Reg::EDX && src_ptr != Reg::EDX) ir = Reg::EDX;
					else if (dest_ptr != Reg::EAX && src_ptr != Reg::EAX) ir = Reg::EAX;
					else ir = Reg::EBX;
					_encode_preserve(ir, reg_in_use, true);
					int blt_ofs = 0;
					while (blt_ofs < size) {
						if (blt_ofs + 4 <= size) {
							encode_mov_reg_mem(4, ir, src_ptr, blt_ofs);
							encode_mov_mem_reg(4, dest_ptr, blt_ofs, ir);
							blt_ofs += 4;
						} else if (blt_ofs + 2 <= size) {
							encode_mov_reg_mem(2, ir, src_ptr, blt_ofs);
							encode_mov_mem_reg(2, dest_ptr, blt_ofs, ir);
							blt_ofs += 2;
						} else {
							encode_mov_reg_mem(1, ir, src_ptr, blt_ofs);
							encode_mov_mem_reg(1, dest_ptr, blt_ofs, ir);
							blt_ofs++;
						}
					}
					_encode_restore(ir, reg_in_use, true);
				}
				void _encode_reg_load(Reg dest, Reg src_ptr, int src_offs, int size, bool allow_compress, uint reg_in_use)
				{
					if (size > 4) {
						if (allow_compress) {
							Reg local = Reg::ECX;
							if (dest == Reg::ECX && src_ptr == Reg::ECX) local = Reg::EDX;
							if (dest == Reg::EDX && src_ptr == Reg::EDX) local = Reg::EBX;
							_encode_preserve(local, reg_in_use, true);
							encode_mov_reg_mem(4, local, src_ptr, src_offs);
							if (size == 8) encode_operation(4, arOp::OR, local, src_ptr, true, src_offs + 4);
							else if (size == 7) {
								encode_operation(2, arOp::OR, local, src_ptr, true, src_offs + 4);
								encode_operation(1, arOp::OR, local, src_ptr, true, src_offs + 6);
							} else if (size == 6) encode_operation(2, arOp::OR, local, src_ptr, true, src_offs + 4);
							else if (size == 5) encode_operation(1, arOp::OR, local, src_ptr, true, src_offs + 4);
							encode_mov_reg_reg(4, dest, local);
							_encode_restore(local, reg_in_use, true);
						} else encode_mov_reg_mem(4, dest, src_ptr, src_offs);
					} else {
						if (size == 4) encode_mov_reg_mem(4, dest, src_ptr, src_offs);
						else if (size == 3) {
							bool rest_src = false, rest_local = false;
							if (dest == src_ptr) {
								if (dest != Reg::EBX) src_ptr = Reg::EBX;
								else src_ptr = Reg::ECX;
								rest_src = true;
								_encode_preserve(src_ptr, reg_in_use, true);
								encode_mov_reg_reg(4, src_ptr, dest);
							}
							Reg local = dest;
							if (local == Reg::ESI || local == Reg::EDI) {
								if (src_ptr != Reg::EAX) local = Reg::EAX;
								else local = Reg::EDX;
								rest_local = true;
							}
							_encode_preserve(local, reg_in_use, rest_local);
							encode_mov_reg_mem(1, local, src_ptr, src_offs + 2);
							encode_shl(local, 16);
							encode_mov_reg_mem(2, local, src_ptr, src_offs);
							if (local != dest) encode_mov_reg_reg(4, dest, local);
							_encode_restore(local, reg_in_use, rest_local);
							_encode_restore(src_ptr, reg_in_use, rest_src);
						} else if (size == 2) encode_mov_reg_mem(2, dest, src_ptr, src_offs);
						else if (size == 1) {
							if (dest == Reg::ESI || dest == Reg::EDI) {
								if (src_ptr == Reg::ESI || src_ptr == Reg::EDI || src_ptr == Reg::EBP) {
									_encode_preserve(Reg::ECX, reg_in_use, true);
									encode_mov_reg_mem(1, Reg::CL, src_ptr, src_offs);
									encode_mov_reg_reg(4, dest, Reg::ECX);
									_encode_restore(Reg::ECX, reg_in_use, true);
								} else {
									encode_mov_reg_mem(1, src_ptr, src_ptr, src_offs);
									encode_mov_reg_reg(4, dest, src_ptr);
								}
							} else encode_mov_reg_mem(1, dest, src_ptr, src_offs);
						}
					}
				}
				void _encode_emulate_mul_64(void) // uses all registers, input on [EAX], [ECX], output on EBX:ECX
				{
					encode_mov_reg_reg(4, Reg::ESI, Reg::EAX);
					encode_mov_reg_reg(4, Reg::EDI, Reg::ECX); // [ESI] * [EDI], use ECX:EBX as accumulator
					encode_mov_reg_mem(4, Reg::EAX, Reg::ESI);
					encode_mul_div(4, mdOp::MUL, Reg::EDI, true); // dword [ESI] * dword [EDI] -> EAX:EDX
					encode_mov_reg_reg(4, Reg::ECX, Reg::EAX);
					encode_mov_reg_reg(4, Reg::EBX, Reg::EDX); // dword [ESI] * dword[EDI] -> ECX:EBX
					encode_mov_reg_mem(4, Reg::EAX, Reg::ESI, 4);
					encode_mul_div(4, mdOp::MUL, Reg::EDI, true); // dword [ESI + 4] * dword [EDI] -> EAX:EDX
					encode_operation(4, arOp::ADD, Reg::EBX, Reg::EAX); // summ
					encode_mov_reg_mem(4, Reg::EAX, Reg::ESI);
					encode_mul_div(4, mdOp::MUL, Reg::EDI, true, 4); // dword [ESI] * dword [EDI + 4] -> EAX:EDX
					encode_operation(4, arOp::ADD, Reg::EBX, Reg::EAX); // summ, the result is ready
				}
				void _encode_emulate_div_64(void) // uses all registers, input on [EAX], [ECX]; output div on EAX:EDX, mod on ESI:EDI
				{
					encode_mov_reg_reg(4, Reg::ESI, Reg::EAX);
					encode_mov_reg_reg(4, Reg::EDI, Reg::ECX); // [ESI] / [EDI]
					encode_mov_reg_mem(4, Reg::EDX, Reg::EDI, 4);
					encode_mov_reg_mem(4, Reg::EAX, Reg::EDI); // [EDI] -> EAX:EDX
					encode_mov_reg_reg(4, Reg::ECX, Reg::EAX);
					encode_operation(4, arOp::OR, Reg::ECX, Reg::EDX);
					_dest.code << 0x75 << 0x00; // JNZ
					int addr = _dest.code.Length();
					_dest.code << 0xCD << 0x04; // INT 4 (OVERFLOW)
					_dest.code[addr - 1] = _dest.code.Length() - addr;
					encode_operation(4, arOp::XOR, Reg::ECX, Reg::ECX); // ECX = 0
					addr = _dest.code.Length();
					encode_test(4, Reg::EDX, 0x80000000);
					_dest.code << 0x75 << 0x00; // JNZ
					int addr2 = _dest.code.Length();
					encode_shift(4, shOp::SHL, Reg::EAX, 1);
					encode_shift(4, shOp::RCL, Reg::EDX, 1);
					encode_add(Reg::ECX, 1);
					_dest.code << 0xEB; _dest.code << (addr - _dest.code.Length() - 1);
					_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
					encode_mov_reg_mem(4, Reg::EBX, Reg::ESI, 4);
					encode_push(Reg::EBX);
					encode_mov_reg_mem(4, Reg::EBX, Reg::ESI);
					encode_push(Reg::EBX);
					encode_operation(4, arOp::XOR, Reg::EBX, Reg::EBX);
					encode_push(Reg::EBX);
					encode_push(Reg::EBX);
					encode_mov_reg_reg(4, Reg::EBX, Reg::ESP);
					addr = _dest.code.Length();
					encode_mov_reg_mem(4, Reg::EDI, Reg::EBX, 12);
					encode_mov_reg_mem(4, Reg::ESI, Reg::EBX, 8);
					encode_operation(4, arOp::SUB, Reg::ESI, Reg::EAX);
					encode_operation(4, arOp::SBB, Reg::EDI, Reg::EDX);
					_dest.code << 0x72 << 0x00; // JB
					addr2 = _dest.code.Length();
					encode_mov_mem_reg(4, Reg::EBX, 12, Reg::EDI);
					encode_mov_mem_reg(4, Reg::EBX, 8, Reg::ESI);
					encode_shift(4, shOp::SHL, Reg::EBX, 1, true);
					encode_shift(4, shOp::RCL, Reg::EBX, 1, true, 4);
					encode_mov_reg_mem(4, Reg::ESI, Reg::EBX);
					encode_add(Reg::ESI, 1);
					encode_mov_mem_reg(4, Reg::EBX, Reg::ESI);
					_dest.code << 0xEB << 0x00; // JMP
					_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
					addr2 = _dest.code.Length();
					encode_shift(4, shOp::SHL, Reg::EBX, 1, true);
					encode_shift(4, shOp::RCL, Reg::EBX, 1, true, 4);
					_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
					encode_test(4, Reg::ECX, 0xFFFFFFFF);
					_dest.code << 0x74 << 0x00; // JZ
					addr2 = _dest.code.Length();
					encode_shift(4, shOp::SHR, Reg::EDX, 1);
					encode_shift(4, shOp::RCR, Reg::EAX, 1);
					encode_add(Reg::ECX, -1);
					_dest.code << 0xEB;
					_dest.code << (addr - _dest.code.Length() - 1);
					_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
					encode_pop(Reg::EAX);
					encode_pop(Reg::EDX);
					encode_pop(Reg::ESI);
					encode_pop(Reg::EDI);
				}
				void _encode_emulate_collect_signum(Reg data, Reg sgn) // pushes abs([data]) onto stack, inverts sgn if [data] is negative. updates [data]. uses ESI and EDI
				{
					encode_mov_reg_mem(4, Reg::EDI, data, 4);
					encode_mov_reg_mem(4, Reg::ESI, data); // [data] -> ESI:EDI
					encode_push(Reg::EDI);
					encode_push(Reg::ESI);
					encode_mov_reg_reg(4, data, Reg::ESP);
					encode_test(4, Reg::EDI, 0x80000000);
					_dest.code << 0x74 << 0x00; // JZ
					int addr = _dest.code.Length();
					encode_operation(4, arOp::XOR, Reg::ESI, Reg::ESI);
					encode_operation(4, arOp::XOR, Reg::EDI, Reg::EDI);
					encode_operation(4, arOp::SUB, Reg::ESI, data, true);
					encode_operation(4, arOp::SBB, Reg::EDI, data, true, 4);
					encode_mov_mem_reg(4, data, Reg::ESI);
					encode_mov_mem_reg(4, data, 4, Reg::EDI);
					encode_invert(4, sgn);
					_dest.code[addr - 1] = _dest.code.Length() - addr;
				}
				void _encode_emulate_set_signum(Reg data_lo, Reg data_hi, Reg sgn) // inverts data_lo:data_hi if sgn != 0. uses ESI and EDI
				{
					encode_test(4, sgn, 1);
					_dest.code << 0x74 << 0x00; // JZ
					int addr = _dest.code.Length();
					encode_operation(4, arOp::XOR, Reg::ESI, Reg::ESI);
					encode_operation(4, arOp::XOR, Reg::EDI, Reg::EDI);
					encode_operation(4, arOp::SUB, Reg::ESI, data_lo);
					encode_operation(4, arOp::SBB, Reg::EDI, data_hi);
					encode_mov_reg_reg(4, data_lo, Reg::ESI);
					encode_mov_reg_reg(4, data_hi, Reg::EDI);
					_dest.code[addr - 1] = _dest.code.Length() - addr;
				}
				void _encode_arithmetics_shift(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
					auto opcode = node.self.index;
					int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					_encode_preserve(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
					_encode_preserve(Reg::ECX, reg_in_use, !idle && disp->reg != Reg::ECX);
					_encode_preserve(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && size == 8);
					_internal_disposition a1, a2;
					a1.reg = Reg::EAX; a2.reg = Reg::ECX;
					a1.size = a2.size = size;
					if (size == 8) a1.flags = a2.flags = DispositionPointer;
					else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
					_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg::EAX));
					_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg::EAX) | uint(Reg::ECX));
					if (size == 8) {
						if (!idle) {
							_encode_preserve(Reg::ESI, reg_in_use, !idle);
							_encode_preserve(Reg::EDI, reg_in_use, !idle);
							encode_mov_reg_mem(4, Reg::EDX, Reg::EAX, 4);
							encode_mov_reg_mem(4, Reg::EAX, Reg::EAX); // EAX:EDX holds the base argument
							encode_mov_reg_mem(4, Reg::EDI, Reg::ECX, 4);
							encode_mov_reg_mem(4, Reg::ESI, Reg::ECX); // ESI:EDI holds the shift argument
							int addr1, addr2, addr3, addr4;
							encode_test(4, Reg::EDI, 0xFFFFFFFF);
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							addr1 = _dest.code.Length();
							encode_test(4, Reg::ESI, 0xFFFFFFC0);
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							addr2 = _dest.code.Length();
							encode_mov_reg_const(4, Reg::ECX, 1); // ESI is pure shift now, ECX is 1
							addr3 = _dest.code.Length();
							encode_test(4, Reg::ESI, 0x3F);
							_dest.code << 0x74; _dest.code << 0x00; // JZ
							addr4 = _dest.code.Length();
							if (opcode == TransformVectorShiftL) {
								encode_shift(4, shOp::SHL, Reg::EAX);
								encode_shift(4, shOp::RCL, Reg::EDX);
							} else if (opcode == TransformVectorShiftR) {
								encode_shift(4, shOp::SHR, Reg::EDX);
								encode_shift(4, shOp::RCR, Reg::EAX);
							} else if (opcode == TransformVectorShiftAL) {
								encode_shift(4, shOp::SAL, Reg::EAX);
								encode_shift(4, shOp::RCL, Reg::EDX);
							} else if (opcode == TransformVectorShiftAR) {
								encode_shift(4, shOp::SAR, Reg::EDX);
								encode_shift(4, shOp::RCR, Reg::EAX);
							}
							encode_add(Reg::ESI, -1);
							_dest.code << 0xEB;
							_dest.code << addr3 - _dest.code.Length() - 1; // LOOP
							_dest.code[addr1 - 1] = _dest.code.Length() - addr1;
							_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL) {
								encode_operation(4, arOp::XOR, Reg::EAX, Reg::EAX);
								encode_operation(4, arOp::XOR, Reg::EDX, Reg::EDX);
							} else if (opcode == TransformVectorShiftAR) {
								encode_shift(4, shOp::SAR, Reg::EDX, 31);
								encode_mov_reg_reg(4, Reg::EAX, Reg::EDX);
							}
							_dest.code[addr4 - 1] = _dest.code.Length() - addr4;
							_encode_restore(Reg::EDI, reg_in_use, !idle);
							_encode_restore(Reg::ESI, reg_in_use, !idle);
						}
						if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(8, 0), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
								encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::EDX);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionRegister) {
							if (!idle) {
								if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
								if (disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
							}
							disp->flags = DispositionRegister;
						}
					} else {
						if (!idle) {
							if (a2.flags & DispositionPointer) encode_mov_reg_mem(size, Reg::ECX, Reg::ECX);
							int mask, max_shift;
							shOp op;
							if (opcode == TransformVectorShiftL) op = shOp::SHL;
							else if (opcode == TransformVectorShiftR) op = shOp::SHR;
							else if (opcode == TransformVectorShiftAL) op = shOp::SAL;
							else if (opcode == TransformVectorShiftAR) op = shOp::SAR;
							if (size == 1) { mask = 0xFFFFFFF8; max_shift = 7; }
							else if (size == 2) { mask = 0xFFFFFFF0; max_shift = 15; }
							else if (size == 4) { mask = 0xFFFFFFE0; max_shift = 31; }
							encode_test(size, Reg::ECX, mask);
							int addr;
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							addr = _dest.code.Length();
							encode_shift(size, op, Reg::EAX);
							_dest.code << 0xEB; _dest.code << 0x00;
							_dest.code[addr - 1] = _dest.code.Length() - addr;
							addr = _dest.code.Length();
							if (opcode == TransformVectorShiftAR) encode_shift(size, op, Reg::EAX, max_shift);
							else encode_mov_reg_const(size, Reg::EAX, 0);
							_dest.code[addr - 1] = _dest.code.Length() - addr;
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != Reg::EAX) encode_mov_reg_reg(size, disp->reg, Reg::EAX);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 4;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(size, 0), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						}
					}
					_encode_restore(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && size == 8);
					_encode_restore(Reg::ECX, reg_in_use, !idle && disp->reg != Reg::ECX);
					_encode_restore(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
				}
				void _encode_arithmetics_compare(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
					auto opcode = node.self.index;
					int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					_encode_preserve(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
					_encode_preserve(Reg::ECX, reg_in_use, !idle && disp->reg != Reg::ECX);
					_encode_preserve(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && size == 8);
					_internal_disposition a1, a2;
					a1.reg = Reg::EAX; a2.reg = Reg::ECX;
					a1.size = a2.size = size;
					if (size == 8) a1.flags = a2.flags = DispositionPointer;
					else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
					_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg::EAX));
					_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg::EAX) | uint(Reg::ECX));
					if (size == 8) {
						bool wants_signed = (opcode == TransformIntegerSLE || opcode == TransformIntegerSGE || opcode == TransformIntegerSL || opcode == TransformIntegerSG);
						bool wants_lesser = (opcode == TransformIntegerULE || opcode == TransformIntegerUL || opcode == TransformIntegerSLE || opcode == TransformIntegerSL);
						bool wants_greater = (opcode == TransformIntegerUGE || opcode == TransformIntegerUG || opcode == TransformIntegerSGE || opcode == TransformIntegerSG);
						bool wants_equal = (opcode == TransformIntegerULE || opcode == TransformIntegerUGE || opcode == TransformIntegerSLE || opcode == TransformIntegerSGE);
						if (!idle) {
							_encode_preserve(Reg::ESI, reg_in_use, true);
							encode_mov_reg_mem(4, Reg::EDX, Reg::EAX, 4);
							encode_mov_reg_mem(4, Reg::EAX, Reg::EAX); // EAX:EDX - first
							encode_mov_reg_mem(4, Reg::ESI, Reg::ECX, 4);
							encode_mov_reg_mem(4, Reg::ECX, Reg::ECX); // ESI:ECX - second
							encode_operation(4, arOp::SUB, Reg::EAX, Reg::ECX);
							encode_operation(4, arOp::SBB, Reg::EDX, Reg::ESI); // EAX:EDX - the difference
							_dest.code << (wants_signed ? 0x7C : 0x72) << 0x00;
							int addr = _dest.code.Length();
							encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
							_dest.code << 0x75; _dest.code << 0x00; // JNZ
							int addr2 = _dest.code.Length();
							encode_mov_reg_const(4, Reg::EAX, wants_equal ? 1 : 0);
							_dest.code << 0xEB; _dest.code << 0x00;
							int addr3 = _dest.code.Length();
							_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							encode_mov_reg_const(4, Reg::EAX, wants_greater ? 1 : 0);
							_dest.code << 0xEB; _dest.code << 0x00;
							int addr4 = _dest.code.Length();
							_dest.code[addr - 1] = _dest.code.Length() - addr;
							encode_mov_reg_const(4, Reg::EAX, wants_lesser ? 1 : 0);
							_dest.code[addr3 - 1] = _dest.code.Length() - addr3;
							_dest.code[addr4 - 1] = _dest.code.Length() - addr4;
							_encode_restore(Reg::ESI, reg_in_use, true);
							encode_operation(4, arOp::XOR, Reg::EDX, Reg::EDX);
						}
						if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(8, 0), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
								encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::EDX);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionRegister) {
							if (!idle) {
								if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
								if (disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
							}
							disp->flags = DispositionRegister;
						}
					} else {
						if (!idle) {
							encode_operation(size, arOp::CMP, Reg::EAX, Reg::ECX, a2.flags & DispositionPointer);
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
							encode_operation(4, arOp::XOR, Reg::EAX, Reg::EAX);
							_dest.code << 0xEB; _dest.code << 5;
							encode_mov_reg_const(4, Reg::EAX, 1);
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != Reg::EAX) encode_mov_reg_reg(size, disp->reg, Reg::EAX);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 4;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(size, 0), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						}
					}
					_encode_restore(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && size == 8);
					_encode_restore(Reg::ECX, reg_in_use, !idle && disp->reg != Reg::ECX);
					_encode_restore(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
				}
				void _encode_arithmetics_core(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					auto opcode = node.self.index;
					if (opcode == TransformIntegerUResize || opcode == TransformIntegerSResize) {
						if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
						int in_size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						int out_size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
						_internal_disposition ld;
						ld.reg = Reg::EAX;
						ld.size = in_size;
						if (out_size <= 4 || in_size <= 4) ld.flags = DispositionRegister; else ld.flags = DispositionPointer;
						_encode_preserve(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
						if (ld.flags & DispositionRegister) {
							_encode_preserve(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && out_size > 4);
							if (!idle) {
								if (opcode == TransformIntegerUResize) {
									if (in_size < 4 && in_size < out_size) {
										auto delta = (4 - in_size) * 8;
										encode_shl(Reg::EAX, delta);
										encode_shr(Reg::EAX, delta);
									}
									if (out_size > 4) encode_operation(4, arOp::XOR, Reg::EDX, Reg::EDX);
								} else if (opcode == TransformIntegerSResize) {
									if (in_size < 2 && out_size >= 2) { _dest.code << 0x66; _dest.code << 0x98; }
									if (in_size < 4 && out_size >= 4) { _dest.code << 0x98; }
									if (in_size < 8 && out_size >= 8) { encode_mov_reg_reg(4, Reg::EDX, Reg::EAX); encode_shift(4, shOp::SAR, Reg::EDX, 31); }
								}
							}
							if (disp->flags & DispositionPointer) {
								*mem_load += _word_align(TH::MakeSize(out_size, 0));
								if (!idle) {
									int offs;
									_allocate_temporary(TH::MakeSize(out_size, 0), TH::MakeFinal(), &offs);
									if (out_size == 1) encode_mov_mem_reg(1, Reg::EBP, offs, Reg::AL);
									else if (out_size == 2) encode_mov_mem_reg(2, Reg::EBP, offs, Reg::AX);
									else if (out_size == 4) encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
									else if (out_size == 8) {
										encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
										encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::EDX);
									}
									encode_lea(disp->reg, Reg::EBP, offs);
								}
								disp->flags = DispositionPointer;
							} else if (disp->flags & DispositionRegister) {
								if (!idle) {
									if ((disp->flags & DispositionCompress) && out_size > 4) encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
									if (disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
								}
								disp->flags = DispositionRegister;
							}
							_encode_restore(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && out_size > 4);
						} else if (ld.flags & DispositionPointer) {
							if (disp->flags & DispositionPointer) {
								if (disp->reg != Reg::EAX && !idle) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
								disp->flags = DispositionPointer;
							} else if (disp->flags & DispositionRegister) {
								if (!idle) _encode_reg_load(disp->reg, Reg::EAX, 0, 8, disp->flags & DispositionCompress, reg_in_use);
								disp->flags = DispositionRegister;
							}
						}
						_encode_restore(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
					} else {
						int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
						if (opcode == TransformVectorInverse || opcode == TransformVectorIsZero || opcode == TransformVectorNotZero ||
							opcode == TransformIntegerInverse || opcode == TransformIntegerAbs) {
							if (node.inputs.Length() != 1 || node.input_specs.Length() != 1) throw InvalidArgumentException();
							_encode_preserve(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
							_encode_preserve(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && size > 4);
							_internal_disposition ld;
							ld.reg = Reg::EAX;
							ld.size = size;
							if (size == 8) ld.flags = DispositionPointer; else ld.flags = DispositionRegister;
							_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(Reg::EAX));
							if (size == 8) {
								if (!idle) {
									encode_mov_reg_mem(4, Reg::EDX, Reg::EAX, 4);
									encode_mov_reg_mem(4, Reg::EAX, Reg::EAX); // EDX:EAX - first
									if (opcode == TransformVectorInverse) {
										encode_invert(4, Reg::EAX);
										encode_invert(4, Reg::EDX);
									} else if (opcode == TransformVectorIsZero) {
										encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg::EAX, 1);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg::EAX, 0);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										encode_operation(4, arOp::XOR, Reg::EDX, Reg::EDX);
									} else if (opcode == TransformVectorNotZero) {
										encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg::EAX, 0);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(4, Reg::EAX, 1);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										encode_operation(4, arOp::XOR, Reg::EDX, Reg::EDX);
									} else if (opcode == TransformIntegerInverse) {
										_encode_preserve(Reg::ESI, reg_in_use, true);
										_encode_preserve(Reg::EDI, reg_in_use, true);
										encode_operation(4, arOp::XOR, Reg::ESI, Reg::ESI);
										encode_operation(4, arOp::XOR, Reg::EDI, Reg::EDI);
										encode_operation(4, arOp::SUB, Reg::ESI, Reg::EAX);
										encode_operation(4, arOp::SBB, Reg::EDI, Reg::EDX);
										encode_mov_reg_reg(4, Reg::EAX, Reg::ESI);
										encode_mov_reg_reg(4, Reg::EDX, Reg::EDI);
										_encode_restore(Reg::EDI, reg_in_use, true);
										_encode_restore(Reg::ESI, reg_in_use, true);
									} else if (opcode == TransformIntegerAbs) {
										encode_test(4, Reg::EDX, 0x80000000);
										_dest.code << 0x74; // JZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										_encode_preserve(Reg::ESI, reg_in_use, true);
										_encode_preserve(Reg::EDI, reg_in_use, true);
										encode_operation(4, arOp::XOR, Reg::ESI, Reg::ESI);
										encode_operation(4, arOp::XOR, Reg::EDI, Reg::EDI);
										encode_operation(4, arOp::SUB, Reg::ESI, Reg::EAX);
										encode_operation(4, arOp::SBB, Reg::EDI, Reg::EDX);
										encode_mov_reg_reg(4, Reg::EAX, Reg::ESI);
										encode_mov_reg_reg(4, Reg::EDX, Reg::EDI);
										_encode_restore(Reg::EDI, reg_in_use, true);
										_encode_restore(Reg::ESI, reg_in_use, true);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									}
								}
								if (disp->flags & DispositionPointer) {
									*mem_load += 8;
									if (!idle) {
										int offs;
										_allocate_temporary(TH::MakeSize(8, 0), TH::MakeFinal(), &offs);
										encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
										encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::EDX);
										encode_lea(disp->reg, Reg::EBP, offs);
									}
									disp->flags = DispositionPointer;
								} else if (disp->flags & DispositionRegister) {
									if (!idle) {
										if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
										if (disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
									}
									disp->flags = DispositionRegister;
								}
							} else {
								if (!idle) {
									if (opcode == TransformVectorInverse) {
										encode_invert(size, Reg::EAX);
									} else if (opcode == TransformVectorIsZero) {
										encode_test(size, Reg::EAX, 0xFFFFFFFF);
										int addr;
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, 1);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, 0);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									} else if (opcode == TransformVectorNotZero) {
										encode_test(size, Reg::EAX, 0xFFFFFFFF);
										int addr;
										_dest.code << 0x75; // JNZ
										_dest.code << 0x00;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, 0);
										_dest.code << 0xEB;
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, 1);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									} else if (opcode == TransformIntegerInverse) {
										encode_negative(size, Reg::EAX);
									} else if (opcode == TransformIntegerAbs) {
										_encode_preserve(Reg::EDX, reg_in_use, true);
										encode_mov_reg_reg(size, Reg::EDX, Reg::EAX);
										if (size == 1) encode_shift(size, shOp::SAR, Reg::EDX, 7);
										else if (size == 2) encode_shift(size, shOp::SAR, Reg::EDX, 15);
										else if (size == 4) encode_shift(size, shOp::SAR, Reg::EDX, 31);
										encode_operation(size, arOp::XOR, Reg::EAX, Reg::EDX);
										encode_operation(size, arOp::SUB, Reg::EAX, Reg::EDX);
										_encode_restore(Reg::EDX, reg_in_use, true);
									}
								}
								if (disp->flags & DispositionRegister) {
									if (!idle && disp->reg != Reg::EAX) encode_mov_reg_reg(size, disp->reg, Reg::EAX);
									disp->flags = DispositionRegister;
								} else if (disp->flags & DispositionPointer) {
									*mem_load += 4;
									if (!idle) {
										int offs;
										_allocate_temporary(TH::MakeSize(size, 0), TH::MakeFinal(), &offs);
										encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
										encode_lea(disp->reg, Reg::EBP, offs);
									}
									disp->flags = DispositionPointer;
								}
							}
							_encode_restore(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX && size > 4);
							_encode_restore(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
						} else {
							if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
							_encode_preserve(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
							_encode_preserve(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX);
							_encode_preserve(Reg::ESI, reg_in_use, !idle && disp->reg != Reg::ESI && size > 4);
							_encode_preserve(Reg::EDI, reg_in_use, !idle && disp->reg != Reg::EDI && size > 4);
							_internal_disposition a1, a2;
							a1.reg = Reg::EAX;
							a2.reg = Reg::EDX;
							a1.size = a2.size = size;
							if (size == 8) a1.flags = a2.flags = DispositionPointer;
							else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
							_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg::EAX));
							_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg::EAX) | uint(Reg::EDX));
							if (size == 8) {
								if (!idle) {
									encode_mov_reg_mem(4, Reg::ESI, Reg::EAX, 4);
									encode_mov_reg_mem(4, Reg::EAX, Reg::EAX); // ESI:EAX - first
									encode_mov_reg_mem(4, Reg::EDI, Reg::EDX, 4);
									encode_mov_reg_mem(4, Reg::EDX, Reg::EDX); // EDI:EDX - second
									if (opcode == TransformLogicalSame || opcode == TransformLogicalNotSame) {
										encode_operation(size, arOp::OR, Reg::EAX, Reg::ESI);
										encode_operation(size, arOp::OR, Reg::EDX, Reg::EDI);
										encode_test(size, Reg::EAX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x74 : 0x75); // JZ / JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_reg(size, Reg::EAX, Reg::EDX);
										_dest.code << 0xEB; // JMP
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_test(size, Reg::EDX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x75 : 0x74); // JNZ / JZ
										_dest.code << 0x00;
										int addr2 = _dest.code.Length();
										encode_operation(size, arOp::XOR, Reg::EAX, Reg::EAX);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
										encode_operation(size, arOp::XOR, Reg::EDX, Reg::EDX);
									} else if (opcode == TransformIntegerEQ || opcode == TransformIntegerNEQ) {
										encode_operation(4, arOp::XOR, Reg::ESI, Reg::EDI);
										encode_operation(4, arOp::XOR, Reg::EAX, Reg::EDX);
										encode_operation(4, arOp::OR, Reg::EAX, Reg::ESI);
										_dest.code << 0x74; // JZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, opcode == TransformIntegerEQ ? 0 : 1); // IF NON-ZERO
										_dest.code << 0xEB; // JMP
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, opcode == TransformIntegerEQ ? 1 : 0); // IF ZERO
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										encode_operation(4, arOp::XOR, Reg::ESI, Reg::ESI);
									} else if (opcode == TransformVectorAnd) {
										encode_operation(4, arOp::AND, Reg::EAX, Reg::EDX);
										encode_operation(4, arOp::AND, Reg::ESI, Reg::EDI);
									} else if (opcode == TransformVectorOr) {
										encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
										encode_operation(4, arOp::OR, Reg::ESI, Reg::EDI);
									} else if (opcode == TransformVectorXor) {
										encode_operation(4, arOp::XOR, Reg::EAX, Reg::EDX);
										encode_operation(4, arOp::XOR, Reg::ESI, Reg::EDI);
									} else if (opcode == TransformIntegerAdd) {
										encode_operation(4, arOp::ADD, Reg::EAX, Reg::EDX);
										encode_operation(4, arOp::ADC, Reg::ESI, Reg::EDI);
									} else if (opcode == TransformIntegerSubt) {
										encode_operation(4, arOp::SUB, Reg::EAX, Reg::EDX);
										encode_operation(4, arOp::SBB, Reg::ESI, Reg::EDI);
									}
								}
								if (disp->flags & DispositionPointer) {
									*mem_load += 8;
									if (!idle) {
										int offs;
										_allocate_temporary(TH::MakeSize(8, 0), TH::MakeFinal(), &offs);
										encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
										encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::ESI);
										encode_lea(disp->reg, Reg::EBP, offs);
									}
									disp->flags = DispositionPointer;
								} else if (disp->flags & DispositionRegister) {
									if (!idle) {
										if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg::EAX, Reg::ESI);
										if (disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
									}
									disp->flags = DispositionRegister;
								}
							} else {
								if (!idle) {
									if (opcode == TransformLogicalSame || opcode == TransformLogicalNotSame) {
										if (a2.flags & DispositionPointer) encode_mov_reg_mem(size, Reg::EDX, Reg::EDX);
										encode_test(size, Reg::EAX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x74 : 0x75); // JZ / JNZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_reg(size, Reg::EAX, Reg::EDX);
										_dest.code << 0xEB; // JMP
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_test(size, Reg::EDX, 0xFFFFFFFF);
										_dest.code << (opcode == TransformLogicalSame ? 0x75 : 0x74); // JNZ / JZ
										_dest.code << 0x00;
										int addr2 = _dest.code.Length();
										encode_operation(size, arOp::XOR, Reg::EAX, Reg::EAX);
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
									} else if (opcode == TransformIntegerEQ || opcode == TransformIntegerNEQ) {
										encode_operation(size, arOp::CMP, Reg::EAX, Reg::EDX, a2.flags & DispositionPointer);
										_dest.code << 0x74; // JZ
										_dest.code << 0x00;
										int addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, opcode == TransformIntegerEQ ? 0 : 1); // IF NON-ZERO
										_dest.code << 0xEB; // JMP
										_dest.code << 0x00;
										_dest.code[addr - 1] = _dest.code.Length() - addr;
										addr = _dest.code.Length();
										encode_mov_reg_const(size, Reg::EAX, opcode == TransformIntegerEQ ? 1 : 0); // IF ZERO
										_dest.code[addr - 1] = _dest.code.Length() - addr;
									} else if (opcode == TransformVectorAnd) encode_operation(size, arOp::AND, Reg::EAX, Reg::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformVectorOr) encode_operation(size, arOp::OR, Reg::EAX, Reg::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformVectorXor) encode_operation(size, arOp::XOR, Reg::EAX, Reg::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformIntegerAdd) encode_operation(size, arOp::ADD, Reg::EAX, Reg::EDX, a2.flags & DispositionPointer);
									else if (opcode == TransformIntegerSubt) encode_operation(size, arOp::SUB, Reg::EAX, Reg::EDX, a2.flags & DispositionPointer);
								}
								if (disp->flags & DispositionRegister) {
									if (!idle && disp->reg != Reg::EAX) encode_mov_reg_reg(size, disp->reg, Reg::EAX);
									disp->flags = DispositionRegister;
								} else if (disp->flags & DispositionPointer) {
									*mem_load += 4;
									if (!idle) {
										int offs;
										_allocate_temporary(TH::MakeSize(size, 0), TH::MakeFinal(), &offs);
										encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
										encode_lea(disp->reg, Reg::EBP, offs);
									}
									disp->flags = DispositionPointer;
								}
							}
							_encode_restore(Reg::EDI, reg_in_use, !idle && disp->reg != Reg::EDI && size > 4);
							_encode_restore(Reg::ESI, reg_in_use, !idle && disp->reg != Reg::ESI && size > 4);
							_encode_restore(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX);
							_encode_restore(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
						}
					}
				}
				void _encode_arithmetics_mul_div(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() != 2 || node.input_specs.Length() != 2) throw InvalidArgumentException();
					auto opcode = node.self.index;
					int size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					_encode_preserve(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
					_encode_preserve(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX);
					_encode_preserve(Reg::ECX, reg_in_use, !idle && disp->reg != Reg::ECX);
					_internal_disposition a1, a2;
					a1.reg = Reg::EAX; a2.reg = Reg::ECX;
					a1.size = a2.size = size;
					if (size == 8) a1.flags = a2.flags = DispositionPointer;
					else { a1.flags = DispositionRegister; a2.flags = DispositionAny; }
					_encode_tree_node(node.inputs[0], idle, mem_load, &a1, reg_in_use | uint(Reg::EAX));
					_encode_tree_node(node.inputs[1], idle, mem_load, &a2, reg_in_use | uint(Reg::EAX) | uint(Reg::ECX));
					if (size == 8) {
						if (!idle) {
							_encode_preserve(Reg::EBX, reg_in_use, true);
							_encode_preserve(Reg::ESI, reg_in_use, true);
							_encode_preserve(Reg::EDI, reg_in_use, true);
							if (opcode == TransformIntegerUMul || opcode == TransformIntegerSMul) {
								_encode_emulate_mul_64();
								encode_mov_reg_reg(4, Reg::EAX, Reg::ECX);
								encode_mov_reg_reg(4, Reg::EDX, Reg::EBX);
							} else if (opcode == TransformIntegerUDiv) {
								_encode_emulate_div_64();
							} else if (opcode == TransformIntegerSDiv) {
								encode_operation(4, arOp::XOR, Reg::EBX, Reg::EBX);
								_encode_emulate_collect_signum(Reg::EAX, Reg::EBX);
								_encode_emulate_collect_signum(Reg::ECX, Reg::EBX);
								encode_push(Reg::EBX);
								_encode_emulate_div_64();
								encode_pop(Reg::EBX);
								encode_add(Reg::ESP, 16);
								_encode_emulate_set_signum(Reg::EAX, Reg::EDX, Reg::EBX);
							} else if (opcode == TransformIntegerUMod) {
								_encode_emulate_div_64();
								encode_mov_reg_reg(4, Reg::EAX, Reg::ESI);
								encode_mov_reg_reg(4, Reg::EDX, Reg::EDI);
							} else if (opcode == TransformIntegerSMod) {
								encode_operation(4, arOp::XOR, Reg::EBX, Reg::EBX);
								_encode_emulate_collect_signum(Reg::EAX, Reg::EBX);
								_encode_emulate_collect_signum(Reg::ECX, Reg::EBX);
								encode_push(Reg::EBX);
								_encode_emulate_div_64();
								encode_pop(Reg::EBX);
								encode_add(Reg::ESP, 16);
								encode_mov_reg_reg(4, Reg::EAX, Reg::ESI);
								encode_mov_reg_reg(4, Reg::EDX, Reg::EDI);
								_encode_emulate_set_signum(Reg::EAX, Reg::EDX, Reg::EBX);
							}
							_encode_restore(Reg::EDI, reg_in_use, true);
							_encode_restore(Reg::ESI, reg_in_use, true);
							_encode_restore(Reg::EBX, reg_in_use, true);
						}
						if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(8, 0), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
								encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::EDX);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionRegister) {
							if (!idle) {
								if (disp->flags & DispositionCompress) encode_operation(4, arOp::OR, Reg::EAX, Reg::EDX);
								if (disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
							}
							disp->flags = DispositionRegister;
						}
					} else {
						Reg result;
						if (!idle) {
							if (opcode == TransformIntegerUMul) {
								result = Reg::EAX;
								encode_mul_div(size, mdOp::MUL, Reg::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerSMul) {
								result = Reg::EAX;
								encode_mul_div(size, mdOp::IMUL, Reg::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerUDiv) {
								result = Reg::EAX;
								if (size == 1) encode_operation(1, arOp::XOR, Reg::AH, Reg::AH);
								else encode_operation(size, arOp::XOR, Reg::EDX, Reg::EDX);
								encode_mul_div(size, mdOp::DIV, Reg::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerSDiv) {
								result = Reg::EAX;
								if (size > 1) {
									encode_mov_reg_reg(size, Reg::EDX, Reg::EAX);
									encode_shift(size, shOp::SAR, Reg::EDX, 31);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								encode_mul_div(size, mdOp::IDIV, Reg::ECX, a2.flags & DispositionPointer);
							} else if (opcode == TransformIntegerUMod) {
								result = Reg::EDX;
								if (size == 1) encode_operation(1, arOp::XOR, Reg::AH, Reg::AH);
								else encode_operation(size, arOp::XOR, Reg::EDX, Reg::EDX);
								encode_mul_div(size, mdOp::DIV, Reg::ECX, a2.flags & DispositionPointer);
								if (size == 1) encode_mov_reg_reg(1, Reg::DL, Reg::AH);
							} else if (opcode == TransformIntegerSMod) {
								result = Reg::EDX;
								if (size > 1) {
									encode_mov_reg_reg(size, Reg::EDX, Reg::EAX);
									encode_shift(size, shOp::SAR, Reg::EDX, 31);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								encode_mul_div(size, mdOp::IDIV, Reg::ECX, a2.flags & DispositionPointer);
								if (size == 1) encode_mov_reg_reg(1, Reg::DL, Reg::AH);
							}
						}
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != result) encode_mov_reg_reg(size, disp->reg, result);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 4;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(size, 0), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, result);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						}
					}
					_encode_restore(Reg::ECX, reg_in_use, !idle && disp->reg != Reg::ECX);
					_encode_restore(Reg::EDX, reg_in_use, !idle && disp->reg != Reg::EDX);
					_encode_restore(Reg::EAX, reg_in_use, !idle && disp->reg != Reg::EAX);
				}
				void _encode_arithmetics(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
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
				void _encode_logics(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformLogicalFork) {
						if (node.inputs.Length() != 3) throw InvalidArgumentException();
						_internal_disposition cond, none;
						cond.reg = Reg::ECX;
						cond.flags = DispositionRegister | DispositionCompress;
						cond.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						none.reg = Reg::NO;
						none.flags = DispositionDiscard;
						none.size = 0;
						if (!cond.size || cond.size > 8) throw InvalidArgumentException();
						_encode_preserve(cond.reg, reg_in_use, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &cond, reg_in_use | uint(cond.reg));
						if (!idle) {
							encode_test(min(cond.size, 4), cond.reg, 0xFFFFFFFF);
							_encode_restore(cond.reg, reg_in_use, true);
							_dest.code << 0x0F; _dest.code << 0x84; // JZ
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
						}
						int addr = _dest.code.Length();
						if (!idle) {
							int local_mem_load = 0, ssoffs;
							_encode_tree_node(node.inputs[1], true, &local_mem_load, &none, reg_in_use);
							_allocate_temporary(TH::MakeSize(local_mem_load, 0), TH::MakeFinal(), &ssoffs);
							_encode_open_scope(local_mem_load, true, ssoffs);
							_encode_tree_node(node.inputs[1], false, &local_mem_load, &none, reg_in_use);
							_encode_close_scope(reg_in_use);
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
						} else _encode_tree_node(node.inputs[1], true, mem_load, &none, reg_in_use);
						addr = _dest.code.Length();
						if (!idle) {
							int local_mem_load = 0, ssoffs;
							_encode_tree_node(node.inputs[2], true, &local_mem_load, &none, reg_in_use);
							_allocate_temporary(TH::MakeSize(local_mem_load, 0), TH::MakeFinal(), &ssoffs);
							_encode_open_scope(local_mem_load, true, ssoffs);
							_encode_tree_node(node.inputs[2], false, &local_mem_load, &none, reg_in_use);
							_encode_close_scope(reg_in_use);
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
						} else _encode_tree_node(node.inputs[2], true, mem_load, &none, reg_in_use);
						if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					} else {
						if (!node.inputs.Length()) throw InvalidArgumentException();
						Reg local = disp->reg != Reg::NO ? disp->reg : Reg::ECX;
						_encode_preserve(local, reg_in_use, local != disp->reg);
						Array<int> put_offs(0x10);
						uint min_size = 0;
						uint rv_size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
						if (!rv_size || rv_size > 8) throw InvalidArgumentException();
						for (int i = 0; i < node.inputs.Length(); i++) {
							auto & n = node.inputs[i];
							auto size = node.input_specs[i].size.num_bytes + WordSize * node.input_specs[i].size.num_words;
							if (!size || size > 8) throw InvalidArgumentException();
							if (size < min_size) min_size = size;
							_internal_disposition ld;
							ld.flags = DispositionRegister | DispositionCompress;
							ld.reg = local;
							ld.size = size;
							if (!idle) {
								int local_mem_load = 0, ssoffs;
								_encode_tree_node(n, true, &local_mem_load, &ld, reg_in_use | uint(local));
								_allocate_temporary(TH::MakeSize(local_mem_load, 0), TH::MakeFinal(), &ssoffs);
								_encode_open_scope(local_mem_load, true, ssoffs);
								_encode_tree_node(n, false, &local_mem_load, &ld, reg_in_use | uint(local));
								_encode_close_scope(reg_in_use | uint(local));
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
						for (auto & p : put_offs) *reinterpret_cast<int *>(_dest.code.GetBuffer() + p - 4) = _dest.code.Length() - p;
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
								int offs;
								_allocate_temporary(node.retval_spec.size, TH::MakeFinal(), &offs);
								encode_mov_mem_reg(4, Reg::EBP, offs, local);
								if (rv_size > 4) encode_mov_mem_reg(4, Reg::EBP, offs + 4, local);
								encode_lea(disp->reg, Reg::EBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						_encode_restore(local, reg_in_use, local != disp->reg);
					}
				}
				void _encode_general_call(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					CC conv;
					bool indirect, retval_byref, preserve_eax, retval_final;
					int first_arg, arg_no;
					if (node.self.ref_class == ReferenceTransform && node.self.index == TransformInvoke) {
						indirect = true; first_arg = 1; arg_no = node.inputs.Length() - 1;
					} else {
						indirect = false; first_arg = 0; arg_no = node.inputs.Length();
					}
					preserve_eax = disp->reg != Reg::EAX;
					retval_byref = _is_out_pass_by_reference(node.retval_spec);
					retval_final = node.retval_final.final.ref_class != ReferenceNull;
					SafePointer< Array<_argument_passage_info> > layout = _make_interface_layout(node.retval_spec, node.input_specs.GetBuffer() + first_arg, arg_no, &conv);
					_encode_preserve(Reg::EAX, reg_in_use, !idle && preserve_eax);
					_encode_preserve(Reg::EDX, reg_in_use, !idle);
					_encode_preserve(Reg::ECX, reg_in_use, !idle);
					int unpush = 0;
					for (auto & info : layout->InversedElements()) {
						if (info.index >= 0) {
							auto & spec = node.input_specs[info.index + first_arg];
							if (info.reg == Reg::NO) {
								_internal_disposition ld;
								ld.reg = Reg::EAX;
								ld.size = spec.size.num_bytes + WordSize * spec.size.num_words;
								ld.flags = info.indirect ? DispositionPointer : DispositionAny;
								_encode_tree_node(node.inputs[info.index + first_arg], idle, mem_load, &ld, reg_in_use | uint(Reg::ECX) | uint(Reg::EAX));
								if (info.indirect) {
									if (!idle) encode_push(ld.reg);
									unpush += 4;
								} else {
									if (ld.flags & DispositionPointer) {
										if (ld.size == 4) {
											unpush += 4;
											if (!idle) { encode_mov_reg_mem(4, Reg::EDX, Reg::EAX); encode_push(Reg::EDX); }
										} else if (ld.size > 0) {
											int sa = _word_align(spec.size); unpush += sa;
											if (!idle) {
												encode_add(Reg::ESP, -sa);
												encode_mov_reg_reg(4, Reg::EDX, Reg::ESP);
												_encode_blt(Reg::EDX, Reg::EAX, ld.size, reg_in_use | uint(Reg::ECX) | uint(Reg::EDX) | uint(Reg::EAX));
											}
										}
									} else if (ld.flags & DispositionRegister) {
										if (ld.size > 0) { if (!idle) encode_push(ld.reg); unpush += 4; }
									}
								}
							} else {
								_internal_disposition ld;
								ld.reg = info.reg;
								ld.size = spec.size.num_bytes + WordSize * spec.size.num_words;
								ld.flags = info.indirect ? DispositionPointer : DispositionRegister;
								_encode_tree_node(node.inputs[info.index + first_arg], idle, mem_load, &ld, reg_in_use | uint(Reg::ECX) | uint(Reg::EAX));
							}
						} else {
							*mem_load += _word_align(node.retval_spec.size);
							if (!idle) {
								int offs;
								_allocate_temporary(node.retval_spec.size, node.retval_final, &offs);
								encode_lea(Reg::EAX, Reg::EBP, offs);
								encode_push(Reg::EAX);
								unpush += 4;
							}
						}
					}
					if (indirect) {
						_internal_disposition ld;
						ld.flags = DispositionRegister;
						ld.reg = Reg::EAX;
						ld.size = 4;
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(Reg::ECX) | uint(Reg::EAX));
					} else {
						if (!idle) encode_put_addr_of(Reg::EAX, node.self);
					}
					if (!idle) {
						encode_call(Reg::EAX, false);
						if (_conv == CallingConvention::Unix && retval_byref) unpush -= 4;
						if (unpush && conv == CC::CDECL) encode_add(Reg::ESP, unpush);
					}
					int quant = _word_align(node.retval_spec.size);
					if (!retval_byref && (node.retval_spec.semantics == ArgumentSemantics::FloatingPoint || quant > 4 || retval_final)) {
						*mem_load += quant;
						if (!idle) {
							int offs;
							_allocate_temporary(node.retval_spec.size, node.retval_final, &offs);
							if (node.retval_spec.semantics == ArgumentSemantics::FloatingPoint) {
								encode_fstp(quant, Reg::EBP, offs);
							} else {
								encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
								if (quant > 4) encode_mov_mem_reg(4, Reg::EBP, offs + 4, Reg::EDX);
							}
							encode_lea(Reg::EAX, Reg::EBP, offs);
						}
						retval_byref = true;
						retval_final = false;
					}
					_encode_restore(Reg::ECX, reg_in_use, !idle);
					_encode_restore(Reg::EDX, reg_in_use, !idle);
					if ((disp->flags & DispositionPointer) && retval_byref) {
						if (!idle && disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && !retval_byref) {
						if (!idle && disp->reg != Reg::EAX) encode_mov_reg_reg(4, disp->reg, Reg::EAX);
						disp->flags = DispositionRegister;
					} else if ((disp->flags & DispositionPointer) && !retval_byref) {
						*mem_load += WordSize;
						if (!idle) {
							int offs;
							_allocate_temporary(TH::MakeSize(0, 1), node.retval_final, &offs);
							encode_mov_mem_reg(4, Reg::EBP, offs, Reg::EAX);
							encode_lea(disp->reg, Reg::EBP, offs);
						}
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && retval_byref) {
						if (!idle) {
							if (quant == 8 && (disp->flags & DispositionCompress)) {
								Reg local = disp->reg == Reg::EAX ? Reg::EDX : disp->reg;
								_encode_preserve(local, reg_in_use, local != disp->reg);
								encode_mov_reg_mem(4, local, Reg::EAX);
								encode_operation(4, arOp::OR, local, Reg::EAX, true, 4);
								if (local != disp->reg) encode_mov_reg_reg(4, disp->reg, local);
								_encode_restore(local, reg_in_use, local != disp->reg);
							} else encode_mov_reg_mem(4, disp->reg, Reg::EAX);
						}
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					}
					_encode_restore(Reg::EAX, reg_in_use, !idle && preserve_eax);
				}
				void _encode_tree_node(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.self.ref_flags & ReferenceFlagInvoke) {
						if (node.self.ref_class == ReferenceTransform) {
							if (node.self.index >= 0x001 && node.self.index < 0x010) {
								if (node.self.index == TransformFollowPointer) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									if (disp->flags & DispositionPointer) {
										_internal_disposition src;
										src.flags = DispositionRegister;
										src.reg = disp->reg;
										src.size = 4;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										_internal_disposition src;
										src.flags = DispositionRegister;
										src.reg = disp->reg;
										src.size = 4;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										if (!idle) _encode_reg_load(disp->reg, disp->reg, 0, disp->size, disp->flags & DispositionCompress, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformTakePointer) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									if (disp->flags & DispositionRegister) {
										_internal_disposition src;
										src.flags = DispositionPointer;
										src.reg = disp->reg;
										src.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionPointer) {
										_internal_disposition src;
										src.flags = DispositionPointer;
										src.reg = disp->reg;
										src.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										*mem_load += _word_align(TH::MakeSize(0, 1));
										if (!idle) {
											int offset;
											_allocate_temporary(TH::MakeSize(0, 1), TH::MakeFinal(), &offset);
											encode_mov_mem_reg(4, Reg::EBP, offset, src.reg);
											encode_lea(src.reg, Reg::EBP, offset);
										}
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformAddressOffset) {
									if (node.inputs.Length() < 2 || node.inputs.Length() > 3) throw InvalidArgumentException();
									_internal_disposition base;
									base.flags = DispositionPointer;
									base.reg = Reg::ESI;
									base.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									_encode_preserve(base.reg, reg_in_use, !idle && disp->reg != Reg::ESI);
									_encode_tree_node(node.inputs[0], idle, mem_load, &base, reg_in_use | uint(base.reg));
									if (node.inputs[1].self.ref_class == ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint offset = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (!idle && offset * scale) encode_add(Reg::ESI, offset * scale);
									} else if (node.inputs[1].self.ref_class != ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										_encode_preserve(Reg::EAX, reg_in_use, !idle);
										_encode_preserve(Reg::EDX, reg_in_use, !idle);
										_encode_preserve(Reg::ECX, reg_in_use, !idle && scale > 1);
										if (scale) {
											_internal_disposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg::EAX;
											offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
											if (offset.size != 4) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg::ESI) | uint(Reg::EAX) | uint(Reg::EDX) | uint(Reg::ECX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(4, Reg::ECX, scale);
													encode_mul_div(4, mdOp::MUL, Reg::ECX);
												}
												encode_operation(4, arOp::ADD, Reg::ESI, Reg::EAX);
											}
										}
										_encode_restore(Reg::ECX, reg_in_use, !idle && scale > 1);
										_encode_restore(Reg::EDX, reg_in_use, !idle);
										_encode_restore(Reg::EAX, reg_in_use, !idle);
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										_encode_preserve(Reg::EAX, reg_in_use, !idle);
										_encode_preserve(Reg::EDX, reg_in_use, !idle);
										_encode_preserve(Reg::ECX, reg_in_use, !idle && scale > 1);
										if (scale) {
											_internal_disposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg::EAX;
											offset.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
											if (offset.size != 4) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(Reg::ESI) | uint(Reg::EAX) | uint(Reg::EDX) | uint(Reg::ECX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(4, Reg::ECX, scale);
													encode_mul_div(4, mdOp::MUL, Reg::ECX);
												}
												encode_operation(4, arOp::ADD, Reg::ESI, Reg::EAX);
											}
										}
										_encode_restore(Reg::ECX, reg_in_use, !idle && scale > 1);
										_encode_restore(Reg::EDX, reg_in_use, !idle);
										_encode_restore(Reg::EAX, reg_in_use, !idle);
									} else {
										_encode_preserve(Reg::EAX, reg_in_use, !idle);
										_encode_preserve(Reg::EDX, reg_in_use, !idle);
										_encode_preserve(Reg::ECX, reg_in_use, !idle);
										_internal_disposition offset;
										offset.flags = DispositionRegister;
										offset.reg = Reg::EAX;
										offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										_internal_disposition scale;
										scale.flags = DispositionRegister;
										scale.reg = Reg::ECX;
										scale.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (offset.size != 4 || scale.size != 4) throw InvalidArgumentException();
										_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg::ESI) | uint(Reg::EAX) | uint(Reg::EDX) | uint(Reg::ECX));
										_encode_tree_node(node.inputs[2], idle, mem_load, &scale, reg_in_use | uint(Reg::ESI) | uint(Reg::EAX) | uint(Reg::EDX) | uint(Reg::ECX));
										if (!idle) {
											encode_mul_div(4, mdOp::MUL, Reg::ECX);
											encode_operation(4, arOp::ADD, Reg::ESI, Reg::EAX);
										}
										_encode_restore(Reg::ECX, reg_in_use, !idle);
										_encode_restore(Reg::EDX, reg_in_use, !idle);
										_encode_restore(Reg::EAX, reg_in_use, !idle);
									}
									if (disp->flags & DispositionPointer) {
										if (disp->reg != Reg::ESI && !idle) encode_mov_reg_reg(4, disp->reg, Reg::ESI);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) _encode_reg_load(disp->reg, Reg::ESI, 0, disp->size, disp->flags & DispositionCompress, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
									_encode_restore(base.reg, reg_in_use, !idle && disp->reg != Reg::ESI);
								} else if (node.self.index == TransformBlockTransfer) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									uint size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									_internal_disposition dest_d, src_d;
									dest_d.reg = Reg::EDI;
									dest_d.size = size;
									dest_d.flags = DispositionPointer;
									src_d.flags = size > 4 ? DispositionPointer : DispositionAny;
									src_d.size = size;
									src_d.reg = Reg::EAX;
									_encode_preserve(Reg::EDI, reg_in_use, !idle && disp->reg != Reg::EDI);
									_encode_preserve(Reg::EAX, reg_in_use, !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &dest_d, reg_in_use | uint(Reg::EDI) | uint(Reg::EAX));
									_encode_tree_node(node.inputs[1], idle, mem_load, &src_d, reg_in_use | uint(Reg::EDI) | uint(Reg::EAX));
									if (src_d.flags & DispositionPointer) {
										if (!idle) _encode_blt(dest_d.reg, src_d.reg, size, reg_in_use | uint(Reg::EDI) | uint(Reg::EAX));
									} else if (!idle)  {
										if (size == 1) encode_mov_mem_reg(1, Reg::EDI, Reg::AL);
										else if (size == 2) encode_mov_mem_reg(2, Reg::EDI, Reg::AX);
										else if (size == 3) {
											encode_mov_mem_reg(2, Reg::EDI, Reg::AX);
											encode_shr(Reg::EAX, 16);
											encode_mov_mem_reg(1, Reg::EDI, 2, Reg::AL);
										} else if (size == 4) encode_mov_mem_reg(4, Reg::EDI, Reg::EAX);
									}
									_encode_restore(Reg::EAX, reg_in_use, !idle);
									if ((disp->flags & DispositionRegister) && !(disp->flags & DispositionPointer)) {
										if (!idle) _encode_reg_load(disp->reg, Reg::EDI, 0, disp->size, disp->flags & DispositionCompress, reg_in_use | uint(Reg::EDI));
										disp->flags = DispositionRegister;
									} else {
										if (disp->flags & DispositionAny) {
											disp->flags = DispositionPointer;
											if (disp->reg != Reg::EDI) encode_mov_reg_reg(4, disp->reg, Reg::EDI);
										}
									}
									_encode_restore(Reg::EDI, reg_in_use, !idle && disp->reg != Reg::EDI);
								} else if (node.self.index == TransformInvoke) {
									_encode_general_call(node, idle, mem_load, disp, reg_in_use);
								} else if (node.self.index == TransformTemporary) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
									*mem_load += _word_align(node.retval_spec.size);
									_internal_disposition ld;
									ld.flags = DispositionDiscard;
									ld.reg = Reg::NO;
									ld.size = 0;
									int offset;
									if (!idle) {
										_allocate_temporary(node.retval_spec.size, node.retval_final, &offset);
										_local_disposition local;
										local.ebp_offset = offset;
										local.size = size;
										_init_locals.Push(local);
									}
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use);
									if (!idle) _init_locals.Pop();
									if (disp->flags & DispositionPointer) {
										if (!idle) encode_lea(disp->reg, Reg::EBP, offset);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) _encode_reg_load(disp->reg, Reg::EBP, offset, disp->size, disp->flags & DispositionCompress, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformBreakIf) {
									if (node.inputs.Length() != 3) throw InvalidArgumentException();
									_encode_tree_node(node.inputs[0], idle, mem_load, disp, reg_in_use);
									_internal_disposition ld;
									ld.flags = DispositionRegister | DispositionCompress;
									ld.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
									ld.reg = Reg::ECX;
									_encode_preserve(ld.reg, reg_in_use, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									if (!idle) {
										encode_test(min(ld.size, 4), ld.reg, 0xFFFFFFFF);
										_dest.code << 0x0F; _dest.code << 0x84; // JZ
										_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
										int addr = _dest.code.Length();
										auto scope = _scopes.GetLast();
										while (scope && !scope->GetValue().shift_esp) scope = scope->GetPrevious();
										if (!scope) throw InvalidStateException();
										encode_lea(Reg::ESP, Reg::EBP, scope->GetValue().frame_base);
										encode_scope_unroll(_current_instruction, _current_instruction + 1 + int(node.input_specs[2].size.num_bytes));
										_dest.code << 0xE9; // JMP
										_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
										_jump_reloc_struct rs;
										rs.machine_offset_at = _dest.code.Length() - 4;
										rs.machine_offset_relative_to = _dest.code.Length();
										rs.xasm_offset_jump_to = _current_instruction + 1 + int(node.input_specs[2].size.num_bytes);
										_jump_reloc << rs;
										*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
									}
									_encode_restore(ld.reg, reg_in_use, !idle);
								} else throw InvalidArgumentException();
							} else if (node.self.index >= 0x010 && node.self.index < 0x013) {
								_encode_logics(node, idle, mem_load, disp, reg_in_use);
							} else if (node.self.index >= 0x013 && node.self.index < 0x050) {
								_encode_arithmetics(node, idle, mem_load, disp, reg_in_use);
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
								Reg ld = Reg::EAX;
								if (ld == disp->reg) ld = Reg::EDX;
								_encode_preserve(ld, reg_in_use, true);
								encode_put_addr_of(ld, node.self);
								_encode_reg_load(disp->reg, ld, 0, disp->size, disp->flags & DispositionCompress, reg_in_use | uint(ld));
								_encode_restore(ld, reg_in_use, true);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					}
				}
				void _encode_expression_evaluation(const ExpressionTree & tree, Reg retval_copy)
				{
					if (retval_copy != Reg::NO && _is_out_pass_by_reference(tree.retval_spec)) throw InvalidArgumentException();
					int _temp_storage = 0;
					_internal_disposition disp;
					disp.reg = retval_copy;
					if (retval_copy == Reg::NO) {
						disp.flags = DispositionDiscard;
						disp.size = 0;
					} else {
						disp.flags = DispositionRegister | DispositionCompress;
						disp.size = 1;
					}
					_encode_tree_node(tree, true, &_temp_storage, &disp, uint(retval_copy));
					_encode_open_scope(_temp_storage, true, 0);
					_encode_tree_node(tree, false, &_temp_storage, &disp, uint(retval_copy));
					_encode_close_scope();
				}
			public:
				EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src) : _conv(conv), _dest(dest), _src(src), _org_inst_offsets(1), _jump_reloc(0x100), _inputs(1)
				{
					_inputs.SetLength(src.inputs.Length());
					_org_inst_offsets.SetLength(src.instset.Length());
					_current_instruction = 0;
				}
				void encode_debugger_trap(void) { _dest.code << 0xCC; }
				void encode_pure_ret(int bytes_unroll)
				{
					if (bytes_unroll) {
						_dest.code << 0xC2;
						_dest.code << (bytes_unroll);
						_dest.code << (bytes_unroll >> 8);
					} else _dest.code << 0xC3;
				}
				void encode_mov_reg_reg(uint quant, Reg dest, Reg src)
				{
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src);
					if (quant == 4) {
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					} else if (quant == 1) {
						_dest.code << 0x88;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					}
				}
				void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr)
				{
					if (src_ptr == Reg::EBP || src_ptr == Reg::ESP) return;
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src_ptr);
					if (quant == 4) {
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					} else if (quant == 1) {
						_dest.code << 0x8A;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					}
				}
				void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, int src_offset)
				{
					if (src_ptr == Reg::ESP) return;
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src_ptr);
					uint8 mode;
					if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
					if (quant == 4) {
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 1) {
						_dest.code << 0x8A;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					}
					if (mode == 0x02) {
						_dest.code << int8(src_offset);
						_dest.code << int8(src_offset >> 8);
						_dest.code << int8(src_offset >> 16);
						_dest.code << int8(src_offset >> 24);
					} else _dest.code << int8(src_offset);
				}
				void encode_mov_reg_const(uint quant, Reg dest, uint32 value)
				{
					auto di = _regular_register_code(dest);
					auto di_lo = di & 0x07;
					if (quant == 4) {
						_dest.code << 0xB8 + di_lo;
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0xB8 + di_lo;
					} else if (quant == 1) {
						_dest.code << 0xB0 + di_lo;
					}
					for (uint q = 0; q < quant; q++) { _dest.code << uint8(value); value >>= 8; }
				}
				void encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg src)
				{
					if (dest_ptr == Reg::EBP || dest_ptr == Reg::ESP) return;
					auto di = _regular_register_code(dest_ptr);
					auto si = _regular_register_code(src);
					if (quant == 4) {
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					} else if (quant == 1) {
						_dest.code << 0x88;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					}
				}
				void encode_mov_mem_reg(uint quant, Reg dest_ptr, int dest_offset, Reg src)
				{
					if (dest_ptr == Reg::ESP) return;
					auto di = _regular_register_code(dest_ptr);
					auto si = _regular_register_code(src);
					uint8 mode;
					if (dest_offset >= -128 && dest_offset < 128) mode = 0x01; else mode = 0x02;
					if (quant == 4) {
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, mode, di & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, mode, di & 0x07);
					} else if (quant == 1) {
						_dest.code << 0x88;
						_dest.code << _make_mod(si & 0x07, mode, di & 0x07);
					}
					if (mode == 0x02) {
						_dest.code << int8(dest_offset);
						_dest.code << int8(dest_offset >> 8);
						_dest.code << int8(dest_offset >> 16);
						_dest.code << int8(dest_offset >> 24);
					} else _dest.code << int8(dest_offset);
				}
				void encode_lea(Reg dest, Reg src_ptr, int src_offset)
				{
					if (src_ptr == Reg::ESP) return;
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src_ptr);
					uint8 mode;
					if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
					_dest.code << 0x8D;
					_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					if (mode == 0x02) {
						_dest.code << int8(src_offset);
						_dest.code << int8(src_offset >> 8);
						_dest.code << int8(src_offset >> 16);
						_dest.code << int8(src_offset >> 24);
					} else _dest.code << int8(src_offset);
				}
				void encode_push(Reg reg)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << (0x50 + (rc & 0x07));
				}
				void encode_pop(Reg reg)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << (0x58 + (rc & 0x07));
				}
				void encode_add(Reg reg, int literal)
				{
					uint rc = _regular_register_code(reg);
					if (literal >= -128 && literal <= 127) {
						_dest.code << 0x83;
						_dest.code << _make_mod(0, 0x3, rc & 0x07);
						_dest.code << int8(literal);
					} else {
						_dest.code << 0x81;
						_dest.code << _make_mod(0, 0x3, rc & 0x07);
						_dest.code << int8(literal);
						_dest.code << int8(literal >> 8);
						_dest.code << int8(literal >> 16);
						_dest.code << int8(literal >> 24);
					}
				}
				void encode_test(uint quant, Reg reg, int literal)
				{
					auto ri = _regular_register_code(reg);
					if (quant == 4) {
						_dest.code << 0xF7;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
						_dest.code << ((literal >> 8) & 0xFF);
						_dest.code << ((literal >> 16) & 0xFF);
						_dest.code << ((literal >> 24) & 0xFF);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0xF7;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
						_dest.code << ((literal >> 8) & 0xFF);
					} else if (quant == 1) {
						_dest.code << 0xF6;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
					}
				}
				void encode_fld(uint quant, Reg src_ptr, int src_offset)
				{
					if (quant == 4) _dest.code << 0xD9;
					else if (quant == 8) _dest.code << 0xDD;
					uint8 mode;
					if (src_offset) {
						if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
					} else mode = 0x00;
					_dest.code << _make_mod(0, mode, _regular_register_code(src_ptr));
					if (mode == 0x02) {
						_dest.code << int8(src_offset);
						_dest.code << int8(src_offset >> 8);
						_dest.code << int8(src_offset >> 16);
						_dest.code << int8(src_offset >> 24);
					} else if (mode == 0x01) _dest.code << int8(src_offset);
				}
				void encode_fstp(uint quant, Reg src_ptr, int src_offset)
				{
					if (quant == 4) _dest.code << 0xD9;
					else if (quant == 8) _dest.code << 0xDD;
					uint8 mode;
					if (src_offset) {
						if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
					} else mode = 0x00;
					_dest.code << _make_mod(3, mode, _regular_register_code(src_ptr));
					if (mode == 0x02) {
						_dest.code << int8(src_offset);
						_dest.code << int8(src_offset >> 8);
						_dest.code << int8(src_offset >> 16);
						_dest.code << int8(src_offset >> 24);
					} else if (mode == 0x01) _dest.code << int8(src_offset);
				}
				void encode_operation(uint quant, arOp op, Reg to, Reg value_ptr, bool indirect = false, int value_offset = 0)
				{
					if (value_ptr == Reg::ESP || value_ptr == Reg::EBP) return;
					auto di = _regular_register_code(to);
					auto si = _regular_register_code(value_ptr);
					auto o = uint(op);
					uint8 mode;
					if (indirect) {
						if (value_offset == 0) mode = 0x00;
						else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
						else mode = 0x02;
					} else mode = 0x03;
					if (quant == 4) {
						_dest.code << (o + 1);
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << (o + 1);
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 1) {
						_dest.code << o;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					}
					if (mode == 0x02) {
						_dest.code << int8(value_offset);
						_dest.code << int8(value_offset >> 8);
						_dest.code << int8(value_offset >> 16);
						_dest.code << int8(value_offset >> 24);
					} else if (mode == 0x01) _dest.code << int8(value_offset);
				}
				void encode_mul_div(uint quant, mdOp op, Reg value_ptr, bool indirect = false, int value_offset = 0)
				{
					if (value_ptr == Reg::ESP || value_ptr == Reg::EBP) return;
					auto si = _regular_register_code(value_ptr);
					auto o = uint(op);
					uint8 mode;
					if (indirect) {
						if (value_offset == 0) mode = 0x00;
						else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
						else mode = 0x02;
					} else mode = 0x03;
					if (quant == 4) {
						_dest.code << 0xF7;
						_dest.code << _make_mod(uint(op), mode, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0xF7;
						_dest.code << _make_mod(uint(op), mode, si & 0x07);
					} else if (quant == 1) {
						_dest.code << 0xF6;
						_dest.code << _make_mod(uint(op), mode, si & 0x07);
					}
					if (mode == 0x02) {
						_dest.code << int8(value_offset);
						_dest.code << int8(value_offset >> 8);
						_dest.code << int8(value_offset >> 16);
						_dest.code << int8(value_offset >> 24);
					} else if (mode == 0x01) _dest.code << int8(value_offset);
				}
				void encode_invert(uint quant, Reg reg)
				{
					auto ri = _regular_register_code(reg);
					if (quant == 4) {
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					} else if (quant == 1) {
						_dest.code << 0xF6;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					}
				}
				void encode_negative(uint quant, Reg reg)
				{
					auto ri = _regular_register_code(reg);
					if (quant == 4) {
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					} else if (quant == 1) {
						_dest.code << 0xF6;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					}
				}
				void encode_shift(uint quant, shOp op, Reg reg, int by = 0, bool indirect = false, int value_offset = 0)
				{
					uint8 oc, ocx;
					auto ri = _regular_register_code(reg);
					if (by) oc = 0xC0; else oc = 0xD2;
					if (op == shOp::ROL) ocx = 0x0;
					else if (op == shOp::ROR) ocx = 0x1;
					else if (op == shOp::RCL) ocx = 0x2;
					else if (op == shOp::RCR) ocx = 0x3;
					else if (op == shOp::SHL) ocx = 0x4;
					else if (op == shOp::SHR) ocx = 0x5;
					else if (op == shOp::SAL) ocx = 0x4;
					else if (op == shOp::SAR) ocx = 0x7;
					uint8 mode;
					if (indirect) {
						if (value_offset == 0) mode = 0x00;
						else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
						else mode = 0x02;
					} else mode = 0x03;
					if (quant == 4) {
						_dest.code << (oc + 1);
						_dest.code << _make_mod(ocx, mode, ri & 0x7);
					} else if (quant == 2) {
						_dest.code << 0x66;
						_dest.code << (oc + 1);
						_dest.code << _make_mod(ocx, mode, ri & 0x7);
					} else if (quant == 1) {
						_dest.code << oc;
						_dest.code << _make_mod(ocx, mode, ri & 0x7);
					}
					if (mode == 0x02) {
						_dest.code << int8(value_offset);
						_dest.code << int8(value_offset >> 8);
						_dest.code << int8(value_offset >> 16);
						_dest.code << int8(value_offset >> 24);
					} else if (mode == 0x01) _dest.code << int8(value_offset);
					if (by) _dest.code << by;
				}
				void encode_shl(Reg reg, int bits)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << 0xC1;
					_dest.code << _make_mod(0x4, 0x3, rc & 0x7);
					_dest.code << bits;
				}
				void encode_shr(Reg reg, int bits)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << 0xC1;
					_dest.code << _make_mod(0x5, 0x3, rc & 0x7);
					_dest.code << bits;
				}
				void encode_call(Reg func_ptr, bool indirect)
				{
					uint rc = _regular_register_code(func_ptr);
					_dest.code << 0xFF;
					_dest.code << _make_mod(0x2, indirect ? 0x00 : 0x03, rc & 0x07);
				}
				void encode_put_addr_of(Reg dest, const ObjectReference & value)
				{
					if (value.ref_class == ReferenceNull) {
						encode_mov_reg_const(4, dest, 0);
					} else if (value.ref_class == ReferenceExternal) {
						encode_mov_reg_const(4, dest, 0);
						_refer_object_at(_src.extrefs[value.index], _dest.code.Length() - 4);
					} else if (value.ref_class == ReferenceData) {
						encode_mov_reg_const(4, dest, value.index);
						_relocate_data_at(_dest.code.Length() - 4);
					} else if (value.ref_class == ReferenceCode) {
						encode_mov_reg_const(4, dest, value.index);
						_relocate_code_at(_dest.code.Length() - 4);
					} else if (value.ref_class == ReferenceArgument) {
						auto & arg = _inputs[value.index];
						if (arg.indirect) encode_mov_reg_mem(4, dest, Reg::EBP, arg.rbp_offset);
						else encode_lea(dest, Reg::EBP, arg.rbp_offset);
					} else if (value.ref_class == ReferenceRetVal) {
						if (_retval.indirect) encode_mov_reg_mem(4, dest, Reg::EBP, _retval.rbp_offset);
						else encode_lea(dest, Reg::EBP, _retval.rbp_offset);
					} else if (value.ref_class == ReferenceLocal) {
						bool found = false;
						for (auto & scp : _scopes) if (scp.first_local_no <= value.index && value.index < scp.first_local_no + scp.locals.Length()) {
							auto & local = scp.locals[value.index - scp.first_local_no];
							encode_lea(dest, Reg::EBP, local.ebp_offset);
							found = true;
							break;
						}
						if (!found) throw InvalidArgumentException();
					} else if (value.ref_class == ReferenceInit) {
						if (!_init_locals.IsEmpty()) {
							auto & local = _init_locals.GetLast()->GetValue();
							encode_lea(dest, Reg::EBP, local.ebp_offset);
						} else throw InvalidArgumentException();
					} else throw InvalidArgumentException();
				}
				void encode_function_prologue(void)
				{
					_stack_clear_size = 0;
					SafePointer< Array<_argument_passage_info> > api = _make_interface_layout(_src.retval, _src.inputs.GetBuffer(), _src.inputs.Length(), &_conv_eval);
					_scope_frame_base = _unroll_base = -WordSize * 3;
					encode_push(Reg::EBP);
					encode_mov_reg_reg(4, Reg::EBP, Reg::ESP);
					encode_push(Reg::EBX);
					encode_push(Reg::EDI);
					encode_push(Reg::ESI);
					if (!_is_out_pass_by_reference(_src.retval)) {
						int size = _word_align(_src.retval.size);
						if (_src.retval.semantics == ArgumentSemantics::FloatingPoint) {
							_retval.bound_to = Reg::ST0;
							_retval.bound_to_hi_dword = Reg::NO;
						} else {
							_retval.bound_to = Reg::EAX;
							_retval.bound_to_hi_dword = Reg::NO;
							if (size > WordSize) _retval.bound_to_hi_dword = Reg::EDX;
						}
						_retval.indirect = false;
						_scope_frame_base -= size;
						_unroll_base -= size;
						_retval.rbp_offset = _scope_frame_base;
						encode_add(Reg::ESP, -size);
					}
					int arg_offs = 2 * WordSize;
					for (int i = 0; i < api->Length(); i++) {
						auto & info = api->ElementAt(i);
						if (info.index >= 0) {
							if (info.reg != Reg::NO) {
								_scope_frame_base -= WordSize;
								_inputs[info.index].bound_to = info.reg;
								_inputs[info.index].bound_to_hi_dword = Reg::NO;
								_inputs[info.index].rbp_offset = _scope_frame_base;
								_inputs[info.index].indirect = info.indirect;
								encode_push(info.reg);
							} else {
								_inputs[info.index].bound_to = Reg::NO;
								_inputs[info.index].bound_to_hi_dword = Reg::NO;
								_inputs[info.index].rbp_offset = arg_offs;
								_inputs[info.index].indirect = info.indirect;
								auto size = info.indirect ? WordSize : _word_align(_src.inputs[info.index].size);
								arg_offs += size;
								_stack_clear_size += size;
							}
						} else {
							_retval.bound_to = Reg::NO;
							_retval.bound_to_hi_dword = Reg::NO;
							_retval.rbp_offset = arg_offs;
							_retval.indirect = true;
							arg_offs += WordSize;
							_stack_clear_size += WordSize;
						}
					}
				}
				void encode_function_epilogue(void)
				{
					if (_unroll_base != _scope_frame_base) encode_lea(Reg::ESP, Reg::EBP, _unroll_base);
					if (_retval.indirect) {
						encode_mov_reg_mem(4, Reg::EAX, Reg::EBP, _retval.rbp_offset);
					} else {
						if (_retval.bound_to == Reg::ST0) {
							auto quant = _word_align(_src.retval.size);
							encode_fld(quant, Reg::EBP, _retval.rbp_offset);
							encode_add(Reg::ESP, quant);
						} else {
							if (_retval.bound_to != Reg::NO) encode_pop(_retval.bound_to);
							if (_retval.bound_to_hi_dword != Reg::NO) encode_pop(_retval.bound_to_hi_dword);
						}
					}
					encode_pop(Reg::ESI);
					encode_pop(Reg::EDI);
					encode_pop(Reg::EBX);
					encode_pop(Reg::EBP);
					if (_conv_eval == CC::THISCALL) encode_pure_ret(_stack_clear_size);
					else if (_conv == CallingConvention::Unix && _retval.indirect) encode_pure_ret(4);
					else encode_pure_ret(0);
				}
				void encode_scope_unroll(int inst_current, int inst_jump_to)
				{
					int current_level = 0;
					int ref_level = 0;
					auto current_scope = _scopes.GetLast();
					while (current_scope && current_scope->GetValue().temporary) {
						_encode_finalize_scope(current_scope->GetValue());
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
									_encode_finalize_scope(current_scope->GetValue());
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
									_encode_finalize_scope(current_scope->GetValue());
									current_scope = current_scope->GetPrevious();
								}
							} else if (_src.instset[i].opcode == OpcodeCloseScope) {
								current_level++;
							}
						}
					}
				}
				void process_encoding(void)
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
							_encode_open_scope(required_size, false, 0);
						} else if (inst.opcode == OpcodeCloseScope) {
							_encode_close_scope();
						} else if (inst.opcode == OpcodeExpression) {
							_encode_expression_evaluation(inst.tree, Reg::NO);
						} else if (inst.opcode == OpcodeNewLocal) {
							auto current_scope_ptr = _scopes.GetLast();
							if (!current_scope_ptr) throw InvalidArgumentException();
							auto & scope = current_scope_ptr->GetValue();
							_local_disposition new_var;
							ZeroMemory(&new_var.finalizer.final, sizeof(new_var.finalizer.final));
							new_var.size = inst.attachment.num_bytes + WordSize * inst.attachment.num_words;
							auto size_padded = _word_align(inst.attachment);
							new_var.ebp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
							scope.frame_size_unused -= size_padded;
							if (scope.frame_size_unused < 0) throw InvalidArgumentException();
							_init_locals.Push(new_var);
							_encode_expression_evaluation(inst.tree, Reg::NO);
							_init_locals.Pop();
							new_var.finalizer = inst.attachment_final;
							scope.locals << new_var;
						} else if (inst.opcode == OpcodeUnconditionalJump) {
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							_jump_reloc_struct rs;
							rs.machine_offset_at = _dest.code.Length() - 4;
							rs.machine_offset_relative_to = _dest.code.Length();
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
						} else if (inst.opcode == OpcodeConditionalJump) {
							_encode_expression_evaluation(inst.tree, Reg::ECX);
							encode_test(1, Reg::ECX, 0xFF);
							_dest.code << 0x0F; _dest.code << 0x84; // JZ
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							int addr = _dest.code.Length();
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							_dest.code << 0xE9; // JMP
							_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
							*reinterpret_cast<int *>(_dest.code.GetBuffer() + addr - 4) = _dest.code.Length() - addr;
							_jump_reloc_struct rs;
							rs.machine_offset_at = _dest.code.Length() - 4;
							rs.machine_offset_relative_to = _dest.code.Length();
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
						} else if (inst.opcode == OpcodeControlReturn) {
							_encode_expression_evaluation(inst.tree, Reg::NO);
							for (auto & scp : _scopes.InversedElements()) _encode_finalize_scope(scp);
							encode_function_epilogue();
						} else InvalidArgumentException();
					}
				}
				void finalize_encoding(void)
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
						EncoderContext ctx(_conv, dest, src);
						ctx.encode_function_prologue();
						ctx.process_encoding();
						ctx.finalize_encoding();
						ctx.encode_debugger_trap();
						while (dest.code.Length() & 0x3) ctx.encode_debugger_trap();
						return true;
					} catch (...) { dest.Clear(); return false; }
				}
				virtual uint GetWordSize(void) noexcept override { return WordSize; }
			};
		}

		IAssemblyTranslator * CreateTranslatorX86i386(CallingConvention conv) { return new (std::nothrow) i386::TranslatorX86i386(conv); }
	}
}