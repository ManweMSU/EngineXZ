#include "xa_t_x64.h"
#include "xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace X64
		{
			constexpr int WordSize = 8;

			enum class Reg : uint {
				NO = 0,
				RAX = 0x0001, RCX = 0x0002, RDX = 0x0004, RBX = 0x0008,
				RSP = 0x0010, RBP = 0x0020, RSI = 0x0040, RDI = 0x0080,
				R8 = 0x0100, R9 = 0x0200, R10 = 0x0400, R11 = 0x0800,
				R12 = 0x1000, R13 = 0x2000, R14 = 0x4000, R15 = 0x8000,
				XMM0 = 0x00010000, XMM1 = 0x00020000, XMM2 = 0x00040000, XMM3 = 0x00080000,
				XMM4 = 0x00100000, XMM5 = 0x00200000, XMM6 = 0x00400000, XMM7 = 0x00800000,
			};
			enum class Op : uint8 { add = 0x02, sub = 0x2A, _and = 0x22, _or = 0x0A, _xor = 0x32, cmp = 0x3A };
			enum class mdOp : uint8 { mul = 0x4, div = 0x6, imul = 0x5, idiv = 0x7 };
			enum class shOp : uint8 { shl, shr, sal, sar };

			enum DispositionFlags {
				DispositionRegister	= 0x01,
				DispositionPointer	= 0x02,
				DispositionDiscard	= 0x04,
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
					int rbp_offset;
					bool indirect;
				};
				struct _local_disposition {
					int rbp_offset;
					int size;
					FinalizerReference finalizer;
				};
				struct _internal_disposition {
					int flags;
					Reg reg;
					int size;
				};
				struct _local_scope {
					int frame_base; // RBP + frame_base = real frame base
					int frame_size;
					int frame_size_unused;
					int first_local_no;
					int current_split_offset;
					Array<_local_disposition> locals = Array<_local_disposition>(0x20);
					bool shift_rsp, temporary;
				};
				struct _jump_reloc_struct {
					uint machine_offset_at;
					uint machine_offset_relative_to;
					uint xasm_offset_jump_to;
				};

				CallingConvention _conv;
				TranslatedFunction & _dest;
				const Function & _src;
				Array<uint> _org_inst_offsets;
				Array<_jump_reloc_struct> _jump_reloc;
				_argument_storage_spec _retval;
				Array<_argument_storage_spec> _inputs;
				int _scope_frame_base, _unroll_base, _current_instruction, _stack_oddity;
				Volumes::Stack<_local_scope> _scopes;
				Volumes::Stack<_local_disposition> _init_locals;

				static bool _is_xmm_register(Reg reg) { return uint(reg) & 0xFFFF0000; }
				static bool _is_pass_by_reference(const ArgumentSpecification & spec) { return _word_align(spec.size) > 8 || spec.semantics == ArgumentSemantics::Object; }
				static bool _needs_stack_storage(const ArgumentSpecification & spec)
				{
					if (_word_align(spec.size) > WordSize) return true;
					return false;
				}
				static uint8 _regular_register_code(Reg reg)
				{
					if (reg == Reg::RAX) return 0;
					else if (reg == Reg::RCX) return 1;
					else if (reg == Reg::RDX) return 2;
					else if (reg == Reg::RBX) return 3;
					else if (reg == Reg::RSP) return 4;
					else if (reg == Reg::RBP) return 5;
					else if (reg == Reg::RSI) return 6;
					else if (reg == Reg::RDI) return 7;
					else if (reg == Reg::R8) return 8;
					else if (reg == Reg::R9) return 9;
					else if (reg == Reg::R10) return 10;
					else if (reg == Reg::R11) return 11;
					else if (reg == Reg::R12) return 12;
					else if (reg == Reg::R13) return 13;
					else if (reg == Reg::R14) return 14;
					else if (reg == Reg::R15) return 15;
					else return 16;
				}
				static uint8 _xmm_register_code(Reg reg)
				{
					if (reg == Reg::XMM0) return 0;
					else if (reg == Reg::XMM1) return 1;
					else if (reg == Reg::XMM2) return 2;
					else if (reg == Reg::XMM3) return 3;
					else if (reg == Reg::XMM4) return 4;
					else if (reg == Reg::XMM5) return 5;
					else if (reg == Reg::XMM6) return 6;
					else if (reg == Reg::XMM7) return 7;
					else return 16;
				}
				static uint8 _make_rex(bool size64_W, bool reg_ext_R, bool index_ext_X, bool opt_ext_B)
				{
					uint8 result = 0x40;
					if (opt_ext_B) result |= 0x01;
					if (index_ext_X) result |= 0x02;
					if (reg_ext_R) result |= 0x04;
					if (size64_W) result |= 0x08;
					return result;
				}
				static uint8 _make_mod(uint8 reg_lo_3, uint8 mod, uint8 reg_mem_lo_3) { return (mod << 6) | (reg_lo_3 << 3) | reg_mem_lo_3; }
				static uint32 _word_align(const ObjectSize & size) { uint full_size = size.num_bytes + WordSize * size.num_words; return (uint64(full_size) + 7) / 8 * 8; }
				Array<_argument_passage_info> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt)
				{
					SafePointer< Array<_argument_passage_info> > result = new Array<_argument_passage_info>(0x40);
					if (_conv == CallingConvention::Windows) {
						for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics == ArgumentSemantics::This) {
							_argument_passage_info info;
							info.index = i;
							info.reg = Reg::NO;
							info.indirect = _is_pass_by_reference(inputs[i]);
							result->Append(info);
						}
					}
					if (_is_pass_by_reference(output)) {
						_argument_passage_info info;
						info.index = -1;
						info.reg = Reg::NO;
						info.indirect = true;
						result->Append(info);
					}
					for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics != ArgumentSemantics::This || _conv == CallingConvention::Unix) {
						_argument_passage_info info;
						info.index = i;
						info.reg = Reg::NO;
						info.indirect = _is_pass_by_reference(inputs[i]);
						result->Append(info);
					}
					int cmr = 0, cfr = 0;
					Reg unix_mr[] = { Reg::RDI, Reg::RSI, Reg::RDX, Reg::RCX, Reg::R8, Reg::R9, Reg::NO };
					Reg unix_fr[] = { Reg::XMM0, Reg::XMM1, Reg::XMM2, Reg::XMM3, Reg::XMM4, Reg::XMM5, Reg::XMM6, Reg::XMM7, Reg::NO };
					for (int i = 0; i < result->Length(); i++) {
						auto & info = result->ElementAt(i);
						if (!info.indirect && inputs[i].semantics == ArgumentSemantics::FloatingPoint) {
							if (_conv == CallingConvention::Windows) {
								if (i == 0) info.reg = Reg::XMM0;
								else if (i == 1) info.reg = Reg::XMM1;
								else if (i == 2) info.reg = Reg::XMM2;
								else if (i == 3) info.reg = Reg::XMM3;
							} else if (_conv == CallingConvention::Unix) {
								info.reg = unix_fr[cfr];
								if (info.reg != Reg::NO) cfr++;
							}
						} else {
							if (_conv == CallingConvention::Windows) {
								if (i == 0) info.reg = Reg::RCX;
								else if (i == 1) info.reg = Reg::RDX;
								else if (i == 2) info.reg = Reg::R8;
								else if (i == 3) info.reg = Reg::R9;
							} else if (_conv == CallingConvention::Unix) {
								info.reg = unix_mr[cmr];
								if (info.reg != Reg::NO) cmr++;
							}
						}
					}
					result->Retain();
					return result;
				}
				int _allocate_temporary(const ObjectSize & size, const FinalizerReference & final, int * rbp_offs = 0)
				{
					auto current_scope_ptr = _scopes.GetLast();
					if (!current_scope_ptr) throw InvalidArgumentException();
					auto & scope = current_scope_ptr->GetValue();
					auto size_padded = _word_align(size);
					_local_disposition new_var;
					new_var.size = size.num_bytes + WordSize * size.num_words;
					new_var.rbp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
					new_var.finalizer = final;
					scope.frame_size_unused -= size_padded;
					if (scope.frame_size_unused < 0) throw InvalidArgumentException();
					auto index = scope.first_local_no + scope.locals.Length();
					scope.locals << new_var;
					if (rbp_offs) *rbp_offs = new_var.rbp_offset;
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
				void _encode_preserve(Reg reg, uint reg_in_use, bool cond_if)
				{
					if (cond_if && (reg_in_use & uint(reg))) {
						encode_push(reg);
						_stack_oddity += 8;
					}
				}
				void _encode_restore(Reg reg, uint reg_in_use, bool cond_if)
				{
					if (cond_if && (reg_in_use & uint(reg))) {
						encode_pop(reg);
						_stack_oddity -= 8;
					}
				}
				void _encode_open_scope(int of_size, bool temporary, int enf_base)
				{
					if (!enf_base) {
						of_size = _word_align(TH::MakeSize(of_size, 0));
						if (of_size & 0xF) of_size += WordSize;
					}
					auto prev = _scopes.GetLast();
					_local_scope ls;
					ls.frame_base = enf_base ? enf_base : (prev ? prev->GetValue().frame_base - of_size : _scope_frame_base - of_size);
					ls.frame_size = of_size;
					ls.frame_size_unused = of_size;
					ls.current_split_offset = 0;
					ls.first_local_no = prev ? prev->GetValue().first_local_no + prev->GetValue().locals.Length() : 0;
					ls.shift_rsp = of_size && !enf_base;
					ls.temporary = temporary;
					_scopes.Push(ls);
					if (ls.shift_rsp) encode_add(Reg::RSP, -of_size);
				}
				void _encode_finalize_scope(const _local_scope & scope, uint reg_in_use = 0)
				{
					_encode_preserve(Reg::RAX, reg_in_use, true);
					if (_conv == CallingConvention::Windows) {
						_encode_preserve(Reg::RCX, reg_in_use, true);
						_encode_preserve(Reg::RDX, reg_in_use, true);
						_encode_preserve(Reg::R8, reg_in_use, true);
						_encode_preserve(Reg::R9, reg_in_use, true);
					} else if (_conv == CallingConvention::Unix) {
						_encode_preserve(Reg::RDI, reg_in_use, true);
						_encode_preserve(Reg::RSI, reg_in_use, true);
						_encode_preserve(Reg::RDX, reg_in_use, true);
						_encode_preserve(Reg::RCX, reg_in_use, true);
						_encode_preserve(Reg::R8, reg_in_use, true);
						_encode_preserve(Reg::R9, reg_in_use, true);
					}
					for (auto & l : scope.locals) if (l.finalizer.final.ref_class != ReferenceNull) {
						bool skip = false;
						for (auto & i : _init_locals) if (i.rbp_offset == l.rbp_offset) { skip = true; break; }
						if (skip) continue;
						int stack_growth = 0;
						if (_conv == CallingConvention::Windows) {
							stack_growth = max(1 + l.finalizer.final_args.Length(), 4) * 8;
							if ((stack_growth + _stack_oddity) & 0xF) {
								encode_add(Reg::RSP, -8);
								stack_growth += 8;
							}
							for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
								auto & arg = l.finalizer.final_args[i];
								if (i == 0) encode_put_addr_of(Reg::RDX, arg);
								else if (i == 1) encode_put_addr_of(Reg::R8, arg);
								else if (i == 2) encode_put_addr_of(Reg::R9, arg);
								else { encode_put_addr_of(Reg::RCX, arg); encode_push(Reg::RCX); }
							}
							encode_lea(Reg::RCX, Reg::RBP, l.rbp_offset);
							encode_add(Reg::RSP, -32);
						} else if (_conv == CallingConvention::Unix) {
							stack_growth = max(l.finalizer.final_args.Length() - 5, 0) * 8;
							if ((stack_growth + _stack_oddity) & 0xF) {
								encode_add(Reg::RSP, -8);
								stack_growth += 8;
							}
							for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
								auto & arg = l.finalizer.final_args[i];
								if (i == 0) encode_put_addr_of(Reg::RSI, arg);
								else if (i == 1) encode_put_addr_of(Reg::RDX, arg);
								else if (i == 2) encode_put_addr_of(Reg::RCX, arg);
								else if (i == 3) encode_put_addr_of(Reg::R8, arg);
								else if (i == 4) encode_put_addr_of(Reg::R9, arg);
								else { encode_put_addr_of(Reg::RDI, arg); encode_push(Reg::RDI); }
							}
							encode_lea(Reg::RDI, Reg::RBP, l.rbp_offset);
						}
						encode_put_addr_of(Reg::RAX, l.finalizer.final);
						encode_call(Reg::RAX, false);
						if (stack_growth) encode_add(Reg::RSP, stack_growth);
					}
					if (_conv == CallingConvention::Windows) {
						_encode_restore(Reg::R9, reg_in_use, true);
						_encode_restore(Reg::R8, reg_in_use, true);
						_encode_restore(Reg::RDX, reg_in_use, true);
						_encode_restore(Reg::RCX, reg_in_use, true);
					} else if (_conv == CallingConvention::Unix) {
						_encode_restore(Reg::R9, reg_in_use, true);
						_encode_restore(Reg::R8, reg_in_use, true);
						_encode_restore(Reg::RCX, reg_in_use, true);
						_encode_restore(Reg::RDX, reg_in_use, true);
						_encode_restore(Reg::RSI, reg_in_use, true);
						_encode_restore(Reg::RDI, reg_in_use, true);
					}
					_encode_restore(Reg::RAX, reg_in_use, true);
					if (scope.shift_rsp) encode_add(Reg::RSP, scope.frame_size);
				}
				void _encode_close_scope(uint reg_in_use = 0)
				{
					auto scope = _scopes.Pop();
					_encode_finalize_scope(scope, reg_in_use);
				}
				void _encode_blt(Reg dest, bool dest_indirect, Reg src, bool src_indirect, int size, uint reg_in_use)
				{
					Reg ir;
					if (dest != Reg::RDX && src != Reg::RDX) ir = Reg::RDX;
					else if (dest != Reg::RAX && src != Reg::RAX) ir = Reg::RAX;
					else ir = Reg::RBX;
					if (dest_indirect && src_indirect) {
						_encode_preserve(ir, reg_in_use, true);
						int blt_ofs = 0;
						while (blt_ofs < size) {
							if (blt_ofs + 8 <= size) {
								encode_mov_reg_mem(8, ir, src, blt_ofs);
								encode_mov_mem_reg(8, dest, blt_ofs, ir);
								blt_ofs += 8;
							} else if (blt_ofs + 4 <= size) {
								encode_mov_reg_mem(4, ir, src, blt_ofs);
								encode_mov_mem_reg(4, dest, blt_ofs, ir);
								blt_ofs += 4;
							} else if (blt_ofs + 2 <= size) {
								encode_mov_reg_mem(2, ir, src, blt_ofs);
								encode_mov_mem_reg(2, dest, blt_ofs, ir);
								blt_ofs += 2;
							} else {
								encode_mov_reg_mem(1, ir, src, blt_ofs);
								encode_mov_mem_reg(1, dest, blt_ofs, ir);
								blt_ofs++;
							}
						}
						_encode_restore(ir, reg_in_use, true);
					} else if (dest_indirect) {
						if (size == 8) {
							encode_mov_mem_reg(8, dest, src);
						} else if (size == 7) {
							_encode_preserve(ir, reg_in_use, true);
							encode_mov_reg_reg(8, ir, src);
							encode_shr(ir, 48);
							encode_mov_mem_reg(1, dest, 6, ir);
							encode_mov_reg_reg(8, ir, src);
							encode_shr(ir, 32);
							encode_mov_mem_reg(2, dest, 4, ir);
							encode_mov_mem_reg(4, dest, src);
							_encode_restore(ir, reg_in_use, true);
						} else if (size == 6) {
							_encode_preserve(ir, reg_in_use, true);
							encode_mov_reg_reg(8, ir, src);
							encode_shr(ir, 32);
							encode_mov_mem_reg(2, dest, 4, ir);
							encode_mov_mem_reg(4, dest, src);
							_encode_restore(ir, reg_in_use, true);
						} else if (size == 5) {
							_encode_preserve(ir, reg_in_use, true);
							encode_mov_reg_reg(8, ir, src);
							encode_shr(ir, 32);
							encode_mov_mem_reg(1, dest, 4, ir);
							encode_mov_mem_reg(4, dest, src);
							_encode_restore(ir, reg_in_use, true);
						} else if (size == 4) {
							encode_mov_mem_reg(4, dest, src);
						} else if (size == 3) {
							_encode_preserve(ir, reg_in_use, true);
							encode_mov_reg_reg(8, ir, src);
							encode_shr(ir, 16);
							encode_mov_mem_reg(1, dest, 2, ir);
							encode_mov_mem_reg(2, dest, src);
							_encode_restore(ir, reg_in_use, true);
						} else if (size == 2) {
							encode_mov_mem_reg(2, dest, src);
						} else if (size == 1) {
							encode_mov_mem_reg(1, dest, src);
						}
					} else if (src_indirect) {
						if (size == 8) {
							encode_mov_reg_mem(8, dest, src);
						} else if (size == 7) {
							encode_mov_reg_mem(1, dest, src, 6);
							encode_shl(dest, 16);
							encode_mov_reg_mem(2, dest, src, 4);
							encode_shl(dest, 32);
							encode_mov_reg_mem(4, dest, src);
						} else if (size == 6) {
							encode_mov_reg_mem(2, dest, src, 4);
							encode_shl(dest, 32);
							encode_mov_reg_mem(4, dest, src);
						} else if (size == 5) {
							encode_mov_reg_mem(1, dest, src, 4);
							encode_shl(dest, 32);
							encode_mov_reg_mem(4, dest, src);
						} else if (size == 4) {
							encode_mov_reg_mem(4, dest, src);
						} else if (size == 3) {
							encode_mov_reg_mem(1, dest, src, 2);
							encode_shl(dest, 16);
							encode_mov_reg_mem(2, dest, src);
						} else if (size == 2) {
							encode_mov_reg_mem(2, dest, src);
						} else if (size == 1) {
							encode_mov_reg_mem(1, dest, src);
						}
					} else {
						encode_mov_reg_reg(8, dest, src);
					}
				}
				void _encode_arithmetics(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.inputs.Length() > 2 || node.inputs.Length() < 1) throw InvalidArgumentException();
					auto size = node.retval_spec.size.num_bytes + WordSize * node.retval_spec.size.num_words;
					if (size != 1 && size != 2 && size != 4 && size != 8) throw InvalidArgumentException();
					auto opcode = node.self.index;
					_internal_disposition core;
					core.flags = DispositionRegister;
					core.size = size;
					core.reg = (disp->reg != Reg::NO) ? disp->reg : Reg::RAX;
					Array<Reg> fsave(2);
					Reg rreg = Reg::NO;
					if (opcode == TransformIntegerSResize || opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR) {
						core.reg = Reg::RAX;
						if (disp->reg != Reg::RAX) fsave << Reg::RAX;
					} if (opcode >= TransformIntegerUMul && opcode <= TransformIntegerSMod) {
						core.reg = Reg::RAX;
						if (disp->reg != Reg::RAX) fsave << Reg::RAX;
						if (disp->reg != Reg::RDX) fsave << Reg::RDX;
					} else if (disp->reg == Reg::NO) fsave << Reg::RAX;
					for (auto & r : fsave.Elements()) _encode_preserve(r, reg_in_use, !idle);
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
									if (size_from < 8 && size >= 8) { _dest.code << _make_rex(true, false, false, false); _dest.code << 0x98; }
								}
								rreg = Reg::RAX;
							} else if (opcode == TransformIntegerInverse) {
								encode_negative(size, core.reg);
							} else if (opcode == TransformIntegerAbs) {
								Reg sec = core.reg == Reg::RDX ? Reg::RCX : Reg::RDX;
								_encode_preserve(sec, reg_in_use, true);
								encode_mov_reg_reg(size, sec, core.reg);
								if (size == 1) encode_shift(size, shOp::sar, sec, 7);
								else if (size == 2) encode_shift(size, shOp::sar, sec, 15);
								else if (size == 4) encode_shift(size, shOp::sar, sec, 31);
								else if (size == 8) encode_shift(size, shOp::sar, sec, 63);
								encode_operation(size, Op::_xor, core.reg, sec);
								encode_operation(size, Op::sub, core.reg, sec);
								_encode_restore(sec, reg_in_use, true);
							}
						}
					} else {
						if (node.inputs.Length() != 2) throw InvalidArgumentException();
						_internal_disposition modifier;
						modifier.flags = DispositionAny;
						modifier.size = size;
						modifier.reg = core.reg == Reg::RCX ? Reg::RDX : Reg::RCX;
						_encode_preserve(modifier.reg, reg_in_use, !idle);
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
								encode_operation(size, Op::_xor, core.reg, core.reg);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
							} else if (opcode == TransformVectorAnd) {
								if (modifier.flags & DispositionPointer) encode_operation(size, Op::_and, core.reg, modifier.reg, true);
								else encode_operation(size, Op::_and, core.reg, modifier.reg);
							} else if (opcode == TransformVectorOr) {
								if (modifier.flags & DispositionPointer) encode_operation(size, Op::_or, core.reg, modifier.reg, true);
								else encode_operation(size, Op::_or, core.reg, modifier.reg);
							} else if (opcode == TransformVectorXor) {
								if (modifier.flags & DispositionPointer) encode_operation(size, Op::_xor, core.reg, modifier.reg, true);
								else encode_operation(size, Op::_xor, core.reg, modifier.reg);
							} else if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR) {
								if (modifier.flags & DispositionPointer) encode_mov_reg_mem(size, Reg::RCX, Reg::RCX);
								int mask, max_shift;
								shOp op;
								if (opcode == TransformVectorShiftL) op = shOp::shl;
								else if (opcode == TransformVectorShiftR) op = shOp::shr;
								else if (opcode == TransformVectorShiftAL) op = shOp::sal;
								else if (opcode == TransformVectorShiftAR) op = shOp::sar;
								if (size == 1) { mask = 0xFFFFFFF8; max_shift = 7; }
								else if (size == 2) { mask = 0xFFFFFFF0; max_shift = 15; }
								else if (size == 4) { mask = 0xFFFFFFE0; max_shift = 31; }
								else if (size == 8) { mask = 0xFFFFFFC0; max_shift = 63; }
								encode_test(size, Reg::RCX, mask);
								int addr;
								_dest.code << 0x75; // JNZ
								_dest.code << 0x00;
								addr = _dest.code.Length();
								encode_shift(size, op, Reg::RAX);
								_dest.code << 0xEB;
								_dest.code << 0x00;
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								addr = _dest.code.Length();
								if (opcode == TransformVectorShiftAR) encode_shift(size, op, Reg::RAX, max_shift);
								else encode_mov_reg_const(size, Reg::RAX, 0);
								_dest.code[addr - 1] = _dest.code.Length() - addr;
								rreg = Reg::RAX;
							} else if (opcode >= TransformIntegerEQ && opcode <= TransformIntegerSG) {
								if (modifier.flags & DispositionPointer) encode_operation(size, Op::cmp, core.reg, modifier.reg, true);
								else encode_operation(size, Op::cmp, core.reg, modifier.reg);
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
								encode_operation(8, Op::_xor, core.reg, core.reg);
								_dest.code << 0xEB;
								_dest.code << 10;
								encode_mov_reg_const(8, core.reg, 1);
							} else if (opcode == TransformIntegerAdd) {
								if (modifier.flags & DispositionPointer) encode_operation(size, Op::add, core.reg, modifier.reg, true);
								else encode_operation(size, Op::add, core.reg, modifier.reg);
							} else if (opcode == TransformIntegerSubt) {
								if (modifier.flags & DispositionPointer) encode_operation(size, Op::sub, core.reg, modifier.reg, true);
								else encode_operation(size, Op::sub, core.reg, modifier.reg);
							} else if (opcode == TransformIntegerUMul) {
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::mul, modifier.reg, true);
								else encode_mul_div(size, mdOp::mul, modifier.reg);
								rreg = Reg::RAX;
							} else if (opcode == TransformIntegerSMul) {
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::imul, modifier.reg, true);
								else encode_mul_div(size, mdOp::imul, modifier.reg);
								rreg = Reg::RAX;
							} else if (opcode == TransformIntegerUDiv) {
								if (size > 1) encode_operation(size, Op::_xor, Reg::RDX, Reg::RDX);
								else { encode_shl(Reg::RAX, 56); encode_shr(Reg::RAX, 56); }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::div, modifier.reg, true);
								else encode_mul_div(size, mdOp::div, modifier.reg);
								rreg = Reg::RAX;
							} else if (opcode == TransformIntegerSDiv) {
								if (size > 1) {
									encode_mov_reg_reg(size, Reg::RDX, Reg::RAX);
									encode_shift(size, shOp::sar, Reg::RDX, 63);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::idiv, modifier.reg, true);
								else encode_mul_div(size, mdOp::idiv, modifier.reg);
								rreg = Reg::RAX;
							} else if (opcode == TransformIntegerUMod) {
								if (size > 1) encode_operation(size, Op::_xor, Reg::RDX, Reg::RDX);
								else { encode_shl(Reg::RAX, 56); encode_shr(Reg::RAX, 56); }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::div, modifier.reg, true);
								else encode_mul_div(size, mdOp::div, modifier.reg);
								if (size == 1) {
									encode_shr(Reg::RAX, 8);
									rreg = Reg::RAX;
								} else rreg = Reg::RDX;
							} else if (opcode == TransformIntegerSMod) {
								if (size > 1) {
									encode_mov_reg_reg(size, Reg::RDX, Reg::RAX);
									encode_shift(size, shOp::sar, Reg::RDX, 63);
								} else { _dest.code << 0x66; _dest.code << 0x98; }
								if (modifier.flags & DispositionPointer) encode_mul_div(size, mdOp::idiv, modifier.reg, true);
								else encode_mul_div(size, mdOp::idiv, modifier.reg);
								if (size == 1) {
									encode_shr(Reg::RAX, 8);
									rreg = Reg::RAX;
								} else rreg = Reg::RDX;
							} else throw InvalidArgumentException();
						}
						_encode_restore(modifier.reg, reg_in_use, !idle);
					}
					if (rreg != Reg::NO && rreg != disp->reg && !idle) encode_mov_reg_reg(size, disp->reg, rreg);
					for (auto & r : fsave.InversedElements()) _encode_restore(r, reg_in_use, !idle);
					if (disp->flags & DispositionRegister) {
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionPointer) {
						(*mem_load) += _word_align(TH::MakeSize(size, 0));
						if (!idle) {
							int offs;
							_allocate_temporary(TH::MakeSize(size, 0), TH::MakeFinal(), &offs);
							encode_mov_mem_reg(size, Reg::RBP, offs, disp->reg);
							encode_lea(disp->reg, Reg::RBP, offs);
						}
						disp->flags = DispositionPointer;
					}
				}
				void _encode_logics(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformLogicalFork) {
						if (node.inputs.Length() != 3) throw InvalidArgumentException();
						_internal_disposition cond, none;
						cond.reg = Reg::RCX;
						cond.flags = DispositionRegister;
						cond.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
						none.reg = Reg::NO;
						none.flags = DispositionDiscard;
						none.size = 0;
						if (!cond.size || cond.size > 8) throw InvalidArgumentException();
						_encode_preserve(cond.reg, reg_in_use, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &cond, reg_in_use | uint(cond.reg));
						if (!idle) {
							encode_test(cond.size, cond.reg, 0xFFFFFFFF);
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
						Reg local = disp->reg != Reg::NO ? disp->reg : Reg::RCX;
						_encode_preserve(local, reg_in_use, local != disp->reg && !idle);
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
							ld.flags = DispositionRegister;
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
								int offs;
								_allocate_temporary(TH::MakeSize(0, 1), TH::MakeFinal(), &offs);
								encode_mov_mem_reg(8, Reg::RBP, offs, local);
								encode_lea(disp->reg, Reg::RBP, offs);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						_encode_restore(local, reg_in_use, local != disp->reg && !idle);
					}
				}
				void _encode_general_call(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					bool indirect, retval_byref, preserve_rax, retval_final;
					int first_arg, arg_no;
					if (node.self.ref_class == ReferenceTransform && node.self.index == TransformInvoke) {
						indirect = true; first_arg = 1; arg_no = node.inputs.Length() - 1;
					} else {
						indirect = false; first_arg = 0; arg_no = node.inputs.Length();
					}
					preserve_rax = disp->reg != Reg::RAX;
					retval_byref = _is_pass_by_reference(node.retval_spec);
					retval_final = node.retval_final.final.ref_class != ReferenceNull;
					SafePointer< Array<_argument_passage_info> > layout = _make_interface_layout(node.retval_spec, node.input_specs.GetBuffer() + first_arg, arg_no);
					Array<Reg> preserve_regs(0x10);
					Array<int> argument_homes(1), argument_layout_index(1);
					if (_conv == CallingConvention::Windows) preserve_regs << Reg::RBX << Reg::RCX << Reg::RDX << Reg::R8 << Reg::R9;
					else if (_conv == CallingConvention::Unix) preserve_regs << Reg::RBX << Reg::RDI << Reg::RSI << Reg::RDX << Reg::RCX << Reg::R8 << Reg::R9;
					_encode_preserve(Reg::RAX, reg_in_use, !idle && preserve_rax);
					for (auto & r : preserve_regs.Elements()) _encode_preserve(r, reg_in_use, !idle);
					uint reg_used_mask = 0;
					uint stack_usage = 0;
					uint num_args_by_stack = 0;
					uint num_args_by_xmm = 0;
					for (auto & info : *layout) {
						if (_is_xmm_register(info.reg)) num_args_by_xmm++;
						else if (info.reg == Reg::NO) num_args_by_stack++;
					}
					if (_conv == CallingConvention::Windows) stack_usage = max(layout->Length(), 4) * 8;
					else if (_conv == CallingConvention::Unix) stack_usage = (num_args_by_stack + num_args_by_xmm) * 8;
					if ((_stack_oddity + stack_usage) & 0xF) stack_usage += 8;
					if (stack_usage && !idle) encode_add(Reg::RSP, -int(stack_usage));
					argument_homes.SetLength(node.inputs.Length() - first_arg);
					argument_layout_index.SetLength(node.inputs.Length() - first_arg);
					uint current_stack_index = 0;
					int rv_offset = 0;
					Reg rv_reg = Reg::NO;
					for (int i = 0; i < layout->Length(); i++) {
						auto & info = layout->ElementAt(i);
						if (info.index >= 0) {
							argument_layout_index[info.index] = i;
							if (_conv == CallingConvention::Windows) {
								argument_homes[info.index] = current_stack_index;
								current_stack_index += 8;
							} else if (_conv == CallingConvention::Unix) {
								if (info.reg == Reg::NO) {
									argument_homes[info.index] = current_stack_index;
									current_stack_index += 8;
								} else argument_homes[info.index] = -1;
							}
						} else {
							if (_conv == CallingConvention::Windows) current_stack_index += 8;
							*mem_load += _word_align(node.retval_spec.size);
							rv_reg = info.reg;
							if (!idle) _allocate_temporary(node.retval_spec.size, node.retval_final, &rv_offset);
						}
					}
					if (_conv == CallingConvention::Unix) for (auto & info : layout->Elements()) if (info.index >= 0 && _is_xmm_register(info.reg)) {
						argument_homes[info.index] = current_stack_index;
						current_stack_index += 8;
					}
					if (indirect) {
						_internal_disposition ld;
						ld.flags = DispositionRegister;
						ld.reg = Reg::RAX;
						ld.size = 8;
						reg_used_mask |= uint(ld.reg);
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | reg_used_mask);
					}
					if (!idle && current_stack_index) {
						encode_mov_reg_reg(8, Reg::RBX, Reg::RSP);
						reg_used_mask |= uint(Reg::RBX);
					}
					for (int i = 0; i < argument_homes.Length(); i++) {
						auto home = argument_homes[i];
						auto & info = layout->ElementAt(argument_layout_index[i]);
						auto & spec = node.input_specs[i + first_arg];
						_internal_disposition ld;
						ld.size = spec.size.num_bytes + WordSize * spec.size.num_words;
						ld.flags = info.indirect ? DispositionPointer : DispositionRegister;
						bool home_mode;
						if (info.reg == Reg::NO || _is_xmm_register(info.reg)) {
							home_mode = true;
							ld.reg = Reg::R14;
						} else {
							home_mode = false;
							ld.reg = info.reg;
							reg_used_mask |= uint(ld.reg);
						}
						if (home_mode) _encode_preserve(ld.reg, reg_in_use | reg_used_mask, !idle);
						_encode_tree_node(node.inputs[i + first_arg], idle, mem_load, &ld, reg_in_use | reg_used_mask | uint(ld.reg));
						if (home_mode) {
							if (!idle) encode_mov_mem_reg(8, Reg::RBX, home, ld.reg);
							_encode_restore(ld.reg, reg_in_use | reg_used_mask, !idle);
						}
					}
					if (!idle && rv_reg != Reg::NO) encode_lea(rv_reg, Reg::RBP, rv_offset);
					if (!idle && num_args_by_xmm) {
						if (indirect) encode_push(Reg::RAX);
						for (int i = 0; i < argument_homes.Length(); i++) {
							auto home = argument_homes[i];
							auto & info = layout->ElementAt(argument_layout_index[i]);
							if (_is_xmm_register(info.reg)) {
								encode_mov_reg_mem(8, Reg::RAX, Reg::RBX, home);
								encode_mov_xmm_reg(8, info.reg, Reg::RAX);
							}
						}
						if (indirect) encode_pop(Reg::RAX);
					}
					if (!indirect && !idle) encode_put_addr_of(Reg::RAX, node.self);
					if (!idle) {
						encode_call(Reg::RAX, false);
						if (stack_usage) encode_add(Reg::RSP, int(stack_usage));
						if (!retval_byref && node.retval_spec.semantics == ArgumentSemantics::FloatingPoint) encode_mov_reg_xmm(8, Reg::RAX, Reg::XMM0);
					}
					for (auto & r : preserve_regs.InversedElements()) _encode_restore(r, reg_in_use, !idle);
					if ((disp->flags & DispositionPointer) && retval_byref) {
						if (!idle && disp->reg != Reg::RAX) encode_mov_reg_reg(8, disp->reg, Reg::RAX);
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && !retval_byref) {
						if (retval_final) {
							*mem_load += WordSize;
							if (!idle) {
								int offs;
								_allocate_temporary(TH::MakeSize(0, 1), node.retval_final, &offs);
								encode_mov_mem_reg(8, Reg::RBP, offs, Reg::RAX);
							}
						}
						if (!idle && disp->reg != Reg::RAX) encode_mov_reg_reg(8, disp->reg, Reg::RAX);
						disp->flags = DispositionRegister;
					} else if ((disp->flags & DispositionPointer) && !retval_byref) {
						*mem_load += WordSize;
						if (!idle) {
							int offs;
							_allocate_temporary(TH::MakeSize(0, 1), node.retval_final, &offs);
							encode_mov_mem_reg(8, Reg::RBP, offs, Reg::RAX);
							encode_lea(disp->reg, Reg::RBP, offs);
						}
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && retval_byref) {
						if (!idle) {
							_encode_preserve(Reg::RCX, reg_in_use, !preserve_rax);
							Reg src = Reg::RAX;
							if (disp->reg == Reg::RAX) {
								src = Reg::RCX;
								encode_mov_reg_reg(8, Reg::RCX, Reg::RAX);
							}
							uint size = _word_align(node.retval_spec.size);
							_encode_blt(disp->reg, false, src, true, size, reg_in_use | uint(disp->reg) | uint(src));
							_encode_restore(Reg::RCX, reg_in_use, !preserve_rax);
						}
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					}
					_encode_restore(Reg::RAX, reg_in_use, !idle && preserve_rax);
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
										src.size = 8;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										_internal_disposition src;
										src.flags = DispositionRegister;
										src.reg = disp->reg;
										src.size = 8;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										if (!idle) encode_mov_reg_mem(8, src.reg, src.reg);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										_internal_disposition src;
										src.flags = DispositionDiscard;
										src.reg = Reg::NO;
										src.size = 0;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
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
											encode_mov_mem_reg(8, Reg::RBP, offset, src.reg);
											encode_lea(src.reg, Reg::RBP, offset);
										}
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionDiscard) {
										_internal_disposition src;
										src.flags = DispositionDiscard;
										src.reg = Reg::NO;
										src.size = 0;
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformAddressOffset) {
									if (node.inputs.Length() < 2 || node.inputs.Length() > 3) throw InvalidArgumentException();
									_internal_disposition base;
									base.flags = DispositionPointer;
									base.reg = Reg::RSI;
									base.size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									_encode_preserve(base.reg, reg_in_use, !idle && disp->reg != Reg::RSI);
									_encode_tree_node(node.inputs[0], idle, mem_load, &base, reg_in_use | uint(base.reg));
									if (node.inputs[1].self.ref_class == ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint offset = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (!idle && offset * scale) {
											if (scale != 0xFFFFFFFF) encode_add(Reg::RSI, offset * scale);
											else encode_add(Reg::RSI, -offset);
										}
									} else if (node.inputs[1].self.ref_class != ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										_encode_preserve(Reg::RAX, reg_in_use, !idle);
										_encode_preserve(Reg::RDX, reg_in_use, !idle);
										_encode_preserve(Reg::RCX, reg_in_use, !idle && scale > 1);
										if (scale) {
											_internal_disposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg::RAX;
											offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg::RSI) | uint(Reg::RAX) | uint(Reg::RDX) | uint(Reg::RCX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(8, Reg::RCX, scale);
													encode_mul_div(8, mdOp::mul, Reg::RCX);
												}
												encode_operation(8, Op::add, Reg::RSI, Reg::RAX);
											}
										}
										_encode_restore(Reg::RCX, reg_in_use, !idle && scale > 1);
										_encode_restore(Reg::RDX, reg_in_use, !idle);
										_encode_restore(Reg::RAX, reg_in_use, !idle);
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										_encode_preserve(Reg::RAX, reg_in_use, !idle);
										_encode_preserve(Reg::RDX, reg_in_use, !idle);
										_encode_preserve(Reg::RCX, reg_in_use, !idle && scale > 1);
										if (scale) {
											_internal_disposition offset;
											offset.flags = DispositionRegister;
											offset.reg = Reg::RAX;
											offset.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(Reg::RSI) | uint(Reg::RAX) | uint(Reg::RDX) | uint(Reg::RCX));
											if (!idle) {
												if (scale > 1) {
													encode_mov_reg_const(8, Reg::RCX, scale);
													encode_mul_div(8, mdOp::mul, Reg::RCX);
												}
												encode_operation(8, Op::add, Reg::RSI, Reg::RAX);
											}
										}
										_encode_restore(Reg::RCX, reg_in_use, !idle && scale > 1);
										_encode_restore(Reg::RDX, reg_in_use, !idle);
										_encode_restore(Reg::RAX, reg_in_use, !idle);
									} else {
										_encode_preserve(Reg::RAX, reg_in_use, !idle);
										_encode_preserve(Reg::RDX, reg_in_use, !idle);
										_encode_preserve(Reg::RCX, reg_in_use, !idle);
										_internal_disposition offset;
										offset.flags = DispositionRegister;
										offset.reg = Reg::RAX;
										offset.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
										_internal_disposition scale;
										scale.flags = DispositionRegister;
										scale.reg = Reg::RCX;
										scale.size = node.input_specs[2].size.num_bytes + WordSize * node.input_specs[2].size.num_words;
										if (offset.size != 8 || scale.size != 8) throw InvalidArgumentException();
										_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(Reg::RSI) | uint(Reg::RAX) | uint(Reg::RDX) | uint(Reg::RCX));
										_encode_tree_node(node.inputs[2], idle, mem_load, &scale, reg_in_use | uint(Reg::RSI) | uint(Reg::RAX) | uint(Reg::RDX) | uint(Reg::RCX));
										if (!idle) {
											encode_mul_div(8, mdOp::mul, Reg::RCX);
											encode_operation(8, Op::add, Reg::RSI, Reg::RAX);
										}
										_encode_restore(Reg::RCX, reg_in_use, !idle);
										_encode_restore(Reg::RDX, reg_in_use, !idle);
										_encode_restore(Reg::RAX, reg_in_use, !idle);
									}
									if (disp->flags & DispositionPointer) {
										if (disp->reg != Reg::RSI && !idle) encode_mov_reg_reg(8, disp->reg, Reg::RSI);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) {
											if (disp->reg != Reg::RSI) {
												_encode_blt(disp->reg, false, Reg::RSI, true, disp->size, reg_in_use | uint(Reg::RSI));
											} else {
												_encode_preserve(Reg::RDI, reg_in_use, true);
												encode_mov_reg_reg(8, Reg::RDI, Reg::RSI);
												_encode_blt(Reg::RSI, false, Reg::RDI, true, disp->size, reg_in_use | uint(Reg::RDI) | uint(Reg::RSI));
												_encode_restore(Reg::RDI, reg_in_use, true);
											}
										}
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
									_encode_restore(base.reg, reg_in_use, !idle && disp->reg != Reg::RSI);
								} else if (node.self.index == TransformBlockTransfer) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									uint size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									_internal_disposition dest_d, src_d;
									dest_d = *disp;
									if (dest_d.reg == Reg::NO) dest_d.reg = Reg::RAX;
									dest_d.size = size;
									dest_d.flags = DispositionPointer;
									src_d.flags = DispositionAny;
									src_d.size = size;
									src_d.reg = Reg::RBX;
									if (src_d.reg == dest_d.reg) src_d.reg = Reg::RAX;
									if (!idle) {
										_encode_preserve(dest_d.reg, reg_in_use, disp->reg == Reg::NO);
										_encode_preserve(src_d.reg, reg_in_use, true);
									}
									_encode_tree_node(node.inputs[0], idle, mem_load, &dest_d, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
									_encode_tree_node(node.inputs[1], idle, mem_load, &src_d, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
									if (!idle) _encode_blt(dest_d.reg, true, src_d.reg, src_d.flags & DispositionPointer, size, reg_in_use);
									if ((disp->flags & DispositionRegister) && !(disp->flags & DispositionPointer)) {
										if (!idle) {
											if (src_d.flags & DispositionPointer) _encode_blt(src_d.reg, false, dest_d.reg, true, size, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
											encode_mov_reg_reg(8, dest_d.reg, src_d.reg);
										}
										disp->flags = DispositionRegister;
										disp->reg = dest_d.reg;
										disp->size = size;
									} else {
										if (disp->flags & DispositionAny) *disp = dest_d;
									}
									if (!idle) {
										_encode_restore(src_d.reg, reg_in_use, true);
										_encode_restore(dest_d.reg, reg_in_use, disp->reg == Reg::NO);
									}
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
										local.rbp_offset = offset;
										local.size = size;
										_init_locals.Push(local);
									}
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use);
									if (!idle) _init_locals.Pop();
									if (disp->flags & DispositionPointer) {
										if (!idle) encode_lea(disp->reg, Reg::RBP, offset);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) encode_mov_reg_mem(8, disp->reg, Reg::RBP, offset);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformBreakIf) {
									if (node.inputs.Length() != 3) throw InvalidArgumentException();
									_encode_tree_node(node.inputs[0], idle, mem_load, disp, reg_in_use);
									_internal_disposition ld;
									ld.flags = DispositionRegister;
									ld.reg = Reg::RCX;
									ld.size = node.input_specs[1].size.num_bytes + WordSize * node.input_specs[1].size.num_words;
									_encode_preserve(ld.reg, reg_in_use, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									if (!idle) {
										encode_test(ld.size, ld.reg, 0xFFFFFFFF);
										_dest.code << 0x0F; _dest.code << 0x84; // JZ
										_dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00; _dest.code << 0x00;
										int addr = _dest.code.Length();
										auto scope = _scopes.GetLast();
										while (scope && !scope->GetValue().shift_rsp) scope = scope->GetPrevious();
										if (scope) encode_lea(Reg::RSP, Reg::RBP, scope->GetValue().frame_base);
										else encode_lea(Reg::RSP, Reg::RBP, _scope_frame_base);
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
								} else if (node.self.index == TransformSplit) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = node.input_specs[0].size.num_bytes + WordSize * node.input_specs[0].size.num_words;
									_internal_disposition ld = *disp;
									if (ld.flags & DispositionDiscard) {
										ld.flags = DispositionPointer;
										ld.reg = Reg::RSI;
										ld.size = size;
									}
									_encode_preserve(ld.reg, reg_in_use, !idle && ld.reg != disp->reg);
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									*mem_load += _word_align(node.input_specs[0].size);
									Reg dest = ld.reg == Reg::RDI ? Reg::RDX : Reg::RDI;
									if (!idle) {
										int offset;
										_allocate_temporary(node.input_specs[0].size, XA::TH::MakeFinal(), &offset);
										_scopes.GetLast()->GetValue().current_split_offset = offset;
										_encode_preserve(dest, reg_in_use | uint(ld.reg), true);
										encode_lea(dest, Reg::RBP, offset);
										_encode_blt(dest, true, ld.reg, ld.flags & DispositionPointer, size, reg_in_use | uint(ld.reg) | uint(dest));
										_encode_restore(dest, reg_in_use | uint(ld.reg), true);
									}
									_encode_restore(ld.reg, reg_in_use, !idle && ld.reg != disp->reg);
									if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									} else *disp = ld;
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
								Reg ld = Reg::RAX;
								if (ld == disp->reg) ld = Reg::R15;
								_encode_preserve(ld, reg_in_use, true);
								encode_put_addr_of(ld, node.self);
								_encode_blt(disp->reg, false, ld, true, disp->size, reg_in_use | uint(ld));
								_encode_restore(ld, reg_in_use, true);
							}
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					}
				}
				void _encode_expression_evaluation(const ExpressionTree & tree, Reg retval_copy)
				{
					if (retval_copy != Reg::NO && _needs_stack_storage(tree.retval_spec)) throw InvalidArgumentException();
					int _temp_storage = 0;
					_internal_disposition disp;
					disp.reg = retval_copy;
					if (retval_copy == Reg::NO) {
						disp.flags = DispositionDiscard;
						disp.size = 0;
					} else {
						disp.flags = DispositionRegister;
						disp.size = 1;
					}
					_encode_tree_node(tree, true, &_temp_storage, &disp, uint(retval_copy));
					_encode_open_scope(_temp_storage, true, 0);
					_encode_tree_node(tree, false, &_temp_storage, &disp, uint(retval_copy));
					_encode_close_scope();
				}
			public:
				EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src) : _conv(conv), _dest(dest), _src(src), _org_inst_offsets(0x200), _jump_reloc(0x100), _inputs(0x20)
				{
					_current_instruction = -1;
					_stack_oddity = 0;
					_org_inst_offsets.SetLength(_src.instset.Length());
				}
				void encode_debugger_trap(void) { _dest.code << 0xCC; }
				void encode_pure_ret(void) { _dest.code << 0xC3; }
				void encode_mov_reg_reg(uint quant, Reg dest, Reg src)
				{
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src);
					if (quant == 8) {
						_dest.code << _make_rex(true, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					} else if (quant == 4) {
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					} else if (quant == 1) {
						if (si >= 4 || di >= 4) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x88;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					}
				}
				void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr)
				{
					if (src_ptr == Reg::RBP || src_ptr == Reg::RSP) return;
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src_ptr);
					if (quant == 8) {
						_dest.code << _make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					} else if (quant == 4) {
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					} else if (quant == 1) {
						if (si >= 4 || di >= 4) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8A;
						_dest.code << _make_mod(di & 0x07, 0x0, si & 0x07);
					}
				}
				void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, int src_offset)
				{
					if (src_ptr == Reg::RSP) return;
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src_ptr);
					uint8 mode;
					if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
					if (quant == 8) {
						_dest.code << _make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 4) {
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x8B;
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 1) {
						if (si >= 4 || di >= 4) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
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
				void encode_mov_reg_const(uint quant, Reg dest, uint64 value)
				{
					auto di = _regular_register_code(dest);
					auto di_lo = di & 0x07;
					if (quant == 8) {
						_dest.code << _make_rex(true, 0, 0, di & 0x08);
						_dest.code << 0xB8 + di_lo;
					} else if (quant == 4) {
						if (di & 0x08) _dest.code << _make_rex(false, 0, 0, di & 0x08);
						_dest.code << 0xB8 + di_lo;
					} else if (quant == 2) {
						_dest.code << 0x66;
						if (di & 0x08) _dest.code << _make_rex(false, 0, 0, di & 0x08);
						_dest.code << 0xB8 + di_lo;
					} else if (quant == 1) {
						if (di >= 4) _dest.code << _make_rex(false, 0, 0, di & 0x08);
						_dest.code << 0xB0 + di_lo;
					}
					for (uint q = 0; q < quant; q++) { _dest.code << uint8(value); value >>= 8; }
				}
				void encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg src)
				{
					if (dest_ptr == Reg::RBP || dest_ptr == Reg::RSP) return;
					auto di = _regular_register_code(dest_ptr);
					auto si = _regular_register_code(src);
					if (quant == 8) {
						_dest.code << _make_rex(true, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					} else if (quant == 4) {
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					} else if (quant == 1) {
						if (si >= 4 || di >= 4) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x88;
						_dest.code << _make_mod(si & 0x07, 0x0, di & 0x07);
					}
				}
				void encode_mov_mem_reg(uint quant, Reg dest_ptr, int dest_offset, Reg src)
				{
					if (dest_ptr == Reg::RSP) return;
					auto di = _regular_register_code(dest_ptr);
					auto si = _regular_register_code(src);
					uint8 mode;
					if (dest_offset >= -128 && dest_offset < 128) mode = 0x01; else mode = 0x02;
					if (quant == 8) {
						_dest.code << _make_rex(true, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, mode, di & 0x07);
					} else if (quant == 4) {
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, mode, di & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x89;
						_dest.code << _make_mod(si & 0x07, mode, di & 0x07);
					} else if (quant == 1) {
						if (si >= 4 || di >= 4) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
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
				void encode_mov_reg_xmm(uint quant, Reg dest, Reg src)
				{
					auto di = _regular_register_code(dest);
					auto si = _xmm_register_code(src);
					if (quant == 8) {
						_dest.code << 0x66;
						_dest.code << _make_rex(true, si & 0x08, 0, di & 0x08);
						_dest.code << 0x0F;
						_dest.code << 0x7E;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					} else if (quant == 4) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, si & 0x08, 0, di & 0x08);
						_dest.code << 0x0F;
						_dest.code << 0x7E;
						_dest.code << _make_mod(si & 0x07, 0x3, di & 0x07);
					}
				}
				void encode_mov_xmm_reg(uint quant, Reg dest, Reg src)
				{
					auto di = _xmm_register_code(dest);
					auto si = _regular_register_code(src);
					if (quant == 8) {
						_dest.code << 0x66;
						_dest.code << _make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F;
						_dest.code << 0x6E;
						_dest.code << _make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (quant == 4) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F;
						_dest.code << 0x6E;
						_dest.code << _make_mod(di & 0x07, 0x3, si & 0x07);
					}
				}
				void encode_lea(Reg dest, Reg src_ptr, int src_offset)
				{
					if (src_ptr == Reg::RSP) return;
					auto di = _regular_register_code(dest);
					auto si = _regular_register_code(src_ptr);
					uint8 mode;
					if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
					_dest.code << _make_rex(true, di & 0x08, 0, si & 0x08);
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
					uint8 rex = _make_rex(false, false, false, rc & 0x08);
					if (rex & 0x0F) _dest.code << rex;
					_dest.code << (0x50 + (rc & 0x07));
				}
				void encode_pop(Reg reg)
				{
					uint rc = _regular_register_code(reg);
					uint8 rex = _make_rex(false, false, false, rc & 0x08);
					if (rex & 0x0F) _dest.code << rex;
					_dest.code << (0x58 + (rc & 0x07));
				}
				void encode_operation(uint quant, Op op, Reg to, Reg value_ptr, bool indirect = false, int value_offset = 0)
				{
					if (value_ptr == Reg::RSP || value_ptr == Reg::RBP) return;
					auto di = _regular_register_code(to);
					auto si = _regular_register_code(value_ptr);
					auto o = uint(op);
					uint8 mode;
					if (indirect) {
						if (value_offset == 0) mode = 0x00;
						else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
						else mode = 0x02;
					} else mode = 0x03;
					if (quant == 8) {
						_dest.code << _make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << (o + 1);
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 4) {
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << (o + 1);
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if ((si & 0x08) || (di & 0x08)) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << (o + 1);
						_dest.code << _make_mod(di & 0x07, mode, si & 0x07);
					} else if (quant == 1) {
						if (si >= 4 || di >= 4) _dest.code << _make_rex(false, di & 0x08, 0, si & 0x08);
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
					if (value_ptr == Reg::RSP || value_ptr == Reg::RBP) return;
					auto si = _regular_register_code(value_ptr);
					auto o = uint(op);
					uint8 mode;
					if (indirect) {
						if (value_offset == 0) mode = 0x00;
						else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
						else mode = 0x02;
					} else mode = 0x03;
					if (quant == 8) {
						_dest.code << _make_rex(true, false, 0, si & 0x08);
						_dest.code << 0xF7;
						_dest.code << _make_mod(uint(op), mode, si & 0x07);
					} else if (quant == 4) {
						if (si & 0x08) _dest.code << _make_rex(false, false, 0, si & 0x08);
						_dest.code << 0xF7;
						_dest.code << _make_mod(uint(op), mode, si & 0x07);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if (si & 0x08) _dest.code << _make_rex(false, false, 0, si & 0x08);
						_dest.code << 0xF7;
						_dest.code << _make_mod(uint(op), mode, si & 0x07);
					} else if (quant == 1) {
						if (si >= 4) _dest.code << _make_rex(false, false, 0, si & 0x08);
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
				void encode_add(Reg reg, int literal)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << _make_rex(true, false, false, rc & 0x08);
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
					if (quant == 8) {
						_dest.code << _make_rex(true, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
						_dest.code << ((literal >> 8) & 0xFF);
						_dest.code << ((literal >> 16) & 0xFF);
						_dest.code << ((literal >> 24) & 0xFF);
					} else if (quant == 4) {
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
						_dest.code << ((literal >> 8) & 0xFF);
						_dest.code << ((literal >> 16) & 0xFF);
						_dest.code << ((literal >> 24) & 0xFF);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
						_dest.code << ((literal >> 8) & 0xFF);
					} else if (quant == 1) {
						if (ri & 0xC) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF6;
						_dest.code << _make_mod(0, 0x3, ri & 0x7);
						_dest.code << (literal & 0xFF);
					}
				}
				void encode_invert(uint quant, Reg reg)
				{
					auto ri = _regular_register_code(reg);
					if (quant == 8) {
						_dest.code << _make_rex(true, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					} else if (quant == 4) {
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					} else if (quant == 1) {
						if (ri & 0xC) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF6;
						_dest.code << _make_mod(0x2, 0x3, ri & 0x7);
					}
				}
				void encode_negative(uint quant, Reg reg)
				{
					auto ri = _regular_register_code(reg);
					if (quant == 8) {
						_dest.code << _make_rex(true, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					} else if (quant == 4) {
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF7;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					} else if (quant == 1) {
						if (ri & 0xC) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << 0xF6;
						_dest.code << _make_mod(0x3, 0x3, ri & 0x7);
					}
				}
				void encode_shift(uint quant, shOp op, Reg reg, int by = 0)
				{
					uint8 oc, ocx;
					auto ri = _regular_register_code(reg);
					if (by) {
						if (op == shOp::shl) { oc = 0xC0; ocx = 0x4; }
						else if (op == shOp::shr) { oc = 0xC0; ocx = 0x5; }
						else if (op == shOp::sal) { oc = 0xC0; ocx = 0x4; }
						else if (op == shOp::sar) { oc = 0xC0; ocx = 0x7; }
					} else {
						if (op == shOp::shl) { oc = 0xD2; ocx = 0x4; }
						else if (op == shOp::shr) { oc = 0xD2; ocx = 0x5; }
						else if (op == shOp::sal) { oc = 0xD2; ocx = 0x4; }
						else if (op == shOp::sar) { oc = 0xD2; ocx = 0x7; }
					}
					if (quant == 8) {
						_dest.code << _make_rex(true, false, false, ri & 0x8);
						_dest.code << (oc + 1);
						_dest.code << _make_mod(ocx, 0x3, ri & 0x7);
					} else if (quant == 4) {
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << (oc + 1);
						_dest.code << _make_mod(ocx, 0x3, ri & 0x7);
					} else if (quant == 2) {
						_dest.code << 0x66;
						if (ri & 0x8) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << (oc + 1);
						_dest.code << _make_mod(ocx, 0x3, ri & 0x7);
					} else if (quant == 1) {
						if (ri & 0xC) _dest.code << _make_rex(false, false, false, ri & 0x8);
						_dest.code << oc;
						_dest.code << _make_mod(ocx, 0x3, ri & 0x7);
					}
					if (by) _dest.code << by;
				}
				void encode_shl(Reg reg, int bits)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << _make_rex(true, false, false, rc & 0x08);
					_dest.code << 0xC1;
					_dest.code << _make_mod(0x4, 0x3, rc & 0x7);
					_dest.code << bits;
				}
				void encode_shr(Reg reg, int bits)
				{
					uint rc = _regular_register_code(reg);
					_dest.code << _make_rex(true, false, false, rc & 0x08);
					_dest.code << 0xC1;
					_dest.code << _make_mod(0x5, 0x3, rc & 0x7);
					_dest.code << bits;
				}
				void encode_call(Reg func_ptr, bool indirect)
				{
					uint rc = _regular_register_code(func_ptr);
					if (rc & 0x08) _dest.code << _make_rex(false, false, false, true);
					_dest.code << 0xFF;
					_dest.code << _make_mod(0x2, indirect ? 0x00 : 0x03, rc & 0x07);
				}
				void encode_put_addr_of(Reg dest, const ObjectReference & value)
				{
					if (value.ref_class == ReferenceNull) {
						encode_mov_reg_const(8, dest, 0);
					} else if (value.ref_class == ReferenceExternal) {
						encode_mov_reg_const(8, dest, 0);
						_refer_object_at(_src.extrefs[value.index], _dest.code.Length() - 8);
					} else if (value.ref_class == ReferenceData) {
						encode_mov_reg_const(8, dest, value.index);
						_relocate_data_at(_dest.code.Length() - 8);
					} else if (value.ref_class == ReferenceCode) {
						encode_mov_reg_const(8, dest, value.index);
						_relocate_code_at(_dest.code.Length() - 8);
					} else if (value.ref_class == ReferenceArgument) {
						auto & arg = _inputs[value.index];
						if (arg.indirect) encode_mov_reg_mem(8, dest, Reg::RBP, arg.rbp_offset);
						else encode_lea(dest, Reg::RBP, arg.rbp_offset);
					} else if (value.ref_class == ReferenceRetVal) {
						if (_retval.indirect) encode_mov_reg_mem(8, dest, Reg::RBP, _retval.rbp_offset);
						else encode_lea(dest, Reg::RBP, _retval.rbp_offset);
					} else if (value.ref_class == ReferenceLocal) {
						bool found = false;
						for (auto & scp : _scopes) if (scp.first_local_no <= value.index && value.index < scp.first_local_no + scp.locals.Length()) {
							auto & local = scp.locals[value.index - scp.first_local_no];
							encode_lea(dest, Reg::RBP, local.rbp_offset);
							found = true;
							break;
						}
						if (!found) throw InvalidArgumentException();
					} else if (value.ref_class == ReferenceInit) {
						if (!_init_locals.IsEmpty()) {
							auto & local = _init_locals.GetLast()->GetValue();
							encode_lea(dest, Reg::RBP, local.rbp_offset);
						} else throw InvalidArgumentException();
					} else if (value.ref_class == ReferenceSplitter) {
						auto scope = _scopes.GetLast();
						if (scope && scope->GetValue().current_split_offset) {
							encode_lea(dest, Reg::RBP, scope->GetValue().current_split_offset);
						} else throw InvalidArgumentException();
					} else throw InvalidArgumentException();
				}
				void encode_function_prologue(void)
				{
					SafePointer< Array<_argument_passage_info> > api = _make_interface_layout(_src.retval, _src.inputs.GetBuffer(), _src.inputs.Length());
					_inputs.SetLength(_src.inputs.Length());
					if (_conv == CallingConvention::Windows) {
						int align = WordSize * 9;
						_unroll_base = -WordSize * 9;
						encode_push(Reg::RBP);
						encode_mov_reg_reg(8, Reg::RBP, Reg::RSP);
						encode_push(Reg::RBX);
						encode_push(Reg::RDI);
						encode_push(Reg::RSI);
						encode_push(Reg::R10);
						encode_push(Reg::R11);
						encode_push(Reg::R12);
						encode_push(Reg::R13);
						encode_push(Reg::R14);
						encode_push(Reg::R15);
						_retval.rbp_offset = 0;
						for (int i = 0; i < api->Length(); i++) {
							auto & info = api->ElementAt(i);
							if (info.index >= 0) {
								_inputs[info.index].bound_to = info.reg;
								_inputs[info.index].rbp_offset = WordSize * (2 + i);
								_inputs[info.index].indirect = info.indirect;
							} else {
								_retval.bound_to = info.reg;
								_retval.indirect = info.indirect;
								_retval.rbp_offset = WordSize * (2 + i);
							}
							if (info.reg != Reg::NO) {
								if (_is_xmm_register(info.reg)) {
									encode_mov_reg_xmm(8, Reg::RAX, info.reg);
									encode_mov_mem_reg(8, Reg::RBP, WordSize * (2 + i), Reg::RAX);
								} else encode_mov_mem_reg(8, Reg::RBP, WordSize * (2 + i), info.reg);
							}
						}
						if (!_retval.rbp_offset) {
							_retval.bound_to = _src.retval.semantics == ArgumentSemantics::FloatingPoint ? Reg::XMM0 : Reg::RAX;
							_retval.indirect = false;
							_retval.rbp_offset = -align - WordSize;
							align += WordSize;
							_unroll_base -= WordSize;
							encode_add(Reg::RSP, -WordSize);
						}
						if (align & 0xF) {
							align += WordSize;
							encode_add(Reg::RSP, -WordSize);
						}
						_scope_frame_base = -align;
					} else if (_conv == CallingConvention::Unix) {
						int align = WordSize * 7;
						_unroll_base = -WordSize * 7;
						encode_push(Reg::RBP);
						encode_mov_reg_reg(8, Reg::RBP, Reg::RSP);
						encode_push(Reg::RBX);
						encode_push(Reg::R10);
						encode_push(Reg::R11);
						encode_push(Reg::R12);
						encode_push(Reg::R13);
						encode_push(Reg::R14);
						encode_push(Reg::R15);
						if (!_is_pass_by_reference(_src.retval)) {
							_retval.bound_to = _src.retval.semantics == ArgumentSemantics::FloatingPoint ? Reg::XMM0 : Reg::RAX;
							_retval.indirect = false;
							_retval.rbp_offset = -align - WordSize;
							align += WordSize;
							_unroll_base -= WordSize;
							encode_add(Reg::RSP, -WordSize);
						}
						int stack_space_offset = 2 * WordSize;
						for (auto & info : *api) {
							_argument_storage_spec * spec;
							if (info.index >= 0) spec = &_inputs[info.index];
							else spec = &_retval;
							spec->bound_to = info.reg;
							spec->indirect = info.indirect;
							if (info.reg == Reg::NO) {
								spec->rbp_offset = stack_space_offset;
								stack_space_offset += WordSize;
							} else {
								if (_is_xmm_register(info.reg)) {
									encode_mov_reg_xmm(8, Reg::RAX, info.reg);
									encode_push(Reg::RAX);
								} else encode_push(info.reg);
								spec->rbp_offset = -align - WordSize;
								align += WordSize;
							}	
						}
						if (align & 0xF) {
							align += WordSize;
							encode_add(Reg::RSP, -WordSize);
						}
						_scope_frame_base = -align;
					}
				}
				void encode_function_epilogue(void)
				{
					if (_conv == CallingConvention::Windows) {
						if (_unroll_base != _scope_frame_base) encode_lea(Reg::RSP, Reg::RBP, _unroll_base);
						if (!_is_pass_by_reference(_src.retval)) {
							encode_pop(Reg::RAX);
							if (_retval.bound_to == Reg::XMM0) {
								auto quant = _src.retval.size.num_bytes + WordSize * _src.retval.size.num_words;
								encode_mov_xmm_reg(quant, Reg::XMM0, Reg::RAX);
							}
						} else encode_mov_reg_mem(8, Reg::RAX, Reg::RBP, _retval.rbp_offset);
						encode_pop(Reg::R15);
						encode_pop(Reg::R14);
						encode_pop(Reg::R13);
						encode_pop(Reg::R12);
						encode_pop(Reg::R11);
						encode_pop(Reg::R10);
						encode_pop(Reg::RSI);
						encode_pop(Reg::RDI);
						encode_pop(Reg::RBX);
						encode_pop(Reg::RBP);
						encode_pure_ret();
					} else if (_conv == CallingConvention::Unix) {
						if (_unroll_base != _scope_frame_base) encode_lea(Reg::RSP, Reg::RBP, _unroll_base);
						if (!_is_pass_by_reference(_src.retval)) {
							encode_pop(Reg::RAX);
							if (_retval.bound_to == Reg::XMM0) {
								auto quant = _src.retval.size.num_bytes + WordSize * _src.retval.size.num_words;
								encode_mov_xmm_reg(quant, Reg::XMM0, Reg::RAX);
							}
						} else encode_mov_reg_mem(8, Reg::RAX, Reg::RBP, _retval.rbp_offset);
						encode_pop(Reg::R15);
						encode_pop(Reg::R14);
						encode_pop(Reg::R13);
						encode_pop(Reg::R12);
						encode_pop(Reg::R11);
						encode_pop(Reg::R10);
						encode_pop(Reg::RBX);
						encode_pop(Reg::RBP);
						encode_pure_ret();
					}
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
							new_var.rbp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
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
							_encode_expression_evaluation(inst.tree, Reg::R15);
							encode_test(1, Reg::R15, 0xFF);
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
						EncoderContext ctx(_conv, dest, src);
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