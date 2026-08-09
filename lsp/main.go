package main

import (
	"context"
	"log"
	"os"

	"github.com/owenrumney/go-lsp/document"
	"github.com/owenrumney/go-lsp/server"
)

func main() {
	if len(os.Args) == 2 && os.Args[1] == "--mcp" {
		if runError := runMCPServer(context.Background()); runError != nil {
			log.Fatal(runError)
		}

		return
	}

	if runError := runLanguageServer(context.Background()); runError != nil {
		log.Fatal(runError)
	}
}

func runLanguageServer(requestContext context.Context) error {
	serverHandler := &languageServerHandler{
		openedDocuments:       document.NewStore(),
		picorubyTypeInference: &picorubyTypeInference{},
	}

	languageServer := server.NewServer(serverHandler)

	return languageServer.Run(requestContext, server.RunStdio())
}
