﻿#pragma once

#include <EngineRuntime.h>

namespace RPC {
	ENGINE_REFLECTED_CLASS(Message, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, jsonrpc);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage, Message)
		ENGINE_DEFINE_REFLECTED_PROPERTY(LONGINTEGER, id);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, method);
		// INHERIT AND DEFINE 'params'
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success, Message)
		ENGINE_DEFINE_REFLECTED_PROPERTY(LONGINTEGER, id);
		// INHERIT AND DEFINE 'result'
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseError, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, code);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, message);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Fail, Message)
		ENGINE_DEFINE_REFLECTED_PROPERTY(LONGINTEGER, id);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(ResponseError, error);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(InitializeParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, locale);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_InitializeParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(InitializeParams, params);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(SemanticTokensLegend, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_ARRAY(STRING, tokenTypes);
		ENGINE_DEFINE_REFLECTED_ARRAY(STRING, tokenModifiers);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SemanticTokensOptions, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SemanticTokensLegend, legend);
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, full);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(CompletionItemType, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, labelDetailsSupport);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(CompletionOptions, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_ARRAY(STRING, triggerCharacters);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(CompletionItemType, completionItem);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SignatureHelpOptions, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_ARRAY(STRING, triggerCharacters);
		ENGINE_DEFINE_REFLECTED_ARRAY(STRING, retriggerCharacters);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ServerCapabilities, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, positionEncoding);
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, textDocumentSync);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(CompletionOptions, completionProvider);
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, hoverProvider);
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, definitionProvider);
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, documentSymbolProvider);
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, foldingRangeProvider);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SemanticTokensOptions, semanticTokensProvider);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SignatureHelpOptions, signatureHelpProvider);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ServerInfo, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, name);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, version);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(InitializeResult, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(ServerCapabilities, capabilities);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(ServerInfo, serverInfo);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_InitializeResult, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(InitializeResult, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(TextDocumentItem, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, uri);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, languageId);
		ENGINE_DEFINE_REFLECTED_PROPERTY(LONGINTEGER, version);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, text);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(TextDocumentIdentifier, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, uri);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(VersionedTextDocumentIdentifier, TextDocumentIdentifier)
		ENGINE_DEFINE_REFLECTED_PROPERTY(LONGINTEGER, version);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(Position, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, line);
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, character);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(Range, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Position, start);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Position, end);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(TextDocumentContentChangeEvent, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Range, range);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, text);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(DidOpenTextDocumentParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(TextDocumentItem, textDocument);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(DidChangeTextDocumentParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(VersionedTextDocumentIdentifier, textDocument);
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(TextDocumentContentChangeEvent, contentChanges);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(DidCloseTextDocumentParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(TextDocumentIdentifier, textDocument);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_DidOpenTextDocumentParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(DidOpenTextDocumentParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_DidChangeTextDocumentParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(DidChangeTextDocumentParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_DidCloseTextDocumentParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(DidCloseTextDocumentParams, params);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(FoldingRange, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, startLine);
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, startCharacter);
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, endLine);
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, endCharacter);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, kind);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, collapsedText);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(FoldingRangeParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(TextDocumentIdentifier, textDocument);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_FoldingRangeParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(FoldingRangeParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_FoldingRange, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(FoldingRange, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(Diagnostic, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Range, range);
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, severity);
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, code);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, source);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, message);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(PublishDiagnosticsParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, uri);
		ENGINE_DEFINE_REFLECTED_PROPERTY(LONGINTEGER, version);
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(Diagnostic, diagnostics);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_PublishDiagnosticsParams, Message)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, method);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(PublishDiagnosticsParams, params);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(TextDocumentPositionParams, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(TextDocumentIdentifier, textDocument);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Position, position);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(MarkupContent, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, kind);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, value);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(Hover, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(MarkupContent, contents);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Range, range);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_HoverParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(TextDocumentPositionParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_Hover, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Hover, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(RequestMessage_SemanticTokensParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(FoldingRangeParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SemanticTokens, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_ARRAY(UINT32, data);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_SemanticTokens, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SemanticTokens, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(RequestMessage_DocumentSymbolParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(FoldingRangeParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(Location, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, uri);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Range, range);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SymbolInformation, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, name);
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, kind);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Location, location);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_SymbolInformation, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(SymbolInformation, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(RequestMessage_DefinitionParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(TextDocumentPositionParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_Location, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(Location, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(CompletionItemLabelDetails, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, description);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(CompletionItem, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, label);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(CompletionItemLabelDetails, labelDetails);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, detail);
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, kind);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, insertText);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_CompletionItem, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(CompletionItem, result);
	ENGINE_END_REFLECTED_CLASS

	ENGINE_REFLECTED_CLASS(ParameterInformation, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY_VOLUME(UINT32, label, 2);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(MarkupContent, documentation);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SignatureInformation, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, label);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(MarkupContent, documentation);
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(ParameterInformation, parameters);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SignatureHelp, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_GENERIC_ARRAY(SignatureInformation, signatures);
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, activeSignature);
		ENGINE_DEFINE_REFLECTED_PROPERTY(UINT32, activeParameter);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SignatureHelpContext, Engine::Reflection::Reflected)
		ENGINE_DEFINE_REFLECTED_PROPERTY(INTEGER, triggerKind);
		ENGINE_DEFINE_REFLECTED_PROPERTY(STRING, triggerCharacter);
		ENGINE_DEFINE_REFLECTED_PROPERTY(BOOLEAN, isRetrigger);
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SignatureHelp, activeSignatureHelp);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(SignatureHelpParams, TextDocumentPositionParams)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SignatureHelpContext, context);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(RequestMessage_SignatureHelpParams, RequestMessage)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SignatureHelpParams, params);
	ENGINE_END_REFLECTED_CLASS
	ENGINE_REFLECTED_CLASS(ResponseMessage_Success_SignatureHelp, ResponseMessage_Success)
		ENGINE_DEFINE_REFLECTED_GENERIC_PROPERTY(SignatureHelp, result);
	ENGINE_END_REFLECTED_CLASS

	namespace Error {
		constexpr int ParseError = -32700;
		constexpr int InvalidRequest = -32600;
		constexpr int MethodNotFound = -32601;
		constexpr int InvalidParams = -32602;
		constexpr int InternalError = -32603;
		constexpr int ServerNotInitialized = -32002;
		constexpr int UnknownErrorCode = -32001;
		constexpr int RequestFailed = -32803;
		constexpr int ServerCancelled = -32802;
		constexpr int ContentModified = -32801;
		constexpr int RequestCancelled = -32800;
	}
}