#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QSocketNotifier>
#include <QTranslator>

#include "I18n.h"
#include "MainWindow.h"
#include "ScrollWheelFocusGuard.h"

#if defined(Q_OS_UNIX)
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

// 不具合報告D4(重大): ETG had no SIGTERM/SIGINT handler at all, so killing it
// that way (a CI job timeout, `timeout`, Ctrl+C in a terminal -- anything
// short of SIGKILL, which cannot be caught by definition) while a long-press
// drag held a mouse button/modifier key down left that button/key held at
// the X server indefinitely: XTest's synthesized press has no association
// with the client process that requested it, so the server does not release
// it just because that client disconnected. Every subsequent pointer move
// anywhere on the display then drags, and every keypress carries whatever
// modifier was stuck, until something else happens to press and release it
// again -- a real problem on a shared/CI machine where this is not the only
// thing using that X session.
//
// A signal handler itself cannot safely call into Qt/X11/AT-SPI (none of
// those are async-signal-safe) -- the standard "self-pipe" trick relays the
// signal onto the normal Qt event loop instead: the handler only writes one
// byte to a socket pair (write() is async-signal-safe), and a QSocketNotifier
// watching the other end runs the actual cleanup from ordinary event-loop
// context once the loop next turns.
int g_terminationSocketPair[2] = {-1, -1};

void handleTerminationSignal(int)
{
    const char byte = 1;
    // Best-effort: if this write somehow fails there is nothing safe left to
    // do from inside a signal handler, so the return value is deliberately
    // ignored here (checking it would need to call another non-async-signal-
    // safe function to report it).
    (void)!::write(g_terminationSocketPair[0], &byte, sizeof(byte));
}

void installTerminationSignalHandling(MainWindow *window)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_terminationSocketPair) != 0)
        return;

    struct sigaction action;
    action.sa_handler = handleTerminationSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);

    // Owned by `window` via QObject parent-child so it is torn down with
    // the application; must outlive this function, hence `new` rather than
    // a local/stack object.
    auto *notifier = new QSocketNotifier(g_terminationSocketPair[1], QSocketNotifier::Read, window);
    QObject::connect(notifier, &QSocketNotifier::activated, window, [window, notifier]() {
        char byte;
        (void)!::read(g_terminationSocketPair[1], &byte, sizeof(byte));
        notifier->setEnabled(false);
        window->releaseHeldInputForTermination();
        QCoreApplication::quit();
    });
}

}  // namespace
#endif

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
    // Bug (SPEC.md 10 不具合報告 D5): this help text used to say "overrides
    // the preset's own saved batch run count", but no such field is ever
    // saved into a preset JSON in the first place -- the GUI's "連続実行
    // 回数" spin box (m_batchRunCountSpin) is a live-session-only control,
    // never written by MainWindow::buildPresetJson()/read back by its
    // loading counterpart. --repeat is this mode's *only* way to set the
    // run count; there is nothing in the preset for it to override.
    const QCommandLineOption repeatOption(
        QStringList{QStringLiteral("repeat")},
        QStringLiteral("Number of consecutive runs for this headless session (presets do not save "
                        "a run count of their own). Default: 1."),
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

    // SPEC.md 不具合報告E7「ホイールの死角」: installed application-wide
    // (not just on MainWindow) so it also covers every dialog's own
    // spin boxes/combo boxes/lists. No-op in headless mode (`window` is
    // never shown(), so nothing ever receives a wheel event there), but
    // harmless to install regardless.
    ScrollWheelFocusGuard scrollWheelFocusGuard;
    app.installEventFilter(&scrollWheelFocusGuard);

    MainWindow window;

#if defined(Q_OS_UNIX)
    installTerminationSignalHandling(&window);
#endif

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
