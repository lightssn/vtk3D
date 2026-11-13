#include "TaskManager.h"
#include <QMetaObject>
#include <exception>

TaskManager::TaskManager(QObject *parent) : QObject(parent), m_worker(new QObject) {
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
}
TaskManager::~TaskManager() {
    cancel();
    m_thread.quit();
    m_thread.wait();
}
void TaskManager::cancel() {
    m_queue.clear();
    m_current.cancellation->store(true);
}
void TaskManager::submit(QString name, std::function<void(const Task &)> work,
                         std::function<void()> complete) {
    m_queue.push_back({std::move(name), std::move(work), std::move(complete)});
    dispatch();
}
void TaskManager::dispatch() {
    if (m_running || m_finishing || m_queue.empty())
        return;
    auto entry = std::move(m_queue.front());
    m_queue.pop_front();
    m_running = true;
    m_current = Task{};
    m_current.progress = [this](int value, const QString &text) {
        QMetaObject::invokeMethod(
            this, [this, value, text] { emit progress(value, text); }, Qt::QueuedConnection);
    };
    emit started(entry.name);
    const Task task = m_current;
    QMetaObject::invokeMethod(
        m_worker,
        [this, task, entry = std::move(entry)] {
            QString error;
            bool cancelled = false;
            try {
                task.check();
                entry.work(task);
                task.check();
            } catch (const TaskCancelled &) {
                cancelled = true;
            } catch (const std::exception &ex) {
                error = QString::fromUtf8(ex.what());
            } catch (...) {
                error = QStringLiteral("Unknown processing error");
            }
            QMetaObject::invokeMethod(
                this,
                [this, task, entry, error, cancelled] {
                    m_running = false;
                    m_finishing = true;
                    if (cancelled || task.cancellation->load())
                        emit finished(tr("任务已取消"));
                    else if (!error.isEmpty())
                        emit failed(error);
                    else {
                        try {
                            if (entry.complete)
                                entry.complete();
                            emit finished(entry.name + tr("完成"));
                        } catch (const std::exception &ex) {
                            emit failed(QString::fromUtf8(ex.what()));
                        }
                    }
                    m_finishing = false;
                    dispatch();
                    if (!busy())
                        emit idle();
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}
