#pragma once
#include <QWidget>
#include <vtkSmartPointer.h>
class vtkImageData;
class vtkImageViewer2;
class QVTKOpenGLNativeWidget;
class QLabel;
class SliceView final : public QWidget {
    Q_OBJECT
  public:
    SliceView(const QString &title, int orientation, QWidget *parent = nullptr);
    ~SliceView() override;
    void setImage(vtkImageData *);
    void setDarkTheme(bool);
    void resetCamera();
    int currentSlice() const;

  protected:
    bool eventFilter(QObject *, QEvent *) override;

  private:
    vtkSmartPointer<vtkImageViewer2> m_viewer;
    QVTKOpenGLNativeWidget *m_widget;
    QLabel *m_label;
    bool m_hasData = false;
    void updateLabel();
};
