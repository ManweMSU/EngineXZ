#include "xv_mi.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

string Localized(int id);
string ErrorDescription(XV::CompilerStatus status);

struct EditorState
{
	Console & console;
	bool result;
	int scroll_x, scroll_y;
	int caret_x, caret_y;
	int caret;
	string file_name;
	SafePointer< Array<uint32> > code;
	SafePointer<XV::ICompilerCallback> callback;
	XV::CodeMetaInfo meta;
	XV::CompilerStatusDesc desc;
	XV::CodeRangeInfo current_range;
	bool show_error, show_variants;
	int var_offset;

	EditorState(Console & cns) : console(cns), result(false), scroll_x(0), scroll_y(0), caret_x(0), caret_y(0), caret(0), show_error(false), show_variants(false) { current_range.from = -1; }
	void RenderWithEllipsis(const string & text, int max_length)
	{
		if (text.Length() > max_length) {
			console.Write(text.Fragment(0, max_length - 3) + L"...");
		} else console.Write(text);
	}
	void SetColorOfTag(XV::CodeRangeTag tag)
	{
		if (tag == XV::CodeRangeTag::Comment) console.SetTextColor(7);
		else if (tag == XV::CodeRangeTag::Keyword) console.SetTextColor(14);
		else if (tag == XV::CodeRangeTag::Punctuation) console.SetTextColor(15);
		else if (tag == XV::CodeRangeTag::Prototype) console.SetTextColor(13);
		else if (tag == XV::CodeRangeTag::LiteralBoolean) console.SetTextColor(12);
		else if (tag == XV::CodeRangeTag::LiteralNumeric) console.SetTextColor(12);
		else if (tag == XV::CodeRangeTag::LiteralString) console.SetTextColor(12);
		else if (tag == XV::CodeRangeTag::LiteralNull) console.SetTextColor(12);
		else if (tag == XV::CodeRangeTag::IdentifierUnknown) console.SetTextColor(15);
		else if (tag == XV::CodeRangeTag::IdentifierNamespace) console.SetTextColor(15);
		else if (tag == XV::CodeRangeTag::IdentifierType) console.SetTextColor(11);
		else if (tag == XV::CodeRangeTag::IdentifierPrototype) console.SetTextColor(11);
		else if (tag == XV::CodeRangeTag::IdentifierConstant) console.SetTextColor(10);
		else if (tag == XV::CodeRangeTag::IdentifierVariable) console.SetTextColor(10);
		else if (tag == XV::CodeRangeTag::IdentifierProperty) console.SetTextColor(9);
		else if (tag == XV::CodeRangeTag::IdentifierField) console.SetTextColor(10);
		else if (tag == XV::CodeRangeTag::IdentifierFunction) console.SetTextColor(9);
		else console.SetTextColor(15);
	}
	void RenderCode(int left, int top, int right, int bottom)
	{
		int x = -scroll_x, y = -scroll_y;
		for (int i = 0; i < code->Length() - 1; i++) {
			if (caret == i) { caret_x = left + x; caret_y = top + y; }
			uint32 ucs = 0;
			if (i == meta.error_absolute_from) console.SetBackgroundColor(4);
			else if (desc.error_line_len > 0 && i >= meta.error_absolute_from + desc.error_line_len) console.SetBackgroundColor(0);
			ucs = code->ElementAt(i);
			auto markup = meta.info[i];
			if (markup) SetColorOfTag(markup->tag);
			if (ucs < 32) {
				if (ucs == L'\n') { y++; x = -scroll_x; }
				else if (ucs == L'\t') { x++; while ((x + scroll_x) & 0x3) x++; }
				ucs = 0;
			}
			if (ucs && x >= 0 && y >= 0 && x < right - left && y < bottom - top) {
				console.MoveCaret(left + x, top + y);
				console.Write(string(&ucs, 1, Encoding::UTF32));
			}
			if (ucs >= 32) x++;
		}
	}
	void Render(bool refresh)
	{
		if (refresh) {
			var_offset = 0;
			meta.autocomplete_at = show_variants ? caret : -1;
			meta.error_absolute_from = -1;
			meta.info.Clear();
			meta.autocomplete.Clear();
			SafePointer<XV::IOutputModule> output;
			XV::CompileModule(Path::GetFileNameWithoutExtension(file_name), *code, output.InnerRef(), callback, desc, &meta);
		}
		int w, h;
		console.GetScreenBufferDimensions(w, h);
		console.SetTextColor(15);
		console.SetBackgroundColor(0);
		console.ClearScreen();
		console.Write(L" ");
		console.SetBackgroundColor(2);
		console.Write(L" ");
		RenderWithEllipsis(file_name, w - 4);
		console.Write(L" ");
		console.SetBackgroundColor(0);
		RenderCode(0, 1, w - 1, h - 2);
		console.MoveCaret(1, h - 1);
		console.SetBackgroundColor(1);
		console.SetTextColor(15);
		console.Write(Localized(151));
		console.SetBackgroundColor(0);
		console.Write(L"  ");
		console.SetBackgroundColor(1);
		console.Write(Localized(152));
		console.SetBackgroundColor(0);
		console.Write(L"  ");
		console.SetBackgroundColor(1);
		console.Write(Localized(153));
		console.SetBackgroundColor(0);
		console.Write(L"  ");
		console.SetBackgroundColor(1);
		console.Write(Localized(154));
		console.SetBackgroundColor(0);
		console.Write(L"  ");
		console.SetBackgroundColor(1);
		console.Write(Localized(155));
		if (show_error && desc.status != XV::CompilerStatus::Success) {
			console.SetBackgroundColor(4);
			console.MoveCaret(1, h - 2);
			console.Write(L" " + ErrorDescription(desc.status) + L" ");
		}
		if (current_range.from >= 0) {
			int length = max(max(current_range.identifier.Length(), current_range.type.Length()), current_range.value.Length()) + 2;
			console.SetBackgroundColor(3);
			console.MoveCaret(w - length - 2, 1);
			console.Write(L" ");
			console.Write(current_range.identifier); console.Write(string(L' ', length - current_range.identifier.Length() + 1));
			console.MoveCaret(w - length - 2, 2);
			console.Write(L" ");
			console.Write(current_range.type); console.Write(string(L' ', length - current_range.type.Length() + 1));
			console.MoveCaret(w - length - 2, 3);
			console.Write(L" ");
			console.Write(current_range.value); console.Write(string(L' ', length - current_range.value.Length() + 1));
		}
		if (show_variants) {
			int length = 0;
			for (auto & v : meta.autocomplete) if (v.key.Length() > length) length = v.key.Length();
			console.SetBackgroundColor(8);
			int y = 0;
			int skip = var_offset;
			for (auto & v : meta.autocomplete) {
				if (skip > 0) { skip--; continue; }
				console.MoveCaret(w - length - 2, y); y++;
				if (y == h - 1) {
					console.SetTextColor(7);
					console.Write(L" ..."); console.Write(string(L' ', length - 2));
					break;
				}
				console.Write(L" ");
				SetColorOfTag(v.value);
				console.Write(v.key); console.Write(string(L' ', length - v.key.Length() + 1));
			}
		}
		console.MoveCaret(caret_x, caret_y);
	}
	void Run(void)
	{
		bool refresh = true;
		while (true) {
			Render(refresh);
			refresh = false;
			ConsoleEventDesc desc;
			console.ReadEvent(desc);
			if (desc.Event == ConsoleEvent::EndOfFile) break;
			else if (desc.Event == ConsoleEvent::KeyInput) {
				if (desc.KeyCode == KeyCodes::Escape && desc.KeyFlags == 0) break;
				else if (desc.KeyCode == KeyCodes::F1 && desc.KeyFlags == 0) { result = true; break; }
				else if (desc.KeyCode == KeyCodes::F2 && desc.KeyFlags == 0) {
					current_range.from = -1;
					for (auto & r : meta.info) if (r.value.from <= caret && r.value.from + r.value.length > caret) current_range = r.value;
				} else if (desc.KeyCode == KeyCodes::F3 && desc.KeyFlags == 0) {
					show_error = !show_error;
				} else if (desc.KeyCode == KeyCodes::F4 && desc.KeyFlags == 0) {
					show_variants = !show_variants;
					refresh = true;
				} else if (desc.KeyCode == KeyCodes::Left && desc.KeyFlags == 0) {
					if (caret > 0) caret--;
					if (show_variants) refresh = true;
				} else if (desc.KeyCode == KeyCodes::Right && desc.KeyFlags == 0) {
					if (code->ElementAt(caret)) caret++;
					if (show_variants) refresh = true;
				} else if (desc.KeyCode == KeyCodes::Up && desc.KeyFlags == 0) {
					int x = 0;
					while (caret > 0 && code->ElementAt(caret) != L'\n' && code->ElementAt(caret) != L'\r') { caret--; x++; }
					while (caret > 0 && (code->ElementAt(caret) == L'\n' || code->ElementAt(caret) == L'\r')) caret--;
					while (caret > 0 && code->ElementAt(caret) != L'\n' && code->ElementAt(caret) != L'\r') caret--;
					while (code->ElementAt(caret) != L'\n' && code->ElementAt(caret) != L'\r' && x) { caret++; x--; }
					if (show_variants) refresh = true;
				} else if (desc.KeyCode == KeyCodes::Down && desc.KeyFlags == 0) {
					int x = 0;
					while (caret > 0 && code->ElementAt(caret) != L'\n' && code->ElementAt(caret) != L'\r') { caret--; x++; }
					caret++;
					while (caret < code->Length() && code->ElementAt(caret) != L'\n' && code->ElementAt(caret) != L'\r' && code->ElementAt(caret) != 0) caret++;
					while (caret < code->Length() && (code->ElementAt(caret) == L'\n' || code->ElementAt(caret) == L'\r')) caret++;
					while (code->ElementAt(caret) != L'\n' && code->ElementAt(caret) != L'\r' && code->ElementAt(caret) != 0 && x) { caret++; x--; }
					if (show_variants) refresh = true;
				} else if (desc.KeyCode == KeyCodes::Up && desc.KeyFlags == ConsoleKeyFlagShift) {
					var_offset--;
				} else if (desc.KeyCode == KeyCodes::Down && desc.KeyFlags == ConsoleKeyFlagShift) {
					var_offset++;
				} else if (desc.KeyCode == KeyCodes::Back && desc.KeyFlags == 0) {
					if (caret > 0) { code->Remove(caret - 1); caret--; }
					refresh = true;
				} else if (desc.KeyCode == KeyCodes::Delete && desc.KeyFlags == 0) {
					if (code->ElementAt(caret)) code->Remove(caret);
					refresh = true;
				}
			} else if (desc.Event == ConsoleEvent::CharacterInput) {
				if (desc.CharacterCode >= 32 || desc.CharacterCode == L'\t') {
					code->Insert(desc.CharacterCode, caret);
					caret++; refresh = true;
				}
			}
		}
	}
};

bool LaunchInteractiveEditor(const Engine::string & file, Engine::Array<Engine::uint32> * code, Engine::XV::ICompilerCallback * callback, Engine::IO::Console & console)
{
	console.AlternateScreenBuffer(true);
	console.SetInputMode(ConsoleInputMode::Raw);
	console.SetTitle(Localized(1));
	EditorState state(console);
	state.file_name = file;
	state.code.SetRetain(code);
	state.callback.SetRetain(callback);
	state.Run();
	console.SetTextColor(-1);
	console.SetBackgroundColor(-1);
	console.SetInputMode(ConsoleInputMode::Echo);
	console.AlternateScreenBuffer(false);
	return state.result;
}