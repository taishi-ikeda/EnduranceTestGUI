#include <QApplication>
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
    window.show();

    return app.exec();
}
