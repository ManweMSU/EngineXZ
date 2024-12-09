#pragma once

#include "xa_types.h"

namespace Engine
{
	namespace XA
	{
		namespace TH {
			ObjectSize MakeSize(void);
			ObjectSize MakeSize(int bytes, int words);
			ArgumentSpecification MakeSpec(ArgumentSemantics semantics, int bytes, int words);
			ArgumentSpecification MakeSpec(ArgumentSemantics semantics, const ObjectSize & size);
			ArgumentSpecification MakeSpec(int bytes, int words);
			ArgumentSpecification MakeSpec(const ObjectSize & size);
			ArgumentSpecification MakeSpec(void);
			ObjectReference MakeRef(void);
			ObjectReference MakeRef(uint8 cls);
			ObjectReference MakeRef(uint8 cls, int index, uint8 flags = ReferenceFlagRefer);
			FinalizerReference MakeFinal(void);
			FinalizerReference MakeFinal(const ObjectReference & final);
			FinalizerReference MakeFinal(const ObjectReference & final, const ObjectReference & xa1);
			FinalizerReference MakeFinal(const ObjectReference & final, const ObjectReference & xa1, const ObjectReference & xa2);
			FinalizerReference MakeFinal(const ObjectReference & final, const Array<ObjectReference> & xa);
			ExpressionTree MakeTree(void);
			ExpressionTree MakeTree(const ObjectReference & self);
			ExpressionTree MakeTreeLiteral(const ObjectSize & literal);
			ExpressionTree & AddTreeInput(ExpressionTree & tree, const ExpressionTree & input, const ArgumentSpecification & input_spec);
			ExpressionTree & AddTreeOutput(ExpressionTree & tree, const ArgumentSpecification & output_spec);
			ExpressionTree & AddTreeOutput(ExpressionTree & tree, const ArgumentSpecification & output_spec, const FinalizerReference & final);
			Statement MakeStatementNOP(void);
			Statement MakeStatementTrap(void);
			Statement MakeStatementOpenScope(void);
			Statement MakeStatementCloseScope(void);
			Statement MakeStatementExpression(const ExpressionTree & tree);
			Statement MakeStatementNewLocal(const ObjectSize & size);
			Statement MakeStatementNewLocal(const ObjectSize & size, const FinalizerReference & final);
			Statement MakeStatementNewLocal(const ObjectSize & size, const ExpressionTree & init);
			Statement MakeStatementNewLocal(const ObjectSize & size, const ExpressionTree & init, const FinalizerReference & final);
			Statement MakeStatementJump(int dist_rel_next_inst);
			Statement MakeStatementJump(int dist_rel_next_inst, const ExpressionTree & condition);
			Statement MakeStatementReturn(void);
			Statement MakeStatementReturn(const ExpressionTree & retval_init);

			// Sample Description's Limitations:
			// - All endianness flags within one description must be the same
			// - The sample must have at least one channel
			// - The channel's length must be within [1, 64]
			// - For channels larger than 16 bits both their length and offset must be a multiple of 8 bits
			// - The floating-point channel's length must be either 16, 32 or 64 bits
			// - The sample's length must be either 2^N or 8N bits
			// - The sample must have either one straight alpha or one premultiplied alpha channel or neither of them
			// - The sample must not have more than one dataN or average data channel
			// - The sample must not have both dataN and average data channels

			DataBlock * CreateSampleDescription(void);
			void AppendSampleDescription(DataBlock * desc, int usage, int format, int bits);
			bool ValidateSampleDescription(const void * address, int maxlength);
			bool SampleDescriptionAreSame(const void * a, const void * b);
			int GetSampleDescriptionNumberOfChannels(const void * desc);
			int GetSampleDescriptionBitLength(const void * desc);
			int GetSampleDescriptionChannelUsage(const void * desc, int index);
			bool GetSampleDescriptionEndianness(const void * desc);
			void GetSampleDescriptionChannelByIndex(const void * desc, int index, int * first_bit = 0, int * bit_length = 0, int * format = 0);
			bool GetSampleDescriptionChannelByUsage(const void * desc, int usage, int * first_bit = 0, int * bit_length = 0, int * format = 0);
		}
	}
}