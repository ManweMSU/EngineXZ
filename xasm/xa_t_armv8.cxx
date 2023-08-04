#include "xa_t_armv8.h"
#include "xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace ARMv8
		{
			constexpr int WordSize = 8;

			enum class Reg : uint {
				NO  = 0,
				X0  = 0x00000001, X1  = 0x00000002, X2  = 0x00000004, X3  = 0x00000008,
				X4  = 0x00000010, X5  = 0x00000020, X6  = 0x00000040, X7  = 0x00000080,
				X8  = 0x00000100, X9  = 0x00000200, X10 = 0x00000400, X11 = 0x00000800,
				X12 = 0x00001000, X13 = 0x00002000, X14 = 0x00004000, X15 = 0x00008000,
				X16 = 0x00010000, X17 = 0x00020000, X18 = 0x00040000, X19 = 0x00080000,
				X20 = 0x00100000, X21 = 0x00200000, X22 = 0x00400000, X23 = 0x00800000,
				X24 = 0x01000000, X25 = 0x02000000, X26 = 0x04000000, X27 = 0x08000000,
				X28 = 0x10000000, X29 = 0x20000000, X30 = 0x40000000, XZ  = 0x80000000,
				FP  = 0x20000000, LR  = 0x40000000, SP  = 0x80000000
			};
			enum class VReg : uint {
				NO  = 0,
				V0  = 0x00000001, V1  = 0x00000002, V2  = 0x00000004, V3  = 0x00000008,
				V4  = 0x00000010, V5  = 0x00000020, V6  = 0x00000040, V7  = 0x00000080,
				V8  = 0x00000100, V9  = 0x00000200, V10 = 0x00000400, V11 = 0x00000800,
				V12 = 0x00001000, V13 = 0x00002000, V14 = 0x00004000, V15 = 0x00008000,
				V16 = 0x00010000, V17 = 0x00020000, V18 = 0x00040000, V19 = 0x00080000,
				V20 = 0x00100000, V21 = 0x00200000, V22 = 0x00400000, V23 = 0x00800000,
				V24 = 0x01000000, V25 = 0x02000000, V26 = 0x04000000, V27 = 0x08000000,
				V28 = 0x10000000, V29 = 0x20000000, V30 = 0x40000000, V31 = 0x80000000,
			};
			enum class Cond : uint {
				Z  = 0x0, NZ = 0x1, C  = 0x2, NC = 0x3,
				S  = 0x4, NS = 0x5, O  = 0x6, NO = 0x7,
				A  = 0x8, BE = 0x9, GE = 0xA, L  = 0xB,
				G  = 0xC, LE = 0xD, ALWAYS = 0xE,
			};
			enum DispositionFlags {
				DispositionRegister	= 0x01,
				DispositionPointer	= 0x02,
				DispositionDiscard	= 0x04,
				DispositionSigned	= 0x10,
				DispositionAny		= DispositionRegister | DispositionPointer
			};

			class EncoderContext
			{
				struct _vector_argument_reload { uint size; VReg reg; };
				struct _argument_passage_info {
					int index;				// 0, 1, ... - real arguments; -1 - retval
					Reg reg_min, reg_max;	// a range of GP registers for argument
					VReg vreg;				// a vector register for argument
					int stack_offset;		// stack offset relative to top of frame pair storage
					bool indirect;			// pass-by-reference
				};
				struct _argument_storage_spec {
					Reg reg_min, reg_max;
					VReg vreg;
					int fp_offset;
					bool indirect;
				};
				struct _internal_disposition {
					int flags;
					Reg reg;
					int size;
				};
				struct _local_disposition {
					int fp_offset;
					int size;
					FinalizerReference finalizer;
				};
				struct _local_scope {
					int frame_base; // FP + frame_base = real frame base
					int frame_size;
					int frame_size_unused;
					int first_local_no;
					int current_split_offset;
					Array<_local_disposition> locals = Array<_local_disposition>(0x20);
					bool shift_sp, temporary;
				};
				struct _arm_reference {
					uint word_offset;
					uint word_reference_mask;
					uint word_reference_offset;
					uint word_offset_right;
				};
				struct _arm_global_reference {
					bool processed;
					_arm_reference ref;
					uint ref_class;
					uint ref_index;
				};
				struct _jump_reloc_struct {
					_arm_reference machine_word_at;
					uint xasm_offset_jump_to;
				};

				CallingConvention _conv;
				TranslatedFunction & _dest;
				const Function & _src;
				Array<uint> _org_inst_offsets;
				Array<_jump_reloc_struct> _jump_reloc;
				int _current_instruction, _frame_base;
				_argument_storage_spec _retval;
				Array<_argument_storage_spec> _inputs;
				Volumes::Stack<_local_scope> _scopes;
				Volumes::Stack<_local_disposition> _init_locals;
				Array<_arm_global_reference> _global_refs;

				static uint _reg_code(Reg reg)
				{
					uint mask = uint(reg), result = 0;
					if (!mask) return 0;
					while (!(mask & 1)) { mask >>= 1; result++; }
					return result;
				}
				static uint _reg_code(VReg reg)
				{
					uint mask = uint(reg), result = 0;
					if (!mask) return 0;
					while (!(mask & 1)) { mask >>= 1; result++; }
					return result;
				}
				static uint _size_eval(const ObjectSize & size) { return size.num_bytes + WordSize * size.num_words; }
				static uint _word_align(uint size) { return (uint64(size) + 7) / 8 * 8; }
				static uint _word_align(const ObjectSize & size) { return _word_align(_size_eval(size)); }
				static uint _dword_align(uint size) { return (uint64(size) + 15) / 16 * 16; }
				static uint _dword_align(const ObjectSize & size) { return _dword_align(_size_eval(size)); }
				static Reg _make_reg(int index) { return static_cast<Reg>(1 << index); }
				static VReg _make_vreg(int index) { return static_cast<VReg>(1 << index); }
				static bool _is_pass_by_ref(const ArgumentSpecification & spec) { return _word_align(spec.size) > 16 || spec.semantics == ArgumentSemantics::Object; }
				static Reg _reg_alloc(uint reg_in_use, uint reg_prohibit)
				{
					for (int i = 0; i < 18; i++) { uint m = 1 << i; if (!(reg_in_use & m) && !(reg_prohibit & m)) return _make_reg(i); }
					for (int i = 0; i < 18; i++) { uint m = 1 << i; if (!(reg_prohibit & m)) return _make_reg(i); }
					throw InvalidArgumentException();
				}
				Array<_argument_passage_info> * _make_interface_layout(const ArgumentSpecification & output, const ArgumentSpecification * inputs, int in_cnt)
				{
					SafePointer< Array<_argument_passage_info> > result = new Array<_argument_passage_info>(0x20);
					int gri = 0, sri = 0, spo = 0;
					for (int i = 0; i < in_cnt; i++) if (inputs[i].semantics == ArgumentSemantics::This && _conv == CallingConvention::Windows) {
						_argument_passage_info info;
						info.index = i;
						info.indirect = _is_pass_by_ref(inputs[i]);
						info.reg_min = info.reg_max = _make_reg(gri);
						info.vreg = VReg::NO;
						info.stack_offset = -1;
						gri++;
						result->Append(info);
					}
					if (_is_pass_by_ref(output)) {
						Reg reg;
						if (_conv == CallingConvention::Windows) { reg = _make_reg(gri); gri++; }
						else if (_conv == CallingConvention::Unix) { reg = Reg::X8; }
						else throw InvalidArgumentException();
						_argument_passage_info info;
						info.index = -1;
						info.reg_min = info.reg_max = reg;
						info.vreg = VReg::NO;
						info.stack_offset = -1;
						info.indirect = true;
						result->Append(info);
					}
					for (int i = 0; i < in_cnt; i++) {
						if (inputs[i].semantics == ArgumentSemantics::This && _conv == CallingConvention::Windows) continue;
						auto & spec = inputs[i];
						_argument_passage_info info;
						info.index = i;
						info.indirect = _is_pass_by_ref(spec);
						if (spec.semantics == ArgumentSemantics::FloatingPoint && !info.indirect) {
							if (sri < 8) {
								info.reg_min = info.reg_max = Reg::NO;
								info.vreg = _make_vreg(sri);
								info.stack_offset = -1;
								sri++;
							} else {
								while (spo & 0x7) spo++;
								info.reg_min = info.reg_max = Reg::NO;
								info.vreg = VReg::NO;
								info.stack_offset = spo;
							}
						} else {
							if (gri < 8 && (info.indirect || _word_align(spec.size) <= 8)) {
								info.reg_min = info.reg_max = _make_reg(gri);
								info.vreg = VReg::NO;
								info.stack_offset = -1;
								gri++;
							} else if (gri < 7 && !info.indirect && _word_align(spec.size) > 8) {
								info.reg_min = _make_reg(gri);
								info.reg_max = _make_reg(gri + 1);
								info.vreg = VReg::NO;
								info.stack_offset = -1;
								gri += 2;
							} else {
								int size = _size_eval(spec.size);
								gri = 8;
								if (_conv == CallingConvention::Windows) {
									if (size < 16) while (spo & 0x7) spo++;
									else while (spo & 0xF) spo++;
								} else if (_conv == CallingConvention::Unix) {
									if (size == 1);
									else if (size < 4) while (spo & 0x1) spo++;
									else if (size < 8) while (spo & 0x3) spo++;
									else if (size < 16) while (spo & 0x7) spo++;
									else while (spo & 0xF) spo++;
								}
								info.reg_min = info.reg_max = Reg::NO;
								info.vreg = VReg::NO;
								info.stack_offset = spo;
							}
						}
						result->Append(info);
					}
					result->Retain();
					return result;
				}
				int _allocate_temporary(const ObjectSize & size, const FinalizerReference & final)
				{
					auto current_scope_ptr = _scopes.GetLast();
					if (!current_scope_ptr) throw InvalidArgumentException();
					auto & scope = current_scope_ptr->GetValue();
					auto size_padded = _word_align(size);
					_local_disposition new_var;
					new_var.size = size.num_bytes + WordSize * size.num_words;
					new_var.fp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
					new_var.finalizer = final;
					scope.frame_size_unused -= size_padded;
					if (scope.frame_size_unused < 0) throw InvalidArgumentException();
					scope.locals << new_var;
					return new_var.fp_offset;
				}
				void _encode_push(Reg reg) { encode_sub(Reg::SP, Reg::SP, 16); encode_store(8, Reg::SP, 0, reg); }
				void _encode_pop(Reg reg) { encode_load(8, false, reg, Reg::SP); encode_add(Reg::SP, Reg::SP, 16); }
				void _encode_preserve(uint mask_preserve, uint mask_in_use, uint mask_enforce, bool cond)
				{
					if (!cond) return;
					for (int i = 0; i < 32; i++) {
						uint m = 1 << i;
						if ((m & mask_preserve) && (m & mask_in_use) && !(m & mask_enforce)) _encode_push(_make_reg(i));
					}
				}
				void _encode_restore(uint mask_preserve, uint mask_in_use, uint mask_enforce, bool cond)
				{
					if (!cond) return;
					for (int i = 31; i >= 0; i--) {
						uint m = 1 << i;
						if ((m & mask_preserve) && (m & mask_in_use) && !(m & mask_enforce)) _encode_pop(_make_reg(i));
					}
				}
				void _encode_transform_to_pointer(Reg reg, uint reg_in_use)
				{
					int offset = _allocate_temporary(TH::MakeSize(0, 1), TH::MakeFinal());
					if (-offset < 0xFF) {
						encode_store(8, Reg::FP, offset, reg);
					} else {
						auto r = _reg_alloc(reg_in_use, uint(reg));
						_encode_preserve(uint(r), reg_in_use, 0, true);
						encode_emulate_lea(r, Reg::FP, offset);
						encode_store(8, r, 0, reg);
						_encode_restore(uint(r), reg_in_use, 0, true);
					}
					encode_emulate_lea(reg, Reg::FP, offset);
				}
				void _encode_load_imm(Reg dest, _arm_reference & ref)
				{
					ref.word_offset = _dest.code.Length();
					ref.word_reference_mask = 0x00FFFFE0;
					ref.word_reference_offset = 3;
					ref.word_offset_right = 0;
					encode_uint32(_reg_code(dest) | 0x58000000);
				}
				void _encode_resolve_reference(const _arm_reference & ref, uint with_addr)
				{
					auto & word = *reinterpret_cast<uint32 *>(_dest.code.GetBuffer() + ref.word_offset);
					word &= ~ref.word_reference_mask;
					if (ref.word_offset_right) word |= ((with_addr - ref.word_offset) >> ref.word_reference_offset) & ref.word_reference_mask;
					else word |= ((with_addr - ref.word_offset) << ref.word_reference_offset) & ref.word_reference_mask;
				}
				void _encode_open_scope(int of_size, bool temporary, int enf_base)
				{
					if (!enf_base) of_size = _dword_align(of_size);
					auto prev = _scopes.GetLast();
					_local_scope ls;
					ls.frame_base = enf_base ? enf_base : (prev ? prev->GetValue().frame_base - of_size : _frame_base - of_size);
					ls.frame_size = of_size;
					ls.frame_size_unused = of_size;
					ls.current_split_offset = 0;
					ls.first_local_no = prev ? prev->GetValue().first_local_no + prev->GetValue().locals.Length() : 0;
					ls.shift_sp = of_size && !enf_base;
					ls.temporary = temporary;
					_scopes.Push(ls);
					if (ls.shift_sp) encode_sub(Reg::SP, Reg::SP, of_size);
				}
				void _encode_finalize_scope(const _local_scope & scope, uint reg_in_use = 0)
				{
					_encode_preserve(0x1FFFF, reg_in_use, 0, true);
					for (auto & l : scope.locals) if (l.finalizer.final.ref_class != ReferenceNull) {
						bool skip = false;
						for (auto & i : _init_locals) if (i.fp_offset == l.fp_offset) { skip = true; break; }
						if (skip) continue;
						int stack_growth = _dword_align(max(l.finalizer.final_args.Length() - 7, 0) * 8);
						encode_sub(Reg::SP, Reg::SP, stack_growth);
						for (int i = l.finalizer.final_args.Length() - 1; i >= 0; i--) {
							auto & arg = l.finalizer.final_args[i];
							if (i == 0) encode_put_addr_of(Reg::X1, arg);
							else if (i == 1) encode_put_addr_of(Reg::X2, arg);
							else if (i == 2) encode_put_addr_of(Reg::X3, arg);
							else if (i == 3) encode_put_addr_of(Reg::X4, arg);
							else if (i == 4) encode_put_addr_of(Reg::X5, arg);
							else if (i == 5) encode_put_addr_of(Reg::X6, arg);
							else if (i == 6) encode_put_addr_of(Reg::X7, arg);
							else { encode_put_addr_of(Reg::X0, arg); encode_store(8, Reg::SP, (i - 7) * 8, Reg::X0); }
						}
						encode_emulate_lea(Reg::X0, Reg::FP, l.fp_offset);
						encode_put_addr_of(Reg::X16, l.finalizer.final);
						encode_branch_call(Reg::X16);
						if (stack_growth) encode_add(Reg::SP, Reg::SP, stack_growth);
					}
					_encode_restore(0x1FFFF, reg_in_use, 0, true);
					if (scope.shift_sp) encode_add(Reg::SP, Reg::SP, scope.frame_size);
				}
				void _encode_close_scope(uint reg_in_use = 0)
				{
					auto scope = _scopes.Pop();
					_encode_finalize_scope(scope, reg_in_use);
				}
				void _encode_blt(Reg dest_ptr, Reg src, bool src_indirect, int size, uint reg_in_use, bool unaligned)
				{
					if (src_indirect) {
						auto ir = _reg_alloc(reg_in_use, uint(dest_ptr) | uint(src));
						_encode_preserve(uint(ir), reg_in_use, 0, true);
						if (unaligned) {
							uint offs = 0;
							uint rest = size;
							while (rest) {
								encode_load(1, false, ir, src, offs);
								encode_store(1, dest_ptr, offs, ir);
								offs++; rest--;
							}
						} else {
							uint offs = 0;
							uint rest = size;
							while (rest) {
								if (rest >= 8) {
									encode_load(8, false, ir, src, offs);
									encode_store(8, dest_ptr, offs, ir);
									offs += 8; rest -= 8;
								} else if (rest >= 4) {
									encode_load(4, false, ir, src, offs);
									encode_store(4, dest_ptr, offs, ir);
									offs += 4; rest -= 4;
								} else if (rest >= 2) {
									encode_load(2, false, ir, src, offs);
									encode_store(2, dest_ptr, offs, ir);
									offs += 2; rest -= 2;
								} else {
									encode_load(1, false, ir, src, offs);
									encode_store(1, dest_ptr, offs, ir);
									offs++; rest--;
								}
							}
						}
						_encode_restore(uint(ir), reg_in_use, 0, true);
					} else {
						if (size != 1 && size != 2 && size != 4 && size != 8) unaligned = true;
						if (unaligned) {
							uint offs = 0;
							uint rest = size;
							while (rest) {
								encode_store(1, dest_ptr, offs, src);
								if (rest > 1) encode_ror(src, src, 8);
								offs++; rest--;
							}
						} else encode_store(size, dest_ptr, 0, src);
					}
				}
				void _encode_arithmetics(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					auto opcode = node.self.index;
					if (opcode == TransformVectorInverse || opcode == TransformVectorIsZero || opcode == TransformVectorNotZero || opcode == TransformIntegerUResize ||
						opcode == TransformIntegerSResize || opcode == TransformIntegerInverse || opcode == TransformIntegerAbs) {
						if (node.inputs.Length() != 1) throw InvalidArgumentException();
						if (node.input_specs.Length() != node.inputs.Length()) throw InvalidArgumentException();
						int size_in = _size_eval(node.input_specs[0].size);
						int size_out = _size_eval(node.retval_spec.size);
						if (size_in != 1 && size_in != 2 && size_in != 4 && size_in != 8) throw InvalidArgumentException();
						if (size_out != 1 && size_out != 2 && size_out != 4 && size_out != 8) throw InvalidArgumentException();
						_internal_disposition core;
						core.flags = DispositionRegister;
						core.size = size_in;
						core.reg = (disp->reg != Reg::NO) ? disp->reg : _reg_alloc(reg_in_use, 0);
						if (opcode == TransformIntegerUResize) {
						} else if (opcode == TransformIntegerSResize || opcode == TransformIntegerInverse || opcode == TransformIntegerAbs) {
							core.flags |= DispositionSigned;
						} else if (disp->flags & DispositionSigned) {
							core.flags |= DispositionSigned;
						}
						_encode_preserve(uint(core.reg), reg_in_use, uint(disp->reg), !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &core, reg_in_use | uint(core.reg));
						if (!idle) {
							if (opcode == TransformVectorInverse) {
								encode_xor_not(core.reg, Reg::XZ, core.reg);
							} else if (opcode == TransformVectorIsZero || opcode == TransformVectorNotZero) {
								_arm_reference jnz, jmp;
								encode_extend(core.reg, core.reg, size_in, false);
								encode_and(Reg::XZ, core.reg, core.reg, true);
								encode_branch_jcc(Cond::NZ, jnz);
								encode_mov_z(core.reg, opcode == TransformVectorIsZero ? 1 : 0, 0);
								encode_branch_jmp(jmp);
								_encode_resolve_reference(jnz, _dest.code.Length());
								encode_mov_z(core.reg, opcode == TransformVectorIsZero ? 0 : 1, 0);
								_encode_resolve_reference(jmp, _dest.code.Length());
							} else if (opcode == TransformIntegerUResize) {
								encode_extend(core.reg, core.reg, size_in, false);
							} else if (opcode == TransformIntegerSResize) {
								encode_extend(core.reg, core.reg, size_in, true);
							} else if (opcode == TransformIntegerInverse) {
								encode_sub(core.reg, Reg::XZ, core.reg, false);
							} else if (opcode == TransformIntegerAbs) {
								encode_extend(core.reg, core.reg, size_in, true);
								auto ir = _reg_alloc(reg_in_use, uint(core.reg));
								_encode_preserve(uint(ir), reg_in_use, 0, true);
								encode_sar(ir, core.reg, 63);
								encode_xor(core.reg, core.reg, ir);
								encode_sub(core.reg, core.reg, ir, false);
								_encode_restore(uint(ir), reg_in_use, 0, true);
							}
						}
						_encode_restore(uint(core.reg), reg_in_use, uint(disp->reg), !idle);
					} else {
						if (node.inputs.Length() != 2) throw InvalidArgumentException();
						if (node.input_specs.Length() != node.inputs.Length()) throw InvalidArgumentException();
						int size = _size_eval(node.retval_spec.size);
						if (size != 1 && size != 2 && size != 4 && size != 8) throw InvalidArgumentException();
						_internal_disposition core, mod;
						core.flags = mod.flags = DispositionRegister;
						core.size = mod.size = size;
						core.reg = (disp->reg != Reg::NO) ? disp->reg : _reg_alloc(reg_in_use, 0);
						mod.reg = _reg_alloc(reg_in_use, uint(core.reg));
						if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformIntegerULE || opcode == TransformIntegerUGE ||
							opcode == TransformIntegerUL || opcode == TransformIntegerUG || opcode == TransformIntegerUMul || opcode == TransformIntegerUDiv || opcode == TransformIntegerUMod) {
						} else if (opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR || opcode == TransformIntegerSLE || opcode == TransformIntegerSGE ||
							opcode == TransformIntegerSL || opcode == TransformIntegerSG || opcode == TransformIntegerSMul || opcode == TransformIntegerSDiv || opcode == TransformIntegerSMod) {
							core.flags |= DispositionSigned;
							mod.flags |= DispositionSigned;
						} else if (disp->flags & DispositionSigned) {
							core.flags |= DispositionSigned;
							mod.flags |= DispositionSigned;
						}
						_encode_preserve(uint(core.reg) | uint(mod.reg), reg_in_use, uint(disp->reg), !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &core, reg_in_use | uint(core.reg));
						_encode_tree_node(node.inputs[1], idle, mem_load, &mod, reg_in_use | uint(core.reg) | uint(mod.reg));
						if (!idle) {
							if (opcode == TransformLogicalSame || opcode == TransformLogicalNotSame) {
								_arm_reference j1, j2, jmp;
								encode_extend(core.reg, core.reg, size, false);
								encode_extend(mod.reg, mod.reg, size, false);
								encode_and(Reg::XZ, core.reg, core.reg, true);
								encode_branch_jcc(opcode == TransformLogicalSame ? Cond::Z : Cond::NZ, j1);
								encode_mov(core.reg, mod.reg);
								encode_branch_jmp(jmp);
								_encode_resolve_reference(j1, _dest.code.Length());
								encode_and(Reg::XZ, mod.reg, mod.reg, true);
								encode_branch_jcc(opcode == TransformLogicalSame ? Cond::NZ : Cond::Z, j2);
								encode_mov_z(core.reg, opcode == TransformLogicalSame ? 1 : 0, 0);
								_encode_resolve_reference(j2, _dest.code.Length());
								_encode_resolve_reference(jmp, _dest.code.Length());
							} else if (opcode == TransformVectorAnd) {
								encode_and(core.reg, core.reg, mod.reg, false);
							} else if (opcode == TransformVectorOr) {
								encode_or(core.reg, core.reg, mod.reg);
							} else if (opcode == TransformVectorXor) {
								encode_xor(core.reg, core.reg, mod.reg);
							} else if (opcode == TransformVectorShiftL || opcode == TransformVectorShiftR || opcode == TransformVectorShiftAL || opcode == TransformVectorShiftAR) {
								encode_extend(mod.reg, mod.reg, size, false);
								if (opcode == TransformVectorShiftR) encode_extend(core.reg, core.reg, size, false);
								else if (opcode == TransformVectorShiftAR) encode_extend(core.reg, core.reg, size, true);
								_arm_reference ja, jmp;
								encode_sub_sf(Reg::XZ, mod.reg, 63);
								encode_branch_jcc(Cond::A, ja);
								if (opcode == TransformVectorShiftR) encode_shr(core.reg, core.reg, mod.reg);
								else if (opcode == TransformVectorShiftAR) encode_sar(core.reg, core.reg, mod.reg);
								else encode_shl(core.reg, core.reg, mod.reg);
								encode_branch_jmp(jmp);
								_encode_resolve_reference(ja, _dest.code.Length());
								if (opcode == TransformVectorShiftAR) encode_sar(core.reg, core.reg, 63);
								else encode_mov_z(core.reg, 0, 0);
								_encode_resolve_reference(jmp, _dest.code.Length());
							} else if (opcode >= TransformIntegerEQ && opcode <= TransformIntegerSG) {
								bool sgn = opcode == TransformIntegerSLE || opcode == TransformIntegerSGE || opcode == TransformIntegerSL || opcode == TransformIntegerSG;
								encode_extend(core.reg, core.reg, size, sgn);
								encode_extend(mod.reg, mod.reg, size, sgn);
								encode_sub(Reg::XZ, core.reg, mod.reg, true);
								if (opcode == TransformIntegerUL) {
									_arm_reference ja, jz, jmp;
									encode_branch_jcc(Cond::A, ja);
									encode_branch_jcc(Cond::Z, jz);
									encode_mov_z(core.reg, 1, 0);
									encode_branch_jmp(jmp);
									_encode_resolve_reference(ja, _dest.code.Length());
									_encode_resolve_reference(jz, _dest.code.Length());
									encode_mov_z(core.reg, 0, 0);
									_encode_resolve_reference(jmp, _dest.code.Length());
								} else if (opcode == TransformIntegerUGE) {
									_arm_reference ja, jz, jmp;
									encode_branch_jcc(Cond::A, ja);
									encode_branch_jcc(Cond::Z, jz);
									encode_mov_z(core.reg, 0, 0);
									encode_branch_jmp(jmp);
									_encode_resolve_reference(ja, _dest.code.Length());
									_encode_resolve_reference(jz, _dest.code.Length());
									encode_mov_z(core.reg, 1, 0);
									_encode_resolve_reference(jmp, _dest.code.Length());
								} else {
									_arm_reference jcc, jmp;
									if (opcode == TransformIntegerEQ) encode_branch_jcc(Cond::Z, jcc);
									else if (opcode == TransformIntegerNEQ) encode_branch_jcc(Cond::NZ, jcc);
									else if (opcode == TransformIntegerULE) encode_branch_jcc(Cond::BE, jcc);
									else if (opcode == TransformIntegerUG) encode_branch_jcc(Cond::A, jcc);
									else if (opcode == TransformIntegerSLE) encode_branch_jcc(Cond::LE, jcc);
									else if (opcode == TransformIntegerSGE) encode_branch_jcc(Cond::GE, jcc);
									else if (opcode == TransformIntegerSL) encode_branch_jcc(Cond::L, jcc);
									else if (opcode == TransformIntegerSG) encode_branch_jcc(Cond::G, jcc);
									encode_mov_z(core.reg, 0, 0);
									encode_branch_jmp(jmp);
									_encode_resolve_reference(jcc, _dest.code.Length());
									encode_mov_z(core.reg, 1, 0);
									_encode_resolve_reference(jmp, _dest.code.Length());
								}
							} else if (opcode == TransformIntegerAdd) {
								encode_add(core.reg, core.reg, mod.reg, false);
							} else if (opcode == TransformIntegerSubt) {
								encode_sub(core.reg, core.reg, mod.reg, false);
							} else if (opcode == TransformIntegerUMul || opcode == TransformIntegerSMul) {
								encode_mul_add(core.reg, Reg::XZ, core.reg, mod.reg);
							} else if (opcode == TransformIntegerUDiv) {
								encode_extend(core.reg, core.reg, size, false);
								encode_extend(mod.reg, mod.reg, size, false);
								encode_div(core.reg, core.reg, mod.reg, false);
							} else if (opcode == TransformIntegerSDiv) {
								encode_extend(core.reg, core.reg, size, true);
								encode_extend(mod.reg, mod.reg, size, true);
								encode_div(core.reg, core.reg, mod.reg, true);
							} else if (opcode == TransformIntegerUMod) {
								auto ir = _reg_alloc(reg_in_use, uint(core.reg) | uint(mod.reg));
								_encode_preserve(uint(ir), reg_in_use, 0, true);
								encode_extend(core.reg, core.reg, size, false);
								encode_extend(mod.reg, mod.reg, size, false);
								encode_div(ir, core.reg, mod.reg, false);
								encode_mul_sub(core.reg, core.reg, mod.reg, ir);
								_encode_restore(uint(ir), reg_in_use, 0, true);
							} else if (opcode == TransformIntegerSMod) {
								auto ir = _reg_alloc(reg_in_use, uint(core.reg) | uint(mod.reg));
								_encode_preserve(uint(ir), reg_in_use, 0, true);
								encode_extend(core.reg, core.reg, size, true);
								encode_extend(mod.reg, mod.reg, size, true);
								encode_div(ir, core.reg, mod.reg, true);
								encode_mul_sub(core.reg, core.reg, mod.reg, ir);
								_encode_restore(uint(ir), reg_in_use, 0, true);
							} else throw InvalidArgumentException();
						}
						_encode_restore(uint(core.reg) | uint(mod.reg), reg_in_use, uint(disp->reg), !idle);
					}
					if (disp->flags & DispositionRegister) {
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionPointer) {
						(*mem_load) += _word_align(TH::MakeSize(0, 1));
						if (!idle) _encode_transform_to_pointer(disp->reg, reg_in_use | uint(disp->reg));
						disp->flags = DispositionPointer;
					}
				}
				void _encode_logics(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					if (node.self.index == TransformLogicalFork) {
						if (node.inputs.Length() != 3) throw InvalidArgumentException();
						_internal_disposition cond, none;
						cond.reg = _reg_alloc(reg_in_use, 0);
						cond.flags = DispositionRegister;
						cond.size = _size_eval(node.input_specs[0].size);
						none.reg = Reg::NO;
						none.flags = DispositionDiscard;
						none.size = 0;
						if (!cond.size || cond.size > 8) throw InvalidArgumentException();
						_encode_preserve(uint(cond.reg), reg_in_use, 0, !idle);
						_encode_tree_node(node.inputs[0], idle, mem_load, &cond, reg_in_use | uint(cond.reg));
						_arm_reference jz, jmp;
						if (!idle) {
							encode_extend(cond.reg, cond.reg, cond.size, false);
							encode_and(Reg::XZ, cond.reg, cond.reg, true);
							_encode_restore(uint(cond.reg), reg_in_use, 0, true);
							encode_branch_jcc(Cond::Z, jz);
						}
						if (!idle) {
							int local_mem_load = 0;
							_encode_tree_node(node.inputs[1], true, &local_mem_load, &none, reg_in_use);
							int ssoffs = _allocate_temporary(TH::MakeSize(local_mem_load, 0), TH::MakeFinal());
							_encode_open_scope(local_mem_load, true, ssoffs);
							_encode_tree_node(node.inputs[1], false, &local_mem_load, &none, reg_in_use);
							_encode_close_scope(reg_in_use);
							encode_branch_jmp(jmp);
							_encode_resolve_reference(jz, _dest.code.Length());
						} else _encode_tree_node(node.inputs[1], true, mem_load, &none, reg_in_use);
						if (!idle) {
							int local_mem_load = 0;
							_encode_tree_node(node.inputs[2], true, &local_mem_load, &none, reg_in_use);
							int ssoffs = _allocate_temporary(TH::MakeSize(local_mem_load, 0), TH::MakeFinal());
							_encode_open_scope(local_mem_load, true, ssoffs);
							_encode_tree_node(node.inputs[2], false, &local_mem_load, &none, reg_in_use);
							_encode_close_scope(reg_in_use);
							_encode_resolve_reference(jmp, _dest.code.Length());
						} else _encode_tree_node(node.inputs[2], true, mem_load, &none, reg_in_use);
						if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					} else {
						if (!node.inputs.Length()) throw InvalidArgumentException();
						Reg local = disp->reg != Reg::NO ? disp->reg : _reg_alloc(reg_in_use, 0);
						_encode_preserve(uint(local), reg_in_use, uint(disp->reg), !idle);
						Array<_arm_reference> ref_list(0x10);
						uint min_size = 0;
						uint rv_size = _size_eval(node.retval_spec.size);
						if (!rv_size || rv_size > 8) throw InvalidArgumentException();
						for (int i = 0; i < node.inputs.Length(); i++) {
							auto & n = node.inputs[i];
							auto size = _size_eval(node.input_specs[i].size);
							if (!size || size > 8) throw InvalidArgumentException();
							if (size < min_size) min_size = size;
							_internal_disposition ld;
							ld.flags = DispositionRegister;
							ld.reg = local;
							ld.size = size;
							if (!idle) {
								int local_mem_load = 0;
								_encode_tree_node(n, true, &local_mem_load, &ld, reg_in_use | uint(local));
								int ssoffs = _allocate_temporary(TH::MakeSize(local_mem_load, 0), TH::MakeFinal());
								_encode_open_scope(local_mem_load, true, ssoffs);
								_encode_tree_node(n, false, &local_mem_load, &ld, reg_in_use | uint(local));
								_encode_close_scope(reg_in_use | uint(local));
							} else _encode_tree_node(n, true, mem_load, &ld, reg_in_use | uint(local));
							if (!idle && i < node.inputs.Length() - 1) {
								encode_extend(local, local, size, false);
								encode_and(Reg::XZ, local, local, true);
								_arm_reference ref;
								if (node.self.index == TransformLogicalAnd) {
									encode_branch_jcc(Cond::Z, ref);
								} else if (node.self.index == TransformLogicalOr) {
									encode_branch_jcc(Cond::NZ, ref);
								} else throw InvalidArgumentException();
								ref_list << ref;
							}
						}
						if (!idle) for (auto & ref : ref_list) _encode_resolve_reference(ref, _dest.code.Length());
						if (disp->flags & DispositionRegister) {
							if (!idle && disp->reg != local) encode_mov(disp->reg, local);
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionPointer) {
							*mem_load += 8;
							if (!idle) {
								_encode_transform_to_pointer(local, reg_in_use | uint(local));
								if (disp->reg != local) encode_mov(disp->reg, local);
							}
							disp->flags = DispositionPointer;
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						}
						_encode_restore(uint(local), reg_in_use, uint(disp->reg), !idle);
					}
				}
				void _encode_general_call(const ExpressionTree & node, bool idle, int * mem_load, _internal_disposition * disp, uint reg_in_use)
				{
					bool indirect, retval_byref, retval_final;
					int first_arg, arg_no;
					if (node.self.ref_class == ReferenceTransform && node.self.index == TransformInvoke) {
						indirect = true; first_arg = 1; arg_no = node.inputs.Length() - 1;
					} else {
						indirect = false; first_arg = 0; arg_no = node.inputs.Length();
					}
					retval_byref = _is_pass_by_ref(node.retval_spec);
					retval_final = node.retval_final.final.ref_class != ReferenceNull;
					SafePointer< Array<_argument_passage_info> > layout = _make_interface_layout(node.retval_spec, node.input_specs.GetBuffer() + first_arg, arg_no);
					_encode_preserve(0x3FFFF, reg_in_use, uint(disp->reg), !idle);
					uint stack_usage = 0;
					for (auto & info : *layout) if (info.stack_offset < 0) {
						uint stack_top = info.stack_offset + _size_eval(node.input_specs[first_arg + info.index].size);
						if (stack_top > stack_usage) stack_usage = stack_top;
					}
					stack_usage = _dword_align(stack_usage);
					if (!idle) encode_sub(Reg::SP, Reg::SP, stack_usage);
					Volumes::Dictionary<Reg, _vector_argument_reload> vec_reload;
					int rv_offset = 0;
					for (auto & info : *layout) {
						if (info.index >= 0) {
							auto & spec = node.input_specs[first_arg + info.index];
							auto & tree = node.inputs[first_arg + info.index];
							_internal_disposition ld;
							ld.size = _size_eval(spec.size);
							if (info.indirect) {
								ld.flags = DispositionPointer;
								ld.reg = info.reg_min != Reg::NO ? info.reg_min : Reg::X17;
							} else {
								if (info.vreg != VReg::NO) {
									ld.flags = DispositionPointer;
									ld.reg = _make_reg(_reg_code(info.vreg) + 9);
									_vector_argument_reload rl;
									rl.reg = info.vreg;
									rl.size = ld.size;
									vec_reload.Append(ld.reg, rl);
								} else if (info.reg_min != Reg::NO && info.reg_max != Reg::NO && info.reg_min != info.reg_max) {
									ld.flags = DispositionPointer;
									ld.reg = info.reg_min;
								} else if (info.reg_min != Reg::NO) {
									ld.flags = DispositionAny;
									ld.reg = info.reg_min;
									if (spec.semantics == ArgumentSemantics::SignedInteger) ld.flags |= DispositionSigned;
								} else {
									ld.flags = DispositionAny;
									ld.reg = Reg::X17;
									if (spec.semantics == ArgumentSemantics::SignedInteger) ld.flags |= DispositionSigned;
								}
							}
							_encode_tree_node(tree, idle, mem_load, &ld, reg_in_use | 0x3FFFF);
							if (!idle) {
								if (info.indirect) {
									if (info.stack_offset >= 0) encode_store(8, Reg::SP, info.stack_offset, ld.reg);
								} else {
									if (info.vreg != VReg::NO) {
									} else if (info.reg_min != Reg::NO && info.reg_max != Reg::NO && info.reg_min != info.reg_max) {
										encode_load(8, false, info.reg_max, info.reg_min, 8);
										encode_load(8, false, info.reg_min, info.reg_min);
									} else if (info.reg_min != Reg::NO) {
										if (ld.flags & DispositionPointer) encode_load(ld.size, spec.semantics == ArgumentSemantics::SignedInteger, info.reg_min, info.reg_min);
									} else {
										if (ld.flags & DispositionPointer) encode_load(ld.size, false, Reg::X17, Reg::X17);
										encode_store(ld.size, Reg::SP, info.stack_offset, Reg::X17);
									}
								}
							}
						} else {
							*mem_load += _word_align(node.retval_spec.size);
							if (!idle) {
								rv_offset = _allocate_temporary(node.retval_spec.size, node.retval_final);
								encode_emulate_lea(info.reg_min, Reg::FP, rv_offset);
							}
						}
					}
					if (indirect) {
						_internal_disposition ld;
						ld.flags = DispositionRegister;
						ld.reg = Reg::X17;
						ld.size = 8;
						_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | 0x3FFFF);
					} else if (!idle) encode_put_addr_of(Reg::X17, node.self);
					if (!idle) {
						for (auto & ld : vec_reload) encode_load(ld.value.size, ld.value.reg, ld.key);
						encode_branch_call(Reg::X17);
						encode_add(Reg::SP, Reg::SP, stack_usage);
					}
					if (!retval_byref) {
						uint size = _size_eval(node.retval_spec.size);
						if (node.retval_spec.semantics == ArgumentSemantics::FloatingPoint) {
							retval_byref = true;
							*mem_load += _word_align(node.retval_spec.size);
							if (!idle) {
								rv_offset = _allocate_temporary(node.retval_spec.size, node.retval_final);
								encode_emulate_lea(Reg::X8, Reg::FP, rv_offset);
								encode_store(_word_align(node.retval_spec.size), Reg::X8, VReg::V0);
							}
						} else if (_size_eval(node.retval_spec.size) > 8) {
							retval_byref = true;
							*mem_load += _word_align(node.retval_spec.size);
							if (!idle) {
								rv_offset = _allocate_temporary(node.retval_spec.size, node.retval_final);
								encode_emulate_lea(Reg::X8, Reg::FP, rv_offset);
								encode_store(8, Reg::X8, 0, Reg::X0);
								encode_store(8, Reg::X8, 8, Reg::X1);
							}
						} else if (retval_final) {
							retval_byref = true;
							*mem_load += _word_align(node.retval_spec.size);
							if (!idle) {
								rv_offset = _allocate_temporary(node.retval_spec.size, node.retval_final);
								encode_emulate_lea(Reg::X8, Reg::FP, rv_offset);
								encode_store(8, Reg::X8, 0, Reg::X0);
							}
						}
					}
					if ((disp->flags & DispositionPointer) && retval_byref) {
						if (!idle) encode_emulate_lea(disp->reg, Reg::FP, rv_offset);
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && !retval_byref) {
						if (!idle && disp->reg != Reg::X0) encode_mov(disp->reg, Reg::X0);
						disp->flags = DispositionRegister;
					} else if ((disp->flags & DispositionPointer) && !retval_byref) {
						*mem_load += WordSize;
						if (!idle) {
							_encode_transform_to_pointer(Reg::X0, reg_in_use | uint(Reg::X0));
							if (disp->reg != Reg::X0) encode_mov(disp->reg, Reg::X0);
						}
						disp->flags = DispositionPointer;
					} else if ((disp->flags & DispositionRegister) && retval_byref) {
						if (!idle) {
							encode_emulate_lea(disp->reg, Reg::FP, rv_offset);
							encode_load(disp->size, disp->flags & DispositionSigned, disp->reg, disp->reg);
						}
						disp->flags = DispositionRegister;
					} else if (disp->flags & DispositionDiscard) {
						disp->flags = DispositionDiscard;
					}
					_encode_restore(0x3FFFF, reg_in_use, uint(disp->reg), !idle);
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
										if (!idle) encode_load(8, false, src.reg, src.reg);
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
										src.size = _size_eval(node.input_specs[0].size);
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionPointer) {
										_internal_disposition src;
										src.flags = DispositionPointer;
										src.reg = disp->reg;
										src.size = _size_eval(node.input_specs[0].size);
										_encode_tree_node(node.inputs[0], idle, mem_load, &src, reg_in_use);
										*mem_load += _word_align(TH::MakeSize(0, 1));
										if (!idle) _encode_transform_to_pointer(src.reg, reg_in_use);
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
									if (disp->reg == Reg::NO) return;
									_internal_disposition base;
									base.flags = DispositionPointer;
									base.reg = disp->reg;
									base.size = _size_eval(node.input_specs[0].size);
									_encode_tree_node(node.inputs[0], idle, mem_load, &base, reg_in_use);
									if (node.inputs[1].self.ref_class == ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint offset = _size_eval(node.input_specs[1].size);
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = _size_eval(node.input_specs[2].size);
										if (!idle && offset * scale) {
											if (scale != 0xFFFFFFFF) encode_add(base.reg, base.reg, offset * scale);
											else encode_sub(base.reg, base.reg, offset);
										}
									} else if (node.inputs[1].self.ref_class != ReferenceLiteral && (node.inputs.Length() == 2 || node.inputs[2].self.ref_class == ReferenceLiteral)) {
										uint scale = 1;
										if (node.inputs.Length() == 3) scale = _size_eval(node.input_specs[2].size);
										auto reg_index = _reg_alloc(reg_in_use, uint(base.reg));
										auto reg_scale = _reg_alloc(reg_in_use, uint(base.reg) | uint(reg_index));
										if (scale < 2) reg_scale = Reg::NO;
										if (scale) {
											_encode_preserve(uint(reg_index) | uint(reg_scale), reg_in_use, 0, !idle);
											_internal_disposition offset;
											offset.flags = DispositionRegister;
											offset.reg = reg_index;
											offset.size = _size_eval(node.input_specs[1].size);
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(base.reg) | uint(reg_index));
											if (!idle) {
												if (scale > 1) {
													encode_load(reg_scale, scale);
													encode_mul_add(base.reg, base.reg, reg_index, reg_scale);
												} else encode_add(base.reg, base.reg, reg_index, false);
											}
											_encode_restore(uint(reg_index) | uint(reg_scale), reg_in_use, 0, !idle);
										}
									} else if (node.inputs.Length() == 3 && node.inputs[1].self.ref_class == ReferenceLiteral && node.inputs[2].self.ref_class != ReferenceLiteral) {
										uint scale = _size_eval(node.input_specs[1].size);
										auto reg_index = _reg_alloc(reg_in_use, uint(base.reg));
										auto reg_scale = _reg_alloc(reg_in_use, uint(base.reg) | uint(reg_index));
										if (scale < 2) reg_scale = Reg::NO;
										if (scale) {
											_encode_preserve(uint(reg_index) | uint(reg_scale), reg_in_use, 0, !idle);
											_internal_disposition offset;
											offset.flags = DispositionRegister;
											offset.reg = reg_index;
											offset.size = _size_eval(node.input_specs[2].size);
											if (offset.size != 8) throw InvalidArgumentException();
											_encode_tree_node(node.inputs[2], idle, mem_load, &offset, reg_in_use | uint(base.reg) | uint(reg_index));
											if (!idle) {
												if (scale > 1) {
													encode_load(reg_scale, scale);
													encode_mul_add(base.reg, base.reg, reg_index, reg_scale);
												} else encode_add(base.reg, base.reg, reg_index, false);
											}
											_encode_restore(uint(reg_index) | uint(reg_scale), reg_in_use, 0, !idle);
										}
									} else {
										auto reg_index = _reg_alloc(reg_in_use, uint(base.reg));
										auto reg_scale = _reg_alloc(reg_in_use, uint(base.reg) | uint(reg_index));
										_encode_preserve(uint(reg_index) | uint(reg_scale), reg_in_use, 0, !idle);
										_internal_disposition offset;
										offset.flags = DispositionRegister;
										offset.reg = reg_index;
										offset.size = _size_eval(node.input_specs[1].size);
										_internal_disposition scale;
										scale.flags = DispositionRegister;
										scale.reg = reg_scale;
										scale.size = _size_eval(node.input_specs[2].size);
										if (offset.size != 8 || scale.size != 8) throw InvalidArgumentException();
										_encode_tree_node(node.inputs[1], idle, mem_load, &offset, reg_in_use | uint(base.reg) | uint(reg_index));
										_encode_tree_node(node.inputs[2], idle, mem_load, &scale, reg_in_use | uint(base.reg) | uint(reg_index) | uint(reg_scale));
										if (!idle) encode_mul_add(base.reg, base.reg, reg_index, reg_scale);
										_encode_restore(uint(reg_index) | uint(reg_scale), reg_in_use, 0, !idle);
									}
									if (disp->flags & DispositionPointer) {
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) encode_load(disp->size, disp->flags & DispositionSigned, disp->reg, disp->reg);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformBlockTransfer) {
									if (node.inputs.Length() != 2) throw InvalidArgumentException();
									uint size = _size_eval(node.input_specs[0].size);
									_internal_disposition dest_d, src_d;
									dest_d = *disp;
									if (dest_d.reg == Reg::NO) dest_d.reg = _reg_alloc(reg_in_use, 0);
									dest_d.size = size;
									dest_d.flags = DispositionPointer;
									src_d.flags = DispositionAny;
									src_d.size = size;
									src_d.reg = _reg_alloc(reg_in_use, uint(dest_d.reg));
									_encode_preserve(uint(dest_d.reg) | uint(src_d.reg), reg_in_use, uint(disp->reg), !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &dest_d, reg_in_use | uint(dest_d.reg));
									_encode_tree_node(node.inputs[1], idle, mem_load, &src_d, reg_in_use | uint(dest_d.reg) | uint(src_d.reg));
									if (!idle) _encode_blt(dest_d.reg, src_d.reg, src_d.flags & DispositionPointer, size, reg_in_use | uint(dest_d.reg) | uint(src_d.reg), node.self.ref_flags & ReferenceFlagUnaligned);
									if ((disp->flags & DispositionRegister) && !(disp->flags & DispositionPointer)) {
										if (!idle) encode_load(disp->size, disp->flags & DispositionSigned, disp->reg, disp->reg);
										disp->flags = DispositionRegister;
									} else {
										if (disp->flags & DispositionAny) *disp = dest_d;
									}
									_encode_restore(uint(dest_d.reg) | uint(src_d.reg), reg_in_use, uint(disp->reg), !idle);
								} else if (node.self.index == TransformInvoke) {
									_encode_general_call(node, idle, mem_load, disp, reg_in_use);
								} else if (node.self.index == TransformTemporary) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = _size_eval(node.retval_spec.size);
									*mem_load += _word_align(node.retval_spec.size);
									_internal_disposition ld;
									ld.flags = DispositionDiscard;
									ld.reg = Reg::NO;
									ld.size = 0;
									int offset;
									if (!idle) {
										offset = _allocate_temporary(node.retval_spec.size, node.retval_final);
										_local_disposition local;
										local.fp_offset = offset;
										local.size = size;
										_init_locals.Push(local);
									}
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use);
									if (!idle) _init_locals.Pop();
									if (disp->flags & DispositionPointer) {
										if (!idle) encode_emulate_lea(disp->reg, Reg::FP, offset);
										disp->flags = DispositionPointer;
									} else if (disp->flags & DispositionRegister) {
										if (!idle) encode_load(8, false, disp->reg, Reg::FP, offset);
										disp->flags = DispositionRegister;
									} else if (disp->flags & DispositionDiscard) {
										disp->flags = DispositionDiscard;
									}
								} else if (node.self.index == TransformBreakIf) {
									if (node.inputs.Length() != 3) throw InvalidArgumentException();
									_encode_tree_node(node.inputs[0], idle, mem_load, disp, reg_in_use);
									_internal_disposition ld;
									ld.flags = DispositionRegister;
									ld.reg = _reg_alloc(reg_in_use, uint(disp->reg));
									ld.size = _size_eval(node.input_specs[1].size);
									_encode_preserve(uint(ld.reg), reg_in_use, 0, !idle);
									_encode_tree_node(node.inputs[1], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									if (!idle) {
										_arm_reference jz, jmp;
										encode_extend(ld.reg, ld.reg, ld.size, false);
										encode_and(Reg::XZ, ld.reg, ld.reg, true);
										encode_branch_jcc(Cond::Z, jz);
										auto scope = _scopes.GetLast();
										while (scope && !scope->GetValue().shift_sp) scope = scope->GetPrevious();
										if (scope) encode_emulate_lea(ld.reg, Reg::FP, scope->GetValue().frame_base);
										else encode_emulate_lea(ld.reg, Reg::FP, _frame_base);
										encode_mov(Reg::SP, ld.reg);
										encode_scope_unroll(_current_instruction, _current_instruction + 1 + int(node.input_specs[2].size.num_bytes));
										encode_branch_jmp(jmp);
										_jump_reloc_struct rs;
										rs.machine_word_at = jmp;
										rs.xasm_offset_jump_to = _current_instruction + 1 + int(node.input_specs[2].size.num_bytes);
										_jump_reloc << rs;
										_encode_resolve_reference(jz, _dest.code.Length());
									}
									_encode_restore(uint(ld.reg), reg_in_use, 0, !idle);
								} else if (node.self.index == TransformSplit) {
									if (node.inputs.Length() != 1) throw InvalidArgumentException();
									auto size = _size_eval(node.input_specs[0].size);
									_internal_disposition ld = *disp;
									if (ld.flags & DispositionDiscard) {
										ld.flags = DispositionPointer;
										ld.reg = _reg_alloc(reg_in_use, 0);
										ld.size = size;
									}
									Reg dest = _reg_alloc(reg_in_use | uint(ld.reg), uint(ld.reg));
									_encode_preserve(uint(ld.reg) | uint(dest), reg_in_use, uint(disp->reg), !idle);
									_encode_tree_node(node.inputs[0], idle, mem_load, &ld, reg_in_use | uint(ld.reg));
									*mem_load += _word_align(node.input_specs[0].size);
									if (!idle) {
										int offset = _allocate_temporary(node.input_specs[0].size, XA::TH::MakeFinal());
										_scopes.GetLast()->GetValue().current_split_offset = offset;
										encode_emulate_lea(dest, Reg::FP, offset);
										_encode_blt(dest, ld.reg, ld.flags & DispositionPointer, size, reg_in_use | uint(ld.reg) | uint(dest), false);
									}
									_encode_restore(uint(ld.reg) | uint(dest), reg_in_use, uint(disp->reg), !idle);
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
							if (!idle) {
								encode_put_addr_of(disp->reg, node.self);
								encode_load(disp->size, disp->flags & DispositionSigned, disp->reg, disp->reg);
							}
							disp->flags = DispositionRegister;
						} else if (disp->flags & DispositionDiscard) {
							disp->flags = DispositionDiscard;
						} else throw InvalidArgumentException();
					}
				}
				void _encode_expression_evaluation(const ExpressionTree & tree, Reg retval_copy)
				{
					if (retval_copy != Reg::NO && _is_pass_by_ref(tree.retval_spec)) throw InvalidArgumentException();
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
					_encode_tree_node(tree, true, &_temp_storage, &disp, uint(retval_copy) | 0xFFFC0000);
					_encode_open_scope(_temp_storage, true, 0);
					_encode_tree_node(tree, false, &_temp_storage, &disp, uint(retval_copy) | 0xFFFC0000);
					_encode_close_scope();
				}
			public:
				EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src) : _conv(conv), _dest(dest), _src(src), _org_inst_offsets(0x200), _global_refs(0x100)
				{
					_org_inst_offsets.SetLength(_src.instset.Length());
					_inputs.SetLength(_src.inputs.Length());
					_current_instruction = 0;
				}
				void encode_uint32(uint32 word) { _dest.code << word; _dest.code << (word >> 8); _dest.code << (word >> 16); _dest.code << (word >> 24); }
				void encode_uint64(uint64 word) { encode_uint32(word); encode_uint32(word >> 32); }
				void encode_debugger_trap(void) { encode_uint32(0xD4200000); }
				void encode_return(Reg to = Reg::X30) { encode_uint32((_reg_code(to) << 5) | 0xD65F0000); }
				void encode_mov_z(Reg dest, uint literal, uint offset)
				{
					uint hw = offset / 16;
					uint opcode = _reg_code(dest) | ((literal & 0xFFFF) << 5) | ((hw & 0x3) << 21) | 0xD2800000;
					encode_uint32(opcode);
				}
				void encode_mov_k(Reg dest, uint literal, uint offset)
				{
					uint hw = offset / 16;
					uint opcode = _reg_code(dest) | ((literal & 0xFFFF) << 5) | ((hw & 0x3) << 21) | 0xF2800000;
					encode_uint32(opcode);
				}
				void encode_load(uint quant, bool sgn, Reg dest, Reg src_ptr, int offset = 0)
				{
					uint opcode;
					if (sgn) {
						if (quant == 8) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0xF8400000;
						} else if (quant == 4) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0xB8800000;
						} else if (quant == 2) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0x78800000;
						} else if (quant == 1) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0x38800000;
						} else throw InvalidArgumentException();
					} else {
						if (quant == 8) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0xF8400000;
						} else if (quant == 4) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0xB8400000;
						} else if (quant == 2) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0x78400000;
						} else if (quant == 1) {
							opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0x38400000;
						} else throw InvalidArgumentException();
					}
					encode_uint32(opcode);
				}
				void encode_load(uint quant, VReg dest, Reg src_ptr)
				{
					uint opcode = _reg_code(dest) | (_reg_code(src_ptr) << 5);
					if (quant == 8) opcode |= 0x0D408400;
					else if (quant == 4) opcode |= 0x0D408000;
					else throw InvalidArgumentException();
					encode_uint32(opcode);
				}
				void encode_load(Reg dest, uint literal)
				{
					if (literal & 0xFFFF) {
						encode_mov_z(dest, literal, 0);
						if (literal & 0xFFFF0000) encode_mov_k(dest, literal >> 16, 16);
					} else if (literal & 0xFFFF0000) {
						encode_mov_z(dest, literal >> 16, 16);
					} else encode_mov_z(dest, 0, 0);
				}
				void encode_store(uint quant, Reg dest_ptr, int offset, Reg src)
				{
					uint opcode;
					if (quant == 8) {
						opcode = _reg_code(src) | (_reg_code(dest_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0xF8000000;
					} else if (quant == 4) {
						opcode = _reg_code(src) | (_reg_code(dest_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0xB8000000;
					} else if (quant == 2) {
						opcode = _reg_code(src) | (_reg_code(dest_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0x78000000;
					} else if (quant == 1) {
						opcode = _reg_code(src) | (_reg_code(dest_ptr) << 5) | ((uint(offset) << 12) & 0x1FF000) | 0x38000000;
					} else throw InvalidArgumentException();
					encode_uint32(opcode);
				}
				void encode_store(uint quant, Reg dest_ptr, VReg src)
				{
					uint opcode = _reg_code(src) | (_reg_code(dest_ptr) << 5);
					if (quant == 8) opcode |= 0x0D008400;
					else if (quant == 4) opcode |= 0x0D008000;
					else throw InvalidArgumentException();
					encode_uint32(opcode);
				}
				void encode_add(Reg dest, Reg op1, uint op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | ((uint(op2) << 10) & 0x3FFC00) | 0x91000000;
					encode_uint32(opcode);
					if (op2 > 0xFFF) {
						opcode = _reg_code(dest) | (_reg_code(op1) << 5) | ((uint(op2) >> 2) & 0x3FFC00) | 0x91400000;
						encode_uint32(opcode);
					}
				}
				void encode_sub(Reg dest, Reg op1, uint op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | ((uint(op2) << 10) & 0x3FFC00) | 0xD1000000;
					encode_uint32(opcode);
					if (op2 > 0xFFF) {
						opcode = _reg_code(dest) | (_reg_code(op1) << 5) | ((uint(op2) >> 2) & 0x3FFC00) | 0xD1400000;
						encode_uint32(opcode);
					}
				}
				void encode_sub_sf(Reg dest, Reg op1, uint op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | ((uint(op2) << 10) & 0x3FFC00) | 0xF1000000;
					encode_uint32(opcode);
				}
				void encode_ror(Reg dest, Reg op1, uint op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op1) << 16) | ((uint(op2) << 10) & 0xFC00) | 0x93C00000;
					encode_uint32(opcode);
				}
				void encode_sar(Reg dest, Reg op1, uint op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | ((uint(op2) << 16) & 0x3F0000) | 0x9340FC00;
					encode_uint32(opcode);
				}
				void encode_emulate_lea(Reg dest, Reg src_ptr, int offset = 0) { if (offset >= 0) encode_add(dest, src_ptr, offset); else encode_sub(dest, src_ptr, -offset); }
				void encode_mov(Reg dest, Reg src) { encode_add(dest, src, 0); }
				void encode_branch_call(Reg routine) { encode_uint32((_reg_code(routine) << 5) | 0xD63F0000); }
				void encode_branch_jmp(_arm_reference & put_addr)
				{
					put_addr.word_offset = _dest.code.Length();
					put_addr.word_reference_mask = 0x03FFFFFF;
					put_addr.word_reference_offset = 2;
					put_addr.word_offset_right = 1;
					encode_uint32(0x14000000);
				}
				void encode_branch_jcc(Cond cond, _arm_reference & put_addr)
				{
					put_addr.word_offset = _dest.code.Length();
					put_addr.word_reference_mask = 0x00FFFFE0;
					put_addr.word_reference_offset = 3;
					put_addr.word_offset_right = 0;
					encode_uint32(uint(cond) | 0x54000000);
				}
				void encode_mul_add(Reg dest, Reg add, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(add) << 10) | (_reg_code(op2) << 16) | 0x9B000000;
					encode_uint32(opcode);
				}
				void encode_mul_sub(Reg dest, Reg add, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(add) << 10) | (_reg_code(op2) << 16) | 0x9B008000;
					encode_uint32(opcode);
				}
				void encode_mul(Reg dest, Reg op1, Reg op2) { encode_mul_add(dest, Reg::XZ, op1, op2); }
				void encode_add(Reg dest, Reg op1, Reg op2, bool sf)
				{
					uint opcode;
					if (sf) opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xAB000000;
					else opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0x8B000000;
					encode_uint32(opcode);
				}
				void encode_sub(Reg dest, Reg op1, Reg op2, bool sf)
				{
					uint opcode;
					if (sf) opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xEB000000;
					else opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xCB000000;
					encode_uint32(opcode);
				}
				void encode_and(Reg dest, Reg op1, Reg op2, bool sf)
				{
					uint opcode;
					if (sf) opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xEA000000;
					else opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0x8A000000;
					encode_uint32(opcode);
				}
				void encode_or(Reg dest, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xAA000000;
					encode_uint32(opcode);
				}
				void encode_xor(Reg dest, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xCA000000;
					encode_uint32(opcode);
				}
				void encode_xor_not(Reg dest, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0xCA200000;
					encode_uint32(opcode);
				}
				void encode_sar(Reg dest, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0x9AC02800;
					encode_uint32(opcode);
				}
				void encode_shr(Reg dest, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0x9AC02400;
					encode_uint32(opcode);
				}
				void encode_shl(Reg dest, Reg op1, Reg op2)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0x9AC02000;
					encode_uint32(opcode);
				}
				void encode_extend(Reg dest, Reg src, uint valid_bytes, bool sgn)
				{
					if (valid_bytes == 8 && dest == src) return;
					uint opcode = _reg_code(dest) | (_reg_code(src) << 5);
					if (sgn) {
						opcode |= 0x93400000;
						if (valid_bytes == 8) encode_mov(dest, src);
						else if (valid_bytes == 4) opcode |= 0x7C00;
						else if (valid_bytes == 2) opcode |= 0x3C00;
						else if (valid_bytes == 1) opcode |= 0x1C00;
						else throw InvalidArgumentException();
					} else {
						opcode |= 0xD3400000;
						if (valid_bytes == 8) encode_mov(dest, src);
						else if (valid_bytes == 4) opcode |= 0x7C00;
						else if (valid_bytes == 2) opcode |= 0x3C00;
						else if (valid_bytes == 1) opcode |= 0x1C00;
						else throw InvalidArgumentException();
					}
					encode_uint32(opcode);
				}
				void encode_div(Reg dest, Reg op1, Reg op2, bool sgn)
				{
					uint opcode = _reg_code(dest) | (_reg_code(op1) << 5) | (_reg_code(op2) << 16) | 0x9AC00800;
					if (sgn) opcode |= 0x0400;
					encode_uint32(opcode);
				}
				void encode_put_global_address(Reg dest, const ObjectReference & value)
				{
					_arm_global_reference ref;
					_encode_load_imm(dest, ref.ref);
					ref.processed = false;
					ref.ref_class = value.ref_class;
					ref.ref_index = value.index;
					_global_refs << ref;
				}
				void encode_put_addr_of(Reg dest, const ObjectReference & value)
				{
					if (value.ref_class == ReferenceNull) {
						encode_put_global_address(dest, value);
					} else if (value.ref_class == ReferenceExternal) {
						encode_put_global_address(dest, value);
					} else if (value.ref_class == ReferenceData) {
						encode_put_global_address(dest, value);
					} else if (value.ref_class == ReferenceCode) {
						encode_put_global_address(dest, value);
					} else if (value.ref_class == ReferenceArgument) {
						auto & arg = _inputs[value.index];
						if (arg.indirect) encode_load(8, false, dest, Reg::FP, arg.fp_offset);
						else encode_emulate_lea(dest, Reg::FP, arg.fp_offset);
					} else if (value.ref_class == ReferenceRetVal) {
						if (_retval.indirect) encode_load(8, false, dest, Reg::FP, _retval.fp_offset);
						else encode_emulate_lea(dest, Reg::FP, _retval.fp_offset);
					} else if (value.ref_class == ReferenceLocal) {
						bool found = false;
						for (auto & scp : _scopes) if (scp.first_local_no <= value.index && value.index < scp.first_local_no + scp.locals.Length()) {
							auto & local = scp.locals[value.index - scp.first_local_no];
							encode_emulate_lea(dest, Reg::FP, local.fp_offset);
							found = true;
							break;
						}
						if (!found) throw InvalidArgumentException();
					} else if (value.ref_class == ReferenceInit) {
						if (!_init_locals.IsEmpty()) {
							auto & local = _init_locals.GetLast()->GetValue();
							encode_emulate_lea(dest, Reg::FP, local.fp_offset);
						} else throw InvalidArgumentException();
					} else if (value.ref_class == ReferenceSplitter) {
						auto scope = _scopes.GetLast();
						if (scope && scope->GetValue().current_split_offset) {
							encode_emulate_lea(dest, Reg::FP, scope->GetValue().current_split_offset);
						} else throw InvalidArgumentException();
					} else throw InvalidArgumentException();
				}
				void encode_function_prologue(void)
				{
					encode_sub(Reg::SP, Reg::SP, 16);
					encode_store(8, Reg::SP, 8, Reg::LR);
					encode_store(8, Reg::SP, 0, Reg::FP);
					encode_mov(Reg::FP, Reg::SP);
					SafePointer< Array<_argument_passage_info> > api = _make_interface_layout(_src.retval, _src.inputs.GetBuffer(), _src.inputs.Length());
					if (!_is_pass_by_ref(_src.retval) && _src.retval.semantics == ArgumentSemantics::FloatingPoint) {
						_retval.indirect = false;
						_retval.vreg = VReg::V0;
						_retval.reg_min = _retval.reg_max = Reg::NO;
						_retval.fp_offset = -1;
					} else if (!_is_pass_by_ref(_src.retval)) {
						_retval.indirect = false;
						_retval.vreg = VReg::NO;
						_retval.reg_min = _retval.reg_max = (_word_align(_src.retval.size) > 0 ? Reg::X0 : Reg::NO);
						_retval.fp_offset = -1;
						if (_word_align(_src.retval.size) > 8) _retval.reg_max = Reg::X1;
					}
					Reg rv_reg = Reg::NO;
					for (auto & info : *api) {
						_argument_storage_spec * as;
						if (info.index >= 0) as = &_inputs[info.index];
						else { as = &_retval; rv_reg = info.reg_min; }
						as->indirect = info.indirect;
						as->fp_offset = (info.stack_offset < 0) ? -1 : info.stack_offset + 16;
						as->reg_min = info.reg_min;
						as->reg_max = info.reg_max;
						as->vreg = info.vreg;
					}
					_frame_base = 0;
					for (auto & arg : _inputs) {
						if (arg.reg_min != Reg::NO) _frame_base -= 8;
						if (arg.reg_max != Reg::NO && arg.reg_min != arg.reg_max) _frame_base -= 8;
						if (arg.vreg != VReg::NO) _frame_base -= 8;
						if (arg.fp_offset < 0) arg.fp_offset = _frame_base;
					}
					if (_retval.reg_min != Reg::NO) _frame_base -= 8;
					if (_retval.reg_max != Reg::NO && _retval.reg_min != _retval.reg_max) _frame_base -= 8;
					if (_retval.vreg != VReg::NO) _frame_base -= 8;
					if (_retval.fp_offset < 0) _retval.fp_offset = _frame_base;
					if ((-_frame_base) & 0xF) _frame_base -= 8;
					encode_sub(Reg::SP, Reg::FP, -_frame_base);
					for (auto & a : _inputs) {
						if (a.reg_min != Reg::NO) {
							encode_store(8, Reg::FP, a.fp_offset, a.reg_min);
							if (a.reg_max != Reg::NO && a.reg_max != a.reg_min) encode_store(8, Reg::FP, a.fp_offset + 8, a.reg_max);
						}
						if (a.vreg != VReg::NO) {
							encode_emulate_lea(Reg::X9, Reg::FP, a.fp_offset);
							encode_store(_size_eval(_src.retval.size), Reg::X9, a.vreg);
						}
					}
					if (rv_reg != Reg::NO) encode_store(8, Reg::FP, _retval.fp_offset, rv_reg);
				}
				void encode_function_epilogue(void)
				{
					if (_is_pass_by_ref(_src.retval)) {
						encode_load(8, false, _conv == CallingConvention::Unix ? Reg::X8 : Reg::X0, Reg::FP, _retval.fp_offset);
					} else if (_retval.vreg != VReg::NO) {
						encode_emulate_lea(Reg::X8, Reg::FP, _retval.fp_offset);
						encode_load(8, VReg::V0, Reg::X8);
					} else {
						encode_load(8, _src.retval.semantics == ArgumentSemantics::SignedInteger, _retval.reg_min, Reg::FP, _retval.fp_offset);
						if (_retval.reg_max != Reg::NO && _retval.reg_max != _retval.reg_min) {
							encode_load(8, _src.retval.semantics == ArgumentSemantics::SignedInteger, _retval.reg_max, Reg::FP, _retval.fp_offset + 8);
						}
					}
					encode_mov(Reg::SP, Reg::FP);
					encode_load(8, false, Reg::FP, Reg::SP, 0);
					encode_load(8, false, Reg::LR, Reg::SP, 8);
					encode_add(Reg::SP, Reg::SP, 16);
					encode_return();
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
							new_var.fp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
							scope.frame_size_unused -= size_padded;
							if (scope.frame_size_unused < 0) throw InvalidArgumentException();
							_init_locals.Push(new_var);
							_encode_expression_evaluation(inst.tree, Reg::NO);
							_init_locals.Pop();
							new_var.finalizer = inst.attachment_final;
							scope.locals << new_var;
						} else if (inst.opcode == OpcodeUnconditionalJump) {
							_jump_reloc_struct rs;
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							encode_branch_jmp(rs.machine_word_at);
							rs.xasm_offset_jump_to = i + 1 + int(inst.attachment.num_bytes);
							_jump_reloc << rs;
						} else if (inst.opcode == OpcodeConditionalJump) {
							_jump_reloc_struct rs;
							_arm_reference jz;
							_encode_expression_evaluation(inst.tree, Reg::X15);
							encode_extend(Reg::X15, Reg::X15, _size_eval(inst.tree.retval_spec.size), false);
							encode_and(Reg::XZ, Reg::X15, Reg::X15, true);
							encode_branch_jcc(Cond::Z, jz);
							encode_scope_unroll(i, i + 1 + int(inst.attachment.num_bytes));
							encode_branch_jmp(rs.machine_word_at);
							_encode_resolve_reference(jz, _dest.code.Length());
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
						uint offset_to = _org_inst_offsets[rs.xasm_offset_jump_to];
						_encode_resolve_reference(rs.machine_word_at, offset_to);
					}
				}
				void encode_dynamic_references(void)
				{
					for (auto & g : _global_refs) {
						if (g.processed) continue;
						uint offs = _dest.code.Length();
						if (g.ref_class == ReferenceNull || g.ref_class == ReferenceExternal) encode_uint64(0); else encode_uint64(g.ref_index);
						for (auto & g2 : _global_refs) if (!g2.processed && g2.ref_class == g.ref_class && g2.ref_index == g.ref_index) {
							g2.processed = true;
							_encode_resolve_reference(g2.ref, offs);
						}
						if (g.ref_class == ReferenceExternal) {
							auto & name = _src.extrefs[g.ref_index];
							auto ent = _dest.extrefs[name];
							if (!ent) {
								_dest.extrefs.Append(name, Array<uint32>(0x100));
								ent = _dest.extrefs[name];
								if (!ent) return;
							}
							ent->Append(offs);
						} else if (g.ref_class == ReferenceData) {
							_dest.data_reloc << offs;
						} else if (g.ref_class == ReferenceCode) {
							_dest.code_reloc << offs;
						}
					}
				}
			};

			class TranslatorARMv8 : public IAssemblyTranslator
			{
				CallingConvention _conv;
			public:
				TranslatorARMv8(CallingConvention conv) : _conv(conv) {}
				virtual ~TranslatorARMv8(void) override {}
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
						ctx.encode_dynamic_references();
						while (dest.code.Length() & 0xF) ctx.encode_debugger_trap();
						return true;
					} catch (...) { dest.Clear(); return false; }
				}
				virtual uint GetWordSize(void) noexcept override { return 8; }
				virtual Platform GetPlatform(void) noexcept override { return Platform::ARM64; }
				virtual CallingConvention GetCallingConvention(void) noexcept override { return _conv; }
				virtual string ToString(void) const override { return L"XA-ARMv8"; }
			};
		}

		IAssemblyTranslator * CreateTranslatorARMv8(CallingConvention conv) { return new ARMv8::TranslatorARMv8(conv); }
	}
}