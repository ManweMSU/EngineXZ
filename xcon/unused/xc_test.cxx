#include "client/xc_api.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::XC;
using namespace Engine::Streaming;

int Main(void)
{
	SetCurrentDirectory(Path::GetDirectory(GetExecutablePath()) + L"/../..");

	XC::ConsoleDesc desc;
	desc.flags = CreateConsoleFlagSetTitle;
	desc.xc_path = L"xcon/_build/windows_x64_debug/xc.exe";
	desc.title = L"suka ty pidor";
	SafePointer<XC::Console> console = new XC::Console(desc);

	while (true) {
		IO::ConsoleEventDesc desc;
		console->ReadEvent(desc);
		if (desc.Event == IO::ConsoleEvent::CharacterInput) {
			if (desc.CharacterCode == L'\n') break;
		} else if (desc.Event == IO::ConsoleEvent::EndOfFile) return 0;
	}

	console->SetForegroundColorIndex(-2);
	console->SetBackgroundColorIndex(-2);
	console->SetWindowTitle(L"сука привет");
	console->Print(L"сука привет\n");
	console->SetCaretPosition(Point(10, 1));
	console->SetForegroundColor(Color(192, 128, 255, 255));
	console->SetBackgroundColor(Color(128, 0, 255, 128));

	console->SetIOMode(IO::ConsoleInputMode::Raw);
	CaretStateDesc csd;
	csd.flags = CaretControlFlagBlinking | CaretControlFlagShape | CaretControlFlagWeight;
	csd.style = CaretStyle::Vertical;
	csd.weight = 0.5;
	console->SetCaretState(csd);
	console->SetFontAttributes(FontAttributeItalic, FontAttributeItalic);
	console->SetCloseDetachedConsole(true);
	while (true) {
		IO::ConsoleEventDesc desc;
		console->ReadEvent(desc);
		if (desc.Event == IO::ConsoleEvent::CharacterInput) {
			console->Print(FormatString(L"CHAR INPUT: %0\n\r", string(&desc.CharacterCode, 1, Encoding::UTF32)));
		} else if (desc.Event == IO::ConsoleEvent::KeyInput) {
			console->Print(FormatString(L"KEY INPUT: %0 [%1]\n\r", desc.KeyCode, desc.KeyFlags));
			if (desc.KeyCode == KeyCodes::Q) {
				console->LoadBackbuffer(L"C:\\Users\\Manwe\\Pictures\\Emilia - Umbrella.jpg");
			} else if (desc.KeyCode == KeyCodes::W) {
				console->ResetBackbuffer();
			} else if (desc.KeyCode == KeyCodes::E) {
				console->CreateBackbuffer(100, 100);
			} else if (desc.KeyCode == KeyCodes::A) {
				console->SetBackbufferStretchMode(Windows::ImageRenderMode::Blit);
			} else if (desc.KeyCode == KeyCodes::S) {
				console->SetBackbufferStretchMode(Windows::ImageRenderMode::Stretch);
			} else if (desc.KeyCode == KeyCodes::D) {
				console->SetBackbufferStretchMode(Windows::ImageRenderMode::FitKeepAspectRatio);
			} else if (desc.KeyCode == KeyCodes::F) {
				console->SetBackbufferStretchMode(Windows::ImageRenderMode::CoverKeepAspectRatio);
			} else if (desc.KeyCode == KeyCodes::Z) {
				SafePointer<IPC::ISharedMemory> mem;
				int w, h;
				console->AccessBackbuffer(mem.InnerRef(), &w, &h);
				console->Print(FormatString(L"Backbuffer of sizes %0 x %1\n\r", w, h));
				if (mem) {
					void * lock;
					if (mem->Map(&lock, IPC::SharedMemoryMapReadWrite)) {
						auto color = Color(255, 0, 128, 255);
						auto pixels = reinterpret_cast<Color *>(lock);
						pixels[0 + 0 * w] = color;
						pixels[1 + 1 * w] = color;
						pixels[2 + 2 * w] = color;
						pixels[2 + 0 * w] = color;
						pixels[0 + 2 * w] = color;
						mem->Unmap();
						console->SynchronizeBackbuffer();
					}
				}
			}
		} else if (desc.Event == IO::ConsoleEvent::ConsoleResized) {
			console->Print(FormatString(L"RESIZE: %0 x %1\n\r", desc.Width, desc.Height));
		}
		if (desc.Event == IO::ConsoleEvent::EndOfFile) break;
	}

	return 0;
}