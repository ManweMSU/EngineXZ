#include "xw_types.h"
#include "../xasm/xa_types_formatter.h"

namespace Engine
{
	namespace XW
	{
		TranslationRules::TranslationRules(void) : extrefs(0x10), blocks(0x10) {}
		void TranslationRules::Clear(void) { extrefs.Clear(); blocks.Clear(); }
		void TranslationRules::Save(Streaming::Stream * dest) const
		{
			XA::Encoder::EncodeALInt(dest, extrefs.Length());
			for (auto & r : extrefs) XA::Encoder::EncodeString(dest, r);
			XA::Encoder::EncodeALInt(dest, blocks.Length());
			for (auto & r : blocks) {
				XA::Encoder::EncodeALInt(dest, uint(r.rule));
				XA::Encoder::EncodeALInt(dest, uint(r.index));
				XA::Encoder::EncodeString(dest, r.text);
			}
		}
		void TranslationRules::Load(Streaming::Stream * src)
		{
			Clear();
			auto num = int(XA::Encoder::DecodeALInt(src));
			for (int i = 0; i < num; i++) extrefs << XA::Encoder::DecodeString(src);
			num = int(XA::Encoder::DecodeALInt(src));
			for (int i = 0; i < num; i++) {
				TranslationBlock block;
				block.rule = static_cast<Rule>(XA::Encoder::DecodeALInt(src));
				block.index = XA::Encoder::DecodeALInt(src);
				block.text = XA::Encoder::DecodeString(src);
				blocks << block;
			}
		}
		void MakeFunction(XI::Module::Function & dest)
		{
			dest.code.SetReference(0);
			dest.code_flags &= XI::Module::Function::FunctionMiscMask;
		}
		void MakeFunction(XI::Module::Function & dest, XA::Function & src)
		{
			Streaming::MemoryStream stream(0x10000);
			src.Save(&stream);
			stream.Seek(0, Streaming::Begin);
			dest.code = stream.ReadAll();
			dest.code_flags &= XI::Module::Function::FunctionMiscMask;
			dest.code_flags |= XI::Module::Function::FunctionClassXW | XI::Module::Function::FunctionXW_HintedXA;
		}
		void MakeFunction(XI::Module::Function & dest, ShaderLanguage lang, TranslationRules & src)
		{
			if ((dest.code_flags & (XI::Module::Function::FunctionClassMask | XI::Module::Function::FunctionTypeMask)) !=
				(XI::Module::Function::FunctionClassXW | XI::Module::Function::FunctionXW_Rules)) dest.code = new DataBlock(1);
			if (!dest.code) dest.code = new DataBlock(1);
			dest.code_flags &= XI::Module::Function::FunctionMiscMask;
			dest.code_flags |= XI::Module::Function::FunctionClassXW | XI::Module::Function::FunctionXW_Rules;
			Streaming::MemoryStream stream(0x10000);
			src.Save(&stream);
			stream.Seek(0, Streaming::Begin);
			SafePointer<DataBlock> data = stream.ReadAll();
			uint32 base = dest.code->Length();
			uint32 abi = uint(lang);
			uint32 len = data->Length();
			dest.code->SetLength(base + 8 + len);
			MemoryCopy(dest.code->GetBuffer() + base, &abi, 4);
			MemoryCopy(dest.code->GetBuffer() + base + 4, &len, 4);
			MemoryCopy(dest.code->GetBuffer() + base + 8, data->GetBuffer(), len);
		}
		void LoadFunction(const string & symbol, const XI::Module::Function & func, IFunctionLoader * loader)
		{
			if (!loader) throw InvalidArgumentException();
			if (!func.code) {
				loader->HandleLoadError(symbol, func, XI::LoadFunctionError::InvalidFunctionFormat);
				return;
			}
			auto fcls = func.code_flags & XI::Module::Function::FunctionClassMask;
			auto ftyp = func.code_flags & XI::Module::Function::FunctionTypeMask;
			if (fcls == XI::Module::Function::FunctionClassXW) {
				if (ftyp == XI::Module::Function::FunctionXW_HintedXA) {
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(func.code->GetBuffer(), func.code->Length());
					loader->HandleFunction(symbol, func, stream);
				} else if (ftyp == XI::Module::Function::FunctionXW_Rules) {
					try {
						auto lang = loader->GetLanguage();
						int pos = 0;
						while (pos + 8 <= func.code->Length()) {
							ShaderLanguage lang_cmp = static_cast<ShaderLanguage>(*reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos));
							uint32 length = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos + 4);
							if (lang_cmp == lang) {
								SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(func.code->GetBuffer() + pos + 8, length);
								loader->HandleRule(symbol, func, stream);
								return;
							} else pos += length + 8;
						}
					} catch (...) {}
					loader->HandleLoadError(symbol, func, XI::LoadFunctionError::NoTargetPlatform);
				} else loader->HandleLoadError(symbol, func, XI::LoadFunctionError::UnknownImageFlags);
			} else loader->HandleLoadError(symbol, func, XI::LoadFunctionError::UnknownImageFlags);
		}
		Array<ShaderLanguage> * LoadRuleVariants(const XI::Module::Function & func)
		{
			SafePointer< Array<ShaderLanguage> > result = new Array<ShaderLanguage>(0x10);
			auto fcls = func.code_flags & XI::Module::Function::FunctionClassMask;
			auto ftyp = func.code_flags & XI::Module::Function::FunctionTypeMask;
			if (func.code && fcls == XI::Module::Function::FunctionClassXW && ftyp == XI::Module::Function::FunctionXW_Rules) {
				int pos = 0;
				while (pos + 8 <= func.code->Length()) {
					ShaderLanguage lang = static_cast<ShaderLanguage>(*reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos));
					uint32 length = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos + 4);
					result->Append(lang);
					pos += length + 8;
				}
			}
			result->Retain();
			return result;
		}
	}
}