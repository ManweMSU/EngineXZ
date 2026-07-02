#include "xe_commem.h"

#include "xe_interfaces.h"
#include "xe_tryblock.h"

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