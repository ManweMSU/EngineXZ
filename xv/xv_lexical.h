#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XV
	{
		enum class TokenType { EOF, Keyword, Identifier, Punctuation, Literal };

		constexpr int TokenLiteralLogicalY	= 1;
		constexpr int TokenLiteralLogicalN	= 2;
		constexpr int TokenLiteralInteger	= 3;
		constexpr int TokenLiteralFloat		= 4;
		constexpr int TokenLiteralString	= 5;

		class Token
		{
		public:
			int range_from, range_length;
			int ex_data;
			TokenType type;
			string contents;
			uint64 contents_i;
			double contents_f;
		};
		class TokenStream : public Object
		{
			const uint32 * _data;
			int _current, _override_base, _length;
		public:
			TokenStream(const uint32 * data, int length, int override_base = 0);
			virtual ~TokenStream(void) override;

			bool ReadToken(Token & dest);
			bool ReadBlock(TokenStream ** deferred_stream);
			void ExtractRange(int from, int length, string & line, int & line_no, int & line_from, int & line_length) const;
			int GetCurrentPosition(void) const;
			string ExtractContents(void) const;
		};
	}
}