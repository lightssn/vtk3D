#pragma once
#include "Task.h"
#include <QObject>
#include <QThread>
#include <deque>

// Lives on the UI thread. Only the worker context moves to the single QThread.
class TaskManager final : public QObject {
    Q_OBJECT
  public:
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager() override;
    void submit(QString name, std::function<void(const Task &)> work, std::function<void()> complete = {});
    void cancel();
    bool busy() const {
        return m_running || !m_queue.empty();
    }
  signals:
    void started(const QString &name);
    void progress(int value, const QString &text);
    void finished(const QString &text);
    void failed(const QString &text);
    void idle();

  private:
    struct Entry {
        QString name;
        std::function<void(const Task &)> work;
        std::function<void()> complete;
    };
    void dispatch();
    QThread m_thread;
    QObject *m_worker;
    std::deque<Entry> m_queue;
    Task m_current;
    bool m_running = false;
    bool m_finishing = false;
};
