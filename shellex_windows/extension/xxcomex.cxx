#include <EngineRuntime.h>

#include "../../ximg/xi_module.h"
#include "../../ximg/xi_resources.h"

#include <Windows.h>
#include <shlobj.h>

#undef InterlockedIncrement
#undef InterlockedDecrement

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::Codec;
using namespace Engine::Graphics;
using namespace Engine::XI;

namespace Engine { namespace Windows { HICON _create_icon(Codec::Frame * frame); } }

const uint8 self_guid[] = { 0xA1, 0x94, 0x8B, 0xFB, 0x80, 0x4D, 0x7A, 0xCA, 0xE3, 0x42, 0x75, 0x92, 0xE1, 0x46, 0x7C, 0x0D };

uint global_ref_count = 0;
HMODULE self_module;

struct FilePropertyInfo
{
	FMTID fmtid;
	uint pid;
	string value;
};
class EngineStreamWrapper : public Stream
{
	IStream * _inner;
public:
	EngineStreamWrapper(IStream * inner) : _inner(inner) { _inner->AddRef(); }
	virtual ~EngineStreamWrapper(void) override { _inner->Release(); }
	virtual void Read(void * buffer, uint32 length) override
	{
		ULONG count;
		if (_inner->Read(buffer, length, &count) != S_OK) throw IO::FileAccessException(IO::Error::Unknown);
		if (count != length) throw IO::FileReadEndOfFileException(count);
	}
	virtual void Write(const void * data, uint32 length) override { throw IO::FileAccessException(IO::Error::AccessDenied); }
	virtual int64 Seek(int64 position, SeekOrigin origin) override
	{
		LARGE_INTEGER to;
		ULARGE_INTEGER result;
		to.QuadPart = position;
		DWORD mode;
		if (origin == SeekOrigin::Begin) mode = STREAM_SEEK_SET;
		else if (origin == SeekOrigin::Current) mode = STREAM_SEEK_CUR;
		else if (origin == SeekOrigin::End) mode = STREAM_SEEK_END;
		else throw InvalidArgumentException();
		if (_inner->Seek(to, mode, &result) != S_OK) throw IO::FileAccessException(IO::Error::Unknown);
		return result.QuadPart;
	}
	virtual uint64 Length(void) override
	{
		STATSTG stat;
		if (_inner->Stat(&stat, STATFLAG_NONAME) != S_OK) throw IO::FileAccessException(IO::Error::Unknown);
		return stat.cbSize.QuadPart;
	}
	virtual void SetLength(uint64 length) override { throw IO::FileAccessException(IO::Error::AccessDenied); }
	virtual void Flush(void) override {}
};
class ExtensionFileInterface : public IExtractIconW, public IPersistFile, public IPropertyStore, public IInitializeWithStream, public IPropertyStoreCapabilities
{
	uint _ref_cnt;
	string _path;
	SafePointer<Module> _module;
	SafePointer< Array<FilePropertyInfo> > _metadata;
private:
	void _internal_init(Stream * from)
	{
		_module.SetReference(0);
		_metadata = new Array<FilePropertyInfo>(0x10);
		try {
			_module = new Module(from, Module::ModuleLoadFlags::LoadResources);
			SafePointer< Volumes::Dictionary<string, string> > metadata = LoadModuleMetadata(_module->resources);
			if (metadata) {
				auto value = metadata->GetElementByKey(MetadataKeyModuleName);
				if (value) {
					FilePropertyInfo info;
					info.fmtid = FMTID_SummaryInformation;
					info.pid = PIDSI_TITLE;
					info.value = *value;
					_metadata->Append(info);
				} else {
					FilePropertyInfo info;
					info.fmtid = FMTID_SummaryInformation;
					info.pid = PIDSI_TITLE;
					info.value = _module->module_import_name;
					_metadata->Append(info);
				}
				value = metadata->GetElementByKey(MetadataKeyCompanyName);
				if (value) {
					FilePropertyInfo info;
					info.fmtid = FMTID_SummaryInformation;
					info.pid = PIDSI_AUTHOR;
					info.value = *value;
					_metadata->Append(info);
				}
				value = metadata->GetElementByKey(MetadataKeyCopyright);
				if (value) {
					FilePropertyInfo info;
					info.fmtid = FMTID_MediaFileSummaryInformation;
					info.pid = PIDMSI_COPYRIGHT;
					info.value = *value;
					_metadata->Append(info);
				}
				value = metadata->GetElementByKey(MetadataKeyVersion);
				if (value) {
					FilePropertyInfo info;
					info.fmtid = FMTID_DocSummaryInformation;
					info.pid = 29;
					info.value = *value;
					_metadata->Append(info);
				}
			} else {
				FilePropertyInfo info;
				info.fmtid = FMTID_SummaryInformation;
				info.pid = PIDSI_TITLE;
				info.value = _module->module_import_name;
				_metadata->Append(info);
			}
			FilePropertyInfo info;
			info.fmtid = FMTID_SummaryInformation;
			info.pid = PIDSI_APPNAME;
			info.value = FormatString(L"%0 %1.%2.%3.%4", _module->assembler_name, _module->assembler_version.major, _module->assembler_version.minor,
				_module->assembler_version.subver, _module->assembler_version.build);
			_metadata->Append(info);
		} catch (...) {}
	}
public:
	ExtensionFileInterface(void) : _ref_cnt(1) { InterlockedIncrement(global_ref_count); }
	~ExtensionFileInterface(void) { InterlockedDecrement(global_ref_count); }
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** result) override
	{
		if (!result) return E_POINTER;
		*result = 0;
		if (MemoryCompare(&iid, &IID_IUnknown, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IUnknown *>(static_cast<IPersistFile *>(this));
			return S_OK;
		} else if (MemoryCompare(&iid, &IID_IPersist, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IPersist *>(this);
			return S_OK;
		} else if (MemoryCompare(&iid, &IID_IPersistFile, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IPersistFile *>(this);
			return S_OK;
		} else if (MemoryCompare(&iid, &IID_IExtractIconW, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IExtractIconW *>(this);
			return S_OK;
		} else if (MemoryCompare(&iid, &IID_IPropertyStore, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IPropertyStore *>(this);
			return S_OK;
		} else if (MemoryCompare(&iid, &IID_IPropertyStoreCapabilities, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IPropertyStoreCapabilities *>(this);
			return S_OK;
		} else if (MemoryCompare(&iid, &IID_IInitializeWithStream, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IInitializeWithStream *>(this);
			return S_OK;
		} else return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override { return InterlockedIncrement(_ref_cnt); }
	virtual ULONG STDMETHODCALLTYPE Release(void) override { auto ref = InterlockedDecrement(_ref_cnt); if (!ref) delete this; return ref; }
	// IPersist
	virtual HRESULT STDMETHODCALLTYPE GetClassID(LPCLSID clsid) override
	{
		if (!clsid) return E_POINTER;
		MemoryCopy(clsid, &self_guid, sizeof(CLSID));
		return S_OK;
	}
	// IInitializeWithStream
	virtual HRESULT STDMETHODCALLTYPE Initialize(IStream * stream, DWORD mode) override
	{
		if (_metadata) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
		try {
			_path = L"";
			SafePointer<Stream> input = new EngineStreamWrapper(stream);
			_internal_init(input);
			return S_OK;
		} catch (...) { _path = L""; _module.SetReference(0); _metadata.SetReference(0); return E_OUTOFMEMORY; }
	}
	// IPersistFile
	virtual HRESULT STDMETHODCALLTYPE IsDirty(void) override { return S_FALSE; }
	virtual HRESULT STDMETHODCALLTYPE Load(LPCOLESTR file_name, DWORD mode) override
	{
		try {
			_path = string(file_name);
			SafePointer<Stream> stream = new FileStream(_path, AccessRead, OpenExisting);
			_internal_init(stream);
			return S_OK;
		} catch (...) { _path = L""; _module.SetReference(0); _metadata.SetReference(0);return E_FAIL; }
	}
	virtual HRESULT STDMETHODCALLTYPE Save(LPCOLESTR file_name, BOOL remember) override { return S_FALSE; }
	virtual HRESULT STDMETHODCALLTYPE SaveCompleted(LPCOLESTR file_name) override { return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE GetCurFile(LPOLESTR * file_name) override
	{
		if (!file_name) return E_POINTER;
		*file_name = 0;
		IMalloc * allocator;
		auto status = CoGetMalloc(1, &allocator);
		if (status != S_OK) return status;
		auto memory = reinterpret_cast<LPOLESTR>(allocator->Alloc(_path.GetEncodedLength(Encoding::UTF16) * 2 + 2));
		allocator->Release();
		if (!memory) return E_OUTOFMEMORY;
		_path.Encode(memory, Encoding::UTF16, true);
		*file_name = memory;
		return _path.Length() ? S_OK : S_FALSE;
	}
	// IExtractIconW
	virtual HRESULT STDMETHODCALLTYPE GetIconLocation(UINT flags, PWSTR icon_file, UINT icon_file_max, int * index, UINT * out_flags) override
	{
		if (!icon_file || !index || !out_flags) return E_POINTER;
		*out_flags = GIL_PERINSTANCE | GIL_DONTCACHE | GIL_NOTFILENAME | GIL_FORCENOSHIELD;
		*index = 0;
		if (icon_file_max) icon_file[0] = 0;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Extract(PCWSTR icon_file, UINT index, HICON * icon_large, HICON * icon_small, UINT size) override
	{
		HICON sml = 0, lrg = 0;
		try {
			if (icon_small) {
				int size_small = HIWORD(size);
				SafePointer<Frame> frame = _module ? LoadModuleIcon(_module->resources, 1, Point(size_small, size_small)) : 0;
				if (frame) {
					sml = Windows::_create_icon(frame);
					if (!sml) throw Exception();
				} else {
					sml = reinterpret_cast<HICON>(LoadImageW(self_module, MAKEINTRESOURCEW(1), IMAGE_ICON, size_small, size_small, 0));
					if (!sml) throw Exception();
				}
			}
			if (icon_large) {
				int size_large = LOWORD(size);
				SafePointer<Frame> frame = _module ? LoadModuleIcon(_module->resources, 1, Point(size_large, size_large)) : 0;
				if (frame) {
					lrg = Windows::_create_icon(frame);
					if (!lrg) throw Exception();
				} else {
					lrg = reinterpret_cast<HICON>(LoadImageW(self_module, MAKEINTRESOURCEW(1), IMAGE_ICON, size_large, size_large, 0));
					if (!lrg) throw Exception();
				}
			}
		} catch (...) {
			if (sml) DestroyIcon(sml);
			if (lrg) DestroyIcon(lrg);
			return S_FALSE;
		}
		if (icon_small) *icon_small = sml;
		if (icon_large) *icon_large = lrg;
		return S_OK;
	}
	// IPropertyStore
	virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD * cProps) override
	{
		if (cProps) *cProps = _metadata ? _metadata->Length() : 0;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE GetAt(DWORD iProp, PROPERTYKEY * pkey) override
	{
		if (!pkey) return E_POINTER;
		ZeroMemory(pkey, sizeof(PROPERTYKEY));
		if (!_metadata) return E_INVALIDARG;
		if (iProp >= _metadata->Length()) return E_INVALIDARG;
		pkey->fmtid = _metadata->ElementAt(iProp).fmtid;
		pkey->pid = _metadata->ElementAt(iProp).pid;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key, PROPVARIANT * pv) override
	{
		if (pv) {
			PropVariantInit(pv);
			for (auto & p : *_metadata) if (p.fmtid == key.fmtid && p.pid == key.pid) {
				auto memory = CoTaskMemAlloc(p.value.GetEncodedLength(Encoding::UTF16) * 2 + 2);
				if (memory) {
					pv->vt = VT_LPWSTR;
					pv->pwszVal = reinterpret_cast<LPWSTR>(memory);
					p.value.Encode(memory, Encoding::UTF16, true);
				}
				break;
			}
		}
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override { return STG_E_ACCESSDENIED; }
	virtual HRESULT STDMETHODCALLTYPE Commit(void) override { return STG_E_ACCESSDENIED; }
	// IPropertyStoreCapabilities
	virtual HRESULT STDMETHODCALLTYPE IsPropertyWritable(REFPROPERTYKEY key) override { return S_FALSE; }
};
class ExtensionClassFactory : public IClassFactory
{
	uint _ref_cnt;
public:
	ExtensionClassFactory(void) : _ref_cnt(1) { InterlockedIncrement(global_ref_count); }
	~ExtensionClassFactory(void) { InterlockedDecrement(global_ref_count); }
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** result) override
	{
		if (!result) return E_POINTER;
		*result = 0;
		if (MemoryCompare(&iid, &IID_IClassFactory, sizeof(IID)) == 0 || MemoryCompare(&iid, &IID_IUnknown, sizeof(IID)) == 0) {
			AddRef();
			*result = static_cast<IClassFactory *>(this);
			return S_OK;
		} else return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override { return InterlockedIncrement(_ref_cnt); }
	virtual ULONG STDMETHODCALLTYPE Release(void) override { auto ref = InterlockedDecrement(_ref_cnt); if (!ref) delete this; return ref; }
	// IClassFactory
	virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown * outer, REFIID iid, void ** result) override
	{
		if (outer) return CLASS_E_NOAGGREGATION;
		if (!result) return E_POINTER;
		*result = 0;
		ExtensionFileInterface * instance;
		try { instance = new ExtensionFileInterface; } catch (...) { return E_OUTOFMEMORY; }
		auto status = instance->QueryInterface(iid, result);
		instance->Release();
		return status;
	}
	virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
	{
		if (lock) InterlockedIncrement(global_ref_count);
		else InterlockedDecrement(global_ref_count);
		return S_OK;
	}
};

void LibraryLoaded(void)
{
	InitializeDefaultCodecs();
	self_module = GetModuleHandleW(ENGINE_VI_APPSYSNAME);
}
void LibraryUnloaded(void) {}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void ** result)
{
	if (!result) return E_INVALIDARG;
	if (MemoryCompare(&clsid, &self_guid, sizeof(CLSID)) == 0) {
		auto classobj = new (std::nothrow) ExtensionClassFactory;
		if (!classobj) return E_OUTOFMEMORY;
		auto status = classobj->QueryInterface(iid, result);
		classobj->Release();
		return status;
	} else return CLASS_E_CLASSNOTAVAILABLE;
}
STDAPI DllCanUnloadNow(void) { return global_ref_count ? S_FALSE : S_OK; }

#ifdef ENGINE_X64
#pragma comment(linker, "/export:DllGetClassObject")
#pragma comment(linker, "/export:DllCanUnloadNow")
#else
#pragma comment(linker, "/export:DllGetClassObject=_DllGetClassObject@12" )
#pragma comment(linker, "/export:DllCanUnloadNow=_DllCanUnloadNow@0" )
#endif