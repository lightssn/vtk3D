#include "ThemeManager.h"

#include "foundation/Settings.h"
#include <QApplication>
#include <QFile>
#include <QPalette>

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    const Settings settings;
    m_theme =
        settings.value(QStringLiteral("appearance/darkTheme"), true).toBool() ? Theme::Dark : Theme::Light;
}

ThemeManager::Theme ThemeManager::theme() const noexcept {
    return m_theme;
}

void ThemeManager::apply(QApplication *app, Theme theme) {
    m_theme = theme;
    const bool dark = theme == Theme::Dark;
    QFile styleFile(dark ? QStringLiteral(":/themes/dark.qss") : QStringLiteral(":/themes/light.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
    app->setPalette(QPalette());

    Settings settings;
    settings.setValue(QStringLiteral("appearance/darkTheme"), dark);
    emit themeChanged(dark);
}
