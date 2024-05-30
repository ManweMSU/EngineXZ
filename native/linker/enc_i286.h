#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XN
	{
		enum class EncoderMode { i286, i386_16, i386_32 };
		enum class Reg {
			NO = 0,
			// i286 mode
			AX = 0x2001, CX = 0x2002, DX = 0x2004, BX = 0x2008,
			SP = 0x2010, BP = 0x2020, SI = 0x2040, DI = 0x2080,
			AL = 0x1001, CL = 0x1002, DL = 0x1004, BL = 0x1008,
			AH = 0x1010, CH = 0x1020, DH = 0x1040, BH = 0x1080,
			CS = 0x2100, DS = 0x2200, ES = 0x2400, SS = 0x2800,
			// i386 mode
			EAX = 0x4001, ECX = 0x4002, EDX = 0x4004, EBX = 0x4008,
			ESP = 0x4010, EBP = 0x4020, ESI = 0x4040, EDI = 0x4080,
			CR0 = 0x014000, CR1 = 0x024000, CR2 = 0x044000, CR3 = 0x084000,
			CR4 = 0x104000, CR5 = 0x204000, CR6 = 0x404000, CR7 = 0x804000,
		};
		enum class JCC {
			JA = 0x77, JAE = 0x73, JB = 0x72, JBE = 0x76,
			JC = 0x72, JE = 0x74, JG = 0x7F, JGE = 0x7D,
			JL = 0x7C, JLE = 0x7E, JNA = 0x76, JNAE = 0x72,
			JNB = 0x73, JNBE = 0x77, JNC = 0x73, JNE = 0x75,
			JNG = 0x7E, JNGE = 0x7C, JNL = 0x7D, JNLE = 0x7F,
			JNO = 0x71, JNP = 0x7B, JNS = 0x79, JNZ = 0x75,
			JO = 0x70, JP = 0x7A, JPE = 0x7A, JPO = 0x7B,
			JS = 0x78, JZ = 0x74,
		};
		enum class AROP {
			ADC = 0x0210, ADD = 0x0000, AND = 0x0420, CMP = 0x0738,
			OR  = 0x0108, SBB = 0x0318, SUB = 0x0528, XOR = 0x0630
		};
		enum class UOP {
			DEC = 0x01, INC = 0x00
		};
		enum class MULOP {
			NEG = 0x03, NOT = 0x02,
			MUL = 0x04, DIV = 0x06, IMUL = 0x05, IDIV = 0x07
		};
		enum class SHOP {
			RCL = 0x02, RCR = 0x03, ROL = 0x00, ROR = 0x01,
			SAL = 0x04, SAR = 0x07, SHL = 0x04, SHR = 0x05,
		};

		class i286_EncoderContext
		{
			EncoderMode _mode;
			DataBlock & _dest;
		public:
			i286_EncoderContext(DataBlock & dest, EncoderMode initmode = EncoderMode::i286);
			void set_encoder_mode(EncoderMode mode);
			EncoderMode get_encoder_mode(void);
			DataBlock & get_destination(void);
			bool is_i386_mode(void);
			bool is_32_bit_mode(void);
			bool is_regular_register(Reg reg);
			bool is_segment_register(Reg reg);
			bool is_control_register(Reg reg);
			uint8 register_code(Reg reg);
			uint8 register_quant(Reg reg);
			uint8 make_mod(uint8 reg, uint8 mod, uint8 reg_mem);
			uint8 segment_prefix(Reg reg);
			uint8 register_address_mode(Reg reg);
			void encode_padding_zeroes(uint align);
			void encode_padding_int3(uint align);
			void encode_data(const void * data, uint length);
			uint get_current_position(void);
			void encode_universal(uint8 opcode, uint8 ri, Reg ptr, Reg seg, int offset);
			void encode_call(uint quant);
			void encode_call(uint quant, Reg addr);
			void encode_call(uint quant, Reg addr_ptr, Reg addr_seg, int addr_offset);
			void encode_cbw(void);
			void encode_cli(void);
			void encode_cwd(void);
			void encode_hlt(void);
			void encode_in(Reg dest, int port);
			void encode_in(Reg dest, Reg port);
			void encode_int3(void);
			void encode_into(void);
			void encode_int(uint8 iv);
			void encode_iret(void);
			void encode_jcc(JCC jcc);
			void encode_jmp(uint quant);
			void encode_jmp(uint quant, Reg addr);
			void encode_jmp(uint quant, Reg addr_ptr, Reg addr_seg, int addr_offset);
			void encode_lahf(void);
			void encode_lds(Reg dest, Reg src_ptr, Reg src_seg, int src_offset);
			void encode_les(Reg dest, Reg src_ptr, Reg src_seg, int src_offset);
			void encode_lea(Reg dest, Reg src_ptr, int src_offset);
			void encode_mov_reg_reg(uint quant, Reg dest, Reg src);
			void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, Reg src_seg, int src_offset);
			void encode_mov_reg_imm(uint quant, Reg dest, int src);
			void encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg dest_seg, int dest_offset, Reg src);
			void encode_arop_reg_reg(AROP op, uint quant, Reg dest, Reg src);
			void encode_arop_reg_mem(AROP op, uint quant, Reg dest, Reg src_ptr, Reg src_seg, int src_offset);
			void encode_arop_reg_imm(AROP op, uint quant, Reg dest, int src);
			void encode_arop_mem_reg(AROP op, uint quant, Reg dest_ptr, Reg dest_seg, int dest_offset, Reg src);
			void encode_uop_reg(UOP op, uint quant, Reg dest);
			void encode_mulop_reg(MULOP op, uint quant, Reg src);
			void encode_nop(void);
			void encode_out(Reg dest, int port);
			void encode_out(Reg dest, Reg port);
			void encode_pop(Reg reg);
			void encode_pop_all(void);
			void encode_pop_flags(void);
			void encode_push(Reg reg);
			void encode_push(int imm);
			void encode_push_all(void);
			void encode_push_flags(void);
			void encode_shop_reg(SHOP op, uint quant, Reg dest);
			void encode_shop_reg(SHOP op, uint quant, Reg dest, Reg rot);
			void encode_shop_reg(SHOP op, uint quant, Reg dest, int rot);
			void encode_ret(int revert, bool far);
			void encode_sahf(void);
			void encode_sti(void);
			void encode_test_reg_reg(uint quant, Reg dest, Reg src);
			void encode_test_reg_imm(uint quant, Reg dest, int src);
			void encode_test_mem_reg(uint quant, Reg dest_ptr, Reg dest_seg, int dest_offset, Reg src);
			void encode_xchg_reg_reg(uint quant, Reg dest, Reg src);
			void encode_xchg_reg_mem(uint quant, Reg dest, Reg src_ptr, Reg src_seg, int src_offset);
		};
	}
}