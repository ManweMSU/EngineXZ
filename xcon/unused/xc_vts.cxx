#include "xc_vts.h"

#include <Windows.h>

namespace Engine
{
	namespace XC
	{
		struct InputSequence
		{
			char intro, second, cmd;
			int num_args;
			int args[16];
			string os_cmd;
		};

		void _print_escape_sequence(const InputSequence & seq)
		{
			AllocConsole();
			IO::Console cns;
			if (seq.intro) {
				auto s = L"ESC" + string(widechar(seq.intro));
				if (seq.second) s += string(widechar(seq.second));
				for (int i = 0; i < seq.num_args; i++) s += L"(" + string(seq.args[i]) + L")";
				s += seq.os_cmd;
				if (seq.cmd) s += string(widechar(seq.cmd));
				cns.WriteLine(s);
			} else cns.WriteLine(seq.os_cmd);
		}

			// 			} else if (s.second == 0 && s.cmd == 'm') {
			
			// 			} else if ((s.second == '?' || s.second == 0) && s.cmd == 'h') {
			// 				if (s.args[0] == 12) _console->SetCaretBlinks(true);
			// 				else if (s.args[0] == 25) _console->SetCaretStyle(CaretStyle::Horizontal);
			// 			} else if ((s.second == '?' || s.second == 0) && s.cmd == 'l') {
			// 				if (s.args[0] == 12) _console->SetCaretBlinks(false);
			// 				else if (s.args[0] == 25) _console->SetCaretStyle(CaretStyle::Null);
			// 			}
			// 		} else {
			
			// 		}
			// 	}));
			// }

		void ExthreadProcess(TerminalProcessingState & state, IPseudoTerminal & pty, ConsoleState & console, InputSequence & seq)
		{
			if (seq.intro == 0) {
				Array<uint32> ucs(1);
				ucs.SetLength(seq.os_cmd.GetEncodedLength(Encoding::UTF32));
				seq.os_cmd.Encode(ucs.GetBuffer(), Encoding::UTF32, false);
				console.Print(ucs.GetBuffer(), ucs.Length());
			} else if (seq.intro == 'D') {
				// TODO: LINE FEED ?
				_print_escape_sequence(seq);
			} else if (seq.intro == 'E') {
				// TODO: LINE FEED
				_print_escape_sequence(seq);
			} else if (seq.intro == 'H') {
				// TODO: SET TAB
				_print_escape_sequence(seq);
			} else if (seq.intro == 'M') {
				// TODO: REVERSE LINE FEED
				_print_escape_sequence(seq);
			} else if (seq.intro == '7') {
				// TODO: PUSH CARET
				_print_escape_sequence(seq);
			} else if (seq.intro == '8') {
				// TODO: POP CARET
				_print_escape_sequence(seq);
			} else if (seq.intro == '[') {
				if (seq.cmd == '@') {
					// TODO: INSERT ZEROES
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'A') {
					Point pos; console.GetCaretPosition(pos);
					console.SetCaretPosition(Point(pos.x, pos.y - max(seq.args[0], 1)));
				} else if (seq.cmd == 'B') {
					Point pos; console.GetCaretPosition(pos);
					console.SetCaretPosition(Point(pos.x, pos.y + max(seq.args[0], 1)));
				} else if (seq.cmd == 'C') {
					Point pos; console.GetCaretPosition(pos);
					console.SetCaretPosition(Point(pos.x + max(seq.args[0], 1), pos.y));
				} else if (seq.cmd == 'D') {
					Point pos; console.GetCaretPosition(pos);
					console.SetCaretPosition(Point(pos.x - max(seq.args[0], 1), pos.y));
				} else if (seq.cmd == 'E') {
					console.LineFeed();
				} else if (seq.cmd == 'F') {
					// TODO: REVERSE LINE FEED
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'G') {
					Point pos; console.GetCaretPosition(pos);
					console.SetCaretPosition(Point(seq.args[0] - 1, pos.y));
				} else if (seq.cmd == 'H') {
					console.SetCaretPosition(Point(seq.args[1] - 1, seq.args[0] - 1));
				} else if (seq.cmd == 'I') {
					// TODO: FORWARD TAB
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'J') {
					console.Erase(true, seq.args[0]);
				} else if (seq.cmd == 'K') {
					console.Erase(false, seq.args[0]);
				} else if (seq.cmd == 'L') {
					// TODO: INSERT LINES
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'M') {
					// TODO: REMOVE LINES
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'P') {
					// TODO: REMOVE CHARACTERS
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'S') {
					// TODO: SCROLL UP LINES
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'T') {
					// TODO: SCROLL DOWN LINES
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'X') {
					// TODO: IMPROVE
					Point pos; console.GetCaretPosition(pos);
					Array<uint32> ucs(1);
					ucs.SetLength(seq.args[0]);
					for (int i = 0; i < seq.args[0]; i++) ucs[i] = L' ';
					console.Print(ucs.GetBuffer(), ucs.Length());
					console.SetCaretPosition(pos);
				} else if (seq.cmd == 'Z') {
					// TODO: REVERSE TAB
					_print_escape_sequence(seq);
				} else if (seq.cmd == '`') {
					// TODO: SET X ABSOLUTE
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'a') {
					// TODO: SET X RELATIVE
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'b') {
					// TODO: REPLICATE LAST CHAR
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'c') {
					pty.WriteOutputStream("\33[?1;2c", 7);
				} else if (seq.cmd == 'd') {
					// TODO: SET Y RELATIVE
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'e') {
					// TODO: SET Y RELATIVE
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'f') {
					console.SetCaretPosition(Point(seq.args[1] - 1, seq.args[0] - 1));
				} else if (seq.cmd == 'g') {
					// TODO: TAB STOPS RESET
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'h') {
					if (seq.args[0] == 12 && seq.second == '?') console.SetCaretBlinks(true);
					else if (seq.args[0] == 25) { if (console.GetCaretStyle() == CaretStyle::Null) console.SetCaretStyle(state.caret_revert_style); }
					else if (seq.args[0] == 1049 && seq.second == '?') {
						_print_escape_sequence(seq);
						// TODO: ALTERNATE SCREEN BUFFER
					}
				} else if (seq.cmd == 'l') {
					if (seq.args[0] == 12 && seq.second == '?') console.SetCaretBlinks(false);
					else if (seq.args[0] == 25) { state.caret_revert_style = console.GetCaretStyle(); console.SetCaretStyle(CaretStyle::Null); }
					else if (seq.args[0] == 1049 && seq.second == '?') {
						_print_escape_sequence(seq);
						// TODO: MAIN SCREEN BUFFER
					}
				} else if (seq.cmd == 'm') {
					if (!seq.num_args) {
						console.RevertAttributionFlags(0xFFFFFFFF);
						console.SetForegroundColorIndex(-1);
						console.SetBackgroundColorIndex(-1);
					}
					for (int i = 0; i < seq.num_args; i++) {
						if (seq.args[i] == 0) {
							console.RevertAttributionFlags(0xFFFFFFFF);
							console.SetForegroundColorIndex(-1);
							console.SetBackgroundColorIndex(-1);
						} else if (seq.args[i] == 1) {
							console.SetAttributionFlags(CellFlagBold, CellFlagBold);
						} else if (seq.args[i] == 22) {
							console.SetAttributionFlags(CellFlagBold, 0);
						} else if (seq.args[i] == 3) {
							console.SetAttributionFlags(CellFlagItalic, CellFlagItalic);
						} else if (seq.args[i] == 23) {
							console.SetAttributionFlags(CellFlagItalic, 0);
						} else if (seq.args[i] == 4) {
							console.SetAttributionFlags(CellFlagUnderline, CellFlagUnderline);
						} else if (seq.args[i] == 24) {
							console.SetAttributionFlags(CellFlagUnderline, 0);
						} else if (seq.args[i] == 7) {
							console.SetForegroundColorIndex(-2);
							console.SetBackgroundColorIndex(-2);
						} else if (seq.args[i] == 27) {
							console.SetForegroundColorIndex(-1);
							console.SetBackgroundColorIndex(-1);
						} else if (seq.args[i] == 38) {
							// 38	Foreground Extended	Applies extended color value to the foreground (see details below)
							_print_escape_sequence(seq);
						} else if (seq.args[i] == 48) {
							// 48	Background Extended	Applies extended color value to the background (see details below)
							_print_escape_sequence(seq);
						} else if (seq.args[i] == 30) console.SetForegroundColorIndex(0);
						else if (seq.args[i] == 31) console.SetForegroundColorIndex(4);
						else if (seq.args[i] == 32) console.SetForegroundColorIndex(2);
						else if (seq.args[i] == 33) console.SetForegroundColorIndex(6);
						else if (seq.args[i] == 34) console.SetForegroundColorIndex(1);
						else if (seq.args[i] == 35) console.SetForegroundColorIndex(5);
						else if (seq.args[i] == 36) console.SetForegroundColorIndex(3);
						else if (seq.args[i] == 37) console.SetForegroundColorIndex(7);
						else if (seq.args[i] == 39) console.SetForegroundColorIndex(-1);
						else if (seq.args[i] == 40) console.SetBackgroundColorIndex(0);
						else if (seq.args[i] == 41) console.SetBackgroundColorIndex(4);
						else if (seq.args[i] == 42) console.SetBackgroundColorIndex(2);
						else if (seq.args[i] == 43) console.SetBackgroundColorIndex(6);
						else if (seq.args[i] == 44) console.SetBackgroundColorIndex(1);
						else if (seq.args[i] == 45) console.SetBackgroundColorIndex(5);
						else if (seq.args[i] == 46) console.SetBackgroundColorIndex(3);
						else if (seq.args[i] == 47) console.SetBackgroundColorIndex(7);
						else if (seq.args[i] == 49) console.SetBackgroundColorIndex(-1);
						else if (seq.args[i] == 90) console.SetForegroundColorIndex(0);
						else if (seq.args[i] == 91) console.SetForegroundColorIndex(12);
						else if (seq.args[i] == 92) console.SetForegroundColorIndex(10);
						else if (seq.args[i] == 93) console.SetForegroundColorIndex(14);
						else if (seq.args[i] == 94) console.SetForegroundColorIndex(9);
						else if (seq.args[i] == 95) console.SetForegroundColorIndex(13);
						else if (seq.args[i] == 96) console.SetForegroundColorIndex(11);
						else if (seq.args[i] == 97) console.SetForegroundColorIndex(15);
						else if (seq.args[i] == 100) console.SetBackgroundColorIndex(0);
						else if (seq.args[i] == 101) console.SetBackgroundColorIndex(12);
						else if (seq.args[i] == 102) console.SetBackgroundColorIndex(10);
						else if (seq.args[i] == 103) console.SetBackgroundColorIndex(14);
						else if (seq.args[i] == 104) console.SetBackgroundColorIndex(9);
						else if (seq.args[i] == 105) console.SetBackgroundColorIndex(13);
						else if (seq.args[i] == 106) console.SetBackgroundColorIndex(11);
						else if (seq.args[i] == 107) console.SetBackgroundColorIndex(15);
					}
				} else if (seq.cmd == 'n') {
					// TODO: DEVICE STATUS REPORT
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'p') {
					if (seq.second == '!') {
						// TODO: DEVICE STATE RESET
						_print_escape_sequence(seq);
					}
				} else if (seq.cmd == 'q') {
					if (seq.second == ' ') {
						if (seq.args[0] == 0) console.RevertCaretVisuals();
						else if (seq.args[0] == 1) { console.SetCaretStyle(CaretStyle::Horizontal); console.SetCaretBlinks(true); console.SetCaretWeight(1.0); }
						else if (seq.args[0] == 2) { console.SetCaretStyle(CaretStyle::Horizontal); console.SetCaretBlinks(false); console.SetCaretWeight(1.0); }
						else if (seq.args[0] == 3) { console.SetCaretStyle(CaretStyle::Horizontal); console.SetCaretBlinks(true); console.SetCaretWeight(0.25); }
						else if (seq.args[0] == 4) { console.SetCaretStyle(CaretStyle::Horizontal); console.SetCaretBlinks(false); console.SetCaretWeight(0.25); }
						else if (seq.args[0] == 5) { console.SetCaretStyle(CaretStyle::Vertical); console.SetCaretBlinks(true); console.SetCaretWeight(0.25); }
						else if (seq.args[0] == 6) { console.SetCaretStyle(CaretStyle::Vertical); console.SetCaretBlinks(false); console.SetCaretWeight(0.25); }
					}
				} else if (seq.cmd == 'r') {
					// TODO: SET SCROLLING REGION
					_print_escape_sequence(seq);
				} else if (seq.cmd == 's') {
					// TODO: SAVE CARET STATE
					_print_escape_sequence(seq);
				} else if (seq.cmd == 'u') {
					// TODO: RESTORE CARET STATE
					_print_escape_sequence(seq);
				}
			} else if (seq.intro == ']') {
				if (seq.os_cmd[0] == L'0' || seq.os_cmd[0] == L'2') {
					console.SetTitle(seq.os_cmd.Fragment(2, -1));
				} else if (seq.os_cmd[0] == L'4') {
					// TODO: IMPLEMENT Modify Screen Colors
				}
			}
		}
		bool InthreadProcess(TerminalProcessingState & state, InputSequence & seq)
		{
			if (seq.intro == '(') {
				if (seq.cmd == '0') state.encoding_mode = 1;
				else if (seq.cmd == 'B') state.encoding_mode = 0;
				return true;
			} else if (seq.intro == '%') {
				if (seq.cmd == '@' || seq.cmd == 'G') state.encoding_mode = 0;
				return true;
			}

			// TODO: IMPLEMENT

			return false;
		}
		bool IsEndOfCSI(char chr)
		{
			return (chr == '@' || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || chr == '^' || chr == '`' ||
				chr == '{' || chr == '|' || chr == '}' || chr == '~');
		}
		bool ReadInputSequence(const TerminalProcessingState & state, const char * buffer, int length, InputSequence & seq, int & used)
		{
			if (used < length) {
				ZeroMemory(&seq.args, sizeof(seq.args));
				seq.intro = seq.second = seq.cmd = 0;
				seq.os_cmd = L"";
				seq.num_args = 0;
				if (buffer[used] == '\33') {
					if (used + 1 < length) {
						seq.intro = buffer[used + 1];
						if (seq.intro == ' ' || seq.intro == '#' || seq.intro == '%') {
							if (used + 2 < length) {
								seq.cmd = buffer[used + 2];
								used += 3;
								return true;
							} else return false;
						} else if (seq.intro == '(' || seq.intro == ')' || seq.intro == '*' || seq.intro == '+' || seq.intro == '-' || seq.intro == '.' || seq.intro == '/') {
							if (used + 2 < length) {
								if (buffer[used + 2] == '\"' || buffer[used + 2] == '%' || buffer[used + 2] == '&') {
									if (used + 3 < length) {
										seq.second = buffer[used + 2];
										seq.cmd = buffer[used + 3];
										used += 4;
										return true;
									} else return false;
								} else {
									seq.cmd = buffer[used + 2];
									used += 3;
									return true;
								}
							} else return false;
						} else if (seq.intro == '[') {
							int csi_init = used + 2;
							int csi_fin = csi_init;
							while (csi_fin < length && !IsEndOfCSI(buffer[csi_fin])) csi_fin++;
							if (csi_fin >= length) return false;
							seq.cmd = buffer[csi_fin];
							while (csi_init < csi_fin) {
								if (buffer[csi_init] >= '0' && buffer[csi_init] <= '9' && seq.num_args < 16) {
									int ns = csi_init;
									while (buffer[csi_init] >= '0' && buffer[csi_init] <= '9') csi_init++;
									int num = 0;
									try { num = string(buffer + ns, csi_init - ns, Encoding::ANSI).ToUInt32(); } catch (...) {}
									seq.args[seq.num_args] = num;
									seq.num_args++;
								} else if (buffer[csi_init] == ';') {
									csi_init++;
								} else {
									seq.second = buffer[csi_init];
									csi_init++;
								}
							}
							used = csi_fin + 1;
							return true;
						} else if (seq.intro == ']' || seq.intro == 'P' || seq.intro == '^' || seq.intro == '_') {
							int s = used + 2;
							while (s < length) {
								if (buffer[s] == '\a') break;
								if (s + 1 < length && buffer[s] == '\33' && buffer[s + 1] == '\\') break;
								s++;
							}
							if (s < length) {
								seq.os_cmd = string(buffer + used + 2, s - used - 2, Encoding::UTF8).NormalizedForm();
								if (buffer[s] == L'\a') used = s + 1;
								else used = s + 2;
								return true;
							} else return false;
						} else {
							used += 2;
							return true;
						}
					} else return false;
				} else {
					int s = used;
					while (used < length && buffer[used] != '\33') used++;
					if (state.encoding_mode == 0) {
						seq.os_cmd = string(buffer + s, used - s, Encoding::UTF8).NormalizedForm();
					}

					// TODO: ADD FRAME DRAWING MODE
					
					return true;
				}
			} else return false;
		}

		void ProcessTerminalInput(TerminalProcessingState & state, IPseudoTerminal & pty, ConsoleState & console, const char * input, int length, int & length_used)
		{
			try {
				length_used = 0;
				SafePointer< Array<InputSequence> > sequence_array = new Array<InputSequence>(0x80);
				InputSequence sequence;
				while (ReadInputSequence(state, input, length, sequence, length_used)) {
					if (!InthreadProcess(state, sequence)) sequence_array->Append(sequence);
				}
				if (sequence_array->Length()) {
					SafePointer<IPseudoTerminal> _pty;
					SafePointer<ConsoleState> _con;
					_pty.SetRetain(&pty);
					_con.SetRetain(&console);
					Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([&state, sequence_array, _pty, _con]() {
						for (auto & s : sequence_array->Elements()) ExthreadProcess(state, *_pty, *_con, s);
					}));
				}
			} catch (...) {}
		}
		void TerminalStateInit(TerminalProcessingState & state)
		{
			state.encoding_mode = 0;
			state.caret_revert_style = CaretStyle::Null;
		}
	}
}