#pragma once
#include "Task.h"
#include <itkCommand.h>

// Reused by image readers and processing filters. Cancellation is cooperative:
// an IO implementation that emits no events can only stop after Update returns.
class CancellationObserver : public itk::Command {
  public:
    using Self = CancellationObserver;
    using Pointer = itk::SmartPointer<Self>;
    itkNewMacro(Self);
    Task task;
    void Execute(itk::Object *, const itk::EventObject &) override { task.check(); }
    void Execute(const itk::Object *, const itk::EventObject &) override { task.check(); }
};
template <class T> void updateWithTask(T *process, const Task &task) {
    auto observer = CancellationObserver::New();
    observer->task = task;
    process->AddObserver(itk::ProgressEvent(), observer);
    process->AddObserver(itk::IterationEvent(), observer);
    task.check();
    process->Update();
    task.check();
}
