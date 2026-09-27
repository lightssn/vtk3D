#include "MainWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

int main(int argc, char* argv[]) {
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("vtk3D"));
    QCoreApplication::setApplicationName(QStringLiteral("vtk3D"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    MainWindow window;
    window.show();
    return app.exec();
    }
