#include "xe_ctx.h"

#include "../ximg/xi_function.h"
#include "../ximg/xi_module.h"

namespace Engine
{
	namespace XE
	{
		struct IncompleteFunction
		{
			string type;
			uint32 flags;
			Volumes::Dictionary<string, string> attrs;
			XA::TranslatedFunction function;
		};
		class FunctionLoader : public XI::IFunctionLoader
		{
		public:
			bool ok;
			XA::IAssemblyTranslator * trans;
			const string * module_name;
			ILoaderCallback * callback;
			SymbolSystem * local;
			Volumes::Dictionary<string, handle> * dl_list;
			Volumes::Dictionary<string, IncompleteFunction> to_link;
		public:
			FunctionLoader(void) {}
			string GetTypeFromSymbol(const string input)
			{
				int index = input.FindFirst(L':');
				if (index >= 0) return input.Fragment(index + 1, -1); else return L"";
			}
			virtual Platform GetArchitecture(void) noexcept override { return trans->GetPlatform(); }
			virtual XA::CallingConvention GetCallingConvention(void) noexcept override { return trans->GetCallingConvention(); }
			virtual void HandleAbstractFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override
			{
				try {
					IncompleteFunction function;
					function.type = GetTypeFromSymbol(symbol);
					function.flags = fin.code_flags;
					function.attrs = fin.attributes;
					XA::Function abstract;
					abstract.Load(fout);
					if (!trans->Translate(function.function, abstract)) throw Exception();
					to_link.Append(symbol, function);
				} catch (...) { HandleLoadError(symbol, fin, XI::LoadFunctionError::InvalidFunctionFormat); }
			}
			virtual void HandlePlatformFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override
			{
				try {
					IncompleteFunction function;
					function.type = GetTypeFromSymbol(symbol);
					function.flags = fin.code_flags;
					function.attrs = fin.attributes;
					function.function.Load(fout);
					to_link.Append(symbol, function);
				} catch (...) { HandleLoadError(symbol, fin, XI::LoadFunctionError::InvalidFunctionFormat); }
			}
			virtual void HandleNearImport(const string & symbol, const XI::Module::Function & fin, const string & func_name) noexcept override
			{
				try {
					auto address = callback->GetRoutineAddress(func_name);
					if (!address) {
						ok = false;
						callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::NoSuchGlobalImport);
					} else {
						SafePointer<FunctionSymbol> func = new FunctionSymbol(address, fin.code_flags, GetTypeFromSymbol(symbol), fin.attributes);
						local->RegisterSymbol(func, symbol);
					}
				} catch (...) { HandleLoadError(symbol, fin, XI::LoadFunctionError::InvalidFunctionFormat); }
			}
			virtual void HandleFarImport(const string & symbol, const XI::Module::Function & fin, const string & func_name, const string & lib_name) noexcept override
			{
				try {
					handle mdl;
					auto mdl_resident = dl_list->GetElementByKey(lib_name);
					if (mdl_resident) {
						mdl = *mdl_resident;
					} else {
						mdl = callback->LoadDynamicLibrary(lib_name);
						if (mdl) dl_list->Append(lib_name, mdl);
					}
					if (mdl) {
						Array<char> rname(1);
						rname.SetLength(func_name.GetEncodedLength(Encoding::UTF8) + 1);
						func_name.Encode(rname.GetBuffer(), Encoding::UTF8, true);
						auto address = GetLibraryRoutine(mdl, rname);
						if (!address) {
							ok = false;
							callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::NoSuchFunctionInDynamicLibrary);
						} else {
							SafePointer<FunctionSymbol> func = new FunctionSymbol(address, fin.code_flags, GetTypeFromSymbol(symbol), fin.attributes);
							local->RegisterSymbol(func, symbol);
						}
					} else {
						ok = false;
						callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::NoSuchDynamicLibrary);
					}
				} catch (...) { HandleLoadError(symbol, fin, XI::LoadFunctionError::InvalidFunctionFormat); }
			}
			virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept override
			{
				ok = false;
				if (error == XI::LoadFunctionError::InvalidFunctionFormat) {
					callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::InvalidFunctionFormat);
				} else if (error == XI::LoadFunctionError::UnknownImageFlags) {
					callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::InvalidFunctionFormat);
				} else if (error == XI::LoadFunctionError::NoTargetPlatform) {
					callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::InvalidFunctionABI);
				} else {
					callback->HandleModuleLoadError(*module_name, symbol, ModuleLoadError::InvalidFunctionFormat);
				}
			}
		};
		class ReferenceResolver : public XA::IReferenceResolver
		{
		public:
			ILoaderCallback * callback;
			SymbolSystem * global, * local;
			Volumes::ObjectDictionary<string, Module> * modules;
			string current_module_name;
			Module * current_module;
			string symbol_failed;
		public:
			ReferenceResolver(void) {}
			virtual uintptr ResolveReference(const string & to) noexcept override
			{
				try {
					int index = to.FindFirst(L':');
					string qual, path;
					qual = index >= 0 ? to.Fragment(0, index) : to;
					path = index >= 0 ? to.Fragment(index + 1, -1) : string(L"");
					if (qual == L"S") {
						const SymbolObject * result = 0;
						int jump = 0;
						while (jump <= 10) {
							auto smbl = local->FindSymbol(path, false);
							if (!smbl) smbl = global->FindSymbol(path, false);
							if (!smbl) throw Exception();
							if (smbl->GetSymbolType() == SymbolType::Alias) {
								path = static_cast<const AliasSymbol *>(smbl)->GetDestination();
								jump++;
							} else {
								result = smbl;
								break;
							}
						}
						if (!result || !result->GetSymbolEntity()) throw Exception();
						return reinterpret_cast<uintptr>(result->GetSymbolEntity());
					} else if (qual == L"M") {
						if (path.Length() && path != current_module_name) {
							auto mdl = modules->GetObjectByKey(path);
							if (mdl) return reinterpret_cast<uintptr>(mdl);
							else throw Exception();
						} else return reinterpret_cast<uintptr>(current_module);
					} else if (qual == L"I") {
						return reinterpret_cast<uintptr>(callback->ExposeInterface(path));
					} else throw Exception();
				} catch (...) { symbol_failed = to; return 0; }
			}
		};

		Module::Module(const ExecutionContext & xc) : _xc(xc) {}
		Module::~Module(void) { for (auto & dl : _resident_dl) ReleaseLibrary(dl.value); }
		const Volumes::ObjectDictionary<string, DataBlock> & Module::GetResources(void) const noexcept { return _rsrc; }
		void Module::GetName(string & name) const noexcept { try { name = _name; } catch (...) { } }
		void Module::GetAssembler(string & name, uint & vmajor, uint & vminor, uint & subver, uint & build) const noexcept { try { name = _tool; vmajor = _v1; vminor = _v2; subver = _v3; build = _v4; } catch (...) {} }
		ExecutionSubsystem Module::GetSubsystem(void) const noexcept { return _xss; }
		const ExecutionContext & Module::GetExecutionContext(void) const noexcept { return _xc; }
		
		ExecutionContext::ExecutionContext(void) : _callback(0), _nameless_counter(0)
		{
			_sync = CreateSemaphore(1);
			if (!_sync) throw Exception();
			_trans = XA::CreatePlatformTranslator();
			_linker = XA::CreateLinker();
			if (!_trans || !_linker) throw Exception();
		}
		ExecutionContext::ExecutionContext(ILoaderCallback * callback) : _callback(callback), _nameless_counter(0)
		{
			_sync = CreateSemaphore(1);
			if (!_sync) throw Exception();
			if (_callback) _callback_object.SetRetain(_callback->ExposeObject());
			_trans = XA::CreatePlatformTranslator();
			_linker = XA::CreateLinker();
			if (!_trans || !_linker) throw Exception();
		}
		ExecutionContext::~ExecutionContext(void)
		{
			for (auto & smbl : _system.GetSymbolTable()) {
				if (smbl.value->GetSymbolType() == SymbolType::Function) {
					auto func = static_cast<const FunctionSymbol *>(smbl.value.Inner());
					if (func->GetFlags() & XI::Module::Function::FunctionShutdown) {
						ErrorContext ectx;
						ectx.error_code = ectx.error_subcode = 0;
						auto sdrt = reinterpret_cast<StandardRoutine>(func->GetSymbolEntity());
						sdrt(&ectx);
					}
				}
			}
		}
		ILoaderCallback * ExecutionContext::GetLoaderCallback(void) const noexcept
		{
			_sync->Wait();
			auto result = _callback;
			_sync->Open();
			return result;
		}
		void ExecutionContext::SetLoaderCallback(ILoaderCallback * callback) noexcept
		{
			_sync->Wait();
			_callback = callback;
			if (_callback) _callback_object.SetRetain(_callback->ExposeObject()); else _callback_object.SetReference(0);
			_sync->Open();
		}
		const Module * ExecutionContext::GetModule(const string & name) const noexcept
		{
			_sync->Wait();
			auto result = _modules.GetElementByKey(name);
			_sync->Open();
			if (result) return *result; else return 0;
		}
		Volumes::Dictionary<string, const Module *> * ExecutionContext::GetLoadedModules(void) const noexcept
		{
			SafePointer< Volumes::Dictionary<string, const Module *> > result;
			_sync->Wait();
			try {
				result = new Volumes::Dictionary<string, const Module *>;
				for (auto & m : _modules) result->Append(m.key, m.value);
			} catch (...) {
				_sync->Open();
				return 0;
			}
			_sync->Open();
			result->Retain();
			return result;
		}
		Volumes::Dictionary<string, const SymbolObject *> * ExecutionContext::GetSymbols(SymbolType of_type, const string & with_attr) const noexcept
		{
			SafePointer< Volumes::Dictionary<string, const SymbolObject *> > result;
			_sync->Wait();
			try {
				result = new Volumes::Dictionary<string, const SymbolObject *>;
				for (auto & smbl : _system.GetSymbolTable()) {
					if (smbl.value->GetSymbolType() == of_type && smbl.value->GetAttributes()) {
						if (smbl.value->GetAttributes()->ElementExists(with_attr)) result->Append(smbl.key, smbl.value);
					}
				}
			} catch (...) {}
			_sync->Open();
			if (result) result->Retain();
			return result;
		}
		Volumes::Dictionary<string, const SymbolObject *> * ExecutionContext::GetSymbols(SymbolType of_type, const string & with_attr, const string & of_value) const noexcept
		{
			SafePointer< Volumes::Dictionary<string, const SymbolObject *> > result;
			_sync->Wait();
			try {
				result = new Volumes::Dictionary<string, const SymbolObject *>;
				for (auto & smbl : _system.GetSymbolTable()) {
					if (smbl.value->GetSymbolType() == of_type && smbl.value->GetAttributes()) {
						auto value = smbl.value->GetAttributes()->GetElementByKey(with_attr);
						if (value && *value == of_value) result->Append(smbl.key, smbl.value);
					}
				}
			} catch (...) {}
			_sync->Open();
			if (result) result->Retain();
			return result;
		}
		const SymbolObject * ExecutionContext::GetSymbol(const string & of_name) const noexcept
		{
			_sync->Wait();
			auto smbl = _system.FindSymbol(of_name);
			_sync->Open();
			return smbl;
		}
		const SymbolObject * ExecutionContext::GetSymbol(SymbolType of_type, const string & with_attr) const noexcept
		{
			const SymbolObject * result = 0;
			_sync->Wait();
			try {
				for (auto & smbl : _system.GetSymbolTable()) {
					if (smbl.value->GetSymbolType() == of_type && smbl.value->GetAttributes()) {
						if (smbl.value->GetAttributes()->ElementExists(with_attr)) {
							result = smbl.value;
							break;
						}
					}
				}
			} catch (...) {}
			_sync->Open();
			return result;
		}
		const SymbolObject * ExecutionContext::GetSymbol(SymbolType of_type, const string & with_attr, const string & of_value) const noexcept
		{
			const SymbolObject * result = 0;
			_sync->Wait();
			try {
				for (auto & smbl : _system.GetSymbolTable()) {
					if (smbl.value->GetSymbolType() == of_type && smbl.value->GetAttributes()) {
						auto value = smbl.value->GetAttributes()->GetElementByKey(with_attr);
						if (value && *value == of_value) {
							result = smbl.value;
							break;
						}
					}
				}
			} catch (...) {}
			_sync->Open();
			return result;
		}
		const SymbolObject * ExecutionContext::GetEntryPoint(void) const noexcept
		{
			const SymbolObject * result = 0;
			_sync->Wait();
			try {
				for (auto & smbl : _system.GetSymbolTable()) {
					if (smbl.value->GetSymbolType() == SymbolType::Function) {
						auto func = static_cast<const FunctionSymbol *>(smbl.value.Inner());
						if (func->GetFlags() & XI::Module::Function::FunctionEntryPoint) {
							result = func;
							break;
						}
					}
				}
			} catch (...) {}
			_sync->Open();
			return result;
		}
		const Module * ExecutionContext::LoadModule(const string & name) noexcept
		{
			_sync->Wait();
			for (auto & m : _modules) if (m.key == name) { auto ref = m.value.Inner(); _sync->Open(); return ref; }
			if (!_callback) { _sync->Open(); return 0; }
			ILoaderCallback * callback = _callback;
			SafePointer<Streaming::Stream> input = callback->OpenModule(name);
			if (!input) callback->HandleModuleLoadError(name, L"", ModuleLoadError::NoSuchModule);
			_sync->Open();
			if (!input) return 0;
			SafePointer<XI::Module> data;
			SafePointer<Module> result;
			try {
				data = new XI::Module(input, XI::Module::ModuleLoadFlags::LoadExecute);
				result = new Module(*this);
				if (data->subsystem == XI::Module::ExecutionSubsystem::NoUI) result->_xss = ExecutionSubsystem::NoUI;
				else if (data->subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) result->_xss = ExecutionSubsystem::ConsoleUI;
				else if (data->subsystem == XI::Module::ExecutionSubsystem::GUI) result->_xss = ExecutionSubsystem::GUI;
				else if (data->subsystem == XI::Module::ExecutionSubsystem::Library) result->_xss = ExecutionSubsystem::Library;
				else throw InvalidArgumentException();
				result->_name = data->module_import_name;
				result->_tool = data->assembler_name;
				result->_v1 = data->assembler_version.major;
				result->_v2 = data->assembler_version.minor;
				result->_v3 = data->assembler_version.subver;
				result->_v4 = data->assembler_version.build;
				result->_data = data->data;
				result->_rsrc = data->resources;
			} catch (...) {
				_sync->Wait();
				callback->HandleModuleLoadError(name, L"", ModuleLoadError::InvalidImageFormat);
				_sync->Open();
				return 0;
			}
			for (auto & md : data->modules_depends_on) if (!LoadModule(md)) return 0;
			SymbolSystem local;
			FunctionLoader loader;
			loader.ok = true;
			loader.trans = _trans;
			loader.module_name = &result->_name;
			loader.callback = callback;
			loader.local = &local;
			loader.dl_list = &result->_resident_dl;
			_sync->Wait();
			try {
				auto word_size = _trans->GetWordSize();
				for (auto & l : data->literals) {
					if (l.key[0] != L'.' && (local.FindSymbol(l.key, false) || _system.FindSymbol(l.key, false))) {
						callback->HandleModuleLoadError(name, l.key, ModuleLoadError::DuplicateSymbol);
						_sync->Open();
						return 0;
					}
					SafePointer<LiteralSymbol> lit;
					if (l.value.contents == XI::Module::Literal::Class::String) {
						lit = new LiteralSymbol(&l.value.data_string, Reflection::PropertyType::String, l.value.attributes);
					} else if (l.value.contents == XI::Module::Literal::Class::Boolean) {
						if (l.value.length == 1) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Boolean, l.value.attributes);
						else throw InvalidArgumentException();
					} else if (l.value.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (l.value.length == 1) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::UInt8, l.value.attributes);
						else if (l.value.length == 2) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::UInt16, l.value.attributes);
						else if (l.value.length == 4) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::UInt32, l.value.attributes);
						else if (l.value.length == 8) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::UInt64, l.value.attributes);
						else throw InvalidArgumentException();
					} else if (l.value.contents == XI::Module::Literal::Class::SignedInteger) {
						if (l.value.length == 1) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Int8, l.value.attributes);
						else if (l.value.length == 2) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Int16, l.value.attributes);
						else if (l.value.length == 4) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Int32, l.value.attributes);
						else if (l.value.length == 8) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Int64, l.value.attributes);
						else throw InvalidArgumentException();
					} else if (l.value.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (l.value.length == 4) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Float, l.value.attributes);
						else if (l.value.length == 8) lit = new LiteralSymbol(&l.value.data_uint64, Reflection::PropertyType::Double, l.value.attributes);
						else throw InvalidArgumentException();
					} else throw InvalidArgumentException();
					local.RegisterSymbol(lit, l.key);
				}
				for (auto & v : data->variables) {
					if (v.key[0] != L'.' && (local.FindSymbol(v.key, false) || _system.FindSymbol(v.key, false))) {
						callback->HandleModuleLoadError(name, v.key, ModuleLoadError::DuplicateSymbol);
						_sync->Open();
						return 0;
					}
					SafePointer<VariableSymbol> var = new VariableSymbol(result->_data->GetBuffer(),
						v.value.offset.num_bytes + v.value.offset.num_words * word_size,
						v.value.size.num_bytes + v.value.size.num_words * word_size,
						v.value.type_canonical_name, v.value.attributes);
					local.RegisterSymbol(var, v.key);
				}
				for (auto & f : data->functions) {
					if (f.value.code_flags & XI::Module::Function::FunctionPrototype) continue;
					if (f.key[0] != L'.' && (local.FindSymbol(f.key, false) || _system.FindSymbol(f.key, false))) {
						callback->HandleModuleLoadError(name, f.key, ModuleLoadError::DuplicateSymbol);
						_sync->Open();
						return 0;
					}
					XI::LoadFunction(f.key, f.value, &loader);
					if (!loader.ok) {
						_sync->Open();
						return 0;
					}
				}
				for (auto & c : data->classes) {
					if (c.key[0] != L'.' && (local.FindSymbol(c.key, false) || _system.FindSymbol(c.key, false))) {
						callback->HandleModuleLoadError(name, c.key, ModuleLoadError::DuplicateSymbol);
						_sync->Open();
						return 0;
					}
					bool class_discard = false;
					SafePointer<ClassSymbol> cls = new ClassSymbol(c.key, c.value.instance_spec.semantics,
						c.value.instance_spec.size.num_bytes + c.value.instance_spec.size.num_words * word_size, c.value.attributes);
					if (c.value.parent_class.vft_pointer_offset.num_bytes != 0xFFFFFFFF && c.value.parent_class.vft_pointer_offset.num_words != 0xFFFFFFFF) {
						int offset = c.value.parent_class.vft_pointer_offset.num_bytes + c.value.parent_class.vft_pointer_offset.num_words * word_size;
						if (c.value.parent_class.interface_name.Length()) cls->AddInterface(c.value.parent_class.interface_name, offset, 0);
						else cls->AddInterface(c.key, offset, 0);
					} else {
						if (c.value.parent_class.interface_name.Length()) cls->AddInterface(c.value.parent_class.interface_name, -1, 0);
						else cls->AddInterface(c.key, -1, 0);
					}
					for (auto & i : c.value.interfaces_implements) {
						int offset = i.vft_pointer_offset.num_bytes + i.vft_pointer_offset.num_words * word_size;
						cls->AddInterface(i.interface_name, offset, offset);
					}
					for (auto & f : c.value.fields) {
						int offset = f.value.offset.num_bytes + f.value.offset.num_words * word_size;
						int size = f.value.size.num_bytes + f.value.size.num_words * word_size;
						cls->AddField(f.key, f.value.type_canonical_name, offset, size, f.value.attributes);
					}
					for (auto & p : c.value.properties) {
						cls->AddProperty(p.key, p.value.type_canonical_name, p.value.setter_name, p.value.getter_name, p.value.attributes);
					}
					for (auto & m : c.value.methods) {
						if (m.value.code_flags & XI::Module::Function::FunctionPrototype) class_discard = true;
						string m_fqn;
						if ((m.value.code_flags & XI::Module::Function::FunctionClassMask) != XI::Module::Function::FunctionClassNull) {
							m_fqn = c.key + L"." + m.key;
							if (local.FindSymbol(m_fqn, false) || _system.FindSymbol(m_fqn, false)) {
								callback->HandleModuleLoadError(name, m_fqn, ModuleLoadError::DuplicateSymbol);
								_sync->Open();
								return 0;
							}
							if (!class_discard) {
								XI::LoadFunction(m_fqn, m.value, &loader);
								if (!loader.ok) {
									_sync->Open();
									return 0;
								}
							}
						}
						cls->AddMethod(m.key, m_fqn, m.value.vft_index.x, m.value.vft_index.y, m.value.attributes);
					}
					if (!class_discard) local.RegisterSymbol(cls, c.key);
				}
			} catch (...) {
				callback->HandleModuleLoadError(name, L"", ModuleLoadError::AllocationFailure);
				_sync->Open();
				return 0;
			}
			data.SetReference(0);
			try {
				ReferenceResolver resolver;
				resolver.callback = callback;
				resolver.global = &_system;
				resolver.local = &local;
				resolver.modules = &_modules;
				resolver.current_module_name = name;
				resolver.current_module = result.Inner();
				Volumes::Dictionary<string, XA::TranslatedFunction *> funcs;
				for (auto & l : loader.to_link) funcs.Append(L"S:" + l.key, &l.value.function);
				result->_code = _linker->LinkFunctions(funcs, &resolver);
				if (!result->_code) {
					callback->HandleModuleLoadError(name, resolver.symbol_failed, ModuleLoadError::LinkageFailure);
					_sync->Open();
					return 0;
				}
				for (auto & l : loader.to_link) {
					auto address = result->_code->GetEntryPoint(L"S:" + l.key);
					SafePointer<FunctionSymbol> func = new FunctionSymbol(address, l.value.flags, l.value.type, l.value.attrs);
					local.RegisterSymbol(func, l.key);
				}
				loader.to_link.Clear();
			} catch (...) {
				callback->HandleModuleLoadError(name, L"", ModuleLoadError::AllocationFailure);
				_sync->Open();
				return 0;
			}
			try {
				_modules.Append(name, result);
				for (auto & smbl : local.GetSymbolTable()) {
					if (smbl.key[0] == L'.') {
						string rename = L"." + string(_nameless_counter);
						_nameless_counter++;
						_system.RegisterSymbol(smbl.value, rename);
					} else _system.RegisterSymbol(smbl.value, smbl.key);
				}
			} catch (...) {
				callback->HandleModuleLoadError(name, L"", ModuleLoadError::AllocationFailure);
				_sync->Open();
				return 0;
			}
			_sync->Open();
			for (auto & smbl : local.GetSymbolTable()) {
				if (smbl.value->GetSymbolType() == SymbolType::Function) {
					auto func = static_cast<const FunctionSymbol *>(smbl.value.Inner());
					if (func->GetFlags() & XI::Module::Function::FunctionInitialize) {
						ErrorContext ectx;
						ectx.error_code = ectx.error_subcode = 0;
						auto inirt = reinterpret_cast<StandardRoutine>(func->GetSymbolEntity());
						inirt(&ectx);
						if (ectx.error_code) {
							for (auto & smbl : local.GetSymbolTable()) {
								if (smbl.value->GetSymbolType() == SymbolType::Function) {
									auto func = static_cast<FunctionSymbol *>(smbl.value.Inner());
									if (func->GetFlags() & XI::Module::Function::FunctionShutdown) {
										func->SetFlags(func->GetFlags() & ~XI::Module::Function::FunctionShutdown);
									}
								}
							}
							_sync->Wait();
							callback->HandleModuleLoadError(name, L"", ModuleLoadError::InitializationFailure);
							_sync->Open();
							return 0;
						}
					}
				}
			}
			return result;
		}
	}
}