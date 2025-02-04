#include "../Interfaces/SystemIO.h"

#define _DARWIN_FEATURE_ONLY_64_BIT_INODE

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <mach-o/dyld.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/param.h>

namespace Engine
{
	namespace IO
	{
		uint PosixErrorToEngineError(int code)
		{
			if (code == 0) return Error::Success;
			else if (code == EACCES) return Error::AccessDenied;
			else if (code == EDQUOT) return Error::NoDiskSpace;
			else if (code == EEXIST) return Error::FileExists;
			else if (code == EISDIR) return Error::FileExists;
			else if (code == EMFILE) return Error::TooManyOpenFiles;
			else if (code == ENAMETOOLONG) return Error::FileNameTooLong;
			else if (code == ENFILE) return Error::NoDiskSpace;
			else if (code == ENOENT) return Error::FileNotFound;
			else if (code == ENOSPC) return Error::NoDiskSpace;
			else if (code == ENOTDIR) return Error::PathNotFound;
			else if (code == EOPNOTSUPP) return Error::NotImplemented;
			else if (code == EROFS) return Error::IsReadOnly;
			else if (code == ETXTBSY) return Error::AccessDenied;
			else if (code == EILSEQ) return Error::BadPathName;
			else if (code == EBADF) return Error::InvalidHandle;
			else if (code == EINVAL) return Error::NotImplemented;
			else if (code == ENOTEMPTY) return Error::DirectoryNotEmpty;
			else if (code == ENOTDIR) return Error::PathNotFound;
			else if (code == ENOTSUP) return Error::NotImplemented;
			else if (code == EPERM) return Error::AccessDenied;
			else if (code == EXDEV) return Error::NotSameDevice;
			else if (code == ENOBUFS) return Error::NotEnoughMemory;
			else if (code == ENOMEM) return Error::NotEnoughMemory;
			else if (code == ENXIO) return Error::InvalidDevice;
			else if (code == EFBIG) return Error::FileTooLarge;
			else if (code == EBUSY) return Error::AccessDenied;
			return Error::Unknown;
		}
		handle CreateFile(const string & path, Streaming::FileAccess access, Streaming::FileCreationMode mode)
		{
			SafePointer<Array<uint8> > Path = Path::NormalizePath(path).EncodeSequence(Encoding::UTF8, true);
			int flags = 0;
			if (access == Streaming::AccessRead) flags = O_RDONLY;
			else if (access == Streaming::AccessWrite) flags = O_WRONLY;
			else if (access == Streaming::AccessReadWrite) flags = O_RDWR;
			else if (access == Streaming::AccessNo) flags = O_RDONLY;
			else throw InvalidArgumentException();
			int result = -1;
			if (mode == Streaming::CreateNew) flags |= O_CREAT | O_EXCL;
			else if (mode == Streaming::CreateAlways) flags |= O_CREAT | O_TRUNC;
			else if (mode == Streaming::OpenAlways) flags |= O_CREAT;
			else if (mode == Streaming::TruncateExisting) flags |= O_TRUNC;
			else if (mode == Streaming::OpenExisting);
			else throw InvalidArgumentException();
			do {
				result = open(reinterpret_cast<char *>(Path->GetBuffer()), flags, 0777);
				if (result == -1 && errno != EINTR) throw FileAccessException(PosixErrorToEngineError(errno));
			} while (result == -1);
			struct stat file_stat;
			if (fstat(result, &file_stat) == -1) {
				auto e = errno;
				close(result);
				throw FileAccessException(PosixErrorToEngineError(e));
			}
			if ((file_stat.st_mode & S_IFMT) == S_IFDIR) {
				close(result);
				throw FileAccessException(Error::AccessDenied);
			}
			int lf;
			if (access == Streaming::AccessWrite || access == Streaming::AccessReadWrite) lf = LOCK_EX | LOCK_NB;
			else if (access == Streaming::AccessRead) lf = LOCK_SH | LOCK_NB;
			if (flock(result, lf) == -1) {
				close(result);
				throw FileAccessException(Error::AccessDenied);
			}
			return handle(result);
		}
		void CreatePipe(handle * pipe_in, handle * pipe_out)
		{
			int result[2];
			if (pipe(result) == -1) throw FileAccessException(PosixErrorToEngineError(errno));
			*pipe_in = reinterpret_cast<handle>(result[1]);
			*pipe_out = reinterpret_cast<handle>(result[0]);
		}
		handle CloneHandle(handle file)
		{
			if (file == InvalidHandle) return InvalidHandle;
			int new_file = dup(reinterpret_cast<intptr>(file));
			if (new_file == -1) throw FileAccessException(PosixErrorToEngineError(errno));
			return handle(new_file);
		}
		void CloseHandle(handle file)
		{
			if (file == InvalidHandle) return;
			close(reinterpret_cast<intptr>(file));
		}
		void ReadFile(handle file, void * to, uint32 amount)
		{
			do {
				auto Read = read(reinterpret_cast<intptr>(file), to, amount);
				if (Read == -1 && errno != EINTR) throw FileAccessException(PosixErrorToEngineError(errno));
				if (Read < amount) throw FileReadEndOfFileException(Read);
				else if (Read == amount) return;
			} while (true);
		}
		void WriteFile(handle file, const void * data, uint32 amount)
		{
			do {
				auto Written = write(reinterpret_cast<intptr>(file), data, amount);
				if ((Written == -1 && errno != EINTR) || Written != amount) throw FileAccessException(PosixErrorToEngineError(errno));
				else if (Written == amount) return;
			} while (true);
		}
		int64 Seek(handle file, int64 position, Streaming::SeekOrigin origin)
		{
			int org = SEEK_SET;
			if (origin == Streaming::Current) org = SEEK_CUR;
			else if (origin == Streaming::End) org = SEEK_END;
			auto result = lseek(reinterpret_cast<intptr>(file), position, org);
			if (result == -1) throw FileAccessException(PosixErrorToEngineError(errno));
			return result;
		}
		uint64 GetFileSize(handle file)
		{
			struct stat info;
			if (fstat(reinterpret_cast<intptr>(file), &info) == -1) throw FileAccessException(PosixErrorToEngineError(errno));
			return info.st_size;
		}
		void SetFileSize(handle file, uint64 size)
		{
			do {
				int io = ftruncate(reinterpret_cast<intptr>(file), size);
				if (io == -1 && errno != EINTR) throw FileAccessException(PosixErrorToEngineError(errno));
				else if (io != -1) return;
			} while (true);
		}
		void Flush(handle file)
		{
			int io;
			do {
				io = fsync(reinterpret_cast<intptr>(file));
				if (io == -1 && errno != EINTR) throw FileAccessException(PosixErrorToEngineError(errno));
			} while (io == -1);
		}
		bool FileExists(const string & path)
		{
			SafePointer<Array<uint8>> Path = Path::NormalizePath(path).EncodeSequence(Encoding::UTF8, true);
			int file = open(reinterpret_cast<char *>(Path->GetBuffer()), O_RDONLY);
			if (file >= 0) {
				struct stat file_stat;
				if (fstat(file, &file_stat) == -1) {
					close(file);
					return false;
				}
				if ((file_stat.st_mode & S_IFMT) == S_IFDIR) {
					close(file);
					return false;
				}
				close(file);
				return true;
			} else return false;
		}
		void MoveFile(const string & from, const string & to)
		{
			SafePointer<Array<uint8>> From = Path::NormalizePath(from).EncodeSequence(Encoding::UTF8, true);
			SafePointer<Array<uint8>> To = Path::NormalizePath(to).EncodeSequence(Encoding::UTF8, true);
			if (rename(reinterpret_cast<char *>(From->GetBuffer()), reinterpret_cast<char *>(To->GetBuffer())) == -1) throw FileAccessException(PosixErrorToEngineError(errno));
		}
		void RemoveFile(const string & path)
		{
			SafePointer<Array<uint8> > Path = Path::NormalizePath(path).EncodeSequence(Encoding::UTF8, true);
			if (unlink(reinterpret_cast<char *>(Path->GetBuffer())) == -1) throw FileAccessException(PosixErrorToEngineError(errno));
		}
		void CreateDirectory(const string & path)
		{
			SafePointer<Array<uint8> > Path = Path::NormalizePath(path).EncodeSequence(Encoding::UTF8, true);
			if (mkdir(reinterpret_cast<char *>(Path->GetBuffer()), 0777) == -1) {
				if (errno == EEXIST) {
					throw DirectoryAlreadyExistsException();
				}
				throw FileAccessException(PosixErrorToEngineError(errno));
			}
		}
		void RemoveDirectory(const string & path)
		{
			SafePointer<Array<uint8> > Path = Path::NormalizePath(path).EncodeSequence(Encoding::UTF8, true);
			if (rmdir(reinterpret_cast<char *>(Path->GetBuffer())) == -1) throw FileAccessException(PosixErrorToEngineError(errno));
		}
		void CreateSymbolicLink(const string & at, const string & to)
		{
			SafePointer< Array<uint8> > At = Path::NormalizePath(at).EncodeSequence(Encoding::UTF8, true);
			SafePointer< Array<uint8> > To = Path::NormalizePath(to).EncodeSequence(Encoding::UTF8, true);
			if (symlink(reinterpret_cast<char *>(To->GetBuffer()), reinterpret_cast<char *>(At->GetBuffer())) == -1) {
				throw FileAccessException(PosixErrorToEngineError(errno));
			}
		}
		void CreateHardLink(const string & at, const string & to)
		{
			SafePointer< Array<uint8> > At = Path::NormalizePath(at).EncodeSequence(Encoding::UTF8, true);
			SafePointer< Array<uint8> > To = Path::NormalizePath(to).EncodeSequence(Encoding::UTF8, true);
			if (linkat(AT_FDCWD, reinterpret_cast<char *>(To->GetBuffer()), AT_FDCWD, reinterpret_cast<char *>(At->GetBuffer()), AT_SYMLINK_FOLLOW) == -1) {
				throw FileAccessException(PosixErrorToEngineError(errno));
			}
		}
		FileType GetFileType(const string & file)
		{
			SafePointer<DataBlock> path = Path::NormalizePath(file).EncodeSequence(Encoding::UTF8, true);
			auto fd = open(reinterpret_cast<const char *>(path->GetBuffer()), O_RDONLY | O_SYMLINK);
			if (fd == -1) throw FileAccessException(PosixErrorToEngineError(errno));
			struct stat file_stat;
			if (fstat(fd, &file_stat) == -1) {
				auto e = errno;
				close(fd);
				throw FileAccessException(PosixErrorToEngineError(e));
			}
			close(fd);
			if ((file_stat.st_mode & S_IFMT) == S_IFREG) return FileType::Regular;
			else if ((file_stat.st_mode & S_IFMT) == S_IFDIR) return FileType::Directory;
			else if ((file_stat.st_mode & S_IFMT) == S_IFLNK) return FileType::SymbolicLink;
			else return FileType::Unknown;
		}
		void GetVolumeSpace(const string & volume, uint64 * total_bytes, uint64 * free_bytes, uint64 * user_available_bytes)
		{
			SafePointer<DataBlock> path = Path::NormalizePath(volume).EncodeSequence(Encoding::UTF8, true);
			struct statfs fss;
			if (statfs(reinterpret_cast<const char *>(path->GetBuffer()), &fss) == -1) throw FileAccessException(PosixErrorToEngineError(errno));
			if (total_bytes) *total_bytes = fss.f_bsize * fss.f_blocks;
			if (free_bytes) *free_bytes = fss.f_bsize * fss.f_bfree;
			if (user_available_bytes) *user_available_bytes = fss.f_bsize * fss.f_bavail;
		}
		string GetCurrentDirectory(void)
		{
			SafePointer< Array<uint8> > Path = new Array<uint8>(PATH_MAX);
			Path->SetLength(PATH_MAX);
			do {
				if (getcwd(reinterpret_cast<char *>(Path->GetBuffer()), Path->Length())) break;
				if (errno == ENOENT) throw Exception();
				else if (errno == ENOMEM) throw OutOfMemoryException();
				else if (errno != ERANGE) throw FileAccessException(PosixErrorToEngineError(errno));
				Path->SetLength(Path->Length() * 2);
			} while(true);
			return string(Path->GetBuffer(), -1, Encoding::UTF8);
		}
		void SetCurrentDirectory(const string & path)
		{
			SafePointer<Array<uint8> > FullPath = new Array<uint8>(PATH_MAX);
			FullPath->SetLength(PATH_MAX);
			SafePointer<Array<uint8> > Path = Path::NormalizePath(path).EncodeSequence(Encoding::UTF8, true);
			realpath(reinterpret_cast<char *>(Path->GetBuffer()), reinterpret_cast<char *>(FullPath->GetBuffer()));
			if (chdir(reinterpret_cast<char *>(FullPath->GetBuffer())) != 0) throw FileAccessException(PosixErrorToEngineError(errno));
		}
		string GetExecutablePath(void)
		{
			Array<uint8> Path(0x800);
			Path.SetLength(0x800);
			uint32 length = Path.Length();
			if (_NSGetExecutablePath(reinterpret_cast<char *>(Path.GetBuffer()), &length) == -1) {
				Path.SetLength(length);
				_NSGetExecutablePath(reinterpret_cast<char *>(Path.GetBuffer()), &length);
			}
			return string(Path.GetBuffer(), -1, Encoding::UTF8);
		}
		handle GetStandardOutput(void)
		{
			struct stat fs;
			if (fstat(1, &fs) != -1) return handle(1);
			else return InvalidHandle;
		}
		handle GetStandardInput(void)
		{
			struct stat fs;
			if (fstat(0, &fs) != -1) return handle(0);
			else return InvalidHandle;
		}
		handle GetStandardError(void)
		{
			struct stat fs;
			if (fstat(2, &fs) != -1) return handle(2);
			else return InvalidHandle;
		}
		void SetStandardOutput(handle file)
		{
			if (file != InvalidHandle) dup2(reinterpret_cast<intptr>(file), 1); else {
				auto ndev = open("/dev/null", O_RDWR);
				if (ndev == -1) throw FileAccessException(PosixErrorToEngineError(errno));
				if (dup2(ndev, 1) == -1) {
					auto e = errno;
					close(ndev);
					throw FileAccessException(PosixErrorToEngineError(e));
				}
				close(ndev);
			}
		}
		void SetStandardInput(handle file)
		{
			if (file != InvalidHandle) dup2(reinterpret_cast<intptr>(file), 0); else {
				auto ndev = open("/dev/null", O_RDWR);
				if (ndev == -1) throw FileAccessException(PosixErrorToEngineError(errno));
				if (dup2(ndev, 0) == -1) {
					auto e = errno;
					close(ndev);
					throw FileAccessException(PosixErrorToEngineError(e));
				}
				close(ndev);
			}
		}
		void SetStandardError(handle file)
		{
			if (file != InvalidHandle) dup2(reinterpret_cast<intptr>(file), 2); else {
				auto ndev = open("/dev/null", O_RDWR);
				if (ndev == -1) throw FileAccessException(PosixErrorToEngineError(errno));
				if (dup2(ndev, 2) == -1) {
					auto e = errno;
					close(ndev);
					throw FileAccessException(PosixErrorToEngineError(e));
				}
				close(ndev);
			}
		}
	}
}