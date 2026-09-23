#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "I18n.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EnduranceTestGUI");
    app.setOrganizationName("asobi");

    // SPEC.md 10追加実装及び修正依頼「GUIを立ち上げなくてもターミナル実行で
    // 保存した手順を引数として与えることでテストを実行できるようにして
    // ください。その際連続実行の回数も引数として設定できるように」: CLI/
    // ヘッドレス実行モード. `--run <preset.json>`'s presence is what
    // switches into this mode; `--repeat` is optional (default 1). See
    // MainWindow::runHeadless() for what actually happens.
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("EnduranceTestGUI -- GUI endurance-test tool. With no options, opens the "
                        "normal GUI. With --run, executes a saved preset headlessly (no window "
                        "shown) instead."));
    const QCommandLineOption runOption(
        QStringList{QStringLiteral("run")},
        QStringLiteral("Run a test preset saved from \"テスト設定を保存...\" headlessly, the same "
                        "way \"⟳ 連続実行\" would, then exit. Requires the preset to have a "
                        "targetLaunchCommand set (so each repeat gets a fresh instance)."),
        QStringLiteral("preset.json"));
    const QCommandLineOption repeatOption(
        QStringList{QStringLiteral("repeat")},
        QStringLiteral("Number of consecutive runs (overrides the preset's own saved batch run "
                        "count). Default: 1."),
        QStringLiteral("count"), QStringLiteral("1"));
    parser.addOption(runOption);
    parser.addOption(repeatOption);
    parser.addHelpOption();
    parser.process(app);

    // Qt's own built-in UI strings -- QDialogButtonBox's standard OK/
    // Cancel/Close buttons, QInputDialog's OK/Cancel, QMessageBox's Yes/No,
    // QFileDialog's "Look in:"/"File name:"/"Files of type:", etc. -- are
    // not covered by this app's own I18n:: system (see I18n.h): they come
    // from Qt's own bundled translations instead, which Qt only applies if
    // the app installs the matching QTranslator itself. Do that here when
    // Japanese is selected, so those controls switch language along with
    // the rest of the UI instead of staying in English. `qtTranslator`
    // must outlive app.exec() (it stays installed the whole time), hence
    // declaring it in main()'s own scope rather than a narrower one.
    // Silently does nothing if qtbase_ja.qm isn't present on this system
    // (falls back to Qt's English originals, same as before this existed).
    QTranslator qtTranslator;
    if (I18n::currentLanguage() == I18n::Language::Japanese) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QString translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
        const QString translationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
        if (qtTranslator.load(QLocale(QLocale::Japanese), QStringLiteral("qtbase"), QStringLiteral("_"),
                               translationsPath))
            app.installTranslator(&qtTranslator);
    }

    MainWindow window;

    if (parser.isSet(runOption)) {
        int repeatCount = 1;
        bool repeatOk = false;
        const int parsedRepeat = parser.value(repeatOption).toInt(&repeatOk);
        if (repeatOk && parsedRepeat > 0)
            repeatCount = parsedRepeat;
        // window.show() is deliberately never called in this mode -- see
        // MainWindow::runHeadless()'s own comment.
        if (!window.runHeadless(parser.value(runOption), repeatCount))
            return 1;
        return app.exec();
    }

    window.show();
    return app.exec();
}
