#include "xv_lexical.h"

#include "xv_meta.h"

namespace Engine
{
	namespace XV
	{
		bool IsIsolatedPunctuation(uint32 ucs)
		{
			return ucs == L'@' || ucs == L'^' || ucs == L'(' || ucs == L')' || ucs == L'[' || ucs == L']' ||
				ucs == L'{' || ucs == L'}' || ucs == L';' || ucs == L':' || ucs == L'~' || ucs == L'.' || ucs == L',' ||
				ucs == L'?';
		}
		bool IsContinualPunctuation(uint32 ucs)
		{
			return ucs == L'#' || ucs == L'%' || ucs == L'&' || ucs == L'*' || ucs == L'+' || ucs == L'-' || ucs == L'=' ||
				ucs == L'|' || ucs == L'/' || ucs == L'<' || ucs == L'>' || ucs == L'!';
		}
		bool IsValidPunctuation(uint32 ucs) { return IsIsolatedPunctuation(ucs) || IsContinualPunctuation(ucs); }
		bool IsReservedPunctuation(uint32 ucs)
		{
			return ucs == L'$' || ucs == L'\'' || ucs == L'\"' || ucs == L'\\' || IsValidPunctuation(ucs);
		}
		bool IsWhitespace(uint32 ucs)
		{
			return ucs == L' ' || ucs == L'\t' || ucs == L'\n' || ucs == L'\r';
		}
		bool IsKeyword(const string & str)
		{
			if (str == Lexic::KeywordNamespace) return true;
			if (str == Lexic::KeywordClass) return true;
			if (str == Lexic::KeywordInterface) return true;
			if (str == Lexic::KeywordAlias) return true;
			if (str == Lexic::KeywordFunction) return true;
			if (str == Lexic::KeywordOperator) return true;
			if (str == Lexic::KeywordEntry) return true;
			if (str == Lexic::KeywordThrows) return true;
			if (str == Lexic::KeywordThrow) return true;
			if (str == Lexic::KeywordSizeOf) return true;
			if (str == Lexic::KeywordSizeOfMX) return true;
			if (str == Lexic::KeywordAlignOf) return true;
			if (str == Lexic::KeywordAlignOfMX) return true;
			if (str == Lexic::KeywordModule) return true;
			if (str == Lexic::KeywordArray) return true;
			if (str == Lexic::KeywordConst) return true;
			if (str == Lexic::KeywordUse) return true;
			if (str == Lexic::KeywordVariable) return true;
			if (str == Lexic::KeywordImport) return true;
			if (str == Lexic::KeywordResource) return true;
			if (str == Lexic::KeywordTry) return true;
			if (str == Lexic::KeywordCatch) return true;
			if (str == Lexic::KeywordClassFunc) return true;
			if (str == Lexic::KeywordCtor) return true;
			if (str == Lexic::KeywordDtor) return true;
			if (str == Lexic::KeywordConvertor) return true;
			if (str == Lexic::KeywordVirtual) return true;
			if (str == Lexic::KeywordPure) return true;
			if (str == Lexic::KeywordContinue) return true;
			if (str == Lexic::KeywordReturn) return true;
			if (str == Lexic::KeywordIf) return true;
			if (str == Lexic::KeywordElse) return true;
			if (str == Lexic::KeywordWhile) return true;
			if (str == Lexic::KeywordDo) return true;
			if (str == Lexic::KeywordFor) return true;
			if (str == Lexic::KeywordBreak) return true;
			if (str == Lexic::KeywordNull) return true;
			if (str == Lexic::KeywordOverride) return true;
			if (str == Lexic::KeywordInherit) return true;
			if (str == Lexic::KeywordInit) return true;
			if (str == Lexic::KeywordStructure) return true;
			if (str == Lexic::KeywordPrototype) return true;
			if (str == Lexic::KeywordNew) return true;
			if (str == Lexic::KeywordDelete) return true;
			if (str == Lexic::KeywordConstruct) return true;
			if (str == Lexic::KeywordDestruct) return true;
			if (str == Lexic::KeywordEnum) return true;
			if (str == Lexic::KeywordAs) return true;
			if (str == Lexic::KeywordTrap) return true;
			if (str == Lexic::KeywordBLT) return true;
			return false;
		}
		bool IsLiteralTrue(const string & str) { return str == Lexic::LiteralYes; }
		bool IsLiteralFalse(const string & str) { return str == Lexic::LiteralNo; }

		void DeserializeDataLiteral(const DataBlock & data, int & current, void * into, int length)
		{
			for (int i = 0; i < length; i++) reinterpret_cast<uint8 *>(into)[i] = data[current + i];
			current += length;
		}
		string DeserializeStringLiteral(const DataBlock & data, int & current, uint8 hdr)
		{
			auto lc = hdr & 0xC0;
			int length;
			if (lc == 0x00) {
				length = data[current];
				current++;
			} else if (lc == 0x40) {
				length = data[current];
				length |= uint(data[current + 1]) << 8;
				current += 2;
			} else if (lc == 0x80) {
				length = data[current];
				length |= uint(data[current + 1]) << 8;
				length |= uint(data[current + 2]) << 16;
				length |= uint(data[current + 3]) << 24;
				current += 4;
			} else length = 0;
			auto result = string(data.GetBuffer() + current, length, Encoding::UTF8);
			current += length;
			return result;
		}
		void SerializeDataLiteral(DataBlock & out, const void * data, int length)
		{
			for (int i = 0; i < length; i++) out << reinterpret_cast<const uint8 *>(data)[i];
		}
		void SerializeStringLiteral(DataBlock & data, uint8 hdr, const string & text)
		{
			SafePointer<DataBlock> utf8 = text.EncodeSequence(Encoding::UTF8, false);
			uint length = utf8->Length();
			if (length < 0x100) {
				data << hdr;
				data << length;
				data << *utf8;
			} else if (length < 0x10000) {
				data << (hdr | 0x40);
				data << (length & 0xFF);
				data << ((length >> 8) & 0xFF);
				data << *utf8;
			} else {
				data << (hdr | 0x80);
				data << (length & 0xFF);
				data << ((length >> 8) & 0xFF);
				data << ((length >> 16) & 0xFF);
				data << ((length >> 24) & 0xFF);
				data << *utf8;
			}
		}

		bool ReadEscapeCode(const uint32 * data, int & current, int length, int max_len, const string & allowed, string & result)
		{
			result = "";
			int n = 0;
			while (n < max_len && current < length) {
				bool found = false;
				for (int i = 0; i < allowed.Length(); i++) if (data[current] == allowed[i]) { found = true; break; }
				if (!found) return true;
				result += string(data + current, 1, Encoding::UTF32);
				current++;
				n++;
			}
			return true;
		}
		bool ReadCharacter(const uint32 * data, int & current, int length, uint32 del, uint32 & result)
		{
			if (current >= length) return false;
			if (data[current] == L'\\') {
				current++;
				if (current >= length) return false;
				auto escc = data[current];
				if (escc == L'\\' || escc == L'\'' || escc == L'\"' || escc == L'?' || escc == L'/') {
					result = escc; current++;
				} else if (escc == L'a' || escc == L'A') {
					result = L'\a'; current++;
				} else if (escc == L'b' || escc == L'B') {
					result = L'\b'; current++;
				} else if (escc == L'e' || escc == L'E') {
					result = L'\e'; current++;
				} else if (escc == L'f' || escc == L'F') {
					result = L'\f'; current++;
				} else if (escc == L'n' || escc == L'N') {
					result = L'\n'; current++;
				} else if (escc == L'r' || escc == L'R') {
					result = L'\r'; current++;
				} else if (escc == L't' || escc == L'T') {
					result = L'\t'; current++;
				} else if (escc == L'v' || escc == L'V') {
					result = L'\v'; current++;
				} else if (escc == L'x' || escc == L'X' || escc == L'U') {
					current++;
					string code;
					if (!ReadEscapeCode(data, current, length, 8, L"0123456789ABCDEFabcdef", code)) return false;
					result = code.ToUInt32(HexadecimalBase);
				} else if (escc == L'u') {
					current++;
					string code;
					if (!ReadEscapeCode(data, current, length, 4, L"0123456789ABCDEFabcdef", code)) return false;
					result = code.ToUInt32(HexadecimalBase);
				} else if (escc >= L'0' && escc <= L'7') {
					string code;
					if (!ReadEscapeCode(data, current, length, 3, L"01234567", code)) return false;
					result = code.ToUInt32(OctalBase);
				} else return false;
			} else {
				if (data[current] == del || data[current] < 32) return false;
				result = data[current];
				current++;
			}
			return true;
		}

		TokenStream::TokenStream(const uint32 * data, int length, CodeMetaInfo * meta, int override_base) : _meta(meta), _data(data), _override_base(override_base), _length(length), _current(0) {}
		TokenStream::~TokenStream(void) {}
		bool TokenStream::ReadToken(Token & dest)
		{
			begin_read_token:
			while (_current < _length && IsWhitespace(_data[_current])) _current++;
			if (_current == _length || _data[_current] == 0) {
				dest.type = TokenType::EOF;
				dest.range_from = _override_base + _current;
				dest.range_length = dest.ex_data = 0;
				dest.contents = L"";
				dest.contents_i = 0;
				dest.contents_f = 0.0;
			} else {
				if (_data[_current] >= L'0' && _data[_current] <= L'9') {
					if (_current < _length - 1 && (_data[_current + 1] == L'x' || _data[_current + 1] == L'X')) {
						_current += 2;
						int from = _current;
						while (_current < _length && ((_data[_current] >= L'0' && _data[_current] <= L'9') ||
							(_data[_current] >= L'a' && _data[_current] <= L'f') ||
							(_data[_current] >= L'A' && _data[_current] <= L'F'))) _current++;
						try {
							dest.type = TokenType::Literal;
							dest.range_from = _override_base + from - 2;
							dest.range_length = _current - dest.range_from + _override_base;
							dest.ex_data = TokenLiteralInteger;
							dest.contents = L"";
							dest.contents_i = string(_data + from, _current - from, Encoding::UTF32).ToUInt64(HexadecimalBase);
							dest.contents_f = 0.0;
						} catch (...) { _current = from - 2; return false; }
					} else if (_current < _length - 1 && (_data[_current + 1] == L'o' || _data[_current + 1] == L'O')) {
						_current += 2;
						int from = _current;
						while (_current < _length && (_data[_current] >= L'0' && _data[_current] <= L'7')) _current++;
						try {
							dest.type = TokenType::Literal;
							dest.range_from = _override_base + from - 2;
							dest.range_length = _current - dest.range_from + _override_base;
							dest.ex_data = TokenLiteralInteger;
							dest.contents = L"";
							dest.contents_i = string(_data + from, _current - from, Encoding::UTF32).ToUInt64(OctalBase);
							dest.contents_f = 0.0;
						} catch (...) { _current = from - 2; return false; }
					} else if (_current < _length - 1 && (_data[_current + 1] == L'b' || _data[_current + 1] == L'B')) {
						_current += 2;
						int from = _current;
						while (_current < _length && (_data[_current] >= L'0' && _data[_current] <= L'1')) _current++;
						try {
							dest.type = TokenType::Literal;
							dest.range_from = _override_base + from - 2;
							dest.range_length = _current - dest.range_from + _override_base;
							dest.ex_data = TokenLiteralInteger;
							dest.contents = L"";
							dest.contents_i = string(_data + from, _current - from, Encoding::UTF32).ToUInt64(BinaryBase);
							dest.contents_f = 0.0;
						} catch (...) { _current = from - 2; return false; }
					} else {
						int from = _current;
						while (_current < _length && ((_data[_current] >= L'0' && _data[_current] <= L'9') ||
							(_data[_current] == L'e') || (_data[_current] == L'E') || (_data[_current] == L'.') ||
							(_data[_current] == L'+' && _data[_current - 1] == L'E') ||
							(_data[_current] == L'+' && _data[_current - 1] == L'e') ||
							(_data[_current] == L'-' && _data[_current - 1] == L'E') ||
							(_data[_current] == L'-' && _data[_current - 1] == L'e'))) _current++;
						dest.type = TokenType::Literal;
						dest.range_from = _override_base + from;
						dest.range_length = _current - dest.range_from + _override_base;
						dest.contents = L"";
						auto contents = string(_data + from, _current - from, Encoding::UTF32);
						if (contents.FindFirst(L'.') >= 0 || contents.FindFirst(L'e') >= 0 || contents.FindFirst(L'E') >= 0) {
							dest.ex_data = TokenLiteralFloat;
							dest.contents_i = 0;
							auto e_first = contents.FindFirst(L'e');
							auto e_last = contents.FindLast(L'e');
							auto E_first = contents.FindFirst(L'E');
							auto E_last = contents.FindLast(L'E');
							int e;
							if (e_first == e_last && e_first >= 0 && E_first < 0) e = e_first;
							else if (E_first == E_last && E_first >= 0 && e_first < 0) e = E_first;
							else if (E_first < 0 && e_first < 0) e = -1;
							else { _current = from; return false; }
							auto contents_f = contents.Fragment(0, e);
							auto contents_e = e >= 0 ? contents.Fragment(e + 1, -1) : L"0";
							try {
								auto frac = contents_f.ToDouble();
								auto exp = contents_e.ToInt32();
								while (exp > 0) { frac *= 10.0; exp--; }
								while (exp < 0) { frac /= 10.0; exp++; }
								dest.contents_f = frac;
							} catch (...) { _current = from; return false; }
						} else {
							try {
								dest.ex_data = TokenLiteralInteger;
								dest.contents_i = contents.ToUInt64();
								dest.contents_f = 0.0;
							} catch (...) { _current = from; return false; }
						}
					}
				} else if (_data[_current] == L'\'') {
					int from = _current;
					_current++;
					uint32 code;
					if (!ReadCharacter(_data, _current, _length, L'\'', code)) return false;
					if (_current >= _length || _data[_current] != L'\'') return false;
					_current++;
					dest.type = TokenType::Literal;
					dest.range_from = _override_base + from;
					dest.range_length = _current - from;
					dest.ex_data = TokenLiteralInteger;
					dest.contents = L"";
					dest.contents_i = code;
					dest.contents_f = 0.0;
				} else if (_data[_current] == L'\"') {
					int from = _current;
					_current++;
					Array<uint32> buffer(0x100);
					while (true) {
						if (_current < _length) {
							if (_data[_current] == L'\"') break; else {
								uint32 code;
								if (!ReadCharacter(_data, _current, _length, L'\"', code)) return false;
								buffer << code;
							}
						} else return false;
					}
					_current++;
					dest.type = TokenType::Literal;
					dest.range_from = _override_base + from;
					dest.range_length = _current - from;
					dest.ex_data = TokenLiteralString;
					dest.contents = string(buffer.GetBuffer(), buffer.Length(), Encoding::UTF32);
					dest.contents_i = 0;
					dest.contents_f = 0.0;
				} else if (_current < _length - 1 && _data[_current] == L'/' && _data[_current + 1] == L'/') {
					int com_begin = _current;
					_current += 2;
					while (_current < _length && _data[_current] != L'\n') _current++;
					if (_meta) {
						CodeRangeInfo range;
						range.from = com_begin + _override_base;
						range.length = _current - com_begin;
						range.tag = CodeRangeTag::Comment;
						range.flags = 0;
						_meta->info.Append(range.from, range);
					}
					goto begin_read_token;
				} else if (_current < _length - 1 && _data[_current] == L'/' && _data[_current + 1] == L'*') {
					int com_begin = _current;
					_current += 2;
					while (_current < _length - 1 && (_data[_current] != L'*' || _data[_current + 1] != L'/')) _current++;
					_current += 2;
					if (_current > _length) _current = _length;
					if (_meta) {
						CodeRangeInfo range;
						range.from = com_begin + _override_base;
						range.length = _current - com_begin;
						range.tag = CodeRangeTag::Comment;
						range.flags = 0;
						_meta->info.Append(range.from, range);
					}
					goto begin_read_token;
				} else if (IsValidPunctuation(_data[_current])) {
					int from = _current;
					if (IsContinualPunctuation(_data[_current])) {
						while (_current < _length && IsContinualPunctuation(_data[_current])) _current++;
					} else _current++;
					dest.type = TokenType::Punctuation;
					dest.range_from = _override_base + from;
					dest.range_length = _current - from;
					dest.ex_data = 0;
					dest.contents = string(_data + from, dest.range_length, Encoding::UTF32);
					dest.contents_i = 0;
					dest.contents_f = 0.0;
				} else if (_data[_current] == L'$') {
					dest.range_from = _current;
					dest.range_length = 1;
					dest.ex_data = 0;
					dest.type = TokenType::PrototypeSymbol;
					dest.contents_i = 0;
					dest.contents_f = 0.0;
					_current++;
				} else if (_data[_current] == L'\\') {
					dest.range_from = _current;
					dest.range_length = 1;
					dest.ex_data = 0;
					dest.type = TokenType::PrototypeCommand;
					dest.contents_i = 0;
					dest.contents_f = 0.0;
					_current++;
				} else {
					int from = _current;
					while (_current < _length && !IsWhitespace(_data[_current]) && !IsReservedPunctuation(_data[_current])) _current++;
					dest.range_from = _override_base + from;
					dest.range_length = _current - from;
					dest.ex_data = 0;
					dest.contents = string(_data + from, dest.range_length, Encoding::UTF32);
					dest.contents_i = 0;
					dest.contents_f = 0.0;
					if (IsLiteralTrue(dest.contents)) {
						dest.type = TokenType::Literal;
						dest.ex_data = TokenLiteralLogicalY;
					} else if (IsLiteralFalse(dest.contents)) {
						dest.type = TokenType::Literal;
						dest.ex_data = TokenLiteralLogicalN;
					} else if (IsKeyword(dest.contents)) dest.type = TokenType::Keyword;
					else dest.type = TokenType::Identifier;
				}
			}
			if (_meta && dest.range_length > 0) {
				CodeRangeInfo range;
				range.from = dest.range_from;
				range.length = dest.range_length;
				range.tag = CodeRangeTag::NoData;
				if (dest.type == TokenType::Keyword) range.tag = CodeRangeTag::Keyword;
				else if (dest.type == TokenType::Punctuation) range.tag = CodeRangeTag::Punctuation;
				else if (dest.type == TokenType::PrototypeSymbol || dest.type == TokenType::PrototypeCommand) range.tag = CodeRangeTag::Prototype;
				else if (dest.type == TokenType::Identifier) range.tag = CodeRangeTag::IdentifierUnknown;
				else if (dest.type == TokenType::Literal) {
					if (dest.ex_data == TokenLiteralLogicalY || dest.ex_data == TokenLiteralLogicalN) range.tag = CodeRangeTag::LiteralBoolean;
					else if (dest.ex_data == TokenLiteralInteger || dest.ex_data == TokenLiteralFloat) range.tag = CodeRangeTag::LiteralNumeric;
					else if (dest.ex_data == TokenLiteralString) range.tag = CodeRangeTag::LiteralString;
				}
				if (range.tag != CodeRangeTag::NoData) {
					range.flags = 0;
					if (dest.type == TokenType::Punctuation && dest.contents == L"{") range.flags |= CodeRangeClauseOpen;
					if (dest.type == TokenType::Punctuation && dest.contents == L"}") range.flags |= CodeRangeClauseClose;
					_meta->info.Append(range.from, range);
				}
			}
			return true;
		}
		bool TokenStream::ReadBlock(ITokenStream ** deferred_stream)
		{
			*deferred_stream = 0;
			int from = _current, end;
			int level = 0;
			while (true) {
				end = _current;
				Token token;
				if (!ReadToken(token)) return false;
				if (token.type == TokenType::EOF) return false;
				else if (token.type == TokenType::Punctuation && token.contents == L"{") level++;
				else if (token.type == TokenType::Punctuation && token.contents == L"}") { level--; if (level < 0) break; }
			}
			*deferred_stream = new TokenStream(_data + from, end - from, 0, _override_base + from);
			return true;
		}
		void TokenStream::ExtractRange(int from, int length, string & line, int & line_no, int & line_from, int & line_length) const
		{
			line_no = 1;
			for (int i = 0; i < from; i++) if (_data[i] == L'\n') line_no++;
			if (from >= _length) from = _length - 1;
			while (from >= 0 && _data[from] == L'\n') from--;
			if (from < 0) {
				line = L"";
				line_from = line_length = 0;
				return;
			}
			int line_s = from, line_e = from;
			while (line_s && _data[line_s] != L'\n') line_s--;
			while (line_s < _length && (_data[line_s] == L'\n' || _data[line_s] == L'\r' || _data[line_s] == L'\t' || _data[line_s] == L' ')) line_s++;
			while (line_e < _length && _data[line_e] != L'\n' && _data[line_e] != L'\r') line_e++;
			if (from + length > _length) length = _length - from;
			line = string(_data + line_s, (line_e - line_s), Encoding::UTF32);
			if (from > line_s) line_from = string(_data + line_s, (from - line_s), Encoding::UTF32).Length();
			else line_from = 0;
			if (length > 0) line_length = string(_data + from, length, Encoding::UTF32).Length();
			else line_length = 0;
		}
		int TokenStream::GetCurrentPosition(void) const { return _override_base + _current; }
		string TokenStream::ExtractContents(void) const { return string(_data, _length, Encoding::UTF32); }
		DataBlock * TokenStream::Serialize(void)
		{
			SafePointer<DataBlock> result = new DataBlock(0x1000);
			_current = 0;
			while (true) {
				Token token;
				if (!ReadToken(token)) return 0;
				SerializeToken(*result, token);
				if (token.type == TokenType::EOF) break;
			}
			result->Retain();
			return result;
		}

		RestorationStream::RestorationStream(DataBlock * data, int offset) : _current(offset) { _data.SetRetain(data); }
		RestorationStream::~RestorationStream(void) {}
		bool RestorationStream::ReadToken(Token & dest)
		{
			uint8 hdr;
			if (_current < _data->Length()) { hdr = _data->ElementAt(_current); _current++; } else hdr = 0;
			auto code = hdr & 0x07;
			if (code == 0) {
				dest.range_from = dest.range_length = dest.ex_data = 0;
				dest.type = TokenType::EOF;
				dest.contents_i = 0;
				dest.contents_f = 0.0;
				return true;
			} else if (code == 1) {
				dest.range_from = dest.range_length = dest.ex_data = 0;
				dest.type = TokenType::Keyword;
				dest.contents = DeserializeStringLiteral(*_data, _current, hdr);
				dest.contents_i = 0;
				dest.contents_f = 0.0;
				return true;
			} else if (code == 2) {
				dest.range_from = dest.range_length = dest.ex_data = 0;
				dest.type = TokenType::Identifier;
				dest.contents = DeserializeStringLiteral(*_data, _current, hdr);
				dest.contents_i = 0;
				dest.contents_f = 0.0;
				return true;
			} else if (code == 3) {
				dest.range_from = dest.range_length = dest.ex_data = 0;
				dest.type = TokenType::Punctuation;
				dest.contents = DeserializeStringLiteral(*_data, _current, hdr);
				dest.contents_i = 0;
				dest.contents_f = 0.0;
				return true;
			} else if (code == 4) {
				dest.range_from = dest.range_length = 0;
				dest.type = TokenType::Literal;
				auto lsc = hdr & 0x38;
				if (lsc == 0x00) {
					dest.ex_data = TokenLiteralLogicalY;
					dest.contents = Lexic::LiteralYes;
					dest.contents_i = 0;
					dest.contents_f = 0.0;
				} else if (lsc == 0x08) {
					dest.ex_data = TokenLiteralLogicalN;
					dest.contents = Lexic::LiteralNo;
					dest.contents_i = 0;
					dest.contents_f = 0.0;
				} else if (lsc == 0x10) {
					dest.ex_data = TokenLiteralInteger;
					dest.contents_f = 0.0;
					DeserializeDataLiteral(*_data, _current, &dest.contents_i, 8);
				} else if (lsc == 0x18) {
					dest.ex_data = TokenLiteralFloat;
					dest.contents_i = 0;
					DeserializeDataLiteral(*_data, _current, &dest.contents_f, 8);
				} else if (lsc == 0x20) {
					dest.ex_data = TokenLiteralString;
					dest.contents = DeserializeStringLiteral(*_data, _current, hdr);
					dest.contents_i = 0;
					dest.contents_f = 0.0;
				} else return false;
				return true;
			} else if (code == 5) {
				dest.range_from = dest.range_length = dest.ex_data = 0;
				dest.type = TokenType::PrototypeSymbol;
				dest.contents_i = 0;
				dest.contents_f = 0.0;
				return true;
			} else if (code == 6) {
				dest.range_from = dest.range_length = dest.ex_data = 0;
				dest.type = TokenType::PrototypeCommand;
				dest.contents_i = 0;
				dest.contents_f = 0.0;
				return true;
			} else return false;
		}
		bool RestorationStream::ReadBlock(ITokenStream ** deferred_stream)
		{
			SafePointer<DataBlock> data = new DataBlock(0x1000);
			*deferred_stream = 0;
			int level = 0;
			while (true) {
				Token token;
				if (!ReadToken(token)) return false;
				if (token.type == TokenType::EOF) return false;
				else if (token.type == TokenType::Punctuation && token.contents == L"{") level++;
				else if (token.type == TokenType::Punctuation && token.contents == L"}") { level--; if (level < 0) break; }
				SerializeToken(*data, token);
			}
			data->Append(0);
			*deferred_stream = new RestorationStream(data);
			return true;
		}
		void RestorationStream::ExtractRange(int from, int length, string & line, int & line_no, int & line_from, int & line_length) const { line = ""; line_no = 1; line_from = 0; line_length = 0; }
		int RestorationStream::GetCurrentPosition(void) const { return 0; }
		string RestorationStream::ExtractContents(void) const { throw InvalidStateException(); }
		DataBlock * RestorationStream::Serialize(void) { _data->Retain(); return _data; }

		void SerializeToken(DataBlock & into, const Token & token)
		{
			if (token.type == TokenType::EOF) into.Append(0);
			else if (token.type == TokenType::Keyword) SerializeStringLiteral(into, 1, token.contents);
			else if (token.type == TokenType::Identifier) SerializeStringLiteral(into, 2, token.contents);
			else if (token.type == TokenType::Punctuation) SerializeStringLiteral(into, 3, token.contents);
			else if (token.type == TokenType::Literal) {
				if (token.ex_data == TokenLiteralLogicalY) into.Append(4 | 0x00);
				else if (token.ex_data == TokenLiteralLogicalN) into.Append(4 | 0x08);
				else if (token.ex_data == TokenLiteralInteger) {
					into.Append(4 | 0x10);
					SerializeDataLiteral(into, &token.contents_i, 8);
				} else if (token.ex_data == TokenLiteralFloat) {
					into.Append(4 | 0x18);
					SerializeDataLiteral(into, &token.contents_f, 8);
				} else if (token.ex_data == TokenLiteralString) SerializeStringLiteral(into, 4 | 0x20, token.contents);
			} else if (token.type == TokenType::PrototypeSymbol) into.Append(5);
			else if (token.type == TokenType::PrototypeCommand) into.Append(6);
		}
	}
}