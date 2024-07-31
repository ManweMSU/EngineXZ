#include "xi_build.h"

#include "xi_function.h"
#include "xi_resources.h"

namespace Engine
{
	namespace XI
	{
		bool BuildAttributes(Storage::RegistryNode * node, Volumes::Dictionary<string, string> & dest, BuilderStatusDesc & status)
		{
			for (auto & v : node->GetValues()) dest.Append(v, node->GetValueString(v));
			return true;
		}
		bool BuildFunction(Storage::RegistryNode * fn, Module::Function & func, BuilderStatusDesc & status, const string & wd)
		{
			func.vft_index = Point(-1, -1);
			func.code_flags = 0;
			if (fn->GetValueType(L"Import") != Storage::RegistryValueType::Unknown) {
				auto routine = fn->GetValueString(L"Import");
				auto library = fn->GetValueString(L"Library");
				if (library.Length()) MakeFunction(func, routine, library);
				else MakeFunction(func, routine);
			} else if (fn->GetValueType(L"Code") != Storage::RegistryValueType::Unknown) {
				auto asm_file = fn->GetValueString(L"Code");
				auto asm_file_full = IO::ExpandPath(wd + L"/" + asm_file);
				string code;
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(asm_file_full, Streaming::AccessRead, Streaming::OpenExisting);
					SafePointer<Streaming::TextReader> rdr = new Streaming::TextReader(stream);
					code = rdr->ReadAll();
				} catch (...) {
					status.file = asm_file_full;
					status.status = BuilderStatus::FileReadError;
					return false;
				}
				XA::Function function;
				XA::CompileFunction(code, function, status.com_stat);
				if (status.com_stat.status != XA::CompilerStatus::Success) {
					status.file = asm_file_full;
					status.status = BuilderStatus::CompilationError;
					return false;
				}
				MakeFunction(func, function);
			} else MakeFunction(func);
			func.vft_index.x = fn->GetValueInteger(L"VFT");
			func.vft_index.y = fn->GetValueInteger(L"VF");
			if (fn->GetValueBoolean(L"EntryPoint")) func.code_flags |= Module::Function::FunctionEntryPoint;
			if (fn->GetValueBoolean(L"OnInitialize")) func.code_flags |= Module::Function::FunctionInitialize;
			if (fn->GetValueBoolean(L"OnShutdown")) func.code_flags |= Module::Function::FunctionShutdown;
			if (fn->GetValueBoolean(L"Throws")) func.code_flags |= Module::Function::FunctionThrows;
			if (fn->GetValueBoolean(L"Instance")) func.code_flags |= Module::Function::FunctionInstance;
			if (fn->GetValueBoolean(L"ThisCall")) func.code_flags |= Module::Function::FunctionThisCall;
			if (fn->GetValueBoolean(L"Prototype")) func.code_flags |= Module::Function::FunctionPrototype;
			if (fn->GetValueBoolean(L"Inline")) func.code_flags |= Module::Function::FunctionInline;
			if (fn->GetValueBoolean(L"Expanding")) func.code_flags |= Module::Function::ConvertorExpanding;
			if (fn->GetValueBoolean(L"Narrowing")) func.code_flags |= Module::Function::ConvertorNarrowing;
			if (fn->GetValueBoolean(L"Expensive")) func.code_flags |= Module::Function::ConvertorExpensive;
			if (fn->GetValueBoolean(L"Similar")) func.code_flags |= Module::Function::ConvertorSimilar;
			SafePointer<Storage::RegistryNode> attrs = fn->OpenNode(L"Attributes");
			if (attrs) if (!BuildAttributes(attrs, func.attributes, status)) return false;
			return true;
		}
		bool BuildLiterals(Storage::RegistryNode * node, Volumes::Dictionary<string, Module::Literal> & dest, BuilderStatusDesc & status)
		{
			for (auto & v : node->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> ln = node->OpenNode(v);
				Module::Literal lit;
				lit.data_uint64 = 0;
				int length = ln->GetValueInteger(L"Length");
				bool no_sign = ln->GetValueBoolean(L"Unsigned");
				auto type = ln->GetValueType(L"Value");
				if (type == Storage::RegistryValueType::Boolean) {
					lit.data_boolean = ln->GetValueBoolean(L"Value");
					lit.contents = Module::Literal::Class::Boolean;
					lit.length = 1;
				} else if (type == Storage::RegistryValueType::String) {
					lit.data_string = ln->GetValueString(L"Value");
					lit.contents = Module::Literal::Class::String;
					lit.length = 0;
				} else if (type == Storage::RegistryValueType::Float) {
					lit.data_float = ln->GetValueFloat(L"Value");
					lit.contents = Module::Literal::Class::FloatingPoint;
					lit.length = 4;
				} else if (type == Storage::RegistryValueType::LongFloat) {
					lit.data_double = ln->GetValueLongFloat(L"Value");
					lit.contents = Module::Literal::Class::FloatingPoint;
					lit.length = 8;
				} else {
					if (no_sign) lit.contents = Module::Literal::Class::UnsignedInteger;
					else lit.contents = Module::Literal::Class::SignedInteger;
					if (length != 8 && length != 4 && length != 2 && length != 1) length = 0;
					if (!length) {
						if (type == Storage::RegistryValueType::LongInteger) length = 8;
						else length = 4;
					}
					lit.length = length;
					if (length == 8) lit.data_sint64 = ln->GetValueLongInteger(L"Value");
					else if (length == 4) lit.data_sint32 = ln->GetValueInteger(L"Value");
					else if (length == 2) lit.data_sint16 = ln->GetValueInteger(L"Value");
					else lit.data_sint8 = ln->GetValueInteger(L"Value");
				}
				SafePointer<Storage::RegistryNode> attrs = ln->OpenNode(L"Attributes");
				if (attrs) if (!BuildAttributes(attrs, lit.attributes, status)) return false;
				dest.Append(v, lit);
			}
			return true;
		}
		bool BuildVariables(Storage::RegistryNode * node, Volumes::Dictionary<string, Module::Variable> & dest, BuilderStatusDesc & status)
		{
			for (auto & v : node->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> vn = node->OpenNode(v);
				Module::Variable var;
				var.type_canonical_name = vn->GetValueString(L"TypeCN");
				var.offset.num_bytes = vn->GetValueInteger(L"Offset.B");
				var.offset.num_words = vn->GetValueInteger(L"Offset.W");
				var.size.num_bytes = vn->GetValueInteger(L"Size.B");
				var.size.num_words = vn->GetValueInteger(L"Size.W");
				SafePointer<Storage::RegistryNode> attrs = vn->OpenNode(L"Attributes");
				if (attrs) if (!BuildAttributes(attrs, var.attributes, status)) return false;
				dest.Append(v, var);
			}
			return true;
		}
		bool BuildFunctions(Storage::RegistryNode * node, Volumes::Dictionary<string, Module::Function> & dest, BuilderStatusDesc & status, const string & wd)
		{
			for (auto & v : node->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> fn = node->OpenNode(v);
				Module::Function func;
				if (!BuildFunction(fn, func, status, wd)) return false;
				dest.Append(v, func);
			}
			return true;
		}
		bool BuildInterfaces(Storage::RegistryNode * node, Array<Module::Interface> & dest, BuilderStatusDesc & status)
		{
			for (auto & v : node->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> in = node->OpenNode(v);
				Module::Interface inter;
				inter.interface_name = v;
				inter.vft_pointer_offset.num_bytes = in->GetValueInteger(L"VFT.B");
				inter.vft_pointer_offset.num_words = in->GetValueInteger(L"VFT.W");
				dest.Append(inter);
			}
			return true;
		}
		bool BuildProperties(Storage::RegistryNode * node, Volumes::Dictionary<string, Module::Property> & dest, BuilderStatusDesc & status, const string & wd)
		{
			for (auto & v : node->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> pn = node->OpenNode(v);
				Module::Property prop;
				prop.type_canonical_name = pn->GetValueString(L"TypeCN");
				prop.getter_name = pn->GetValueString(L"Get");
				prop.setter_name = pn->GetValueString(L"Set");
				SafePointer<Storage::RegistryNode> attrs = pn->OpenNode(L"Attributes");
				if (attrs) if (!BuildAttributes(attrs, prop.attributes, status)) return false;
				dest.Append(v, prop);
			}
			return true;
		}
		bool BuildClasses(Storage::RegistryNode * node, Volumes::Dictionary<string, Module::Class> & dest, BuilderStatusDesc & status, const string & wd)
		{
			for (auto & v : node->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> cn = node->OpenNode(v);
				Module::Class cls;
				cls.class_nature = Module::Class::Nature::Standard;
				if (string::CompareIgnoreCase(cn->GetValueString(L"Nature"), L"core") == 0) cls.class_nature = Module::Class::Nature::Core;
				else if (string::CompareIgnoreCase(cn->GetValueString(L"Nature"), L"interface") == 0) cls.class_nature = Module::Class::Nature::Interface;
				auto it = cn->GetValueString(L"Instance.Semantics");
				cls.instance_spec.size.num_bytes = cn->GetValueInteger(L"Instance.Size.B");
				cls.instance_spec.size.num_words = cn->GetValueInteger(L"Instance.Size.W");
				if (string::CompareIgnoreCase(it, L"error") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::ErrorData;
				else if (string::CompareIgnoreCase(it, L"rtti") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::RTTI;
				else if (string::CompareIgnoreCase(it, L"this") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::This;
				else if (string::CompareIgnoreCase(it, L"object") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::Object;
				else if (string::CompareIgnoreCase(it, L"float") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::FloatingPoint;
				else if (string::CompareIgnoreCase(it, L"uint") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::Integer;
				else if (string::CompareIgnoreCase(it, L"sint") == 0) cls.instance_spec.semantics = XA::ArgumentSemantics::SignedInteger;
				else cls.instance_spec.semantics = XA::ArgumentSemantics::Unclassified;
				cls.parent_class.interface_name = cn->GetValueString(L"Parent");
				cls.parent_class.vft_pointer_offset.num_bytes = cn->GetValueInteger(L"VFT.B");
				cls.parent_class.vft_pointer_offset.num_words = cn->GetValueInteger(L"VFT.W");
				SafePointer<Storage::RegistryNode> node;
				node = cn->OpenNode(L"Interfaces");
				if (node) if (!BuildInterfaces(node, cls.interfaces_implements, status)) return false;
				node = cn->OpenNode(L"Fields");
				if (node) if (!BuildVariables(node, cls.fields, status)) return false;
				node = cn->OpenNode(L"Properties");
				if (node) if (!BuildProperties(node, cls.properties, status, wd)) return false;
				node = cn->OpenNode(L"Methods");
				if (node) if (!BuildFunctions(node, cls.methods, status, wd)) return false;
				SafePointer<Storage::RegistryNode> attrs = cn->OpenNode(L"Attributes");
				if (attrs) if (!BuildAttributes(attrs, cls.attributes, status)) return false;
				dest.Append(v, cls);
			}
			return true;
		}
		bool BuildAliases(Storage::RegistryNode * node, Volumes::Dictionary<string, string> & dest, BuilderStatusDesc & status)
		{
			for (auto & v : node->GetValues()) dest.Append(v, node->GetValueString(v));
			return true;
		}
		bool BuildResources(Storage::RegistryNode * node, Volumes::ObjectDictionary<uint64, DataBlock> & dest, BuilderStatusDesc & status, const string & wd)
		{
			SafePointer<Storage::RegistryNode> meta_node = node->OpenNode(L"Metadata");
			SafePointer<Storage::RegistryNode> icon_node = node->OpenNode(L"Icons");
			SafePointer<Storage::RegistryNode> data_node = node->OpenNode(L"Data");
			if (meta_node) {
				Volumes::Dictionary<string, string> meta;
				for (auto & v : meta_node->GetValues()) meta.Append(v, meta_node->GetValueString(v));
				if (!meta.IsEmpty()) AddModuleMetadata(dest, meta);
			}
			if (icon_node) {
				for (auto & v : icon_node->GetValues()) {
					auto path = IO::ExpandPath(wd + L"/" + icon_node->GetValueString(v));
					try {
						int id = v.ToInt32();
						SafePointer<Streaming::Stream> stream = new Streaming::FileStream(path, Streaming::AccessRead, Streaming::OpenExisting);
						SafePointer<Codec::Image> image = Codec::DecodeImage(stream);
						if (!image) throw Exception();
						AddModuleIcon(dest, id, image);
					} catch (...) {
						status.status = BuilderStatus::FileReadError;
						status.file = path;
						return false;
					}
				}
			}
			if (data_node) {
				for (auto & v : data_node->GetValues()) {
					auto path = IO::ExpandPath(wd + L"/" + data_node->GetValueString(v));
					auto spl = v.Split(L':');
					if (spl.Length() == 2) try {
						SafePointer<Streaming::Stream> stream = new Streaming::FileStream(path, Streaming::AccessRead, Streaming::OpenExisting);
						SafePointer<DataBlock> data = stream->ReadAll();
						dest.Append(MakeResourceID(spl[0], spl[1].ToInt32()), data);
					} catch (...) {
						status.status = BuilderStatus::FileReadError;
						status.file = path;
						return false;
					}
				}
			}
			return true;
		}
		void BuildModule(const string & scheme_file, Module & dest, BuilderStatusDesc & status)
		{
			try {
				auto scheme_file_full = IO::ExpandPath(scheme_file);
				auto path = IO::Path::GetDirectory(scheme_file_full);
				SafePointer<Storage::Registry> scheme;
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(scheme_file_full, Streaming::AccessRead, Streaming::OpenExisting);
					try {
						scheme = Storage::CompileTextRegistry(stream);
						if (!scheme) throw Exception();
					} catch (...) {
						status.file = scheme_file_full;
						status.status = BuilderStatus::TextRegistryError;
						return;
					}
				} catch (...) {
					status.file = scheme_file_full;
					status.status = BuilderStatus::FileReadError;
					return;
				}
				{
					auto ss = scheme->GetValueString(L"Subsystem");
					if (string::CompareIgnoreCase(ss, L"console") == 0) dest.subsystem = Module::ExecutionSubsystem::ConsoleUI;
					else if (string::CompareIgnoreCase(ss, L"gui") == 0) dest.subsystem = Module::ExecutionSubsystem::GUI;
					else if (string::CompareIgnoreCase(ss, L"library") == 0) dest.subsystem = Module::ExecutionSubsystem::Library;
					else dest.subsystem = Module::ExecutionSubsystem::NoUI;
					dest.module_import_name = scheme->GetValueString(L"Name");
					dest.assembler_name = BuilderStamp;
					dest.assembler_version.major = BuilderVersionMajor;
					dest.assembler_version.minor = BuilderVersionMinor;
					dest.assembler_version.subver = BuilderSubversion;
					dest.assembler_version.build = BuilderBuildNumber;
					dest.modules_depends_on = scheme->GetValueString(L"Dependencies").Split(L';');
					dest.data = new DataBlock(1);
					dest.data->SetLength(scheme->GetValueBinarySize(L"Data"));
					scheme->GetValueBinary(L"Data", dest.data->GetBuffer());
					for (int i = dest.modules_depends_on.Length() - 1; i >= 0; i--) if (!dest.modules_depends_on[i].Length()) dest.modules_depends_on.Remove(i);
				}
				SafePointer<Storage::RegistryNode> node;
				node = scheme->OpenNode(L"Literals");
				if (node) if (!BuildLiterals(node, dest.literals, status)) return;
				node = scheme->OpenNode(L"Variables");
				if (node) if (!BuildVariables(node, dest.variables, status)) return;
				node = scheme->OpenNode(L"Functions");
				if (node) if (!BuildFunctions(node, dest.functions, status, path)) return;
				node = scheme->OpenNode(L"Classes");
				if (node) if (!BuildClasses(node, dest.classes, status, path)) return;
				node = scheme->OpenNode(L"Aliases");
				if (node) if (!BuildAliases(node, dest.aliases, status)) return;
				node = scheme->OpenNode(L"Resources");
				if (node) if (!BuildResources(node, dest.resources, status, path)) return;
				status.status = BuilderStatus::Success;
			} catch (...) {
				status.status = BuilderStatus::InternalError;
			}
		}
	}
}