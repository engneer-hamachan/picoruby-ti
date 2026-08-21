package main

import (
	"context"
	"net/url"
	"os"
	"path/filepath"
	"reflect"
	"testing"

	"github.com/owenrumney/go-lsp/document"
	"github.com/owenrumney/go-lsp/lsp"
)

func TestInitializeReturnsConfiguredCapabilities(test *testing.T) {
	serverHandler := &languageServerHandler{}

	initializeResult, initializeError :=
		serverHandler.Initialize(
			context.Background(),
			&lsp.InitializeParams{},
		)

	if initializeError != nil {
		test.Fatalf("Initialize returned an error: %v", initializeError)
	}

	expectedTriggerCharacters := []string{
		"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
		"l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
		"w", "x", "y", "z",
		"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
		"L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
		"W", "X", "Y", "Z",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		".", "_",
	}

	if initializeResult.Capabilities.CompletionProvider == nil {
		test.Fatal("completion provider is not configured")
	}

	if !reflect.DeepEqual(
		initializeResult.Capabilities.CompletionProvider.TriggerCharacters,
		expectedTriggerCharacters,
	) {
		test.Fatal("completion trigger characters do not match")
	}

	if initializeResult.Capabilities.HoverProvider == nil ||
		!*initializeResult.Capabilities.HoverProvider {
		test.Fatal("hover provider is not configured")
	}

	if initializeResult.Capabilities.TextDocumentSync == nil {
		test.Fatal("text document sync is not configured")
	}

	if initializeResult.Capabilities.TextDocumentSync.OpenClose == nil ||
		!*initializeResult.Capabilities.TextDocumentSync.OpenClose {
		test.Fatal("text document open and close sync is not configured")
	}

	if initializeResult.Capabilities.TextDocumentSync.Change !=
		lsp.SyncIncremental {
		test.Fatal("incremental text document sync is not configured")
	}
}

func TestGetRubyDocument(test *testing.T) {
	serverHandler := &languageServerHandler{
		openedDocuments: document.NewStore(),
	}

	rubyDocumentParameters := &lsp.DidOpenTextDocumentParams{
		TextDocument: lsp.TextDocumentItem{
			URI:        "file:///ruby-document.rb",
			LanguageID: "ruby",
			Version:    1,
			Text:       "value = 1",
		},
	}

	_, openError := serverHandler.openedDocuments.Open(rubyDocumentParameters)
	if openError != nil {
		test.Fatalf("opening the Ruby document failed: %v", openError)
	}

	rubyDocument, rubyDocumentAvailable :=
		serverHandler.getRubyDocument(rubyDocumentParameters.TextDocument.URI)

	if !rubyDocumentAvailable {
		test.Fatal("the Ruby document is unavailable")
	}

	textDocumentParameters := &lsp.DidOpenTextDocumentParams{
		TextDocument: lsp.TextDocumentItem{
			URI:        "file:///text-document.txt",
			LanguageID: "plaintext",
			Version:    1,
			Text:       "value = 1",
		},
	}

	_, openError = serverHandler.openedDocuments.Open(textDocumentParameters)
	if openError != nil {
		test.Fatalf("opening the text document failed: %v", openError)
	}

	rubyDocument, rubyDocumentAvailable =
		serverHandler.getRubyDocument(textDocumentParameters.TextDocument.URI)

	if rubyDocumentAvailable {
		test.Fatal("the text document was returned as a Ruby document")
	}

	if rubyDocument != nil {
		test.Fatal("a document was returned for the text document")
	}
}

func TestMakeDiagnosticsByPicoRuby(test *testing.T) {
	typeInference := &picorubyTypeInference{}

	diagnostics :=
		typeInference.makeDiagnosticsByPicoRuby(
			"\"x\".tr(1, \"a\")",
		)

	if len(diagnostics) != 1 {
		test.Fatalf("diagnostic count = %d, want 1", len(diagnostics))
	}

	diagnostic := diagnostics[0]

	if diagnostic.startByteOffset != 7 || diagnostic.endByteOffset != 8 {
		test.Fatalf(
			"diagnostic offsets = %d:%d, want 7:8",
			diagnostic.startByteOffset,
			diagnostic.endByteOffset,
		)
	}

	expectedMessage :=
		"type mismatch: expected String, but got Integer for String.tr"

	if diagnostic.message != expectedMessage {
		test.Fatalf(
			"diagnostic message = %q, want %q",
			diagnostic.message,
			expectedMessage,
		)
	}
}

func TestMakeHoverResultByPicoRubyEscapesUnionTypeForMarkdown(test *testing.T) {
	typeInference := &picorubyTypeInference{}

	hoverResult :=
		typeInference.makeHoverResultByPicoRuby(
			"value = true ? 1 : \"x\"\nvalue",
			len("value = true ? 1 : \"x\"\n"),
		)

	if hoverResult == nil {
		test.Fatal("hover result is unavailable")
	}

	expectedHoverContent := "value: Union&lt;Integer String&gt;"

	if hoverResult.Contents.Value != expectedHoverContent {
		test.Fatalf(
			"hover content = %q, want %q",
			hoverResult.Contents.Value,
			expectedHoverContent,
		)
	}
}

func TestPrependTiPreloadSources(test *testing.T) {
	temporaryDirectoryPath := test.TempDir()
	rubyDocumentPath := filepath.Join(temporaryDirectoryPath, "current.rb")
	loaderManifestPath :=
		filepath.Join(temporaryDirectoryPath, tiLoaderFilename)
	preloadSourcePath := filepath.Join(temporaryDirectoryPath, "preload.rb")

	writeError := os.WriteFile(preloadSourcePath, []byte("value = 1"), 0600)
	if writeError != nil {
		test.Fatalf("writing the preload source failed: %v", writeError)
	}

	writeError = os.WriteFile(
		loaderManifestPath,
		[]byte("preload.rb\ncurrent.rb\r\nmissing.rb\n"),
		0600,
	)
	if writeError != nil {
		test.Fatalf("writing the loader manifest failed: %v", writeError)
	}

	documentURI := lsp.DocumentURI(
		(&url.URL{Scheme: "file", Path: rubyDocumentPath}).String(),
	)
	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(documentURI, "value")
	expectedCombinedRubyCode := "value = 1\nvalue"

	if combinedRubyCode != expectedCombinedRubyCode {
		test.Fatalf(
			"combined Ruby code = %q, want %q",
			combinedRubyCode,
			expectedCombinedRubyCode,
		)
	}

	if rubyCodeByteOffset != len("value = 1\n") {
		test.Fatalf(
			"Ruby code byte offset = %d, want %d",
			rubyCodeByteOffset,
			len("value = 1\n"),
		)
	}
}

func TestPrependTiPreloadSourcesWithoutManifest(test *testing.T) {
	temporaryDirectoryPath := test.TempDir()
	rubyDocumentPath := filepath.Join(temporaryDirectoryPath, "current.rb")
	documentURI := lsp.DocumentURI(
		(&url.URL{Scheme: "file", Path: rubyDocumentPath}).String(),
	)
	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(documentURI, "value = 1")
	expectedCombinedRubyCode := "value = 1"

	if combinedRubyCode != expectedCombinedRubyCode {
		test.Fatalf(
			"combined Ruby code = %q, want %q",
			combinedRubyCode,
			expectedCombinedRubyCode,
		)
	}

	if rubyCodeByteOffset != 0 {
		test.Fatalf(
			"Ruby code byte offset = %d, want 0",
			rubyCodeByteOffset,
		)
	}
}

func TestMakeDiagnosticsByPicoRubyWithPreloadSources(test *testing.T) {
	temporaryDirectoryPath := test.TempDir()
	rubyDocumentPath := filepath.Join(temporaryDirectoryPath, "current.rb")
	loaderManifestPath :=
		filepath.Join(temporaryDirectoryPath, tiLoaderFilename)
	preloadSourcePath := filepath.Join(temporaryDirectoryPath, "preload.rb")

	writeError := os.WriteFile(
		preloadSourcePath,
		[]byte("value = \"x\""),
		0600,
	)
	if writeError != nil {
		test.Fatalf("writing the preload source failed: %v", writeError)
	}

	writeError = os.WriteFile(
		loaderManifestPath,
		[]byte("preload.rb\n"),
		0600,
	)
	if writeError != nil {
		test.Fatalf("writing the loader manifest failed: %v", writeError)
	}

	documentURI := lsp.DocumentURI(
		(&url.URL{Scheme: "file", Path: rubyDocumentPath}).String(),
	)
	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(documentURI, "value.tr(1, \"a\")")
	diagnostics := (&picorubyTypeInference{}).makeDiagnosticsByPicoRuby(
		combinedRubyCode,
	)

	if len(diagnostics) != 1 {
		test.Fatalf("diagnostic count = %d, want 1", len(diagnostics))
	}

	if diagnostics[0].startByteOffset-rubyCodeByteOffset != 9 ||
		diagnostics[0].endByteOffset-rubyCodeByteOffset != 10 {
		test.Fatalf(
			"diagnostic offsets = %d:%d, want 9:10",
			diagnostics[0].startByteOffset-rubyCodeByteOffset,
			diagnostics[0].endByteOffset-rubyCodeByteOffset,
		)
	}
}
