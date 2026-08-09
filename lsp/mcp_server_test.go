package main

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestClassesToolUsesLoaderManifest(test *testing.T) {
	temporaryDirectoryPath := test.TempDir()
	preloadFilePath := filepath.Join(temporaryDirectoryPath, "preload.rb")
	rubyFilePath := filepath.Join(temporaryDirectoryPath, "current.rb")
	loaderManifestPath := filepath.Join(
		temporaryDirectoryPath,
		tiLoaderFilename,
	)

	if writeError := os.WriteFile(
		preloadFilePath,
		[]byte("class PreloadedClass\nend"),
		0600,
	); writeError != nil {
		test.Fatalf("writing preload file failed: %v", writeError)
	}
	if writeError := os.WriteFile(rubyFilePath, []byte(""), 0600); writeError != nil {
		test.Fatalf("writing Ruby file failed: %v", writeError)
	}
	if writeError := os.WriteFile(
		loaderManifestPath,
		[]byte("preload.rb\n"),
		0600,
	); writeError != nil {
		test.Fatalf("writing loader manifest failed: %v", writeError)
	}

	toolHandler := &mcpToolHandler{typeInference: &picorubyTypeInference{}}
	_, output, toolError := toolHandler.classesTool(
		context.Background(),
		nil,
		fileInput{FilePath: rubyFilePath},
	)
	if toolError != nil {
		test.Fatalf("classes tool returned an error: %v", toolError)
	}

	for _, class := range output.Classes {
		if class.Name == "PreloadedClass" {
			return
		}
	}

	test.Fatal("PreloadedClass is unavailable")
}

func TestMethodsToolRejectsUnavailableClass(test *testing.T) {
	temporaryDirectoryPath := test.TempDir()
	rubyFilePath := filepath.Join(temporaryDirectoryPath, "current.rb")

	if writeError := os.WriteFile(rubyFilePath, []byte(""), 0600); writeError != nil {
		test.Fatalf("writing Ruby file failed: %v", writeError)
	}

	toolHandler := &mcpToolHandler{typeInference: &picorubyTypeInference{}}
	_, _, toolError := toolHandler.methodsTool(
		context.Background(),
		nil,
		methodsInput{
			FilePath:  rubyFilePath,
			ClassName: "UnavailableClass",
		},
	)

	if toolError == nil {
		test.Fatal("methods tool did not return an error")
	}
}
