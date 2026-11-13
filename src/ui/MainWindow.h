#pragma once
#include "foundation/TaskManager.h"
#include "services/Services.h"
#include <QMainWindow>
class FourViewer;
class ParameterPanel;
class ThemeManager;
class QLabel;
class QProgressBar;
class MainWindow final : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void importPath(const QString &);
    bool hasData() const {
        return bool(m_project.data().volume.image);
    }
  signals:
    void dataReady();
    void taskFailed(const QString &);

  private:
    void process();
    void showProject();
    void exportSurface(bool skin);
    void registerVolume();
    void openProject();
    void saveProject();
    FourViewer *m_viewer;
    ParameterPanel *m_parameters;
    ThemeManager *m_theme;
    QLabel *m_status;
    QProgressBar *m_progress;
    ProjectService m_project;
    vtkSmartPointer<vtkImageData> m_displayImage;
    ImportService m_import;
    ProcessingService m_processing;
    ExportService m_export;
    TaskManager m_tasks;
};
