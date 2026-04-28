#pragma once

#include <UserInterface/InterfaceFormat.h>
#include <Storage/Registry.h>

namespace Engine
{
	namespace UI
	{
		namespace Markup
		{
			enum class ErrorClass { OK, UnknownError, SourceAccess, MainInvalidToken, IncludedInvalidToken, UnexpectedLexem, InvalidLocaleIdentifier, InvalidSystemColor, InvalidEffect, InvalidConstantType, InvalidProperty, ObjectRedifinition, NumericConstantTypeMismatch, UndefinedObject };
			enum class WarningClass { InvalidControlParent, UnknownPlatformName };

			struct InputDesc
			{
				string main_uiml;
				bool build_as_style;
				Array<string> include = Array<string>(0x10);
			};
			struct ErrorDesc
			{
				ErrorClass status;
				string desc;
				int position, length;
			};
			struct WarningDesc
			{
				WarningClass status;
				string desc;
				int position, length;
			};
			class IWarningReporter : public Object
			{
			public:
				virtual void ReportWarning(const WarningDesc & desc) = 0;
			};
			class IFileProvider : public Object
			{
			public:
				virtual string LocateFile(const string & reference_file_name) = 0;
				virtual Streaming::Stream * OpenFile(const string & effective_file_name) = 0;
			};

			void SetWarningReporterCallback(IWarningReporter * callback);
			IWarningReporter * GetWarningReporterCallback(void);
			void SetFileProvider(IFileProvider * provider);
			IFileProvider * GetFileProvider(void);
			void SetSuborderingTable(Storage::RegistryNode * table);
			Storage::RegistryNode * GetSuborderingTable(void);

			Format::InterfaceTemplateImage * CompileInterface(const InputDesc & input, ErrorDesc & error);
		}
	}
}