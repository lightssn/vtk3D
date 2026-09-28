#pragma once

#include <QMainWindow>

class QAction;
class QLabel;
class MedicalViewer;
class QCheckBox;
class QSpinBox;
class QToolButton;
class QProgressBar;
class ThemeManager;

class MainWindow final : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);

    private slots:
        void openVolume();
        void exportSkin();
        void exportBone();
        void toggleTheme();

    private:
        void buildUi();
        QWidget* createSidebar();
        void createActions();
        void tryLoadDefaultData();
        void setDataActionsEnabled(bool enabled);
        void exportSurface(bool skin);

        MedicalViewer* m_viewer = nullptr;
        ThemeManager* m_themeManager = nullptr;
        QLabel* m_fileLabel = nullptr;
        QLabel* m_dimensionLabel = nullptr;
        QToolButton* m_themeButton = nullptr;
        QAction* m_exportSkinAction = nullptr;
        QAction* m_exportBoneAction = nullptr;
        QProgressBar* m_progressBar = nullptr;
        QLabel* m_progressLabel = nullptr;
    };
