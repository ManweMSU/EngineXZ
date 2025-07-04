#include "xe_commem.h"

#include "xe_interfaces.h"

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
		class XSharedMemory : public Object
		{
			SafePointer<IPC::ISharedMemory> _mem;
		public:
			XSharedMemory(const string & name, uint length, bool create_new)
			{
				uint error;
				_mem = IPC::CreateSharedMemory(name, length, create_new ? IPC::SharedMemoryCreateNew : IPC::SharedMemoryOpenExisting, &error);
				if (!_mem) {
					if (error == IPC::ErrorAlreadyExists) throw IO::FileAccessException(IO::Error::FileExists);
					else if (error == IPC::ErrorDoesNotExist) throw IO::FileAccessException(IO::Error::FileNotFound);
					else if (error == IPC::ErrorInvalidArgument) throw InvalidArgumentException();
					else if (error == IPC::ErrorBadFileName) throw IO::FileAccessException(IO::Error::BadPathName);
					else if (error == IPC::ErrorAllocation) throw OutOfMemoryException();
					else throw IO::FileAccessException(IO::Error::Unknown);
				}
			}
			virtual ~XSharedMemory(void) override {}
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _mem->GetSegmentName(); XE_TRY_OUTRO(L""); }
			virtual uint GetLength(void) noexcept { return _mem->GetLength(); }
			virtual bool Map(void ** address, bool read_mode, bool write_mode) noexcept
			{
				uint mode = 0;
				if (read_mode) mode |= IPC::SharedMemoryMapRead;
				if (write_mode) mode |= IPC::SharedMemoryMapWrite;
				return _mem->Map(address, mode);
			}
			virtual void Unmap(void) noexcept { return _mem->Unmap(); }
		};
		class SharedMemoryExtension : public IAPIExtension
		{
			static SafePointer<XSharedMemory> _create_shared_memory(const string & name, uint length, bool create_new, ErrorContext & ectx) noexcept { XE_TRY_INTRO return new XSharedMemory(name, length, create_new); XE_TRY_OUTRO(0) }
		public:
			SharedMemoryExtension(void) {}
			virtual ~SharedMemoryExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"commem") == 0) return reinterpret_cast<const void *>(&_create_shared_memory);
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		void ActivateSharedMemory(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new SharedMemoryExtension;
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
	}
}