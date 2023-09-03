#pragma once

#include "xv_compiler.h"

namespace Engine
{
	namespace XV
	{
		enum class TokenType { EOF = 0, Keyword = 1, Identifier = 2, Punctuation = 3, Literal = 4, PrototypeSymbol = 5, PrototypeCommand = 6 };

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
		class ITokenStream : public Object
		{
		public:
			virtual bool ReadToken(Token & dest) = 0;
			virtual bool ReadBlock(ITokenStream ** deferred_stream) = 0;
			virtual void ExtractRange(int from, int length, string & line, int & line_no, int & line_from, int & line_length) const = 0;
			virtual int GetCurrentPosition(void) const = 0;
			virtual string ExtractContents(void) const = 0;
			virtual DataBlock * Serialize(void) = 0;
		};
		class TokenStream : public ITokenStream
		{
			CodeMetaInfo * _meta;
			const uint32 * _data;
			int _current, _override_base, _length;
		public:
			TokenStream(const uint32 * data, int length, CodeMetaInfo * meta, int override_base = 0);
			virtual ~TokenStream(void) override;

			virtual bool ReadToken(Token & dest) override;
			virtual bool ReadBlock(ITokenStream ** deferred_stream) override;
			virtual void ExtractRange(int from, int length, string & line, int & line_no, int & line_from, int & line_length) const override;
			virtual int GetCurrentPosition(void) const override;
			virtual string ExtractContents(void) const override;
			virtual DataBlock * Serialize(void) override;
		};
		class RestorationStream : public ITokenStream
		{
			SafePointer<DataBlock> _data;
			int _current;
		public:
			RestorationStream(DataBlock * data, int offset = 0);
			virtual ~RestorationStream(void) override;

			virtual bool ReadToken(Token & dest) override;
			virtual bool ReadBlock(ITokenStream ** deferred_stream) override;
			virtual void ExtractRange(int from, int length, string & line, int & line_no, int & line_from, int & line_length) const override;
			virtual int GetCurrentPosition(void) const override;
			virtual string ExtractContents(void) const override;
			virtual DataBlock * Serialize(void) override;
		};

		void SerializeToken(DataBlock & into, const Token & token);
		bool IsReservedPunctuation(uint32 ucs);
	}
}