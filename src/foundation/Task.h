#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

class TaskCancelled : public std::runtime_error {
  public:
    TaskCancelled() : std::runtime_error("Task cancelled") {}
};
struct Task {
    std::shared_ptr<std::atomic_bool> cancellation = std::make_shared<std::atomic_bool>(false);
    std::function<void(int, const QString &)> progress;
    void check() const {
        if (cancellation->load())
            throw TaskCancelled();
    }
    void report(int value, const QString &text) const {
        check();
        if (progress)
            progress(value, text);
    }
};
