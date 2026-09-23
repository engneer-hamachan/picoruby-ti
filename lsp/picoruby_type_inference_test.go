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
	methods, classFound := typeInference.makeMethodsByPicoRuby("", "String", false)

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
	rubyCode :=
		"class UserClass\n  def answer(value, *values, **options) = 1\nend"
	methods, classFound :=
		typeInference.makeMethodsByPicoRuby(rubyCode, "UserClass", false)

	if !classFound {
		test.Fatal("UserClass is unavailable")
	}

	for _, method := range methods {
		if method.Name == "answer" {
			if method.Signature !=
				"answer(value, *values, **options) -> Integer" {
				test.Fatalf(
					"answer signature = %q, want %q",
					method.Signature,
					"answer(value, *values, **options) -> Integer",
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
		typeInference.makeMethodsByPicoRuby("", "UnavailableClass", false)

	if classFound {
		test.Fatal("UnavailableClass is available")
	}
	if len(methods) != 0 {
		test.Fatalf("method count = %d, want 0", len(methods))
	}
}

func TestMakeMethodsByPicoRubyExcludesObjectMethods(test *testing.T) {
	typeInference := &picorubyTypeInference{}
	methods, classFound :=
		typeInference.makeMethodsByPicoRuby("", "Integer", true)

	if !classFound {
		test.Fatal("Integer is unavailable")
	}

	foundTimes := false

	for _, method := range methods {
		if method.Name == "block_given?" {
			test.Fatal("Object method block_given? is included")
		}
		if method.Name == "times" {
			foundTimes = true
		}
	}

	if !foundTimes {
		test.Fatal("Integer method times is unavailable")
	}
}
