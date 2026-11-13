#pragma once
#include <QSettings>
class Settings {
  public:
    QVariant value(const QString &key, const QVariant &fallback = {}) const {
        return QSettings().value(key, fallback);
    }
    void setValue(const QString &key, const QVariant &value) {
        QSettings().setValue(key, value);
    }
};
