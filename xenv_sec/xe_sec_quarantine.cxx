#include "xe_sec_quarantine.h"

#ifdef ENGINE_WINDOWS
#include <Windows.h>
#endif
#ifdef ENGINE_MACOSX
#include <sys/xattr.h>
#endif

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			bool IsFileOnQuarantine(const string & path)
			{
				#if defined(ENGINE_WINDOWS)
					auto handle = CreateFileW(IO::ExpandPath(path) + L":Zone.Identifier", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
					if (handle == INVALID_HANDLE_VALUE) return false;
					bool status = false;
					try {
						Streaming::FileStream stream(handle);
						Streaming::TextReader reader(&stream, Encoding::ANSI);
						bool valid_section = false;
						while (!reader.EofReached()) {
							auto line = reader.ReadLine();
							if (line[0] == L'[') {
								valid_section = line == L"[ZoneTransfer]";
							} else if (valid_section && line.Fragment(0, 7) == L"ZoneId=") {
								auto zone_id = line.Fragment(7, -1).ToUInt32();
								status = zone_id != 0;
								break;
							}
						}
					} catch (...) { CloseHandle(handle); throw; }
					CloseHandle(handle);
					return status;
				#elif defined(ENGINE_MACOSX)
					SafePointer<DataBlock> path_utf8 = path.EncodeSequence(Encoding::UTF8, true);
					DataBlock data(1);
					auto length = getxattr(reinterpret_cast<const char *>(path_utf8->GetBuffer()), "com.apple.quarantine", 0, 0, 0, 0);
					if (length < 0) return false;
					data.SetLength(length);
					length = getxattr(reinterpret_cast<const char *>(path_utf8->GetBuffer()), "com.apple.quarantine", data.GetBuffer(), length, 0, 0);
					if (length < 0) return false;
					auto quarantine_attribute = string(data.GetBuffer(), data.Length(), Encoding::UTF8).Fragment(0, 4).ToUInt32(HexadecimalBase);
					return quarantine_attribute & 0x80;
				#else
					return false;
				#endif
			}
		}
	}
}