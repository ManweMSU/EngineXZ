#include "xi_function.h"

namespace Engine
{
	namespace XI
	{
		uint32 EncodeABI(Platform arch, XA::CallingConvention cc)
		{
			uint32 result = 0;
			if (arch == Platform::X86) result = 1;
			else if (arch == Platform::X64) result = 2;
			else if (arch == Platform::ARM) result = 3;
			else if (arch == Platform::ARM64) result = 4;
			else throw InvalidArgumentException();
			if (cc == XA::CallingConvention::Windows) result |= 0x10000;
			else if (cc == XA::CallingConvention::Unix) result |= 0x20000;
			else throw InvalidArgumentException();
			return result;
		}

		void MakeFunction(Module::Function & dest)
		{
			dest.code.SetReference(0);
			dest.code_flags &= Module::Function::FunctionMiscMask;
		}
		void MakeFunction(Module::Function & dest, XA::Function & src)
		{
			Streaming::MemoryStream stream(0x10000);
			src.Save(&stream);
			stream.Seek(0, Streaming::Begin);
			dest.code = stream.ReadAll();
			dest.code_flags &= Module::Function::FunctionMiscMask;
			dest.code_flags |= Module::Function::FunctionClassXA | Module::Function::FunctionXA_Abstract;
		}
		void MakeFunction(Module::Function & dest, Platform arch, XA::CallingConvention cc, XA::TranslatedFunction & src)
		{
			if ((dest.code_flags & (Module::Function::FunctionClassMask | Module::Function::FunctionTypeMask)) !=
				(Module::Function::FunctionClassXA | Module::Function::FunctionXA_Platform)) dest.code = new DataBlock(1);
			if (!dest.code) dest.code = new DataBlock(1);
			dest.code_flags &= Module::Function::FunctionMiscMask;
			dest.code_flags |= Module::Function::FunctionClassXA | Module::Function::FunctionXA_Platform;
			Streaming::MemoryStream stream(0x10000);
			src.Save(&stream);
			stream.Seek(0, Streaming::Begin);
			SafePointer<DataBlock> data = stream.ReadAll();
			uint32 base = dest.code->Length();
			uint32 abi = EncodeABI(arch, cc);
			uint32 len = data->Length();
			dest.code->SetLength(base + 8 + len);
			MemoryCopy(dest.code->GetBuffer() + base, &abi, 4);
			MemoryCopy(dest.code->GetBuffer() + base + 4, &len, 4);
			MemoryCopy(dest.code->GetBuffer() + base + 8, data->GetBuffer(), len);
		}
		void MakeFunction(Module::Function & dest, const string & import_name)
		{
			dest.code = import_name.EncodeSequence(Encoding::UTF16, false);
			dest.code_flags &= Module::Function::FunctionMiscMask;
			dest.code_flags |= Module::Function::FunctionClassImport | Module::Function::FunctionImportNear;
		}
		void MakeFunction(Module::Function & dest, const string & import_name, const string & dl_name)
		{
			dest.code = new DataBlock(1);
			dest.code_flags &= Module::Function::FunctionMiscMask;
			dest.code_flags |= Module::Function::FunctionClassImport | Module::Function::FunctionImportFar;
			SafePointer<DataBlock> s1 = import_name.EncodeSequence(Encoding::UTF16, true);
			SafePointer<DataBlock> s2 = dl_name.EncodeSequence(Encoding::UTF16, false);
			dest.code->Append(*s1);
			dest.code->Append(*s2);
		}
		void LoadFunction(const string & symbol, const Module::Function & func, IFunctionLoader * loader)
		{
			if (!func.code) throw InvalidArgumentException();
			auto fcls = func.code_flags & Module::Function::FunctionClassMask;
			auto ftyp = func.code_flags & Module::Function::FunctionTypeMask;
			// if (fcls == Module::Function::FunctionClassXA) {
			// 	if (ftyp == Module::Function::FunctionXA_Abstract) {
			// 		Streaming::MemoryStream stream(func.code->GetBuffer(), func.code->Length());
			// 		XA::Function xa_func;
			// 		XA::TranslatedFunction xa_func_t;
			// 		xa_func.Load(&stream);
			// 		if (!loader->GetTranslator()->Translate(xa_func_t, xa_func)) throw InvalidArgumentException();
			// 		loader->HandleFunction(symbol, func, xa_func_t);
			// 	} else if (ftyp == Module::Function::FunctionXA_Platform) {
			// 		uint32 abi = EncodeABI(loader->GetArchitecture(), loader->GetCallingConvention());
			// 		int pos = 0;
			// 		while (pos + 8 <= func.code->Length()) {
			// 			uint32 abi_cmp = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos);
			// 			uint32 length = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos + 4);
			// 			if (abi_cmp == abi) {
			// 				Streaming::MemoryStream stream(func.code->GetBuffer() + pos + 8, length);
			// 				XA::TranslatedFunction xa_func;
			// 				xa_func.Load(&stream);
			// 				loader->HandleFunction(symbol, func, xa_func);
			// 				return;
			// 			} else pos += length + 8;
			// 		}
			// 	} else throw InvalidArgumentException();
			// } else if (fcls == Module::Function::FunctionClassImport) {
			// 	if (ftyp == Module::Function::FunctionImportNear) {
			// 		auto name = string(func.code->GetBuffer(), func.code->Length(), Encoding::UTF16);
			// 		auto imp = loader->ImportFunction(name);
			// 		if (!imp) throw InvalidArgumentException();
			// 		loader->HandleFunction(symbol, func, imp);
			// 	} else if (ftyp == Module::Function::FunctionImportFar) {
			// 		auto ustr = reinterpret_cast<const uint16 *>(func.code->GetBuffer());
			// 		auto ulen = func.code->Length() / 2;
			// 		int sep = 0;
			// 		while (sep < ulen && ustr[sep]) sep++;
			// 		if (sep >= ulen) throw InvalidArgumentException();
			// 		auto name = string(func.code->GetBuffer(), sep * 2, Encoding::UTF16);
			// 		auto lib = string(func.code->GetBuffer() + sep * 2 + 2, func.code->Length() - sep * 2 - 2, Encoding::UTF16);
			// 		auto hlib = loader->LoadDynamicLibrary(lib);
			// 		if (!hlib) throw InvalidArgumentException();
			// 		Array<char> byte_name(1);
			// 		byte_name.SetLength(name.GetEncodedLength(Encoding::UTF8) + 1);
			// 		name.Encode(byte_name.GetBuffer(), Encoding::UTF8, true);
			// 		auto imp = GetLibraryRoutine(hlib, byte_name);
			// 		if (!imp) throw InvalidArgumentException();
			// 		loader->HandleFunction(symbol, func, imp);
			// 	} else throw InvalidArgumentException();
			// } else throw InvalidArgumentException();
		}
	}
}