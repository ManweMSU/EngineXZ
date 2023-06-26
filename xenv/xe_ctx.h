#pragma once

#include "../xasm/xa_trans.h"
#include "xe_rtss.h"

namespace Engine
{
	namespace XE
	{
		enum class ModuleLoadError {
			NoSuchModule, InvalidImageFormat, InvalidFunctionFormat, InvalidFunctionABI, DuplicateSymbol, LinkageFailure,
			NoSuchGlobalImport, NoSuchDynamicLibrary, NoSuchFunctionInDynamicLibrary, AllocationFailure, InitializationFailure
		};
		enum class ExecutionSubsystem { NoUI, ConsoleUI, GUI, Library };

		class ILoaderCallback
		{
		public:
			virtual Streaming::Stream * OpenModule(const string & module_name) noexcept = 0;
			virtual void * GetRoutineAddress(const string & routine_name) noexcept = 0;
			virtual handle LoadDynamicLibrary(const string & library_name) noexcept = 0;
			virtual void HandleModuleLoadError(const string & module_name, const string & subject, ModuleLoadError error) noexcept = 0;
			virtual Object * ExposeObject(void) noexcept = 0;
			virtual void * ExposeInterface(const string & interface) noexcept = 0;
		};

		class Module : public Object
		{
			friend class ExecutionContext;
			Module(const ExecutionContext & xc);
			const ExecutionContext & _xc;
			Volumes::Dictionary<string, handle> _resident_dl;
			Volumes::ObjectDictionary<string, DataBlock> _rsrc;
			string _name, _tool;
			uint _v1, _v2, _v3, _v4;
			ExecutionSubsystem _xss;
			SafePointer<DataBlock> _data;
			SafePointer<XA::IExecutable> _code;
		public:
			virtual ~Module(void) override;
			const Volumes::ObjectDictionary<string, DataBlock> & GetResources(void) const noexcept;
			void GetName(string & name) const noexcept;
			void GetAssembler(string & name, uint & vmajor, uint & vminor, uint & subver, uint & build) const noexcept;
			ExecutionSubsystem GetSubsystem(void) const noexcept;
			const ExecutionContext & GetExecutionContext(void) const noexcept;
		};
		class ExecutionContext : public Object // Thread-Safe!
		{
			SafePointer<Semaphore> _sync;
			SafePointer<Object> _callback_object;
			ILoaderCallback * _callback;
			SymbolSystem _system;
			Volumes::ObjectDictionary<string, Module> _modules;
			SafePointer<XA::IAssemblyTranslator> _trans;
			SafePointer<XA::IExecutableLinker> _linker;
			uint _nameless_counter;
		public:
			ExecutionContext(void);
			ExecutionContext(ILoaderCallback * callback);
			virtual ~ExecutionContext(void) override;

			ILoaderCallback * GetLoaderCallback(void) const noexcept;
			void SetLoaderCallback(ILoaderCallback * callback) noexcept;

			const Module * GetModule(const string & name) const noexcept;
			Volumes::Dictionary<string, const Module *> * GetLoadedModules(void) const noexcept;
			Volumes::Dictionary<string, const SymbolObject *> * GetSymbols(SymbolType of_type, const string & with_attr) const noexcept;
			Volumes::Dictionary<string, const SymbolObject *> * GetSymbols(SymbolType of_type, const string & with_attr, const string & of_value) const noexcept;
			const SymbolObject * GetSymbol(const string & of_name) const noexcept;
			const SymbolObject * GetSymbol(SymbolType of_type, const string & with_attr) const noexcept;
			const SymbolObject * GetSymbol(SymbolType of_type, const string & with_attr, const string & of_value) const noexcept;
			const SymbolObject * GetEntryPoint(void) const noexcept;

			const Module * LoadModule(const string & name) noexcept;
		};

		struct ErrorContext
		{
			uintptr error_code;
			uintptr error_subcode;
		};
		typedef void (* StandardRoutine) (ErrorContext * ectx);
	}
}