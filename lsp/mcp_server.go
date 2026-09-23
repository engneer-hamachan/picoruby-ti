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
	mcp.AddTool(
		mcpServer,
		&mcp.Tool{
			Name:        "methods_without_object",
			Description: "Return instance and static methods for a picoruby-ti class, excluding methods inherited from Object.",
		},
		toolHandler.methodsWithoutObjectTool,
	)
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

	return toolHandler.makeMethodsToolResult(input, false)
}

func (toolHandler *mcpToolHandler) methodsWithoutObjectTool(
	_ context.Context,
	_ *mcp.CallToolRequest,
	input methodsInput,
) (*mcp.CallToolResult, methodsOutput, error) {

	return toolHandler.makeMethodsToolResult(input, true)
}

func (toolHandler *mcpToolHandler) makeMethodsToolResult(
	input methodsInput,
	excludeObjectMethods bool,
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
			excludeObjectMethods,
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
