#include "ui/FourViewer.h"
#include "ui/MainWindow.h"
#include "ui/SliceView.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>
#include <QWheelEvent>

int main(int argc, char *argv[]) {
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("vtk3D"));
    QCoreApplication::setApplicationName(QStringLiteral("vtk3D"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    const bool smoke = app.arguments().contains("--smoke-test");
    app.setProperty("smokeTest", smoke);
    MainWindow window;
    window.show();
    if (smoke) {
        QObject::connect(&window, &MainWindow::taskFailed, &app, [&app](const QString &) { app.exit(2); });
        QObject::connect(&window, &MainWindow::dataReady, &app, [&app, &window] {
            QTimer::singleShot(250, &app, [&app, &window] {
                auto *four = window.findChild<FourViewer *>();
                const auto slices = window.findChildren<SliceView *>();
                if (!four || slices.size() != 3) {
                    app.exit(4);
                    return;
                }
                auto *slice = slices.front();
                const int before = slice->currentSlice();
                auto *widget = slice->findChild<QVTKOpenGLNativeWidget *>();
                QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton,
                                  Qt::NoModifier, Qt::NoScrollPhase, false);
                QApplication::sendEvent(widget, &wheel);
                if (slice->currentSlice() != before + 1) {
                    app.exit(5);
                    return;
                }
                four->setBoneVisible(false);
                four->setBoneVisible(true);
                four->setSkinOpacity(20);
                four->setSkinOpacity(34);
                four->setDarkTheme(false);
                four->setDarkTheme(true);
                four->resetCameras();
                window.grab().save("build/ui-smoke.png");
                QFile marker("build/ui-smoke.ok");
                if (marker.open(QIODevice::WriteOnly))
                    marker.write("pipeline rendered");
                app.exit(0);
            });
        });
        QTimer::singleShot(180000, &app, [&app] { app.exit(3); });
        QTimer::singleShot(0, &window, [&window, &app] { window.importPath(app.arguments().last()); });
    }
    return app.exec();
}
