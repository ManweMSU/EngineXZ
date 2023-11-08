#include "xe_rtff.h"

#include "xe_interfaces.h"
#include "xe_conapi.h"

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XE
	{
		class RegistryValue : public DynamicObject
		{
			SafePointer<Storage::RegistryNode> _node;
			string _path;
			uint64 _value_data;
			string _value_string;
		public:
			RegistryValue(Storage::RegistryNode * node, const string & path) : _path(path), _value_data(0) { _node.SetRetain(node); }
			virtual ~RegistryValue(void) override {}
			virtual string ToString(void) const override { try { return _node->GetValueString(_path); } catch (...) { return L""; } }
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				XE_TRY_INTRO
				if (cls->GetClassName() == L"int32") {
					*reinterpret_cast<int32 *>(&_value_data) = _node->GetValueInteger(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"frac") {
					*reinterpret_cast<float *>(&_value_data) = _node->GetValueFloat(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"logicum") {
					*reinterpret_cast<bool *>(&_value_data) = _node->GetValueBoolean(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"linea") {
					_value_string = _node->GetValueString(_path);
					return &_value_string;
				} else if (cls->GetClassName() == L"int64") {
					*reinterpret_cast<int64 *>(&_value_data) = _node->GetValueLongInteger(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"dfrac") {
					*reinterpret_cast<double *>(&_value_data) = _node->GetValueLongFloat(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"imago.color") {
					*reinterpret_cast<Color *>(&_value_data) = _node->GetValueColor(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"tempus") {
					*reinterpret_cast<Time *>(&_value_data) = _node->GetValueTime(_path);
					return &_value_data;
				} else if (cls->GetClassName() == L"@praeformae.I0(Cdordo)(Cnint8)") {
					SafePointer<DataBlock> data = new DataBlock(1);
					data->SetLength(_node->GetValueBinarySize(_path));
					_node->GetValueBinary(_path, data->GetBuffer());
					data->Retain();
					return data;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
				XE_TRY_OUTRO(0)
			}
			virtual void * GetType(void) noexcept override { return 0; }
		};
		class RegistryNode : public Object
		{
			SafePointer<Storage::RegistryNode> _node;
		public:
			RegistryNode(Storage::RegistryNode * node) { _node.SetRetain(node); }
			virtual ~RegistryNode(void) override {}
			virtual Array<string> GetSubnodes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _node->GetSubnodes();
				XE_TRY_OUTRO(Array<string>(1))
			}
			virtual Array<string> GetValues(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _node->GetValues();
				XE_TRY_OUTRO(Array<string>(1))
			}
			virtual void CreateNode(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->CreateNode(name);
				XE_TRY_OUTRO()
			}
			virtual void RemoveNode(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->RemoveNode(name);
				XE_TRY_OUTRO()
			}
			virtual void RenameNode(const string & name, const string & to, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->RenameNode(name, to);
				XE_TRY_OUTRO()
			}
			virtual SafePointer<RegistryNode> OpenNode(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Storage::RegistryNode> node = _node->OpenNode(name);
				if (!node) throw InvalidArgumentException();
				SafePointer<RegistryNode> wrapper = new RegistryNode(node);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			virtual void CreateValueA(const string & name, Storage::RegistryValueType type, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->CreateValue(name, type);
				XE_TRY_OUTRO()
			}
			virtual void CreateValueB(const string & name, const ClassSymbol * cls, ErrorContext & ectx) noexcept
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
				XE_TRY_INTRO
				if (cls->GetClassName() == L"int32") _node->CreateValue(name, Storage::RegistryValueType::Integer);
				else if (cls->GetClassName() == L"frac") _node->CreateValue(name, Storage::RegistryValueType::Float);
				else if (cls->GetClassName() == L"logicum") _node->CreateValue(name, Storage::RegistryValueType::Boolean);
				else if (cls->GetClassName() == L"linea") _node->CreateValue(name, Storage::RegistryValueType::String);
				else if (cls->GetClassName() == L"int64") _node->CreateValue(name, Storage::RegistryValueType::LongInteger);
				else if (cls->GetClassName() == L"dfrac") _node->CreateValue(name, Storage::RegistryValueType::LongFloat);
				else if (cls->GetClassName() == L"imago.color") _node->CreateValue(name, Storage::RegistryValueType::Color);
				else if (cls->GetClassName() == L"tempus") _node->CreateValue(name, Storage::RegistryValueType::Time);
				else if (cls->GetClassName() == L"@praeformae.I0(Cdordo)(Cnint8)") _node->CreateValue(name, Storage::RegistryValueType::Binary);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual void RemoveValue(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->RemoveValue(name);
				XE_TRY_OUTRO()
			}
			virtual void RenameValue(const string & name, const string & to, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->RenameValue(name, to);
				XE_TRY_OUTRO()
			}
			virtual Storage::RegistryValueType GetValueType(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _node->GetValueType(name);
				XE_TRY_OUTRO(Storage::RegistryValueType::Unknown)
			}
			virtual SafePointer<DynamicObject> ReadValue(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<DynamicObject> result = new RegistryValue(_node, name);
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual void WriteValueA(const string & path, int value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueB(const string & path, float value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueC(const string & path, bool value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueD(const string & path, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueE(const string & path, const uint * value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, string(value, -1, Encoding::UTF32));
				XE_TRY_OUTRO()
			}
			virtual void WriteValueF(const string & path, int64 value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueG(const string & path, double value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueH(const string & path, const Color & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueI(const string & path, const Time & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueJ(const string & path, const void * value, int size, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value, size);
				XE_TRY_OUTRO()
			}
			virtual void WriteValueK(const string & path, const DataBlock & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_node->SetValue(path, value.GetBuffer(), value.Length());
				XE_TRY_OUTRO()
			}
			Storage::RegistryNode * GetInnerNode(void) noexcept { return _node; }
		};
		class Registry : public RegistryNode
		{
			SafePointer<Storage::Registry> _registry;
		public:
			Registry(Storage::Registry * registry) : RegistryNode(registry) { _registry.SetRetain(registry); }
			virtual ~Registry(void) override {}
			virtual void Save(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				_registry->Save(stream);
				XE_TRY_OUTRO()
			}
			virtual void SaveTextA(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				Storage::RegistryToText(_registry, stream);
				XE_TRY_OUTRO()
			}
			virtual void SaveTextB(XStream * xstream, Encoding encoding, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				Storage::RegistryToText(_registry, stream, encoding);
				XE_TRY_OUTRO()
			}
			virtual void SaveTextC(XTextEncoder * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto text = SaveTextD(ectx);
				if (ectx.error_code) return;
				xstream->Write(text, ectx);
				XE_TRY_OUTRO()
			}
			virtual string SaveTextD(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return Storage::RegistryToText(_registry);
				XE_TRY_OUTRO(L"")
			}
			virtual SafePointer<Registry> Clone(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Storage::Registry> copy = _registry->Clone();
				SafePointer<Registry> wrapper = new Registry(copy);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
		};
		class Compressor : public Object
		{
		public:
			struct CompressionDesc
			{
				uint chain;
				uint quality;
				uint block;
				int num_threads;
			};
		private:
			CompressionDesc _desc;
			SafePointer<ThreadPool> _pool;
		public:
			Compressor(const CompressionDesc & desc) : _desc(desc) { if (_desc.num_threads != 1) _pool = new ThreadPool(_desc.num_threads); }
			virtual ~Compressor(void) override {}
			virtual void GetInformation(CompressionDesc & desc) noexcept { desc = _desc; }
			virtual void Process(XStream * xstream_to, XStream * xstream_from, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> to = WrapFromXStream(xstream_to);
				SafePointer<Streaming::Stream> from = WrapFromXStream(xstream_from);
				if (_desc.chain && _desc.block) {
					Storage::CompressionQuality qual;
					if (_desc.quality == 0) qual = Storage::CompressionQuality::Force;
					else if (_desc.quality == 1) qual = Storage::CompressionQuality::Optional;
					else if (_desc.quality == 2) qual = Storage::CompressionQuality::Sequential;
					else if (_desc.quality == 3) qual = Storage::CompressionQuality::Variative;
					else if (_desc.quality == 4) qual = Storage::CompressionQuality::ExtraVariative;
					else throw InvalidStateException();
					Storage::ChainCompress(to, from, _desc.chain, qual, _pool, _desc.block);
				} else Storage::ChainDecompress(to, from, _pool);
				XE_TRY_OUTRO()
			}
		};
		class ArchiveFile : public Object
		{
			SafePointer<Storage::Archive> _arc;
			Storage::ArchiveFile _file;
		public:
			ArchiveFile(Storage::Archive * arc, Storage::ArchiveFile file) : _file(file) { _arc.SetRetain(arc); }
			virtual ~ArchiveFile(void) override {}
			virtual SafePointer<XStream> Open(uint stream_mode, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream;
				if (stream_mode == 0) stream = _arc->QueryFileStream(_file, Storage::ArchiveStream::MetadataBased);
				else if (stream_mode == 1) stream = _arc->QueryFileStream(_file, Storage::ArchiveStream::Native);
				else if (stream_mode == 2) stream = _arc->QueryFileStream(_file, Storage::ArchiveStream::ForceDecompressed);
				else throw InvalidArgumentException();
				SafePointer<XStream> xstream = WrapToXStream(stream);
				return xstream;
				XE_TRY_OUTRO(0)
			}
			virtual uint GetIndex(void) noexcept { return _file; }
			virtual string GetName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileName(_file);
				XE_TRY_OUTRO(L"")
			}
			virtual string GetType(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileType(_file);
				XE_TRY_OUTRO(L"")
			}
			virtual uint GetID(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileID(_file);
				XE_TRY_OUTRO(0)
			}
			virtual uint GetUser(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileCustomData(_file);
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetCreationDate(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileCreationTime(_file);
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetAccessDate(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileAccessTime(_file);
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetAlternationDate(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->GetFileAlterTime(_file);
				XE_TRY_OUTRO(0)
			}
			virtual bool IsCompressed(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _arc->IsFileCompressed(_file);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<Object> GetAttributes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Volumes::Dictionary<string, string> > dict = new Volumes::Dictionary<string, string>;
				SafePointer< Array<string> > keys = _arc->GetFileAttributes(_file);
				for (auto & k : *keys) dict->Append(k, _arc->GetFileAttribute(_file, k));
				SafePointer<Object> result = CreateMetadataDictionary(dict);
				return result;
				XE_TRY_OUTRO(0)
			}
		};
		class Archive : public Object
		{
			SafePointer<Storage::Archive> _arc;
		public:
			Archive(XStream * xstream, bool load_meta)
			{
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				_arc = Storage::OpenArchive(stream, load_meta ? Storage::ArchiveMetadataUsage::LoadMetadata : Storage::ArchiveMetadataUsage::IgnoreMetadata);
				if (!_arc) throw InvalidFormatException();
			}
			virtual ~Archive(void) override {}
			virtual uint IteratorFirst(void) noexcept { return 1; }
			virtual uint IteratorLast(void) noexcept { return _arc->GetFileCount(); }
			virtual uint IteratorPreFirst(void) noexcept { return 0; }
			virtual uint IteratorPastLast(void) noexcept { return _arc->GetFileCount() + 1; }
			virtual int GetFileCount(void) noexcept { return _arc->GetFileCount(); }
			virtual bool HasMetadata(void) noexcept { return _arc->HasMetadata(); }
			virtual SafePointer<ArchiveFile> GetFileA(uint index, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (index < 1 || index > _arc->GetFileCount()) throw InvalidArgumentException();
				SafePointer<ArchiveFile> file = new ArchiveFile(_arc, index);
				return file;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<ArchiveFile> GetFileB(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto file = _arc->FindArchiveFile(name);
				if (!file) throw IO::FileAccessException(IO::Error::FileNotFound);
				return GetFileA(file, ectx);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<ArchiveFile> GetFileC(const string & type, uint id, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto file = _arc->FindArchiveFile(type, id);
				if (!file) throw IO::FileAccessException(IO::Error::FileNotFound);
				return GetFileA(file, ectx);
				XE_TRY_OUTRO(0)
			}
		};
		class NewArchive : public Object
		{
		public:
			struct NewArchiveDesc
			{
				int file_count;
				uint format;
			};
			struct FileDesc
			{
				string name;
				string type;
				uint id;
				uint user;
				Time date_created;
				Time date_accessed;
				Time date_alternated;
				const string * attributes;
				XStream * data_stream;
				const void * data;
				const Compressor::CompressionDesc * compression;
				int attribute_count;
				int data_length;
				uint mask;
			};
		private:
			NewArchiveDesc _desc;
			SafePointer<Storage::NewArchive> _arc;
			SafePointer<ThreadPool> _pool;
		public:
			NewArchive(XStream * xstream, const NewArchiveDesc & desc) : _desc(desc)
			{
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				_arc = Storage::CreateArchive(stream, _desc.file_count, _desc.format);
			}
			virtual ~NewArchive(void) override {}
			virtual void GetInformation(NewArchiveDesc & desc) noexcept { desc = _desc; }
			virtual void WriteFile(uint index, const FileDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_arc) throw InvalidStateException();
				if (index < 1 || index > uint(_desc.file_count)) throw InvalidArgumentException();
				if (desc.mask & 0x0003) {
					SafePointer<Streaming::Stream> input;
					if (desc.mask & 0x0001) {
						if (desc.mask & 0x0002) throw InvalidArgumentException();
						if (!desc.data || !desc.data_length) throw InvalidArgumentException();
						input = new Streaming::MemoryStream(desc.data, desc.data_length);
					} else {
						if (!desc.data_stream) throw InvalidArgumentException();
						input = WrapFromXStream(desc.data_stream);
					}
					if (desc.mask & 0x0400) _arc->SetFileCompressionFlag(index, true);
					if (desc.mask & 0x0200) {
						if (desc.mask & 0x0400) throw InvalidArgumentException();
						if (!desc.compression || !desc.compression->block || !desc.compression->chain) throw InvalidArgumentException();
						if (desc.compression->num_threads != 1 && !_pool) _pool = new ThreadPool(desc.compression->num_threads);
						Storage::CompressionQuality qual;
						if (desc.compression->quality == 0) qual = Storage::CompressionQuality::Force;
						else if (desc.compression->quality == 1) qual = Storage::CompressionQuality::Optional;
						else if (desc.compression->quality == 2) qual = Storage::CompressionQuality::Sequential;
						else if (desc.compression->quality == 3) qual = Storage::CompressionQuality::Variative;
						else if (desc.compression->quality == 4) qual = Storage::CompressionQuality::ExtraVariative;
						else throw InvalidArgumentException();
						_arc->SetFileData(index, input, desc.compression->chain, qual, _pool, desc.compression->block);
					} else _arc->SetFileData(index, input);
				} else if (desc.mask & 0x0600) throw InvalidArgumentException();
				if (desc.mask & 0x0004) _arc->SetFileName(index, desc.name);
				if (desc.mask & 0x0008) {
					_arc->SetFileType(index, desc.type);
					_arc->SetFileID(index, desc.id);
				}
				if (desc.mask & 0x0010) _arc->SetFileCustom(index, desc.user);
				if (desc.mask & 0x0020) _arc->SetFileCreationTime(index, desc.date_created);
				if (desc.mask & 0x0040) _arc->SetFileAccessTime(index, desc.date_accessed);
				if (desc.mask & 0x0080) _arc->SetFileAlterTime(index, desc.date_alternated);
				if (desc.mask & 0x0100) {
					if (!desc.attributes || desc.attribute_count <= 0) throw InvalidArgumentException();
					for (int i = 0; i < desc.attribute_count; i++) _arc->SetFileAttribute(index, desc.attributes[2 * i], desc.attributes[2 * i + 1]);
				}
				XE_TRY_OUTRO()
			}
			virtual void Finalize(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_arc) throw InvalidStateException();
				_arc->Finalize();
				_arc.SetReference(0);
				_pool.SetReference(0);
				XE_TRY_OUTRO()
			}
		};
		class StringTable : public Object
		{
			SafePointer<Storage::StringTable> _table;
		public:
			StringTable(XStream * xstream) { SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream); _table = new Storage::StringTable(stream); }
			StringTable(void) { _table = new Storage::StringTable; }
			virtual ~StringTable(void) override {}
			virtual const string * Read(int index, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return &_table->GetString(index);
				XE_TRY_OUTRO(0)
			}
			virtual void Add(const string & text, int index, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_table->AddString(text, index);
				XE_TRY_OUTRO()
			}
			virtual void Remove(int index, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_table->RemoveString(index);
				XE_TRY_OUTRO()
			}
			virtual SafePointer< Array<int> > GetIndex(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<int> > result = _table->GetIndex();
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual void Save(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				_table->Save(stream);
				XE_TRY_OUTRO()
			}
		};
		class FileFormatExtension : public IAPIExtension
		{
			static SafePointer<RegistryNode> _registry_merge(RegistryNode ** nodes, int count, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				ObjectArray<Storage::RegistryNode> list(count);
				for (int i = 0; i < count; i++) list.Append(nodes[i]->GetInnerNode());
				SafePointer<Storage::RegistryNode> node = Storage::CreateMergedNode(list);
				if (!node) throw OutOfMemoryException();
				SafePointer<RegistryNode> wrapper = new RegistryNode(node);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_allocate(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Storage::Registry> base = Storage::CreateRegistry();
				if (!base) throw OutOfMemoryException();
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_create_from_node(RegistryNode * node, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Storage::Registry> registry = Storage::CreateRegistryFromNode(node->GetInnerNode());
				SafePointer<Registry> wrapper = new Registry(registry);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_load_1(XStream * stream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> input = WrapFromXStream(stream);
				SafePointer<Storage::Registry> base = Storage::LoadRegistry(input);
				if (!base) {
					input->Seek(0, Streaming::Begin);
					base = Storage::CompileTextRegistry(input);
					if (!base) throw InvalidFormatException();
				}
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_load_2(XStream * stream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> input = WrapFromXStream(stream);
				SafePointer<Storage::Registry> base = Storage::CompileTextRegistry(input);
				if (!base) throw InvalidFormatException();
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_load_3(XStream * stream, uint encoding, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> input = WrapFromXStream(stream);
				SafePointer<Storage::Registry> base = Storage::CompileTextRegistry(input, static_cast<Encoding>(encoding));
				if (!base) throw InvalidFormatException();
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_load_4(XTextDecoder * stream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto code = stream->ReadAll(ectx);
				if (ectx.error_code) return 0;
				SafePointer<Storage::Registry> base = Storage::CompileTextRegistry(code);
				if (!base) throw InvalidFormatException();
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_load_5(const string & code, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Storage::Registry> base = Storage::CompileTextRegistry(code);
				if (!base) throw InvalidFormatException();
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Registry> _registry_load_6(XStream * stream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> input = WrapFromXStream(stream);
				SafePointer<Storage::Registry> base = Storage::LoadRegistry(input);
				if (!base) throw InvalidFormatException();
				SafePointer<Registry> wrapper = new Registry(base);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Compressor> _compressor_allocate_1(const Compressor::CompressionDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Compressor> com = new Compressor(desc);
				return com;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<Compressor> _compressor_allocate_2(int num_threads, ErrorContext & ectx) noexcept
			{
				Compressor::CompressionDesc desc;
				desc.chain = desc.block = desc.quality = 0;
				desc.num_threads = num_threads;
				return _compressor_allocate_1(desc, ectx);
			}
			static SafePointer<Archive> _archive_load_1(XStream * xstream, ErrorContext & ectx) noexcept { return _archive_load_2(xstream, true, ectx); }
			static SafePointer<Archive> _archive_load_2(XStream * xstream, bool load_meta, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Archive> arc = new Archive(xstream, load_meta);
				return arc;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<NewArchive> _archive_allocate(XStream * xstream, const NewArchive::NewArchiveDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<NewArchive> arc = new NewArchive(xstream, desc);
				return arc;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<StringTable> _string_table_allocate(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<StringTable> result = new StringTable;
				return result;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<StringTable> _string_table_load(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<StringTable> result = new StringTable(xstream);
				return result;
				XE_TRY_OUTRO(0)
			}
		public:
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"fo_oa2") < 0) {
					if (string::Compare(routine_name, L"fo_at1") < 0) {
						if (string::Compare(routine_name, L"fo_ac2") < 0) {
							if (string::Compare(routine_name, L"fo_ac1") < 0) {
								if (string::Compare(routine_name, L"fo_aa1") == 0) return reinterpret_cast<const void *>(&_archive_allocate);
							} else {
								if (string::Compare(routine_name, L"fo_ac1") == 0) return reinterpret_cast<const void *>(&_compressor_allocate_1);
							}
						} else {
							if (string::Compare(routine_name, L"fo_al") < 0) {
								if (string::Compare(routine_name, L"fo_ac2") == 0) return reinterpret_cast<const void *>(&_compressor_allocate_2);
							} else {
								if (string::Compare(routine_name, L"fo_al") == 0) return reinterpret_cast<const void *>(&_string_table_allocate);
							}
						}
					} else {
						if (string::Compare(routine_name, L"fo_mt") < 0) {
							if (string::Compare(routine_name, L"fo_at2") < 0) {
								if (string::Compare(routine_name, L"fo_at1") == 0) return reinterpret_cast<const void *>(&_registry_allocate);
							} else {
								if (string::Compare(routine_name, L"fo_at2") == 0) return reinterpret_cast<const void *>(&_registry_create_from_node);
							}
						} else {
							if (string::Compare(routine_name, L"fo_oa1") < 0) {
								if (string::Compare(routine_name, L"fo_mt") == 0) return reinterpret_cast<const void *>(&_registry_merge);
							} else {
								if (string::Compare(routine_name, L"fo_oa1") == 0) return reinterpret_cast<const void *>(&_archive_load_1);
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"fo_ot3") < 0) {
						if (string::Compare(routine_name, L"fo_ot1") < 0) {
							if (string::Compare(routine_name, L"fo_ol") < 0) {
								if (string::Compare(routine_name, L"fo_oa2") == 0) return reinterpret_cast<const void *>(&_archive_load_2);
							} else {
								if (string::Compare(routine_name, L"fo_ol") == 0) return reinterpret_cast<const void *>(&_string_table_load);
							}
						} else {
							if (string::Compare(routine_name, L"fo_ot2") < 0) {
								if (string::Compare(routine_name, L"fo_ot1") == 0) return reinterpret_cast<const void *>(&_registry_load_1);
							} else {
								if (string::Compare(routine_name, L"fo_ot2") == 0) return reinterpret_cast<const void *>(&_registry_load_2);
							}
						}
					} else {
						if (string::Compare(routine_name, L"fo_ot5") < 0) {
							if (string::Compare(routine_name, L"fo_ot4") < 0) {
								if (string::Compare(routine_name, L"fo_ot3") == 0) return reinterpret_cast<const void *>(&_registry_load_3);
							} else {
								if (string::Compare(routine_name, L"fo_ot4") == 0) return reinterpret_cast<const void *>(&_registry_load_4);
							}
						} else {
							if (string::Compare(routine_name, L"fo_ot6") < 0) {
								if (string::Compare(routine_name, L"fo_ot5") == 0) return reinterpret_cast<const void *>(&_registry_load_5);
							} else {
								if (string::Compare(routine_name, L"fo_ot6") == 0) return reinterpret_cast<const void *>(&_registry_load_6);
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		void ActivateFileFormatIO(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new FileFormatExtension;
			if (!ldr.RegisterAPIExtension(ext)) throw InvalidStateException();
		}
	}
}