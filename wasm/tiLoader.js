// @bjorn3/browser_wasi_shim v0.4.2
// Licensed under MIT OR Apache-2.0; this project uses it under the MIT License.
// Source: https://github.com/bjorn3/browser_wasi_shim
import { WASI } from "https://cdn.jsdelivr.net/npm/@bjorn3/browser_wasi_shim@0.4.2/dist/index.js";

const wasm = await WebAssembly.compileStreaming(
  fetch(new URL("./picoruby_ti.wasm", import.meta.url))
);

const wasi = new WASI([], [], []);
const instance = await WebAssembly.instantiate(wasm, {
  wasi_snapshot_preview1: wasi.wasiImport,
});

wasi.initialize(instance);

const ti = instance.exports;
const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

function readString(pointer) {
  if (pointer === 0) return null;

  const bytes = new Uint8Array(ti.memory.buffer);
  let end = pointer;

  while (bytes[end] !== 0) end++;

  return textDecoder.decode(bytes.subarray(pointer, end));
}

function toByteOffset(source, utf16Offset) {
  return textEncoder.encode(source.slice(0, utf16Offset)).length;
}

function toUtf16Offset(sourceBytes, byteOffset) {
  return textDecoder.decode(sourceBytes.subarray(0, byteOffset)).length;
}

// Strings returned by picoruby-ti live in its arena until the next call,
// so every result is copied into JS values before withSource returns.
function withSource(source, callback) {
  const sourceBytes = textEncoder.encode(source);
  const sourcePointer = ti.malloc(Math.max(sourceBytes.length, 1));

  new Uint8Array(
    ti.memory.buffer,
    sourcePointer,
    sourceBytes.length
  ).set(sourceBytes);

  try {
    return callback(sourcePointer, sourceBytes);
  } finally {
    ti.free(sourcePointer);
  }
}

function suggest(source, cursor) {
  return withSource(source, (sourcePointer, sourceBytes) => {
    const count = ti.ti_fill_suggestions_from_source(
      sourcePointer,
      sourceBytes.length,
      toByteOffset(source, cursor)
    );

    const suggestions = [];

    for (let index = 0; index < count; index++) {
      suggestions.push({
        contents: readString(ti.ti_get_suggestion_contents(index)),
        detail: readString(ti.ti_get_suggestion_detail(index)),
        document: readString(ti.ti_get_suggestion_document(index)),
        className: readString(ti.ti_get_suggestion_class_name(index)),
      });
    }

    return suggestions;
  });
}

function hover(source, cursor) {
  return withSource(source, (sourcePointer, sourceBytes) => {
    const found = ti.ti_find_hover_from_source(
      sourcePointer,
      sourceBytes.length,
      toByteOffset(source, cursor)
    );

    if (!found) return null;

    return {
      variableName: readString(ti.ti_get_hover_variable_name()),
      typeName: readString(ti.ti_get_hover_type_name()),
      methodSignature: readString(ti.ti_get_hover_method_signature()),
      methodDocument: readString(ti.ti_get_hover_method_document()),
      isMethod: ti.ti_get_hover_is_method() !== 0,
    };
  });
}

function diagnose(source) {
  return withSource(source, (sourcePointer, sourceBytes) => {
    const count = ti.ti_fill_diagnostics_from_source(
      sourcePointer,
      sourceBytes.length
    );

    const diagnostics = [];

    for (let index = 0; index < count; index++) {
      diagnostics.push({
        startOffset: toUtf16Offset(
          sourceBytes,
          ti.ti_get_diagnostic_start_byte_offset(index)
        ),
        endOffset: toUtf16Offset(
          sourceBytes,
          ti.ti_get_diagnostic_end_byte_offset(index)
        ),
        message: readString(ti.ti_get_diagnostic_message(index)),
      });
    }

    return diagnostics;
  });
}

window.tiSuggest = suggest;
window.tiHover = hover;
window.tiDiagnostics = diagnose;
window.tiReady = true;
