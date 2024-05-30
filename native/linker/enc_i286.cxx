#include "enc_i286.h"

namespace Engine
{
	namespace XN
	{
		i286_EncoderContext::i286_EncoderContext(DataBlock & dest, EncoderMode initmode) : _dest(dest), _mode(initmode) {}
		void i286_EncoderContext::set_encoder_mode(EncoderMode mode) { _mode = mode; }
		EncoderMode i286_EncoderContext::get_encoder_mode(void) { return _mode; }
		DataBlock & i286_EncoderContext::get_destination(void) { return _dest; }
		bool i286_EncoderContext::is_i386_mode(void) { return _mode == EncoderMode::i386_16 || _mode == EncoderMode::i386_32; }
		bool i286_EncoderContext::is_32_bit_mode(void) { return _mode == EncoderMode::i386_32; }
		bool i286_EncoderContext::is_regular_register(Reg reg) { return (uint(reg) & 0x0FF) != 0; }
		bool i286_EncoderContext::is_segment_register(Reg reg) { return (uint(reg) & 0xF00) != 0; }
		bool i286_EncoderContext::is_control_register(Reg reg) { return (uint(reg) & 0xFF0000) != 0; }
		uint8 i286_EncoderContext::register_code(Reg reg)
		{
			uint ri = uint(reg) & 0xFF0FFF;
			if (ri == 0x001) return 0;
			else if (ri == 0x002) return 1;
			else if (ri == 0x004) return 2;
			else if (ri == 0x008) return 3;
			else if (ri == 0x010) return 4;
			else if (ri == 0x020) return 5;
			else if (ri == 0x040) return 6;
			else if (ri == 0x080) return 7;
			else if (ri == 0x100) return 1;
			else if (ri == 0x200) return 3;
			else if (ri == 0x400) return 0;
			else if (ri == 0x800) return 2;
			else if (ri == 0x010000) return 0;
			else if (ri == 0x020000) return 1;
			else if (ri == 0x040000) return 2;
			else if (ri == 0x080000) return 3;
			else if (ri == 0x100000) return 4;
			else if (ri == 0x200000) return 5;
			else if (ri == 0x400000) return 6;
			else if (ri == 0x800000) return 7;
			else return 0xFF;
		}
		uint8 i286_EncoderContext::register_quant(Reg reg)
		{
			uint ri = uint(reg) & 0xF000;
			if (ri == 0x1000) return 1;
			else if (ri == 0x2000) return 2;
			else if (ri == 0x4000 && is_i386_mode()) return 4;
			else return 0;
		}
		uint8 i286_EncoderContext::make_mod(uint8 reg, uint8 mod, uint8 reg_mem) { return (mod << 6) | (reg << 3) | reg_mem; }
		uint8 i286_EncoderContext::segment_prefix(Reg reg)
		{
			if (reg == Reg::CS) return 0x2E;
			else if (reg == Reg::DS) return 0x3E;
			else if (reg == Reg::ES) return 0x26;
			else if (reg == Reg::SS) return 0x36;
			else return 0;
		}
		uint8 i286_EncoderContext::register_address_mode(Reg reg)
		{
			if (is_32_bit_mode()) {
				if (reg == Reg::NO) return 5;
				else if (register_quant(reg) == 4 && reg != Reg::ESP) return register_code(reg);
				else throw InvalidArgumentException();
			} else {
				if (reg == Reg::SI) return 4;
				else if (reg == Reg::DI) return 5;
				else if (reg == Reg::BP) return 6;
				else if (reg == Reg::NO) return 6;
				else if (reg == Reg::BX) return 7;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_padding_zeroes(uint align) { while (_dest.Length() % align) _dest << 0; }
		void i286_EncoderContext::encode_padding_int3(uint align) { while (_dest.Length() % align) encode_int3(); }
		void i286_EncoderContext::encode_data(const void * data, uint length)
		{
			int offset = _dest.Length();
			_dest.SetLength(offset + length);
			MemoryCopy(_dest.GetBuffer() + offset, data, length);
		}
		uint i286_EncoderContext::get_current_position(void) { return _dest.Length(); }
		void i286_EncoderContext::encode_universal(uint8 opcode, uint8 ri, Reg ptr, Reg seg, int offset)
		{
			if (seg != Reg::NO) {
				if (!is_segment_register(seg)) throw InvalidArgumentException();
				_dest << segment_prefix(seg);
			}
			int mod, os, mi = register_address_mode(ptr);
			if (ptr == Reg::NO) {
				mod = 0;
				os = is_32_bit_mode() ? 4 : 2;
			} else {
				if (offset == 0 && ptr != Reg::BP && ptr != Reg::EBP) { mod = 0; os = 0; }
				else if (abs(offset) < 128) { mod = 1; os = 1; }
				else { mod = 2; os = is_32_bit_mode() ? 4 : 2; }
			}
			_dest << opcode << make_mod(ri, mod, mi);
			if (os == 4) _dest << offset << (offset >> 8) << (offset >> 16) << (offset >> 24);
			if (os == 2) _dest << offset << (offset >> 8);
			else if (os == 1) _dest << offset;
		}
		void i286_EncoderContext::encode_call(uint quant)
		{
			if (is_32_bit_mode()) {
				if (quant == 4) _dest << 0xE8 << 0 << 0 << 0 << 0;
				else if (quant == 6) _dest << 0x9A << 0 << 0 << 0 << 0 << 0 << 0;
				else throw InvalidArgumentException();
			} else {
				if (quant == 2) _dest << 0xE8 << 0 << 0;
				else if (quant == 4) _dest << 0x9A << 0 << 0 << 0 << 0;
				else if (quant == 6 && is_i386_mode()) _dest << 0x66 << 0x9A << 0 << 0 << 0 << 0 << 0 << 0;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_call(uint quant, Reg addr)
		{
			if (is_32_bit_mode()) {
				if (!is_regular_register(addr) || register_quant(addr) != 4) throw InvalidArgumentException();
				if (quant == 4) _dest << 0xFF << make_mod(2, 3, register_code(addr));
				else throw InvalidArgumentException();
			} else {
				if (!is_regular_register(addr) || register_quant(addr) != 2) throw InvalidArgumentException();
				if (quant == 2) _dest << 0xFF << make_mod(2, 3, register_code(addr));
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_call(uint quant, Reg addr_ptr, Reg addr_seg, int addr_offset)
		{
			if (is_32_bit_mode()) {
				if (quant == 6) {
					encode_universal(0xFF, 3, addr_ptr, addr_seg, addr_offset);
				} else if (quant == 4) {
					encode_universal(0xFF, 2, addr_ptr, addr_seg, addr_offset);
				} else throw InvalidArgumentException();
			} else {
				if (quant == 6 && is_i386_mode()) {
					_dest << 0x66;
					encode_universal(0xFF, 3, addr_ptr, addr_seg, addr_offset);
				} else if (quant == 4) {
					encode_universal(0xFF, 3, addr_ptr, addr_seg, addr_offset);
				} else if (quant == 2) {
					encode_universal(0xFF, 2, addr_ptr, addr_seg, addr_offset);
				} else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_cbw(void) { _dest << 0x98; }
		void i286_EncoderContext::encode_cli(void) { _dest << 0xFA; }
		void i286_EncoderContext::encode_cwd(void) { _dest << 0x99; }
		void i286_EncoderContext::encode_hlt(void) { _dest << 0xF4; }
		void i286_EncoderContext::encode_in(Reg dest, int port)
		{
			if (is_32_bit_mode()) {
				if (dest == Reg::AL) _dest << 0xE4 << port;
				else if (dest == Reg::AX) _dest << 0x66 << 0xE5 << port;
				else if (dest == Reg::EAX) _dest << 0xE5 << port;
				else throw InvalidArgumentException();
			} else {
				if (dest == Reg::AL) _dest << 0xE4 << port;
				else if (dest == Reg::AX) _dest << 0xE5 << port;
				else if (dest == Reg::EAX && is_i386_mode()) _dest << 0x66 << 0xE5 << port;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_in(Reg dest, Reg port)
		{
			if (port != Reg::DX) throw InvalidArgumentException();
			if (is_32_bit_mode()) {
				if (dest == Reg::AL) _dest << 0xEC;
				else if (dest == Reg::AX) _dest << 0x66 << 0xED;
				else if (dest == Reg::EAX) _dest << 0xED;
				else throw InvalidArgumentException();
			} else {
				if (dest == Reg::AL) _dest << 0xEC;
				else if (dest == Reg::AX) _dest << 0xED;
				else if (dest == Reg::EAX && is_i386_mode()) _dest << 0x66 << 0xED;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_int3(void) { _dest << 0xCC; }
		void i286_EncoderContext::encode_into(void) { _dest << 0xCE; }
		void i286_EncoderContext::encode_int(uint8 iv) { _dest << 0xCD << iv; }
		void i286_EncoderContext::encode_iret(void) { _dest << 0xCF; }
		void i286_EncoderContext::encode_jcc(JCC jcc) { _dest << uint(jcc) << 0; }
		void i286_EncoderContext::encode_jmp(uint quant)
		{
			if (is_32_bit_mode()) {
				if (quant == 1) _dest << 0xEB << 0;
				else if (quant == 2) _dest << 0x66 << 0xE9 << 0 << 0;
				else if (quant == 4) _dest << 0xE9 << 0 << 0 << 0 << 0;
				else if (quant == 6) _dest << 0xEA << 0 << 0 << 0 << 0 << 0 << 0;
				else throw InvalidArgumentException();
			} else {
				if (quant == 1) _dest << 0xEB << 0;
				else if (quant == 2) _dest << 0xE9 << 0 << 0;
				else if (quant == 4) _dest << 0xEA << 0 << 0 << 0 << 0;
				else if (quant == 6 && is_i386_mode()) _dest << 0x66 << 0xEA << 0 << 0 << 0 << 0 << 0 << 0;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_jmp(uint quant, Reg addr)
		{
			if (is_32_bit_mode()) {
				if (!is_regular_register(addr) || register_quant(addr) != 4) throw InvalidArgumentException();
				if (quant == 4) _dest << 0xFF << make_mod(4, 3, register_code(addr));
				else throw InvalidArgumentException();
			} else {
				if (!is_regular_register(addr) || register_quant(addr) != 2) throw InvalidArgumentException();
				if (quant == 2) _dest << 0xFF << make_mod(4, 3, register_code(addr));
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_jmp(uint quant, Reg addr_ptr, Reg addr_seg, int addr_offset)
		{
			if (is_32_bit_mode()) {
				if (quant == 6) {
					encode_universal(0xFF, 5, addr_ptr, addr_seg, addr_offset);
				} else if (quant == 4) {
					encode_universal(0xFF, 4, addr_ptr, addr_seg, addr_offset);
				} else throw InvalidArgumentException();
			} else {
				if (quant == 6 && is_i386_mode()) {
					_dest << 0x66;
					encode_universal(0xFF, 5, addr_ptr, addr_seg, addr_offset);
				} else if (quant == 4) {
					encode_universal(0xFF, 5, addr_ptr, addr_seg, addr_offset);
				} else if (quant == 2) {
					encode_universal(0xFF, 4, addr_ptr, addr_seg, addr_offset);
				} else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_lahf(void) { _dest << 0x9F; }
		void i286_EncoderContext::encode_lds(Reg dest, Reg src_ptr, Reg src_seg, int src_offset)
		{
			if (!is_regular_register(dest)) throw InvalidArgumentException();
			auto rq = register_quant(dest);
			auto di = register_code(dest);
			if (rq == 2) {
				if (is_32_bit_mode()) _dest << 0x66;
			} else if (rq == 4) {
				if (!is_32_bit_mode() && is_i386_mode()) _dest << 0x66;
				else if (!is_i386_mode()) throw InvalidArgumentException();
			} else throw InvalidArgumentException();
			encode_universal(0xC5, di, src_ptr, src_seg, src_offset);
		}
		void i286_EncoderContext::encode_les(Reg dest, Reg src_ptr, Reg src_seg, int src_offset)
		{
			if (!is_regular_register(dest)) throw InvalidArgumentException();
			auto rq = register_quant(dest);
			auto di = register_code(dest);
			if (rq == 2) {
				if (is_32_bit_mode()) _dest << 0x66;
			} else if (rq == 4) {
				if (!is_32_bit_mode() && is_i386_mode()) _dest << 0x66;
				else if (!is_i386_mode()) throw InvalidArgumentException();
			} else throw InvalidArgumentException();
			encode_universal(0xC4, di, src_ptr, src_seg, src_offset);
		}
		void i286_EncoderContext::encode_lea(Reg dest, Reg src_ptr, int src_offset)
		{
			if (!is_regular_register(dest)) throw InvalidArgumentException();
			auto rq = register_quant(dest);
			auto di = register_code(dest);
			if (rq == 2) {
				if (is_32_bit_mode()) _dest << 0x66;
			} else if (rq == 4) {
				if (!is_32_bit_mode() && is_i386_mode()) _dest << 0x66;
				else if (!is_i386_mode()) throw InvalidArgumentException();
			} else throw InvalidArgumentException();
			encode_universal(0x8D, di, src_ptr, Reg::NO, src_offset);
		}
		void i286_EncoderContext::encode_mov_reg_reg(uint quant, Reg dest, Reg src)
		{
			auto di = register_code(dest);
			auto si = register_code(src);
			if (is_control_register(dest) && is_regular_register(src) && quant == 4 && is_i386_mode()) {
				_dest << 0x0F << 0x22 << make_mod(di, 0, si);
			} else if (is_regular_register(dest) && is_control_register(src) && quant == 4 && is_i386_mode()) {
				_dest << 0x0F << 0x20 << make_mod(si, 0, di);
			} else if (is_segment_register(dest) && is_regular_register(src) && quant == 2) {
				_dest << 0x8E << make_mod(di, 0x3, si);
			} else if (is_regular_register(dest) && is_segment_register(src) && quant == 2) {
				_dest << 0x8C << make_mod(si, 0x3, di);
			} else if (is_regular_register(dest) && is_regular_register(src)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0x89 << make_mod(si, 0x3, di);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0x89 << make_mod(si, 0x3, di);
				} else if (quant == 1) {
					_dest << 0x88 << make_mod(si, 0x3, di);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, Reg src_seg, int src_offset)
		{
			auto di = register_code(dest);
			if (is_segment_register(dest) && quant == 2) {
				encode_universal(0x8E, di, src_ptr, src_seg, src_offset);
			} else if (is_regular_register(dest)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x8B, di, src_ptr, src_seg, src_offset);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x8B, di, src_ptr, src_seg, src_offset);
				} else if (quant == 1) {
					encode_universal(0x8A, di, src_ptr, src_seg, src_offset);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_mov_reg_imm(uint quant, Reg dest, int src)
		{
			if (is_regular_register(dest)) {
				auto di = register_code(dest);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << (0xB8 + di) << src << (src >> 8) << (src >> 16) << (src >> 24);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << (0xB8 + di) << src << (src >> 8);
				} else if (quant == 1) {
					_dest << (0xB0 + di) << src;
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg dest_seg, int dest_offset, Reg src)
		{
			auto si = register_code(src);
			if (is_segment_register(src) && quant == 2) {
				encode_universal(0x8C, si, dest_ptr, dest_seg, dest_offset);
			} else if (is_regular_register(src)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x89, si, dest_ptr, dest_seg, dest_offset);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x89, si, dest_ptr, dest_seg, dest_offset);
				} else if (quant == 1) {
					encode_universal(0x88, si, dest_ptr, dest_seg, dest_offset);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_arop_reg_reg(AROP op, uint quant, Reg dest, Reg src)
		{
			if (is_regular_register(dest) && is_regular_register(src)) {
				auto di = register_code(dest);
				auto si = register_code(src);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << (uint(op) + 3) << make_mod(di, 0x3, si);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << (uint(op) + 3) << make_mod(di, 0x3, si);
				} else if (quant == 1) {
					_dest << (uint(op) + 2) << make_mod(di, 0x3, si);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_arop_reg_mem(AROP op, uint quant, Reg dest, Reg src_ptr, Reg src_seg, int src_offset)
		{
			auto di = register_code(dest);
			if (is_regular_register(dest)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					encode_universal(uint(op) + 3, di, src_ptr, src_seg, src_offset);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					encode_universal(uint(op) + 3, di, src_ptr, src_seg, src_offset);
				} else if (quant == 1) {
					encode_universal(uint(op) + 2, di, src_ptr, src_seg, src_offset);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_arop_reg_imm(AROP op, uint quant, Reg dest, int src)
		{
			if (is_regular_register(dest)) {
				auto di = register_code(dest);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0x81 << make_mod(uint(op) >> 8, 3, di) << src << (src >> 8) << (src >> 16) << (src >> 24);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0x81 << make_mod(uint(op) >> 8, 3, di) << src << (src >> 8);
				} else if (quant == 1) {
					_dest << 0x80 << make_mod(uint(op) >> 8, 3, di) << src;
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_arop_mem_reg(AROP op, uint quant, Reg dest_ptr, Reg dest_seg, int dest_offset, Reg src)
		{
			auto si = register_code(src);
			if (is_regular_register(src)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					encode_universal(uint(op) + 1, si, dest_ptr, dest_seg, dest_offset);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					encode_universal(uint(op) + 1, si, dest_ptr, dest_seg, dest_offset);
				} else if (quant == 1) {
					encode_universal(uint(op), si, dest_ptr, dest_seg, dest_offset);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_uop_reg(UOP op, uint quant, Reg dest)
		{
			if (is_regular_register(dest)) {
				auto di = register_code(dest);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0xFF << make_mod(uint(op), 0x3, di);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0xFF << make_mod(uint(op), 0x3, di);
				} else if (quant == 1) {
					_dest << 0xFE << make_mod(uint(op), 0x3, di);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_mulop_reg(MULOP op, uint quant, Reg src)
		{
			if (is_regular_register(src)) {
				auto si = register_code(src);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0xF7 << make_mod(uint(op), 0x3, si);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0xF7 << make_mod(uint(op), 0x3, si);
				} else if (quant == 1) {
					_dest << 0xF6 << make_mod(uint(op), 0x3, si);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_nop(void) { _dest << 0x90; }
		void i286_EncoderContext::encode_out(Reg dest, int port)
		{
			if (is_32_bit_mode()) {
				if (dest == Reg::AL) _dest << 0xE6 << port;
				else if (dest == Reg::AX) _dest << 0x66 << 0xE7 << port;
				else if (dest == Reg::EAX) _dest << 0xE7 << port;
				else throw InvalidArgumentException();
			} else {
				if (dest == Reg::AL) _dest << 0xE6 << port;
				else if (dest == Reg::AX) _dest << 0xE7 << port;
				else if (dest == Reg::EAX && is_i386_mode()) _dest << 0x66 << 0xE7 << port;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_out(Reg dest, Reg port)
		{
			if (port != Reg::DX) throw InvalidArgumentException();
			if (is_32_bit_mode()) {
				if (dest == Reg::AL) _dest << 0xEE;
				else if (dest == Reg::AX) _dest << 0x66 << 0xEF;
				else if (dest == Reg::EAX) _dest << 0xEF;
				else throw InvalidArgumentException();
			} else {
				if (dest == Reg::AL) _dest << 0xEE;
				else if (dest == Reg::AX) _dest << 0xEF;
				else if (dest == Reg::EAX && is_i386_mode()) _dest << 0x66 << 0xEF;
				else throw InvalidArgumentException();
			}
		}
		void i286_EncoderContext::encode_pop(Reg reg)
		{
			if (reg == Reg::DS) _dest << 0x1F;
			else if (reg == Reg::ES) _dest << 0x07;
			else if (reg == Reg::SS) _dest << 0x17;
			else if (is_regular_register(reg)) {
				if (is_32_bit_mode()) {
					if (register_quant(reg) == 2) _dest << 0x66;
					else if (register_quant(reg) != 4) throw InvalidArgumentException();
				} else {
					if (register_quant(reg) == 4 && is_i386_mode()) _dest << 0x66;
					else if (register_quant(reg) != 2) throw InvalidArgumentException();
				}
				_dest << (0x58 | register_code(reg));
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_pop_all(void) { _dest << 0x61; }
		void i286_EncoderContext::encode_pop_flags(void) { _dest << 0x9D; }
		void i286_EncoderContext::encode_push(Reg reg)
		{
			if (reg == Reg::CS) _dest << 0x0E;
			else if (reg == Reg::DS) _dest << 0x1E;
			else if (reg == Reg::ES) _dest << 0x06;
			else if (reg == Reg::SS) _dest << 0x16;
			else if (is_regular_register(reg)) {
				if (is_32_bit_mode()) {
					if (register_quant(reg) == 2) _dest << 0x66;
					else if (register_quant(reg) != 4) throw InvalidArgumentException();
				} else {
					if (register_quant(reg) == 4 && is_i386_mode()) _dest << 0x66;
					else if (register_quant(reg) != 2) throw InvalidArgumentException();
				}
				_dest << (0x50 | register_code(reg));
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_push(int imm)
		{
			if (abs(imm) < 128) _dest << 0x6A << imm;
			else if (is_32_bit_mode()) _dest << 0x68 << imm << (imm >> 8) << (imm >> 16) << (imm >> 24);
			else _dest << 0x68 << imm << (imm >> 8);
		}
		void i286_EncoderContext::encode_push_all(void) { _dest << 0x60; }
		void i286_EncoderContext::encode_push_flags(void) { _dest << 0x9C; }
		void i286_EncoderContext::encode_shop_reg(SHOP op, uint quant, Reg dest)
		{
			if (!is_regular_register(dest)) throw InvalidArgumentException();
			if (quant == 2 || (quant == 4 && is_i386_mode())) {
				if ((quant == 4) != is_32_bit_mode()) _dest << 0x66;
				_dest << 0xD1 << make_mod(uint(op), 3, register_code(dest));
			} else if (quant == 1) _dest << 0xD0 << make_mod(uint(op), 3, register_code(dest));
			else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_shop_reg(SHOP op, uint quant, Reg dest, Reg rot)
		{
			if (!is_regular_register(dest) || rot != Reg::CL) throw InvalidArgumentException();
			if (quant == 2 || (quant == 4 && is_i386_mode())) {
				if ((quant == 4) != is_32_bit_mode()) _dest << 0x66;
				_dest << 0xD3 << make_mod(uint(op), 3, register_code(dest));
			} else if (quant == 1) _dest << 0xD2 << make_mod(uint(op), 3, register_code(dest));
			else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_shop_reg(SHOP op, uint quant, Reg dest, int rot)
		{
			if (!is_regular_register(dest)) throw InvalidArgumentException();
			if (quant == 2 || (quant == 4 && is_i386_mode())) {
				if ((quant == 4) != is_32_bit_mode()) _dest << 0x66;
				_dest << 0xC1 << make_mod(uint(op), 3, register_code(dest));
			} else if (quant == 1) _dest << 0xC0 << make_mod(uint(op), 3, register_code(dest));
			else throw InvalidArgumentException();
			_dest << rot;
		}
		void i286_EncoderContext::encode_ret(int revert, bool far)
		{
			if (far) {
				if (revert) _dest << 0xCA << revert << (revert >> 8);
				else _dest << 0xCB;
			} else {
				if (revert) _dest << 0xC2 << revert << (revert >> 8);
				else _dest << 0xC3;
			}
		}
		void i286_EncoderContext::encode_sahf(void) { _dest << 0x9E; }
		void i286_EncoderContext::encode_sti(void) { _dest << 0xFB; }
		void i286_EncoderContext::encode_test_reg_reg(uint quant, Reg dest, Reg src)
		{
			if (is_regular_register(dest) && is_regular_register(src)) {
				auto di = register_code(dest);
				auto si = register_code(src);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0x85 << make_mod(si, 0x3, di);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0x85 << make_mod(si, 0x3, di);
				} else if (quant == 1) {
					_dest << 0x84 << make_mod(si, 0x3, di);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_test_reg_imm(uint quant, Reg dest, int src)
		{
			if (is_regular_register(dest)) {
				auto di = register_code(dest);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0xF7 << make_mod(0, 3, di) << src << (src >> 8) << (src >> 16) << (src >> 24);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0xF7 << make_mod(0, 3, di) << src << (src >> 8);
				} else if (quant == 1) {
					_dest << 0xF6 << make_mod(0, 3, di) << src;
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_test_mem_reg(uint quant, Reg dest_ptr, Reg dest_seg, int dest_offset, Reg src)
		{
			auto si = register_code(src);
			if (is_regular_register(src)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x85, si, dest_ptr, dest_seg, dest_offset);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x85, si, dest_ptr, dest_seg, dest_offset);
				} else if (quant == 1) {
					encode_universal(0x84, si, dest_ptr, dest_seg, dest_offset);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_xchg_reg_reg(uint quant, Reg dest, Reg src)
		{
			if (is_regular_register(dest) && is_regular_register(src)) {
				auto di = register_code(dest);
				auto si = register_code(src);
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					_dest << 0x87 << make_mod(si, 0x3, di);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					_dest << 0x87 << make_mod(si, 0x3, di);
				} else if (quant == 1) {
					_dest << 0x86 << make_mod(si, 0x3, di);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
		void i286_EncoderContext::encode_xchg_reg_mem(uint quant, Reg dest, Reg src_ptr, Reg src_seg, int src_offset)
		{
			auto di = register_code(dest);
			if (is_regular_register(dest)) {
				if (quant == 4 && is_i386_mode()) {
					if (!is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x87, di, src_ptr, src_seg, src_offset);
				} else if (quant == 2) {
					if (is_32_bit_mode()) _dest << 0x66;
					encode_universal(0x87, di, src_ptr, src_seg, src_offset);
				} else if (quant == 1) {
					encode_universal(0x86, di, src_ptr, src_seg, src_offset);
				} else throw InvalidArgumentException();
			} else throw InvalidArgumentException();
		}
	}
}