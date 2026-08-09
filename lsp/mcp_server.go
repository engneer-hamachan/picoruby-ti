package main

import (
	"context"
	"fmt"
	"net/url"
	"os"
	"path/filepath"

	"github.com/modelcontextprotocol/go-sdk/mcp"
	"github.com/owenrumney/go-lsp/document"
	"github.com/owenrumney/go-lsp/lsp"
)

type fileInput struct {
	FilePath string `json:"file_path" jsonschema:"absolute or relative path to a Ruby file"`
}

type positionInput struct {
	FilePath  string `json:"file_path" jsonschema:"absolute or relative path to a Ruby file"`
	Line      int    `json:"line" jsonschema:"zero-based LSP line position"`
	Character int    `json:"character" jsonschema:"zero-based LSP UTF-16 character position"`
}

type methodsInput struct {
	FilePath  string `json:"file_path" jsonschema:"absolute or relative path to a Ruby file"`
	ClassName string `json:"class_name" jsonschema:"class name whose methods are requested"`
}

type diagnosticOutput struct {
	Diagnostics []lsp.Diagnostic `json:"diagnostics"`
}

type completionOutput struct {
	CompletionItems []lsp.CompletionItem `json:"completion_items"`
}

type hoverOutput struct {
	Hover *lsp.Hover `json:"hover"`
}

type classesOutput struct {
	Classes []picorubyClass `json:"classes"`
}

type methodsOutput struct {
	Methods []picorubyMethod `json:"methods"`
}

type mcpToolHandler struct {
	typeInference *picorubyTypeInference
}

func runMCPServer(requestContext context.Context) error {
	mcpServer :=
		mcp.NewServer(
			&mcp.Implementation{Name: "picoruby-ti", Version: "v1.0.0"},
			nil,
		)

	toolHandler := &mcpToolHandler{typeInference: &picorubyTypeInference{}}
	toolHandler.registerTools(mcpServer)

	return mcpServer.Run(requestContext, &mcp.StdioTransport{})
}

func (toolHandler *mcpToolHandler) registerTools(mcpServer *mcp.Server) {
	mcp.AddTool(
		mcpServer,
		&mcp.Tool{
			Name:        "diagnostic",
			Description: "Return picoruby-ti diagnostics for a Ruby file.",
		},
		toolHandler.diagnosticTool,
	)
	mcp.AddTool(
		mcpServer,
		&mcp.Tool{
			Name:        "completion",
			Description: "Return picoruby-ti completion items at an LSP position in a Ruby file.",
		},
		toolHandler.completionTool,
	)
	mcp.AddTool(
		mcpServer,
		&mcp.Tool{
			Name:        "hover",
			Description: "Return picoruby-ti hover information at an LSP position in a Ruby file.",
		},
		toolHandler.hoverTool,
	)
	mcp.AddTool(
		mcpServer,
		&mcp.Tool{
			Name:        "classes",
			Description: "Return classes available to a Ruby file through picoruby-ti.",
		},
		toolHandler.classesTool,
	)
	mcp.AddTool(
		mcpServer,
		&mcp.Tool{
			Name:        "methods",
			Description: "Return instance and static methods for a picoruby-ti class.",
		},
		toolHandler.methodsTool,
	)
}

func (toolHandler *mcpToolHandler) diagnosticTool(
	_ context.Context,
	_ *mcp.CallToolRequest,
	input fileInput,
) (*mcp.CallToolResult, diagnosticOutput, error) {

	rubyDocument, readError := readRubyDocument(input.FilePath)
	if readError != nil {
		return nil, diagnosticOutput{}, readError
	}

	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(
			rubyDocument.URI(),
			rubyDocument.Text(),
		)

	rubyCodeEndByteOffset := rubyCodeByteOffset + len(rubyDocument.Text())

	engineDiagnostics :=
		toolHandler.typeInference.makeDiagnosticsByPicoRuby(combinedRubyCode)

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

		diagnostics = append(diagnostics, lsp.Diagnostic{
			Range:    lsp.Range{Start: startPosition, End: endPosition},
			Severity: &diagnosticSeverity,
			Source:   "picoruby-ti",
			Message:  engineDiagnostic.message,
		})
	}

	return nil, diagnosticOutput{Diagnostics: diagnostics}, nil
}

func (toolHandler *mcpToolHandler) completionTool(
	_ context.Context,
	_ *mcp.CallToolRequest,
	input positionInput,
) (*mcp.CallToolResult, completionOutput, error) {

	rubyDocument, readError := readRubyDocument(input.FilePath)
	if readError != nil {
		return nil, completionOutput{}, readError
	}

	rubyCodeCursorByteOffset, positionError :=
		rubyDocument.OffsetAt(
			lsp.Position{Line: input.Line, Character: input.Character},
		)

	if positionError != nil {
		return nil, completionOutput{}, positionError
	}

	combinedRubyCode, rubyCodeByteOffset :=
		prependTiPreloadSources(
			rubyDocument.URI(),
			rubyDocument.Text(),
		)

	completionItems :=
		toolHandler.typeInference.makeCompletionItemsByPicoRuby(
			combinedRubyCode,
			rubyCodeByteOffset+rubyCodeCursorByteOffset,
		)

	return nil, completionOutput{CompletionItems: completionItems}, nil
}

func (toolHandler *mcpToolHandler) hoverTool(
	_ context.Context,
	_ *mcp.CallToolRequest,
	input positionInput,
) (*mcp.CallToolResult, hoverOutput, error) {

	rubyDocument, readError := readRubyDocument(input.FilePath)
	if readError != nil {
		return nil, hoverOutput{}, readError
	}

	rubyCodeCursorByteOffset, positionError := rubyDocument.OffsetAt(
		lsp.Position{Line: input.Line, Character: input.Character},
	)

	if positionError != nil {
		return nil, hoverOutput{}, positionError
	}

	combinedRubyCode, rubyCodeByteOffset := prependTiPreloadSources(
		rubyDocument.URI(),
		rubyDocument.Text(),
	)

	hoverResult := toolHandler.typeInference.makeHoverResultByPicoRuby(
		combinedRubyCode,
		rubyCodeByteOffset+rubyCodeCursorByteOffset,
	)

	return nil, hoverOutput{Hover: hoverResult}, nil
}

func (toolHandler *mcpToolHandler) classesTool(
	_ context.Context,
	_ *mcp.CallToolRequest,
	input fileInput,
) (*mcp.CallToolResult, classesOutput, error) {

	rubyDocument, readError := readRubyDocument(input.FilePath)
	if readError != nil {
		return nil, classesOutput{}, readError
	}

	combinedRubyCode, _ :=
		prependTiPreloadSources(
			rubyDocument.URI(),
			rubyDocument.Text(),
		)

	classes := toolHandler.typeInference.makeClassesByPicoRuby(combinedRubyCode)

	return nil, classesOutput{Classes: classes}, nil
}

func (toolHandler *mcpToolHandler) methodsTool(
	_ context.Context,
	_ *mcp.CallToolRequest,
	input methodsInput,
) (*mcp.CallToolResult, methodsOutput, error) {

	rubyDocument, readError := readRubyDocument(input.FilePath)
	if readError != nil {
		return nil, methodsOutput{}, readError
	}

	combinedRubyCode, _ :=
		prependTiPreloadSources(
			rubyDocument.URI(),
			rubyDocument.Text(),
		)

	methods, classFound :=
		toolHandler.typeInference.makeMethodsByPicoRuby(
			combinedRubyCode,
			input.ClassName,
		)

	if !classFound {
		return nil, methodsOutput{}, fmt.Errorf(
			"class %q is unavailable",
			input.ClassName,
		)
	}

	return nil, methodsOutput{Methods: methods}, nil
}

func readRubyDocument(filePath string) (*document.Document, error) {
	absoluteFilePath, absolutePathError := filepath.Abs(filePath)
	if absolutePathError != nil {
		return nil, absolutePathError
	}

	rubyCodeBytes, readError := os.ReadFile(absoluteFilePath)
	if readError != nil {
		return nil, readError
	}

	documentURI :=
		lsp.DocumentURI(
			(&url.URL{Scheme: "file", Path: absoluteFilePath}).String(),
		)

	documentStore := document.NewStore()

	rubyDocument, openError :=
		documentStore.Open(
			&lsp.DidOpenTextDocumentParams{
				TextDocument: lsp.TextDocumentItem{
					URI:        documentURI,
					LanguageID: "ruby",
					Version:    1,
					Text:       string(rubyCodeBytes),
				},
			},
		)

	if openError != nil {
		return nil, openError
	}

	return rubyDocument, nil
}
