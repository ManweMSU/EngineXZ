#include "xa_dasm.h"

namespace Engine
{
	namespace XA
	{
		namespace Disassembler
		{
			void DisassemblyExternals(const Function & input, Streaming::ITextWriter & output)
			{
				for (auto & ext : input.extrefs) {
					output << L"EXTERNAL \"" << Syntax::FormatStringToken(ext) << L"\"" << IO::LineFeedSequence;
				}
				if (input.extrefs.Length()) output.LineFeed();
			}
			void DisassemblyData(const Function & input, Streaming::ITextWriter & output)
			{
				if (input.data.Length()) {
					output.WriteLine(L"DATA {");
					int len = input.data.Length();
					int pos = 0;
					const uint8 * pdata = input.data.GetBuffer();
					while (pos < len) {
						if (len - pos >= 8) {
							output << L"\tQWORD 0x" << string(*reinterpret_cast<const uint64 *>(pdata + pos), HexadecimalBase, 16) << IO::LineFeedSequence;
							pos += 8;
						} else if (len - pos >= 4) {
							output << L"\tDWORD 0x" << string(*reinterpret_cast<const uint32 *>(pdata + pos), HexadecimalBase, 8) << IO::LineFeedSequence;
							pos += 4;
						} else if (len - pos >= 2) {
							output << L"\tWORD 0x" << string(uint(*reinterpret_cast<const uint16 *>(pdata + pos)), HexadecimalBase, 4) << IO::LineFeedSequence;
							pos += 2;
						} else {
							output << L"\tBYTE 0x" << string(uint(*reinterpret_cast<const uint8 *>(pdata + pos)), HexadecimalBase, 2) << IO::LineFeedSequence;
							pos++;
						}
					}
					output.WriteLine(L"}");
					output.LineFeed();
				}
			}
			string DisassemblyInteger(uint value, bool consider_signed)
			{
				if (consider_signed) {
					int sv = value;
					if (sv > 0) return L"+" + string(sv);
					else return string(sv);
				} else {
					return string(value);
				}
			}
			string DisassemblySize(const ObjectSize & size, bool consider_signed)
			{
				DynamicString result;
				if (size.num_bytes) {
					result << DisassemblyInteger(size.num_bytes, consider_signed);
					if (size.num_words) {
						result << L" W " << DisassemblyInteger(size.num_words, consider_signed);
					}
				} else if (size.num_words) {
					result << L"W " << DisassemblyInteger(size.num_words, consider_signed);
				} else {
					result << L"0";
				}
				return result.ToString();
			}
			string DisassemblySpec(const ArgumentSpecification & spec, bool consider_signed)
			{
				DynamicString result;
				result << DisassemblySize(spec.size, consider_signed) << L" : ";
				if (spec.semantics == ArgumentSemantics::Unknown) result << L"?";
				else if (spec.semantics == ArgumentSemantics::Unclassified) result << L"-";
				else if (spec.semantics == ArgumentSemantics::Integer) result << L"UINT";
				else if (spec.semantics == ArgumentSemantics::SignedInteger) result << L"SINT";
				else if (spec.semantics == ArgumentSemantics::FloatingPoint) result << L"FLOAT";
				else if (spec.semantics == ArgumentSemantics::Object) result << L"OBJECT";
				else if (spec.semantics == ArgumentSemantics::This) result << L"THIS";
				else if (spec.semantics == ArgumentSemantics::RTTI) result << L"RTTI";
				else if (spec.semantics == ArgumentSemantics::ErrorData) result << L"ERROR";
				else throw InvalidArgumentException();
				return result.ToString();
			}
			string DisassemblyInterface(const Array<ArgumentSpecification> & inputs, const ArgumentSpecification & output, const Array<ExpressionTree> * real_inputs = 0)
			{
				DynamicString result;
				for (int i = 0; i < inputs.Length(); i++) {
					result << DisassemblySpec(inputs[i], real_inputs ? real_inputs->ElementAt(i).self.ref_class == ReferenceLiteral : false) << L" ";
				}
				result << L"=> " << DisassemblySpec(output, false);
				return result.ToString();
			}
			void DisassemblyInterface(const Function & input, Streaming::ITextWriter & output)
			{
				output << L"INTERFACE " << DisassemblyInterface(input.inputs, input.retval) << IO::LineFeedSequence;
				output << IO::LineFeedSequence;
			}
			void DisassemblyReference(const ObjectReference & ref, Streaming::ITextWriter & output)
			{
				if (ref.ref_class == ReferenceNull) output << L"NULL";
				else if (ref.ref_class == ReferenceExternal) output << L"E";
				else if (ref.ref_class == ReferenceData) output << L"D";
				else if (ref.ref_class == ReferenceCode) output << L"C";
				else if (ref.ref_class == ReferenceArgument) output << L"A";
				else if (ref.ref_class == ReferenceRetVal) output << L"R";
				else if (ref.ref_class == ReferenceLocal) output << L"L";
				else if (ref.ref_class == ReferenceInit) output << L"I";
				else if (ref.ref_class == ReferenceSplitter) output << L"S";
				else if (ref.ref_class == ReferenceTransform) output << L"@";
				else if (ref.ref_class == ReferenceLiteral) output << L"-";
				else throw InvalidArgumentException();
				if (ref.ref_class == ReferenceTransform) {
					if (ref.index == TransformFollowPointer) output << L"PTR_FOLLOW";
					else if (ref.index == TransformTakePointer) output << L"PTR_TAKE";
					else if (ref.index == TransformAddressOffset) output << L"OFFSET";
					else if (ref.index == TransformBlockTransfer) output << L"BLT";
					else if (ref.index == TransformInvoke) output << L"CALL";
					else if (ref.index == TransformTemporary) output << L"NEW";
					else if (ref.index == TransformBreakIf) output << L"BREAKIF";
					else if (ref.index == TransformSplit) output << L"SPLIT";
					else if (ref.index == TransformLogicalAnd) output << L"ALL";
					else if (ref.index == TransformLogicalOr) output << L"ANY";
					else if (ref.index == TransformLogicalFork) output << L"FORK";
					else if (ref.index == TransformLogicalSame) output << L"SAME";
					else if (ref.index == TransformLogicalNotSame) output << L"NOTSAME";
					else if (ref.index == TransformVectorAnd) output << L"AND";
					else if (ref.index == TransformVectorOr) output << L"OR";
					else if (ref.index == TransformVectorXor) output << L"XOR";
					else if (ref.index == TransformVectorInverse) output << L"INVERSE";
					else if (ref.index == TransformVectorShiftL) output << L"SHL";
					else if (ref.index == TransformVectorShiftR) output << L"SHR";
					else if (ref.index == TransformVectorShiftAL) output << L"SAL";
					else if (ref.index == TransformVectorShiftAR) output << L"SAR";
					else if (ref.index == TransformVectorIsZero) output << L"ZERO";
					else if (ref.index == TransformVectorNotZero) output << L"NOTZERO";
					else if (ref.index == TransformIntegerEQ) output << L"EQ";
					else if (ref.index == TransformIntegerNEQ) output << L"NEQ";
					else if (ref.index == TransformIntegerULE) output << L"U_LE";
					else if (ref.index == TransformIntegerUGE) output << L"U_GE";
					else if (ref.index == TransformIntegerUL) output << L"U_L";
					else if (ref.index == TransformIntegerUG) output << L"U_G";
					else if (ref.index == TransformIntegerSLE) output << L"S_LE";
					else if (ref.index == TransformIntegerSGE) output << L"S_GE";
					else if (ref.index == TransformIntegerSL) output << L"S_L";
					else if (ref.index == TransformIntegerSG) output << L"S_G";
					else if (ref.index == TransformIntegerUResize) output << L"U_RESIZE";
					else if (ref.index == TransformIntegerSResize) output << L"S_RESIZE";
					else if (ref.index == TransformIntegerInverse) output << L"NEG";
					else if (ref.index == TransformIntegerAbs) output << L"ABS";
					else if (ref.index == TransformIntegerAdd) output << L"ADD";
					else if (ref.index == TransformIntegerSubt) output << L"SUB";
					else if (ref.index == TransformIntegerUMul) output << L"U_MUL";
					else if (ref.index == TransformIntegerSMul) output << L"S_MUL";
					else if (ref.index == TransformIntegerUDiv) output << L"U_DIV";
					else if (ref.index == TransformIntegerSDiv) output << L"S_DIV";
					else if (ref.index == TransformIntegerUMod) output << L"U_MOD";
					else if (ref.index == TransformIntegerSMod) output << L"S_MOD";
					else throw InvalidArgumentException();
				} else if (ref.ref_class == ReferenceLiteral) {
				} else if (ref.index) {
					output << L"[" << string(ref.index) << L"]";
				}
			}
			void DisassemblyFinalizer(const FinalizerReference & ref, Streaming::ITextWriter & output)
			{
				DisassemblyReference(ref.final, output);
				if (ref.final_args.Length()) {
					for (int i = 0; i < ref.final_args.Length(); i++) {
						if (i) output << L", "; else output << L"(";
						DisassemblyReference(ref.final_args[i], output);
					}
					output << L")";
				}
			}
			void DisassemblyTree(const ExpressionTree & tree, Streaming::ITextWriter & output, const string & prefix)
			{
				DisassemblyReference(tree.self, output);
				if (tree.self.ref_flags & ReferenceFlagInvoke) {
					if (tree.self.ref_flags & ReferenceFlagUnaligned) output << L"!";
					output << L": " << DisassemblyInterface(tree.input_specs, tree.retval_spec, &tree.inputs) << L"(" << IO::LineFeedSequence;
					auto sp = prefix + L"\t";
					for (int i = 0; i < tree.inputs.Length(); i++) {
						output << sp;
						DisassemblyTree(tree.inputs[i], output, sp);
						if (i < tree.inputs.Length() - 1) output << L",";
						output << IO::LineFeedSequence;
					}
					output << prefix << L")";
					if (tree.retval_final.final.ref_class != ReferenceNull) {
						output << L"# ";
						DisassemblyFinalizer(tree.retval_final, output);
					}
				}
			}
			void DisassemblyStatement(const Statement & s, Streaming::ITextWriter & output)
			{
				bool has_tree = false;
				bool has_size = false;
				bool has_final = false;
				bool size_is_signed = false;
				output << L"\t";
				if (s.opcode == OpcodeNOP) {
					output << L"NOP";
				} else if (s.opcode == OpcodeTrap) {
					output << L"TRAP";
				} else if (s.opcode == OpcodeOpenScope) {
					output << L"ENTER";
				} else if (s.opcode == OpcodeCloseScope) {
					output << L"LEAVE";
				} else if (s.opcode == OpcodeExpression) {
					has_tree = true;
					output << L"EVAL";
				} else if (s.opcode == OpcodeNewLocal) {
					has_tree = has_size = has_final = true;
					output << L"NEW";
				} else if (s.opcode == OpcodeUnconditionalJump) {
					has_size = size_is_signed = true;
					output << L"JUMP";
				} else if (s.opcode == OpcodeConditionalJump) {
					has_tree = has_size = size_is_signed = true;
					output << L"JUMPIF";
				} else if (s.opcode == OpcodeControlReturn) {
					has_tree = true;
					output << L"RET";
				} else throw InvalidArgumentException();
				if (has_tree) {
					output << L" {" << IO::LineFeedSequence;
					output << L"\t\t";
					DisassemblyTree(s.tree, output, L"\t\t");
					output << IO::LineFeedSequence << L"\t}";
				}
				if (has_size) {
					output << L" @ " << DisassemblySize(s.attachment, size_is_signed);
				}
				if (has_final && s.attachment_final.final.ref_class != ReferenceNull) {
					output << L" # ";
					DisassemblyFinalizer(s.attachment_final, output);
				}
				output << IO::LineFeedSequence;
			}
			void DisassemblyCode(const Function & input, Streaming::ITextWriter & output)
			{
				if (input.instset.Length()) {
					output.WriteLine(L"CODE {");
					for (auto & s : input.instset) DisassemblyStatement(s, output);
					output.WriteLine(L"}");
				}
			}
		}
		void DisassemblyFunction(const Function & input, Streaming::ITextWriter * output)
		{
			if (!output) throw InvalidArgumentException();
			Disassembler::DisassemblyExternals(input, *output);
			Disassembler::DisassemblyData(input, *output);
			Disassembler::DisassemblyInterface(input, *output);
			Disassembler::DisassemblyCode(input, *output);
		}
	}
}