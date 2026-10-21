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
#include <cstdlib>
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
// 不具合報告E-06(高、D4の抜け道): SIGHUP (closing the terminal/an SSH
// disconnect -- plausibly the single most likely real-world way this
// process actually gets killed) and SIGABRT (abort()/ASan and similar
// detected-corruption aborts) are handled the same way below. SIGABRT is
// best-effort only, not guaranteed: glibc's abort() restores SIGABRT's
// default disposition and re-raises it immediately once a caught instance's
// handler returns, specifically so the process is guaranteed to actually
// terminate even with a handler installed -- there may simply not be time
// left for the event loop this handler defers to ever get a turn before
// that second, uncatchable raise arrives. SIGKILL cannot be caught by any
// process, by definition (not handled here at all; a stuck-input scenario
// from one requires an external watcher process, outside this one's own
// control, to notice and clean up after it -- SPEC.md 8章の既知の制約参照).
//
// A signal handler itself cannot safely call into Qt/X11/AT-SPI (none of
// those are async-signal-safe) -- the standard "self-pipe" trick relays the
// signal onto the normal Qt event loop instead: the handler only writes one
// byte to a socket pair (write() is async-signal-safe), and a QSocketNotifier
// watching the other end runs the actual cleanup from ordinary event-loop
// context once the loop next turns.
int g_terminationSocketPair[2] = {-1, -1};
// Which signal actually triggered the most recent write to the pipe above --
// only ever read back from the (single-threaded) Qt event loop that the
// notifier below runs on, so a plain global is fine despite being written
// from a signal handler (sig_atomic_t's whole purpose is making exactly this
// safe without needing a lock a signal handler couldn't take anyway).
volatile sig_atomic_t g_terminationSignal = 0;

void handleTerminationSignal(int signum)
{
    g_terminationSignal = signum;
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
    sigaction(SIGHUP, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);

    // 不具合報告E-09(高): previously disabled this notifier for good the
    // very first time it fired, before calling QCoreApplication::quit() --
    // which, for a visible top-level window, triggers that window's own
    // closeEvent() (see MainWindow::closeEvent()'s own updated comment).
    // An unattended run with unsaved changes then hung forever on a
    // confirmation dialog nobody was there to answer, with every
    // subsequent SIGTERM/SIGINT silently ignored (this notifier was already
    // disabled, so their own writes to the pipe were never even read). That
    // specific hang no longer happens (closeEvent() now recognizes this
    // exact situation and skips the confirmation on its own), but this
    // notifier is also deliberately left enabled from here on regardless,
    // as a second line of defense: if anything else ever left the shutdown
    // stuck for some other reason, a second signal still forces an
    // immediate, unconditional exit rather than silently doing nothing.
    auto *notifier = new QSocketNotifier(g_terminationSocketPair[1], QSocketNotifier::Read, window);
    auto *attemptCount = new int(0);  // owned by the lambda capture below, freed with it
    QObject::connect(notifier, &QSocketNotifier::activated, window,
                      [window, attemptCount]() {
                          char byte;
                          (void)!::read(g_terminationSocketPair[1], &byte, sizeof(byte));
                          const int signum = g_terminationSignal;
                          window->releaseHeldInputForTermination(signum);
                          ++*attemptCount;
                          if (*attemptCount >= 2) {
                              // Already tried the ordinary way once and
                              // whatever that triggered hasn't actually
                              // exited yet -- stop waiting on Qt's own
                              // shutdown path (which could in principle
                              // still be stuck on something this process
                              // doesn't know about) and force it immediately.
                              // _Exit() skips atexit()/static-destructor
                              // cleanup entirely, which is fine: the one
                              // thing that genuinely needed to happen
                              // (releasing physically-held input) was just
                              // done above, synchronously, by
                              // releaseHeldInputForTermination().
                              std::_Exit(1);
                          }
                          QCoreApplication::exit(1);
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
