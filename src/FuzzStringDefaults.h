#pragma once

#include <QStringList>

// SPEC.md 追加実装依頼「異常入力ファジング」: the built-in starting point for
// TestConfig::fuzzStringCorpus (MainWindow's app-wide, QSettings-backed
// master list -- see its own comment). Kept in its own file/function rather
// than inline in MainWindow.cpp purely because the literal corpus itself is
// long; nothing here depends on Qt widgets or the rest of the app.
namespace FuzzStringDefaults
{
// A broad, general-purpose starting corpus covering the input-handling bug
// categories a text field most commonly fails on: very long strings,
// control/NUL-adjacent characters, format-string specifiers, SQL/script/
// command-injection patterns, path traversal, numeric boundary values, and
// Unicode edge cases (bidi override, zero-width characters, combining mark
// pileups, non-BMP emoji/ZWJ sequences, full-width forms, locale-sensitive
// casing). Not exhaustive -- the user is expected to add targeted entries
// of their own via the "表示" menu's corpus editor -- but meant to give
// useful coverage out of the box without any setup.
QStringList builtInCorpus();
}
