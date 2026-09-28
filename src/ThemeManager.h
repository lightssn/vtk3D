#pragma once

#include <QObject>

class QApplication;

class ThemeManager final : public QObject {
        Q_OBJECT

    public:
        enum class Theme { Light, Dark };

        explicit ThemeManager(QObject* parent = nullptr);
        Theme theme() const noexcept;
        void apply(QApplication* app, Theme theme);

    signals:
        void themeChanged(bool dark);

    private:
        Theme m_theme = Theme::Dark;
    };

