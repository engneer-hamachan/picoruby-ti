package main

import (
	"context"

	"github.com/owenrumney/go-lsp/document"
	"github.com/owenrumney/go-lsp/lsp"
	"github.com/owenrumney/go-lsp/server"
)

type languageServerHandler struct {
	openedDocuments       *document.Store
	picorubyTypeInference *picorubyTypeInference
	client                *server.Client
}

func (languageServer *languageServerHandler) SetClient(client *server.Client) {
	languageServer.client = client
}

func (languageServer *languageServerHandler) Initialize(
	_ context.Context,
	_ *lsp.InitializeParams,
) (*lsp.InitializeResult, error) {

	capabilityEnabled := true

	return &lsp.InitializeResult{
		Capabilities: lsp.ServerCapabilities{
			CompletionProvider: &lsp.CompletionOptions{
				TriggerCharacters: []string{
					"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
					"l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
					"w", "x", "y", "z",
					"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
					"L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
					"W", "X", "Y", "Z",
					"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
					".", "_",
				},
			},
			HoverProvider: &capabilityEnabled,
			TextDocumentSync: &lsp.TextDocumentSyncOptions{
				OpenClose: &capabilityEnabled,
				Change:    lsp.SyncIncremental,
			},
		},
		ServerInfo: &lsp.ServerInfo{
			Name: "picoruby-ti-lsp",
		},
	}, nil
}

func (languageServer *languageServerHandler) Shutdown(
	_ context.Context,
) error {
	return nil
}

func (languageServer *languageServerHandler) DidOpen(
	requestContext context.Context,
	parameters *lsp.DidOpenTextDocumentParams,
) error {

	_, openError := languageServer.openedDocuments.Open(parameters)
	if openError != nil {
		return openError
	}

	return languageServer.publishDiagnostics(
		requestContext,
		parameters.TextDocument.URI,
	)
}

func (languageServer *languageServerHandler) DidChange(
	requestContext context.Context,
	parameters *lsp.DidChangeTextDocumentParams,
) error {

	_, changeError := languageServer.openedDocuments.Change(parameters)
	if changeError != nil {
		return changeError
	}

	return languageServer.publishDiagnostics(
		requestContext,
		parameters.TextDocument.URI,
	)
}

func (languageServer *languageServerHandler) DidClose(
	requestContext context.Context,
	parameters *lsp.DidCloseTextDocumentParams,
) error {

	languageServer.openedDocuments.Close(parameters)

	if languageServer.client == nil {
		return nil
	}

	return languageServer.client.PublishDiagnostics(
		requestContext,
		&lsp.PublishDiagnosticsParams{
			URI:         parameters.TextDocument.URI,
			Diagnostics: []lsp.Diagnostic{},
		},
	)
}

func (languageServer *languageServerHandler) publishDiagnostics(
	requestContext context.Context,
	documentURI lsp.DocumentURI,
) error {

	if languageServer.client == nil {
		return nil
	}

	rubyDocument, rubyDocumentAvailable :=
		languageServer.getRubyDocument(documentURI)

	if !rubyDocumentAvailable {
		return nil
	}

	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(documentURI, rubyDocument.Text())

	rubyCodeEndByteOffset := rubyCodeByteOffset + len(rubyDocument.Text())

	engineDiagnostics :=
		languageServer.picorubyTypeInference.makeDiagnosticsByPicoRuby(
			combinedRubyCode,
		)

	diagnosticSeverity := lsp.SeverityError
	diagnostics := make([]lsp.Diagnostic, 0, len(engineDiagnostics))

	for _, engineDiagnostic := range engineDiagnostics {
		if engineDiagnostic.startByteOffset < rubyCodeByteOffset ||
			engineDiagnostic.endByteOffset > rubyCodeEndByteOffset {
			continue
		}

		startPosition, startPositionError :=
			rubyDocument.PositionAt(
				engineDiagnostic.startByteOffset - rubyCodeByteOffset,
			)

		if startPositionError != nil {
			continue
		}

		endPosition, endPositionError :=
			rubyDocument.PositionAt(
				engineDiagnostic.endByteOffset - rubyCodeByteOffset,
			)

		if endPositionError != nil {
			continue
		}

		diagnostics = append(
			diagnostics,
			lsp.Diagnostic{
				Range: lsp.Range{
					Start: startPosition,
					End:   endPosition,
				},
				Severity: &diagnosticSeverity,
				Source:   "picoruby-ti",
				Message:  engineDiagnostic.message,
			},
		)
	}

	documentVersion := rubyDocument.Version()

	return languageServer.client.PublishDiagnostics(
		requestContext,
		&lsp.PublishDiagnosticsParams{
			URI:         documentURI,
			Version:     &documentVersion,
			Diagnostics: diagnostics,
		},
	)
}

func (languageServer *languageServerHandler) getRubyDocument(
	documentURI lsp.DocumentURI,
) (*document.Document, bool) {

	rubyDocument, documentExists :=
		languageServer.openedDocuments.Get(documentURI)

	if !documentExists {
		return nil, false
	}

	if rubyDocument.LanguageID() != "ruby" {
		return nil, false
	}

	return rubyDocument, true
}

func (languageServer *languageServerHandler) Completion(
	_ context.Context,
	parameters *lsp.CompletionParams,
) (*lsp.CompletionList, error) {

	rubyDocument, rubyDocumentAvailable :=
		languageServer.getRubyDocument(parameters.TextDocument.URI)

	if !rubyDocumentAvailable {
		return nil, nil
	}

	rubyCodeCursorByteOffset, positionError :=
		rubyDocument.OffsetAt(parameters.Position)

	if positionError != nil {
		return nil, nil
	}

	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(
			parameters.TextDocument.URI,
			rubyDocument.Text(),
		)

	completionItems :=
		languageServer.picorubyTypeInference.makeCompletionItemsByPicoRuby(
			combinedRubyCode,
			rubyCodeByteOffset+rubyCodeCursorByteOffset,
		)

	return &lsp.CompletionList{
		Items: completionItems,
	}, nil
}

func (languageServer *languageServerHandler) Hover(
	_ context.Context,
	parameters *lsp.HoverParams,
) (*lsp.Hover, error) {

	rubyDocument, rubyDocumentAvailable :=
		languageServer.getRubyDocument(parameters.TextDocument.URI)

	if !rubyDocumentAvailable {
		return nil, nil
	}

	rubyCodeCursorByteOffset, positionError :=
		rubyDocument.OffsetAt(parameters.Position)

	if positionError != nil {
		return nil, nil
	}

	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(
			parameters.TextDocument.URI,
			rubyDocument.Text(),
		)

	hoverResult :=
		languageServer.picorubyTypeInference.makeHoverResultByPicoRuby(
			combinedRubyCode,
			rubyCodeByteOffset+rubyCodeCursorByteOffset,
		)

	return hoverResult, nil
}
