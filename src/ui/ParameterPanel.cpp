#include "ParameterPanel.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QStyle>

ParameterPanel::ParameterPanel(QWidget *parent) : QWidget(parent) {
    setObjectName("sidebar");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    auto label = [this](const QString &text, const char *name) {
        auto *result = new QLabel(text, this);
        result->setObjectName(name);
        return result;
    };
    auto separator = [this, layout] {
        auto *line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setObjectName("separator");
        layout->addWidget(line);
    };
    auto *open = new QPushButton(tr("打开影像数据"), this);
    m_open = open;
    open->setObjectName("primaryButton");
    open->setMinimumHeight(42);
    connect(open, &QPushButton::clicked, this, &ParameterPanel::importRequested);
    auto *card = new QWidget(this);
    card->setObjectName("infoCard");
    auto *info = new QVBoxLayout(card);
    info->setContentsMargins(12, 10, 12, 10);
    info->addWidget(label(tr("当前数据"), "caption"));
    m_fileLabel = label(tr("尚未加载"), "fileName");
    info->addWidget(m_fileLabel);
    m_dimensionLabel = label("—", "muted");
    info->addWidget(m_dimensionLabel);
    layout->addWidget(card);
    layout->addWidget(open);
    separator();
    layout->addWidget(label(tr("组织图层"), "sectionTitle"));
    auto tissue = [this, layout, label](bool skin) {
        int layer = skin ? 0 : 1;
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(label("●", skin ? "skinDot" : "boneDot"));
        auto *visible = new QCheckBox(skin ? tr("皮肤表面") : tr("骨骼表面"), this);
        visible->setChecked(true);
        row->addWidget(visible, 1);
        layout->addLayout(row);
        connect(visible, &QCheckBox::toggled, this,
                [this, layer](bool v) { emit visibilityChanged(layer, v); });
        auto *opacityRow = new QHBoxLayout;
        opacityRow->setContentsMargins(20, 0, 0, 0);
        opacityRow->addWidget(label(tr("不透明度"), "muted"));
        auto *opacity = new QSlider(Qt::Horizontal, this);
        opacity->setRange(0, 100);
        opacity->setValue(skin ? 34 : 100);
        opacityRow->addWidget(opacity, 1);
        auto *value = label(QString::number(opacity->value()) + "%", "valueLabel");
        value->setFixedWidth(38);
        opacityRow->addWidget(value);
        connect(opacity, &QSlider::valueChanged, this, [this, layer, value](int v) {
            value->setText(QString::number(v) + "%");
            emit opacityChanged(layer, v);
        });
        layout->addLayout(opacityRow);
        auto *thresholdRow = new QHBoxLayout;
        thresholdRow->setContentsMargins(20, 0, 0, 4);
        thresholdRow->addWidget(label(tr("分割下限"), "muted"));
        thresholdRow->addStretch();
        auto *spin = new QSpinBox(this);
        spin->setRange(-32768, 65535);
        spin->setValue(skin ? 500 : 1150);
        thresholdRow->addWidget(spin);
        layout->addLayout(thresholdRow);
        return spin;
    };
    m_skin = tissue(true);
    m_bone = tissue(false);
    auto *overlayRow = new QHBoxLayout;
    auto *overlay = new QCheckBox(tr("骨骼标签"), this);
    overlay->setObjectName("labelOverlay");
    overlay->setToolTip(tr("在二维切片上叠加骨骼分割区域"));
    auto *opacity = new QSlider(Qt::Horizontal, this);
    opacity->setRange(5, 90);
    opacity->setValue(35);
    opacity->setToolTip(tr("标签不透明度"));
    overlayRow->addWidget(overlay);
    overlayRow->addWidget(opacity, 1);
    layout->addLayout(overlayRow);
    connect(overlay, &QCheckBox::toggled, this, [this, opacity](bool visible) {
        emit overlayChanged(visible, opacity->value());
    });
    connect(opacity, &QSlider::valueChanged, this, [this, overlay](int value) {
        emit overlayChanged(overlay->isChecked(), value);
    });
    auto *note = label(tr("阈值使用输入灰度；未确认单位时不标作 HU。"), "footnote");
    note->setWordWrap(true);
    layout->addWidget(note);
    separator();
    auto *toggle = new QToolButton(this);
    toggle->setObjectName("debugToggle");
    toggle->setText(tr("▸ 高级处理参数"));
    toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
    layout->addWidget(toggle);
    auto *advanced = new QWidget(this);
    advanced->setObjectName("debugPanel");
    auto *form = new QFormLayout(advanced);
    form->setContentsMargins(12, 10, 12, 10);
    form->setSpacing(10);
    auto integer = [advanced, form](const QString &text, int lo, int hi, int initial) {
        auto *w = new QSpinBox(advanced);
        w->setRange(lo, hi);
        w->setValue(initial);
        form->addRow(text, w);
        return w;
    };
    auto real = [advanced, form](const QString &text, double lo, double hi, double initial) {
        auto *w = new QDoubleSpinBox(advanced);
        w->setRange(lo, hi);
        w->setValue(initial);
        form->addRow(text, w);
        return w;
    };
    m_radius = integer(tr("中值去噪半径"), 0, 3, 1);
    m_spacing = real(tr("重采样间距"), 0, 10, 0);
    m_spacing->setToolTip(tr("0 表示保持原始间距"));
    m_largest = new QCheckBox(tr("仅保留最大连通域"), advanced);
    form->addRow(m_largest);
    m_closing = integer(tr("闭运算半径"), 0, 3, 0);
    m_smoothing = integer(tr("表面平滑迭代"), 0, 100, 15);
    m_reduction = real(tr("网格减面比例"), 0, 0.95, 0.25);
    m_reduction->setSingleStep(0.05);
    m_registrationIterations = integer(tr("配准最大迭代"), 1, 300, 40);
    m_registrationStep = real(tr("配准初始步长"), 0.01, 20, 1);
    advanced->hide();
    layout->addWidget(advanced);
    connect(toggle, &QToolButton::toggled, advanced, &QWidget::setVisible);
    connect(toggle, &QToolButton::toggled, this, [toggle](bool open) {
        toggle->setText(open ? QObject::tr("▾ 高级处理参数") : QObject::tr("▸ 高级处理参数"));
    });
    auto *apply = new QPushButton(tr("应用处理与重建"), this);
    m_apply = apply;
    apply->setObjectName("processButton");
    layout->addWidget(apply);
    connect(apply, &QPushButton::clicked, this, [this] {
        if (m_busy) emit cancelRequested();
        else emit processRequested();
    });
    separator();
    layout->addStretch();
    auto *reset = new QPushButton(tr("重置全部视图"), this);
    reset->setObjectName("secondaryButton");
    layout->addWidget(reset);
    connect(reset, &QPushButton::clicked, this, &ParameterPanel::resetRequested);
    auto *theme = new QToolButton(this);
    theme->setText(tr("切换日间 / 夜间模式"));
    theme->setObjectName("themeButton");
    theme->setToolButtonStyle(Qt::ToolButtonTextOnly);
    theme->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(theme);
    connect(theme, &QToolButton::clicked, this, &ParameterPanel::themeRequested);
    separator();
    m_statistics = label(tr("处理后显示体积、模型统计与耗时"), "statistics");
    m_statistics->setWordWrap(true);
    m_statistics->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_statistics);
}
void ParameterPanel::setBusy(bool busy) {
    m_busy = busy;
    m_apply->setText(busy ? tr("取消任务") : tr("应用处理与重建"));
    m_apply->setProperty("busy", busy);
    m_apply->style()->unpolish(m_apply);
    m_apply->style()->polish(m_apply);
    m_open->setEnabled(!busy);
}
void ParameterPanel::setVolumeInfo(const QString &path, const QString &dimensions) {
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_fileLabel->setToolTip(path);
    m_dimensionLabel->setText(dimensions + tr(" voxels"));
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

void ParameterPanel::setStatistics(const QJsonObject &s) {
    if (s.isEmpty()) { m_statistics->setText(tr("尚无处理统计")); return; }
    auto n = [&s](const char *key) { return s[key].toDouble(); };
    m_statistics->setText(tr("骨骼标签体积  %1 mL\n"
        "皮肤  %4 点 / %5 三角形\n骨骼  %6 点 / %7 三角形\n"
        "表面积  皮肤 %2 / 骨骼 %3 cm²\n"
        "总计 %8 s\n预处理 %9 / 分割 %10 s\n重建 %11 / 网格 %12 s")
        .arg(n("labelVolumeMm3") / 1000, 0, 'f', 2)
        .arg(n("skinAreaMm2") / 100, 0, 'f', 1).arg(n("boneAreaMm2") / 100, 0, 'f', 1)
        .arg(qint64(n("skinPoints"))).arg(qint64(n("skinTriangles")))
        .arg(qint64(n("bonePoints"))).arg(qint64(n("boneTriangles")))
        .arg(n("totalMs") / 1000, 0, 'f', 2).arg(n("preprocessMs") / 1000, 0, 'f', 2)
        .arg(n("segmentationMs") / 1000, 0, 'f', 2).arg(n("reconstructionMs") / 1000, 0, 'f', 2)
        .arg(n("surfaceMs") / 1000, 0, 'f', 2));
    m_statistics->setToolTip(tr("体积来自非零分割标签；表面积来自处理后网格。单位按输入几何解释。"));
}
