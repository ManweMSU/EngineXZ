﻿#pragma once

#include <EngineRuntime.h>
#include "../../ximg/xi_function.h"
#include "../../ximg/xi_resources.h"
#include "../../xasm/xa_trans.h"
#include "sys_hdr.h"

namespace Engine
{
	namespace XN
	{
		constexpr const widechar * IntrinsicGlobalInit	= L"xn/ini/modulos";
		constexpr const widechar * IntrinsicGlobalSdwn	= L"xn/exi/modulos";
		constexpr const widechar * IntrinsicGlobalEntry	= L"xn/introitus";
		constexpr const widechar * IntrinsicImageArch	= L"xn/architectura";

		constexpr const widechar * AttributeStdCall		= L"stdcall";
		constexpr const widechar * AttributeSysEntry	= L"xn.introitus";
		constexpr const widechar * AttributeDynamicLink	= L"xn.dynamice.adl";
		constexpr const widechar * AttributeDynamicCall	= L"xn.dynamice.inv";

		constexpr const widechar * AttributeRequiredStack	= L"XN.Acervus";
		constexpr const widechar * AttributeDesiredStack	= L"XN.AcervusMaximus";
		constexpr const widechar * AttributeRequiredHeap	= L"XN.Memoria";
		constexpr const widechar * AttributeDesiredHeap		= L"XN.MemoriaMaxima";

		constexpr const widechar * AttributeMinOSVersion	= L"XN.VersioSystemaeMinima";

		constexpr const widechar * AttributeNoVersionInfo	= L"XN.InformatioVersionisNulla";
		constexpr const widechar * AttributeNoManifest		= L"XN.ManifestumNullum";
		constexpr const widechar * AttributeNoCommCtrl60	= L"XN.CommCtrl60Nullus";
		constexpr const widechar * AttributeNoHighDPI		= L"XN.CulicareDensumNullum";
		constexpr const widechar * AttributeNeedsElevation	= L"XN.ExaltatioNecessaria";

		constexpr const widechar * AttributeEnvironment		= L"XN.Circumiectum";
		constexpr const widechar * EnvironmentCommCtrl		= L"commctrl";
		constexpr const widechar * EnvironmentCOM			= L"com";
		constexpr const widechar * EnvironmentSilentError	= L"silens";
		constexpr const widechar * EnvironmentConsole		= L"consolatorium";

		enum class LinkerErrorCode {
			Success					= 0,
			MainModuleLoadingError	= 1,
			ResourceLoadingError	= 2,
			ModuleNotFound			= 3,
			LocalImportInModule		= 4,
			UnknownSymbolReference	= 5,
			IllegalSymbolReference	= 6,
			NoMainEntryPoint		= 7,
			NoRootEntryPoint		= 8,
			WrongABI				= 9,
		};
		struct LinkerInput
		{
			string path_xx, path_attr;
			Array<string> path_xo = Array<string>(0x10);
			Volumes::Dictionary<uint, string> resources;
			Platform arch = Platform::Unknown;
			XA::Environment osenv = XA::Environment::Unknown;
			uint64 base_rva;
		};
		struct LinkerOutput
		{
			XI::Module::ExecutionSubsystem subsystem;
			uint64 image_base, entry_rva;
			uint32 cs_rva, ds_rva, ls_rva, is_rva, rs_rva;
			uint32 ls_size, is_size, rs_size;
			SafePointer<DataBlock> cs, ds, ls, is, rs;
			uint32 required_stack, required_heap;
			uint32 desired_stack, desired_heap;
			int32 minimal_os_major, minimal_os_minor;
		};
		struct LinkerError
		{
			LinkerErrorCode code;
			string object;
		};
		void Link(const LinkerInput & input, LinkerOutput & output, LinkerError & error);
		uint Align(uint number, uint alignment);
		void AddSection(PESuperHeader32 & hdr, const char * name, DataBlock * ss, uint32 rva, uint32 flags);
		void AddSection(PESuperHeader64 & hdr, const char * name, DataBlock * ss, uint32 rva, uint32 flags);
		uint PEFormatHeaderSize(Platform arch);
	}
}