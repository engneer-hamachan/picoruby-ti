package main

import "testing"

func TestMakeClassesByPicoRubyIncludesBuiltinAndDefinedClasses(test *testing.T) {
	typeInference := &picorubyTypeInference{}
	classes := typeInference.makeClassesByPicoRuby("class UserClass\nend")
	foundObject := false
	foundUserClass := false

	for _, class := range classes {
		if class.Name == "Object" {
			foundObject = true
		}
		if class.Name == "UserClass" {
			foundUserClass = true
		}
	}

	if !foundObject {
		test.Fatal("Object is unavailable")
	}
	if !foundUserClass {
		test.Fatal("UserClass is unavailable")
	}
}

func TestMakeMethodsByPicoRubyReturnsInstanceAndStaticMethods(test *testing.T) {
	typeInference := &picorubyTypeInference{}
	methods, classFound := typeInference.makeMethodsByPicoRuby("", "String")

	if !classFound {
		test.Fatal("String is unavailable")
	}

	foundInstanceMethod := false
	foundStaticMethod := false

	for _, method := range methods {
		if method.Static {
			foundStaticMethod = true
		} else {
			foundInstanceMethod = true
		}
	}

	if !foundInstanceMethod {
		test.Fatal("String instance methods are unavailable")
	}
	if !foundStaticMethod {
		test.Fatal("String static methods are unavailable")
	}
}

func TestMakeMethodsByPicoRubyReturnsDefinedMethodSignature(test *testing.T) {
	typeInference := &picorubyTypeInference{}
	rubyCode := "class UserClass\n  def answer(value) = 1\nend"
	methods, classFound :=
		typeInference.makeMethodsByPicoRuby(rubyCode, "UserClass")

	if !classFound {
		test.Fatal("UserClass is unavailable")
	}

	for _, method := range methods {
		if method.Name == "answer" {
			if method.Signature != "answer(value) -> Integer" {
				test.Fatalf(
					"answer signature = %q, want %q",
					method.Signature,
					"answer(value) -> Integer",
				)
			}
			if method.Static {
				test.Fatal("answer is marked as static")
			}

			return
		}
	}

	test.Fatal("answer is unavailable")
}

func TestMakeMethodsByPicoRubyReportsUnavailableClass(test *testing.T) {
	typeInference := &picorubyTypeInference{}
	methods, classFound :=
		typeInference.makeMethodsByPicoRuby("", "UnavailableClass")

	if classFound {
		test.Fatal("UnavailableClass is available")
	}
	if len(methods) != 0 {
		test.Fatalf("method count = %d, want 0", len(methods))
	}
}
