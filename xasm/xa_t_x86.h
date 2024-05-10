#pragma once

#include "xa_trans.h"

namespace Engine
{
	namespace XA
	{
		namespace X86
		{
			typedef uint Reg;
			namespace Reg32
			{
				constexpr uint NO = 0, ST0 = 0x01000000,
				EAX = 0x0001, ECX = 0x0002, EDX = 0x0004, EBX = 0x0008,
				ESP = 0x0010, EBP = 0x0020, ESI = 0x0040, EDI = 0x0080,
				AX = 0x0001, CX = 0x0002, DX = 0x0004, BX = 0x0008,
				SP = 0x0010, BP = 0x0020, SI = 0x0040, DI = 0x0080,
				AL = 0x0001, CL = 0x0002, DL = 0x0004, BL = 0x0008,
				AH = 0x0010, CH = 0x0020, DH = 0x0040, BH = 0x0080;
			};
			namespace Reg64
			{
				constexpr uint NO = 0,
				RAX = 0x0001, RCX = 0x0002, RDX = 0x0004, RBX = 0x0008,
				RSP = 0x0010, RBP = 0x0020, RSI = 0x0040, RDI = 0x0080,
				R8 = 0x0100, R9 = 0x0200, R10 = 0x0400, R11 = 0x0800,
				R12 = 0x1000, R13 = 0x2000, R14 = 0x4000, R15 = 0x8000,
				XMM0 = 0x00010000, XMM1 = 0x00020000, XMM2 = 0x00040000, XMM3 = 0x00080000,
				XMM4 = 0x00100000, XMM5 = 0x00200000, XMM6 = 0x00400000, XMM7 = 0x00800000;
			};

			enum class ABI { CDECL, THISCALL, FASTCALL };

			enum class arOp : uint8 { ADD = 0x02, ADC = 0x12, SBB = 0x1A, SUB = 0x2A, AND = 0x22, OR = 0x0A, XOR = 0x32, CMP = 0x3A };
			enum class mdOp : uint8 { MUL = 0x4, DIV = 0x6, IMUL = 0x5, IDIV = 0x7 };
			enum class shOp : uint8 { ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR };

			enum DispositionFlags {
				DispositionRegister	= 0x01,
				DispositionPointer	= 0x02,
				DispositionDiscard	= 0x04,
				DispositionCompress	= 0x10, // i386 mode only
				DispositionReuse	= 0x20, // i386 mode only
				DispositionAny		= DispositionRegister | DispositionPointer
			};

			struct JumpRelocStruct {
				uint machine_offset_at;
				uint machine_offset_relative_to;
				uint xasm_offset_jump_to;
			};
			struct ArgumentPassageInfo {
				int index;		// 0, 1, ... - real arguments; -1 - retval
				Reg reg;		// holder register; NO for stack storage;
				bool indirect;	// pass-by-reference
			};
			struct ArgumentStorageSpec {
				Reg bound_to;
				Reg bound_to_hi_dword;
				int bp_offset;
				bool indirect;
			};
			struct InternalDisposition {
				int flags;
				Reg reg;
				int size;
			};
			struct VectorDisposition {
				int size;
				Reg reg_lo;
				Reg reg_hi;
			};
			struct LocalDisposition {
				int bp_offset;
				int size;
				FinalizerReference finalizer;
			};
			struct LocalScope {
				int frame_base; // *BP + frame_base = real frame base
				int frame_size;
				int frame_size_unused;
				int first_local_no;
				int current_split_offset;
				Array<LocalDisposition> locals = Array<LocalDisposition>(0x20);
				bool shift_sp, temporary;
			};

			class EncoderContext
			{
				bool _x64_mode;
			protected:
				TranslatedFunction & _dest;
				const Function & _src;
				CallingConvention _conv;
				ABI _abi;
				Array<uint> _org_inst_offsets;
				Array<JumpRelocStruct> _jump_reloc;
				ArgumentStorageSpec _retval;
				Array<ArgumentStorageSpec> _inputs;
				int _scope_frame_base, _unroll_base, _current_instruction, _stack_oddity, _stack_clear_size;
				Volumes::Stack<LocalScope> _scopes;
				Volumes::Stack<LocalDisposition> _init_locals;
			protected:
				EncoderContext(CallingConvention conv, TranslatedFunction & dest, const Function & src, bool x64);
				static uint8 regular_register_code(uint reg);
				static uint8 xmm_register_code(uint reg);
				static uint8 make_rex(bool size64_W, bool reg_ext_R, bool index_ext_X, bool opt_ext_B);
				static uint8 make_mod(uint8 reg_lo_3, uint8 mod, uint8 reg_mem_lo_3);
				static bool is_vector_retval_transform(uint opcode);
				uint32 object_size(const ObjectSize & size);
				uint32 word_size(void);
				uint32 word_align(const ObjectSize & size);
				int allocate_temporary(const ObjectSize & size, int * index = 0);
				Reg allocate_xmm(uint xmm_used, uint xmm_protected);
				void allocate_xmm(uint xmm_used, uint xmm_protected, VectorDisposition & disp);
				void assign_finalizer(int local_index, const FinalizerReference & final);
				void relocate_code_at(int offset);
				void relocate_data_at(int offset);
				void refer_object_at(const string & name, int offset);
				void encode_preserve(Reg reg, uint reg_in_use, uint reg_unmask, bool cond_if);
				void encode_restore(Reg reg, uint reg_in_use, uint reg_unmask, bool cond_if);
				void encode_open_scope(int of_size, bool temporary, int enf_base);
				virtual void encode_finalize_scope(const LocalScope & scope, uint reg_in_use = 0) = 0;
				void encode_close_scope(uint reg_in_use = 0);
				void encode_blt(Reg dest, bool dest_indirect, Reg src, bool src_indirect, int size, uint reg_in_use);
				void encode_blt(Reg dest_ptr, Reg src_ptr, int size, uint reg_in_use);
				void encode_reg_load_32(Reg dest, Reg src_ptr, int src_offs, int size, bool allow_compress, uint reg_in_use);
				void encode_emulate_mul_64(void); // uses all registers, input on [EAX], [ECX], output on EBX:ECX
				void encode_emulate_div_64(void); // uses all registers, input on [EAX], [ECX]; output div on EAX:EDX, mod on ESI:EDI
				void encode_emulate_collect_signum(Reg data, Reg sgn); // pushes abs([data]) onto stack, inverts sgn if [data] is negative. updates [data]. uses ESI and EDI
				void encode_emulate_set_signum(Reg data_lo, Reg data_hi, Reg sgn); // inverts data_lo:data_hi if sgn != 0. uses ESI and EDI
				void encode_mov_reg_reg(uint quant, Reg dest, Reg src);
				void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr);
				void encode_mov_reg_mem(uint quant, Reg dest, Reg src_ptr, int src_offset);
				void encode_mov_reg_const(uint quant, Reg dest, uint64 value);
				void encode_mov_mem_reg(uint quant, Reg dest_ptr, Reg src);
				void encode_mov_mem_reg(uint quant, Reg dest_ptr, int dest_offset, Reg src);
				void encode_mov_reg_xmm(uint quant, Reg dest, Reg src);
				void encode_mov_xmm_reg(uint quant, Reg dest, Reg src);
				void encode_mov_mem_xmm(uint quant, Reg dest, int dest_offset, Reg src);
				void encode_mov_xmm_mem(uint quant, Reg dest, Reg src, int src_offset);
				void encode_mov_xmm_mem_hi(uint quant, Reg dest, Reg src, int src_offset);
				void encode_lea(Reg dest, Reg src_ptr, int src_offset);
				void encode_push(Reg reg);
				void encode_pop(Reg reg);
				void encode_add(Reg reg, int literal);
				void encode_and(Reg reg, int literal);
				void encode_xor(Reg reg, int literal);
				void encode_test(uint quant, Reg reg, int literal);
				void encode_operation(uint quant, arOp op, Reg to, Reg value_ptr, bool indirect = false, int value_offset = 0);
				void encode_mul_div(uint quant, mdOp op, Reg value_ptr, bool indirect = false, int value_offset = 0);
				void encode_fld(uint quant, Reg src_ptr, int src_offset);
				void encode_fild(uint quant, Reg src_ptr, int src_offset);
				void encode_fldz(void);
				void encode_fstp(uint quant, Reg src_ptr, int src_offset);
				void encode_fisttp(uint quant, Reg src_ptr, int src_offset);
				void encode_fcomp(uint quant, Reg a2_ptr, int a2_offset);
				void encode_fcompp(void);
				void encode_fstsw(void);
				void encode_fadd(uint quant, Reg a2_ptr, int a2_offset);
				void encode_fsub(uint quant, Reg a2_ptr, int a2_offset);
				void encode_fmul(uint quant, Reg a2_ptr, int a2_offset);
				void encode_fdiv(uint quant, Reg a2_ptr, int a2_offset);
				void encode_fabs(void);
				void encode_fneg(void);
				void encode_fsqrt(void);
				void encode_invert(uint quant, Reg reg);
				void encode_negative(uint quant, Reg reg);
				void encode_shift(uint quant, shOp op, Reg reg, int by = 0, bool indirect = false, int value_offset = 0);
				void encode_shl(Reg reg, int bits);
				void encode_shr(Reg reg, int bits);
				void encode_call(Reg func_ptr, bool indirect);
				void encode_put_addr_of(Reg dest, const ObjectReference & value);
			public:
				void encode_debugger_trap(void);
				void encode_pure_ret(int bytes_unroll = 0);
				virtual void encode_function_prologue(void) = 0;
				virtual void encode_function_epilogue(void) = 0;
				virtual void encode_scope_unroll(int inst_current, int inst_jump_to) = 0;
				virtual void process_encoding(void) = 0;
				virtual void finalize_encoding(void) = 0;
			};
		}
	}
}