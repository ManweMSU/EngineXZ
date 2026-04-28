#include "uiml.h"

#include <UserInterface/InterfaceFormat.h>
#include <UserInterface/ControlClasses.h>
#include <Miscellaneous/DynamicString.h>

#include <Syntax/Tokenization.h>

using namespace Engine::Streaming;
using namespace Engine::Syntax;

namespace Engine
{
	namespace UI
	{
		namespace Markup
		{
			SafePointer<IWarningReporter> warning_reporter;
			SafePointer<IFileProvider> file_provider;
			SafePointer<Storage::RegistryNode> subordering;
			void SetWarningReporterCallback(IWarningReporter * callback) { warning_reporter.SetRetain(callback); }
			IWarningReporter * GetWarningReporterCallback(void) { return warning_reporter; }
			void SetFileProvider(IFileProvider * provider) { file_provider.SetRetain(provider); }
			IFileProvider * GetFileProvider(void) { return file_provider; }
			void SetSuborderingTable(Storage::RegistryNode * table) { subordering.SetRetain(table); }
			Storage::RegistryNode * GetSuborderingTable(void) { return subordering; }
			class CustomControlClass : public Object, public Template::ControlReflectedBase
			{
				struct _class_property {
					string name;
					Reflection::PropertyType type;
				};
				string _class_name;
				Array<_class_property> _class_properties;
				static Reflection::PropertyInfo _make_prop_info(const _class_property & prop)
				{
					Reflection::PropertyInfo info;
					info.Address = reinterpret_cast<void *>(1);
					info.Volume = info.ElementSize = 0;
					info.Type = prop.type;
					info.InnerType = Reflection::PropertyType::Unknown;
					info.Name = prop.name;
					return info;
				}
			public:
				CustomControlClass(const string & name) : _class_name(name), _class_properties(0x80) {}
				virtual ~CustomControlClass(void) override {}
				virtual void EnumerateProperties(Reflection::IPropertyEnumerator & enumerator) override
				{
					for (auto & p : _class_properties) enumerator.EnumerateProperty(p.name, reinterpret_cast<void *>(1), p.type, Reflection::PropertyType::Unknown, 0, 0);
				}
				virtual Reflection::PropertyInfo GetProperty(const string & name) override
				{
					for (auto & p : _class_properties) if (p.name == name) return _make_prop_info(p);
					Reflection::PropertyInfo info;
					info.Address = 0;
					info.Volume = info.ElementSize = 0;
					info.Type = info.InnerType = Reflection::PropertyType::Unknown;
					info.Name = L"";
					return info;
				}
				virtual string GetTemplateClass(void) const override { return _class_name; }
				bool HasProperty(const string & name) const
				{
					for (auto & p : _class_properties) if (p.name == name) return true;
					return false;
				}
				void AddProperty(const string & name, Reflection::PropertyType type)
				{
					_class_property prop;
					prop.name = name;
					prop.type = type;
					_class_properties << prop;
				}
			};
			class SourceLoadingException : public Exception
			{
			public:
				string FileName;
				SourceLoadingException(const string & file) : FileName(file) {}
			};
			class InnerSourceErrorException : public Exception
			{
			public:
				string FileName;
				int OuterPosition;
				InnerSourceErrorException(const string & file, int pos) : FileName(file), OuterPosition(pos) {}
			};
			class InnerSourceWrongIncludeException : public Exception
			{
			public:
				int OuterPosition;
				InnerSourceWrongIncludeException(int pos) : OuterPosition(pos) {}
			};
			class SyntaxException : public Exception
			{
			public:
				int StartPosition;
				int EndPosition;
				ErrorClass LocalError;
				SyntaxException(const string & Code, Array<Token> & Tokens, int Position, ErrorClass cls)
				{
					StartPosition = Tokens[Position].SourcePosition;
					LocalError = cls;
					int NextToken = Position;
					while (NextToken < Tokens.Length() && Tokens[NextToken].SourcePosition == StartPosition) NextToken++;
					EndPosition = Tokens[NextToken].SourcePosition;
					while (EndPosition > StartPosition && (Code[EndPosition] == L' ' || Code[EndPosition] < 32)) EndPosition--;
				}
			};
			struct MacroInfo {
				struct Macro
				{
					Array<string> arguments = Array<string>(0x10);
					SafePointer< Array<Token> > tokens;
				};
				Volumes::Dictionary<string, Macro> Macroses;
			};
			string LoadSource(const string & file_name, string & effective_file_name)
			{
				try {
					SafePointer<Stream> source;
					if (file_provider) {
						effective_file_name = file_provider->LocateFile(file_name);
						if (!effective_file_name.Length()) throw IO::FileAccessException(IO::Error::FileNotFound);
						source = file_provider->OpenFile(effective_file_name);
					} else {
						effective_file_name = file_name;
						source = new FileStream(file_name, AccessRead, OpenExisting);
					}
					TextReader reader(source);
					DynamicString result;
					while (!reader.EofReached()) {
						auto line = reader.ReadLine();
						if (line.Length() > 1 && line[0] == L'#' && line[1] == L'#') result << L'\n';
						else result << line << L'\n';
					}
					return result.ToString();
				} catch (...) {
					throw SourceLoadingException(file_name);
				}
			}
			Array<Token> * ParseSource(const string & file_name, MacroInfo & macro, const Array<string> & include, Spelling & spelling, string * store = 0)
			{
				string code, effective_file_name;
				code = LoadSource(file_name, effective_file_name);
				if (store) *store = code;
				SafePointer< Array<Token> > local_code = ParseText(code, spelling);
				string inner_file;
				string current_path = IO::Path::GetDirectory(effective_file_name);
				int outer_pos = -1;
				try {
					for (int i = 0; i < local_code->Length(); i++) {
						MacroInfo::Macro * mptr;
						if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L"#") {
							if ((local_code->ElementAt(i + 1).Class == TokenClass::Constant && local_code->ElementAt(i + 1).ValueClass == TokenConstantClass::String) ||
								local_code->ElementAt(i + 1).Class == TokenClass::Identifier) {
								outer_pos = local_code->ElementAt(i + 1).SourcePosition;
								string resource = (local_code->ElementAt(i + 1).Class == TokenClass::Identifier) ? (local_code->ElementAt(i + 1).Content + L"/main.uiml") : local_code->ElementAt(i + 1).Content;
								if (file_provider) inner_file = file_provider->LocateFile(current_path + L"/" + resource); else {
									auto test = current_path + L"/" + resource;
									if (IO::FileExists(test)) inner_file = IO::ExpandPath(test); else inner_file = L"";
								}
								for (auto & i : include) {
									if (inner_file.Length()) break;
									auto test = i + L"/" + resource;
									if (file_provider) inner_file = file_provider->LocateFile(test); else {
										if (IO::FileExists(test)) inner_file = IO::ExpandPath(test); else inner_file = L"";
									}
								}
								if (!inner_file.Length()) throw IO::FileAccessException(IO::Error::FileNotFound);
								SafePointer< Array<Token> > inner_code = ParseSource(inner_file, macro, include, spelling);
								for (int j = 0; j < inner_code->Length(); j++) inner_code->ElementAt(j).SourcePosition = outer_pos;
								inner_code->RemoveLast();
								SafePointer< Array<Token> > new_local = new Array<Token>(0x1000);
								new_local->Append(local_code->GetBuffer(), i);
								new_local->Append(*inner_code);
								new_local->Append(local_code->GetBuffer() + i + 2, local_code->Length() - i - 2);
								local_code.SetRetain(new_local);
								i += inner_code->Length() - 1;
							} else {
								throw InnerSourceWrongIncludeException(local_code->ElementAt(i + 1).SourcePosition);
							}
						} else if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L"&" && local_code->ElementAt(i + 1).Class == TokenClass::Constant && local_code->ElementAt(i + 1).ValueClass == TokenConstantClass::String) {
							local_code->ElementAt(i + 1).Content = IO::ExpandPath(current_path + L"/" + local_code->ElementAt(i + 1).Content);
						} else if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L"$" && local_code->ElementAt(i + 1).Class == TokenClass::Identifier) {
							int mbegin = i;
							int mend = i + 1;
							auto & name = local_code->ElementAt(i + 1).Content;
							while (mend < local_code->Length() && (local_code->ElementAt(mend).Class != TokenClass::CharCombo || local_code->ElementAt(mend).Content != L"$")) mend++;
							if (mend < local_code->Length() && !macro.Macroses.ElementExists(name)) {
								MacroInfo::Macro m;
								m.tokens = new Array<Token>(1);
								m.tokens->Append(local_code->GetBuffer() + mbegin + 2, mend - mbegin - 2);
								macro.Macroses.Append(name, m);
								int remove = mend - mbegin + 1;
								for (int j = mend + 1; j < local_code->Length(); j++) local_code->ElementAt(j - remove) = local_code->ElementAt(j);
								local_code->SetLength(local_code->Length() - remove);
								i--;
							}
						} else if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L"$" && local_code->ElementAt(i + 1).Class == TokenClass::CharCombo && local_code->ElementAt(i + 1).Content == L"$") {
							Array<string> args(0x10);
							int mbegin = i;
							int mend = i + 2;
							while (mend < local_code->Length() && (local_code->ElementAt(mend).Class != TokenClass::CharCombo || local_code->ElementAt(mend).Content != L"$")) mend++;
							int mmid = mend++;
							while (mend < local_code->Length() && (local_code->ElementAt(mend).Class != TokenClass::CharCombo || local_code->ElementAt(mend).Content != L"$")) mend++;
							if (mmid < local_code->Length()) {
								for (int j = mbegin + 2; j < mmid; j++) {
									auto & token = local_code->ElementAt(j);
									if (token.Class == TokenClass::Identifier) args << token.Content;
								}
							}
							if (mend < local_code->Length() && args.Length() && !macro.Macroses.ElementExists(args[0])) {
								MacroInfo::Macro m;
								m.arguments.Append(args.GetBuffer() + 1, args.Length() - 1);
								m.tokens = new Array<Token>(1);
								m.tokens->Append(local_code->GetBuffer() + mmid + 1, mend - mmid - 1);
								macro.Macroses.Append(args[0], m);
								int remove = mend - mbegin + 1;
								for (int j = mend + 1; j < local_code->Length(); j++) local_code->ElementAt(j - remove) = local_code->ElementAt(j);
								local_code->SetLength(local_code->Length() - remove);
								i--;
							}
						} else if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L"$" && local_code->ElementAt(i + 1).Class == TokenClass::CharCombo && local_code->ElementAt(i + 1).Content == L"-") {
							int mbegin = i;
							i += 2;
							if (local_code->ElementAt(i).Class == TokenClass::Identifier) {
								macro.Macroses.Remove(local_code->ElementAt(i).Content);
								i++;
								SafePointer< Array<Token> > new_local = new Array<Token>(0x1000);
								new_local->Append(local_code->GetBuffer(), mbegin);
								new_local->Append(local_code->GetBuffer() + i, local_code->Length() - i);
								local_code.SetRetain(new_local);
								i = mbegin - 1;
							}
						} else if (local_code->ElementAt(i).Class == TokenClass::Identifier && (mptr = macro.Macroses[local_code->ElementAt(i).Content])) {
							if (mptr->arguments.Length()) {
								int mbegin = i++;
								ObjectArray< Array<Token> > subs(0x10);
								if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L"(") {
									i++;
									while (local_code->ElementAt(i).Class != TokenClass::CharCombo || local_code->ElementAt(i).Content != L")") {
										if (local_code->ElementAt(i).Class == TokenClass::CharCombo && local_code->ElementAt(i).Content == L",") i++;
										int abegin = i, level = 0;
										while (i < local_code->Length()) {
											auto & token = local_code->ElementAt(i);
											if (token.Class == TokenClass::CharCombo && token.Content == L"(") { level++; i++; }
											else if (token.Class == TokenClass::CharCombo && token.Content == L")") { if (level) { level--; i++; } else break; }
											else if (token.Class == TokenClass::CharCombo && token.Content == L"," && level == 0) break;
											else i++;
										}
										if (i == local_code->Length()) break;
										SafePointer< Array<Token> > arg = new Array<Token>(1);
										arg->Append(local_code->GetBuffer() + abegin, i - abegin);
										subs.Append(arg);
									}
									i++;
								}
								if (i > local_code->Length()) i = local_code->Length();
								if (subs.Length() == mptr->arguments.Length()) {
									Volumes::ObjectDictionary< string, Array<Token> > subsd;
									for (int j = 0; j < mptr->arguments.Length(); j++) subsd.Append(mptr->arguments[j], &subs[j]);
									SafePointer< Array<Token> > new_local = new Array<Token>(0x1000);
									new_local->Append(local_code->GetBuffer(), mbegin);
									for (auto & t : *mptr->tokens) {
										Array<Token> * argsub;
										if (t.Class == TokenClass::Identifier && (argsub = subsd[t.Content])) new_local->Append(*argsub);
										else new_local->Append(t);
									}
									new_local->Append(local_code->GetBuffer() + i, local_code->Length() - i);
									local_code.SetRetain(new_local);
									i = mbegin - 1;
								}
							} else {
								SafePointer< Array<Token> > new_local = new Array<Token>(0x1000);
								new_local->Append(local_code->GetBuffer(), i);
								new_local->Append(*mptr->tokens);
								new_local->Append(local_code->GetBuffer() + i + 1, local_code->Length() - i - 1);
								local_code.SetRetain(new_local);
								i--;
							}
						}
					}
					local_code->Retain();
					return local_code;
				} catch (SourceLoadingException) {
					throw;
				} catch (InnerSourceWrongIncludeException & e) {
					throw ParserSpellingException(e.OuterPosition, L"Invalid include directive.");
				} catch (...) {
					throw InnerSourceErrorException(inner_file, outer_pos);
				}
			}
			bool CheckPlatformName(const string & name)
			{
				if (name == L"Windows") return true;
				else if (name == L"MacOSX") return true;
				else if (name == L"Linux") return true;
				else return false;
			}
			struct DynamicInfo {
				typedef Array<string> LocaleSet;
				struct PlatformContents {
					string PlatformName;
					Array<Format::InterfaceColor> Colors;
					Array<Format::InterfaceTexture> Textures;
					Array<Format::InterfaceFont> Fonts;
					Array<Format::InterfaceApplication> Applications;
					Array<Format::InterfaceDialog> Styles;
					SafeArray<Format::InterfaceDialog> Dialogs;

					PlatformContents(void) : Colors(0x10), Textures(0x10), Fonts(0x10), Applications(0x40), Styles(0x40), Dialogs(0x40) {}
					int GetColorIndex(int ID)
					{
						for (int i = 0; i < Colors.Length(); i++) if (Colors[i].ID == ID) return i;
						return -1;
					}
					int GetTextureIndex(const string & name)
					{
						for (int i = 0; i < Textures.Length(); i++) if (Textures[i].Name == name) return i;
						return -1;
					}
					int GetFontIndex(const string & name)
					{
						for (int i = 0; i < Fonts.Length(); i++) if (Fonts[i].Name == name) return i;
						return -1;
					}
					int GetApplicationIndex(const string & name)
					{
						for (int i = 0; i < Applications.Length(); i++) if (Applications[i].Name == name) return i;
						return -1;
					}
					int GetStyleIndex(const string & name, const string & cls)
					{
						for (int i = 0; i < Styles.Length(); i++) if (Styles[i].Name == name && Styles[i].Root.Class == cls) return i;
						return -1;
					}
					int GetDialogIndex(const string & name)
					{
						for (int i = 0; i < Dialogs.Length(); i++) if (Dialogs[i].Name == name) return i;
						return -1;
					}
				};
				struct LocaleString {
					string HumanIdentifier;
					int MachineIdentifier;
					LocaleSet Locales;
					LocaleSet Variants;
				};
				struct ColorAlias {
					string HumanIdentifier;
					int MachineIdentifier;
				};
				struct DefaultStyle {
					string Class;
					string Style;
				};
				struct ControlClassInfo
				{
					string Name;
					SafePointer<CustomControlClass> Base;
					Volumes::Set<string> AllowedChildren;
				};
				enum class ConstantType { Integer, Double, Coordinate, CoordinateTemplate, Argument };
				struct Constant {
					string Name;
					ConstantType Type;
					Template::Coordinate Value;
					bool CanBeCasted(ConstantType to) {
						if (Type == ConstantType::Integer) return true;
						if (Type == ConstantType::Double) return true;
						if (Type == ConstantType::Coordinate) {
							if (to == ConstantType::Coordinate || to == ConstantType::CoordinateTemplate) return true;
							return false;
						}
						if (Type == ConstantType::CoordinateTemplate) {
							if (to == ConstantType::CoordinateTemplate) return true;
							return false;
						}
						if (Type == ConstantType::Argument) {
							if (to == ConstantType::Argument || to == ConstantType::CoordinateTemplate) return true;
							return false;
						}
						return false;
					}
					void Cast(ConstantType to) {
						if (Type == ConstantType::Integer) {
							if (to == ConstantType::Double) {
								Value.Zoom.GetValue() = double(Value.Absolute.GetValue());
								Value.Absolute.GetValue() = 0;
							}
						} else if (Type == ConstantType::Double) {
							if (to != ConstantType::Double) {
								Value.Absolute.GetValue() = int(Value.Zoom.GetValue());
								Value.Zoom.GetValue() = 0.0;
							}
						} else if (Type == ConstantType::Argument) {
							if (to == ConstantType::CoordinateTemplate) {
								Value.Absolute = Template::IntegerTemplate::Undefined(Name);
								Name = L"";
							}
						}
						Type = to;
					}
					Coordinate ToCoordinate(void) const { return Coordinate(Value.Absolute.GetValue(), Value.Zoom.GetValue(), Value.Anchor.GetValue()); }
				};
				LocaleSet LastSet;
				LocaleSet GlobalLocaleSet;
				int TextureID;
				int UnnamedObject;

				Volumes::Set<string> CommonClasses;
				Array<ControlClassInfo> ClassData;

				Array<Constant> Constants;
				Array<LocaleString> Strings;
				Array<ColorAlias> Colors;
				Array<DefaultStyle> DefaultStyles;
				Array<PlatformContents> Contents;

				const string & Code;
				Format::InterfaceTemplateImage & Output;
				Array<Token> & Tokens;
				int Position;

				DynamicInfo(const string & code, Array<Token> & src, Format::InterfaceTemplateImage & out) : LastSet(0x10), GlobalLocaleSet(0x10), TextureID(1), UnnamedObject(0), ClassData(0x100),
					Constants(0x20), Strings(0x100), Colors(0x40), DefaultStyles(0x20), Contents(0x10), Code(code), Output(out), Tokens(src), Position(0)
				{
					GlobalLocaleSet << L"_"; LastSet << L"_";
					if (subordering) {
						SafePointer<Storage::RegistryNode> com_node = subordering->OpenNode(L"@common");
						if (com_node) {
							for (auto & v : com_node->GetValues()) if (com_node->GetValueBoolean(v)) CommonClasses.AddElement(v);
						}
						for (auto & cn : subordering->GetSubnodes()) if (cn[0] != L'@') {
							ControlClassInfo cls;
							cls.Name = cn;
							SafePointer<Storage::RegistryNode> cls_node = subordering->OpenNode(cn);
							if (cls_node) {
								for (auto & v : cls_node->GetValues()) if (cls_node->GetValueBoolean(v)) cls.AllowedChildren.AddElement(v);
							}
							ClassData << cls;
						}
					}
				}
				Token & GetToken(int shift = 0) { return Tokens[Position + shift]; }
				void MoveNext(int shift = 1) { Position += shift; }
				int GetStringID(const string & name)
				{
					for (int i = 0; i < Strings.Length(); i++) if (Strings[i].HumanIdentifier == name) return Strings[i].MachineIdentifier;
					return -1;
				}
				int GetColorID(const string & name)
				{
					for (int i = 0; i < Colors.Length(); i++) if (Colors[i].HumanIdentifier == name) return Colors[i].MachineIdentifier;
					return -1;
				}
				int GetConstantIndex(const string & name)
				{
					for (int i = 0; i < Constants.Length(); i++) if (Constants[i].Name == name) return i;
					return -1;
				}
				PlatformContents & GetContentsFor(const string & platform)
				{
					for (int i = 0; i < Contents.Length(); i++) if (Contents[i].PlatformName == platform) return Contents[i];
					Contents << PlatformContents();
					Contents.LastElement().PlatformName = platform;
					return Contents.LastElement();
				}
				Format::InterfaceApplication * FindApplication(const string & name)
				{
					for (int p = 0; p < Contents.Length(); p++) for (int a = 0; a < Contents[p].Applications.Length(); a++) if (Contents[p].Applications[a].Name == name) return &Contents[p].Applications[a];
					return 0;
				}
				string GetDefaultStyle(const string & class_for)
				{
					for (int i = 0; i < DefaultStyles.Length(); i++) if (DefaultStyles[i].Class == class_for) return DefaultStyles[i].Style;
					return L"";
				}
				void SetDefaultStyle(const string & class_for, const string & style)
				{
					for (int i = 0; i < DefaultStyles.Length(); i++) if (DefaultStyles[i].Class == class_for) {
						DefaultStyles[i].Style = style;
						return;
					}
					DefaultStyle stl;
					stl.Class = class_for; stl.Style = style;
					DefaultStyles << stl;
				}
				int GetUnnamed(void) { UnnamedObject++; return UnnamedObject; }
			};
#define DED(info) info.Code, info.Tokens, info.Position
			DynamicInfo::Constant EXPRESSION(DynamicInfo & info);
			DynamicInfo::Constant NUMERIC_VALUE(DynamicInfo & info)
			{
				if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"(") {
					info.MoveNext();
					auto Result = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L")") throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					return Result;
				} else if (info.GetToken().Class == TokenClass::Identifier) {
					int index = info.GetConstantIndex(info.GetToken().Content);
					if (index == -1) throw SyntaxException(DED(info), ErrorClass::UndefinedObject);
					info.MoveNext();
					auto Result = info.Constants[index];
					Result.Name = L"";
					return Result;
				} else if (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::Numeric) {
					DynamicInfo::Constant Result;
					Result.Value.Absolute = Template::IntegerTemplate(0);
					Result.Value.Zoom = Template::DoubleTemplate(0.0);
					Result.Value.Anchor = Template::DoubleTemplate(0.0);
					if (info.GetToken().NumericClass() == NumericTokenClass::Integer) {
						Result.Type = DynamicInfo::ConstantType::Integer;
						Result.Value.Absolute = Template::IntegerTemplate(int(info.GetToken().AsInteger()));
					} else {
						Result.Type = DynamicInfo::ConstantType::Double;
						Result.Value.Zoom = Template::DoubleTemplate(info.GetToken().AsDouble());
					}
					info.MoveNext();
					return Result;
				} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"@") {
					info.MoveNext();
					if ((info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::String) && info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					DynamicInfo::Constant Result;
					Result.Name = info.GetToken().Content;
					Result.Type = DynamicInfo::ConstantType::Argument;
					Result.Value.Absolute = Template::IntegerTemplate(0);
					Result.Value.Zoom = Template::DoubleTemplate(0.0);
					Result.Value.Anchor = Template::DoubleTemplate(0.0);
					info.MoveNext();
					return Result;
				} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
			}
			DynamicInfo::Constant SUMMAND(DynamicInfo & info)
			{
				DynamicInfo::Constant Value = NUMERIC_VALUE(info);
				while (info.GetToken().Class == TokenClass::CharCombo && (info.GetToken().Content == L"*" || info.GetToken().Content == L"/")) {
					string op = info.GetToken().Content;
					info.MoveNext();
					int pos_were = info.Position;
					DynamicInfo::Constant Arg = NUMERIC_VALUE(info);
					bool failed = false;
					if (Value.Type == DynamicInfo::ConstantType::Integer) {
						if (Arg.Type == DynamicInfo::ConstantType::Integer) {
							if (op == L"*") Value.Value.Absolute.GetValue() *= Arg.Value.Absolute.GetValue();
							else Value.Value.Absolute.GetValue() /= Arg.Value.Absolute.GetValue();
						} else if (Arg.Type == DynamicInfo::ConstantType::Double) {
							Value.Cast(DynamicInfo::ConstantType::Double);
							if (op == L"*") Value.Value.Zoom.GetValue() *= Arg.Value.Zoom.GetValue();
							else Value.Value.Zoom.GetValue() /= Arg.Value.Zoom.GetValue();
						} else if (Arg.Type == DynamicInfo::ConstantType::Coordinate && op == L"*") {
							swap(Value, Arg);
							Value.Value.Absolute.GetValue() *= Arg.Value.Absolute.GetValue();
							Value.Value.Zoom.GetValue() *= double(Arg.Value.Absolute.GetValue());
							Value.Value.Anchor.GetValue() *= double(Arg.Value.Absolute.GetValue());
						} else failed = true;
					} else if (Value.Type == DynamicInfo::ConstantType::Double) {
						if (Arg.Type == DynamicInfo::ConstantType::Integer) {
							Arg.Cast(DynamicInfo::ConstantType::Double);
							if (op == L"*") Value.Value.Zoom.GetValue() *= Arg.Value.Zoom.GetValue();
							else Value.Value.Zoom.GetValue() /= Arg.Value.Zoom.GetValue();
						} else if (Arg.Type == DynamicInfo::ConstantType::Double) {
							if (op == L"*") Value.Value.Zoom.GetValue() *= Arg.Value.Zoom.GetValue();
							else Value.Value.Zoom.GetValue() /= Arg.Value.Zoom.GetValue();
						} else if (Arg.Type == DynamicInfo::ConstantType::Coordinate && op == L"*") {
							swap(Value, Arg);
							Value.Value.Absolute.GetValue() = int(double(Value.Value.Absolute.GetValue()) * Arg.Value.Zoom.GetValue());
							Value.Value.Zoom.GetValue() *= Arg.Value.Zoom.GetValue();
							Value.Value.Anchor.GetValue() *= Arg.Value.Zoom.GetValue();
						} else failed = true;
					} else if (Value.Type == DynamicInfo::ConstantType::Coordinate && op == L"*") {
						if (Arg.Type == DynamicInfo::ConstantType::Integer) {
							Value.Value.Absolute.GetValue() *= Arg.Value.Absolute.GetValue();
							Value.Value.Zoom.GetValue() *= double(Arg.Value.Absolute.GetValue());
							Value.Value.Anchor.GetValue() *= double(Arg.Value.Absolute.GetValue());
						} else if (Arg.Type == DynamicInfo::ConstantType::Double) {
							Value.Value.Absolute.GetValue() = int(double(Value.Value.Absolute.GetValue()) * Arg.Value.Zoom.GetValue());
							Value.Value.Zoom.GetValue() *= Arg.Value.Zoom.GetValue();
							Value.Value.Anchor.GetValue() *= Arg.Value.Zoom.GetValue();
						} else failed = true;
					} else if (Value.Type == DynamicInfo::ConstantType::Coordinate && op == L"/") {
						if (Arg.Type == DynamicInfo::ConstantType::Integer) {
							Value.Value.Absolute.GetValue() /= Arg.Value.Absolute.GetValue();
							Value.Value.Zoom.GetValue() /= double(Arg.Value.Absolute.GetValue());
							Value.Value.Anchor.GetValue() /= double(Arg.Value.Absolute.GetValue());
						} else if (Arg.Type == DynamicInfo::ConstantType::Double) {
							Value.Value.Absolute.GetValue() = int(double(Value.Value.Absolute.GetValue()) / Arg.Value.Zoom.GetValue());
							Value.Value.Zoom.GetValue() /= Arg.Value.Zoom.GetValue();
							Value.Value.Anchor.GetValue() /= Arg.Value.Zoom.GetValue();
						} else failed = true;
					} else failed = true;
					if (failed) {
						info.Position = pos_were;
						throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
					}
				}
				return Value;
			}
			DynamicInfo::Constant EXPRESSION(DynamicInfo & info)
			{
				DynamicInfo::Constant Value;
				Value.Type = DynamicInfo::ConstantType::Integer;
				Value.Value.Absolute = Template::IntegerTemplate(0);
				Value.Value.Zoom = Template::DoubleTemplate(0.0);
				Value.Value.Anchor = Template::DoubleTemplate(0.0);
				string op = L"";
				string subop = L"";
				do {
					if (op == L"") {
						if ((info.GetToken().Class == TokenClass::CharCombo || info.GetToken().Class == TokenClass::Keyword) && (info.GetToken().Content == L"+" || info.GetToken().Content == L"-" || info.GetToken().Content == L"w" || info.GetToken().Content == L"z")) {
							op = info.GetToken().Content;
							info.MoveNext();
						} else op = L"+";
					} else {
						if ((info.GetToken().Class != TokenClass::CharCombo && info.GetToken().Class != TokenClass::Keyword) || (info.GetToken().Content != L"+" && info.GetToken().Content != L"-" && info.GetToken().Content != L"w" && info.GetToken().Content != L"z")) {
							throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						}
						op = info.GetToken().Content;
						info.MoveNext();
					}
					if ((op == L"w" || op == L"z") && info.GetToken().Class == TokenClass::CharCombo && (info.GetToken().Content == L"+" || info.GetToken().Content == L"-")) {
						subop = info.GetToken().Content;
						info.MoveNext();
					} else subop = L"+";
					int pos_were = info.Position;
					DynamicInfo::Constant Summand = SUMMAND(info);
					if (op == L"w" || op == L"z") {
						if (Value.Type == DynamicInfo::ConstantType::Argument) Value.Cast(DynamicInfo::ConstantType::CoordinateTemplate);
						else if (Value.Type == DynamicInfo::ConstantType::Double || Value.Type == DynamicInfo::ConstantType::Integer) Value.Cast(DynamicInfo::ConstantType::Coordinate);
						if (Summand.Type == DynamicInfo::ConstantType::Integer) {
							if (op == L"w") {
								Summand.Value.Anchor.GetValue() = double(Summand.Value.Absolute.GetValue());
								Summand.Value.Absolute.GetValue() = 0;
							} else {
								Summand.Value.Zoom.GetValue() = double(Summand.Value.Absolute.GetValue());
								Summand.Value.Absolute.GetValue() = 0;
							}
							Summand.Type = DynamicInfo::ConstantType::Coordinate;
						} else if (Summand.Type == DynamicInfo::ConstantType::Double) {
							if (op == L"w") {
								Summand.Value.Anchor.GetValue() = Summand.Value.Zoom.GetValue();
								Summand.Value.Zoom.GetValue() = 0.0;
							}
							Summand.Type = DynamicInfo::ConstantType::Coordinate;
						} else if (Summand.Type == DynamicInfo::ConstantType::Argument && subop == L"+") {
							if (op == L"w") {
								Summand.Value.Anchor = Template::DoubleTemplate::Undefined(Summand.Name);
								Summand.Name = L"";
							} else {
								Summand.Value.Zoom = Template::DoubleTemplate::Undefined(Summand.Name);
								Summand.Name = L"";
							}
							Summand.Type = DynamicInfo::ConstantType::CoordinateTemplate;
						} else {
							info.Position = pos_were;
							throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
						}
						op = subop;
					}
					if (Value.Type == DynamicInfo::ConstantType::Integer && Value.Value.Absolute.GetValue() == 0 && Summand.Type == DynamicInfo::ConstantType::Argument) {
						Value = Summand;
					} else {
						if (Summand.Type == DynamicInfo::ConstantType::Argument) Summand.Cast(DynamicInfo::ConstantType::CoordinateTemplate);
						if (Value.Type == DynamicInfo::ConstantType::Integer) Value.Cast(Summand.Type);
						else if (Value.Type == DynamicInfo::ConstantType::Double) {
							if (Summand.Type == DynamicInfo::ConstantType::Integer) Summand.Cast(Value.Type);
							else if (Summand.Type != DynamicInfo::ConstantType::Double) Value.Cast(Summand.Type);
						} else if (Summand.Type == DynamicInfo::ConstantType::Integer || Summand.Type == DynamicInfo::ConstantType::Double) Summand.Cast(Value.Type);
						bool failed = false;
						if (!Value.Value.Absolute.IsDefined()) {
							if (!Summand.Value.Absolute.IsDefined() || Summand.Value.Absolute.GetValue()) failed = true;
						} else if (!Summand.Value.Absolute.IsDefined()) {
							if (Value.Value.Absolute.GetValue()) failed = true; else {
								Value.Value.Absolute = Summand.Value.Absolute;
							}
						} else {
							if (op == L"+") Value.Value.Absolute.GetValue() += Summand.Value.Absolute.GetValue();
							else Value.Value.Absolute.GetValue() -= Summand.Value.Absolute.GetValue();
						}
						if (!Value.Value.Zoom.IsDefined()) {
							if (!Summand.Value.Zoom.IsDefined() || Summand.Value.Zoom.GetValue()) failed = true;
						} else if (!Summand.Value.Zoom.IsDefined()) {
							if (Value.Value.Zoom.GetValue()) failed = true; else {
								Value.Value.Zoom = Summand.Value.Zoom;
							}
						} else {
							if (op == L"+") Value.Value.Zoom.GetValue() += Summand.Value.Zoom.GetValue();
							else Value.Value.Zoom.GetValue() -= Summand.Value.Zoom.GetValue();
						}
						if (!Value.Value.Anchor.IsDefined()) {
							if (!Summand.Value.Anchor.IsDefined() || Summand.Value.Anchor.GetValue()) failed = true;
						} else if (!Summand.Value.Anchor.IsDefined()) {
							if (Value.Value.Anchor.GetValue()) failed = true; else {
								Value.Value.Anchor = Summand.Value.Anchor;
							}
						} else {
							if (op == L"+") Value.Value.Anchor.GetValue() += Summand.Value.Anchor.GetValue();
							else Value.Value.Anchor.GetValue() -= Summand.Value.Anchor.GetValue();
						}
						if (failed) {
							info.Position = pos_were;
							throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
						}
						if (!Value.Value.IsDefined()) Value.Type = DynamicInfo::ConstantType::CoordinateTemplate;
					}
				} while ((info.GetToken().Class == TokenClass::CharCombo || info.GetToken().Class == TokenClass::Keyword) && (info.GetToken().Content == L"+" || info.GetToken().Content == L"-" || info.GetToken().Content == L"w" || info.GetToken().Content == L"z"));
				return Value;
			}
			DynamicInfo::LocaleSet LOCALE_SET(DynamicInfo & info)
			{
				DynamicInfo::LocaleSet set(0x10);
				info.MoveNext();
				if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L']') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				while (info.GetToken().Class == TokenClass::Identifier) {
					string val = info.GetToken().Content.LowerCase();
					if (val != L"_" && val.Length() != 2) throw SyntaxException(DED(info), ErrorClass::InvalidLocaleIdentifier);
					set << val;
					info.MoveNext();
					bool was = false;
					for (int i = 0; i < info.GlobalLocaleSet.Length(); i++) if (info.GlobalLocaleSet[i] == val) { was = true; break; }
					if (!was) info.GlobalLocaleSet << val;
				}
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L']') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				return set;
			}
			Format::InterfaceColor COLOR_OBJECT(DynamicInfo & info)
			{
				Format::InterfaceColor result;
				result.ID = 0;
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				if (info.GetToken().Class == TokenClass::Identifier) {
					result.IsSystemColor = true;
					result.Value.Value = 0;
					result.Value.a = 255;
					auto & sysclr = info.GetToken().Content;
					if (sysclr == L"Theme") result.Value.r = 1;
					else if (sysclr == L"WindowBackgroup") result.Value.r = 2;
					else if (sysclr == L"WindowText") result.Value.r = 3;
					else if (sysclr == L"SelectedBackground") result.Value.r = 4;
					else if (sysclr == L"SelectedText") result.Value.r = 5;
					else if (sysclr == L"MenuBackground") result.Value.r = 6;
					else if (sysclr == L"MenuText") result.Value.r = 7;
					else if (sysclr == L"MenuHotBackground") result.Value.r = 8;
					else if (sysclr == L"MenuHotText") result.Value.r = 9;
					else if (sysclr == L"GrayedText") result.Value.r = 10;
					else if (sysclr == L"Hyperlink") result.Value.r = 11;
					else throw SyntaxException(DED(info), ErrorClass::InvalidSystemColor);
					info.MoveNext();
				} else if (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::Numeric) {
					result.IsSystemColor = false;
					result.Value.Value = 0;
					result.Value.a = 255;
					if (info.GetToken().NumericClass() == NumericTokenClass::Integer) {
						auto val = info.GetToken().AsInteger();
						if (val > 255) result.Value.r = 255; else result.Value.r = uint8(val);
					} else {
						auto val = min(max(info.GetToken().AsDouble(), 0.0), 1.0);
						result.Value.r = uint8(255.0 * val);
					}
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::Numeric) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					if (info.GetToken().NumericClass() == NumericTokenClass::Integer) {
						auto val = info.GetToken().AsInteger();
						if (val > 255) result.Value.g = 255; else result.Value.g = uint8(val);
					} else {
						auto val = min(max(info.GetToken().AsDouble(), 0.0), 1.0);
						result.Value.g = uint8(255.0 * val);
					}
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::Numeric) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					if (info.GetToken().NumericClass() == NumericTokenClass::Integer) {
						auto val = info.GetToken().AsInteger();
						if (val > 255) result.Value.b = 255; else result.Value.b = uint8(val);
					} else {
						auto val = min(max(info.GetToken().AsDouble(), 0.0), 1.0);
						result.Value.b = uint8(255.0 * val);
					}
					info.MoveNext();
				} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L',') {
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::Numeric) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					if (info.GetToken().NumericClass() == NumericTokenClass::Integer) {
						auto val = info.GetToken().AsInteger();
						if (val > 255) result.Value.a = 255; else result.Value.a = uint8(val);
					} else {
						auto val = min(max(info.GetToken().AsDouble(), 0.0), 1.0);
						result.Value.a = uint8(255.0 * val);
					}
					info.MoveNext();
				}
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				return result;
			}
			Codec::Image * IMAGE_IMPORT(DynamicInfo & info)
			{
				double scale = 0.0;
				if (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::Numeric) {
					scale = info.GetToken().AsDouble();
					info.MoveNext();
				}
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'&') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::String) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				string path = info.GetToken().Content;
				SafePointer<Codec::Image> result;
				{
					try {
						SafePointer<Stream> stream;
						if (file_provider) {
							auto name = file_provider->LocateFile(path);
							if (!name.Length()) throw IO::FileAccessException(IO::Error::FileNotFound);
							stream = file_provider->OpenFile(name);
						} else stream = new FileStream(path, AccessRead, OpenExisting);
						result = Codec::DecodeImage(stream);
						if (!result) throw Exception();
					}
					catch (...) { throw SourceLoadingException(path); }
				}
				if (scale) for (int i = 0; i < result->Frames.Length(); i++) result->Frames.ElementAt(i)->DpiUsage = scale;
				for (int i = 0; i < result->Frames.Length(); i++) {
					SafePointer<Codec::Frame> conv = result->Frames.ElementAt(i)->ConvertFormat(Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Normal, Codec::ScanOrigin::BottomUp);
					result->Frames.SetElement(conv, i);
				}
				info.MoveNext();
				while (info.GetToken().Class == TokenClass::Identifier) {
					auto & effect = info.GetToken().Content;
					if (effect == L"Grayscale") {
						for (int f = 0; f < result->Frames.Length(); f++) {
							auto & frame = result->Frames[f];
							for (int y = 0; y < frame.GetHeight(); y++) for (int x = 0; x < frame.GetWidth(); x++) {
								Color clr = frame.GetPixel(x, y);
								uint8 v = uint8((uint32(clr.r) + uint32(clr.g) + uint32(clr.b)) / 3);
								clr.r = clr.g = clr.b = v;
								frame.SetPixel(x, y, clr);
							}
						}
					} else if (effect == L"BlindGrayscale") {
						for (int f = 0; f < result->Frames.Length(); f++) {
							auto & frame = result->Frames[f];
							for (int y = 0; y < frame.GetHeight(); y++) for (int x = 0; x < frame.GetWidth(); x++) {
								Color clr = frame.GetPixel(x, y);
								uint8 v = 64 + uint8((uint32(clr.r) + uint32(clr.g) + uint32(clr.b)) / 6);
								clr.r = clr.g = clr.b = v;
								frame.SetPixel(x, y, clr);
							}
						}
					} else throw SyntaxException(DED(info), ErrorClass::InvalidEffect);
					info.MoveNext();
				}
				result->Retain();
				return result;
			}
			Codec::Image * TEXTURE_OBJECT(DynamicInfo & info)
			{
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				SafePointer<Codec::Image> result = IMAGE_IMPORT(info);
				while (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L',') {
					info.MoveNext();
					SafePointer<Codec::Image> next = IMAGE_IMPORT(info);
					result->Frames << next->Frames;
				}
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				result->Retain();
				return result;
			}
			void FONT_PROP(DynamicInfo & info, Format::InterfaceFont & font)
			{
				auto & prop = info.GetToken().Content;
				if (prop == L"face") {
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::String) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					font.FontFace = info.GetToken().Content;
					info.MoveNext();
				} else if (prop == L"serif") {
					info.MoveNext();
					font.FontFace = Graphics::SystemSerifFont;
				} else if (prop == L"sans_serif") {
					info.MoveNext();
					font.FontFace = Graphics::SystemSansSerifFont;
				} else if (prop == L"mono_serif") {
					info.MoveNext();
					font.FontFace = Graphics::SystemMonoSerifFont;
				} else if (prop == L"mono_sans_serif") {
					info.MoveNext();
					font.FontFace = Graphics::SystemMonoSansSerifFont;
				} else if (prop == L"height") {
					info.MoveNext();
					int pos_were = info.Position;
					auto value = EXPRESSION(info);
					if (!value.CanBeCasted(DynamicInfo::ConstantType::Coordinate)) {
						info.Position = pos_were;
						throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
					}
					value.Cast(DynamicInfo::ConstantType::Coordinate);
					font.Height.Absolute = value.Value.Absolute.GetValue();
					font.Height.Scalable = value.Value.Zoom.GetValue();
					font.Height.Anchor = 0.0;
				} else if (prop == L"weight") {
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::Numeric || info.GetToken().NumericClass() != NumericTokenClass::Integer) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					font.Weight = int(info.GetToken().AsInteger());
					info.MoveNext();
				} else if (prop == L"italic") {
					info.MoveNext();
					font.IsItalic = true;
				} else if (prop == L"underline") {
					info.MoveNext();
					font.IsUnderline = true;
				} else if (prop == L"strikeout") {
					info.MoveNext();
					font.IsStrikeout = true;
				} else throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
			}
			Rectangle RECTANGLE(DynamicInfo & info)
			{
				if (info.GetToken().Class == TokenClass::Identifier && info.GetToken().Content == L"parent") {
					info.MoveNext();
					return Rectangle::Entire();
				} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"{") {
					info.MoveNext();
					int pos_were;
					pos_were = info.Position;
					auto left = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					if (!left.CanBeCasted(DynamicInfo::ConstantType::Coordinate)) { info.Position = pos_were; throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch); }
					pos_were = info.Position;
					auto top = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					if (!top.CanBeCasted(DynamicInfo::ConstantType::Coordinate)) { info.Position = pos_were; throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch); }
					pos_were = info.Position;
					auto right = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					if (!right.CanBeCasted(DynamicInfo::ConstantType::Coordinate)) { info.Position = pos_were; throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch); }
					pos_were = info.Position;
					auto bottom = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					if (!bottom.CanBeCasted(DynamicInfo::ConstantType::Coordinate)) { info.Position = pos_were; throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch); }
					left.Cast(DynamicInfo::ConstantType::Coordinate);
					top.Cast(DynamicInfo::ConstantType::Coordinate);
					right.Cast(DynamicInfo::ConstantType::Coordinate);
					bottom.Cast(DynamicInfo::ConstantType::Coordinate);
					return Rectangle(left.ToCoordinate(), top.ToCoordinate(), right.ToCoordinate(), bottom.ToCoordinate());
				} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
			}
			Template::Rectangle RECTANGLE_ARG(DynamicInfo & info)
			{
				if (info.GetToken().Class == TokenClass::Identifier && info.GetToken().Content == L"parent") {
					info.MoveNext();
					return Template::Rectangle(Coordinate(0), Coordinate(0), Coordinate::Right(), Coordinate::Bottom());
				} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"{") {
					info.MoveNext();
					auto left = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					auto top = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					auto right = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L',') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					auto bottom = EXPRESSION(info);
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					left.Cast(DynamicInfo::ConstantType::CoordinateTemplate);
					top.Cast(DynamicInfo::ConstantType::CoordinateTemplate);
					right.Cast(DynamicInfo::ConstantType::CoordinateTemplate);
					bottom.Cast(DynamicInfo::ConstantType::CoordinateTemplate);
					return Template::Rectangle(left.Value, top.Value, right.Value, bottom.Value);
				} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
			}
			Format::InterfaceColorPropertySetter COLOR(DynamicInfo & info) {
				if (info.GetToken().Class == TokenClass::Identifier) {
					int ID = info.GetColorID(info.GetToken().Content);
					if (ID == -1) throw SyntaxException(DED(info), ErrorClass::UndefinedObject);
					Format::InterfaceColorPropertySetter result;
					result.Class = 2;
					result.Value = ID;
					result.Name = L"";
					info.MoveNext();
					return result;
				} else {
					auto clr = COLOR_OBJECT(info);
					Format::InterfaceColorPropertySetter result;
					result.Class = clr.IsSystemColor ? 1 : 0;
					result.Value = clr.Value;
					result.Name = L"";
					return result;
				}
			}
			Format::InterfaceColorTemplate COLOR_ARG(DynamicInfo & info) {
				if (info.GetToken().Class == TokenClass::Identifier) {
					int ID = info.GetColorID(info.GetToken().Content);
					if (ID == -1) throw SyntaxException(DED(info), ErrorClass::UndefinedObject);
					Format::InterfaceColorTemplate result;
					result.Argument = L"";
					result.Class = 2;
					result.Value = ID;
					result.Name = L"";
					info.MoveNext();
					return result;
				} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"@") {
					info.MoveNext();
					if ((info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::String) && info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					Format::InterfaceColorTemplate result;
					result.Argument = info.GetToken().Content;
					result.Class = 0;
					result.Value = 0;
					result.Name = L"";
					info.MoveNext();
					return result;
				} else {
					auto clr = COLOR_OBJECT(info);
					Format::InterfaceColorTemplate result;
					result.Argument = L"";
					result.Class = clr.IsSystemColor ? 1 : 0;
					result.Value = clr.Value;
					result.Name = L"";
					return result;
				}
			}
			Format::InterfaceStringTemplate RESOURCE_ARG(DynamicInfo & info) {
				if (info.GetToken().Class == TokenClass::Identifier) {
					Format::InterfaceStringTemplate result;
					result.Argument = L"";
					result.Value = info.GetToken().Content;
					result.Name = L"";
					info.MoveNext();
					return result;
				} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"@") {
					info.MoveNext();
					if ((info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::String) && info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					Format::InterfaceStringTemplate result;
					result.Argument = info.GetToken().Content;
					result.Value = L"";
					result.Name = L"";
					info.MoveNext();
					return result;
				} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
			}
			void SHAPE_OBJECT(DynamicInfo & info, Format::InterfaceShape & shape)
			{
				if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				if (info.GetToken().Content == L"clone") {
					info.MoveNext();
					if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					auto & name = info.GetToken().Content;
					auto app = info.FindApplication(name);
					if (!app) throw SyntaxException(DED(info), ErrorClass::UndefinedObject);
					shape = app->Root;
					info.MoveNext();
					return;
				}
				if (info.GetToken().Content == L"frame") {
					shape.Class = L"Frame";
				} else if (info.GetToken().Content == L"bar") {
					shape.Class = L"Bar";
				} else if (info.GetToken().Content == L"texture") {
					shape.Class = L"Texture";
				} else if (info.GetToken().Content == L"text") {
					shape.Class = L"Text";
				} else if (info.GetToken().Content == L"blur") {
					shape.Class = L"Blur";
				} else if (info.GetToken().Content == L"inversion") {
					shape.Class = L"Inversion";
				} else throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
				info.MoveNext();
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				Format::InterfaceRectangleTemplate bar_gradient_rect;
				{
					bar_gradient_rect.Name = L"GradientPoints";
					for (int i = 0; i < 12; i++) bar_gradient_rect.Argument[i] = L"";
					for (int i = 0; i < 4; i++) bar_gradient_rect.Absolute[i] = 0;
					for (int i = 0; i < 7; i++) bar_gradient_rect.Scalable[i] = 0.0;
					bar_gradient_rect.Scalable[7] = 1.0;
				}
				while (info.GetToken().Class == TokenClass::Identifier) {
					auto & prop = info.GetToken().Content;
					if (prop == L"position") {
						info.MoveNext();
						auto rect = RECTANGLE_ARG(info);
						shape.RectangleValues.AppendNew();
						auto & val = shape.RectangleValues.InnerArray.LastElement();
						val.Name = L"Position";
						val.Absolute[0] = rect.Left.Absolute.IsDefined() ? rect.Left.Absolute.GetValue() : 0;
						val.Argument[0] = rect.Left.Absolute.GetArgument();
						val.Scalable[0] = rect.Left.Zoom.IsDefined() ? rect.Left.Zoom.GetValue() : 0.0;
						val.Argument[1] = rect.Left.Zoom.GetArgument();
						val.Scalable[1] = rect.Left.Anchor.IsDefined() ? rect.Left.Anchor.GetValue() : 0.0;
						val.Argument[2] = rect.Left.Anchor.GetArgument();
						val.Absolute[1] = rect.Top.Absolute.IsDefined() ? rect.Top.Absolute.GetValue() : 0;
						val.Argument[3] = rect.Top.Absolute.GetArgument();
						val.Scalable[2] = rect.Top.Zoom.IsDefined() ? rect.Top.Zoom.GetValue() : 0.0;
						val.Argument[4] = rect.Top.Zoom.GetArgument();
						val.Scalable[3] = rect.Top.Anchor.IsDefined() ? rect.Top.Anchor.GetValue() : 0.0;
						val.Argument[5] = rect.Top.Anchor.GetArgument();
						val.Absolute[2] = rect.Right.Absolute.IsDefined() ? rect.Right.Absolute.GetValue() : 0;
						val.Argument[6] = rect.Right.Absolute.GetArgument();
						val.Scalable[4] = rect.Right.Zoom.IsDefined() ? rect.Right.Zoom.GetValue() : 0.0;
						val.Argument[7] = rect.Right.Zoom.GetArgument();
						val.Scalable[5] = rect.Right.Anchor.IsDefined() ? rect.Right.Anchor.GetValue() : 0.0;
						val.Argument[8] = rect.Right.Anchor.GetArgument();
						val.Absolute[3] = rect.Bottom.Absolute.IsDefined() ? rect.Bottom.Absolute.GetValue() : 0;
						val.Argument[9] = rect.Bottom.Absolute.GetArgument();
						val.Scalable[6] = rect.Bottom.Zoom.IsDefined() ? rect.Bottom.Zoom.GetValue() : 0.0;
						val.Argument[10] = rect.Bottom.Zoom.GetArgument();
						val.Scalable[7] = rect.Bottom.Anchor.IsDefined() ? rect.Bottom.Anchor.GetValue() : 0.0;
						val.Argument[11] = rect.Bottom.Anchor.GetArgument();
					} else {
						if (shape.Class == L"Frame") {
							if (prop == L"clip") {
								info.MoveNext();
								shape.IntegerValues.AppendNew();
								shape.IntegerValues.InnerArray.LastElement().Name = L"RenderMode";
								shape.IntegerValues.InnerArray.LastElement().Argument = L"";
								shape.IntegerValues.InnerArray.LastElement().Value = 1;
							} else if (prop == L"layer") {
								info.MoveNext();
								shape.IntegerValues.AppendNew();
								shape.IntegerValues.InnerArray.LastElement().Name = L"RenderMode";
								shape.IntegerValues.InnerArray.LastElement().Argument = L"";
								shape.IntegerValues.InnerArray.LastElement().Value = 2;
							} else if (prop == L"opacity") {
								info.MoveNext();
								shape.DoubleValues.AppendNew();
								shape.DoubleValues.InnerArray.LastElement().Name = L"Opacity";
								auto pos_were = info.Position;
								auto val = EXPRESSION(info);
								if (!val.CanBeCasted(DynamicInfo::ConstantType::Double)) {
									if (val.Type == DynamicInfo::ConstantType::Argument) {
										shape.DoubleValues.InnerArray.LastElement().Argument = val.Name;
										shape.DoubleValues.InnerArray.LastElement().Value = 0.0;
									} else {
										info.Position = pos_were;
										throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
									}
								} else {
									val.Cast(DynamicInfo::ConstantType::Double);
									shape.DoubleValues.InnerArray.LastElement().Argument = L"";
									shape.DoubleValues.InnerArray.LastElement().Value = val.Value.Zoom.GetValue();
								}
							} else {
								shape.Children.AppendNew();
								SHAPE_OBJECT(info, shape.Children.InnerArray.LastElement());
							}
						} else {
							info.MoveNext();
							if (shape.Class == L"Bar") {
								if (prop == L"color") {
									auto clr = COLOR_ARG(info);
									clr.Name = L"Gradient";
									shape.ColorValues << clr;
								} else if (prop == L"gradient") {
									if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
									info.MoveNext();
									while (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') {
										if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
										info.MoveNext();
										auto clr = COLOR_ARG(info);
										clr.Name = L"Gradient";
										shape.ColorValues << clr;
										shape.DoubleValues.AppendNew();
										shape.DoubleValues.InnerArray.LastElement().Name = L"Gradient";
										auto pos_were = info.Position;
										auto val = EXPRESSION(info);
										if (!val.CanBeCasted(DynamicInfo::ConstantType::Double)) {
											if (val.Type == DynamicInfo::ConstantType::Argument) {
												shape.DoubleValues.InnerArray.LastElement().Argument = val.Name;
												shape.DoubleValues.InnerArray.LastElement().Value = 0.0;
											} else {
												info.Position = pos_were;
												throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
											}
										} else {
											val.Cast(DynamicInfo::ConstantType::Double);
											shape.DoubleValues.InnerArray.LastElement().Argument = L"";
											shape.DoubleValues.InnerArray.LastElement().Value = val.Value.Zoom.GetValue();
										}
										if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
										info.MoveNext();
									}
									info.MoveNext();
								} else if (prop == L"gradient_range") {
									auto rect = RECTANGLE_ARG(info);
									bar_gradient_rect.Absolute[0] = rect.Left.Absolute.IsDefined() ? rect.Left.Absolute.GetValue() : 0;
									bar_gradient_rect.Argument[0] = rect.Left.Absolute.GetArgument();
									bar_gradient_rect.Scalable[0] = rect.Left.Zoom.IsDefined() ? rect.Left.Zoom.GetValue() : 0.0;
									bar_gradient_rect.Argument[1] = rect.Left.Zoom.GetArgument();
									bar_gradient_rect.Scalable[1] = rect.Left.Anchor.IsDefined() ? rect.Left.Anchor.GetValue() : 0.0;
									bar_gradient_rect.Argument[2] = rect.Left.Anchor.GetArgument();
									bar_gradient_rect.Absolute[1] = rect.Top.Absolute.IsDefined() ? rect.Top.Absolute.GetValue() : 0;
									bar_gradient_rect.Argument[3] = rect.Top.Absolute.GetArgument();
									bar_gradient_rect.Scalable[2] = rect.Top.Zoom.IsDefined() ? rect.Top.Zoom.GetValue() : 0.0;
									bar_gradient_rect.Argument[4] = rect.Top.Zoom.GetArgument();
									bar_gradient_rect.Scalable[3] = rect.Top.Anchor.IsDefined() ? rect.Top.Anchor.GetValue() : 0.0;
									bar_gradient_rect.Argument[5] = rect.Top.Anchor.GetArgument();
									bar_gradient_rect.Absolute[2] = rect.Right.Absolute.IsDefined() ? rect.Right.Absolute.GetValue() : 0;
									bar_gradient_rect.Argument[6] = rect.Right.Absolute.GetArgument();
									bar_gradient_rect.Scalable[4] = rect.Right.Zoom.IsDefined() ? rect.Right.Zoom.GetValue() : 0.0;
									bar_gradient_rect.Argument[7] = rect.Right.Zoom.GetArgument();
									bar_gradient_rect.Scalable[5] = rect.Right.Anchor.IsDefined() ? rect.Right.Anchor.GetValue() : 0.0;
									bar_gradient_rect.Argument[8] = rect.Right.Anchor.GetArgument();
									bar_gradient_rect.Absolute[3] = rect.Bottom.Absolute.IsDefined() ? rect.Bottom.Absolute.GetValue() : 0;
									bar_gradient_rect.Argument[9] = rect.Bottom.Absolute.GetArgument();
									bar_gradient_rect.Scalable[6] = rect.Bottom.Zoom.IsDefined() ? rect.Bottom.Zoom.GetValue() : 0.0;
									bar_gradient_rect.Argument[10] = rect.Bottom.Zoom.GetArgument();
									bar_gradient_rect.Scalable[7] = rect.Bottom.Anchor.IsDefined() ? rect.Bottom.Anchor.GetValue() : 0.0;
									bar_gradient_rect.Argument[11] = rect.Bottom.Anchor.GetArgument();
								} else if (prop == L"horizontal") {
									for (int i = 0; i < 12; i++) bar_gradient_rect.Argument[i] = L"";
									for (int i = 0; i < 4; i++) bar_gradient_rect.Absolute[i] = 0;
									for (int i = 0; i < 8; i++) bar_gradient_rect.Scalable[i] = 0.0;
									bar_gradient_rect.Scalable[5] = 1.0;
								} else {
									info.Position--;
									throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
								}
							} else if (shape.Class == L"Texture") {
								if (prop == L"from") {
									auto rect = RECTANGLE_ARG(info);
									shape.RectangleValues.AppendNew();
									auto & val = shape.RectangleValues.InnerArray.LastElement();
									val.Name = L"Source";
									val.Absolute[0] = rect.Left.Absolute.IsDefined() ? rect.Left.Absolute.GetValue() : 0;
									val.Argument[0] = rect.Left.Absolute.GetArgument();
									val.Scalable[0] = rect.Left.Zoom.IsDefined() ? rect.Left.Zoom.GetValue() : 0.0;
									val.Argument[1] = rect.Left.Zoom.GetArgument();
									val.Scalable[1] = rect.Left.Anchor.IsDefined() ? rect.Left.Anchor.GetValue() : 0.0;
									val.Argument[2] = rect.Left.Anchor.GetArgument();
									val.Absolute[1] = rect.Top.Absolute.IsDefined() ? rect.Top.Absolute.GetValue() : 0;
									val.Argument[3] = rect.Top.Absolute.GetArgument();
									val.Scalable[2] = rect.Top.Zoom.IsDefined() ? rect.Top.Zoom.GetValue() : 0.0;
									val.Argument[4] = rect.Top.Zoom.GetArgument();
									val.Scalable[3] = rect.Top.Anchor.IsDefined() ? rect.Top.Anchor.GetValue() : 0.0;
									val.Argument[5] = rect.Top.Anchor.GetArgument();
									val.Absolute[2] = rect.Right.Absolute.IsDefined() ? rect.Right.Absolute.GetValue() : 0;
									val.Argument[6] = rect.Right.Absolute.GetArgument();
									val.Scalable[4] = rect.Right.Zoom.IsDefined() ? rect.Right.Zoom.GetValue() : 0.0;
									val.Argument[7] = rect.Right.Zoom.GetArgument();
									val.Scalable[5] = rect.Right.Anchor.IsDefined() ? rect.Right.Anchor.GetValue() : 0.0;
									val.Argument[8] = rect.Right.Anchor.GetArgument();
									val.Absolute[3] = rect.Bottom.Absolute.IsDefined() ? rect.Bottom.Absolute.GetValue() : 0;
									val.Argument[9] = rect.Bottom.Absolute.GetArgument();
									val.Scalable[6] = rect.Bottom.Zoom.IsDefined() ? rect.Bottom.Zoom.GetValue() : 0.0;
									val.Argument[10] = rect.Bottom.Zoom.GetArgument();
									val.Scalable[7] = rect.Bottom.Anchor.IsDefined() ? rect.Bottom.Anchor.GetValue() : 0.0;
									val.Argument[11] = rect.Bottom.Anchor.GetArgument();
								} else if (prop == L"texture") {
									auto val = RESOURCE_ARG(info);
									val.Name = L"Texture";
									shape.StringValues << val;
								} else if (prop == L"keep_aspect_ratio") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"RenderMode";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 1;
								} else if (prop == L"fill") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"RenderMode";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 2;
								} else if (prop == L"no_stretch") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"RenderMode";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 3;
								} else {
									info.Position--;
									throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
								}
							} else if (shape.Class == L"Text") {
								if (prop == L"font") {
									auto val = RESOURCE_ARG(info);
									val.Name = L"Font";
									shape.StringValues << val;
								} else if (prop == L"text") {
									if (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::String) {
										Format::InterfaceStringTemplate val;
										val.Name = L"Text";
										val.Argument = L"";
										val.Value = info.GetToken().Content;
										shape.StringValues << val;
										info.MoveNext();
									} else {
										auto val = RESOURCE_ARG(info);
										if (val.Argument.Length()) {
											val.Name = L"Text";
											shape.StringValues << val;
										} else {
											Format::InterfaceIntegerTemplate ref;
											ref.Name = L"Text";
											ref.Argument = L"";
											ref.Value = info.GetStringID(val.Value);
											shape.IntegerValues << ref;
											if (ref.Value == -1) {
												info.Position--;
												throw SyntaxException(DED(info), ErrorClass::UndefinedObject);
											}
										}
									}
								} else if (prop == L"color") {
									auto clr = COLOR_ARG(info);
									clr.Name = L"Color";
									shape.ColorValues << clr;
								} else if (prop == L"left") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"HorizontalAlign";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 0;
								} else if (prop == L"right") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"HorizontalAlign";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 2;
								} else if (prop == L"center") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"HorizontalAlign";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 1;
								} else if (prop == L"top") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"VerticalAlign";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 0;
								} else if (prop == L"bottom") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"VerticalAlign";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 2;
								} else if (prop == L"vcenter") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"VerticalAlign";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 1;
								} else if (prop == L"multiline") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"Multiline";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 1;
								} else if (prop == L"word_wrap") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"WordWrap";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 1;
								} else if (prop == L"ellipsis") {
									shape.IntegerValues.AppendNew();
									shape.IntegerValues.InnerArray.LastElement().Name = L"Ellipsis";
									shape.IntegerValues.InnerArray.LastElement().Argument = L"";
									shape.IntegerValues.InnerArray.LastElement().Value = 1;
								} else {
									info.Position--;
									throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
								}
							} else if (shape.Class == L"Blur") {
								if (prop == L"power") {
									shape.DoubleValues.AppendNew();
									shape.DoubleValues.InnerArray.LastElement().Name = L"Power";
									auto pos_were = info.Position;
									auto val = EXPRESSION(info);
									if (!val.CanBeCasted(DynamicInfo::ConstantType::Double)) {
										if (val.Type == DynamicInfo::ConstantType::Argument) {
											shape.DoubleValues.InnerArray.LastElement().Argument = val.Name;
											shape.DoubleValues.InnerArray.LastElement().Value = 0.0;
										} else {
											info.Position = pos_were;
											throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
										}
									} else {
										val.Cast(DynamicInfo::ConstantType::Double);
										shape.DoubleValues.InnerArray.LastElement().Argument = L"";
										shape.DoubleValues.InnerArray.LastElement().Value = val.Value.Zoom.GetValue();
									}
								} else {
									info.Position--;
									throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
								}
							} else if (shape.Class == L"Inversion") {
								info.Position--;
								throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
							}
						}
					}
				}
				if (shape.Class == L"Bar") shape.RectangleValues << bar_gradient_rect;
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
			}
			void CONTROL_OBJECT(DynamicInfo & info, DynamicInfo::PlatformContents & ns, Format::InterfaceControl & control, const string & parent_class);
			void CONTROL_PROPERTY(DynamicInfo & info, DynamicInfo::PlatformContents & ns, Format::InterfaceControl & control, Template::ControlReflectedBase * base)
			{
				auto & prop_name = info.GetToken().Content;
				auto prop = base->GetProperty(prop_name);
				if (!prop.Address) throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
				info.MoveNext();
				if (prop.Type == Reflection::PropertyType::Integer) {
					auto pos_were = info.Position;
					auto val = EXPRESSION(info);
					if (!val.CanBeCasted(DynamicInfo::ConstantType::Coordinate)) {
						info.Position = pos_were;
						throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
					}
					val.Cast(DynamicInfo::ConstantType::Coordinate);
					Format::InterfaceCoordinatePropertySetter setter;
					setter.Name = prop_name;
					setter.Absolute = val.Value.Absolute.GetValue();
					setter.Scalable = val.Value.Zoom.GetValue();
					control.CoordinateSetters << setter;
				} else if (prop.Type == Reflection::PropertyType::Double) {
					auto pos_were = info.Position;
					auto val = EXPRESSION(info);
					if (!val.CanBeCasted(DynamicInfo::ConstantType::Double)) {
						info.Position = pos_were;
						throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
					}
					val.Cast(DynamicInfo::ConstantType::Double);
					Format::InterfaceCoordinatePropertySetter setter;
					setter.Name = prop_name;
					setter.Absolute = 0;
					setter.Scalable = val.Value.Zoom.GetValue();
					control.CoordinateSetters << setter;
				} else if (prop.Type == Reflection::PropertyType::Boolean) {
					Format::InterfaceCoordinatePropertySetter setter;
					setter.Name = prop_name;
					if (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::Boolean) {
						setter.Absolute = (info.GetToken().AsBoolean() ? 1 : 0);
						info.MoveNext();
					} else setter.Absolute = 1;
					setter.Scalable = 0.0;
					control.CoordinateSetters << setter;
				} else if (prop.Type == Reflection::PropertyType::String) {
					if (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::String) {
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = info.GetToken().Content;
						control.StringsSetters << setter;
						info.MoveNext();
					} else if (info.GetToken().Class == TokenClass::Identifier) {
						int ID = info.GetStringID(info.GetToken().Content);
						if (ID == -1) throw SyntaxException(DED(info), ErrorClass::UndefinedObject);
						info.MoveNext();
						Format::InterfaceCoordinatePropertySetter setter;
						setter.Name = prop_name;
						setter.Absolute = ID;
						setter.Scalable = 0.0;
						control.CoordinateSetters << setter;
					} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				} else if (prop.Type == Reflection::PropertyType::Color) {
					Format::InterfaceColorPropertySetter setter = COLOR(info);
					setter.Name = prop_name;
					control.ColorSetters << setter;
				} else if (prop.Type == Reflection::PropertyType::Texture || prop.Type == Reflection::PropertyType::Font) {
					if ((info.GetToken().Class == TokenClass::Identifier) || (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::String)) {
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = info.GetToken().Content;
						control.StringsSetters << setter;
						info.MoveNext();
					} else if (info.GetToken().Class == TokenClass::Keyword && info.GetToken().Content == L"null") {
						info.MoveNext();
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = L"";
						control.StringsSetters << setter;
					} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				} else if (prop.Type == Reflection::PropertyType::Application) {
					if ((info.GetToken().Class == TokenClass::Identifier) || (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::String)) {
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = info.GetToken().Content;
						control.StringsSetters << setter;
						info.MoveNext();
					} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"{") {
						info.MoveNext();
						ns.Applications.Append(Format::InterfaceApplication());
						auto & app = ns.Applications.LastElement();
						app.Name = L"@UNNAMED" + string(uint32(info.GetUnnamed()), HexadecimalBase, 8);
						SHAPE_OBJECT(info, app.Root);
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = app.Name;
						control.StringsSetters << setter;
					} else if (info.GetToken().Class == TokenClass::Keyword && info.GetToken().Content == L"null") {
						info.MoveNext();
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = L"";
						control.StringsSetters << setter;
					} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				} else if (prop.Type == Reflection::PropertyType::Dialog) {
					if ((info.GetToken().Class == TokenClass::Identifier) || (info.GetToken().Class == TokenClass::Constant && info.GetToken().ValueClass == TokenConstantClass::String)) {
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = info.GetToken().Content;
						control.StringsSetters << setter;
						info.MoveNext();
					} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"{") {
						info.MoveNext();
						ns.Dialogs.Append(Format::InterfaceDialog());
						auto & dlg = ns.Dialogs.LastElement();
						dlg.Name = L"@UNNAMED" + string(uint32(info.GetUnnamed()), HexadecimalBase, 8);
						CONTROL_OBJECT(info, ns, dlg.Root, L"");
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = dlg.Name;
						control.StringsSetters << setter;
					} else if (info.GetToken().Class == TokenClass::Keyword && info.GetToken().Content == L"null") {
						info.MoveNext();
						Format::InterfaceStringPropertySetter setter;
						setter.Name = prop_name;
						setter.Value = L"";
						control.StringsSetters << setter;
					} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				} else if (prop.Type == Reflection::PropertyType::Rectangle) {
					Format::InterfaceRectanglePropertySetter setter;
					setter.Name = prop_name;
					setter.Value = RECTANGLE(info);
					control.RectangleSetters << setter;
				}
			}
			void STYLE_OBJECT(DynamicInfo & info, DynamicInfo::PlatformContents & ns, Format::InterfaceDialog & style)
			{
				bool release = true;
				auto base = Template::Controls::CreateControlByClass(style.Root.Class);
				if (!base) {
					release = false;
					for (auto & d : info.ClassData) if (d.Name == style.Root.Class && d.Base.Inner()) { base = d.Base.Inner(); break; }
				}
				if (!base) throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
				try {
					bool set_default = false;
					while (info.GetToken().Class == TokenClass::Identifier) {
						if (info.GetToken().Content == L"default") {
							info.MoveNext();
							set_default = true;
						} else if (info.GetToken().Content == L"inherit") {
							info.MoveNext();
							if (info.GetToken().Class == TokenClass::Identifier) {
								style.Root.Styles << info.GetToken().Content;
								info.MoveNext();
							} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"@" && info.GetToken(1).Class == TokenClass::Identifier && info.GetToken(1).Content == L"default") {
								string def = info.GetDefaultStyle(style.Root.Class);
								if (def.Length()) style.Root.Styles << def;
								info.MoveNext(2);
							} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						} else {
							CONTROL_PROPERTY(info, ns, style.Root, base);
						}
					}
					if (set_default) info.SetDefaultStyle(style.Root.Class, style.Name);
				}
				catch (...) { if (release) delete base; throw; }
				if (release) delete base;
			}
			void CONTROL_OBJECT(DynamicInfo & info, DynamicInfo::PlatformContents & ns, Format::InterfaceControl & control, const string & parent_class)
			{
				if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				if (parent_class.Length() && warning_reporter && subordering) {
					bool subord_failed = true;
					for (auto & c : info.ClassData) if (c.Name == parent_class) {
						if (c.AllowedChildren[info.GetToken().Content]) {
							subord_failed = false;
						} else if (c.AllowedChildren[L"*"] && info.CommonClasses[info.GetToken().Content]) {
							subord_failed = false;
						}
						break;
					}
					if (subord_failed) {
						WarningDesc wdesc;
						wdesc.status = WarningClass::InvalidControlParent;
						wdesc.position = info.GetToken().SourcePosition;
						wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
						wdesc.desc = info.GetToken().Content + L"," + parent_class;
						warning_reporter->ReportWarning(wdesc);
					}
				}
				bool release = true;
				auto base = Template::Controls::CreateControlByClass(info.GetToken().Content);
				if (!base) {
					release = false;
					for (auto & d : info.ClassData) if (d.Name == info.GetToken().Content && d.Base.Inner()) { base = d.Base.Inner(); break; }
				}
				if (!base) throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
				control.Class = base->GetTemplateClass();
				info.MoveNext();
				try {
					string def = info.GetDefaultStyle(control.Class);
					if (def.Length()) control.Styles << def;
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
					while (info.GetToken().Class == TokenClass::Identifier) {
						if (info.GetToken().Content == L"position") {
							info.MoveNext();
							control.Position = RECTANGLE(info);
						} else if (info.GetToken().Content == L"style") {
							info.MoveNext();
							if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							control.Styles << info.GetToken().Content;
							info.MoveNext();
						} else if (info.GetToken().Content == L"controls") {
							info.MoveNext();
							if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							info.MoveNext();
							while (info.GetToken().Class == TokenClass::Identifier) {
								control.Children.AppendNew();
								CONTROL_OBJECT(info, ns, control.Children.InnerArray.LastElement(), control.Class);
							}
							if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							info.MoveNext();
						} else if (base->GetProperty(info.GetToken().Content).Address) {
							CONTROL_PROPERTY(info, ns, control, base);
						} else {
							control.Children.AppendNew();
							CONTROL_OBJECT(info, ns, control.Children.InnerArray.LastElement(), control.Class);
						}
					}
					if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					info.MoveNext();
				} catch (...) { if (release) delete base; throw; }
				if (release) delete base;
			}
			void CLASS_OBJECT(DynamicInfo & info)
			{
				if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				auto class_name = info.GetToken().Content;
				auto built_in = Template::Controls::CreateControlByClass(class_name);
				if (built_in) {
					delete built_in;
					throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
				}
				for (auto & d : info.ClassData) if (d.Name == class_name) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
				bool set_as_common = false;
				DynamicInfo::ControlClassInfo ctl_class;
				ctl_class.Name = class_name;
				ctl_class.Base = new CustomControlClass(class_name);
				info.MoveNext();
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				while (info.GetToken().Class == TokenClass::Identifier) {
					if (info.GetToken().Content == L"property") {
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						auto prop_name = info.GetToken().Content;
						auto prop_type = Reflection::PropertyType::Unknown;
						if (ctl_class.Base->HasProperty(prop_name)) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L":") throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						if (info.GetToken().Content == L"float") prop_type = Reflection::PropertyType::Double;
						else if (info.GetToken().Content == L"integer") prop_type = Reflection::PropertyType::Integer;
						else if (info.GetToken().Content == L"boolean") prop_type = Reflection::PropertyType::Boolean;
						else if (info.GetToken().Content == L"color") prop_type = Reflection::PropertyType::Color;
						else if (info.GetToken().Content == L"string") prop_type = Reflection::PropertyType::String;
						else if (info.GetToken().Content == L"rectangle") prop_type = Reflection::PropertyType::Rectangle;
						else if (info.GetToken().Content == L"font") prop_type = Reflection::PropertyType::Font;
						else if (info.GetToken().Content == L"texture") prop_type = Reflection::PropertyType::Texture;
						else if (info.GetToken().Content == L"application") prop_type = Reflection::PropertyType::Application;
						else if (info.GetToken().Content == L"dialog") prop_type = Reflection::PropertyType::Dialog;
						else throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
						info.MoveNext();
						ctl_class.Base->AddProperty(prop_name, prop_type);
					} else if (info.GetToken().Content == L"child") {
						info.MoveNext();
						if (info.GetToken().Class == TokenClass::Identifier) {
							if (ctl_class.AllowedChildren[info.GetToken().Content]) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
							ctl_class.AllowedChildren.AddElement(info.GetToken().Content);
							info.MoveNext();
						} else if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L"*") {
							if (ctl_class.AllowedChildren[info.GetToken().Content]) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
							ctl_class.AllowedChildren.AddElement(info.GetToken().Content);
							info.MoveNext();
						} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
					} else if (info.GetToken().Content == L"common_parent") {
						if (set_as_common) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
						info.MoveNext();
						set_as_common = true;
					} else throw SyntaxException(DED(info), ErrorClass::InvalidProperty);
				}
				if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				info.MoveNext();
				info.ClassData << ctl_class;
				if (set_as_common) info.CommonClasses.AddElement(class_name);
			}
			void DOCUMENT(DynamicInfo & info)
			{
				while (info.GetToken().Class == TokenClass::Identifier) {
					if (info.GetToken().Content == L"constant") {
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						auto & type = info.GetToken().Content;
						if (type != L"integer" && type != L"double" && type != L"coordinate") throw SyntaxException(DED(info), ErrorClass::InvalidConstantType);
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						auto & name = info.GetToken().Content;
						if (info.GetConstantIndex(name) != -1) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'=') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						int pos_were = info.Position;
						DynamicInfo::ConstantType type_val;
						if (type == L"integer") type_val = DynamicInfo::ConstantType::Integer;
						else if (type == L"double") type_val = DynamicInfo::ConstantType::Double;
						else if (type == L"coordinate") type_val = DynamicInfo::ConstantType::Coordinate;
						auto value = EXPRESSION(info);
						if (!value.CanBeCasted(type_val)) {
							info.Position = pos_were;
							throw SyntaxException(DED(info), ErrorClass::NumericConstantTypeMismatch);
						}
						value.Cast(type_val);
						value.Name = name;
						info.Constants << value;
					} else if (info.GetToken().Content == L"resource") {
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						if (info.GetToken().Content == L"color") {
							info.MoveNext();
							if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							string name = info.GetToken().Content;
							string platform = L"";
							int ID = info.GetColorID(name);
							if (ID == -1) {
								ID = info.Colors.Length() ? (info.Colors.LastElement().MachineIdentifier + 1) : 1;
								info.Colors << DynamicInfo::ColorAlias{ name, ID };
							}
							info.MoveNext();
							if (info.GetToken().Class == TokenClass::Identifier) {
								platform = info.GetToken().Content;
								if (!CheckPlatformName(platform) && warning_reporter) {
									WarningDesc wdesc;
									wdesc.status = WarningClass::UnknownPlatformName;
									wdesc.position = info.GetToken().SourcePosition;
									wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
									wdesc.desc = platform;
									warning_reporter->ReportWarning(wdesc);
								}
								info.MoveNext();
							}
							auto & cont = info.GetContentsFor(platform);
							int index = cont.GetColorIndex(ID);
							if (index != -1) {
								info.Position--;
								throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
							}
							Format::InterfaceColor obj = COLOR_OBJECT(info);
							obj.ID = ID;
							obj.Name = name;
							cont.Colors << obj;
						} else if (info.GetToken().Content == L"string") {
							info.MoveNext();
							if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							string name = info.GetToken().Content;
							if (info.GetStringID(name) != -1) throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
							info.MoveNext();
							if (info.GetToken().Class == TokenClass::CharCombo && info.GetToken().Content == L'[') info.LastSet = LOCALE_SET(info);
							if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							info.MoveNext();
							auto ValueSet = info.LastSet;
							for (int i = 0; i < ValueSet.Length(); i++) {
								if (info.GetToken().Class != TokenClass::Constant || info.GetToken().ValueClass != TokenConstantClass::String) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
								ValueSet[i] = info.GetToken().Content;
								info.MoveNext();
							}
							if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							info.MoveNext();
							int ID = info.Strings.Length() ? (info.Strings.LastElement().MachineIdentifier + 1) : 1;
							info.Strings << DynamicInfo::LocaleString{ name, ID, info.LastSet, ValueSet };
						} else if (info.GetToken().Content == L"texture") {
							info.MoveNext();
							if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							string name = info.GetToken().Content;
							string platform = L"";
							info.MoveNext();
							if (info.GetToken().Class == TokenClass::Identifier) {
								platform = info.GetToken().Content;
								if (!CheckPlatformName(platform) && warning_reporter) {
									WarningDesc wdesc;
									wdesc.status = WarningClass::UnknownPlatformName;
									wdesc.position = info.GetToken().SourcePosition;
									wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
									wdesc.desc = platform;
									warning_reporter->ReportWarning(wdesc);
								}
								info.MoveNext();
							}
							auto & cont = info.GetContentsFor(platform);
							int index = cont.GetTextureIndex(name);
							if (index != -1) {
								info.Position--;
								throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
							}
							SafePointer<Codec::Image> Texture = TEXTURE_OBJECT(info);
							if (Texture) {
								Format::InterfaceTexture obj;
								obj.ImageID = info.TextureID;
								obj.Name = name;
								info.Output.Textures.Append(info.TextureID, Texture);
								info.TextureID++;
								cont.Textures << obj;
							}
						} else if (info.GetToken().Content == L"font") {
							info.MoveNext();
							if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							string name = info.GetToken().Content;
							string platform = L"";
							info.MoveNext();
							if (info.GetToken().Class == TokenClass::Identifier) {
								platform = info.GetToken().Content;
								if (!CheckPlatformName(platform) && warning_reporter) {
									WarningDesc wdesc;
									wdesc.status = WarningClass::UnknownPlatformName;
									wdesc.position = info.GetToken().SourcePosition;
									wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
									wdesc.desc = platform;
									warning_reporter->ReportWarning(wdesc);
								}
								info.MoveNext();
							}
							auto & cont = info.GetContentsFor(platform);
							int index = cont.GetFontIndex(name);
							if (index != -1) {
								info.Position--;
								throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
							}
							cont.Fonts.Append(Format::InterfaceFont());
							auto & font = cont.Fonts.LastElement();
							font.Name = name;
							font.FontFace = Graphics::SystemSerifFont;
							font.Height.Absolute = 0;
							font.Height.Anchor = 0.0;
							font.Height.Scalable = 20.0;
							font.IsItalic = false;
							font.IsStrikeout = false;
							font.IsUnderline = false;
							font.Weight = 400;
							if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							info.MoveNext();
							while (info.GetToken().Class == TokenClass::Identifier) FONT_PROP(info, font);
							if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
							info.MoveNext();
						}
					} else if (info.GetToken().Content == L"application") {
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						string name = info.GetToken().Content;
						string platform = L"";
						info.MoveNext();
						if (info.GetToken().Class == TokenClass::Identifier) {
							platform = info.GetToken().Content;
							if (!CheckPlatformName(platform) && warning_reporter) {
								WarningDesc wdesc;
								wdesc.status = WarningClass::UnknownPlatformName;
								wdesc.position = info.GetToken().SourcePosition;
								wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
								wdesc.desc = platform;
								warning_reporter->ReportWarning(wdesc);
							}
							info.MoveNext();
						}
						auto & cont = info.GetContentsFor(platform);
						int index = cont.GetApplicationIndex(name);
						if (index != -1) {
							info.Position--;
							throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
						}
						cont.Applications.Append(Format::InterfaceApplication());
						auto & app = cont.Applications.LastElement();
						app.Name = name;
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						SHAPE_OBJECT(info, app.Root);
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
					} else if (info.GetToken().Content == L"class") {
						info.MoveNext();
						CLASS_OBJECT(info);
					} else if (info.GetToken().Content == L"style") {
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						string name = info.GetToken().Content;
						string platform = L"";
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						string cls = info.GetToken().Content;
						info.MoveNext();
						if (info.GetToken().Class == TokenClass::Identifier) {
							platform = info.GetToken().Content;
							if (!CheckPlatformName(platform) && warning_reporter) {
								WarningDesc wdesc;
								wdesc.status = WarningClass::UnknownPlatformName;
								wdesc.position = info.GetToken().SourcePosition;
								wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
								wdesc.desc = platform;
								warning_reporter->ReportWarning(wdesc);
							}
							info.MoveNext();
						}
						auto & cont = info.GetContentsFor(platform);
						int index = cont.GetStyleIndex(name, cls);
						if (index != -1) {
							info.Position--;
							throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
						}
						cont.Styles.Append(Format::InterfaceDialog());
						auto & stl = cont.Styles.LastElement();
						stl.Name = name;
						stl.Root.Class = cls;
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						STYLE_OBJECT(info, cont, stl);
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
					} else if (info.GetToken().Content == L"dialog") {
						info.MoveNext();
						if (info.GetToken().Class != TokenClass::Identifier) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						string name = info.GetToken().Content;
						string platform = L"";
						info.MoveNext();
						if (info.GetToken().Class == TokenClass::Identifier) {
							platform = info.GetToken().Content;
							if (!CheckPlatformName(platform) && warning_reporter) {
								WarningDesc wdesc;
								wdesc.status = WarningClass::UnknownPlatformName;
								wdesc.position = info.GetToken().SourcePosition;
								wdesc.length = info.GetToken(1).SourcePosition - info.GetToken(0).SourcePosition;
								wdesc.desc = platform;
								warning_reporter->ReportWarning(wdesc);
							}
							info.MoveNext();
						}
						auto & cont = info.GetContentsFor(platform);
						int index = cont.GetDialogIndex(name);
						if (index != -1) {
							info.Position--;
							throw SyntaxException(DED(info), ErrorClass::ObjectRedifinition);
						}
						cont.Dialogs.Append(Format::InterfaceDialog());
						auto & dlg = cont.Dialogs.LastElement();
						dlg.Name = name;
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'{') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
						CONTROL_OBJECT(info, cont, dlg.Root, L"");
						if (info.GetToken().Class != TokenClass::CharCombo || info.GetToken().Content != L'}') throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
						info.MoveNext();
					} else throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
				}
				if (info.GetToken().Class != TokenClass::EndOfStream) throw SyntaxException(DED(info), ErrorClass::UnexpectedLexem);
			}
			Format::InterfaceTemplateImage * CompileInterface(const InputDesc & input, ErrorDesc & error)
			{
				try {
					// STEP ONE. Loading and lexical analysis.
					string master_source;
					SafePointer< Array<Token> > source;
					Spelling uiml_spelling;
					uiml_spelling.BooleanFalseLiteral = L"false";
					uiml_spelling.BooleanTrueLiteral = L"true";
					uiml_spelling.CommentBlockClosingWord = L"*/";
					uiml_spelling.CommentBlockOpeningWord = L"/*";
					uiml_spelling.CommentEndOfLineWord = L"//";
					uiml_spelling.InfinityLiteral = L"float_infinity";
					uiml_spelling.NonNumberLiteral = L"float_nan";
					uiml_spelling.Keywords << L"w";
					uiml_spelling.Keywords << L"z";
					uiml_spelling.Keywords << L"null";
					uiml_spelling.IsolatedChars << L'{';
					uiml_spelling.IsolatedChars << L'}';
					uiml_spelling.IsolatedChars << L',';
					uiml_spelling.IsolatedChars << L':';
					uiml_spelling.IsolatedChars << L'@';
					uiml_spelling.IsolatedChars << L'#';
					uiml_spelling.IsolatedChars << L'&';
					uiml_spelling.IsolatedChars << L'$';
					uiml_spelling.IsolatedChars << L'[';
					uiml_spelling.IsolatedChars << L']';
					uiml_spelling.IsolatedChars << L'+';
					uiml_spelling.IsolatedChars << L'-';
					uiml_spelling.IsolatedChars << L'*';
					uiml_spelling.IsolatedChars << L'/';
					uiml_spelling.IsolatedChars << L'(';
					uiml_spelling.IsolatedChars << L')';
					uiml_spelling.IsolatedChars << L'=';
					{
						MacroInfo macro;
						source = ParseSource(IO::ExpandPath(input.main_uiml), macro, input.include, uiml_spelling, &master_source);
					}
					// STEP TWO. Syntax analysis and execution.
					SafePointer<Format::InterfaceTemplateImage> image = new Format::InterfaceTemplateImage;
					DynamicInfo dynamic(master_source, *source, *image);
					DOCUMENT(dynamic);
					// STEP THREE. Post-production
					for (int i = 0; i < dynamic.GlobalLocaleSet.Length(); i++) {
						SafePointer<Storage::StringTable> locale = new Storage::StringTable;
						image->Locales.Append(dynamic.GlobalLocaleSet[i], locale);
					}
					for (int i = 0; i < dynamic.Strings.Length(); i++) {
						auto & str = dynamic.Strings[i];
						int ID = str.MachineIdentifier;
						image->StringIDs.Append(str.HumanIdentifier, str.MachineIdentifier);
						for (int j = 0; j < str.Locales.Length(); j++) {
							auto locale = image->Locales[str.Locales[j]];
							locale->AddString(str.Variants[j], ID);
						}
					}
					for (int i = 0; i < dynamic.Contents.Length(); i++) {
						image->Assets << Format::InterfaceAsset();
						auto & asset = image->Assets.LastElement();
						auto & cont = dynamic.Contents[i];
						asset.SystemFilter = cont.PlatformName;
						asset.Colors.InnerArray << cont.Colors;
						asset.Textures.InnerArray << cont.Textures;
						asset.Fonts.InnerArray << cont.Fonts;
						asset.Applications.InnerArray << cont.Applications;
						asset.Styles.InnerArray << cont.Styles;
						for (auto & d : cont.Dialogs) asset.Dialogs.InnerArray.Append(d);
						if (input.build_as_style) {
							for (int j = 0; j < asset.Styles.Length(); j++) {
								auto & stl = asset.Styles[j];
								int def_index = -1;
								for (int k = 0; k < dynamic.DefaultStyles.Length(); k++) {
									if (dynamic.DefaultStyles[k].Style == stl.Name && dynamic.DefaultStyles[k].Class == stl.Root.Class) {
										def_index = k;
										break;
									}
								}
								if (def_index >= 0) {
									asset.Dialogs << stl;
									asset.Dialogs.InnerArray.LastElement().Name = L"@default:" + stl.Root.Class;
								}
							}
						}
					}
					image->Retain();
					error.status = ErrorClass::OK;
					error.position = -1;
					error.length = -1;
					error.desc = L"";
					return image;
				} catch (SyntaxException & e) {
					error.status = e.LocalError;
					error.position = e.StartPosition;
					error.length = e.EndPosition - e.StartPosition;
					error.desc = L"";
					return 0;
				} catch (SourceLoadingException & e) {
					error.status = ErrorClass::SourceAccess;
					error.position = -1;
					error.length = -1;
					error.desc = e.FileName;
					return 0;
				} catch (ParserSpellingException & e) {
					error.status = ErrorClass::MainInvalidToken;
					error.position = e.Position;
					error.length = 0;
					error.desc = L"";
					return 0;
				} catch (InnerSourceErrorException & e) {
					error.status = ErrorClass::IncludedInvalidToken;
					error.position = e.OuterPosition;
					error.length = 0;
					error.desc = e.FileName;
					return 0;
				} catch (...) {
					error.status = ErrorClass::UnknownError;
					error.position = -1;
					error.length = -1;
					error.desc = L"";
					return 0;
				}
			}
		}
	}
}
