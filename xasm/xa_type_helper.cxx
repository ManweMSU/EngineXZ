#include "xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace TH {
			ObjectSize MakeSize(void)
			{
				ObjectSize result;
				result.num_bytes = result.num_words = 0;
				return result;
			}
			ObjectSize MakeSize(int bytes, int words)
			{
				ObjectSize result;
				result.num_bytes = bytes;
				result.num_words = words;
				return result;
			}
			ArgumentSpecification MakeSpec(ArgumentSemantics semantics, int bytes, int words)
			{
				ArgumentSpecification result;
				result.semantics = semantics;
				result.size = MakeSize(bytes, words);
				return result;
			}
			ArgumentSpecification MakeSpec(ArgumentSemantics semantics, const ObjectSize & size)
			{
				ArgumentSpecification result;
				result.semantics = semantics;
				result.size = size;
				return result;
			}
			ArgumentSpecification MakeSpec(int bytes, int words)
			{
				ArgumentSpecification result;
				result.semantics = (bytes || words) ? ArgumentSemantics::Unclassified : ArgumentSemantics::Unknown;
				result.size = MakeSize(bytes, words);
				return result;
			}
			ArgumentSpecification MakeSpec(const ObjectSize & size)
			{
				ArgumentSpecification result;
				result.semantics = (size.num_bytes || size.num_words) ? ArgumentSemantics::Unclassified : ArgumentSemantics::Unknown;
				result.size = size;
				return result;
			}
			ArgumentSpecification MakeSpec(void)
			{
				ArgumentSpecification result;
				result.semantics = ArgumentSemantics::Unknown;
				result.size = MakeSize();
				return result;
			}
			ObjectReference MakeRef(void)
			{
				ObjectReference result;
				result.ref_class = ReferenceNull;
				result.ref_flags = ReferenceFlagRefer;
				result.unused = 0;
				result.index = 0;
				return result;
			}
			ObjectReference MakeRef(uint8 cls)
			{
				ObjectReference result;
				result.ref_class = cls;
				result.ref_flags = ReferenceFlagRefer;
				result.unused = 0;
				result.index = 0;
				return result;
			}
			ObjectReference MakeRef(uint8 cls, int index, uint8 flags)
			{
				ObjectReference result;
				result.ref_class = cls;
				result.ref_flags = flags;
				result.unused = 0;
				result.index = index;
				return result;
			}
			FinalizerReference MakeFinal(void)
			{
				FinalizerReference result;
				result.final = MakeRef();
				return result;
			}
			FinalizerReference MakeFinal(const ObjectReference & final)
			{
				FinalizerReference result;
				result.final = final;
				return result;
			}
			FinalizerReference MakeFinal(const ObjectReference & final, const ObjectReference & xa1)
			{
				FinalizerReference result;
				result.final = final;
				result.final_args << xa1;
				return result;
			}
			FinalizerReference MakeFinal(const ObjectReference & final, const ObjectReference & xa1, const ObjectReference & xa2)
			{
				FinalizerReference result;
				result.final = final;
				result.final_args << xa1;
				result.final_args << xa2;
				return result;
			}
			FinalizerReference MakeFinal(const ObjectReference & final, const Array<ObjectReference> & xa)
			{
				FinalizerReference result;
				result.final = final;
				result.final_args = xa;
				return result;
			}
			ExpressionTree MakeTree(void)
			{
				ExpressionTree result;
				result.self = MakeRef();
				result.retval_spec = MakeSpec();
				result.retval_final = MakeFinal();
				return result;
			}
			ExpressionTree MakeTree(const ObjectReference & self)
			{
				ExpressionTree result;
				result.self = self;
				result.retval_spec = MakeSpec();
				result.retval_final = MakeFinal();
				return result;
			}
			ExpressionTree MakeTreeLiteral(const ObjectSize & literal)
			{
				ExpressionTree result;
				result.self = MakeRef(ReferenceLiteral);
				result.retval_spec = MakeSpec(literal);
				result.retval_final = MakeFinal();
				return result;
			}
			ExpressionTree & AddTreeInput(ExpressionTree & tree, const ExpressionTree & input, const ArgumentSpecification & input_spec)
			{
				tree.inputs << input;
				tree.input_specs << input_spec;
				return tree;
			}
			ExpressionTree & AddTreeOutput(ExpressionTree & tree, const ArgumentSpecification & output_spec)
			{
				if (tree.retval_spec.size.num_bytes || tree.retval_spec.size.num_words) throw InvalidStateException();
				tree.retval_spec = output_spec;
				tree.retval_final = MakeFinal();
				return tree;
			}
			ExpressionTree & AddTreeOutput(ExpressionTree & tree, const ArgumentSpecification & output_spec, const FinalizerReference & final)
			{
				if (tree.retval_spec.size.num_bytes || tree.retval_spec.size.num_words) throw InvalidStateException();
				tree.retval_spec = output_spec;
				tree.retval_final = final;
				return tree;
			}
			Statement MakeStatementNOP(void)
			{
				Statement result;
				result.opcode = OpcodeNOP;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementTrap(void)
			{
				Statement result;
				result.opcode = OpcodeTrap;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementOpenScope(void)
			{
				Statement result;
				result.opcode = OpcodeOpenScope;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementCloseScope(void)
			{
				Statement result;
				result.opcode = OpcodeCloseScope;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementExpression(const ExpressionTree & tree)
			{
				Statement result;
				result.opcode = OpcodeExpression;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = tree;
				return result;
			}
			Statement MakeStatementNewLocal(const ObjectSize & size)
			{
				Statement result;
				result.opcode = OpcodeNewLocal;
				result.attachment = size;
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementNewLocal(const ObjectSize & size, const FinalizerReference & final)
			{
				Statement result;
				result.opcode = OpcodeNewLocal;
				result.attachment = size;
				result.attachment_final = final;
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementNewLocal(const ObjectSize & size, const ExpressionTree & init)
			{
				Statement result;
				result.opcode = OpcodeNewLocal;
				result.attachment = size;
				result.attachment_final = MakeFinal();
				result.tree = init;
				return result;
			}
			Statement MakeStatementNewLocal(const ObjectSize & size, const ExpressionTree & init, const FinalizerReference & final)
			{
				Statement result;
				result.opcode = OpcodeNewLocal;
				result.attachment = size;
				result.attachment_final = final;
				result.tree = init;
				return result;
			}
			Statement MakeStatementJump(int dist_rel_next_inst)
			{
				Statement result;
				result.opcode = OpcodeUnconditionalJump;
				result.attachment = MakeSize(dist_rel_next_inst, 0);
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementJump(int dist_rel_next_inst, const ExpressionTree & condition)
			{
				Statement result;
				result.opcode = OpcodeConditionalJump;
				result.attachment = MakeSize(dist_rel_next_inst, 0);
				result.attachment_final = MakeFinal();
				result.tree = condition;
				return result;
			}
			Statement MakeStatementReturn(void)
			{
				Statement result;
				result.opcode = OpcodeControlReturn;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = MakeTree();
				return result;
			}
			Statement MakeStatementReturn(const ExpressionTree & retval_init)
			{
				Statement result;
				result.opcode = OpcodeControlReturn;
				result.attachment = MakeSize();
				result.attachment_final = MakeFinal();
				result.tree = retval_init;
				return result;
			}
			DataBlock * CreateSampleDescription(void)
			{
				SafePointer<DataBlock> result = new DataBlock(0x20);
				result->SetLength(2);
				result->ElementAt(0) = result->ElementAt(1) = 0;
				result->Retain();
				return result;
			}
			void AppendSampleDescription(DataBlock * desc, int usage, int format, int bits)
			{
				if (!desc || bits <= 0 || bits > 64 || usage < 0 || format < 0 || usage > 511 || format > 511) throw InvalidArgumentException();
				uint16 word = (bits << 9) | usage | format;
				int length = desc->Length();
				if (length & 1 || length < 2) throw InvalidArgumentException();
				desc->SetLength(length + 2);
				*reinterpret_cast<uint16 *>(desc->GetBuffer() + length - 2) = word;
				*reinterpret_cast<uint16 *>(desc->GetBuffer() + length) = 0;
			}
			bool ValidateSampleDescription(const void * address, int maxlength)
			{
				if (!address) return false;
				auto data = reinterpret_cast<const uint16 *>(address);
				int index = 0;
				while (true) {
					if (2 * index + 1 >= maxlength) return false;
					if (!data[index]) return true;
					index++;
				}
			}
			bool SampleDescriptionAreSame(const void * a, const void * b)
			{
				auto d1 = reinterpret_cast<const uint16 *>(a);
				auto d2 = reinterpret_cast<const uint16 *>(b);
				int index = 0;
				while (true) {
					if ((d1[index] & ~BlockTransferFormatReadOnly) != (d2[index] & ~BlockTransferFormatReadOnly)) return false;
					if (!d1[index]) return true;
					index++;
				}
			}
			int GetSampleDescriptionNumberOfChannels(const void * desc)
			{
				if (!desc) return 0;
				int count = 0;
				auto data = reinterpret_cast<const uint16 *>(desc);
				while (data[count]) count++;
				return count;
			}
			int GetSampleDescriptionBitLength(const void * desc)
			{
				if (!desc) return 0;
				auto data = reinterpret_cast<const uint16 *>(desc);
				auto channels = GetSampleDescriptionNumberOfChannels(desc);
				int length = 0;
				for (int i = 0; i < channels; i++) length += (int(data[i]) >> 9);
				return length;
			}
			int GetSampleDescriptionChannelUsage(const void * desc, int index)
			{
				if (!desc) return 0;
				auto data = reinterpret_cast<const uint16 *>(desc);
				return data[index] & BlockTransferMaskChannel;
			}
			bool GetSampleDescriptionEndianness(const void * desc)
			{
				if (!desc) return false;
				auto data = reinterpret_cast<const uint16 *>(desc);
				return (data[0] & BlockTransferFormatBE) ? true : false;
			}
			void GetSampleDescriptionChannelByIndex(const void * desc, int index, int * first_bit, int * bit_length, int * format)
			{
				if (!desc) return;
				auto data = reinterpret_cast<const uint16 *>(desc);
				int offset = 0;
				for (int i = 0; i < index; i++) {
					int bl = int(data[i]) >> 9;
					offset += bl;
				}
				int bl = int(data[index]) >> 9;
				if (first_bit) *first_bit = offset;
				if (bit_length) *bit_length = bl;
				if (format) *format = data[index] & BlockTransferMaskFormat;
			}
			bool GetSampleDescriptionChannelByUsage(const void * desc, int usage, int * first_bit, int * bit_length, int * format)
			{
				if (!desc) return false;
				auto data = reinterpret_cast<const uint16 *>(desc);
				int offset = 0;
				int index = 0;
				while (data[index]) {
					int bl = int(data[index]) >> 9;
					if ((data[index] & BlockTransferMaskChannel) == usage) {
						if (first_bit) *first_bit = offset;
						if (bit_length) *bit_length = bl;
						if (format) *format = data[index] & BlockTransferMaskFormat;
						return true;
					}
					offset += bl;
					index++;
				}
				return false;
			}
		}
	}
}