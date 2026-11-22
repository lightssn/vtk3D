#include "ui/MainWindow.h"
#include "../tests/UiChecks.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

int main(int argc, char *argv[]) {
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("vtk3D"));
    QCoreApplication::setApplicationName(QStringLiteral("vtk3D"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    const bool stress = app.arguments().contains("--stress-test");
    const bool closeTest = app.arguments().contains("--close-test");
    const bool smoke = app.arguments().contains("--smoke-test") || stress || closeTest ||
                       app.arguments().contains("--cancel-test");
    app.setProperty("smokeTest", smoke);
    MainWindow window;
    window.show();
    if (smoke) startUiChecks(app, window);
    return app.exec();
}
