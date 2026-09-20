#pragma once

#include <QString>

// Minimal runtime-selectable UI language support (Japanese / English).
//
// Design: every user-facing string in this codebase is written as Japanese
// QStringLiteral text at its call site (that's the app's original/native
// language). Rather than introduce a parallel set of opaque message-id
// constants, I18n::t() takes that same Japanese text as its lookup key into
// a translation table (see I18n.cpp) and returns either it unchanged
// (Language::Japanese) or its English translation (Language::English, falls
// back to the Japanese text itself if a key is somehow missing from the
// table rather than showing nothing).
//
// Language selection is persisted via QSettings (organization "asobi",
// application "EnduranceTestGUI" -- see main.cpp) and takes effect on next
// launch rather than live: most static UI text here (window title, button
// labels, group box titles, column headers, ...) is set once when each
// widget is constructed, not re-applied on every event loop iteration the
// way dynamically generated log/status messages are, so a full live
// retranslate would need per-widget bookkeeping this app doesn't otherwise
// have any use for. main() calls I18n::currentLanguage() (which lazily
// loads the saved preference on first use) before constructing MainWindow,
// so the whole UI is built in the selected language from the start.
namespace I18n
{
enum class Language
{
    Japanese,
    English,
};

// The language currently in effect. Lazily loads the persisted preference
// (QSettings key "language", defaulting to Japanese) on first call.
Language currentLanguage();

// Updates the in-memory current language AND persists it to QSettings.
// Does not retranslate already-constructed widgets (see file comment above)
// -- callers should tell the user the change takes effect after restart.
void setLanguage(Language lang);

// Translates `japaneseText` (used verbatim as the lookup key) according to
// currentLanguage(). Safe to call with any Japanese QStringLiteral used
// throughout the UI; strings with no user-facing meaning (JSON property
// names, file-path fragments meant to stay stable across languages, etc.)
// should NOT be passed through this and are left as plain QStringLiteral.
QString t(const QString &japaneseText);

}  // namespace I18n
