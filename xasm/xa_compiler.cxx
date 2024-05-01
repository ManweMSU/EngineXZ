#include "xa_compiler.h"
#include "xa_type_helper.h"

using namespace Engine::Syntax;

namespace Engine
{
	namespace XA
	{
		void MakeErrorInfo(const string & input, CompilerStatusDesc & status, int abs_at, int abs_to)
		{
			int len = input.Length();
			int s = abs_at;
			while (s && input[s - 1] != L'\n' && input[s - 1] != L'\r') s--;
			while (s < len && (input[s] == L' ' || input[s] == L'\t')) s++;
			int e = abs_at;
			while (e < abs_to && e < len && input[e] != L'\n' && input[e] != L'\r') e++;
			int le = e;
			while (le < len && input[le] != L'\n' && input[le] != L'\r') le++;
			status.error_line = input.Fragment(s, le - s);
			status.error_line_pos = abs_at - s;
			status.error_line_len = e - abs_at;
			status.error_line_no = 1;
			for (int i = s; i >= 0; i--) if (input[i] == L'\n') status.error_line_no++;
		}
		class CompilerException : public Exception
		{
		public:
			CompilerStatus reason;
			int at;
			CompilerException(CompilerStatus status, int token_index) : reason(status), at(token_index) {}
		};
		class CompilerContext
		{
			const string & _input;
			const Array<Token> & _text;
			Function & _output;
			int _pos;
		public:
			CompilerContext(const string & input, const Array<Token> & text, Function & output) : _input(input), _text(text), _output(output), _pos(0) {}
			void ProcessDataAtom(void)
			{
				uint quant = 0;
				int sign = 0; // 0 - positive, 1 - negative
				if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"BYTE") quant = 1;
				else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"WORD") quant = 2;
				else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"DWORD") quant = 4;
				else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"QWORD") quant = 8;
				else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
				_pos++;
				if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"ALIGN") {
					_pos++;
					while (_output.data.Length() % quant) _output.data << 0x00;
				} else {
					if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"-") { _pos++; sign = !sign; }
					if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"+") { _pos++; }
					if (_text[_pos].Class == TokenClass::Constant) {
						if (_text[_pos].ValueClass == TokenConstantClass::String) {
							if (quant == 8) throw CompilerException(CompilerStatus::DataSizeNotSupported, _pos);
							Encoding enc;
							if (quant == 1) enc = Encoding::UTF8;
							else if (quant == 2) enc = Encoding::UTF16;
							else if (quant == 4) enc = Encoding::UTF32;
							SafePointer<DataBlock> data = _text[_pos].Content.EncodeSequence(enc, true);
							_output.data << *data;
						} else if (_text[_pos].ValueClass == TokenConstantClass::Numeric) {
							for (int i = 0; i < quant; i++) _output.data << 0x00;
							if (_text[_pos].NumericClass() == NumericTokenClass::Integer) {
								auto num = _text[_pos].AsInteger();
								if (sign) num = -num;
								if (quant == 1) *reinterpret_cast<uint8 *>(_output.data.GetBuffer() + _output.data.Length() - quant) = num;
								else if (quant == 2) *reinterpret_cast<uint16 *>(_output.data.GetBuffer() + _output.data.Length() - quant) = num;
								else if (quant == 4) *reinterpret_cast<uint32 *>(_output.data.GetBuffer() + _output.data.Length() - quant) = num;
								else if (quant == 8) *reinterpret_cast<uint64 *>(_output.data.GetBuffer() + _output.data.Length() - quant) = num;
							} else {
								if (quant == 1 || quant == 2) throw CompilerException(CompilerStatus::DataSizeNotSupported, _pos);
								auto num = _text[_pos].AsDouble();
								if (quant == 4) *reinterpret_cast<float *>(_output.data.GetBuffer() + _output.data.Length() - quant) = num;
								else if (quant == 8) *reinterpret_cast<double *>(_output.data.GetBuffer() + _output.data.Length() - quant) = num;
							}
						} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
					} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
					_pos++;
				}
			}
			void ProcessDataBlock(void)
			{
				while (_text[_pos].Class != TokenClass::CharCombo || _text[_pos].Content != L"}") ProcessDataAtom();
				_pos++;
			}
			void ProcessExternalAtom(void)
			{
				if (_text[_pos].Class == TokenClass::Constant && _text[_pos].ValueClass == TokenConstantClass::String) {
					_output.extrefs << _text[_pos].Content;
					_pos++;
				} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
			}
			void ProcessExternalBlock(void)
			{
				while (_text[_pos].Class != TokenClass::CharCombo || _text[_pos].Content != L"}") ProcessExternalAtom();
				_pos++;
			}
			void ProcessSize(ObjectSize & size, bool linear_only = false)
			{
				int sign = 0;
				if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"-") { _pos++; sign = !sign; }
				if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"+") { _pos++; }
				if (_text[_pos].Class == TokenClass::Constant && _text[_pos].ValueClass == TokenConstantClass::Numeric && _text[_pos].NumericClass() == NumericTokenClass::Integer) {
					size.num_bytes = _text[_pos].AsInteger();
					size.num_words = 0;
					if (sign) size.num_bytes = -int(size.num_bytes);
					_pos++;
					if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"W" && !linear_only) {
						ObjectSize si;
						ProcessSize(si);
						size.num_bytes += si.num_bytes;
						size.num_words += si.num_words;
					}
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"W" && !linear_only) {
					_pos++;
					ObjectSize si;
					ProcessSize(si, true);
					size.num_bytes = 0;
					size.num_words = si.num_bytes;
					if (sign) size.num_words = -int(size.num_words);
				} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
			}
			void ProcessSpecification(ArgumentSpecification & spec)
			{
				ProcessSize(spec.size);
				if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L":") {
					_pos++;
					if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"?") spec.semantics = ArgumentSemantics::Unknown;
					else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"-") spec.semantics = ArgumentSemantics::Unclassified;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"INT") spec.semantics = ArgumentSemantics::SignedInteger;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"SINT") spec.semantics = ArgumentSemantics::SignedInteger;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"UINT") spec.semantics = ArgumentSemantics::Integer;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"FLOAT") spec.semantics = ArgumentSemantics::FloatingPoint;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"OBJECT") spec.semantics = ArgumentSemantics::Object;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"THIS") spec.semantics = ArgumentSemantics::This;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"RTTI") spec.semantics = ArgumentSemantics::RTTI;
					else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"ERROR") spec.semantics = ArgumentSemantics::ErrorData;
					else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
					_pos++;
				} else spec.semantics = spec.size.num_bytes || spec.size.num_words ? ArgumentSemantics::Unclassified : ArgumentSemantics::Unknown;
			}
			void ProcessInterface(Array<ArgumentSpecification> & inputs, ArgumentSpecification & output)
			{
				while (_text[_pos].Class != TokenClass::CharCombo || _text[_pos].Content != L"=>") {
					ArgumentSpecification spec;
					ProcessSpecification(spec);
					inputs << spec;
				}
				_pos++;
				ProcessSpecification(output);
			}
			bool ProcessStandardTransform(ObjectReference & ref)
			{
				auto & i = _text[_pos].Content;
				// General purpose
				if (i == L"PTR_FOLLOW") ref.index = TransformFollowPointer;
				else if (i == L"PTR_TAKE") ref.index = TransformTakePointer;
				else if (i == L"OFFSET") ref.index = TransformAddressOffset;
				else if (i == L"BLT") ref.index = TransformBlockTransfer;
				else if (i == L"CALL") ref.index = TransformInvoke;
				else if (i == L"NEW") ref.index = TransformTemporary;
				else if (i == L"BREAKIF") ref.index = TransformBreakIf;
				else if (i == L"SPLIT") ref.index = TransformSplit;
				// Logical
				else if (i == L"ALL") ref.index = TransformLogicalAnd;
				else if (i == L"ANY") ref.index = TransformLogicalOr;
				else if (i == L"FORK") ref.index = TransformLogicalFork;
				else if (i == L"NOT") ref.index = TransformLogicalNot;
				else if (i == L"SAME") ref.index = TransformLogicalSame;
				else if (i == L"NOTSAME") ref.index = TransformLogicalNotSame;
				// Vector
				else if (i == L"AND") ref.index = TransformVectorAnd;
				else if (i == L"OR") ref.index = TransformVectorOr;
				else if (i == L"XOR") ref.index = TransformVectorXor;
				else if (i == L"INVERSE") ref.index = TransformVectorInverse;
				else if (i == L"SHL") ref.index = TransformVectorShiftL;
				else if (i == L"SHR") ref.index = TransformVectorShiftR;
				else if (i == L"SAL") ref.index = TransformVectorShiftAL;
				else if (i == L"SAR") ref.index = TransformVectorShiftAR;
				else if (i == L"ZERO") ref.index = TransformVectorIsZero;
				else if (i == L"NOTZERO") ref.index = TransformVectorNotZero;
				// Arithmetics - comparison
				else if (i == L"EQ") ref.index = TransformIntegerEQ;
				else if (i == L"NEQ") ref.index = TransformIntegerNEQ;
				else if (i == L"U_LE") ref.index = TransformIntegerULE;
				else if (i == L"U_GE") ref.index = TransformIntegerUGE;
				else if (i == L"U_L") ref.index = TransformIntegerUL;
				else if (i == L"U_G") ref.index = TransformIntegerUG;
				else if (i == L"S_LE") ref.index = TransformIntegerSLE;
				else if (i == L"S_GE") ref.index = TransformIntegerSGE;
				else if (i == L"S_L") ref.index = TransformIntegerSL;
				else if (i == L"S_G") ref.index = TransformIntegerSG;
				// Arithmetics - transforms
				else if (i == L"U_RESIZE") ref.index = TransformIntegerUResize;
				else if (i == L"S_RESIZE") ref.index = TransformIntegerSResize;
				else if (i == L"NEG") ref.index = TransformIntegerInverse;
				else if (i == L"ABS") ref.index = TransformIntegerAbs;
				// Arithmetics - main
				else if (i == L"ADD") ref.index = TransformIntegerAdd;
				else if (i == L"SUB") ref.index = TransformIntegerSubt;
				else if (i == L"U_MUL") ref.index = TransformIntegerUMul;
				else if (i == L"S_MUL") ref.index = TransformIntegerSMul;
				else if (i == L"U_DIV") ref.index = TransformIntegerUDiv;
				else if (i == L"S_DIV") ref.index = TransformIntegerSDiv;
				else if (i == L"U_MOD") ref.index = TransformIntegerUMod;
				else if (i == L"S_MOD") ref.index = TransformIntegerSMod;
				// Else there is a failure
				else return false;
				return true;
			}
			bool ProcessFloatingTransform(ObjectReference & ref)
			{
				auto & i = _text[_pos].Content;
				// FPU: Manipulation
				if (i == L"FP_RESIZE_16") { ref.index = TransformFloatResize; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_RESIZE_32") { ref.index = TransformFloatResize; }
				else if (i == L"FP_RESIZE_64") { ref.index = TransformFloatResize; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_GATHER_16") { ref.index = TransformFloatGather; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_GATHER_32") { ref.index = TransformFloatGather; }
				else if (i == L"FP_GATHER_64") { ref.index = TransformFloatGather; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_SCATTER_16") { ref.index = TransformFloatScatter; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_SCATTER_32") { ref.index = TransformFloatScatter; }
				else if (i == L"FP_SCATTER_64") { ref.index = TransformFloatScatter; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_REORDER_16") { ref.index = TransformFloatRecombine; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_REORDER_32") { ref.index = TransformFloatRecombine; }
				else if (i == L"FP_REORDER_64") { ref.index = TransformFloatRecombine; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_INTEGER_16") { ref.index = TransformFloatInteger; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_INTEGER_32") { ref.index = TransformFloatInteger; }
				else if (i == L"FP_INTEGER_64") { ref.index = TransformFloatInteger; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_RND_N_16") { ref.index = TransformFloatRoundTN; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_RND_N_32") { ref.index = TransformFloatRoundTN; }
				else if (i == L"FP_RND_N_64") { ref.index = TransformFloatRoundTN; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_RND_Z_16") { ref.index = TransformFloatRoundTZ; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_RND_Z_32") { ref.index = TransformFloatRoundTZ; }
				else if (i == L"FP_RND_Z_64") { ref.index = TransformFloatRoundTZ; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_RND_PI_16") { ref.index = TransformFloatRoundTPI; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_RND_PI_32") { ref.index = TransformFloatRoundTPI; }
				else if (i == L"FP_RND_PI_64") { ref.index = TransformFloatRoundTPI; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_RND_NI_16") { ref.index = TransformFloatRoundTNI; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_RND_NI_32") { ref.index = TransformFloatRoundTNI; }
				else if (i == L"FP_RND_NI_64") { ref.index = TransformFloatRoundTNI; ref.ref_flags |= ReferenceFlagLong; }
				// FPU: Comparison
				else if (i == L"FP_ZERO_16") { ref.index = TransformFloatIsZero; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_ZERO_32") { ref.index = TransformFloatIsZero; }
				else if (i == L"FP_ZERO_64") { ref.index = TransformFloatIsZero; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_NOTZERO_16") { ref.index = TransformFloatNotZero; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_NOTZERO_32") { ref.index = TransformFloatNotZero; }
				else if (i == L"FP_NOTZERO_64") { ref.index = TransformFloatNotZero; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_EQ_16") { ref.index = TransformFloatEQ; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_EQ_32") { ref.index = TransformFloatEQ; }
				else if (i == L"FP_EQ_64") { ref.index = TransformFloatEQ; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_NEQ_16") { ref.index = TransformFloatNEQ; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_NEQ_32") { ref.index = TransformFloatNEQ; }
				else if (i == L"FP_NEQ_64") { ref.index = TransformFloatNEQ; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_LE_16") { ref.index = TransformFloatLE; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_LE_32") { ref.index = TransformFloatLE; }
				else if (i == L"FP_LE_64") { ref.index = TransformFloatLE; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_GE_16") { ref.index = TransformFloatGE; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_GE_32") { ref.index = TransformFloatGE; }
				else if (i == L"FP_GE_64") { ref.index = TransformFloatGE; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_L_16") { ref.index = TransformFloatL; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_L_32") { ref.index = TransformFloatL; }
				else if (i == L"FP_L_64") { ref.index = TransformFloatL; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_G_16") { ref.index = TransformFloatG; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_G_32") { ref.index = TransformFloatG; }
				else if (i == L"FP_G_64") { ref.index = TransformFloatG; ref.ref_flags |= ReferenceFlagLong; }
				// FPU: Arithmetics
				else if (i == L"FP_ADD_16") { ref.index = TransformFloatAdd; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_ADD_32") { ref.index = TransformFloatAdd; }
				else if (i == L"FP_ADD_64") { ref.index = TransformFloatAdd; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_SUB_16") { ref.index = TransformFloatSubt; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_SUB_32") { ref.index = TransformFloatSubt; }
				else if (i == L"FP_SUB_64") { ref.index = TransformFloatSubt; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_MUL_16") { ref.index = TransformFloatMul; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_MUL_32") { ref.index = TransformFloatMul; }
				else if (i == L"FP_MUL_64") { ref.index = TransformFloatMul; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_MADD_16") { ref.index = TransformFloatMulAdd; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_MADD_32") { ref.index = TransformFloatMulAdd; }
				else if (i == L"FP_MADD_64") { ref.index = TransformFloatMulAdd; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_MSUB_16") { ref.index = TransformFloatMulSubt; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_MSUB_32") { ref.index = TransformFloatMulSubt; }
				else if (i == L"FP_MSUB_64") { ref.index = TransformFloatMulSubt; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_DIV_16") { ref.index = TransformFloatDiv; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_DIV_32") { ref.index = TransformFloatDiv; }
				else if (i == L"FP_DIV_64") { ref.index = TransformFloatDiv; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_ABS_16") { ref.index = TransformFloatAbs; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_ABS_32") { ref.index = TransformFloatAbs; }
				else if (i == L"FP_ABS_64") { ref.index = TransformFloatAbs; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_NEG_16") { ref.index = TransformFloatInverse; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_NEG_32") { ref.index = TransformFloatInverse; }
				else if (i == L"FP_NEG_64") { ref.index = TransformFloatInverse; ref.ref_flags |= ReferenceFlagLong; }
				else if (i == L"FP_SQRT_16") { ref.index = TransformFloatSqrt; ref.ref_flags |= ReferenceFlagShort; }
				else if (i == L"FP_SQRT_32") { ref.index = TransformFloatSqrt; }
				else if (i == L"FP_SQRT_64") { ref.index = TransformFloatSqrt; ref.ref_flags |= ReferenceFlagLong; }
				// Else there is a failure
				else return false;
				return true;
			}
			void ProcessReference(ObjectReference & ref, bool allow_literals, bool allow_intrinsic)
			{
				if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"NULL") {
					ref.ref_class = ReferenceNull;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"E") {
					ref.ref_class = ReferenceExternal;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"D") {
					ref.ref_class = ReferenceData;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"C") {
					ref.ref_class = ReferenceCode;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"A") {
					ref.ref_class = ReferenceArgument;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"R") {
					ref.ref_class = ReferenceRetVal;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"L") {
					ref.ref_class = ReferenceLocal;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"I") {
					ref.ref_class = ReferenceInit;
				} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"S") {
					ref.ref_class = ReferenceSplitter;
				} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"@" && allow_intrinsic) {
					ref.ref_class = ReferenceTransform;
				} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"-" && allow_literals) {
					ref.ref_class = ReferenceLiteral;
				} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
				_pos++;
				if (ref.ref_class == ReferenceTransform) {
					if (_text[_pos].Class != TokenClass::Identifier) throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
					if (!ProcessStandardTransform(ref) && !ProcessFloatingTransform(ref)) throw CompilerException(CompilerStatus::UnknownInrinsic, _pos);
					_pos++;
				} else {
					if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"[") {
						_pos++;
						if (_text[_pos].Class != TokenClass::Constant || _text[_pos].ValueClass != TokenConstantClass::Numeric || _text[_pos].NumericClass() != NumericTokenClass::Integer) {
							throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
						}
						ref.index = _text[_pos].AsInteger();
						_pos++;
						if (_text[_pos].Class != TokenClass::CharCombo || _text[_pos].Content != L"]") throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
						_pos++;
					}
				}
			}
			void ProcessFinalizer(FinalizerReference & ref)
			{
				ProcessReference(ref.final, false, false);
				if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"(") {
					_pos++;
					while (true) {
						ObjectReference subref;
						ProcessReference(subref, false, false);
						ref.final_args << subref;
						if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L")") {
							_pos++;
							break;
						} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L",") {
							_pos++;
						} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
					}
				}
			}
			void ProcessTree(ExpressionTree & tree)
			{
				int spos = _pos;
				ProcessReference(tree.self, true, true);
				while (true) {
					if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"(") {
						tree.self.ref_flags |= ReferenceFlagInvoke;
						_pos++;
						if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L")") {
							_pos++;
						} else {
							while (true) {
								ExpressionTree subtree = TH::MakeTree();
								ProcessTree(subtree);
								tree.inputs << subtree;
								if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L")") {
									_pos++;
									break;
								} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L",") {
									_pos++;
								} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
							}
						}
					} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"#") {
						_pos++;
						ProcessFinalizer(tree.retval_final);
					} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L":") {
						_pos++;
						ProcessInterface(tree.input_specs, tree.retval_spec);
					} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"!") {
						_pos++;
						tree.self.ref_flags |= ReferenceFlagUnaligned;
					} else break;
				}
				if (tree.inputs.Length() != tree.input_specs.Length()) {
					throw CompilerException(CompilerStatus::CallInputLengthMismatch, spos);
				}
			}
			void ProcessStatement(void)
			{
				Statement statement;
				statement.attachment = TH::MakeSize();
				statement.attachment_final = TH::MakeFinal();
				statement.tree = TH::MakeTree();
				if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"NOP") {
					_pos++;
					statement.opcode = OpcodeNOP;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"TRAP") {
					_pos++;
					statement.opcode = OpcodeTrap;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"ENTER") {
					_pos++;
					statement.opcode = OpcodeOpenScope;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"LEAVE") {
					_pos++;
					statement.opcode = OpcodeCloseScope;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"EVAL") {
					_pos++;
					statement.opcode = OpcodeExpression;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"NEW") {
					_pos++;
					statement.opcode = OpcodeNewLocal;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"JUMP") {
					_pos++;
					statement.opcode = OpcodeUnconditionalJump;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"JUMPIF") {
					_pos++;
					statement.opcode = OpcodeConditionalJump;
				} else if (_text[_pos].Class == TokenClass::Identifier && _text[_pos].Content == L"RET") {
					_pos++;
					statement.opcode = OpcodeControlReturn;
				} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
				while (true) {
					if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"{") {
						_pos++;
						ProcessTree(statement.tree);
						if (_text[_pos].Class != TokenClass::CharCombo || _text[_pos].Content != L"}") throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
						_pos++;
					} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"@") {
						_pos++;
						ProcessSize(statement.attachment);
					} else if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"#") {
						_pos++;
						ProcessFinalizer(statement.attachment_final);
					} else break;
				}
				_output.instset << statement;
			}
			void ProcessCodeBlock(void)
			{
				while (_text[_pos].Class != TokenClass::CharCombo || _text[_pos].Content != L"}") ProcessStatement();
				_pos++;
			}
			void Process(void)
			{
				while (_pos < _text.Length()) {
					if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"DATA") {
						_pos++;
						if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"{") {
							_pos++;
							ProcessDataBlock();
						} else ProcessDataAtom();
					} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"EXTERNAL") {
						_pos++;
						if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"{") {
							_pos++;
							ProcessExternalBlock();
						} else ProcessExternalAtom();
					} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"INTERFACE") {
						_pos++;
						ProcessInterface(_output.inputs, _output.retval);
					} else if (_text[_pos].Class == TokenClass::Keyword && _text[_pos].Content == L"CODE") {
						_pos++;
						if (_text[_pos].Class == TokenClass::CharCombo && _text[_pos].Content == L"{") {
							_pos++;
							ProcessCodeBlock();
						} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
					} else if (_text[_pos].Class == TokenClass::EndOfStream) {
						break;
					} else throw CompilerException(CompilerStatus::AnotherTokenExpected, _pos);
				}
			}
		};
		void CompileFunction(const string & input, Function & output, CompilerStatusDesc & result)
		{
			SafePointer< Array<Token> > text;
			try {
				output.Clear();
				Spelling spelling;
				spelling.Keywords << L"NULL";
				spelling.Keywords << L"E";
				spelling.Keywords << L"D";
				spelling.Keywords << L"C";
				spelling.Keywords << L"A";
				spelling.Keywords << L"R";
				spelling.Keywords << L"L";
				spelling.Keywords << L"I";
				spelling.Keywords << L"S";
				spelling.Keywords << L"W";
				spelling.Keywords << L"EXTERNAL";
				spelling.Keywords << L"INTERFACE";
				spelling.Keywords << L"DATA";
				spelling.Keywords << L"CODE";
				spelling.Keywords << L"BYTE";
				spelling.Keywords << L"WORD";
				spelling.Keywords << L"DWORD";
				spelling.Keywords << L"QWORD";
				spelling.Keywords << L"ALIGN";
				spelling.IsolatedChars << L'{';
				spelling.IsolatedChars << L'}';
				spelling.IsolatedChars << L'(';
				spelling.IsolatedChars << L')';
				spelling.IsolatedChars << L'[';
				spelling.IsolatedChars << L']';
				spelling.IsolatedChars << L'-';
				spelling.IsolatedChars << L'+';
				spelling.IsolatedChars << L':';
				spelling.IsolatedChars << L'?';
				spelling.IsolatedChars << L'@';
				spelling.IsolatedChars << L'#';
				spelling.IsolatedChars << L',';
				spelling.IsolatedChars << L'!';
				spelling.CombinableChars << L'=';
				spelling.CombinableChars << L'>';
				spelling.InfinityLiteral = L"INFINITY";
				spelling.InfinityLiteral = L"NAN";
				spelling.CommentEndOfLineWord = L";";
				spelling.AllowNonLatinNames = false;
				text = ParseText(input, spelling);
				CompilerContext ctx(input, *text, output);
				ctx.Process();
				result.status = CompilerStatus::Success;
				result.error_line_no = result.error_line_pos = result.error_line_len = -1;
			} catch (CompilerException & e) {
				result.status = e.reason;
				MakeErrorInfo(input, result, text->ElementAt(e.at).SourcePosition,
					e.at < text->Length() - 1 ? text->ElementAt(e.at + 1).SourcePosition : text->ElementAt(e.at).SourcePosition + 1);
			} catch (ParserSpellingException & e) {
				result.status = CompilerStatus::InvalidToken;
				MakeErrorInfo(input, result, e.Position, e.Position + 1);
			} catch (...) {
				result.status = CompilerStatus::InternalError;
				result.error_line_no = result.error_line_pos = result.error_line_len = -1;
			}
		}
	}
}