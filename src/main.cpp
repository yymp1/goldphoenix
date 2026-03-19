#include "AppTypes.h"
#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("超级无敌金凤凰1.0"));
    app.setOrganizationName(QStringLiteral("Codex"));

    qRegisterMetaType<VideoState>("VideoState");
    qRegisterMetaType<BurstResult>("BurstResult");

    MainWindow window;
    window.show();
    return app.exec();
}
