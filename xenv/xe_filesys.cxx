#include "xe_filesys.h"

#include "xe_interfaces.h"

#include <PlatformSpecific/UnixFileAccess.h>

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::DirectoryAlreadyExistsException & e) { ectx.error_code = 6; ectx.error_subcode = Engine::IO::Error::FileExists; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XE
	{
		class XFileStream : public XStream
		{
			SafePointer<Streaming::FileStream> _stream;
		public:
			XFileStream(Streaming::FileStream * stream) { _stream.SetRetain(stream); }
			virtual ~XFileStream(void) override {}
			virtual string ToString(void) const override { try { return _stream->ToString(); } catch (...) { return L""; } }
			virtual int Read(void * data, int length, ErrorContext & ectx) noexcept override
			{
				try { _stream->Read(data, length); return length; }
				catch (IO::FileReadEndOfFileException & e) { return e.DataRead; }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
				return 0;
			}
			virtual void Write(const void * data, int length, ErrorContext & ectx) noexcept override
			{
				try { _stream->Write(data, length); }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			virtual int64 Seek(int64 to, int origin, ErrorContext & ectx) noexcept override
			{
				try {
					if (origin == 0) return _stream->Seek(to, Streaming::Begin);
					else if (origin == 1) return _stream->Seek(to, Streaming::Current);
					else if (origin == 2) return _stream->Seek(to, Streaming::End);
					else throw InvalidArgumentException();
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
				return 0;
			}
			virtual int64 GetLength(ErrorContext & ectx) noexcept override
			{
				try { return _stream->Length(); }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
				return 0;
			}
			virtual void SetLength(const int64 & length, ErrorContext & ectx) noexcept override
			{
				try { _stream->SetLength(length); }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			virtual void Flush(void) noexcept override { try { _stream->Flush(); } catch (...) {} }
			virtual bool IsXV(void) noexcept override { return false; }
			virtual uint64 GetCreationTime(ErrorContext & ectx) noexcept { XE_TRY_INTRO return IO::DateTime::GetFileCreationTime(_stream->Handle()).Ticks; XE_TRY_OUTRO(0) }
			virtual void SetCreationTime(const Time & time, ErrorContext & ectx) noexcept { XE_TRY_INTRO IO::DateTime::SetFileCreationTime(_stream->Handle(), time); XE_TRY_OUTRO() }
			virtual uint64 GetAccessTime(ErrorContext & ectx) noexcept { XE_TRY_INTRO return IO::DateTime::GetFileAccessTime(_stream->Handle()).Ticks; XE_TRY_OUTRO(0) }
			virtual void SetAccessTime(const Time & time, ErrorContext & ectx) noexcept { XE_TRY_INTRO IO::DateTime::SetFileAccessTime(_stream->Handle(), time); XE_TRY_OUTRO() }
			virtual uint64 GetAlterTime(ErrorContext & ectx) noexcept { XE_TRY_INTRO return IO::DateTime::GetFileAlterTime(_stream->Handle()).Ticks; XE_TRY_OUTRO(0) }
			virtual void SetAlterTime(const Time & time, ErrorContext & ectx) noexcept { XE_TRY_INTRO IO::DateTime::SetFileAlterTime(_stream->Handle(), time); XE_TRY_OUTRO() }
			virtual int GetUserAccess(ErrorContext & ectx) noexcept { XE_TRY_INTRO return IO::Unix::GetFileUserAccessRights(_stream->Handle()); XE_TRY_OUTRO(0) }
			virtual void SetUserAccess(const int & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto value_1 = IO::Unix::GetFileGroupAccessRights(_stream->Handle());
				auto value_2 = IO::Unix::GetFileOtherAccessRights(_stream->Handle());
				IO::Unix::SetFileAccessRights(_stream->Handle(), value, value_1, value_2);
				XE_TRY_OUTRO()
			}
			virtual int GetGroupAccess(ErrorContext & ectx) noexcept { XE_TRY_INTRO return IO::Unix::GetFileGroupAccessRights(_stream->Handle()); XE_TRY_OUTRO(0) }
			virtual void SetGroupAccess(const int & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto value_1 = IO::Unix::GetFileUserAccessRights(_stream->Handle());
				auto value_2 = IO::Unix::GetFileOtherAccessRights(_stream->Handle());
				IO::Unix::SetFileAccessRights(_stream->Handle(), value_1, value, value_2);
				XE_TRY_OUTRO()
			}
			virtual int GetCommonAccess(ErrorContext & ectx) noexcept { XE_TRY_INTRO return IO::Unix::GetFileOtherAccessRights(_stream->Handle()); XE_TRY_OUTRO(0) }
			virtual void SetCommonAccess(const int & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto value_1 = IO::Unix::GetFileUserAccessRights(_stream->Handle());
				auto value_2 = IO::Unix::GetFileGroupAccessRights(_stream->Handle());
				IO::Unix::SetFileAccessRights(_stream->Handle(), value_1, value_2, value);
				XE_TRY_OUTRO()
			}
		};
		class IFileSystemExtension
		{
		public:
			virtual SafePointer< Array<string> > GetArguments(ErrorContext & ectx) noexcept = 0;
			virtual string ExpandPath(const string & value, ErrorContext & ectx) noexcept = 0;
			virtual string ExpandPathRelative(const string & base, const string & value, ErrorContext & ectx) noexcept = 0;
			virtual string GetWorkingDirectory(ErrorContext & ectx) noexcept = 0;
			virtual void SetWorkingDirectory(const string & value, ErrorContext & ectx) noexcept = 0;
			virtual string GetExecutablePath(ErrorContext & ectx) noexcept = 0;
			virtual string GetParentPath(const string & value, ErrorContext & ectx) noexcept = 0;
			virtual string GetPathPart(const string & value, int part, ErrorContext & ectx) noexcept = 0;
			virtual SafePointer< Array<IO::Search::Volume> > GetVolumes(ErrorContext & ectx) noexcept = 0;
			virtual uint64 GetVolumeMemory(const string & vol, int index, ErrorContext & ectx) noexcept = 0;
			virtual SafePointer< Array<string> > GetFiles(const string & at, const string & filter, int mode, ErrorContext & ectx) noexcept = 0;
			virtual bool CheckFileStatus(const string & path, int mode) noexcept = 0;
			virtual void OpenExternally(const string & path, int mode, ErrorContext & ectx) noexcept = 0;
			virtual SafePointer<XFileStream> CreateFile(const string & path, int access, int open, ErrorContext & ectx) noexcept = 0;
			virtual void CreateDirectory(const string & path, int mode, ErrorContext & ectx) noexcept = 0;
			virtual void CreateLink(const string & path, const string & value, int mode, ErrorContext & ectx) noexcept = 0;
			virtual string ReadLink(const string & path, ErrorContext & ectx) noexcept = 0;
			virtual void Move(const string & path, const string & dest, int mode, ErrorContext & ectx) noexcept = 0;
			virtual void Remove(const string & path, int mode, ErrorContext & ectx) noexcept = 0;
		};
		class FileSystemExtension : public IAPIExtension, public IFileSystemExtension
		{
			string _exec_path;
			Array<string> _args;
		public:
			FileSystemExtension(const string & exec_path, const string * argv, int argc) : _exec_path(exec_path), _args(argc) { _args.Append(argv, argc); }
			virtual ~FileSystemExtension(void) override {}
			void CopyAttributes(handle from, handle to)
			{
				IO::Unix::SetFileAccessRights(to, IO::Unix::GetFileUserAccessRights(from), IO::Unix::GetFileGroupAccessRights(from), IO::Unix::GetFileOtherAccessRights(from));
				IO::DateTime::SetFileCreationTime(to, IO::DateTime::GetFileCreationTime(from));
				IO::DateTime::SetFileAlterTime(to, IO::DateTime::GetFileAlterTime(from));
				IO::DateTime::SetFileAccessTime(to, IO::DateTime::GetFileAccessTime(from));
			}
			void Copy(const string & from, const string & to, int mode)
			{
				if (mode < 1 || mode > 4) throw InvalidArgumentException();
				auto from_full = IO::ExpandPath(from);
				auto to_full = IO::ExpandPath(to);
				if (mode == 1) {
					if (CheckFileStatus(to_full, 0)) IO::RemoveFile(to_full);
					else if (CheckFileStatus(to_full, 1)) IO::RemoveEntireDirectory(to_full);
				}
				if (CheckFileStatus(from_full, 0)) {
					if (mode == 4) {
						Streaming::FileStream from_stream(from_full, Streaming::AccessRead, Streaming::OpenExisting);
						Streaming::FileStream to_stream(to_full, Streaming::AccessWrite, Streaming::CreateNew);
						from_stream.CopyTo(&to_stream);
						CopyAttributes(from_stream.Handle(), to_stream.Handle());
					} else if (mode == 3) {
						Streaming::FileStream from_stream(from_full, Streaming::AccessRead, Streaming::OpenExisting);
						try {
							Streaming::FileStream to_stream(to_full, Streaming::AccessWrite, Streaming::CreateNew);
							from_stream.CopyTo(&to_stream);
							CopyAttributes(from_stream.Handle(), to_stream.Handle());
						} catch (...) {}
					} else {
						Streaming::FileStream from_stream(from_full, Streaming::AccessRead, Streaming::OpenExisting);
						Streaming::FileStream to_stream(to_full, Streaming::AccessWrite, Streaming::CreateAlways);
						from_stream.CopyTo(&to_stream);
						CopyAttributes(from_stream.Handle(), to_stream.Handle());
					}
				} else if (CheckFileStatus(from_full, 1)) {
					SafePointer< Array<string> > files = IO::Search::GetFiles(from_full + L"/*");
					SafePointer< Array<string> > dirs = IO::Search::GetDirectories(from_full + L"/*");
					try { IO::CreateDirectory(to_full); } catch (...) {}
					for (auto & obj : *files) Copy(from_full + L"/" + obj, to_full + L"/" + obj, mode);
					for (auto & obj : *dirs) Copy(from_full + L"/" + obj, to_full + L"/" + obj, mode);
				}
			}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override { return 0; }
			virtual const void * ExposeInterface(const string & interface) noexcept override
			{
				if (interface == L"syslim") return static_cast<IFileSystemExtension *>(this);
				else return 0;
			}
			virtual SafePointer< Array<string> > GetArguments(ErrorContext & ectx) noexcept override { XE_TRY_INTRO return new Array<string>(_args); XE_TRY_OUTRO(0) }
			virtual string ExpandPath(const string & value, ErrorContext & ectx) noexcept override { XE_TRY_INTRO return IO::ExpandPath(value); XE_TRY_OUTRO(L"") }
			virtual string ExpandPathRelative(const string & base, const string & value, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				#ifdef ENGINE_WINDOWS
					if (value.Length() > 1 && value[1] == L':') return IO::ExpandPath(value);
					else return IO::ExpandPath(base + L"\\" + value);
				#else
					if (value[0] == L'/') return IO::ExpandPath(value);
					else return IO::ExpandPath(base + L"/" + value);
				#endif
				XE_TRY_OUTRO(L"")
			}
			virtual string GetWorkingDirectory(ErrorContext & ectx) noexcept override { XE_TRY_INTRO return IO::GetCurrentDirectory(); XE_TRY_OUTRO(L"") }
			virtual void SetWorkingDirectory(const string & value, ErrorContext & ectx) noexcept override { XE_TRY_INTRO IO::SetCurrentDirectory(value); XE_TRY_OUTRO() }
			virtual string GetExecutablePath(ErrorContext & ectx) noexcept override { XE_TRY_INTRO return _exec_path; XE_TRY_OUTRO(L"") }
			virtual string GetParentPath(const string & value, ErrorContext & ectx) noexcept override { XE_TRY_INTRO return IO::Path::GetDirectory(value); XE_TRY_OUTRO(L"") }
			virtual string GetPathPart(const string & value, int part, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (part == 0) return IO::Path::GetFileName(value);
				else if (part == 1) return IO::Path::GetFileNameWithoutExtension(value);
				else if (part == 2) return IO::Path::GetExtension(value);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO(L"")
			}
			virtual SafePointer< Array<IO::Search::Volume> > GetVolumes(ErrorContext & ectx) noexcept override { XE_TRY_INTRO return IO::Search::GetVolumes(); XE_TRY_OUTRO(0) }
			virtual uint64 GetVolumeMemory(const string & vol, int index, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				uint64 memory = 0;
				IO::GetVolumeSpace(vol, (index == 0) ? &memory : 0, (index == 1) ? &memory : 0, (index == 2) ? &memory : 0);
				return memory;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< Array<string> > GetFiles(const string & at, const string & filter, int mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				SafePointer< Array<string> > result;
				if (mode == 0) result = IO::Search::GetFiles(at + L"/" + filter, false);
				else if (mode == 1) result = IO::Search::GetFiles(at + L"/" + filter, true);
				else if (mode == 2) result = IO::Search::GetDirectories(at + L"/" + filter);
				else throw InvalidArgumentException();
				for (auto & r : result->Elements()) r = IO::ExpandPath(at + L"/" + r);
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual bool CheckFileStatus(const string & path, int mode) noexcept override
			{
				try {
					auto type = IO::GetFileType(path);
					if (type == IO::FileType::Regular && mode == 0) return true;
					else if (type == IO::FileType::Directory && mode == 1) return true;
					else if (type == IO::FileType::SymbolicLink && mode == 2) return true;
					else return false;
				} catch (...) { return false; }
			}
			virtual void OpenExternally(const string & path, int mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode == 0) Shell::ShowInBrowser(path, false);
				else if (mode == 1) Shell::OpenFile(path);
				else if (mode == 2) Shell::ShowInBrowser(path, true);
				else if (mode == 3) Shell::OpenCommandPrompt(path);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual SafePointer<XFileStream> CreateFile(const string & path, int access, int open, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				Streaming::FileAccess access_value;
				Streaming::FileCreationMode creation_value;
				if (access & ~0x06) throw InvalidArgumentException();
				else if (!(access & 0x06)) throw InvalidArgumentException();
				if (access == 0x02) access_value = Streaming::FileAccess::AccessWrite;
				else if (access == 0x04) access_value = Streaming::FileAccess::AccessRead;
				else if (access == 0x06) access_value = Streaming::FileAccess::AccessReadWrite;
				else throw InvalidArgumentException();
				if (open == 1) creation_value = Streaming::FileCreationMode::CreateAlways;
				else if (open == 2) creation_value = Streaming::FileCreationMode::CreateNew;
				else if (open == 3) creation_value = Streaming::FileCreationMode::OpenAlways;
				else if (open == 4) creation_value = Streaming::FileCreationMode::OpenExisting;
				else if (open == 5) creation_value = Streaming::FileCreationMode::TruncateExisting;
				else throw InvalidArgumentException();
				SafePointer<Streaming::FileStream> stream = new Streaming::FileStream(path, access_value, creation_value);
				return new XFileStream(stream);
				XE_TRY_OUTRO(0)
			}
			virtual void CreateDirectory(const string & path, int mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode == 0) IO::CreateDirectory(path);
				else if (mode == 1) IO::CreateDirectoryTree(path);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual void CreateLink(const string & path, const string & value, int mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode == 0) IO::CreateSymbolicLink(path, value);
				else if (mode == 1) IO::CreateHardLink(path, value);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual string ReadLink(const string & path, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				return IO::GetSymbolicLinkDestination(path);
				XE_TRY_OUTRO(L"")
			}
			virtual void Move(const string & path, const string & dest, int mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode) Copy(path, dest, mode); else IO::MoveFile(path, dest);
				XE_TRY_OUTRO()
			}
			virtual void Remove(const string & path, int mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode == 0) IO::RemoveFile(path);
				else if (mode == 1) IO::RemoveDirectory(path);
				else if (mode == 2) IO::RemoveEntireDirectory(path);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
		};

		void ActivateFileIO(StandardLoader & ldr, const string & exec_path, const string * argv, int argc)
		{
			SafePointer<FileSystemExtension> ext = new FileSystemExtension(exec_path, argv, argc);
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
	}
}