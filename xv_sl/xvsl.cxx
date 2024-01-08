#include <ClusterClient.h>
#include "../xexec/xx_com.h"
#include "../xv/xv_compiler.h"
#include "../ximg/xi_module.h"
#include "xvsl_struct.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::Storage;

SafePointer<StringTable> localization;
SafePointer<Cluster::Client> debug_client;
SafePointer<Streaming::ITextWriter> debug_console;
SafePointer<ThreadPool> pool;
SafePointer<Semaphore> access_sync;
SafePointer<XV::ManualVolume> manual;
Array<string> module_search_paths;

string Localized(int id)
{
	try {
		if (!localization) throw InvalidStateException();
		return localization->GetString(id);
	} catch (...) { return FormatString(L"LOC(%0)", id); }
}
string ErrorDescription(XV::CompilerStatus status)
{
	try {
		if (!localization) throw InvalidStateException();
		return localization->GetString(400 + int(status));
	} catch (...) { return Localized(303); }
}
string TagDescription(XV::CodeRangeTag tag)
{
	if (tag == XV::CodeRangeTag::Comment) return Localized(161);
	else if (tag == XV::CodeRangeTag::Keyword) return Localized(162);
	else if (tag == XV::CodeRangeTag::Punctuation) return Localized(163);
	else if (tag == XV::CodeRangeTag::Prototype) return Localized(164);
	else if (tag == XV::CodeRangeTag::LiteralBoolean) return Localized(165);
	else if (tag == XV::CodeRangeTag::LiteralNumeric) return Localized(166);
	else if (tag == XV::CodeRangeTag::LiteralString) return Localized(167);
	else if (tag == XV::CodeRangeTag::LiteralNull) return Localized(168);
	else if (tag == XV::CodeRangeTag::IdentifierUnknown) return Localized(169);
	else if (tag == XV::CodeRangeTag::IdentifierNamespace) return Localized(170);
	else if (tag == XV::CodeRangeTag::IdentifierType) return Localized(171);
	else if (tag == XV::CodeRangeTag::IdentifierPrototype) return Localized(172);
	else if (tag == XV::CodeRangeTag::IdentifierFunction) return Localized(173);
	else if (tag == XV::CodeRangeTag::IdentifierConstant) return Localized(174);
	else if (tag == XV::CodeRangeTag::IdentifierVariable) return Localized(175);
	else if (tag == XV::CodeRangeTag::IdentifierProperty) return Localized(176);
	else if (tag == XV::CodeRangeTag::IdentifierField) return Localized(177);
	else return L"";
}
string ArgumentInfoToString(const XV::ArgumentInfo & inf)
{
	if (inf.type.Length()) return inf.type;
	return TagDescription(inf.tag);
}

typedef Array<uint32> Code;
void AbsoluteToLineColumn(Code & code, uint abs, uint & line, uint & chr);
void LineColumnToAbsolute(Code & code, uint line, uint chr, uint & abs);
string MarkdownEscape(const string & in);
string UncoverURI(const string & uri)
{
	#ifdef ENGINE_WINDOWS
	auto path = uri.Fragment(8, -1);
	#else
	auto path = uri.Fragment(7, -1);
	#endif
	Array<char> result(0x100);
	for (int i = 0; i < path.Length(); i++) {
		if (path[i] == L'%') {
			auto digits = path.Fragment(i + 1, 2);
			i += 2;
			result << digits.ToUInt32(HexadecimalBase);
		} else result << path[i];
	}
	return string(result.GetBuffer(), result.Length(), Encoding::UTF8);
}

struct CodeAnalyzeDesc
{
	bool ok;
	int64 version;
	SafePointer<Code> code;
	XV::CompilerStatusDesc * status;
	XV::CodeMetaInfo * meta;
};
struct IOData
{
	Volumes::Dictionary<string, string> headers;
	SafePointer<DataBlock> data;
};
class IOChannel : public Object
{
	handle _in, _out;

	void _read_header(string & key, string & value)
	{
		Array<char> buffer(0x100);
		while (true) {
			char chr;
			ReadFile(_in, &chr, 1);
			buffer << chr;
			if (buffer.Length() >= 2 && buffer[buffer.Length() - 2] == '\r' && buffer[buffer.Length() - 1] == '\n') {
				if (buffer.Length() == 2) {
					key = value = L"";
					return;
				}
				buffer[buffer.Length() - 2] = 0;
				auto line = string(buffer.GetBuffer(), -1, Encoding::UTF8);
				auto index = line.FindFirst(L": ");
				if (index < 0) {
					if (debug_console) debug_console->WriteLine(L"Wrong header format: " + line);
					throw InvalidFormatException();
				}
				key = line.Fragment(0, index);
				value = line.Fragment(index + 2, -1);
				return;
			}
		}
	}
	void _read_headers(IOData & data)
	{
		while (true) {
			string key, value;
			_read_header(key, value);
			if (key.Length()) data.headers.Append(key, value);
			else break;
		}
	}
	void _write_header(const string & key, const string & value)
	{
		if (key.Length()) {
			SafePointer<DataBlock> data = (key + L": " + value + L"\r\n").EncodeSequence(Encoding::UTF8, false);
			WriteFile(_out, data->GetBuffer(), data->Length());
		} else WriteFile(_out, "\r\n", 2);
	}
public:
	IOChannel(handle in, handle out) : _in(in), _out(out) {}
	virtual ~IOChannel(void) override {}
	void Read(IOData & data)
	{
		data.headers.Clear();
		data.data.SetReference(0);
		_read_headers(data);
		auto length_field = data.headers[L"Content-Length"];
		if (!length_field) {
			if (debug_console) debug_console->WriteLine(L"No length header");
			throw InvalidFormatException();
		}
		auto length = length_field->ToUInt32();
		data.data = new DataBlock(length);
		data.data->SetLength(length);
		if (length) {
			uint pos = 0;
			while (pos < length) {
				try { ReadFile(_in, data.data->GetBuffer() + pos, length - pos); pos += length; }
				catch (FileReadEndOfFileException & e) { pos += e.DataRead; if (!e.DataRead) throw; }
			}
		}
	}
	void Write(const IOData & data)
	{
		if (!data.data) throw InvalidArgumentException();
		_write_header(L"Content-Length", data.data->Length());
		for (auto & hdr : data.headers) _write_header(hdr.key, hdr.value);
		_write_header(L"", L"");
		WriteFile(_out, data.data->GetBuffer(), data.data->Length());
	}
	void Write(const void * data, int length)
	{
		if (!data) throw InvalidArgumentException();
		_write_header(L"Content-Length", length);
		_write_header(L"", L"");
		WriteFile(_out, data, length);
	}
	void Write(RPC::Message & message)
	{
		Streaming::MemoryStream stream(0x1000);
		Reflection::JsonSerializer serializer(&stream, true);
		message.Serialize(&serializer);
		access_sync->Wait();
		Write(stream.GetBuffer(), stream.Length());
		access_sync->Open();
	}
};
class CodeDocument : public Object
{
	struct _code_task {
		CodeAnalyzeDesc * desc;
		SafePointer<IDispatchTask> task;
	};

	string _uri;
	int64 _latest_version;
	SafePointer<IOChannel> _channel;
	SafePointer<Code> _current_version;
	SafePointer<Semaphore> _sync;
	Volumes::List<_code_task> _tasks;
	XV::CompilerStatusDesc _com_status;
	XV::CodeMetaInfo _com_meta;
	XV::CodeMetaInfo _func_meta;
	bool _analyze_in_progress;

	void _launch_analyzer(void)
	{
		if (_analyze_in_progress) return;
		SafePointer<CodeDocument> self;
		self.SetRetain(this);
		_analyze_in_progress = true;
		pool->SubmitTask(CreateFunctionalTask([self]() {
			while (true) {
				int64 version;
				string uri, name;
				SafePointer<Code> code;
				SafePointer<XV::ICompilerCallback> callback;
				self->GetCurrentVersion(version, uri, code);
				if (uri.Fragment(0, 7) == L"file://") {
					auto path = UncoverURI(uri);
					auto src_dir = Path::GetDirectory(path);
					name = Path::GetFileNameWithoutExtension(path);
					SafePointer<XV::ICompilerCallback> core = XV::CreateCompilerCallback(0, 0, module_search_paths.GetBuffer(), module_search_paths.Length(), 0);
					callback = XV::CreateCompilerCallback(&src_dir, 1, &src_dir, 1, core);
				} else {
					name = L"novus";
					callback = XV::CreateCompilerCallback(0, 0, module_search_paths.GetBuffer(), module_search_paths.Length(), 0);
				}
				XV::CompilerStatusDesc desc;
				XV::CodeMetaInfo meta;
				meta.autocomplete_at = -1;
				meta.function_info_at = -1;
				meta.error_absolute_from = -1;
				XV::CompileModule(name, *code, 0, callback, desc, &meta);
				if (self->CommitMeta(version, code, desc, meta)) break;
			}
		}));
	}
	void _execute_task(_code_task & task, Code * code)
	{
		task.desc->ok = true;
		task.desc->code.SetRetain(code);
		task.desc->meta = &_com_meta;
		task.desc->status = &_com_status;
		task.task->DoTask(0);
	}
	void _update_diagnostics(bool clear)
	{
		RPC::RequestMessage_PublishDiagnosticsParams message;
		message.jsonrpc = L"2.0";
		message.method = L"textDocument/publishDiagnostics";
		message.params.uri = _uri;
		message.params.version = _latest_version;
		if (!clear && _com_status.status != XV::CompilerStatus::Success) {
			RPC::Diagnostic diag;
			if (_com_meta.error_absolute_from >= 0 && _com_status.error_line_len > 0) {
				AbsoluteToLineColumn(*_current_version, _com_meta.error_absolute_from, diag.range.start.line, diag.range.start.character);
				AbsoluteToLineColumn(*_current_version, _com_meta.error_absolute_from + _com_status.error_line_len, diag.range.end.line, diag.range.end.character);
			} else {
				diag.range.start.line = diag.range.start.character = 0;
				AbsoluteToLineColumn(*_current_version, _current_version->Length(), diag.range.end.line, diag.range.end.character);
			}
			diag.severity = 1;
			diag.code = int(_com_status.status);
			diag.source = ENGINE_VI_APPNAME;
			diag.message = ErrorDescription(_com_status.status);
			message.params.diagnostics.InnerArray << diag;
		}
		_channel->Write(message);
	}
public:
	CodeDocument(IOChannel * channel, const RPC::DidOpenTextDocumentParams & document)
	{
		_channel.SetRetain(channel);
		_com_status.status = XV::CompilerStatus::Success;
		_analyze_in_progress = false;
		_sync = CreateSemaphore(0);
		_uri = document.textDocument.uri;
		_latest_version = document.textDocument.version;
		_current_version = new Code(0x100);
		if (!_sync) throw Exception();
		_current_version->SetLength(document.textDocument.text.GetEncodedLength(Encoding::UTF32));
		document.textDocument.text.Encode(_current_version->GetBuffer(), Encoding::UTF32, false);
		_launch_analyzer();
		_sync->Open();
	}
	virtual ~CodeDocument(void) override { _update_diagnostics(true); }
	void Update(const RPC::DidChangeTextDocumentParams & document)
	{
		_sync->Wait();
		for (auto & c : document.contentChanges) {
			uint from, to;
			LineColumnToAbsolute(*_current_version, c.range.start.line, c.range.start.character, from);
			LineColumnToAbsolute(*_current_version, c.range.end.line, c.range.end.character, to);
			Array<uint32> insert_text(1);
			insert_text.SetLength(c.text.GetEncodedLength(Encoding::UTF32));
			c.text.Encode(insert_text.GetBuffer(), Encoding::UTF32, false);
			uint remove = to - from;
			uint insert = insert_text.Length();
			int diff = insert - remove;
			if (diff < 0) {
				for (int i = to; i < _current_version->Length(); i++) _current_version->ElementAt(i + diff) = _current_version->ElementAt(i);
				_current_version->SetLength(_current_version->Length() + diff);
			} else if (diff > 0) {
				_current_version->SetLength(_current_version->Length() + diff);
				for (int i = _current_version->Length() - 1; i >= to + diff; i--) _current_version->ElementAt(i) = _current_version->ElementAt(i - diff);
			}
			for (int i = 0; i < insert; i++) _current_version->ElementAt(from + i) = insert_text[i];
		}
		_latest_version = document.textDocument.version;
		_launch_analyzer();
		_sync->Open();
	}
	bool CommitMeta(int64 forver, Code * code, const XV::CompilerStatusDesc & status, const XV::CodeMetaInfo & meta)
	{
		bool result;
		_sync->Wait();
		result = forver == _latest_version;
		_com_status = status;
		_com_meta = meta;
		auto current = _tasks.GetFirst();
		while (current) {
			auto next = current->GetNext();
			if (current->GetValue().desc->version <= forver) {
				auto & value = current->GetValue();
				_execute_task(value, code);
				_tasks.Remove(current);
			}
			current = next;
		}
		if (result) {
			_update_diagnostics(false);
			_analyze_in_progress = false;
		}
		_sync->Open();
		return result;
	}
	void GetCurrentVersion(int64 & version, string & uri, SafePointer<Code> & code)
	{
		_sync->Wait();
		version = _latest_version;
		uri = _uri;
		code = new Code(*_current_version);
		_sync->Open();
	}
	void PerformCodeTask(CodeAnalyzeDesc * desc, IDispatchTask * task)
	{
		_code_task t;
		t.desc = desc;
		t.task.SetRetain(task);
		_sync->Wait();
		desc->version = _latest_version;
		if (_analyze_in_progress) _tasks.InsertLast(t); else _execute_task(t, _current_version);
		_sync->Open();
	}
	void GetDocumentMetaStorage(XV::CodeMetaInfo & info)
	{
		_sync->Wait();
		try { info = _func_meta; } catch (...) {}
		_sync->Open();
	}
	void SetDocumentMetaStorage(const XV::CodeMetaInfo & info)
	{
		_sync->Wait();
		try { _func_meta = info; } catch (...) {}
		_sync->Open();
	}
};
class CodeRepositorium : public Object
{
	SafePointer<IOChannel> _channel;
	SafePointer<Semaphore> _sync;
	Volumes::ObjectDictionary<string, CodeDocument> _documents;
public:
	CodeRepositorium(IOChannel * channel) { _sync = CreateSemaphore(1); if (!_sync) throw Exception(); _channel.SetRetain(channel); }
	virtual ~CodeRepositorium(void) override {}
	void RegisterDocument(const RPC::DidOpenTextDocumentParams & document)
	{
		_sync->Wait();
		if (_documents.ElementExists(document.textDocument.uri)) _documents.Remove(document.textDocument.uri);
		try {
			SafePointer<CodeDocument> code = new CodeDocument(_channel, document);
			_documents.Append(document.textDocument.uri, code);
		} catch (...) { _sync->Open(); }
		_sync->Open();
	}
	void UpdateDocument(const RPC::DidChangeTextDocumentParams & document)
	{
		_sync->Wait();
		auto code = _documents.GetObjectByKey(document.textDocument.uri);
		if (code) code->Update(document);
		_sync->Open();
	}
	void UnregisterDocument(const RPC::DidCloseTextDocumentParams & document)
	{
		_sync->Wait();
		_documents.Remove(document.textDocument.uri);
		_sync->Open();
	}
	void PerformCodeTask(const string & uri, CodeAnalyzeDesc * desc, IDispatchTask * task)
	{
		_sync->Wait();
		auto code = _documents.GetObjectByKey(uri);
		if (code) code->PerformCodeTask(desc, task);
		else if (debug_console) debug_console->WriteLine(FormatString(L"No code document with URI ", uri));
		_sync->Open();
	}
	CodeDocument * FindDocument(const string & uri)
	{
		_sync->Wait();
		auto code = _documents.GetObjectByKey(uri);
		_sync->Open();
		return code;
	}
};
class MarkdownFormatter : public XV::IManualTextFormatter
{
public:
	virtual string PlainText(const string & text) override { return MarkdownEscape(text);  }
	virtual string BoldText(const string & inner) override { return L"**" + inner + L"**"; }
	virtual string ItalicText(const string & inner) override { return L"_" + inner + L"_"; }
	virtual string UnderlinedText(const string & inner) override { return inner; }
	virtual string LinkedText(const string & inner, const string & to) override { return inner; }
	virtual string Paragraph(const string & inner) override { return L"\n\n" + inner; }
	virtual string ListItem(int index, const string & inner) override { return L"\n" + string(index) + L". " + inner; }
	virtual string Code(const string & inner) override { return MarkdownEscape(inner); }
};

SafePointer<CodeRepositorium> code;

void RestoreObject(Reflection::Reflected & object, const IOData & data)
{
	Reflection::PropertyZeroInitializer init;
	object.EnumerateProperties(init);
	Streaming::MemoryStream stream(data.data->GetBuffer(), data.data->Length());
	Reflection::JsonSerializer serializer(&stream, true);
	object.Deserialize(&serializer);
}
void RespondWithError(IOChannel * channel, const RPC::RequestMessage & req, int error)
{
	Streaming::MemoryStream stream(0x1000);
	Reflection::JsonSerializer serializer(&stream, true);
	RPC::ResponseMessage_Fail responce;
	responce.jsonrpc = req.jsonrpc;
	responce.id = req.id;
	responce.error.code = error;
	responce.Serialize(&serializer);
	access_sync->Wait();
	channel->Write(stream.GetBuffer(), stream.Length());
	access_sync->Open();
}
void RespondWithObject(IOChannel * channel, const RPC::RequestMessage & req, RPC::ResponseMessage_Success & object)
{
	Streaming::MemoryStream stream(0x1000);
	Reflection::JsonSerializer serializer(&stream, true);
	object.jsonrpc = req.jsonrpc;
	object.id = req.id;
	object.Serialize(&serializer);
	access_sync->Wait();
	channel->Write(stream.GetBuffer(), stream.Length());
	access_sync->Open();
}
void RespondWithOK(IOChannel * channel, const RPC::RequestMessage & req)
{
	RPC::ResponseMessage_Success responce;
	RespondWithObject(channel, req, responce);
}

void AbsoluteToLineColumn(Code & code, uint abs, uint & line, uint & chr)
{
	int local_base = 0;
	line = 0;
	for (int i = 0; i < abs; i++) {
		if (code[i] == L'\n') line++;
		else if (i && code[i] != L'\n' && code[i] != L'\r' && (code[i - 1] == L'\n' || code[i - 1] == L'\r')) local_base = i;
	}
	if (abs && code[abs] != L'\n' && code[abs] != L'\r' && (code[abs - 1] == L'\n' || code[abs - 1] == L'\r')) local_base = abs;
	chr = abs - local_base;
}
void LineColumnToAbsolute(Code & code, uint line, uint chr, uint & abs)
{
	abs = 0;
	for (int i = 0; i < line; i++) {
		while (abs < code.Length() && code[abs] != L'\n') abs++;
		if (abs < code.Length()) abs++;
	}
	for (int i = 0; i < chr; i++) abs++;
}
string MarkdownEscape(const string & in)
{
	DynamicString result;
	for (int i = 0; i < in.Length(); i++) {
		if (in[i] < 32) continue;
		if (in[i] == L'\\' || in[i] == L'`' || in[i] == L'*' || in[i] == L'_' || in[i] == L'{' ||
			in[i] == L'}' || in[i] == L'[' || in[i] == L']' || in[i] == L'<' || in[i] == L'>' ||
			in[i] == L'(' || in[i] == L')' || in[i] == L'#' || in[i] == L'+' || in[i] == L'-' ||
			in[i] == L'.' || in[i] == L'!' || in[i] == L'|') result << L'\\';
		result << in[i];
	}
	return result.ToString();
}
string MarkdownFormat(const string & in)
{
	MarkdownFormatter fmt;
	return XV::FormatRichText(in, &fmt);
}
string ExtractContents(const XV::ManualSection * section)
{
	auto loc = section->GetContents(Assembly::CurrentLocale);
	if (loc.Length()) return MarkdownFormat(loc); else return MarkdownFormat(section->GetContents(L""));
}
string ExtractContents(const XV::ManualPage * page, XV::ManualSectionClass section)
{
	for (auto & s : page->GetSections()) if (s.GetClass() == section) return ExtractContents(&s);
	return L"";
}
const XV::ManualPage * FindManualPage(const XV::ManualVolume * volume, const string & path)
{
	if (!volume) return 0;
	auto result = volume->FindPage(path);
	if (result) return result;
	auto pure_path = path.Fragment(0, path.FindFirst(L':'));
	auto del = pure_path.FindLast(L'.');
	if (del >= 0) {
		auto parent_class_path = pure_path.Fragment(0, del);
		auto parent_class_page = volume->FindPage(parent_class_path);
		if (parent_class_page && (parent_class_page->GetClass() == XV::ManualPageClass::Class || parent_class_page->GetClass() == XV::ManualPageClass::Prototype)) {
			auto inh_list_sect = parent_class_page->FindSection(XV::ManualSectionClass::ObjectType);
			if (inh_list_sect) {
				auto inh_list = inh_list_sect->GetContents(L"").Split(L'\33');
				auto postfix = path.Fragment(del, -1);
				for (auto & inh : inh_list) try {
					auto other_class_path = XI::Module::TypeReference(inh).GetClassName();
					auto other_page = FindManualPage(volume, other_class_path + postfix);
					if (other_page) return other_page;
				} catch (...) {}
				return 0;
			} else return 0;
		} else return 0;
	} else return 0;
}

void HandleMessage(IOChannel * channel, const RPC::RequestMessage & base, const IOData & data)
{
	if (base.method == L"textDocument/didOpen") {
		RPC::RequestMessage_DidOpenTextDocumentParams info;
		RestoreObject(info, data);
		if (debug_console) debug_console->WriteLine(FormatString(L"Open: %0 - %1", info.params.textDocument.languageId, info.params.textDocument.uri));
		code->RegisterDocument(info.params);
	} else if (base.method == L"textDocument/didChange") {
		RPC::RequestMessage_DidChangeTextDocumentParams info;
		RestoreObject(info, data);
		if (debug_console) debug_console->WriteLine(FormatString(L"Change: %0", info.params.textDocument.uri));
		code->UpdateDocument(info.params);
	} else if (base.method == L"textDocument/didClose") {
		RPC::RequestMessage_DidCloseTextDocumentParams info;
		RestoreObject(info, data);
		if (debug_console) debug_console->WriteLine(FormatString(L"Close: %0", info.params.textDocument.uri));
		code->UnregisterDocument(info.params);
	} else if (base.method == L"$/cancelRequest") {
	} else if (base.method == L"textDocument/foldingRange") {
		RPC::RequestMessage_FoldingRangeParams info;
		RestoreObject(info, data);
		auto task = CreateStructuredTask<CodeAnalyzeDesc>([channel, base](CodeAnalyzeDesc & value) {
			if (value.ok) {
				RPC::ResponseMessage_Success_FoldingRange result;
				Volumes::Stack<XV::CodeRangeInfo> opens;
				for (auto & info : value.meta->info) {
					auto & range = info.value;
					if (range.flags & XV::CodeRangeClauseOpen) {
						opens.Push(range);
					} else if (range.flags & XV::CodeRangeClauseClose) {
						try {
							auto open = opens.Pop();
							RPC::FoldingRange fr;
							while (open.from && ((*value.code)[open.from - 1] == L' ' || (*value.code)[open.from - 1] == L'\t' ||
								(*value.code)[open.from - 1] == L'\n' || (*value.code)[open.from - 1] == L'\r')) open.from--;
							AbsoluteToLineColumn(*value.code, open.from, fr.startLine, fr.startCharacter);
							AbsoluteToLineColumn(*value.code, range.from + range.length, fr.endLine, fr.endCharacter);
							if (fr.startLine != fr.endLine) {
								fr.kind = L"region";
								fr.collapsedText = L"{ ... }";
								result.result.InnerArray << fr;
							}
						} catch (...) {}
					} else if (range.tag == XV::CodeRangeTag::Comment) {
						RPC::FoldingRange fr;
						AbsoluteToLineColumn(*value.code, range.from, fr.startLine, fr.startCharacter);
						AbsoluteToLineColumn(*value.code, range.from + range.length, fr.endLine, fr.endCharacter);
						if (fr.startLine != fr.endLine) {
							fr.kind = L"comment";
							fr.collapsedText = L"/*...*/";
							result.result.InnerArray << fr;
						}
					}
				}
				RespondWithObject(channel, base, result);
			} else RespondWithError(channel, base, RPC::Error::MethodNotFound);
		});
		code->PerformCodeTask(info.params.textDocument.uri, &task->Value1, task);
	} else if (base.method == L"textDocument/hover") {
		RPC::RequestMessage_HoverParams info;
		RestoreObject(info, data);
		auto task = CreateStructuredTask<CodeAnalyzeDesc>([channel, info](CodeAnalyzeDesc & value) {
			if (value.ok) {
				RPC::ResponseMessage_Success_Hover result;
				uint abs_pos;
				LineColumnToAbsolute(*value.code, info.params.position.line, info.params.position.character, abs_pos);
				result.result.range.start = info.params.position;
				result.result.range.end = info.params.position;
				result.result.range.end.character++;
				result.result.contents.kind = L"plaintext";
				for (auto & info : value.meta->info) {
					auto & range = info.value;
					if (range.from <= abs_pos && range.from + range.length > abs_pos) {
						AbsoluteToLineColumn(*value.code, range.from, result.result.range.start.line, result.result.range.start.character);
						AbsoluteToLineColumn(*value.code, range.from + range.length, result.result.range.end.line, result.result.range.end.character);
						DynamicString text;
						auto desc = TagDescription(range.tag);
						if (desc.Length()) {
							if (text.Length()) text += L"\n\n";
							text += L"*" + MarkdownEscape(desc) + L"*";
						}
						if (range.identifier.Length()) {
							if (text.Length()) text += L"\n\n";
							text += Localized(181) + L"**" + MarkdownEscape(range.identifier) + L"**";
						}
						if (range.type.Length()) {
							if (text.Length()) text += L"\n\n";
							text += Localized(182) + L"**" + MarkdownEscape(range.type) + L"**";
						}
						if (range.value.Length()) {
							if (text.Length()) text += L"\n\n";
							text += Localized(183) + L"**\'" + MarkdownEscape(range.value) + L"\'**";
						}
						if (manual) {
							auto record = FindManualPage(manual, range.path);
							if (record) {
								if (text.Length()) text += L"\n\n";
								text += ExtractContents(record, XV::ManualSectionClass::Summary);
							}
						}
						result.result.contents.kind = L"markdown";
						result.result.contents.value = text.ToString();
						break;
					}
				}
				RespondWithObject(channel, info, result);
			} else RespondWithError(channel, info, RPC::Error::MethodNotFound);
		});
		code->PerformCodeTask(info.params.textDocument.uri, &task->Value1, task);
	} else if (base.method == L"textDocument/semanticTokens/full") {
		RPC::RequestMessage_SemanticTokensParams info;
		RestoreObject(info, data);
		auto task = CreateStructuredTask<CodeAnalyzeDesc>([channel, info](CodeAnalyzeDesc & value) {
			if (value.ok) {
				RPC::ResponseMessage_Success_SemanticTokens result;
				result.result.data = Array<uint32>(0x10000);
				auto & data = result.result.data;
				uint prev_line = 0;
				uint prev_char = 0;
				for (auto & info : value.meta->info) {
					auto & range = info.value;
					uint line, chr;
					AbsoluteToLineColumn(*value.code, range.from, line, chr);
					int tag = -1, subtag = 0;
					uint32 block[5];
					block[0] = line - prev_line;
					if (line == prev_line) block[1] = chr - prev_char;
					else block[1] = chr;
					block[2] = range.length;
					if (range.flags & XV::CodeRangeSymbolDefinition) subtag |= 1;
					if (range.tag == XV::CodeRangeTag::IdentifierConstant) subtag |= 2;
					else if (range.tag == XV::CodeRangeTag::LiteralBoolean) subtag |= 2;
					else if (range.tag == XV::CodeRangeTag::LiteralNumeric) subtag |= 2;
					else if (range.tag == XV::CodeRangeTag::LiteralString) subtag |= 2;
					else if (range.tag == XV::CodeRangeTag::LiteralNull) subtag |= 2;
					if (range.tag == XV::CodeRangeTag::Keyword) tag = 6;
					else if (range.tag == XV::CodeRangeTag::LiteralBoolean) tag = 6;
					else if (range.tag == XV::CodeRangeTag::LiteralNumeric) tag = 8;
					else if (range.tag == XV::CodeRangeTag::LiteralString) tag = 7;
					else if (range.tag == XV::CodeRangeTag::LiteralNull) tag = 6;
					else if (range.tag == XV::CodeRangeTag::IdentifierUnknown) tag = 1;
					else if (range.tag == XV::CodeRangeTag::IdentifierNamespace) tag = 0;
					else if (range.tag == XV::CodeRangeTag::IdentifierType) tag = 2;
					else if (range.tag == XV::CodeRangeTag::IdentifierPrototype) tag = 9;
					else if (range.tag == XV::CodeRangeTag::IdentifierFunction) tag = 5;
					else if (range.tag == XV::CodeRangeTag::IdentifierConstant) tag = 3;
					else if (range.tag == XV::CodeRangeTag::IdentifierVariable) tag = 3;
					else if (range.tag == XV::CodeRangeTag::IdentifierProperty) tag = 4;
					else if (range.tag == XV::CodeRangeTag::IdentifierField) tag = 3;
					if (tag >= 0) {
						block[3] = tag;
						block[4] = subtag;
						data.Append(block, 5);
						prev_line = line;
						prev_char = chr;
					}
				}
				RespondWithObject(channel, info, result);
			} else RespondWithError(channel, info, RPC::Error::MethodNotFound);
		});
		code->PerformCodeTask(info.params.textDocument.uri, &task->Value1, task);
	} else if (base.method == L"textDocument/documentSymbol") {
		RPC::RequestMessage_DocumentSymbolParams info;
		RestoreObject(info, data);
		auto task = CreateStructuredTask<CodeAnalyzeDesc>([channel, info](CodeAnalyzeDesc & value) {
			if (value.ok) {
				RPC::ResponseMessage_Success_SymbolInformation result;
				for (auto & mi : value.meta->info) {
					auto & range = mi.value;
					if (range.flags & XV::CodeRangeSymbolDefinition) {
						if (range.flags & XV::CodeRangeSymbolLocal) continue;
						if (!range.identifier.Length()) continue;
						RPC::SymbolInformation inf;
						auto del = range.identifier.FindLast(L'.');
						inf.name = range.identifier;
						inf.location.uri = info.params.textDocument.uri;
						AbsoluteToLineColumn(*value.code, range.from, inf.location.range.start.line, inf.location.range.start.character);
						AbsoluteToLineColumn(*value.code, range.from + range.length, inf.location.range.end.line, inf.location.range.end.character);
						if (range.tag == XV::CodeRangeTag::IdentifierNamespace) inf.kind = 3;
						else if (range.tag == XV::CodeRangeTag::IdentifierType) inf.kind = 5;
						else if (range.tag == XV::CodeRangeTag::IdentifierFunction) inf.kind = 12;
						else if (range.tag == XV::CodeRangeTag::IdentifierProperty) inf.kind = 7;
						else if (range.tag == XV::CodeRangeTag::IdentifierField) inf.kind = 8;
						else if (range.tag == XV::CodeRangeTag::IdentifierVariable) inf.kind = 13;
						else if (range.tag == XV::CodeRangeTag::IdentifierConstant) inf.kind = 14;
						else inf.kind = 13;
						result.result.InnerArray << inf;
					}
				}
				RespondWithObject(channel, info, result);
			} else RespondWithError(channel, info, RPC::Error::MethodNotFound);
		});
		code->PerformCodeTask(info.params.textDocument.uri, &task->Value1, task);
	} else if (base.method == L"textDocument/definition") {
		RPC::RequestMessage_DefinitionParams info;
		RestoreObject(info, data);
		auto task = CreateStructuredTask<CodeAnalyzeDesc>([channel, info](CodeAnalyzeDesc & value) {
			if (value.ok) {
				RPC::ResponseMessage_Success_Location result;
				uint abs_pos;
				LineColumnToAbsolute(*value.code, info.params.position.line, info.params.position.character, abs_pos);
				result.result.uri = info.params.textDocument.uri;
				result.result.range.start = info.params.position;
				result.result.range.end = info.params.position;
				result.result.range.end.character++;
				string smbl_for;
				for (auto & info : value.meta->info) {
					auto & range = info.value;
					if (range.from <= abs_pos && range.from + range.length > abs_pos && range.identifier.Length()) {
						smbl_for = range.identifier;
						break;
					}
				}
				if (smbl_for.Length()) {
					for (auto & info : value.meta->info) {
						auto & range = info.value;
						if (range.identifier == smbl_for && range.flags & XV::CodeRangeSymbolDefinition) {
							AbsoluteToLineColumn(*value.code, range.from, result.result.range.start.line, result.result.range.start.character);
							AbsoluteToLineColumn(*value.code, range.from + range.length, result.result.range.end.line, result.result.range.end.character);
							break;
						}
					}
				}
				RespondWithObject(channel, info, result);
			} else RespondWithError(channel, info, RPC::Error::MethodNotFound);
		});
		code->PerformCodeTask(info.params.textDocument.uri, &task->Value1, task);
	} else if (base.method == L"textDocument/completion") {
		RPC::RequestMessage_DefinitionParams info;
		RestoreObject(info, data);
		SafePointer<CodeDocument> document;
		SafePointer<IOChannel> channel_ref;
		document.SetRetain(code->FindDocument(info.params.textDocument.uri));
		channel_ref.SetRetain(channel);
		if (document) pool->SubmitTask(CreateFunctionalTask([document, channel_ref, info]() {
			int64 version;
			string uri, name;
			SafePointer<Code> code;
			SafePointer<XV::ICompilerCallback> callback;
			document->GetCurrentVersion(version, uri, code);
			if (uri.Fragment(0, 7) == L"file://") {
				auto path = UncoverURI(uri);
				auto src_dir = Path::GetDirectory(path);
				name = Path::GetFileNameWithoutExtension(path);
				SafePointer<XV::ICompilerCallback> core = XV::CreateCompilerCallback(0, 0, module_search_paths.GetBuffer(), module_search_paths.Length(), 0);
				callback = XV::CreateCompilerCallback(&src_dir, 1, &src_dir, 1, core);
			} else {
				name = L"novus";
				callback = XV::CreateCompilerCallback(0, 0, module_search_paths.GetBuffer(), module_search_paths.Length(), 0);
			}
			XV::CompilerStatusDesc desc;
			XV::CodeMetaInfo meta;
			uint ac;
			LineColumnToAbsolute(*code, info.params.position.line, info.params.position.character, ac);
			code->Append(0);
			code->ElementAt(ac) = L'A';
			meta.autocomplete_at = ac;
			meta.function_info_at = -1;
			meta.error_absolute_from = -1;
			XV::CompileModule(name, *code, 0, callback, desc, &meta);
			RPC::ResponseMessage_Success_CompletionItem responce;
			for (auto & a : meta.autocomplete) {
				RPC::CompletionItem com;
				com.label = com.insertText = a.key;
				com.labelDetails.description = com.detail = TagDescription(a.value);
				if (a.value == XV::CodeRangeTag::IdentifierNamespace) com.kind = 3;
				else if (a.value == XV::CodeRangeTag::IdentifierType) com.kind = 5;
				else if (a.value == XV::CodeRangeTag::IdentifierFunction) com.kind = 12;
				else if (a.value == XV::CodeRangeTag::IdentifierProperty) com.kind = 7;
				else if (a.value == XV::CodeRangeTag::IdentifierField) com.kind = 8;
				else if (a.value == XV::CodeRangeTag::IdentifierVariable) com.kind = 13;
				else if (a.value == XV::CodeRangeTag::IdentifierConstant) com.kind = 14;
				else com.kind = 13;
				responce.result.InnerArray << com;
			}
			RespondWithObject(channel_ref, info, responce);
		}));
	} else if (base.method == L"textDocument/signatureHelp") {
		RPC::RequestMessage_SignatureHelpParams info;
		RestoreObject(info, data);
		SafePointer<CodeDocument> document;
		SafePointer<IOChannel> channel_ref;
		document.SetRetain(code->FindDocument(info.params.textDocument.uri));
		channel_ref.SetRetain(channel);
		if (document) pool->SubmitTask(CreateFunctionalTask([document, channel_ref, info]() {
			int64 version;
			string uri, name;
			SafePointer<Code> code;
			SafePointer<XV::ICompilerCallback> callback;
			document->GetCurrentVersion(version, uri, code);
			if (uri.Fragment(0, 7) == L"file://") {
				auto path = UncoverURI(uri);
				auto src_dir = Path::GetDirectory(path);
				name = Path::GetFileNameWithoutExtension(path);
				SafePointer<XV::ICompilerCallback> core = XV::CreateCompilerCallback(0, 0, module_search_paths.GetBuffer(), module_search_paths.Length(), 0);
				callback = XV::CreateCompilerCallback(&src_dir, 1, &src_dir, 1, core);
			} else {
				name = L"novus";
				callback = XV::CreateCompilerCallback(0, 0, module_search_paths.GetBuffer(), module_search_paths.Length(), 0);
			}
			XV::CompilerStatusDesc desc;
			XV::CodeMetaInfo meta;
			uint ac;
			LineColumnToAbsolute(*code, info.params.position.line, info.params.position.character, ac);
			code->Append(0);
			code->ElementAt(ac) = L'A';
			meta.autocomplete_at = -1;
			meta.function_info_at = ac;
			meta.error_absolute_from = -1;
			XV::CompileModule(name, *code, 0, callback, desc, &meta);
			RPC::ResponseMessage_Success_SignatureHelp responce;
			uint count = meta.overloads.Count();
			if (count) {
				responce.result.activeParameter = meta.function_info_argument;
				responce.result.activeSignature = min(info.params.context.activeSignatureHelp.activeSignature, count - 1);
				for (auto & o : meta.overloads) {
					auto page = FindManualPage(manual, o.path);
					RPC::SignatureInformation inf;
					inf.label = FormatString(L"%1 %0(", o.identifier, ArgumentInfoToString(o.retval));
					inf.documentation.kind = L"markdown";
					if (page) {
						inf.documentation.value = ExtractContents(page, XV::ManualSectionClass::Summary);
					}
					int index = 0;
					for (auto & a : o.args) {
						if (index) inf.label += L", "; index++;
						RPC::ParameterInformation pi;
						pi.documentation.kind = L"markdown";
						pi.label[0] = inf.label.GetEncodedLength(Encoding::UTF16);
						inf.label += ArgumentInfoToString(a);
						if (page) {
							for (auto & s : page->GetSections()) if (s.GetSubjectIndex() == index - 1 && s.GetClass() == XV::ManualSectionClass::ArgumentSection) {
								inf.label += L" " + s.GetSubjectName();
								pi.documentation.value = L"**" + ArgumentInfoToString(a) + L" " + MarkdownEscape(s.GetSubjectName()) + L"**: " + ExtractContents(&s);
								break;
							}
						}
						pi.label[1] = inf.label.GetEncodedLength(Encoding::UTF16);
						inf.parameters.InnerArray.Append(pi);
					}
					inf.label += L")";
					responce.result.signatures.InnerArray.Append(inf);
				}
			}
			RespondWithObject(channel_ref, info, responce);
		}));
	} else RespondWithError(channel, base, RPC::Error::MethodNotFound);
}

int Main(void)
{
	int exit_code = 0;
	try {
		bool run = false;
		int num_threads = 0;
		SafePointer< Array<string> > args = GetCommandLine();
		for (int i = 1; i < args->Length(); i++) {
			if (args->ElementAt(i) == L"--act") {
				debug_client = new Cluster::Client;
				debug_client->SetConnectionServiceName(L"XV Servus Linguae");
				debug_client->SetConnectionServiceID(L"engine.xv.sl");
				debug_client->Connect();
				debug_console = debug_client->CreateLoggingService();
				debug_console->WriteLine("Salve!");
			} else if (args->ElementAt(i) == L"--opera") {
				run = true;
			} else if (args->ElementAt(i) == L"--numerus") {
				i++; if (i >= args->Length()) return 2;
				num_threads = args->ElementAt(i).ToInt32();
			} else return 2;
		}
		if (!run) return 0;
		SafePointer<IOChannel> channel = new IOChannel(GetStandardInput(), GetStandardOutput());
		pool = new ThreadPool(num_threads);
		access_sync = CreateSemaphore(1);
		code = new CodeRepositorium(channel);
		if (!access_sync) throw Exception();
		int state = 0;
		if (debug_console && num_threads) {
			debug_console->WriteLine(FormatString(L"Thread count: %0", num_threads));
		}
		while (true) {
			IOData data;
			RPC::RequestMessage base_request;
			channel->Read(data);
			RestoreObject(base_request, data);
			if (debug_console) {
				debug_console->WriteLine(FormatString(L"Request ID=%0, Method=%1", base_request.id, base_request.method));
			}
			if (base_request.method == L"initialize") {
				if (state) {
					RespondWithError(channel, base_request, RPC::Error::RequestFailed);
				} else {
					RPC::RequestMessage_InitializeParams init;
					RPC::ResponseMessage_Success_InitializeResult result;
					RestoreObject(init, data);
					Assembly::CurrentLocale = init.params.locale.Fragment(0, 2);
					try {
						Codec::InitializeDefaultCodecs();
						auto root = Path::GetDirectory(GetExecutablePath());
						SafePointer<Registry> xv_conf = XX::LoadConfiguration(root + L"/xv.ini");
						try {
							auto core = xv_conf->GetValueString(L"XE");
							if (core) XX::IncludeComponent(module_search_paths, root + L"/" + core);
						} catch (...) {}
						try {
							auto store = xv_conf->GetValueString(L"Entheca");
							if (store.Length()) XX::IncludeStoreIntegration(module_search_paths, root + L"/" + store);
						} catch (...) {}
						for (auto & msp : module_search_paths) try {
							SafePointer< Array<string> > files = Search::GetFiles(msp + L"/*." + string(XI::FileExtensionManual));
							for (auto & f : *files) try {
								SafePointer<Streaming::Stream> stream = new Streaming::FileStream(msp + "/" + f, Streaming::AccessRead, Streaming::OpenExisting);
								SafePointer<XV::ManualVolume> volume = new XV::ManualVolume(stream);
								if (manual) manual->Unify(volume); else manual = volume;
							} catch (...) {}
						} catch (...) {}
						auto language_override = xv_conf->GetValueString(L"Lingua");
						if (language_override.Length()) Assembly::CurrentLocale = language_override;
						auto localizations = xv_conf->GetValueString(L"Locale");
						if (localizations.Length()) {
							try {
								SafePointer<Streaming::Stream> table = new Streaming::FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", Streaming::AccessRead, Streaming::OpenExisting);
								localization = new StringTable(table);
							} catch (...) {}
							if (!localization) {
								auto language_default = xv_conf->GetValueString(L"LinguaDefalta");
								try {
									SafePointer<Streaming::Stream> table = new Streaming::FileStream(root + L"/" + localizations + L"/" + language_default + L".ecst", Streaming::AccessRead, Streaming::OpenExisting);
									localization = new StringTable(table);
								} catch (...) {}
							}
						}
						result.result.serverInfo.name = ENGINE_VI_APPNAME;
						result.result.serverInfo.version = ENGINE_VI_APPVERSION;
						result.result.capabilities.positionEncoding = L"utf-32";
						result.result.capabilities.textDocumentSync = 2;
						result.result.capabilities.completionProvider.triggerCharacters << L".";
						result.result.capabilities.completionProvider.completionItem.labelDetailsSupport = true;
						result.result.capabilities.hoverProvider = true;
						result.result.capabilities.definitionProvider = true;
						result.result.capabilities.documentSymbolProvider = true;
						result.result.capabilities.foldingRangeProvider = true;
						result.result.capabilities.semanticTokensProvider.full = true;
						result.result.capabilities.signatureHelpProvider.triggerCharacters << L"(";
						result.result.capabilities.signatureHelpProvider.triggerCharacters << L"[";
						result.result.capabilities.signatureHelpProvider.triggerCharacters << L",";
						auto & legend = result.result.capabilities.semanticTokensProvider.legend;
						legend.tokenTypes << L"namespace";			// 0
						legend.tokenTypes << L"type";				// 1
						legend.tokenTypes << L"class";				// 2
						legend.tokenTypes << L"variable";			// 3
						legend.tokenTypes << L"property";			// 4
						legend.tokenTypes << L"function";			// 5
						legend.tokenTypes << L"keyword";			// 6
						legend.tokenTypes << L"string";				// 7
						legend.tokenTypes << L"number";				// 8
						legend.tokenTypes << L"macro";				// 9
						legend.tokenModifiers << L"declaration";	// 1
						legend.tokenModifiers << L"readonly";		// 2
						RespondWithObject(channel, init, result);
					} catch (...) { RespondWithError(channel, base_request, RPC::Error::RequestFailed); }
				}
			} else if (base_request.method == L"initialized") {
				state = 1;
			} else if (base_request.method == L"shutdown") {
				if (state == 1) {
					pool->Wait();
					pool.SetReference(0);
					code.SetReference(0);
					state = 2;
					RespondWithOK(channel, base_request);
				} else RespondWithError(channel, base_request, RPC::Error::ServerNotInitialized);
			} else if (base_request.method == L"exit") {
				if (state != 2) exit_code = 1;
				break;
			} else {
				if (state == 1) {
					try { HandleMessage(channel, base_request, data); }
					catch (...) { RespondWithError(channel, base_request, RPC::Error::InternalError); }
				} else RespondWithError(channel, base_request, RPC::Error::ServerNotInitialized);
			}
		}
		if (debug_console) {
			debug_console->WriteLine("Vale!");
			debug_console.SetReference(0);
			debug_client->Disconnect();
			debug_client->Wait();
			debug_client.SetReference(0);
		}
	} catch (Exception & e) {
		if (debug_console) debug_console->WriteLine(L"Error: " + e.ToString());
		return 1;
	} catch (...) { return 1; }
	return exit_code;
}