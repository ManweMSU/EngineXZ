#include "xa_t_x86.h"
#include "xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace X86
		{
			EncoderContext::EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src, bool x64) : _conv(conv), _dest(dest), _src(src), _org_inst_offsets(1), _jump_reloc(0x100), _inputs(1), _x64_mode(x64)
			{
				_current_instruction = -1;
				_stack_oddity = 0;
				_inputs.SetLength(src.inputs.Length());
				_org_inst_offsets.SetLength(src.instset.Length());
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
				if (_conv == CallingConvention::Windows) xmm = 0x003F0000; else xmm = 0x00FF0000;
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
						encode_add(Reg64::RSP, -16);
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
						encode_add(Reg64::RSP, 16);
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
				if (ls.shift_sp) encode_add(Reg64::RSP, -of_size);
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
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, 0x3, di & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, 0x3, di & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, 0x3, di & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x88 << make_mod(si & 0x07, 0x3, di & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr)
			{
				if (src_ptr == Reg64::RBP || src_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src_ptr);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8B << make_mod(di & 0x07, 0x0, si & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8B << make_mod(di & 0x07, 0x0, si & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8B << make_mod(di & 0x07, 0x0, si & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8A << make_mod(di & 0x07, 0x0, si & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, int src_offset)
			{
				if (src_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src_ptr);
				uint8 mode;
				if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8B << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8B << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8B << make_mod(di & 0x07, mode, si & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x8A << make_mod(di & 0x07, mode, si & 0x07);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_mov_reg_const(uint quant, Reg dest, uint64 value)
			{
				auto di = regular_register_code(dest);
				auto di_lo = di & 0x07;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, 0, 0, di & 0x08);
					_dest.code << 0xB8 + di_lo;
				} else if (quant == 4) {
					if (_x64_mode && (di & 0x08)) _dest.code << make_rex(false, 0, 0, di & 0x08);
					_dest.code << 0xB8 + di_lo;
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && (di & 0x08)) _dest.code << make_rex(false, 0, 0, di & 0x08);
					_dest.code << 0xB8 + di_lo;
				} else if (quant == 1) {
					if (_x64_mode && (di >= 4)) _dest.code << make_rex(false, 0, 0, di & 0x08);
					_dest.code << 0xB0 + di_lo;
				} else throw InvalidArgumentException();
				for (uint q = 0; q < quant; q++) { _dest.code << uint8(value); value >>= 8; }
			}
			void EncoderContext::encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg src)
			{
				if (dest_ptr == Reg64::RBP || dest_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest_ptr);
				auto si = regular_register_code(src);
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, 0x0, di & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, 0x0, di & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, 0x0, di & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x88 << make_mod(si & 0x07, 0x0, di & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_mov_mem_reg(uint quant, Reg dest_ptr, int dest_offset, Reg src)
			{
				if (dest_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest_ptr);
				auto si = regular_register_code(src);
				uint8 mode;
				if (dest_offset >= -128 && dest_offset < 128) mode = 0x01; else mode = 0x02;
				if (quant == 8 && _x64_mode) {
					_dest.code << make_rex(true, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, mode, di & 0x07);
				} else if (quant == 4) {
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, mode, di & 0x07);
				} else if (quant == 2) {
					_dest.code << 0x66;
					if (_x64_mode && ((si & 0x08) || (di & 0x08))) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x89 << make_mod(si & 0x07, mode, di & 0x07);
				} else if (quant == 1) {
					if (_x64_mode && (si >= 4 || di >= 4)) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x88 << make_mod(si & 0x07, mode, di & 0x07);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(dest_offset);
					_dest.code << int8(dest_offset >> 8);
					_dest.code << int8(dest_offset >> 16);
					_dest.code << int8(dest_offset >> 24);
				} else _dest.code << int8(dest_offset);
			}
			void EncoderContext::encode_mov_reg_xmm(uint quant, Reg dest, Reg src)
			{
				if (!_x64_mode) throw InvalidStateException();
				auto di = regular_register_code(dest);
				auto si = xmm_register_code(src);
				if (quant == 8) {
					_dest.code << 0x66 << make_rex(true, si & 0x08, 0, di & 0x08);
					_dest.code << 0x0F << 0x7E << make_mod(si & 0x07, 0x3, di & 0x07);
				} else if (quant == 4) {
					_dest.code << 0x66;
					if ((si & 0x08) || (di & 0x08)) _dest.code << make_rex(false, si & 0x08, 0, di & 0x08);
					_dest.code << 0x0F << 0x7E << make_mod(si & 0x07, 0x3, di & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_mov_xmm_reg(uint quant, Reg dest, Reg src)
			{
				if (!_x64_mode) throw InvalidStateException();
				auto di = xmm_register_code(dest);
				auto si = regular_register_code(src);
				if (quant == 8) {
					_dest.code << 0x66 << make_rex(true, di & 0x08, 0, si & 0x08);
					_dest.code << 0x0F << 0x6E << make_mod(di & 0x07, 0x3, si & 0x07);
				} else if (quant == 4) {
					_dest.code << 0x66;
					if ((si & 0x08) || (di & 0x08)) _dest.code << make_rex(false, di & 0x08, 0, si & 0x08);
					_dest.code << 0x0F << 0x6E << make_mod(di & 0x07, 0x3, si & 0x07);
				} else throw InvalidArgumentException();
			}
			void EncoderContext::encode_mov_mem_xmm(uint quant, Reg dest, int dest_offset, Reg src)
			{
				if (!_x64_mode) throw InvalidStateException();
				auto di = regular_register_code(dest);
				auto si = xmm_register_code(src);
				uint8 mode;
				if (dest_offset == 0) mode = 0x00; else if (dest_offset >= -128 && dest_offset < 128) mode = 0x01; else mode = 0x02;
				if (quant == 16 && dest_offset == 0 && dest == Reg64::RSP) {
					_dest.code << 0x66 << 0x0F << 0x11;
					_dest.code << make_mod(si, 0, 4) << make_mod(4, 0, 4);
				} else if (quant == 16) {
					_dest.code << 0x66 << 0x0F << 0x11;
					_dest.code << make_mod(si, mode, di);
				} else if (quant == 12) {
					encode_mov_mem_xmm(8, dest, dest_offset, src);
					_dest.code << 0x0F << 0xC6 << make_mod(si, 0x03, si) << 0xAA;
					encode_mov_mem_xmm(4, dest, dest_offset + 8, src);
					return;
				} else if (quant == 8) {
					_dest.code << 0xF2 << 0x0F << 0x11;
					_dest.code << make_mod(si, mode, di);
				} else if (quant == 4) {
					_dest.code << 0xF3 << 0x0F << 0x11;
					_dest.code << make_mod(si, mode, di);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(dest_offset);
					_dest.code << int8(dest_offset >> 8);
					_dest.code << int8(dest_offset >> 16);
					_dest.code << int8(dest_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(dest_offset);
			}
			void EncoderContext::encode_mov_xmm_mem(uint quant, Reg dest, Reg src, int src_offset)
			{
				if (!_x64_mode) throw InvalidStateException();
				auto di = xmm_register_code(dest);
				auto si = regular_register_code(src);
				uint8 mode;
				if (src_offset == 0) mode = 0x00; else if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
				if (quant == 16 && src_offset == 0 && src == Reg64::RSP) {
					_dest.code << 0x66 << 0x0F << 0x10;
					_dest.code << make_mod(di, 0, 4) << make_mod(4, 0, 4);
				} else if (quant == 16) {
					_dest.code << 0x66 << 0x0F << 0x10;
					_dest.code << make_mod(di, mode, si);
				} else if (quant == 12) {
					encode_mov_xmm_mem(4, dest, src, src_offset + 8);
					_dest.code << 0x66 << 0x0F << 0x16;
					_dest.code << make_mod(di, mode, si);
				} else if (quant == 8) {
					_dest.code << 0xF2 << 0x0F << 0x10;
					_dest.code << make_mod(di, mode, si);
				} else if (quant == 4) {
					_dest.code << 0xF3 << 0x0F << 0x10;
					_dest.code << make_mod(di, mode, si);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
				if (quant == 12) _dest.code << 0x0F << 0xC6 << make_mod(di, 0x03, di) << 0x4E;
			}
			void EncoderContext::encode_mov_xmm_mem_hi(uint quant, Reg dest, Reg src, int src_offset)
			{
				if (!_x64_mode) throw InvalidStateException();
				auto di = xmm_register_code(dest);
				auto si = regular_register_code(src);
				uint8 mode;
				if (src_offset == 0) mode = 0x00; else if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
				if (quant == 8) {
					_dest.code << 0x66 << 0x0F << 0x16;
					_dest.code << make_mod(di, mode, si);
				} else throw InvalidArgumentException();
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_lea(Reg dest, Reg src_ptr, int src_offset)
			{
				if (src_ptr == Reg64::RSP) return;
				auto di = regular_register_code(dest);
				auto si = regular_register_code(src_ptr);
				uint8 mode;
				if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
				if (_x64_mode) _dest.code << make_rex(true, di & 0x08, 0, si & 0x08);
				_dest.code << 0x8D;
				_dest.code << make_mod(di & 0x07, mode, si & 0x07);
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_push(Reg reg)
			{
				uint rc = regular_register_code(reg);
				uint8 rex = make_rex(false, false, false, rc & 0x08);
				if (rex & 0x0F) _dest.code << rex;
				_dest.code << (0x50 + (rc & 0x07));
			}
			void EncoderContext::encode_pop(Reg reg)
			{
				uint rc = regular_register_code(reg);
				uint8 rex = make_rex(false, false, false, rc & 0x08);
				if (rex & 0x0F) _dest.code << rex;
				_dest.code << (0x58 + (rc & 0x07));
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
				if (value_ptr == Reg64::RSP || value_ptr == Reg64::RBP) return;
				auto di = regular_register_code(to);
				auto si = regular_register_code(value_ptr);
				auto o = uint(op);
				uint8 mode;
				if (indirect) {
					if (value_offset == 0) mode = 0x00;
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
				if (!_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD9;
				else if (quant == 8) _dest.code << 0xDD;
				else throw InvalidArgumentException();
				uint8 mode;
				if (src_offset) {
					if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
				} else mode = 0x00;
				_dest.code << make_mod(0, mode, regular_register_code(src_ptr));
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
			}
			void EncoderContext::encode_fstp(uint quant, Reg src_ptr, int src_offset)
			{
				if (!_x64_mode) throw InvalidStateException();
				if (quant == 4) _dest.code << 0xD9;
				else if (quant == 8) _dest.code << 0xDD;
				else throw InvalidArgumentException();
				uint8 mode;
				if (src_offset) {
					if (src_offset >= -128 && src_offset < 128) mode = 0x01; else mode = 0x02;
				} else mode = 0x00;
				_dest.code << make_mod(3, mode, regular_register_code(src_ptr));
				if (mode == 0x02) {
					_dest.code << int8(src_offset);
					_dest.code << int8(src_offset >> 8);
					_dest.code << int8(src_offset >> 16);
					_dest.code << int8(src_offset >> 24);
				} else if (mode == 0x01) _dest.code << int8(src_offset);
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
		}
	}
}