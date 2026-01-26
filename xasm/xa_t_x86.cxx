#include "xa_t_x86.h"
#include "xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace X86
		{
			modRM make_modRM(uint reg_i, uint rm_i)
			{
				modRM result;
				result.reg_i = reg_i;
				result.rm_i = rm_i;
				result.x_i = 0;
				result.offset = 0;
				result.x_scale = 0;
				return result;
			}
			modRM make_modRM_with_offset(uint reg_i, uint rm_i, int offset)
			{
				modRM result;
				result.reg_i = reg_i;
				result.rm_i = rm_i;
				result.x_i = 0;
				result.offset = offset;
				result.x_scale = 0;
				return result;
			}
			modRM make_modRM_with_indexing(uint reg_i, uint rm_i, uint x_i, uint scale)
			{
				modRM result;
				result.reg_i = reg_i;
				result.rm_i = rm_i;
				result.x_i = x_i;
				result.offset = 0;
				result.x_scale = scale;
				return result;
			}
			modRM make_modRM_full(uint reg_i, uint rm_i, int offset, uint x_i, uint scale)
			{
				modRM result;
				result.reg_i = reg_i;
				result.rm_i = rm_i;
				result.x_i = x_i;
				result.offset = offset;
				result.x_scale = scale;
				return result;
			}

			EncoderContext::EncoderContext(Environment osenv, TranslatedFunction & dest, const Function & src, bool x64) : _osenv(osenv), _dest(dest), _src(src), _org_inst_offsets(1), _jump_reloc(0x100), _inputs(1), _x64_mode(x64)
			{
				_current_instruction = -1;
				_stack_oddity = _allocated_frame_size = 0;
				_inputs.SetLength(src.inputs.Length());
				_org_inst_offsets.SetLength(src.instset.Length());
			}
			uint32 EncoderContext::logarithm(uint32 value) noexcept
			{
				if (!value) return InvalidLogarithm;
				uint32 r = 0;
				while (value != 1) {
					if (value & 1) return InvalidLogarithm;
					value >>= 1; r++;
				}
				return r;
			}
			uint8 EncoderContext::regular_register_code(uint reg)
			{
				if (reg == Reg64::RAX) return 0;
				else if (reg == Reg64::RCX) return 1;
				else if (reg == Reg64::RDX) return 2;
				else if (reg == Reg64::RBX) return 3;
				else if (reg == Reg64::RSP) return 4;
				else if (reg == Reg64::RBP) return 5;
				else if (reg == Reg64::RSI) return 6;
				else if (reg == Reg64::RDI) return 7;
				else if (reg == Reg64::R8) return 8;
				else if (reg == Reg64::R9) return 9;
				else if (reg == Reg64::R10) return 10;
				else if (reg == Reg64::R11) return 11;
				else if (reg == Reg64::R12) return 12;
				else if (reg == Reg64::R13) return 13;
				else if (reg == Reg64::R14) return 14;
				else if (reg == Reg64::R15) return 15;
				else return 16;
			}
			uint8 EncoderContext::xmm_register_code(uint reg)
			{
				if (reg == Reg64::XMM0) return 0;
				else if (reg == Reg64::XMM1) return 1;
				else if (reg == Reg64::XMM2) return 2;
				else if (reg == Reg64::XMM3) return 3;
				else if (reg == Reg64::XMM4) return 4;
				else if (reg == Reg64::XMM5) return 5;
				else if (reg == Reg64::XMM6) return 6;
				else if (reg == Reg64::XMM7) return 7;
				else if (reg == Reg64::XMM8) return 8;
				else if (reg == Reg64::XMM9) return 9;
				else if (reg == Reg64::XMM10) return 10;
				else if (reg == Reg64::XMM11) return 11;
				else if (reg == Reg64::XMM12) return 12;
				else if (reg == Reg64::XMM13) return 13;
				else if (reg == Reg64::XMM14) return 14;
				else if (reg == Reg64::XMM15) return 15;
				else return 16;
			}
			uint8 EncoderContext::make_rex(bool size64_W, bool reg_ext_R, bool index_ext_X, bool opt_ext_B)
			{
				uint8 result = 0x40;
				if (opt_ext_B) result |= 0x01;
				if (index_ext_X) result |= 0x02;
				if (reg_ext_R) result |= 0x04;
				if (size64_W) result |= 0x08;
				return result;
			}
			uint8 EncoderContext::make_mod(uint8 reg_lo_3, uint8 mod, uint8 reg_mem_lo_3) { return (mod << 6) | (reg_lo_3 << 3) | reg_mem_lo_3; }
			bool EncoderContext::is_vector_retval_transform(uint opcode)
			{
				if (opcode >= 0x080 && opcode < 0x100) {
					if (opcode == TransformFloatInteger || (opcode >= TransformFloatIsZero && opcode <= TransformFloatG)) return false;
					else return true;
				} else return false;
			}
			uint32 EncoderContext::object_size(const ObjectSize & size) { return size.num_bytes + word_size() * size.num_words; }
			uint32 EncoderContext::word_size(void) { return _x64_mode ? 8 : 4; }
			uint32 EncoderContext::word_align(const ObjectSize & size)
			{
				auto wordsize = word_size();
				return (uint64(object_size(size)) + wordsize - 1) / wordsize * wordsize;
			}
			int EncoderContext::allocate_temporary(const ObjectSize & size, int * index)
			{
				auto current_scope_ptr = _scopes.GetLast();
				if (!current_scope_ptr) throw InvalidArgumentException();
				auto & scope = current_scope_ptr->GetValue();
				auto size_padded = word_align(size);
				LocalDisposition new_var;
				new_var.size = object_size(size);
				new_var.bp_offset = scope.frame_base + scope.frame_size_unused - size_padded;
				new_var.finalizer = TH::MakeFinal();
				scope.frame_size_unused -= size_padded;
				if (scope.frame_size_unused < 0) throw InvalidArgumentException();
				if (index) *index = scope.first_local_no + scope.locals.Length();
				scope.locals << new_var;
				return new_var.bp_offset;
			}
			Reg EncoderContext::allocate_xmm(uint xmm_used, uint xmm_protected)
			{
				uint xmm;
				if (_osenv == Environment::Windows || _osenv == Environment::EFI) xmm = 0x003F0000; else xmm = 0x00FF0000;
				uint xmm_free = xmm & ~xmm_used;
				if (!xmm_free) xmm_free = xmm & ~xmm_protected;
				if (!xmm_free) return Reg64::NO;
				xmm = 0x00010000;
				while (!(xmm_free & xmm)) xmm <<= 1;
				return xmm;
			}
			void EncoderContext::allocate_xmm(uint xmm_used, uint xmm_protected, VectorDisposition & disp)
			{
				disp.reg_lo = allocate_xmm(xmm_used, xmm_protected);
				disp.reg_hi = disp.size > 16 ? allocate_xmm(xmm_used | disp.reg_lo, xmm_protected | disp.reg_lo) : Reg64::NO;
			}
			void EncoderContext::assign_finalizer(int local_index, const FinalizerReference & final)
			{
				for (auto & scp : _scopes) if (scp.first_local_no <= local_index && local_index < scp.first_local_no + scp.locals.Length()) {
					auto & local = scp.locals[local_index - scp.first_local_no];
					local.finalizer = final;
					return;
				}
				throw InvalidArgumentException();
			}
			void EncoderContext::relocate_code_at(int offset) { _dest.code_reloc << offset; }
			void EncoderContext::relocate_data_at(int offset) { _dest.data_reloc << offset; }
			void EncoderContext::encode_rex(bool rexW, const modRM & m, bool index_lo_8bit)
			{
				bool use_sib = false;
				if (m.x_scale || (m.rm_i & 0x7) == 0x4) use_sib = true;
				if (use_sib) {
					if (m.x_i == 0x4) throw InvalidArgumentException();
					uint fxx_i = m.x_scale ? m.x_i : 0x4;
					if (_x64_mode && index_lo_8bit) {
						if ((m.reg_i & 0xC) || (m.rm_i & 0x8) || (fxx_i & 0x8) || rexW) _dest.code << make_rex(rexW, m.reg_i & 0x8, fxx_i & 0x8, m.rm_i & 0x8);
					} else {
						if ((m.reg_i & 0x8) || (m.rm_i & 0x8) || (fxx_i & 0x8) || rexW) _dest.code << make_rex(rexW, m.reg_i & 0x8, fxx_i & 0x8, m.rm_i & 0x8);
					}
				} else {
					if (_x64_mode && index_lo_8bit) {
						if ((m.reg_i & 0xC) || (m.rm_i & 0x8) || rexW) _dest.code << make_rex(rexW, m.reg_i & 0x8, false, m.rm_i & 0x8);
					} else {
						if ((m.reg_i & 0x8) || (m.rm_i & 0x8) || rexW) _dest.code << make_rex(rexW, m.reg_i & 0x8, false, m.rm_i & 0x8);
					}
				}
			}
			void EncoderContext::encode_rex(bool rexW, uint reg_i, uint rm_i, bool index_lo_8bit)
			{
				if (((reg_i & 0x8) || (rm_i & 0x8) || rexW) && !_x64_mode) throw InvalidStateException();
				if (_x64_mode && index_lo_8bit) {
					if ((reg_i & 0xC) || (rm_i & 0xC) || rexW) _dest.code << make_rex(rexW, reg_i & 0x8, false, rm_i & 0x8);
				} else {
					if ((reg_i & 0x8) || (rm_i & 0x8) || rexW) _dest.code << make_rex(rexW, reg_i & 0x8, false, rm_i & 0x8);
				}
			}
			void EncoderContext::encode_mod(const modRM & m)
			{
				bool use_sib = false;
				if (m.x_scale || (m.rm_i & 0x7) == 0x4) use_sib = true;
				if (use_sib) {
					if (m.x_i == 0x4) throw InvalidArgumentException();
					uint fxx_i = m.x_scale ? m.x_i : 0x4;
					uint mod, ss;
					if (m.offset) {
						if (m.offset >= -128 && m.offset <= 127) mod = 1;
						else mod = 2;
					} else mod = 0;
					if (m.x_scale) {
						if (m.x_scale == 1) ss = 0;
						else if (m.x_scale == 2) ss = 1;
						else if (m.x_scale == 4) ss = 2;
						else if (m.x_scale == 8) ss = 3;
						else throw InvalidArgumentException();
					} else ss = 0;
					_dest.code << make_mod(m.reg_i & 0x7, mod, 0x4) << make_mod(fxx_i & 0x7, ss, m.rm_i & 0x7);
					if (mod == 2) {
						_dest.code << uint8(m.offset);
						_dest.code << uint8(m.offset >> 8);
						_dest.code << uint8(m.offset >> 16);
						_dest.code << uint8(m.offset >> 24);
					} else if (mod == 1) _dest.code << uint8(m.offset);
				} else {
					uint mod;
					if (m.offset || (m.rm_i & 0x7) == 0x5) {
						if (m.offset >= -128 && m.offset <= 127) mod = 1;
						else mod = 2;
					} else mod = 0;
					_dest.code << make_mod(m.reg_i & 0x7, mod, m.rm_i & 0x7);
					if (mod == 2) {
						_dest.code << uint8(m.offset);
						_dest.code << uint8(m.offset >> 8);
						_dest.code << uint8(m.offset >> 16);
						_dest.code << uint8(m.offset >> 24);
					} else if (mod == 1) _dest.code << uint8(m.offset);
				}
			}
			void EncoderContext::encode_mod(uint reg_i, uint rm_i) { _dest.code << make_mod(reg_i & 0x7, 0x3, rm_i & 0x7); }
			void EncoderContext::refer_object_at(const string & name, int offset)
			{
				auto ent = _dest.extrefs[name];
				if (!ent) {
					_dest.extrefs.Append(name, Array<uint32>(0x100));
					ent = _dest.extrefs[name];
					if (!ent) return;
				}
				ent->Append(offset);
			}
			void EncoderContext::encode_preserve(Reg reg, uint reg_in_use, uint reg_unmask, bool cond_if)
			{
				if (reg < 0x00010000U) {
					if (cond_if && (reg_in_use & reg) && !(reg_unmask & reg)) {
						encode_push(reg);
						_stack_oddity += word_size();
					}
				} else {
					if (cond_if && (reg_in_use & reg) && !(reg_unmask & reg)) {
						encode_stack_alloc(16);
						encode_mov_mem_xmm(16, Reg64::RSP, 0, reg);
						_stack_oddity += 16;
					}
				}
			}
			void EncoderContext::encode_restore(Reg reg, uint reg_in_use, uint reg_unmask, bool cond_if)
			{
				if (reg < 0x00010000U) {
					if (cond_if && (reg_in_use & reg) && !(reg_unmask & reg)) {
						encode_pop(reg);
						_stack_oddity -= word_size();
					}
				} else {
					if (cond_if && (reg_in_use & reg) && !(reg_unmask & reg)) {
						encode_mov_xmm_mem(16, reg, Reg64::RSP, 0);
						encode_stack_dealloc(16);
						_stack_oddity -= 16;
					}
				}
			}
			void EncoderContext::encode_open_scope(int of_size, bool temporary, int enf_base)
			{
				if (!enf_base) {
					of_size = word_align(TH::MakeSize(of_size, 0));
					if (_x64_mode && (of_size & 0xF)) of_size += word_size();
				}
				auto prev = _scopes.GetLast();
				LocalScope ls;
				ls.frame_base = enf_base ? enf_base : (prev ? prev->GetValue().frame_base - of_size : _scope_frame_base - of_size);
				ls.frame_size = of_size;
				ls.frame_size_unused = of_size;
				ls.current_split_offset = 0;
				ls.first_local_no = prev ? prev->GetValue().first_local_no + prev->GetValue().locals.Length() : 0;
				ls.shift_sp = of_size && !enf_base;
				ls.temporary = temporary;
				_scopes.Push(ls);
				if (ls.shift_sp) encode_stack_alloc(of_size);
			}
			void EncoderContext::encode_close_scope(uint reg_in_use)
			{
				auto scope = _scopes.Pop();
				encode_finalize_scope(scope, reg_in_use);
			}
			void EncoderContext::encode_blt(Reg dest, bool dest_indirect, Reg src, bool src_indirect, int size, uint reg_in_use)
			{
				Reg ir;
				if (dest != Reg64::RDX && src != Reg64::RDX) ir = Reg64::RDX;
				else if (dest != Reg64::RAX && src != Reg64::RAX) ir = Reg64::RAX;
				else ir = Reg64::RBX;
				if (dest_indirect && src_indirect) {
					encode_preserve(ir, reg_in_use, 0, true);
					int blt_ofs = 0;
					while (blt_ofs < size) {
						if (blt_ofs + 8 <= size && _x64_mode) {
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
					encode_restore(ir, reg_in_use, 0, true);
				} else if (dest_indirect) {
					if (size == 8) {
						encode_mov_mem_reg(8, dest, src);
					} else if (size == 7) {
						encode_preserve(ir, reg_in_use, 0, true);
						encode_mov_reg_reg(word_size(), ir, src);
						encode_shr(ir, 48);
						encode_mov_mem_reg(1, dest, 6, ir);
						encode_mov_reg_reg(word_size(), ir, src);
						encode_shr(ir, 32);
						encode_mov_mem_reg(2, dest, 4, ir);
						encode_mov_mem_reg(4, dest, src);
						encode_restore(ir, reg_in_use, 0, true);
					} else if (size == 6) {
						encode_preserve(ir, reg_in_use, 0, true);
						encode_mov_reg_reg(word_size(), ir, src);
						encode_shr(ir, 32);
						encode_mov_mem_reg(2, dest, 4, ir);
						encode_mov_mem_reg(4, dest, src);
						encode_restore(ir, reg_in_use, 0, true);
					} else if (size == 5) {
						encode_preserve(ir, reg_in_use, 0, true);
						encode_mov_reg_reg(word_size(), ir, src);
						encode_shr(ir, 32);
						encode_mov_mem_reg(1, dest, 4, ir);
						encode_mov_mem_reg(4, dest, src);
						encode_restore(ir, reg_in_use, 0, true);
					} else if (size == 4) {
						encode_mov_mem_reg(4, dest, src);
					} else if (size == 3) {
						encode_preserve(ir, reg_in_use, 0, true);
						encode_mov_reg_reg(word_size(), ir, src);
						encode_shr(ir, 16);
						encode_mov_mem_reg(1, dest, 2, ir);
						encode_mov_mem_reg(2, dest, src);
						encode_restore(ir, reg_in_use, 0, true);
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
				} else encode_mov_reg_reg(word_size(), dest, src);
			}
			void EncoderContext::encode_blt(Reg dest_ptr, Reg src_ptr, int size, uint reg_in_use) { encode_blt(dest_ptr, true, src_ptr, true, size, reg_in_use); }
			void EncoderContext::encode_reg_load_32(Reg dest, Reg src_ptr, int src_offs, int size, bool allow_compress, uint reg_in_use)
			{
				if (size > 4) {
					if (allow_compress) {
						Reg local = Reg32::ECX;
						if (dest == Reg32::ECX && src_ptr == Reg32::ECX) local = Reg32::EDX;
						if (dest == Reg32::EDX && src_ptr == Reg32::EDX) local = Reg32::EBX;
						encode_preserve(local, reg_in_use, 0, true);
						encode_mov_reg_mem(4, local, src_ptr, src_offs);
						if (size == 8) encode_operation(4, arOp::OR, local, src_ptr, true, src_offs + 4);
						else if (size == 7) {
							encode_operation(2, arOp::OR, local, src_ptr, true, src_offs + 4);
							encode_operation(1, arOp::OR, local, src_ptr, true, src_offs + 6);
						} else if (size == 6) encode_operation(2, arOp::OR, local, src_ptr, true, src_offs + 4);
						else if (size == 5) encode_operation(1, arOp::OR, local, src_ptr, true, src_offs + 4);
						encode_mov_reg_reg(4, dest, local);
						encode_restore(local, reg_in_use, 0, true);
					} else encode_mov_reg_mem(4, dest, src_ptr, src_offs);
				} else {
					if (size == 4) encode_mov_reg_mem(4, dest, src_ptr, src_offs);
					else if (size == 3) {
						bool rest_src = false, rest_local = false;
						if (dest == src_ptr) {
							if (dest != Reg32::EBX) src_ptr = Reg32::EBX;
							else src_ptr = Reg32::ECX;
							rest_src = true;
							encode_preserve(src_ptr, reg_in_use, 0, true);
							encode_mov_reg_reg(4, src_ptr, dest);
						}
						Reg local = dest;
						if (local == Reg32::ESI || local == Reg32::EDI) {
							if (src_ptr != Reg32::EAX) local = Reg32::EAX;
							else local = Reg32::EDX;
							rest_local = true;
						}
						encode_preserve(local, reg_in_use, 0, rest_local);
						encode_mov_reg_mem(1, local, src_ptr, src_offs + 2);
						encode_shl(local, 16);
						encode_mov_reg_mem(2, local, src_ptr, src_offs);
						if (local != dest) encode_mov_reg_reg(4, dest, local);
						encode_restore(local, reg_in_use, 0, rest_local);
						encode_restore(src_ptr, reg_in_use, 0, rest_src);
					} else if (size == 2) encode_mov_reg_mem(2, dest, src_ptr, src_offs);
					else if (size == 1) {
						if (dest == Reg32::ESI || dest == Reg32::EDI) {
							if (src_ptr == Reg32::ESI || src_ptr == Reg32::EDI || src_ptr == Reg32::EBP) {
								encode_preserve(Reg32::ECX, reg_in_use, 0, true);
								encode_mov_reg_mem(1, Reg32::CL, src_ptr, src_offs);
								encode_mov_reg_reg(4, dest, Reg32::ECX);
								encode_restore(Reg32::ECX, reg_in_use, 0, true);
							} else {
								encode_mov_reg_mem(1, src_ptr, src_ptr, src_offs);
								encode_mov_reg_reg(4, dest, src_ptr);
							}
						} else encode_mov_reg_mem(1, dest, src_ptr, src_offs);
					}
				}
			}
			void EncoderContext::encode_emulate_mul_64(void) // uses all registers, input on [EAX], [ECX], output on EBX:ECX
			{
				encode_mov_reg_reg(4, Reg32::ESI, Reg32::EAX);
				encode_mov_reg_reg(4, Reg32::EDI, Reg32::ECX); // [ESI] * [EDI], use ECX:EBX as accumulator
				encode_mov_reg_mem(4, Reg32::EAX, Reg32::ESI);
				encode_mul_div(4, mdOp::MUL, Reg32::EDI, true); // dword [ESI] * dword [EDI] -> EAX:EDX
				encode_mov_reg_reg(4, Reg32::ECX, Reg32::EAX);
				encode_mov_reg_reg(4, Reg32::EBX, Reg32::EDX); // dword [ESI] * dword[EDI] -> ECX:EBX
				encode_mov_reg_mem(4, Reg32::EAX, Reg32::ESI, 4);
				encode_mul_div(4, mdOp::MUL, Reg32::EDI, true); // dword [ESI + 4] * dword [EDI] -> EAX:EDX
				encode_operation(4, arOp::ADD, Reg32::EBX, Reg32::EAX); // summ
				encode_mov_reg_mem(4, Reg32::EAX, Reg32::ESI);
				encode_mul_div(4, mdOp::MUL, Reg32::EDI, true, 4); // dword [ESI] * dword [EDI + 4] -> EAX:EDX
				encode_operation(4, arOp::ADD, Reg32::EBX, Reg32::EAX); // summ, the result is ready
			}
			void EncoderContext::encode_emulate_div_64(void) // uses all registers, input on [EAX], [ECX]; output div on EAX:EDX, mod on ESI:EDI
			{
				encode_mov_reg_reg(4, Reg32::ESI, Reg32::EAX);
				encode_mov_reg_reg(4, Reg32::EDI, Reg32::ECX); // [ESI] / [EDI]
				encode_mov_reg_mem(4, Reg32::EDX, Reg32::EDI, 4);
				encode_mov_reg_mem(4, Reg32::EAX, Reg32::EDI); // [EDI] -> EAX:EDX
				encode_mov_reg_reg(4, Reg32::ECX, Reg32::EAX);
				encode_operation(4, arOp::OR, Reg32::ECX, Reg32::EDX);
				_dest.code << 0x75 << 0x00; // JNZ
				int addr = _dest.code.Length();
				_dest.code << 0xCD << 0x04; // INT 4 (OVERFLOW)
				_dest.code[addr - 1] = _dest.code.Length() - addr;
				encode_operation(4, arOp::XOR, Reg32::ECX, Reg32::ECX); // ECX = 0
				addr = _dest.code.Length();
				encode_test(4, Reg32::EDX, 0x80000000);
				_dest.code << 0x75 << 0x00; // JNZ
				int addr2 = _dest.code.Length();
				encode_shift(4, shOp::SHL, Reg32::EAX, 1);
				encode_shift(4, shOp::RCL, Reg32::EDX, 1);
				encode_add(Reg32::ECX, 1);
				_dest.code << 0xEB; _dest.code << (addr - _dest.code.Length() - 1);
				_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
				encode_mov_reg_mem(4, Reg32::EBX, Reg32::ESI, 4);
				encode_push(Reg32::EBX);
				encode_mov_reg_mem(4, Reg32::EBX, Reg32::ESI);
				encode_push(Reg32::EBX);
				encode_operation(4, arOp::XOR, Reg32::EBX, Reg32::EBX);
				encode_push(Reg32::EBX);
				encode_push(Reg32::EBX);
				encode_mov_reg_reg(4, Reg32::EBX, Reg32::ESP);
				addr = _dest.code.Length();
				encode_mov_reg_mem(4, Reg32::EDI, Reg32::EBX, 12);
				encode_mov_reg_mem(4, Reg32::ESI, Reg32::EBX, 8);
				encode_operation(4, arOp::SUB, Reg32::ESI, Reg32::EAX);
				encode_operation(4, arOp::SBB, Reg32::EDI, Reg32::EDX);
				_dest.code << 0x72 << 0x00; // JB
				addr2 = _dest.code.Length();
				encode_mov_mem_reg(4, Reg32::EBX, 12, Reg32::EDI);
				encode_mov_mem_reg(4, Reg32::EBX, 8, Reg32::ESI);
				encode_shift(4, shOp::SHL, Reg32::EBX, 1, true);
				encode_shift(4, shOp::RCL, Reg32::EBX, 1, true, 4);
				encode_mov_reg_mem(4, Reg32::ESI, Reg32::EBX);
				encode_add(Reg32::ESI, 1);
				encode_mov_mem_reg(4, Reg32::EBX, Reg32::ESI);
				_dest.code << 0xEB << 0x00; // JMP
				_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
				addr2 = _dest.code.Length();
				encode_shift(4, shOp::SHL, Reg32::EBX, 1, true);
				encode_shift(4, shOp::RCL, Reg32::EBX, 1, true, 4);
				_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
				encode_test(4, Reg32::ECX, 0xFFFFFFFF);
				_dest.code << 0x74 << 0x00; // JZ
				addr2 = _dest.code.Length();
				encode_shift(4, shOp::SHR, Reg32::EDX, 1);
				encode_shift(4, shOp::RCR, Reg32::EAX, 1);
				encode_add(Reg32::ECX, -1);
				_dest.code << 0xEB;
				_dest.code << (addr - _dest.code.Length() - 1);
				_dest.code[addr2 - 1] = _dest.code.Length() - addr2;
				encode_pop(Reg32::EAX);
				encode_pop(Reg32::EDX);
				encode_pop(Reg32::ESI);
				encode_pop(Reg32::EDI);
			}
			void EncoderContext::encode_emulate_collect_signum(Reg data, Reg sgn) // pushes abs([data]) onto stack, inverts sgn if [data] is negative. updates [data]. uses ESI and EDI
			{
				encode_mov_reg_mem(4, Reg32::EDI, data, 4);
				encode_mov_reg_mem(4, Reg32::ESI, data); // [data] -> ESI:EDI
				encode_push(Reg32::EDI);
				encode_push(Reg32::ESI);
				encode_mov_reg_reg(4, data, Reg32::ESP);
				encode_test(4, Reg32::EDI, 0x80000000);
				_dest.code << 0x74 << 0x00; // JZ
				int addr = _dest.code.Length();
				encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::ESI);
				encode_operation(4, arOp::XOR, Reg32::EDI, Reg32::EDI);
				encode_operation(4, arOp::SUB, Reg32::ESI, data, true);
				encode_operation(4, arOp::SBB, Reg32::EDI, data, true, 4);
				encode_mov_mem_reg(4, data, Reg32::ESI);
				encode_mov_mem_reg(4, data, 4, Reg32::EDI);
				if (sgn != Reg32::NO) encode_invert(4, sgn);
				_dest.code[addr - 1] = _dest.code.Length() - addr;
			}
			void EncoderContext::encode_emulate_set_signum(Reg data_lo, Reg data_hi, Reg sgn) // inverts data_lo:data_hi if sgn != 0. uses ESI and EDI
			{
				encode_test(4, sgn, 1);
				_dest.code << 0x74 << 0x00; // JZ
				int addr = _dest.code.Length();
				encode_operation(4, arOp::XOR, Reg32::ESI, Reg32::ESI);
				encode_operation(4, arOp::XOR, Reg32::EDI, Reg32::EDI);
				encode_operation(4, arOp::SUB, Reg32::ESI, data_lo);
				encode_operation(4, arOp::SBB, Reg32::EDI, data_hi);
				encode_mov_reg_reg(4, data_lo, Reg32::ESI);
				encode_mov_reg_reg(4, data_hi, Reg32::EDI);
				_dest.code[addr - 1] = _dest.code.Length() - addr;
			}
			void EncoderContext::encode_mov_reg_reg(uint quant, Reg dest, Reg src)
			{
				if (quant != 8 && quant != 4 && quant != 2 && quant != 1) throw InvalidArgumentException();
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src);
				if (quant == 2) _dest.code << 0x66;
				encode_rex(quant == 8, si, di, quant == 1);
				_dest.code << (quant == 1 ? 0x88 : 0x89);
				encode_mod(si, di);
			}
			void EncoderContext::encode_mov_reg_mem(uint quant, const modRM & m)
			{
				if (quant != 8 && quant != 4 && quant != 2 && quant != 1) throw InvalidArgumentException();
				if (quant == 2) _dest.code << 0x66;
				encode_rex(quant == 8, m, quant == 1);
				_dest.code << (quant == 1 ? 0x8A : 0x8B);
				encode_mod(m);
			}
			void EncoderContext::encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr)
			{
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src_ptr);
				encode_mov_reg_mem(quant, make_modRM(di, si));
			}
			void EncoderContext::encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, int src_offset)
			{
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src_ptr);
				encode_mov_reg_mem(quant, make_modRM_with_offset(di, si, src_offset));
			}
			void EncoderContext::encode_mov_reg_const(uint quant, Reg dest, uint64 value)
			{
				if (quant != 8 && quant != 4 && quant != 2 && quant != 1) throw InvalidArgumentException();
				auto di = regular_register_code(dest);
				auto m = make_modRM(0, di);
				if (quant == 2) _dest.code << 0x66;
				encode_rex(quant == 8, m, quant == 1);
				_dest.code << ((quant == 1 ? 0xB0 : 0xB8) | (di & 0x7));
				for (uint q = 0; q < quant; q++) { _dest.code << uint8(value); value >>= 8; }
			}
			void EncoderContext::encode_mov_mem_reg(uint quant, const modRM & m)
			{
				if (quant != 8 && quant != 4 && quant != 2 && quant != 1) throw InvalidArgumentException();
				if (quant == 2) _dest.code << 0x66;
				encode_rex(quant == 8, m, quant == 1);
				_dest.code << (quant == 1 ? 0x88 : 0x89);
				encode_mod(m);
			}
			void EncoderContext::encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg src)
			{
				auto di = regular_register_code(dest_ptr);
				auto si = regular_register_code(src);
				encode_mov_mem_reg(quant, make_modRM(si, di));
			}
			void EncoderContext::encode_mov_mem_reg(uint quant, Reg dest_ptr, int dest_offset, Reg src)
			{
				auto di = regular_register_code(dest_ptr);
				auto si = regular_register_code(src);
				encode_mov_mem_reg(quant, make_modRM_with_offset(si, di, dest_offset));
			}
			void EncoderContext::encode_mov_xmm_xmm(Reg dest, Reg src)
			{
				auto di = xmm_register_code(dest);
				auto si = xmm_register_code(src);
				encode_rex(false, di, si);
				_dest.code << 0x0F << 0x28;
				encode_mod(di, si);
			}
			void EncoderContext::encode_mov_reg_xmm(uint quant, Reg dest, Reg src)
			{
				if (quant != 8 && quant != 4) throw InvalidArgumentException();
				auto di = regular_register_code(dest);
				auto si = xmm_register_code(src);
				_dest.code << 0x66;
				encode_rex(quant == 8, si, di);
				_dest.code << 0x0F << 0x7E;
				encode_mod(si, di);
			}
			void EncoderContext::encode_mov_xmm_reg(uint quant, Reg dest, Reg src)
			{
				if (quant != 8 && quant != 4) throw InvalidArgumentException();
				auto di = xmm_register_code(dest);
				auto si = regular_register_code(src);
				_dest.code << 0x66;
				encode_rex(quant == 8, di, si);
				_dest.code << 0x0F << 0x6E;
				encode_mod(di, si);
			}
			void EncoderContext::encode_mov_mem_xmm(uint quant, Reg dest, int dest_offset, Reg src)
			{
				if (quant != 16 && quant != 12 && quant != 8 && quant != 4) throw InvalidArgumentException();
				auto di = regular_register_code(dest);
				auto si = xmm_register_code(src);
				auto m = make_modRM_with_offset(si, di, dest_offset);
				if (quant == 16) {
					_dest.code << 0x66;
					encode_rex(false, m);
					_dest.code << 0x0F << 0x11;
					encode_mod(m);
				} else if (quant == 8) {
					_dest.code << 0xF2;
					encode_rex(false, m);
					_dest.code << 0x0F << 0x11;
					encode_mod(m);
				} else if (quant == 4) {
					_dest.code << 0xF3;
					encode_rex(false, m);
					_dest.code << 0x0F << 0x11;
					encode_mod(m);
				} else if (quant == 12) {
					encode_mov_mem_xmm(8, dest, dest_offset, src);
					encode_simd_shuffle(4, src, src, 2, 2, 2, 2);
					encode_mov_mem_xmm(4, dest, dest_offset + 8, src);
				}
			}
			void EncoderContext::encode_mov_xmm_mem(uint quant, Reg dest, Reg src, int src_offset)
			{
				if (quant != 16 && quant != 12 && quant != 8 && quant != 4) throw InvalidArgumentException();
				auto di = xmm_register_code(dest);
				auto si = regular_register_code(src);
				auto m = make_modRM_with_offset(di, si, src_offset);
				if (quant == 16) {
					_dest.code << 0x66;
					encode_rex(false, m);
					_dest.code << 0x0F << 0x10;
					encode_mod(m);
				} else if (quant == 8) {
					_dest.code << 0xF2;
					encode_rex(false, m);
					_dest.code << 0x0F << 0x10;
					encode_mod(m);
				} else if (quant == 4) {
					_dest.code << 0xF3;
					encode_rex(false, m);
					_dest.code << 0x0F << 0x10;
					encode_mod(m);
				} else if (quant == 12) {
					encode_mov_xmm_mem(4, dest, src, src_offset + 8);
					encode_mov_xmm_mem_hi(8, dest, src, src_offset);
					encode_simd_shuffle(4, dest, dest, 2, 3, 0, 1);
				}
			}
			void EncoderContext::encode_mov_xmm_mem_hi(uint quant, Reg dest, Reg src, int src_offset)
			{
				if (quant != 8) throw InvalidArgumentException();
				auto di = xmm_register_code(dest);
				auto si = regular_register_code(src);
				auto m = make_modRM_with_offset(di, si, src_offset);
				_dest.code << 0x66;
				encode_rex(false, m);
				_dest.code << 0x0F << 0x16;
				encode_mod(m);
			}
			void EncoderContext::encode_lea(const modRM & m)
			{
				encode_rex(_x64_mode, m);
				_dest.code << 0x8D;
				encode_mod(m);
			}
			void EncoderContext::encode_lea(Reg dest, Reg src_ptr, int src_offset)
			{
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src_ptr);
				encode_lea(make_modRM_with_offset(di, si, src_offset));
			}
			void EncoderContext::encode_mov_sx(uint dest_quant, Reg dest, uint src_quant, Reg src)
			{
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src);
				if (src_quant == 1) {
					if (dest_quant == 2) {
						_dest.code << 0x66;
						if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xBE << make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (dest_quant == 4) {
						if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xBE << make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (dest_quant == 8 && _x64_mode) {
						_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xBE << make_mod(di & 0x07, 0x3, si & 0x07);
					} else throw InvalidStateException();
				} else if (src_quant == 2) {
					if (dest_quant == 4) {
						if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xBF << make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (dest_quant == 8 && _x64_mode) {
						_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xBF << make_mod(di & 0x07, 0x3, si & 0x07);
					} else throw InvalidStateException();
				} else if (src_quant == 4) {
					if (dest_quant == 8 && _x64_mode) {
						_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x63 << make_mod(di & 0x07, 0x3, si & 0x07);
					} else throw InvalidStateException();
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_mov_zx(uint dest_quant, Reg dest, uint src_quant, Reg src)
			{
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src);
				if (src_quant == 1) {
					if (dest_quant == 2) {
						_dest.code << 0x66;
						if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xB6 << make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (dest_quant == 4) {
						if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xB6 << make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (dest_quant == 8 && _x64_mode) {
						_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xB6 << make_mod(di & 0x07, 0x3, si & 0x07);
					} else throw InvalidStateException();
				} else if (src_quant == 2) {
					if (dest_quant == 4) {
						if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xB7 << make_mod(di & 0x07, 0x3, si & 0x07);
					} else if (dest_quant == 8 && _x64_mode) {
						_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
						_dest.code << 0x0F << 0xB7 << make_mod(di & 0x07, 0x3, si & 0x07);
					} else throw InvalidStateException();
				} else if (src_quant == 4) {
					if (dest_quant == 8 && _x64_mode) {
						encode_mov_reg_reg(4, dest, src);
					} else throw InvalidStateException();
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_push(Reg reg)
			{
				uint rc = regular_register_code(reg);
				uint8 rex = make_rex(false, false, false, rc & 0x08);
				if (rex & 0x0F) _dest.code << rex;
				_dest.code << (0x50 + (rc & 0x07));
				if (_x64_mode) _allocated_frame_size += 8;
				else _allocated_frame_size += 4;
			}
			void EncoderContext::encode_pop(Reg reg)
			{
				uint rc = regular_register_code(reg);
				uint8 rex = make_rex(false, false, false, rc & 0x08);
				if (rex & 0x0F) _dest.code << rex;
				_dest.code << (0x58 + (rc & 0x07));
				if (_x64_mode) _allocated_frame_size -= 8;
				else _allocated_frame_size -= 4;
				if (_allocated_frame_size < 0) throw InvalidStateException();
			}
			void EncoderContext::encode_stack_alloc(uint size)
			{
				if (_osenv == Environment::Windows) {
					while (size) {
						if ((_allocated_frame_size + size) / VirtualMemoryPageSize != _allocated_frame_size / VirtualMemoryPageSize) {
							auto paragraph = _x64_mode ? 8 : 4;
							auto prox_page_align = (_allocated_frame_size / VirtualMemoryPageSize + 1) * VirtualMemoryPageSize;
							auto allocate = prox_page_align - _allocated_frame_size;
							if (allocate < paragraph) throw InvalidStateException();
							auto shift = allocate - paragraph;
							size -= allocate;
							if (shift) encode_add(Reg64::RSP, -shift);
							encode_push(Reg64::RBP);
							_allocated_frame_size = prox_page_align;
						} else {
							encode_add(Reg64::RSP, -size);
							_allocated_frame_size += size;
							size = 0;
						}
					}
				} else {
					encode_add(Reg64::RSP, -size);
					_allocated_frame_size += size;
				}
			}
			void EncoderContext::encode_stack_dealloc(uint size)
			{
				encode_add(Reg64::RSP, size);
				_allocated_frame_size -= size;
				if (_allocated_frame_size < 0) throw InvalidStateException();
			}
			void EncoderContext::encode_add(Reg reg, int literal)
			{
				uint rc = regular_register_code(reg);
				if (_x64_mode) _dest.code << make_rex(true, false, false, rc & 0x08);
				if (literal >= -128 && literal <= 127) {
					_dest.code << 0x83 << make_mod(0, 0x3, rc & 0x07);
					_dest.code << int8(literal);
				} else {
					_dest.code << 0x81 << make_mod(0, 0x3, rc & 0x07);
					_dest.code << int8(literal);
					_dest.code << int8(literal >> 8);
					_dest.code << int8(literal >> 16);
					_dest.code << int8(literal >> 24);
				}
			}
			void EncoderContext::encode_and(Reg reg, int literal)
			{
				uint rc = regular_register_code(reg);
				if (_x64_mode) _dest.code << make_rex(true, false, false, rc & 0x08);
				if (literal >= -128 && literal <= 127) {
					_dest.code << 0x83 << make_mod(4, 0x3, rc & 0x07);
					_dest.code << int8(literal);
				} else {
					_dest.code << 0x81 << make_mod(4, 0x3, rc & 0x07);
					_dest.code << int8(literal);
					_dest.code << int8(literal >> 8);
					_dest.code << int8(literal >> 16);
					_dest.code << int8(literal >> 24);
				}
			}
			void EncoderContext::encode_xor(Reg reg, int literal)
			{
				uint rc = regular_register_code(reg);
				if (_x64_mode) _dest.code << make_rex(true, false, false, rc & 0x08);
				if (literal >= -128 && literal <= 127) {
					_dest.code << 0x83 << make_mod(6, 0x3, rc & 0x07);
					_dest.code << int8(literal);
				} else {
					_dest.code << 0x81 << make_mod(6, 0x3, rc & 0x07);
					_dest.code << int8(literal);
					_dest.code << int8(literal >> 8);
					_dest.code << int8(literal >> 16);
					_dest.code << int8(literal >> 24);
				}
			}
			void EncoderContext::encode_xadd(uint quant, Reg dest_ptr, Reg src)
			{
				if (dest_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest_ptr);
				auto si = regular_register_code(src);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, si & 0x08, 0, di & 0x08);
					_dest.code << 0x0F << 0xC1 << make_mod(si & 0x07, 0, di & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x0F << 0xC1 << make_mod(si & 0x07, 0, di & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x0F << 0xC1 << make_mod(si & 0x07, 0, di & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x0F << 0xC0 << make_mod(si & 0x07, 0, di & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_xchg(uint quant, Reg dest_ptr, Reg src)
			{
				if (dest_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest_ptr);
				auto si = regular_register_code(src);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, si & 0x08, 0, di & 0x08);
					_dest.code << 0x87 << make_mod(si & 0x07, 0, di & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x87 << make_mod(si & 0x07, 0, di & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x87 << make_mod(si & 0x07, 0, di & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x86 << make_mod(si & 0x07, 0, di & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_test(uint quant, Reg reg, int literal)
			{
				auto ri = regular_register_code(reg);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0, 0x3, ri & 0x7);
					_dest.code << (literal & 0xFF);
					_dest.code << ((literal >> 8) & 0xFF);
					_dest.code << ((literal >> 16) & 0xFF);
					_dest.code << ((literal >> 24) & 0xFF);
				} else if (quant == 4) {
					if ((ri & 0x8) && _x64_mode) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0, 0x3, ri & 0x7);
					_dest.code << (literal & 0xFF);
					_dest.code << ((literal >> 8) & 0xFF);
					_dest.code << ((literal >> 16) & 0xFF);
					_dest.code << ((literal >> 24) & 0xFF);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if ((ri & 0x8) && _x64_mode) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0, 0x3, ri & 0x7);
					_dest.code << (literal & 0xFF);
					_dest.code << ((literal >> 8) & 0xFF);
				} else if (quant == 1) {
					if ((ri & 0xC) && _x64_mode) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF6 << make_mod(0, 0x3, ri & 0x7);
					_dest.code << (literal & 0xFF);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_operation(uint quant, arOp op, Reg to, Reg value_ptr, bool indirect, int value_offset)
			{
				if (value_ptr == Reg64::RSP) return;
				auto di = regular_register_code(to);
				auto si = regular_register_code(value_ptr);
				auto o = uint(op);
				uint8 mode;
				if (indirect) {
					if (value_offset == 0 && value_ptr != Reg64::RBP) mode = 0x00;
					else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
					else mode = 0x02;
				} else mode = 0x03;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
					_dest.code << (o + 1) << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << (o + 1) << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << (o + 1) << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << o << make_mod(di & 0x07, mode, si & 0x07);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(value_offset);
					_dest.code << int8(value_offset >> 8);
					_dest.code << int8(value_offset >> 16);
					_dest.code << int8(value_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(value_offset);
			}
			void EncoderContext::encode_mul(uint quant, Reg to, Reg value, bool indirect, int value_offset)
			{
				if (value == Reg64::RSP) throw InvalidArgumentException();
				auto di = regular_register_code(to);
				auto si = regular_register_code(value);
				uint8 mode;
				if (indirect) {
					if (value_offset == 0 && value != Reg64::RBP) mode = 0x00;
					else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
					else mode = 0x02;
				} else mode = 0x03;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
					_dest.code << 0x0F << 0xAF << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x0F << 0xAF << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x0F << 0xAF << make_mod(di & 0x07, mode, si & 0x07);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(value_offset);
					_dest.code << int8(value_offset >> 8);
					_dest.code << int8(value_offset >> 16);
					_dest.code << int8(value_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(value_offset);
			}
			void EncoderContext::encode_mul3(uint quant, Reg to, Reg value1, int value2, bool indirect, int value_offset)
			{
				if (value1 == Reg64::RSP) throw InvalidArgumentException();
				auto di = regular_register_code(to);
				auto si = regular_register_code(value1);
				uint8 mode;
				if (indirect) {
					if (value_offset == 0 && value1 != Reg64::RBP) mode = 0x00;
					else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
					else mode = 0x02;
				} else mode = 0x03;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
					_dest.code << 0x6B << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x6B << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x6B << make_mod(di & 0x07, mode, si & 0x07);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(value_offset);
					_dest.code << int8(value_offset >> 8);
					_dest.code << int8(value_offset >> 16);
					_dest.code << int8(value_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(value_offset);
				_dest.code << value2;
			}
			void EncoderContext::encode_mul_div(uint quant, mdOp op, Reg value_ptr, bool indirect, int value_offset)
			{
				if (value_ptr == Reg64::RSP || value_ptr == Reg64::RBP) return;
				auto si = regular_register_code(value_ptr);
				auto o = uint(op);
				uint8 mode;
				if (indirect) {
					if (value_offset == 0) mode = 0x00;
					else if (value_offset >= -128 && value_offset < 128) mode = 0x01;
					else mode = 0x02;
				} else mode = 0x03;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, false, 0, si & 0x08);
					_dest.code << 0xF7 << make_mod(uint(op), mode, si & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && (si & 0x08)) _dest.code << make_rex(false, false, 0, si & 0x08);
					_dest.code << 0xF7 << make_mod(uint(op), mode, si & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && (si & 0x08)) _dest.code << make_rex(false, false, 0, si & 0x08);
					_dest.code << 0xF7 << make_mod(uint(op), mode, si & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4)) _dest.code << make_rex(false, false, 0, si & 0x08);
					_dest.code << 0xF6 << make_mod(uint(op), mode, si & 0x07);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(value_offset);
					_dest.code << int8(value_offset >> 8);
					_dest.code << int8(value_offset >> 16);
					_dest.code << int8(value_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(value_offset);
			}
			void EncoderContext::encode_fld(uint quant, Reg src_ptr, int src_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD9;
				else if (quant == 8) _dest.code << 0xDD;
				else throw InvalidArgumentException();
				uint8 mode;
				if (src_offset) { if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(0, mode, regular_register_code(src_ptr));
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_fild(uint quant, Reg src_ptr, int src_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 2) _dest.code << 0xDF;
				else if (quant == 4) _dest.code << 0xDB;
				else if (quant == 8) _dest.code << 0xDF;
				else throw InvalidArgumentException();
				uint8 mode;
				if (src_offset) { if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(quant == 8 ? 5 : 0, mode, regular_register_code(src_ptr));
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_fldz(void)
			{
				if (_x64_mode) throw InvalidStateException();
				_dest.code << 0xD9 << 0xEE;
			}
			void EncoderContext::encode_fstp(uint quant, Reg src_ptr, int src_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD9;
				else if (quant == 8) _dest.code << 0xDD;
				else throw InvalidArgumentException();
				uint8 mode;
				if (src_offset) { if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(3, mode, regular_register_code(src_ptr));
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_fisttp(uint quant, Reg src_ptr, int src_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 2) _dest.code << 0xDF;
				else if (quant == 4) _dest.code << 0xDB;
				else if (quant == 8) _dest.code << 0xDD;
				else throw InvalidArgumentException();
				uint8 mode;
				if (src_offset) { if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(1, mode, regular_register_code(src_ptr));
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_fcomp(uint quant, Reg a2_ptr, int a2_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD8;
				else if (quant == 8) _dest.code << 0xDC;
				else throw InvalidArgumentException();
				uint8 mode;
				if (a2_offset) { if (a2_offset >= -128 && a2_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(3, mode, regular_register_code(a2_ptr));
				if (mode == 0x02) {
					_dest.code << int8(a2_offset);
					_dest.code << int8(a2_offset >> 8);
					_dest.code << int8(a2_offset >> 16);
					_dest.code << int8(a2_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(a2_offset);
			}
			void EncoderContext::encode_fcompp(void)
			{
				if (_x64_mode) throw InvalidStateException();
				_dest.code << 0xDE << 0xD9;
			}
			void EncoderContext::encode_fstsw(void)
			{
				if (_x64_mode) throw InvalidStateException();
				_dest.code << 0xDF << 0xE0;
			}
			void EncoderContext::encode_fadd(uint quant, Reg a2_ptr, int a2_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD8;
				else if (quant == 8) _dest.code << 0xDC;
				else throw InvalidArgumentException();
				uint8 mode;
				if (a2_offset) { if (a2_offset >= -128 && a2_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(0, mode, regular_register_code(a2_ptr));
				if (mode == 0x02) {
					_dest.code << int8(a2_offset);
					_dest.code << int8(a2_offset >> 8);
					_dest.code << int8(a2_offset >> 16);
					_dest.code << int8(a2_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(a2_offset);
			}
			void EncoderContext::encode_fsub(uint quant, Reg a2_ptr, int a2_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD8;
				else if (quant == 8) _dest.code << 0xDC;
				else throw InvalidArgumentException();
				uint8 mode;
				if (a2_offset) { if (a2_offset >= -128 && a2_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(4, mode, regular_register_code(a2_ptr));
				if (mode == 0x02) {
					_dest.code << int8(a2_offset);
					_dest.code << int8(a2_offset >> 8);
					_dest.code << int8(a2_offset >> 16);
					_dest.code << int8(a2_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(a2_offset);
			}
			void EncoderContext::encode_fmul(uint quant, Reg a2_ptr, int a2_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD8;
				else if (quant == 8) _dest.code << 0xDC;
				else throw InvalidArgumentException();
				uint8 mode;
				if (a2_offset) { if (a2_offset >= -128 && a2_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(1, mode, regular_register_code(a2_ptr));
				if (mode == 0x02) {
					_dest.code << int8(a2_offset);
					_dest.code << int8(a2_offset >> 8);
					_dest.code << int8(a2_offset >> 16);
					_dest.code << int8(a2_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(a2_offset);
			}
			void EncoderContext::encode_fdiv(uint quant, Reg a2_ptr, int a2_offset)
			{
				if (_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD8;
				else if (quant == 8) _dest.code << 0xDC;
				else throw InvalidArgumentException();
				uint8 mode;
				if (a2_offset) { if (a2_offset >= -128 && a2_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << make_mod(6, mode, regular_register_code(a2_ptr));
				if (mode == 0x02) {
					_dest.code << int8(a2_offset);
					_dest.code << int8(a2_offset >> 8);
					_dest.code << int8(a2_offset >> 16);
					_dest.code << int8(a2_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(a2_offset);
			}
			void EncoderContext::encode_fabs(void)
			{
				if (_x64_mode) throw InvalidStateException();
				_dest.code << 0xD9 << 0xE1;
			}
			void EncoderContext::encode_fneg(void)
			{
				if (_x64_mode) throw InvalidStateException();
				_dest.code << 0xD9 << 0xE0;
			}
			void EncoderContext::encode_fsqrt(void)
			{
				if (_x64_mode) throw InvalidStateException();
				_dest.code << 0xD9 << 0xFA;
			}
			void EncoderContext::encode_cpuid(void) { _dest.code << 0x0F << 0xA2; }
			void EncoderContext::encode_simd_xor(Reg xmm_dest, Reg xmm_src)
			{
				uint di = xmm_register_code(xmm_dest);
				uint si = xmm_register_code(xmm_src);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
				_dest.code << 0x0F << 0x57 << make_mod(di & 0x7, 0x3, si & 0x7);
			}
			void EncoderContext::encode_simd_xor(Reg xmm_dest, Reg src_ptr, int src_offset)
			{
				uint di = xmm_register_code(xmm_dest);
				uint si = regular_register_code(src_ptr);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
				uint8 mode;
				if (src_offset) { if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02; } else mode = 0x00;
				_dest.code << 0x0F << 0x57 << make_mod(di & 0x7, mode, si & 0x7);
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_simd_shuffle(uint quant, Reg xmm_dest_src_lo, Reg xmm_src_hi, uint i0, uint i1, uint i2, uint i3)
			{
				uint di = xmm_register_code(xmm_dest_src_lo);
				uint si = xmm_register_code(xmm_src_hi);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				if (quant == 8) {
					_dest.code << 0x66;
					if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
					_dest.code << 0x0F << 0xC6 << make_mod(di & 0x7, 0x3, si & 0x7);
					_dest.code << uint8(i0 | (i1 << 1));
				} else if (quant == 4) {
					if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
					_dest.code << 0x0F << 0xC6 << make_mod(di & 0x7, 0x3, si & 0x7);
					_dest.code << uint8(i0 | (i1 << 2) | (i2 << 4) | (i3 << 6));
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_read_random(uint quant, Reg dest)
			{
				auto ri = regular_register_code(dest);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, false, false, ri & 0x8);
					_dest.code << 0x0F << 0xC7 << make_mod(0x6, 0x3, ri & 0x7);
				} else if (quant == 4) {
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0x0F << 0xC7 << make_mod(0x6, 0x3, ri & 0x7);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0x0F << 0xC7 << make_mod(0x6, 0x3, ri & 0x7);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_aes_enc(Reg xmm_dest, Reg xmm_key)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = xmm_register_code(xmm_key);
				_dest.code << 0x66;
				encode_rex(false, di, ki);
				_dest.code << 0x0F << 0x38 << 0xDC;
				encode_mod(di, ki);
			}
			void EncoderContext::encode_aes_enc(Reg xmm_dest, Reg key_ptr, int key_offset)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = regular_register_code(key_ptr);
				auto m = make_modRM_with_offset(di, ki, key_offset);
				_dest.code << 0x66;
				encode_rex(false, m);
				_dest.code << 0x0F << 0x38 << 0xDC;
				encode_mod(m);
			}
			void EncoderContext::encode_aes_enc_last(Reg xmm_dest, Reg xmm_key)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = xmm_register_code(xmm_key);
				_dest.code << 0x66;
				encode_rex(false, di, ki);
				_dest.code << 0x0F << 0x38 << 0xDD;
				encode_mod(di, ki);
			}
			void EncoderContext::encode_aes_enc_last(Reg xmm_dest, Reg key_ptr, int key_offset)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = regular_register_code(key_ptr);
				auto m = make_modRM_with_offset(di, ki, key_offset);
				_dest.code << 0x66;
				encode_rex(false, m);
				_dest.code << 0x0F << 0x38 << 0xDD;
				encode_mod(m);
			}
			void EncoderContext::encode_aes_dec(Reg xmm_dest, Reg xmm_key)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = xmm_register_code(xmm_key);
				_dest.code << 0x66;
				encode_rex(false, di, ki);
				_dest.code << 0x0F << 0x38 << 0xDE;
				encode_mod(di, ki);
			}
			void EncoderContext::encode_aes_dec(Reg xmm_dest, Reg key_ptr, int key_offset)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = regular_register_code(key_ptr);
				auto m = make_modRM_with_offset(di, ki, key_offset);
				_dest.code << 0x66;
				encode_rex(false, m);
				_dest.code << 0x0F << 0x38 << 0xDE;
				encode_mod(m);
			}
			void EncoderContext::encode_aes_dec_last(Reg xmm_dest, Reg xmm_key)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = xmm_register_code(xmm_key);
				_dest.code << 0x66;
				encode_rex(false, di, ki);
				_dest.code << 0x0F << 0x38 << 0xDF;
				encode_mod(di, ki);
			}
			void EncoderContext::encode_aes_dec_last(Reg xmm_dest, Reg key_ptr, int key_offset)
			{
				uint di = xmm_register_code(xmm_dest);
				uint ki = regular_register_code(key_ptr);
				auto m = make_modRM_with_offset(di, ki, key_offset);
				_dest.code << 0x66;
				encode_rex(false, m);
				_dest.code << 0x0F << 0x38 << 0xDF;
				encode_mod(m);
			}
			void EncoderContext::encode_aes_inversed_mix_columns(Reg xmm_dest, Reg xmm_src)
			{
				uint di = xmm_register_code(xmm_dest);
				uint si = xmm_register_code(xmm_src);
				_dest.code << 0x66;
				encode_rex(false, di, si);
				_dest.code << 0x0F << 0x38 << 0xDB;
				encode_mod(di, si);
			}
			void EncoderContext::encode_aes_inversed_mix_columns(Reg xmm_dest, Reg src_ptr, int src_offset)
			{
				uint di = xmm_register_code(xmm_dest);
				uint si = regular_register_code(src_ptr);
				auto m = make_modRM_with_offset(di, si, src_offset);
				_dest.code << 0x66;
				encode_rex(false, m);
				_dest.code << 0x0F << 0x38 << 0xDB;
				encode_mod(m);
			}
			void EncoderContext::encode_aes_keygen(Reg xmm_dest, Reg xmm_src, uint rc)
			{
				uint di = xmm_register_code(xmm_dest);
				uint si = xmm_register_code(xmm_src);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				_dest.code << 0x66;
				if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
				_dest.code << 0x0F << 0x3A << 0xDF << make_mod(di & 0x7, 0x3, si & 0x7) << uint8(rc);
			}
			void EncoderContext::encode_sha256_rounds2(Reg xmm_state_cdgh, Reg xmm_state_abef, Reg xmm_round_words)
			{
				if (xmm_round_words != Reg64::XMM0) throw InvalidArgumentException();
				uint di = xmm_register_code(xmm_state_cdgh);
				uint si = xmm_register_code(xmm_state_abef);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
				_dest.code << 0x0F << 0x38 << 0xCB << make_mod(di & 0x7, 0x3, si & 0x7);
			}
			void EncoderContext::encode_sha256_msg_words4_part1(Reg xmm_dest_src1, Reg xmm_src2)
			{
				uint di = xmm_register_code(xmm_dest_src1);
				uint si = xmm_register_code(xmm_src2);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
				_dest.code << 0x0F << 0x38 << 0xCC << make_mod(di & 0x7, 0x3, si & 0x7);
			}
			void EncoderContext::encode_sha256_msg_words4_part2(Reg xmm_dest_im, Reg xmm_src4)
			{
				uint di = xmm_register_code(xmm_dest_im);
				uint si = xmm_register_code(xmm_src4);
				if ((di & 0x8 || si & 0x8) && !_x64_mode) throw InvalidStateException();
				if (di & 0x8 || si & 0x8) _dest.code << make_rex(false, di & 0x8, false, si & 0x8);
				_dest.code << 0x0F << 0x38 << 0xCD << make_mod(di & 0x7, 0x3, si & 0x7);
			}
			void EncoderContext::encode_invert(uint quant, Reg reg)
			{
				auto ri = regular_register_code(reg);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0x2, 0x3, ri & 0x7);
				} else if (quant == 4) {
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0x2, 0x3, ri & 0x7);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0x2, 0x3, ri & 0x7);
				} else if (quant == 1) {
					if (_x64_mode && (ri & 0xC)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF6 << make_mod(0x2, 0x3, ri & 0x7);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_negative(uint quant, Reg reg)
			{
				auto ri = regular_register_code(reg);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0x3, 0x3, ri & 0x7);
				} else if (quant == 4) {
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0x3, 0x3, ri & 0x7);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF7 << make_mod(0x3, 0x3, ri & 0x7);
				} else if (quant == 1) {
					if (_x64_mode && (ri & 0xC)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << 0xF6 << make_mod(0x3, 0x3, ri & 0x7);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_shift(uint quant, shOp op, Reg reg, int by, bool indirect, int value_offset)
			{
				uint8 oc, ocx;
				auto ri = regular_register_code(reg);
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
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, false, false, ri & 0x8);
					_dest.code << (oc + 1) << make_mod(ocx, mode, ri & 0x7);
				} else if (quant == 4) {
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << (oc + 1) << make_mod(ocx, mode, ri & 0x7);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && (ri & 0x8)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << (oc + 1) << make_mod(ocx, mode, ri & 0x7);
				} else if (quant == 1) {
					if (_x64_mode && (ri & 0xC)) _dest.code << make_rex(false, false, false, ri & 0x8);
					_dest.code << oc << make_mod(ocx, mode, ri & 0x7);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(value_offset);
					_dest.code << int8(value_offset >> 8);
					_dest.code << int8(value_offset >> 16);
					_dest.code << int8(value_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(value_offset);
				if (by) _dest.code << by;
			}
			void EncoderContext::encode_shl(Reg reg, int bits)
			{
				uint rc = regular_register_code(reg);
				if (_x64_mode) _dest.code << make_rex(true, false, false, rc & 0x08);
				_dest.code << 0xC1 << make_mod(0x4, 0x3, rc & 0x7);
				_dest.code << bits;
			}
			void EncoderContext::encode_shr(Reg reg, int bits)
			{
				uint rc = regular_register_code(reg);
				if (_x64_mode) _dest.code << make_rex(true, false, false, rc & 0x08);
				_dest.code << 0xC1 << make_mod(0x5, 0x3, rc & 0x7);
				_dest.code << bits;
			}
			void EncoderContext::encode_call(Reg func_ptr, bool indirect)
			{
				uint rc = regular_register_code(func_ptr);
				if (_x64_mode && (rc & 0x08)) _dest.code << make_rex(false, false, false, true);
				_dest.code << 0xFF << make_mod(0x2, indirect ? 0x00 : 0x03, rc & 0x07);
			}
			void EncoderContext::encode_put_addr_of(Reg dest, const ObjectReference & value)
			{
				auto wordsize = word_size();
				if (value.ref_class == ReferenceNull) {
					encode_mov_reg_const(wordsize, dest, 0);
				} else if (value.ref_class == ReferenceExternal) {
					encode_mov_reg_const(wordsize, dest, 0);
					refer_object_at(_src.extrefs[value.index], _dest.code.Length() - wordsize);
				} else if (value.ref_class == ReferenceData) {
					encode_mov_reg_const(wordsize, dest, value.index);
					relocate_data_at(_dest.code.Length() - wordsize);
				} else if (value.ref_class == ReferenceCode) {
					encode_mov_reg_const(wordsize, dest, value.index);
					relocate_code_at(_dest.code.Length() - wordsize);
				} else if (value.ref_class == ReferenceArgument) {
					auto & arg = _inputs[value.index];
					if (arg.indirect) encode_mov_reg_mem(wordsize, dest, Reg64::RBP, arg.bp_offset);
					else encode_lea(dest, Reg64::RBP, arg.bp_offset);
				} else if (value.ref_class == ReferenceRetVal) {
					if (_retval.indirect) encode_mov_reg_mem(wordsize, dest, Reg64::RBP, _retval.bp_offset);
					else encode_lea(dest, Reg64::RBP, _retval.bp_offset);
				} else if (value.ref_class == ReferenceLocal) {
					bool found = false;
					for (auto & scp : _scopes) if (scp.first_local_no <= value.index && value.index < scp.first_local_no + scp.locals.Length()) {
						auto & local = scp.locals[value.index - scp.first_local_no];
						encode_lea(dest, Reg64::RBP, local.bp_offset);
						found = true;
						break;
					}
					if (!found) throw InvalidArgumentException();
				} else if (value.ref_class == ReferenceInit) {
					if (!_init_locals.IsEmpty()) {
						auto & local = _init_locals.GetLast()->GetValue();
						encode_lea(dest, Reg64::RBP, local.bp_offset);
					} else throw InvalidArgumentException();
				} else if (value.ref_class == ReferenceSplitter) {
					auto scope = _scopes.GetLast();
					if (scope && scope->GetValue().current_split_offset) {
						encode_lea(dest, Reg64::RBP, scope->GetValue().current_split_offset);
					} else throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_debugger_trap(void) { _dest.code << 0xCC; }
			void EncoderContext::encode_pure_ret(int bytes_unroll) { if (bytes_unroll) _dest.code << 0xC2 << (bytes_unroll) << (bytes_unroll >> 8); else _dest.code << 0xC3; }
			// void EncoderContext::encode_block_transfer(int opcode, int dim, const void * df, const void * sf, bool ss, Reg dimptr, Reg ddptr, Reg sdptr, Reg bf, ArgumentSpecification bfs)
			// {
			// 	int xloop_org, xloop_jmp, xloop_end;
			// 	int yloop_org, yloop_jmp, yloop_end;
			// 	int zloop_org, zloop_jmp, zloop_end;
			// 	int dest_bpp = TH::GetSampleDescriptionBitLength(df);
			// 	int src_bpp = TH::GetSampleDescriptionBitLength(sf);
			// 	if (_x64_mode) {
			// 		encode_push(Reg64::RBP);
			// 		encode_mov_reg_reg(8, Reg64::RBP, Reg64::RSP);
			// 		encode_add(Reg64::RSP, -16);
			// 		// R12      - destination's current line address
			// 		// R13      - source's current line address
			// 		// R14      - linear index X
			// 		// [RBP-8]  - linear index Y
			// 		// [RBP-16] - linear index Z
			// 		if (dim == 3) {
			// 			encode_operation(8, arOp::XOR, Reg64::R14, Reg64::R14);
			// 			encode_mov_mem_reg(8, Reg64::RBP, -16, Reg64::R14);
			// 			zloop_org = _dest.code.Length();
			// 			encode_operation(8, arOp::CMP, Reg64::R14, dimptr, true, 16);
			// 			_dest.code << 0x0F << 0x83 << 0x00 << 0x00 << 0x00 << 0x00; // JAE
			// 			zloop_jmp = _dest.code.Length();
			// 		}
			// 		if (dim >= 2) {
			// 			encode_operation(8, arOp::XOR, Reg64::R14, Reg64::R14);
			// 			encode_mov_mem_reg(8, Reg64::RBP, -8, Reg64::R14);
			// 			yloop_org = _dest.code.Length();
			// 			encode_operation(8, arOp::CMP, Reg64::R14, dimptr, true, 8);
			// 			_dest.code << 0x0F << 0x83 << 0x00 << 0x00 << 0x00 << 0x00; // JAE
			// 			yloop_jmp = _dest.code.Length();
			// 			encode_mov_reg_mem(8, Reg64::R12, ddptr, 0);
			// 			encode_mul(8, Reg64::R14, ddptr, true, 8);
			// 			encode_operation(8, arOp::ADD, Reg64::R12, Reg64::R14);
			// 			if (!ss) {
			// 				encode_mov_reg_mem(8, Reg64::R13, sdptr, 0);
			// 				encode_mov_reg_mem(8, Reg64::R14, Reg64::RBP, -8);
			// 				encode_mul(8, Reg64::R14, sdptr, true, 8);
			// 				encode_operation(8, arOp::ADD, Reg64::R13, Reg64::R14);
			// 			} else encode_mov_reg_reg(8, Reg64::R13, sdptr);
			// 			if (dim == 3) {
			// 				encode_mov_reg_mem(8, Reg64::R8, Reg64::RBP, -16);
			// 				if (!ss) encode_mov_reg_reg(8, Reg64::R9, Reg64::R8);
			// 				encode_mul(8, Reg64::R8, ddptr, true, 16);
			// 				if (!ss) encode_mul(8, Reg64::R9, sdptr, true, 16);
			// 				encode_operation(8, arOp::ADD, Reg64::R12, Reg64::R8);
			// 				if (!ss) encode_operation(8, arOp::ADD, Reg64::R13, Reg64::R9);
			// 			}
			// 		} else {
			// 			encode_mov_reg_mem(8, Reg64::R12, ddptr, 0);
			// 			if (!ss) encode_mov_reg_mem(8, Reg64::R13, sdptr, 0);
			// 			else encode_mov_reg_reg(8, Reg64::R13, sdptr);
			// 		}
			// 		encode_operation(8, arOp::XOR, Reg64::R14, Reg64::R14);
			// 		xloop_org = _dest.code.Length();
			// 		encode_operation(8, arOp::CMP, Reg64::R14, dimptr, true);
			// 		_dest.code << 0x0F << 0x83 << 0x00 << 0x00 << 0x00 << 0x00; // JAE
			// 		xloop_jmp = _dest.code.Length();
			// 		if (dest_bpp >= 8) {
			// 			if (dest_bpp != 8) encode_mul3(8, Reg64::R8, Reg64::R14, dest_bpp / 8);
			// 			else encode_mov_reg_reg(8, Reg64::R8, Reg64::R14);
			// 			encode_operation(8, arOp::ADD, Reg64::R8, Reg64::R12);
			// 		} else {
			// 			int lb = -3;
			// 			while ((1 << (lb + 3)) < dest_bpp) lb++;
			// 			encode_mov_reg_reg(8, Reg64::R8, Reg64::R14);
			// 			encode_shr(Reg64::R8, -lb);
			// 			encode_operation(8, arOp::ADD, Reg64::R8, Reg64::R12);
			// 		}
			// 		if (!ss) {
			// 			if (src_bpp >= 8) {
			// 				if (src_bpp != 8) encode_mul3(8, Reg64::R9, Reg64::R14, src_bpp / 8);
			// 				else encode_mov_reg_reg(8, Reg64::R9, Reg64::R14);
			// 				encode_operation(8, arOp::ADD, Reg64::R9, Reg64::R13);
			// 			} else {
			// 				int lb = -3;
			// 				while ((1 << (lb + 3)) < src_bpp) lb++;
			// 				encode_mov_reg_reg(8, Reg64::R9, Reg64::R14);
			// 				encode_shr(Reg64::R9, -lb);
			// 				encode_operation(8, arOp::ADD, Reg64::R9, Reg64::R13);
			// 			}
			// 		} else encode_mov_reg_reg(8, Reg64::R9, Reg64::R13);
			// 		if (opcode == TransformBltCopy) {
			// 			if (TH::SampleDescriptionAreSame(df, sf)) {
			// 				bool full_write = true;
			// 				Array<bool> wrm(0x200);
			// 				int ncnl = TH::GetSampleDescriptionNumberOfChannels(df);
			// 				bool be = TH::GetSampleDescriptionEndianness(df);
			// 				for (int i = 0; i < ncnl; i++) {
			// 					int length, format;
			// 					TH::GetSampleDescriptionChannelByIndex(df, i, 0, &length, &format);
			// 					if (format & BlockTransferFormatReadOnly) for (int j = 0; j < length; j++) { wrm << false; full_write = false; }
			// 					else for (int j = 0; j < length; j++) wrm << true;
			// 				}
			// 				if (full_write) {
			// 					if (!ss || dest_bpp >= 8) {
			// 						int num_bytes = (dest_bpp + 7) / 8;
			// 						int cp = 0;
			// 						while (cp < num_bytes) {
			// 							int quant = num_bytes - cp;
			// 							if (quant >= 8) quant = 8;
			// 							else if (quant >= 4) quant = 4;
			// 							else if (quant >= 2) quant = 2;
			// 							else quant = 1;
			// 							encode_mov_reg_mem(quant, Reg64::RAX, Reg64::R9, cp);
			// 							encode_mov_mem_reg(quant, Reg64::R8, cp, Reg64::RAX);
			// 							cp += quant;
			// 						}
			// 					} else {
			// 						encode_mov_reg_mem(1, Reg64::RAX, Reg64::R9);
			// 						if (dest_bpp == 4) {
			// 							encode_and(Reg64::RAX, 0xF);
			// 							encode_mov_reg_reg(1, Reg64::RCX, Reg64::RAX);
			// 							encode_shl(Reg64::RCX, 4);
			// 							encode_operation(1, arOp::OR, Reg64::RAX, Reg64::RCX);
			// 						} else if (dest_bpp == 2) {
			// 							encode_and(Reg64::RAX, 0x3);
			// 							encode_mov_reg_reg(1, Reg64::RCX, Reg64::RAX);
			// 							encode_shl(Reg64::RCX, 2);
			// 							encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 							encode_operation(1, arOp::OR, Reg64::RAX, Reg64::RCX);
			// 							encode_shl(Reg64::RCX, 4);
			// 							encode_operation(1, arOp::OR, Reg64::RAX, Reg64::RCX);
			// 						} else if (dest_bpp == 1) {
			// 							encode_and(Reg64::RAX, 0x1);
			// 							encode_shift(1, shOp::SHL, Reg64::RAX, 7);
			// 							encode_shift(1, shOp::SAR, Reg64::RAX, 7);
			// 						} else throw InvalidArgumentException();
			// 						encode_mov_mem_reg(1, Reg64::R8, Reg64::RAX);
			// 					}
			// 				} else {
			// 					if (dest_bpp >= 8) {
			// 						int num_bytes = dest_bpp / 8;
			// 						if (be) for (int o = 0; o < num_bytes; o++) for (int i = 0; i < 4; i++) wrm.SwapAt(8 * o + i, 8 * o + 7 - i);
			// 						int cp = 0;
			// 						while (cp < num_bytes) {
			// 							int quant = num_bytes - cp;
			// 							if (quant >= 4) quant = 4;
			// 							else if (quant >= 2) quant = 2;
			// 							else quant = 1;
			// 							uint wrmask = 0, rdmask = 0, commask = 0;
			// 							for (int i = 0; i < 8 * quant; i++) {
			// 								uint bit = uint(1) << uint(i);
			// 								commask |= bit;
			// 								if (wrm[cp * 8 + i]) wrmask |= bit; else rdmask |= bit;
			// 							}
			// 							if (wrmask) {
			// 								if (wrmask == commask) {
			// 									encode_mov_reg_mem(quant, Reg64::RAX, Reg64::R9, cp);
			// 									encode_mov_mem_reg(quant, Reg64::R8, cp, Reg64::RAX);
			// 								} else {
			// 									encode_mov_reg_mem(quant, Reg64::RAX, Reg64::R9, cp);
			// 									encode_mov_reg_mem(quant, Reg64::RCX, Reg64::R8, cp);
			// 									encode_and(Reg64::RAX, wrmask);
			// 									encode_and(Reg64::RCX, rdmask);
			// 									encode_operation(quant, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 									encode_mov_mem_reg(quant, Reg64::R8, cp, Reg64::RCX);
			// 								}
			// 							}
			// 							cp += quant;
			// 						}
			// 					} else {
			// 						if (be) for (int i = 0; i < wrm.Length() / 2; i++) wrm.SwapAt(i, wrm.Length() - i - 1);
			// 						uint wrmask = 0, rdmask = 0;
			// 						for (int i = 0; i < 8; i++) {
			// 							uint bit = uint(1) << uint(i);
			// 							if (wrm[i % dest_bpp]) wrmask |= bit; else rdmask |= bit;
			// 						}
			// 						if (ss) {
			// 							encode_mov_reg_mem(1, Reg64::RAX, Reg64::R9);
			// 							encode_mov_reg_mem(1, Reg64::RCX, Reg64::R8);
			// 							encode_and(Reg64::RAX, wrmask);
			// 							encode_and(Reg64::RCX, rdmask);
			// 							encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 							if (dest_bpp == 4) {
			// 								encode_shl(Reg64::RAX, 4);
			// 								encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 							} else if (dest_bpp == 2) {
			// 								encode_shl(Reg64::RAX, 2);
			// 								encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 								encode_shl(Reg64::RAX, 2);
			// 								encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 								encode_shl(Reg64::RAX, 2);
			// 								encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 							} else if (dest_bpp == 1) {
			// 								encode_shift(1, shOp::SHL, Reg64::RCX, 7);
			// 								encode_shift(1, shOp::SAR, Reg64::RCX, 7);
			// 							} else throw InvalidArgumentException();
			// 							encode_mov_mem_reg(1, Reg64::R8, Reg64::RCX);
			// 						} else {
			// 							encode_mov_reg_mem(1, Reg64::RAX, Reg64::R9);
			// 							encode_mov_reg_mem(1, Reg64::RCX, Reg64::R8);
			// 							encode_and(Reg64::RAX, wrmask);
			// 							encode_and(Reg64::RCX, rdmask);
			// 							encode_operation(1, arOp::OR, Reg64::RCX, Reg64::RAX);
			// 							encode_mov_mem_reg(1, Reg64::R8, Reg64::RCX);
			// 						}
			// 					}
			// 				}
			// 			} else {
			// 				int ncnl = TH::GetSampleDescriptionNumberOfChannels(df);

			// 				// auto dam = Codec::AlphaMode::Straight;
			// 				// auto sam = Codec::AlphaMode::Straight;
			// 				// auto alpha = Reg64::NO;
			// 				// if (TH::GetSampleDescriptionChannelByUsage(df, BlockTransferChannelAlphaPremultiplied))

			// 				for (int i = 0; i < ncnl; i++) {
			// 					int usage = TH::GetSampleDescriptionChannelUsage(df, i);
			// 					if (usage == BlockTransferChannelAlphaStraight) {

			// 					} else if (usage == BlockTransferChannelAlphaStraight) {
									
			// 					}
			// 				}

			// 				// I. CONVERT ALPHA, LOAD FOR CHANNEL CORRECTION
			// 				// II. CONVERT OTHERS

			// 				// 

			// 				// 	// TODO: OPERATION
			// 				// 	// TODO: ALPHA MODE CONVERSION - LOAD ALPHA

			// 				// 	//
			// 				// 	//TH::Get

			// 				// }
			// 			}
			// 		} else if (TH::SampleDescriptionAreSame(df, sf)) {

			// 			// TODO: OPERATION
			// 			// BASE BYTE OF OPERATION: R8/R9, INDEX R14
			// 			// TODO: CHECK FORMAT IDENTITY

			// 		} else throw InvalidArgumentException();
			// 		encode_add(Reg64::R14, 1);
			// 		xloop_end = _dest.code.Length() + 5;
			// 		_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
			// 		*reinterpret_cast<int32 *>(_dest.code.GetBuffer() + xloop_jmp - 4) = xloop_end - xloop_jmp;
			// 		*reinterpret_cast<int32 *>(_dest.code.GetBuffer() + xloop_end - 4) = xloop_org - xloop_end;
			// 		if (dim >= 2) {
			// 			encode_mov_reg_mem(8, Reg64::R14, Reg64::RBP, -8);
			// 			encode_add(Reg64::R14, 1);
			// 			encode_mov_mem_reg(8, Reg64::RBP, -8, Reg64::R14);
			// 			yloop_end = _dest.code.Length() + 5;
			// 			_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
			// 			*reinterpret_cast<int32 *>(_dest.code.GetBuffer() + yloop_jmp - 4) = yloop_end - yloop_jmp;
			// 			*reinterpret_cast<int32 *>(_dest.code.GetBuffer() + yloop_end - 4) = yloop_org - yloop_end;
			// 		}
			// 		if (dim == 3) {
			// 			encode_mov_reg_mem(8, Reg64::R14, Reg64::RBP, -16);
			// 			encode_add(Reg64::R14, 1);
			// 			encode_mov_mem_reg(8, Reg64::RBP, -16, Reg64::R14);
			// 			zloop_end = _dest.code.Length() + 5;
			// 			_dest.code << 0xE9 << 0x00 << 0x00 << 0x00 << 0x00; // JMP
			// 			*reinterpret_cast<int32 *>(_dest.code.GetBuffer() + zloop_jmp - 4) = zloop_end - zloop_jmp;
			// 			*reinterpret_cast<int32 *>(_dest.code.GetBuffer() + zloop_end - 4) = zloop_org - zloop_end;
			// 		}
			// 		encode_mov_reg_reg(8, Reg64::RSP, Reg64::RBP);
			// 		encode_pop(Reg64::RBP);
			// 	} else {

			// 		// TODO: IMPLEMENT 32-BIT

			// 	}
			// }
		}
	}
}