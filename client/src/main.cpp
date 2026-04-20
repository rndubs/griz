#include "App.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("LLNL"));
    QCoreApplication::setApplicationName(QStringLiteral("griz-client"));
#ifdef GRIZ_CLIENT_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(GRIZ_CLIENT_VERSION));
#endif

    griz::App grizApp;
    grizApp.parseArgs(QCoreApplication::arguments());
    grizApp.show();

    return app.exec();
}
