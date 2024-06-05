#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XA
	{
		enum class ArgumentSemantics : uint
		{
			Unknown			= 0x00,
			Unclassified	= 0x01,
			Integer			= 0x02,
			SignedInteger	= 0x03,
			FloatingPoint	= 0x04,
			Object			= 0x05,
			This			= 0x06,
			RTTI			= 0x07,
			ErrorData		= 0x08,
		};
		enum ReferenceClass : uint8
		{
			ReferenceNull		= 0x00,
			ReferenceExternal	= 0x01,
			ReferenceData		= 0x02,
			ReferenceCode		= 0x03,
			ReferenceArgument	= 0x04,
			ReferenceRetVal		= 0x05,
			ReferenceLocal		= 0x06,
			ReferenceInit		= 0x07,
			ReferenceSplitter	= 0x08,
			ReferenceTransform	= 0x80,
			ReferenceLiteral	= 0x81,
		};
		enum ReferenceFlags : uint8
		{
			ReferenceFlagRefer		= 0x00,
			ReferenceFlagInvoke		= 0x01,
			ReferenceFlagUnaligned	= 0x02,
			ReferenceFlagShort		= 0x04,
			ReferenceFlagLong		= 0x08,
			ReferenceFlagVectorCom	= 0x10,
		};
		enum ReferenceTransforms : uint32
		{
			TransformNull			= 0x0000, // invalid
			TransformFollowPointer	= 0x0001, // 1 argument - pointer type; retval - destination
			TransformTakePointer	= 0x0002, // 1 argument - any type; retval - pointer
			TransformAddressOffset	= 0x0003, // 3 arguments - base object; offset, literals allowed; offset scale = 1, literals allowed; retval - sub object
			TransformBlockTransfer	= 0x0004, // 2 arguments - dest; src; retval - dest
			TransformInvoke			= 0x0005, // N + 1 arguments - the function to invoke pointer; its arguments then; retval - the function's one
			TransformTemporary		= 0x0006, // 1 argument - no type data; retval - new entity, finilizer supplied
			TransformBreakIf		= 0x0007, // 3 arguments - anything; integer; literal; retval - anything
			TransformSplit			= 0x0008, // 1 argument - anything; retval - anything
			TransformAtomicAdd		= 0x0009, // 2 arguments - destination integer; integer; retval - a copy of the new destination
			TransformLogicalAnd		= 0x0010, // any number of integer arguments; retval - integer
			TransformLogicalOr		= 0x0011, // any number of integer arguments; retval - integer
			TransformLogicalFork	= 0x0012, // 3 arguments - integer; no type; no type; retval - no type
			TransformLogicalNot		= 0x0028, // 1 argument - integer; retval - integer
			TransformLogicalSame	= 0x0013, // 2 arguments - integers; retval - integer
			TransformLogicalNotSame	= 0x0014, // 2 arguments - integers; retval - integer
			TransformVectorAnd		= 0x0020, // 2 arguments - integers; retval - integer
			TransformVectorOr		= 0x0021, // 2 arguments - integers; retval - integer
			TransformVectorXor		= 0x0022, // 2 arguments - integers; retval - integer
			TransformVectorInverse	= 0x0023, // 1 argument - integer; retval - integer
			TransformVectorShiftL	= 0x0024, // 2 arguments - integers; retval - integer
			TransformVectorShiftR	= 0x0025, // 2 arguments - integers; retval - integer
			TransformVectorShiftAL	= 0x0026, // 2 arguments - integers; retval - integer
			TransformVectorShiftAR	= 0x0027, // 2 arguments - integers; retval - integer
			TransformVectorIsZero	= 0x0028, // 1 argument - integer; retval - integer
			TransformVectorNotZero	= 0x0029, // 1 argument - integer; retval - integer
			TransformIntegerEQ		= 0x0030, // 2 arguments - integers; retval - integer
			TransformIntegerNEQ		= 0x0031, // 2 arguments - integers; retval - integer
			TransformIntegerULE		= 0x0032, // 2 arguments - integers; retval - integer
			TransformIntegerUGE		= 0x0033, // 2 arguments - integers; retval - integer
			TransformIntegerUL		= 0x0034, // 2 arguments - integers; retval - integer
			TransformIntegerUG		= 0x0035, // 2 arguments - integers; retval - integer
			TransformIntegerSLE		= 0x0036, // 2 arguments - integers; retval - integer
			TransformIntegerSGE		= 0x0037, // 2 arguments - integers; retval - integer
			TransformIntegerSL		= 0x0038, // 2 arguments - integers; retval - integer
			TransformIntegerSG		= 0x0039, // 2 arguments - integers; retval - integer
			TransformIntegerUResize	= 0x003A, // 1 argument - integer; retval - integer
			TransformIntegerSResize	= 0x003B, // 1 argument - integer; retval - integer
			TransformIntegerInverse	= 0x003C, // 1 argument - integer; retval - integer
			TransformIntegerAbs		= 0x003D, // 1 argument - integer; retval - integer
			TransformIntegerAdd		= 0x0040, // 2 arguments - integers; retval - integer
			TransformIntegerSubt	= 0x0041, // 2 arguments - integers; retval - integer
			TransformIntegerUMul	= 0x0042, // 2 arguments - integers; retval - integer
			TransformIntegerSMul	= 0x0043, // 2 arguments - integers; retval - integer
			TransformIntegerUDiv	= 0x0044, // 2 arguments - integers; retval - integer
			TransformIntegerSDiv	= 0x0045, // 2 arguments - integers; retval - integer
			TransformIntegerUMod	= 0x0046, // 2 arguments - integers; retval - integer
			TransformIntegerSMod	= 0x0047, // 2 arguments - integers; retval - integer
			// FPU/VPU Extension, may be affected by the SHORT/LONG flags
			TransformFloatResize	= 0x0080, // 1 argument - a vector to resize; retval - vector; the flag specifies the source precision
			TransformFloatGather	= 0x0081, // N arguments - creates a vector from scalar inputs; retval - vector; type specification matters
			TransformFloatScatter	= 0x0082, // 1 argument - a vector to split, N arguments - the destinations; retval - the source; type specification matters
			TransformFloatRecombine	= 0x0083, // 2 arguments - a vector to recombine, a recombination mask literal; retval - vector;
			TransformFloatInteger	= 0x0084, // 1 argument - a vector; retval - an integer array; type specification matters
			TransformFloatIsZero	= 0x0090, // 1 argument - a vector; retval - integer
			TransformFloatNotZero	= 0x0091, // 1 argument - a vector; retval - integer
			TransformFloatEQ		= 0x0092, // 2 arguments - vectors; retval - integer
			TransformFloatNEQ		= 0x0093, // 2 arguments - vectors; retval - integer
			TransformFloatLE		= 0x0094, // 2 arguments - vectors; retval - integer
			TransformFloatGE		= 0x0095, // 2 arguments - vectors; retval - integer
			TransformFloatL			= 0x0096, // 2 arguments - vectors; retval - integer
			TransformFloatG			= 0x0097, // 2 arguments - vectors; retval - integer
			TransformFloatAdd		= 0x00A0, // 2 arguments - vectors; retval - vector
			TransformFloatSubt		= 0x00A1, // 2 arguments - vectors; retval - vector
			TransformFloatMul		= 0x00A2, // 2 arguments - vectors; retval - vector
			TransformFloatMulAdd	= 0x00A3, // 3 arguments - vectors; retval - vector
			TransformFloatMulSubt	= 0x00A4, // 3 arguments - vectors; retval - vector
			TransformFloatDiv		= 0x00A5, // 2 arguments - vectors; retval - vector
			TransformFloatAbs		= 0x00A6, // 1 argument - a vector; retval - vector
			TransformFloatInverse	= 0x00A7, // 1 argument - a vector; retval - vector
			TransformFloatSqrt		= 0x00A8, // 1 argument - a vector; retval - vector
			TransformFloatReduce	= 0x00A9, // 1 argument - a vector; retval - scalar
		};
		enum StatementOpcodes : uint32
		{
			OpcodeNOP				= 0x0000,
			OpcodeTrap				= 0x0001,
			OpcodeOpenScope			= 0x0002,
			OpcodeCloseScope		= 0x0003,
			OpcodeExpression		= 0x0004,
			OpcodeNewLocal			= 0x0005,
			OpcodeUnconditionalJump	= 0x0006,
			OpcodeConditionalJump	= 0x0007,
			OpcodeControlReturn		= 0x0008,
		};

		struct ObjectSize
		{
			uint num_bytes;
			uint num_words;
		};
		struct ArgumentSpecification
		{
			ObjectSize size;
			ArgumentSemantics semantics;
		};
		struct ObjectReference
		{
			union {
				struct {
					uint8 ref_class;
					uint8 ref_flags;
					uint16 unused;
					uint32 index;
				};
				uint64 qword;
			};
		};
		struct FinalizerReference
		{
			ObjectReference final;
			Array<ObjectReference> final_args;

			FinalizerReference(void);
		};
		struct ExpressionTree
		{
			ObjectReference self;
			Array<ExpressionTree> inputs;
			Array<ArgumentSpecification> input_specs;
			FinalizerReference retval_final;
			ArgumentSpecification retval_spec;

			ExpressionTree(void);
		};
		struct Statement
		{
			uint32 opcode;
			ObjectSize attachment;
			FinalizerReference attachment_final;
			ExpressionTree tree;
		};
		struct Function
		{
			Array<string> extrefs;
			Array<uint8> data;
			ArgumentSpecification retval;
			Array<ArgumentSpecification> inputs;
			Array<Statement> instset;

			Function(void);
			void Clear(void);
			void Save(Streaming::Stream * dest);
			void Load(Streaming::Stream * src);
		};
		struct TranslatedFunction
		{
			Array<uint8> data;
			Array<uint8> code;
			Volumes::Dictionary< string, Array<uint32> > extrefs;
			Array<uint32> code_reloc, data_reloc;

			TranslatedFunction(void);
			void Clear(void);
			void Save(Streaming::Stream * dest);
			void Load(Streaming::Stream * src);
		};
	}
}