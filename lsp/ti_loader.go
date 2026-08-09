package main

import (
	"net/url"
	"os"
	"path/filepath"
	"strings"

	"github.com/owenrumney/go-lsp/lsp"
)

const tiLoaderFilename = ".ti-loader.manifest"

func prependTiPreloadSources(
	documentURI lsp.DocumentURI,
	rubyCode string,
) (combinedRubyCode string, rubyCodeByteOffset int) {

	documentURL, parseError := url.Parse(string(documentURI))
	if parseError != nil || documentURL.Scheme != "file" {
		return rubyCode, 0
	}

	documentFilePath := documentURL.Path
	loaderDirectoryPath := filepath.Dir(documentFilePath)
	loaderPath := filepath.Join(loaderDirectoryPath, tiLoaderFilename)
	loaderManifestBytes, readError := os.ReadFile(loaderPath)

	if readError != nil {
		return rubyCode, 0
	}

	var combinedRubyCodeBuilder strings.Builder

	for _, preloadPath := range strings.Split(string(loaderManifestBytes), "\n") {
		preloadPath = strings.TrimSuffix(preloadPath, "\r")

		if preloadPath == "" {
			continue
		}

		resolvedPreloadPath := filepath.Join(loaderDirectoryPath, preloadPath)

		if resolvedPreloadPath == documentFilePath {
			continue
		}

		preloadSourceBytes, readError := os.ReadFile(resolvedPreloadPath)
		if readError != nil {
			continue
		}

		combinedRubyCodeBuilder.Write(preloadSourceBytes)
		combinedRubyCodeBuilder.WriteByte('\n')
	}

	rubyCodeByteOffset = combinedRubyCodeBuilder.Len()
	combinedRubyCodeBuilder.WriteString(rubyCode)

	return combinedRubyCodeBuilder.String(), rubyCodeByteOffset
}
