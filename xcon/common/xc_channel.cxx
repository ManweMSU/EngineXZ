#include "xc_channel.h"

#ifdef ENGINE_WINDOWS
#include <Windows.h>

namespace Engine
{
	namespace XC
	{
		class Channel : public IChannel
		{
			HANDLE _io;
			OVERLAPPED _async_in, _async_out;
		private:
			static int _read_file_async(HANDLE io, LPOVERLAPPED async, void * buffer, int length) noexcept
			{
				DWORD read;
				if (!ReadFile(io, buffer, length, &read, async)) {
					if (GetLastError() == ERROR_IO_PENDING) { if (!GetOverlappedResult(io, async, &read, TRUE)) return -1; }
					else return -1;
				}
				return read;
			}
			static int _write_file_async(HANDLE io, LPOVERLAPPED async, const void * buffer, int length) noexcept
			{
				DWORD written;
				if (!WriteFile(io, buffer, length, &written, async)) {
					if (GetLastError() == ERROR_IO_PENDING) { if (!GetOverlappedResult(io, async, &written, TRUE)) return -1; }
					else return -1;
				}
				return written;
			}
		public:
			Channel(HANDLE io) : _io(io)
			{
				_async_in.hEvent = CreateEventW(0, TRUE, FALSE, L"");
				if (!_async_in.hEvent) throw Exception();
				_async_out.hEvent = CreateEventW(0, TRUE, FALSE, L"");
				if (!_async_out.hEvent) { CloseHandle(_async_in.hEvent); throw Exception(); }
				_async_in.Offset = _async_in.OffsetHigh = _async_out.Offset = _async_out.OffsetHigh = 0;
			}
			virtual ~Channel(void) override { CloseHandle(_io); CloseHandle(_async_in.hEvent); CloseHandle(_async_out.hEvent); }
			virtual bool SendRequest(const Request & req) noexcept override
			{
				uint hdr[2] = { req.verb, req.data ? req.data->Length() : 0 };
				if (_write_file_async(_io, &_async_out, &hdr, sizeof(hdr)) != sizeof(hdr)) return false;
				if (req.data && _write_file_async(_io, &_async_out, req.data->GetBuffer(), req.data->Length()) != req.data->Length()) return false;
				if (req.verb == 0) FlushFileBuffers(_io);
				return true;
			}
			virtual bool ReadRequest(Request & req) noexcept override
			{
				try {
					uint hdr[2];
					int num_read = _read_file_async(_io, &_async_in, &hdr, sizeof(hdr));
					if (num_read != sizeof(hdr)) throw Exception();
					req.verb = hdr[0];
					req.data = new DataBlock(1);
					req.data->SetLength(hdr[1]);
					uint read = 0;
					while (hdr[1]) {
						num_read = _read_file_async(_io, &_async_in, req.data->GetBuffer() + read, hdr[1]);
						if (num_read <= 0) throw Exception();
						hdr[1] -= num_read;
						read += num_read;
					}
					return true;
				} catch (...) { return false; }
			}
		};
		class ChannelServer : public IChannelServer
		{
			string _path;
			HANDLE _io;
			OVERLAPPED _async;
		public:
			ChannelServer(const string & path) : _path(path)
			{
				_io = CreateNamedPipeW(path, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED, 0, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, 0);
				if (_io == INVALID_HANDLE_VALUE) throw IO::FileAccessException();
				_async.hEvent = CreateEventW(0, TRUE, FALSE, L"");
				if (!_async.hEvent) { CloseHandle(_io); throw Exception(); }
				_async.Offset = _async.OffsetHigh = 0;
			}
			virtual ~ChannelServer(void) override { CloseHandle(_io); CloseHandle(_async.hEvent); }
			virtual void GetChannelPath(string & path) noexcept override { try { path = _path; } catch (...) {} }
			virtual IChannel * Accept(void) noexcept override
			{
				try {
					if (!ConnectNamedPipe(_io, &_async)) {
						auto error = GetLastError();
						if (error == ERROR_IO_PENDING) {
							DWORD size;
							if (!GetOverlappedResult(_io, &_async, &size, TRUE)) { if (GetLastError() != ERROR_PIPE_CONNECTED) throw IO::FileAccessException(); }
						} else if (error != ERROR_PIPE_CONNECTED) throw IO::FileAccessException();
					}
					auto new_instance = CreateNamedPipeW(_path, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, 0, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, 0);
					if (new_instance == INVALID_HANDLE_VALUE) throw IO::FileAccessException();
					SafePointer<IChannel> result = new Channel(_io);
					_io = new_instance;
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
		};

		IChannelServer * CreateChannelServer(void)
		{
			int counter = 0;
			while (true) {
				try {
					auto path = FormatString(L"\\\\.\\pipe\\xcdev%0", counter);
					SafePointer<IChannelServer> server = new ChannelServer(path);
					server->Retain();
					return server;
				} catch (...) {}
				counter++;
			}
		}
		IChannel * ConnectChannel(const string & at_path)
		{
			auto io = CreateFileW(at_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
			if (io == INVALID_HANDLE_VALUE) throw IO::FileAccessException();
			return new Channel(io);
		}
	}
}

#endif