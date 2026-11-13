#pragma once
#include <QDebug>
class Logger {
  public:
    static void error(const QString &text) {
        qWarning().noquote() << text;
    }
};
