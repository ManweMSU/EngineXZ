#include "xi_function.h"

namespace Engine
{
	namespace XI
	{
		uint32 EncodeABI(Platform arch, XA::Environment osenv)
		{
			uint32 result = 0;
			if (arch == Platform::X86) result = 1;
			else if (arch == Platform::X64) result = 2;
			else if (arch == Platform::ARM) result = 3;
			else if (arch == Platform::ARM64) result = 4;
			else throw InvalidArgumentException();
			if (osenv == XA::Environment::Windows)		result |= 0x010000;
			else if (osenv == XA::Environment::EFI)		result |= 0x110000;
			else if (osenv == XA::Environment::MacOSX)	result |= 0x020000;
			else if (osenv == XA::Environment::Linux)	result |= 0x120000;
			else if (osenv == XA::Environment::XSO)		result |= 0x030000;
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
		void MakeFunction(Module::Function & dest, Platform arch, XA::Environment osenv, XA::TranslatedFunction & src)
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
			uint32 abi = EncodeABI(arch, osenv);
			uint32 len = data->Length();
			dest.code->SetLength(base + 8 + len);
			MemoryCopy(dest.code->GetBuffer() + base, &abi, 4);
			MemoryCopy(dest.code->GetBuffer() + base + 4, &len, 4);
			MemoryCopy(dest.code->GetBuffer() + base + 8, data->GetBuffer(), len);
		}
		void MakeFunction(Module::Function & dest, const string & import_name)
		{
			dest.code = import_name.EncodeSequence(Encoding::UTF16, true);
			dest.code_flags &= Module::Function::FunctionMiscMask;
			dest.code_flags |= Module::Function::FunctionClassImport | Module::Function::FunctionImportNear;
		}
		void MakeFunction(Module::Function & dest, const string & import_name, const string & dl_name)
		{
			dest.code = new DataBlock(1);
			dest.code_flags &= Module::Function::FunctionMiscMask;
			dest.code_flags |= Module::Function::FunctionClassImport | Module::Function::FunctionImportFar;
			SafePointer<DataBlock> s1 = import_name.EncodeSequence(Encoding::UTF16, true);
			SafePointer<DataBlock> s2 = dl_name.EncodeSequence(Encoding::UTF16, true);
			dest.code->Append(*s1);
			dest.code->Append(*s2);
		}
		void LoadFunction(const string & symbol, const Module::Function & func, IFunctionLoader * loader)
		{
			if (!loader) throw InvalidArgumentException();
			if (!func.code) {
				loader->HandleLoadError(symbol, func, LoadFunctionError::InvalidFunctionFormat);
				return;
			}
			auto fcls = func.code_flags & Module::Function::FunctionClassMask;
			auto ftyp = func.code_flags & Module::Function::FunctionTypeMask;
			if (fcls == Module::Function::FunctionClassXA) {
				if (ftyp == Module::Function::FunctionXA_Abstract) {
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(func.code->GetBuffer(), func.code->Length());
					loader->HandleAbstractFunction(symbol, func, stream);
				} else if (ftyp == Module::Function::FunctionXA_Platform) {
					try {
						uint32 abi = EncodeABI(loader->GetArchitecture(), loader->GetEnvironment());
						int pos = 0;
						while (pos + 8 <= func.code->Length()) {
							uint32 abi_cmp = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos);
							uint32 length = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos + 4);
							if (abi_cmp == abi) {
								SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(func.code->GetBuffer() + pos + 8, length);
								loader->HandlePlatformFunction(symbol, func, stream);
								return;
							} else pos += length + 8;
						}
					} catch (...) {}
					loader->HandleLoadError(symbol, func, LoadFunctionError::NoTargetPlatform);
				} else loader->HandleLoadError(symbol, func, LoadFunctionError::UnknownImageFlags);
			} else if (fcls == Module::Function::FunctionClassImport) {
				if (ftyp == Module::Function::FunctionImportNear) {
					auto name = string(func.code->GetBuffer(), -1, Encoding::UTF16);
					loader->HandleNearImport(symbol, func, name);
				} else if (ftyp == Module::Function::FunctionImportFar) {
					auto ustr = reinterpret_cast<const uint16 *>(func.code->GetBuffer());
					auto ulen = func.code->Length() / 2;
					int sep = 0;
					while (sep < ulen && ustr[sep]) sep++;
					if (sep >= ulen) loader->HandleLoadError(symbol, func, LoadFunctionError::InvalidFunctionFormat);
					auto name = string(func.code->GetBuffer(), sep * 2, Encoding::UTF16);
					auto lib = string(func.code->GetBuffer() + sep * 2 + 2, -1, Encoding::UTF16);
					loader->HandleFarImport(symbol, func, name, lib);
				} else loader->HandleLoadError(symbol, func, LoadFunctionError::UnknownImageFlags);
			} else loader->HandleLoadError(symbol, func, LoadFunctionError::UnknownImageFlags);
		}
		Array<uint32> * LoadFunctionABI(const Module::Function & func)
		{
			SafePointer< Array<uint32> > result = new Array<uint32>(0x10);
			auto fcls = func.code_flags & Module::Function::FunctionClassMask;
			auto ftyp = func.code_flags & Module::Function::FunctionTypeMask;
			if (func.code && fcls == Module::Function::FunctionClassXA && ftyp == Module::Function::FunctionXA_Platform) {
				int pos = 0;
				while (pos + 8 <= func.code->Length()) {
					uint32 abi = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos);
					uint32 length = *reinterpret_cast<const uint32 *>(func.code->GetBuffer() + pos + 4);
					result->Append(abi);
					pos += length + 8;
				}
			}
			result->Retain();
			return result;
		}
		void ReadFunctionABI(uint32 word, Platform & arch, XA::Environment & osenv)
		{
			if ((word & 0xFFFF) == 1) arch = Platform::X86;
			else if ((word & 0xFFFF) == 2) arch = Platform::X64;
			else if ((word & 0xFFFF) == 3) arch = Platform::ARM;
			else if ((word & 0xFFFF) == 4) arch = Platform::ARM64;
			else arch = Platform::Unknown;
			if ((word & 0xFFFF0000)			== 0x010000) osenv = XA::Environment::Windows;
			else if ((word & 0xFFFF0000)	== 0x110000) osenv = XA::Environment::EFI;
			else if ((word & 0xFFFF0000)	== 0x020000) osenv = XA::Environment::MacOSX;
			else if ((word & 0xFFFF0000)	== 0x120000) osenv = XA::Environment::Linux;
			else if ((word & 0xFFFF0000)	== 0x030000) osenv = XA::Environment::XSO;
			else osenv = XA::Environment::Unknown;
		}
	}
}