#include "xe_logger.h"

#include "../ximg/xi_module.h"

namespace Engine
{
	namespace XE
	{
		class InternalLoggerInterface
		{
		public:
			virtual void Log(const string & line) noexcept = 0;
		};
		class Logger : public IAPIExtension, public InternalLoggerInterface
		{
			SafePointer< Volumes::Dictionary<string, string> > _dict;
			SafePointer<Object> _retain;
			ILoggerSink * _sink;
			
			static void _error_desc(uintptr ec, uintptr esc, const Module * mdl, string & error, string & suberror)
			{
				ErrorContext ectx;
				ectx.error_code = ec;
				ectx.error_subcode = esc;
				GetErrorDescription(ectx, mdl->GetExecutionContext(), error, suberror);
			}
		public:
			Logger(void) : _sink(0) {}
			virtual ~Logger(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"descriptio_erroris") return _error_desc;
				else return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override
			{
				if (interface == L"actuarius") return static_cast<InternalLoggerInterface *>(this);
				else return 0;
			}
			virtual void Log(const string & line) noexcept override { if (_sink) _sink->Log(line); }
			SafePointer< Volumes::Dictionary<string, string> > & GetLocalization(void) noexcept { return _dict; }
			void SetSink(ILoggerSink * sink)
			{
				_retain.SetReference(0);
				_sink = sink;
				if (_sink) _retain.SetRetain(_sink->ExposeObject());
			}
			static Logger * ExposeLogger(const ExecutionContext & xctx)
			{
				auto callback = xctx.GetLoaderCallback();
				if (!callback) return 0;
				auto logger = reinterpret_cast<InternalLoggerInterface *>(callback->ExposeInterface(L"actuarius"));
				if (!logger) return 0;
				return static_cast<Logger *>(logger);
			}
		};

		string GetErrorDescription(const string & ident, const Volumes::Dictionary<string, string> * dict)
		{
			auto del = ident.FindLast(L'.');
			auto loc = dict->GetElementByKey(ident.Fragment(del + 1, -1));
			if (loc) return *loc; else return ident;
		}
		void GetErrorDescription(const ErrorContext & ectx, const ExecutionContext & xctx, string & error, string & suberror)
		{
			try {
				string error_ident, suberror_ident;
				SafePointer< Volumes::Dictionary<string, const SymbolObject *> > smbl = xctx.GetSymbols(SymbolType::Literal, L"error");
				for (auto & s : *smbl) {
					if (static_cast<const LiteralSymbol *>(s.value)->GetValueType() == Reflection::PropertyType::Integer) {
						auto data = static_cast<const int *>(static_cast<const LiteralSymbol *>(s.value)->GetSymbolEntity());
						if (*data == ectx.error_code) { error_ident = s.key; break; }
					}
				}
				if (error_ident.Length()) {
					error = error_ident;
					if (ectx.error_subcode) {
						auto del = error_ident.FindLast(L'.');
						smbl = xctx.GetSymbols(SymbolType::Literal, L"sub_error", error_ident.Fragment(del + 1, -1));
						for (auto & s : *smbl) {
							if (static_cast<const LiteralSymbol *>(s.value)->GetValueType() == Reflection::PropertyType::Integer) {
								auto data = static_cast<const int *>(static_cast<const LiteralSymbol *>(s.value)->GetSymbolEntity());
								if (*data == ectx.error_subcode) { suberror_ident = s.key; break; }
							}
						}
						if (suberror_ident.Length()) suberror = suberror_ident;
						else suberror = L"SE:" + string(ectx.error_subcode, HexadecimalBase, 8);
					}
					auto logger = Logger::ExposeLogger(xctx);
					if (logger) {
						auto dict = logger->GetLocalization().Inner();
						if (dict) {
							error = GetErrorDescription(error, dict);
							suberror = GetErrorDescription(suberror, dict);
						}
					}
				} else {
					error = L"E:" + string(ectx.error_code, HexadecimalBase, 8);
					suberror = L"SE:" + string(ectx.error_subcode, HexadecimalBase, 8);
				}
			} catch (...) {}
		}
		IAPIExtension * CreateLogger(void) { return new Logger; }
		void SetLoggerSink(const ExecutionContext & xctx, ILoggerSink * sink)
		{
			auto logger = Logger::ExposeLogger(xctx);
			if (logger) logger->SetSink(sink);
		}
		void SetErrorLocalization(const ExecutionContext & xctx, Volumes::Dictionary<string, string> * dict)
		{
			auto logger = Logger::ExposeLogger(xctx);
			if (logger) logger->GetLocalization().SetRetain(dict);
		}
		Volumes::Dictionary<string, string> * LoadLocalizationTable(Streaming::Stream * stream)
		{
			SafePointer< Volumes::Dictionary<string, string> > result = new Volumes::Dictionary<string, string>;
			XI::Module module(stream, XI::Module::ModuleLoadFlags::LoadLink);
			for (auto & lit : module.literals) if (lit.value.contents == XI::Module::Literal::Class::String) {
				result->Append(lit.key, lit.value.data_string);
			}
			result->Retain();
			return result;
		}
		void LoadErrorLocalization(const ExecutionContext & xctx, const string & name)
		{
			auto callback = xctx.GetLoaderCallback();
			if (!callback) return;
			SafePointer<Streaming::Stream> stream = callback->OpenModule(name);
			SafePointer< Volumes::Dictionary<string, string> > dict = LoadLocalizationTable(stream);
			SetErrorLocalization(xctx, dict);
		}
	}
}