#include "xi_module.h"

namespace Engine
{
	namespace XI
	{
		namespace Format
		{
			namespace DS
			{
				ENGINE_PACKED_STRUCTURE(XI_Header)
					uint64 signature;		// "xximago"
					uint32 format_version;	// 0
					uint32 target_subsystem;
					uint32 info_segment_offset;
					uint32 info_segment_size;
					uint32 data_segment_offset;
					uint32 data_segment_size;
					uint32 rsrc_segment_offset;
					uint32 rsrc_segment_size;
					uint32 module_name_offset;
					uint32 module_assembler_offset;
					uint32 module_assembler_version_major;
					uint32 module_assembler_version_minor;
					uint32 module_assembler_version_subversion;
					uint32 module_assembler_version_build;
					uint32 import_list_offset;
					uint32 import_list_size;
					uint32 symbol_list_offset;
					uint32 symbol_list_size;
					uint32 resource_list_offset;
					uint32 resource_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Symbol)
					uint32 symbol_name_offset;
					uint32 symbol_type; // 1 - literal, 2 - class, 3 - variable, 4 - function, 5 - alias, 6 - property, 7 - prototype
					uint32 symbol_data_offset;
					uint32 symbol_data_size; // not used for alias
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Resource)
					uint32 resource_type;
					uint32 resource_id;
					uint32 resource_data_offset;
					uint32 resource_data_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Attribute)
					uint32 attribute_name_offset;
					uint32 attribute_value_offset;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Literal)
					uint32 literal_class;
					uint32 literal_size;
					uint64 literal_data;
					uint32 literal_attribute_list_offset;
					uint32 literal_attribute_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Variable)
					uint32 variable_type_cn_offset;
					uint32 variable_offset_byte_size;
					uint32 variable_offset_word_size;
					uint32 variable_size_byte_size;
					uint32 variable_size_word_size;
					uint32 variable_attribute_list_offset;
					uint32 variable_attribute_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Function)
					uint32 function_flags;
					uint32 function_code_offset;
					uint32 function_code_size;
					uint32 function_vft_table_index;
					uint32 function_vft_function_index;
					uint32 function_attribute_list_offset;
					uint32 function_attribute_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Property)
					uint32 property_type_cn_offset;
					uint32 property_getter_name_offset;
					uint32 property_setter_name_offset;
					uint32 property_attribute_list_offset;
					uint32 property_attribute_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Interface)
					uint32 class_name_offset;
					uint32 vftp_byte_offset;
					uint32 vftp_word_offset;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Class)
					uint32 class_language_semantics;
					uint32 class_argument_semantics;
					uint32 class_byte_size;
					uint32 class_word_size;
					XI_Interface class_parent;
					uint32 class_interface_list_offset;
					uint32 class_interface_list_size;
					uint32 class_symbol_list_offset;
					uint32 class_symbol_list_size;
					uint32 class_attribute_list_offset;
					uint32 class_attribute_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Prototype)
					uint32 prototype_target_language_name_offset;
					uint32 prototype_data_offset;
					uint32 prototype_data_size;
					uint32 prototype_attribute_list_offset;
					uint32 prototype_attribute_list_size;
				ENGINE_END_PACKED_STRUCTURE
			}

			template<class T> uint32 PreserveSpace(DataBlock & data, const T * t, int num_items = 1)
			{
				if (sizeof(T) * num_items) {
					uint32 offset = data.Length();
					data.SetLength(offset + sizeof(T) * num_items);
					return offset;
				} else return 0;
			}
			template<class T> void EnplaceObject(DataBlock & data, uint32 at, const T * t, int num_items = 1) { if (sizeof(T) * num_items) MemoryCopy(data.GetBuffer() + at, t, sizeof(T) * num_items); }
			uint32 EnplaceString(DataBlock & data, const string & text)
			{
				auto offset = data.Length();
				SafePointer<DataBlock> enc = text.EncodeSequence(Encoding::UTF16, true);
				data.SetLength(offset + enc->Length());
				MemoryCopy(data.GetBuffer() + offset, enc->GetBuffer(), enc->Length());
				return offset;
			}
			template<class T> const T * ReadObjects(const DataBlock & data, uint32 at) { return reinterpret_cast<const T *>(data.GetBuffer() + at); }
			string ReadString(const DataBlock & data, uint32 offset) { return string(data.GetBuffer() + offset, -1, Encoding::UTF16); }
			void EncodeImports(DataBlock & info, DS::XI_Header & hdr, const Array<string> & list)
			{
				Array<uint32> names(0x10);
				names.SetLength(list.Length());
				hdr.import_list_offset = PreserveSpace(info, names.GetBuffer(), names.Length());
				hdr.import_list_size = list.Length();
				for (int i = 0; i < list.Length(); i++) names[i] = EnplaceString(info, list[i]);
				EnplaceObject(info, hdr.import_list_offset, names.GetBuffer(), names.Length());
			}
			void EncodeAttributes(DataBlock & info, uint32 & offset, uint32 & size, const Volumes::Dictionary<string, string> & src)
			{
				Array<DS::XI_Attribute> atable(1);
				atable.SetLength(src.Count());
				offset = PreserveSpace(info, atable.GetBuffer(), atable.Length());
				size = atable.Length();
				int wp = 0;
				for (auto & attr : src) {
					atable[wp].attribute_name_offset = EnplaceString(info, attr.key);
					atable[wp].attribute_value_offset = EnplaceString(info, attr.value);
					wp++;
				}
				EnplaceObject(info, offset, atable.GetBuffer(), atable.Length());
			}
			void EncodeLiteral(DataBlock & info, uint32 & offset, uint32 & size, const Module::Literal & src)
			{
				DS::XI_Literal hdr;
				offset = PreserveSpace(info, &hdr);
				size = sizeof(hdr);
				hdr.literal_class = uint32(src.contents);
				hdr.literal_size = src.length;
				if (src.contents == Module::Literal::Class::String) hdr.literal_data = EnplaceString(info, src.data_string);
				else hdr.literal_data = src.data_uint64;
				EncodeAttributes(info, hdr.literal_attribute_list_offset, hdr.literal_attribute_list_size, src.attributes);
				EnplaceObject(info, offset, &hdr);
			}
			void EncodeVariable(DataBlock & info, uint32 & offset, uint32 & size, const Module::Variable & src)
			{
				DS::XI_Variable hdr;
				offset = PreserveSpace(info, &hdr);
				size = sizeof(hdr);
				hdr.variable_type_cn_offset = EnplaceString(info, src.type_canonical_name);
				hdr.variable_offset_byte_size = src.offset.num_bytes;
				hdr.variable_offset_word_size = src.offset.num_words;
				hdr.variable_size_byte_size = src.size.num_bytes;
				hdr.variable_size_word_size = src.size.num_words;
				EncodeAttributes(info, hdr.variable_attribute_list_offset, hdr.variable_attribute_list_size, src.attributes);
				EnplaceObject(info, offset, &hdr);
			}
			void EncodeFunction(DataBlock & info, uint32 & offset, uint32 & size, const Module::Function & src)
			{
				DS::XI_Function hdr;
				offset = PreserveSpace(info, &hdr);
				size = sizeof(hdr);
				hdr.function_flags = src.code_flags;
				hdr.function_vft_table_index = src.vft_index.x;
				hdr.function_vft_function_index = src.vft_index.y;
				if (src.code && src.code->Length()) {
					hdr.function_code_offset = PreserveSpace(info, src.code->GetBuffer(), src.code->Length());
					hdr.function_code_size = src.code->Length();
					EnplaceObject(info, hdr.function_code_offset, src.code->GetBuffer(), src.code->Length());
				} else {
					hdr.function_code_offset = 0;
					hdr.function_code_size = 0;
				}
				EncodeAttributes(info, hdr.function_attribute_list_offset, hdr.function_attribute_list_size, src.attributes);
				EnplaceObject(info, offset, &hdr);
			}
			void EncodeProperty(DataBlock & info, uint32 & offset, uint32 & size, const Module::Property & src)
			{
				uint32 drain;
				DS::XI_Property hdr;
				offset = PreserveSpace(info, &hdr);
				size = sizeof(hdr);
				hdr.property_type_cn_offset = EnplaceString(info, src.type_canonical_name);
				hdr.property_getter_name_offset = EnplaceString(info, src.getter_name);
				hdr.property_setter_name_offset = EnplaceString(info, src.setter_name);
				EncodeAttributes(info, hdr.property_attribute_list_offset, hdr.property_attribute_list_size, src.attributes);
				EnplaceObject(info, offset, &hdr);
			}
			void EncodeInterface(DataBlock & info, DS::XI_Interface & dest, const Module::Interface & src)
			{
				dest.class_name_offset = EnplaceString(info, src.interface_name);
				dest.vftp_byte_offset = src.vft_pointer_offset.num_bytes;
				dest.vftp_word_offset = src.vft_pointer_offset.num_words;
			}
			void EncodeClass(DataBlock & info, uint32 & offset, uint32 & size, const Module::Class & src)
			{
				DS::XI_Class hdr;
				offset = PreserveSpace(info, &hdr);
				size = sizeof(hdr);
				hdr.class_language_semantics = uint32(src.class_nature);
				hdr.class_argument_semantics = uint32(src.instance_spec.semantics);
				hdr.class_byte_size = src.instance_spec.size.num_bytes;
				hdr.class_word_size = src.instance_spec.size.num_words;
				EncodeInterface(info, hdr.class_parent, src.parent_class);
				if (src.interfaces_implements.Length()) {
					Array<DS::XI_Interface> ilist(1);
					ilist.SetLength(src.interfaces_implements.Length());
					hdr.class_interface_list_offset = PreserveSpace(info, ilist.GetBuffer(), ilist.Length());
					hdr.class_interface_list_size = ilist.Length();
					for (int i = 0; i < ilist.Length(); i++) EncodeInterface(info, ilist[i], src.interfaces_implements[i]);
					EnplaceObject(info, hdr.class_interface_list_offset, ilist.GetBuffer(), ilist.Length());
				} else {
					hdr.class_interface_list_offset = 0;
					hdr.class_interface_list_size = 0;
				}
				Array<DS::XI_Symbol> stable(1);
				stable.SetLength(src.fields.Count() + src.properties.Count() + src.methods.Count());
				hdr.class_symbol_list_offset = PreserveSpace(info, stable.GetBuffer(), stable.Length());
				hdr.class_symbol_list_size = stable.Length();
				int wp = 0;
				for (auto & f : src.fields) {
					stable[wp].symbol_type = 3;
					stable[wp].symbol_name_offset = EnplaceString(info, f.key);
					EncodeVariable(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, f.value);
					wp++;
				}
				for (auto & p : src.properties) {
					stable[wp].symbol_type = 6;
					stable[wp].symbol_name_offset = EnplaceString(info, p.key);
					EncodeProperty(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, p.value);
					wp++;
				}
				for (auto & m : src.methods) {
					stable[wp].symbol_type = 4;
					stable[wp].symbol_name_offset = EnplaceString(info, m.key);
					EncodeFunction(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, m.value);
					wp++;
				}
				EnplaceObject(info, hdr.class_symbol_list_offset, stable.GetBuffer(), stable.Length());
				EncodeAttributes(info, hdr.class_attribute_list_offset, hdr.class_attribute_list_size, src.attributes);
				EnplaceObject(info, offset, &hdr);
			}
			void EncodePrototype(DataBlock & info, uint32 & offset, uint32 & size, const Module::Prototype & src)
			{
				DS::XI_Prototype hdr;
				offset = PreserveSpace(info, &hdr);
				size = sizeof(hdr);
				hdr.prototype_target_language_name_offset = EnplaceString(info, src.target_language);
				if (src.data && src.data->Length()) {
					hdr.prototype_data_offset = PreserveSpace(info, src.data->GetBuffer(), src.data->Length());
					hdr.prototype_data_size = src.data->Length();
					EnplaceObject(info, hdr.prototype_data_offset, src.data->GetBuffer(), src.data->Length());
				} else {
					hdr.prototype_data_offset = 0;
					hdr.prototype_data_size = 0;
				}
				EncodeAttributes(info, hdr.prototype_attribute_list_offset, hdr.prototype_attribute_list_size, src.attributes);
				EnplaceObject(info, offset, &hdr);
			}
			void EncodeSymbols(DataBlock & info, DS::XI_Header & hdr, const Module & src)
			{
				Array<DS::XI_Symbol> stable(1);
				stable.SetLength(src.literals.Count() + src.classes.Count() + src.variables.Count() + src.functions.Count() + src.aliases.Count() + src.prototypes.Count());
				hdr.symbol_list_offset = PreserveSpace(info, stable.GetBuffer(), stable.Length());
				hdr.symbol_list_size = stable.Length();
				int wp = 0;
				for (auto & l : src.literals) {
					stable[wp].symbol_type = 1;
					stable[wp].symbol_name_offset = EnplaceString(info, l.key);
					EncodeLiteral(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, l.value);
					wp++;
				}
				for (auto & c : src.classes) {
					stable[wp].symbol_type = 2;
					stable[wp].symbol_name_offset = EnplaceString(info, c.key);
					EncodeClass(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, c.value);
					wp++;
				}
				for (auto & v : src.variables) {
					stable[wp].symbol_type = 3;
					stable[wp].symbol_name_offset = EnplaceString(info, v.key);
					EncodeVariable(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, v.value);
					wp++;
				}
				for (auto & f : src.functions) {
					stable[wp].symbol_type = 4;
					stable[wp].symbol_name_offset = EnplaceString(info, f.key);
					EncodeFunction(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, f.value);
					wp++;
				}
				for (auto & a : src.aliases) {
					stable[wp].symbol_type = 5;
					stable[wp].symbol_name_offset = EnplaceString(info, a.key);
					stable[wp].symbol_data_offset = EnplaceString(info, a.value);
					stable[wp].symbol_data_size = 0;
					wp++;
				}
				for (auto & p : src.prototypes) {
					stable[wp].symbol_type = 7;
					stable[wp].symbol_name_offset = EnplaceString(info, p.key);
					EncodePrototype(info, stable[wp].symbol_data_offset, stable[wp].symbol_data_size, p.value);
					wp++;
				}
				EnplaceObject(info, hdr.symbol_list_offset, stable.GetBuffer(), stable.Length());
			}
			void EncodeResources(DataBlock & rsrc, DS::XI_Header & hdr, const Module & src)
			{
				Array<DS::XI_Resource> rlist(1);
				rlist.SetLength(src.resources.Count());
				hdr.resource_list_offset = PreserveSpace(rsrc, rlist.GetBuffer(), rlist.Length());
				hdr.resource_list_size = rlist.Length();
				auto current = src.resources.GetFirst();
				for (int i = 0; i < rlist.Length(); i++) {
					auto & ri = rlist[i];
					auto & rs = current->GetValue();
					auto rnp = rs.key.Split(L':');
					if (rnp.Length() != 2) throw InvalidArgumentException();
					SafePointer<DataBlock> tdata = rnp[0].EncodeSequence(Encoding::ANSI, false);
					ri.resource_type = 0;
					ri.resource_id = rnp[1].ToUInt32();
					MemoryCopy(&ri.resource_type, tdata->GetBuffer(), min(tdata->Length(), 4));
					ri.resource_data_offset = PreserveSpace(rsrc, rs.value->GetBuffer(), rs.value->Length());
					ri.resource_data_size = rs.value->Length();
					EnplaceObject(rsrc, ri.resource_data_offset, rs.value->GetBuffer(), rs.value->Length());
					current = current->GetNext();
				}
				EnplaceObject(rsrc, hdr.resource_list_offset, rlist.GetBuffer(), rlist.Length());
			}
			void DecodeImports(Module & dest, const DS::XI_Header & hdr, const DataBlock & info)
			{
				auto itable = ReadObjects<uint32>(info, hdr.import_list_offset);
				for (uint i = 0; i < hdr.import_list_size; i++) dest.modules_depends_on << ReadString(info, itable[i]);
			}
			void DecodeAttributes(Volumes::Dictionary<string, string> & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto atable = ReadObjects<DS::XI_Attribute>(info, offset);
				for (uint i = 0; i < size; i++) dest.Append(ReadString(info, atable[i].attribute_name_offset), ReadString(info, atable[i].attribute_value_offset));
			}
			void DecodeLiteral(Module::Literal & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto hdr = ReadObjects<DS::XI_Literal>(info, offset);
				dest.contents = static_cast<Module::Literal::Class>(hdr->literal_class);
				dest.length = hdr->literal_size;
				if (dest.contents == Module::Literal::Class::String) {
					dest.data_uint64 = 0;
					dest.data_string = ReadString(info, hdr->literal_data);
				} else dest.data_uint64 = hdr->literal_data;
				DecodeAttributes(dest.attributes, info, hdr->literal_attribute_list_offset, hdr->literal_attribute_list_size);
			}
			void DecodeVariable(Module::Variable & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto hdr = ReadObjects<DS::XI_Variable>(info, offset);
				dest.type_canonical_name = ReadString(info, hdr->variable_type_cn_offset);
				dest.offset.num_bytes = hdr->variable_offset_byte_size;
				dest.offset.num_words = hdr->variable_offset_word_size;
				dest.size.num_bytes = hdr->variable_size_byte_size;
				dest.size.num_words = hdr->variable_size_word_size;
				DecodeAttributes(dest.attributes, info, hdr->variable_attribute_list_offset, hdr->variable_attribute_list_size);
			}
			void DecodeFunction(Module::Function & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto hdr = ReadObjects<DS::XI_Function>(info, offset);
				dest.code_flags = hdr->function_flags;
				dest.vft_index.x = hdr->function_vft_table_index;
				dest.vft_index.y = hdr->function_vft_function_index;
				dest.code = new DataBlock(1);
				dest.code->SetLength(hdr->function_code_size);
				MemoryCopy(dest.code->GetBuffer(), info.GetBuffer() + hdr->function_code_offset, hdr->function_code_size);
				DecodeAttributes(dest.attributes, info, hdr->function_attribute_list_offset, hdr->function_attribute_list_size);
			}
			void DecodeProperty(Module::Property & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto hdr = ReadObjects<DS::XI_Property>(info, offset);
				dest.type_canonical_name = ReadString(info, hdr->property_type_cn_offset);
				dest.getter_name = ReadString(info, hdr->property_getter_name_offset);
				dest.setter_name = ReadString(info, hdr->property_setter_name_offset);
				DecodeAttributes(dest.attributes, info, hdr->property_attribute_list_offset, hdr->property_attribute_list_size);
			}
			void DecodeInterface(Module::Interface & dest, const DataBlock & info, const DS::XI_Interface & src)
			{
				dest.interface_name = ReadString(info, src.class_name_offset);
				dest.vft_pointer_offset.num_bytes = src.vftp_byte_offset;
				dest.vft_pointer_offset.num_words = src.vftp_word_offset;
			}
			void DecodeClass(Module::Class & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto hdr = ReadObjects<DS::XI_Class>(info, offset);
				dest.class_nature = static_cast<Module::Class::Nature>(hdr->class_language_semantics);
				dest.instance_spec.semantics = static_cast<XA::ArgumentSemantics>(hdr->class_argument_semantics);
				dest.instance_spec.size.num_bytes = hdr->class_byte_size;
				dest.instance_spec.size.num_words = hdr->class_word_size;
				DecodeInterface(dest.parent_class, info, hdr->class_parent);
				auto ihdr = ReadObjects<DS::XI_Interface>(info, hdr->class_interface_list_offset);
				for (uint i = 0; i < hdr->class_interface_list_size; i++) {
					Module::Interface mint;
					DecodeInterface(mint, info, ihdr[i]);
					dest.interfaces_implements.Append(mint);
				}
				auto stable = ReadObjects<DS::XI_Symbol>(info, hdr->class_symbol_list_offset);
				for (uint i = 0; i < hdr->class_symbol_list_size; i++) {
					auto & si = stable[i];
					auto name = ReadString(info, si.symbol_name_offset);
					if (si.symbol_type == 3) {
						Module::Variable var;
						DecodeVariable(var, info, si.symbol_data_offset, si.symbol_data_size);
						dest.fields.Append(name, var);
					} else if (si.symbol_type == 4) {
						Module::Function func;
						DecodeFunction(func, info, si.symbol_data_offset, si.symbol_data_size);
						dest.methods.Append(name, func);
					} else if (si.symbol_type == 6) {
						Module::Property prop;
						DecodeProperty(prop, info, si.symbol_data_offset, si.symbol_data_size);
						dest.properties.Append(name, prop);
					} else throw InvalidFormatException();
				}
				DecodeAttributes(dest.attributes, info, hdr->class_attribute_list_offset, hdr->class_attribute_list_size);
			}
			void DecodePrototype(Module::Prototype & dest, const DataBlock & info, uint32 offset, uint32 size)
			{
				auto hdr = ReadObjects<DS::XI_Prototype>(info, offset);
				dest.target_language = ReadString(info, hdr->prototype_target_language_name_offset);
				dest.data = new DataBlock(1);
				dest.data->SetLength(hdr->prototype_data_size);
				MemoryCopy(dest.data->GetBuffer(), info.GetBuffer() + hdr->prototype_data_offset, hdr->prototype_data_size);
				DecodeAttributes(dest.attributes, info, hdr->prototype_attribute_list_offset, hdr->prototype_attribute_list_size);
			}
			void DecodeSymbols(Module & dest, const DS::XI_Header & hdr, const DataBlock & info, Module::ModuleLoadFlags flags)
			{
				auto stable = ReadObjects<DS::XI_Symbol>(info, hdr.symbol_list_offset);
				for (uint i = 0; i < hdr.symbol_list_size; i++) {
					auto & si = stable[i];
					auto name = ReadString(info, si.symbol_name_offset);
					if (si.symbol_type == 1) {
						Module::Literal lit;
						DecodeLiteral(lit, info, si.symbol_data_offset, si.symbol_data_size);
						dest.literals.Append(name, lit);
					} else if (si.symbol_type == 2) {
						Module::Class cls;
						DecodeClass(cls, info, si.symbol_data_offset, si.symbol_data_size);
						dest.classes.Append(name, cls);
					} else if (si.symbol_type == 3) {
						Module::Variable var;
						DecodeVariable(var, info, si.symbol_data_offset, si.symbol_data_size);
						dest.variables.Append(name, var);
					} else if (si.symbol_type == 4) {
						Module::Function func;
						DecodeFunction(func, info, si.symbol_data_offset, si.symbol_data_size);
						dest.functions.Append(name, func);
					} else if (si.symbol_type == 5) {
						dest.aliases.Append(name, ReadString(info, si.symbol_data_offset));
					} else if (si.symbol_type == 7) {
						Module::Prototype proto;
						DecodePrototype(proto, info, si.symbol_data_offset, si.symbol_data_size);
						dest.prototypes.Append(name, proto);
					} else throw InvalidFormatException();
				}
			}
			void DecodeResources(Module & dest, const DS::XI_Header & hdr, const DataBlock & rsrc)
			{
				auto rtable = ReadObjects<DS::XI_Resource>(rsrc, hdr.resource_list_offset);
				for (uint i = 0; i < hdr.resource_list_size; i++) {
					auto & ri = rtable[i];
					auto key = string(&ri.resource_type, 4, Encoding::ANSI) + L":" + string(ri.resource_id);
					SafePointer<DataBlock> rd = new DataBlock(1);
					rd->SetLength(ri.resource_data_size);
					MemoryCopy(rd->GetBuffer(), rsrc.GetBuffer() + ri.resource_data_offset, ri.resource_data_size);
					dest.resources.Append(key, rd);
				}
			}

			void RestoreModule(Module & dest, Streaming::Stream * src, Module::ModuleLoadFlags flags)
			{
				DS::XI_Header hdr;
				src->Seek(0, Streaming::Begin);
				src->Read(&hdr, sizeof(hdr));
				if (MemoryCompare(&hdr.signature, "xximago", 8) || hdr.format_version) throw InvalidFormatException();
				dest.subsystem = static_cast<Module::ExecutionSubsystem>(hdr.target_subsystem);
				SafePointer<DataBlock> info, rsrc;
				if (flags == Module::ModuleLoadFlags::LoadAll || flags == Module::ModuleLoadFlags::LoadExecute || flags == Module::ModuleLoadFlags::LoadLink) {
					if (hdr.info_segment_size) {
						src->Seek(hdr.info_segment_offset, Streaming::Begin);
						info = src->ReadBlock(hdr.info_segment_size);
					}
				}
				if (flags == Module::ModuleLoadFlags::LoadAll || flags == Module::ModuleLoadFlags::LoadExecute) {
					if (hdr.data_segment_size) {
						src->Seek(hdr.data_segment_offset, Streaming::Begin);
						dest.data = src->ReadBlock(hdr.data_segment_size);
					}
				}
				if (flags == Module::ModuleLoadFlags::LoadAll || flags == Module::ModuleLoadFlags::LoadExecute || flags == Module::ModuleLoadFlags::LoadResources) {
					if (hdr.rsrc_segment_size) {
						src->Seek(hdr.rsrc_segment_offset, Streaming::Begin);
						rsrc = src->ReadBlock(hdr.rsrc_segment_size);
					}
				}
				if (info) {
					dest.module_import_name = ReadString(*info, hdr.module_name_offset);
					dest.assembler_name = ReadString(*info, hdr.module_assembler_offset);
					dest.assembler_version.major = hdr.module_assembler_version_major;
					dest.assembler_version.minor = hdr.module_assembler_version_minor;
					dest.assembler_version.subver = hdr.module_assembler_version_subversion;
					dest.assembler_version.build = hdr.module_assembler_version_build;
				} else {
					dest.assembler_version.major = dest.assembler_version.minor = 0;
					dest.assembler_version.subver = dest.assembler_version.build = 0;
				}
				if (info) {
					DecodeImports(dest, hdr, *info);
					DecodeSymbols(dest, hdr, *info, flags);
				}
				if (rsrc) DecodeResources(dest, hdr, *rsrc);
			}
			void EncodeModule(Streaming::Stream * dest, const Module & src)
			{
				DataBlock info(0x10000), rsrc(0x10000);
				DS::XI_Header hdr;
				MemoryCopy(&hdr.signature, "xximago", 8);
				hdr.format_version = 0;
				hdr.target_subsystem = uint(src.subsystem);
				hdr.module_name_offset = EnplaceString(info, src.module_import_name);
				hdr.module_assembler_offset = EnplaceString(info, src.assembler_name);
				hdr.module_assembler_version_major = src.assembler_version.major;
				hdr.module_assembler_version_minor = src.assembler_version.minor;
				hdr.module_assembler_version_subversion = src.assembler_version.subver;
				hdr.module_assembler_version_build = src.assembler_version.build;
				EncodeImports(info, hdr, src.modules_depends_on);
				EncodeSymbols(info, hdr, src);
				EncodeResources(rsrc, hdr, src);
				hdr.info_segment_size = info.Length();
				hdr.info_segment_offset = hdr.info_segment_size ? sizeof(hdr) : 0;
				hdr.data_segment_size = src.data ? src.data->Length() : 0;
				hdr.data_segment_offset = hdr.data_segment_size ? sizeof(hdr) + hdr.info_segment_size : 0;
				hdr.rsrc_segment_size = rsrc.Length();
				hdr.rsrc_segment_offset = hdr.rsrc_segment_size ? sizeof(hdr) + hdr.info_segment_size + hdr.data_segment_size : 0;
				dest->Write(&hdr, sizeof(hdr));
				if (hdr.info_segment_size) dest->Write(info.GetBuffer(), info.Length());
				if (hdr.data_segment_size) dest->Write(src.data->GetBuffer(), src.data->Length());
				if (hdr.rsrc_segment_size) dest->Write(rsrc.GetBuffer(), rsrc.Length());
			}
		}

		Module::TypeReference::TypeReference(const string & src) : _desc(src), _from(0) { _length = src.Length(); }
		Module::TypeReference::TypeReference(const string & src, int from, int length) : _desc(src), _from(from), _length(length) {}
		string Module::TypeReference::QueryCanonicalName(void) const { return _desc.Fragment(_from, _length); }
		Module::TypeReference::Class Module::TypeReference::GetReferenceClass(void) const
		{
			if (_length <= 0) return Class::Unknown;
			if (_desc[_from] == L'C') return Class::Class;
			else if (_desc[_from] == L'?') return Class::AbstractPlaceholder;
			else if (_desc[_from] == L'A') return Class::Array;
			else if (_desc[_from] == L'P') return Class::Pointer;
			else if (_desc[_from] == L'R') return Class::Reference;
			else if (_desc[_from] == L'F') return Class::Function;
			else if (_desc[_from] == L'I') return Class::AbstractInstance;
			else return Class::Unknown;
		}
		string Module::TypeReference::GetClassName(void) const
		{
			if (GetReferenceClass() == Class::Class) return _desc.Fragment(_from + 1, _length - 1);
			else throw InvalidStateException();
		}
		int Module::TypeReference::GetAbstractPlaceholderIndex(void) const
		{
			if (GetReferenceClass() == Class::AbstractPlaceholder) return _desc.Fragment(_from + 1, _length - 1).ToInt32();
			else throw InvalidStateException();
		}
		int Module::TypeReference::GetArrayVolume(void) const
		{
			if (GetReferenceClass() == Class::Array) {
				int i = 1;
				while (i < _length && _desc[_from + i] != L',') i++;
				return _desc.Fragment(_from + 1, i - 1).ToInt32();
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetArrayElement(void) const
		{
			if (GetReferenceClass() == Class::Array) {
				int i = 1;
				while (i < _length && _desc[_from + i] != L',') i++;
				if (i >= _length) throw InvalidFormatException();
				return TypeReference(_desc, _from + i + 1, _length - i - 1);
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetPointerDestination(void) const
		{
			if (GetReferenceClass() == Class::Pointer) return TypeReference(_desc, _from + 1, _length - 1);
			else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetReferenceDestination(void) const
		{
			if (GetReferenceClass() == Class::Reference) return TypeReference(_desc, _from + 1, _length - 1);
			else throw InvalidStateException();
		}
		Array<Module::TypeReference> * Module::TypeReference::GetFunctionSignature(void) const
		{
			if (GetReferenceClass() == Class::Function) {
				SafePointer< Array<TypeReference> > result = new Array<TypeReference>(0x10);
				int i = 1;
				while (i < _length) {
					while (i < _length && _desc[_from + i] != L'(') i++;
					if (i >= _length) break;
					i++;
					int s = i, l = 1;
					while (i < _length) {
						if (_desc[_from + i] == L'(') l++; else if (_desc[_from + i] == L')') { l--; if (!l) break; }
						i++;
					}
					if (l) throw InvalidFormatException();
					result->Append(TypeReference(_desc, _from + s, i - s));
					i++;
				}
				if (result->Length() == 0) throw InvalidFormatException();
				result->Retain();
				return result;
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetAbstractInstanceBase(void) const
		{
			if (GetReferenceClass() == Class::AbstractInstance) {
				int i = 1, n = 0;
				while (i < _length) {
					while (i < _length && _desc[_from + i] != L'(') i++;
					if (i >= _length) break;
					i++; n++;
					int s = i, l = 1;
					while (i < _length) {
						if (_desc[_from + i] == L'(') l++; else if (_desc[_from + i] == L')') { l--; if (!l) break; }
						i++;
					}
					if (l) throw InvalidFormatException();
					if (n == 1) return TypeReference(_desc, _from + s, i - s);
					i++;
				}
				throw InvalidFormatException();
			} else throw InvalidStateException();
		}
		int Module::TypeReference::GetAbstractInstanceParameterIndex(void) const
		{
			if (GetReferenceClass() == Class::AbstractInstance) {
				int i = 1;
				while (i < _length && _desc[_from + i] != L'(') i++;
				return _desc.Fragment(_from + 1, i - 1).ToInt32();
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetAbstractInstanceParameterType(void) const
		{
			if (GetReferenceClass() == Class::AbstractInstance) {
				int i = 1, n = 0;
				while (i < _length) {
					while (i < _length && _desc[_from + i] != L'(') i++;
					if (i >= _length) break;
					i++; n++;
					int s = i, l = 1;
					while (i < _length) {
						if (_desc[_from + i] == L'(') l++; else if (_desc[_from + i] == L')') { l--; if (!l) break; }
						i++;
					}
					if (l) throw InvalidFormatException();
					if (n == 2) return TypeReference(_desc, _from + s, i - s);
					i++;
				}
				throw InvalidFormatException();
			} else throw InvalidStateException();
		}
		string Module::TypeReference::MakeClassReference(const string & class_name) { return L"C" + class_name; }
		string Module::TypeReference::MakeAbstractPlaceholder(int param_index) { return L"?" + string(param_index); }
		string Module::TypeReference::MakeArray(const string & cn, int volume) { return L"A" + string(volume) + L"," + cn; }
		string Module::TypeReference::MakePointer(const string & cn) { return L"P" + cn; }
		string Module::TypeReference::MakeReference(const string & cn) { return L"R" + cn; }
		string Module::TypeReference::MakeFunction(const string & rv_cn, const Array<string> * args_cn)
		{
			DynamicString result;
			result << L"F(" << rv_cn << L")";
			if (args_cn) for (auto & a : *args_cn) result << L"(" << a << L")";
			return result.ToString();
		}
		string Module::TypeReference::MakeInstance(const string & cn_of, int param_index, const string & cn_with) { return L"I" + string(param_index) + L"(" + cn_of + L")(" + cn_with + L")"; }
		string Module::TypeReference::ToString(void) const
		{
			auto cls = GetReferenceClass();
			if (cls == Class::Class) return GetClassName();
			else if (cls == Class::AbstractPlaceholder) return L"?" + string(GetAbstractPlaceholderIndex());
			else if (cls == Class::Array) return GetArrayElement().ToString() + L" [" + string(GetArrayVolume()) + L"]";
			else if (cls == Class::Pointer) return GetPointerDestination().ToString() + L" PTR";
			else if (cls == Class::Reference) return GetReferenceDestination().ToString() + L" REF";
			else if (cls == Class::Function) {
				SafePointer< Array<TypeReference> > sign = GetFunctionSignature();
				DynamicString result;
				result << L"(";
				for (int i = 1; i < sign->Length(); i++) { if (i > 1) result << L", "; result << sign->ElementAt(i).ToString(); }
				result << L") -> (" << sign->ElementAt(0).ToString() << L")";
				return result.ToString();
			} else if (cls == Class::AbstractInstance) {
				DynamicString result;
				result << GetAbstractInstanceBase().ToString();
				result << L" WITH ?" << string(GetAbstractInstanceParameterIndex()) << L" = (";
				result << GetAbstractInstanceParameterType().ToString() << L")";
				return result.ToString();
			} else return L"UNKNOWN";
		}

		Module::Class::Class(void) : interfaces_implements(0x10) {}

		Module::Module(void) : modules_depends_on(0x10)
		{
			subsystem = ExecutionSubsystem::ConsoleUI;
			module_import_name = L"module";
			assembler_name = L"XI";
			assembler_version.major = assembler_version.minor = 0;
			assembler_version.subver = assembler_version.build = 0;
		}
		Module::Module(Streaming::Stream * source) : Module(source, ModuleLoadFlags::LoadAll) {}
		Module::Module(Streaming::Stream * source, ModuleLoadFlags flags) : modules_depends_on(0x10) { Format::RestoreModule(*this, source, flags); }
		Module::~Module(void) {}
		void Module::Save(Streaming::Stream * dest) { Format::EncodeModule(dest, *this); }
	}
}