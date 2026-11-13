#include "ParameterPanel.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
ParameterPanel::ParameterPanel(QWidget *parent) : QWidget(parent) {
    setObjectName("sidebar");
    auto *form = new QFormLayout(this);
    auto integer = [this, form](const QString &label, int low, int high, int value) {
        auto *spin = new QSpinBox(this);
        spin->setRange(low, high);
        spin->setValue(value);
        form->addRow(label, spin);
        return spin;
    };
    auto real = [this, form](const QString &label, double low, double high, double value) {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(low, high);
        spin->setValue(value);
        form->addRow(label, spin);
        return spin;
    };
    form->addRow(new QLabel(tr("ITK 影像处理 / VTK 重建"), this));
    m_skin = integer(tr("皮肤下限"), -32768, 65535, 500);
    m_bone = integer(tr("骨骼下限"), -32768, 65535, 1150);
    form->addRow(new QLabel(tr("FullHead 标量有偏移，阈值非标准 HU"), this));
    m_radius = integer(tr("中值去噪半径"), 0, 3, 1);
    m_spacing = real(tr("重采样间距（0=保持）"), 0, 10, 0);
    m_largest = new QCheckBox(tr("仅保留最大连通域"), this);
    form->addRow(m_largest);
    m_closing = integer(tr("闭运算半径"), 0, 3, 0);
    m_smoothing = integer(tr("网格平滑次数"), 0, 100, 15);
    m_reduction = real(tr("网格减面比例"), 0, 0.95, 0.25);
    m_reduction->setSingleStep(0.05);
    m_registrationIterations = integer(tr("配准最大迭代"), 1, 300, 40);
    m_registrationStep = real(tr("配准初始步长"), 0.01, 20, 1);
    auto *apply = new QPushButton(tr("应用处理与重建"), this);
    form->addRow(apply);
    connect(apply, &QPushButton::clicked, this, &ParameterPanel::processRequested);
    for (int i = 0; i < 2; ++i) {
        auto *visible = new QCheckBox(i ? tr("显示骨骼") : tr("显示皮肤"), this);
        visible->setChecked(true);
        form->addRow(visible);
        connect(visible, &QCheckBox::toggled, this,
                [this, i](bool value) { emit visibilityChanged(i, value); });
        auto *opacity = new QSlider(Qt::Horizontal, this);
        opacity->setRange(0, 100);
        opacity->setValue(i ? 100 : 34);
        form->addRow(tr("不透明度"), opacity);
        connect(opacity, &QSlider::valueChanged, this,
                [this, i](int value) { emit opacityChanged(i, value); });
    }
}
float ParameterPanel::skinThreshold() const {
    return static_cast<float>(m_skin->value());
}
float ParameterPanel::boneThreshold() const {
    return static_cast<float>(m_bone->value());
}
PreprocessingParameter ParameterPanel::preprocessing() const {
    return {static_cast<unsigned>(m_radius->value()), m_spacing->value()};
}
SegmentationParameter ParameterPanel::segmentation() const {
    SegmentationParameter p;
    p.largestComponent = m_largest->isChecked();
    p.closingRadius = m_closing->value();
    return p;
}
SurfaceProcessingParameter ParameterPanel::surfaceProcessing() const {
    return {m_smoothing->value(), m_reduction->value(), true};
}
RegistrationParameter ParameterPanel::registration() const {
    return {static_cast<unsigned>(m_registrationIterations->value()), m_registrationStep->value()};
}
void ParameterPanel::restore(const QJsonObject &p) {
    if (p.isEmpty())
        return;
    m_skin->setValue(p["skinThreshold"].toInt(500));
    m_bone->setValue(p["boneThreshold"].toInt(1150));
    m_radius->setValue(p["medianRadius"].toInt(1));
    m_spacing->setValue(p["spacing"].toDouble());
    m_largest->setChecked(p["largestComponent"].toBool());
    m_closing->setValue(p["closingRadius"].toInt());
    m_smoothing->setValue(p["smoothingIterations"].toInt(15));
    m_reduction->setValue(p["reduction"].toDouble());
}
