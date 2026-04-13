#include "ui/DisplayAudioPanel.h"
#include "network/C64Connection.h"
#include "video/CrtRenderer.h"
#include "audio/AudioPlayer.h"
#include "models/CrtPreset.h"
#include "app/Log.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFrame>

DisplayAudioPanel::DisplayAudioPanel(C64Connection *connection, CrtRenderer *renderer, QWidget *parent)
    : QWidget(parent)
    , m_connection(connection)
    , m_renderer(renderer)
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    // --- Audio ---
    addSection(layout, "Audio");
    addSlider(layout, "volume", "Volume", 0.0f, 1.0f);
    addSlider(layout, "balance", "Balance", -1.0f, 1.0f);

    // Separator
    auto *sep1 = new QFrame(content);
    sep1->setFrameShape(QFrame::HLine);
    layout->addWidget(sep1);

    // --- CRT Preset ---
    addSection(layout, "CRT Preset");
    m_presetCombo = new QComboBox(content);
    for (const auto &preset : CrtPreset::builtInPresets())
        m_presetCombo->addItem(preset.name, preset.id);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayAudioPanel::onPresetChanged);
    layout->addWidget(m_presetCombo);

    // --- Scanlines ---
    addSubsection(layout, "Scanlines");
    addSlider(layout, "scanlineIntensity", "Intensity", 0.0f, 1.0f);
    addSlider(layout, "scanlineWidth", "Width", 0.0f, 1.0f);

    addSubsection(layout, "Blur & Bloom");
    addSlider(layout, "blurRadius", "Blur", 0.0f, 1.0f);
    addSlider(layout, "bloomIntensity", "Bloom Intensity", 0.0f, 1.0f);
    addSlider(layout, "bloomRadius", "Bloom Radius", 0.0f, 1.0f);

    // --- Tint ---
    addSubsection(layout, "Tint");
    auto *tintRow = new QHBoxLayout;
    auto *tintLabel = new QLabel("Type", content);
    tintLabel->setFixedWidth(100);
    tintRow->addWidget(tintLabel);
    m_tintModeCombo = new QComboBox(content);
    m_tintModeCombo->addItems({"None", "Amber", "Green", "Monochrome"});
    connect(m_tintModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayAudioPanel::onTintModeChanged);
    tintRow->addWidget(m_tintModeCombo);
    layout->addLayout(tintRow);
    addSlider(layout, "tintStrength", "Strength", 0.0f, 1.0f);

    // --- Phosphor Mask ---
    addSubsection(layout, "Phosphor Mask");
    auto *maskRow = new QHBoxLayout;
    auto *maskLabel = new QLabel("Type", content);
    maskLabel->setFixedWidth(100);
    maskRow->addWidget(maskLabel);
    m_maskTypeCombo = new QComboBox(content);
    m_maskTypeCombo->addItems({"None", "Aperture Grille", "Shadow Mask", "Slot Mask"});
    connect(m_maskTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DisplayAudioPanel::onMaskTypeChanged);
    maskRow->addWidget(m_maskTypeCombo);
    layout->addLayout(maskRow);
    addSlider(layout, "maskIntensity", "Intensity", 0.0f, 1.0f);

    // --- Screen Shape ---
    addSubsection(layout, "Screen Shape");
    addSlider(layout, "curvatureAmount", "Curvature", 0.0f, 1.0f);
    addSlider(layout, "vignetteStrength", "Vignette", 0.0f, 1.0f);

    // --- Afterglow ---
    addSubsection(layout, "Afterglow");
    addSlider(layout, "afterglowStrength", "Strength", 0.0f, 1.0f);
    addSlider(layout, "afterglowDecaySpeed", "Decay Speed", 1.0f, 15.0f);

    layout->addStretch();

    scrollArea->setWidget(content);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);

    // Initialize combos from current settings
    if (m_renderer) {
        auto s = m_renderer->crtSettings();
        m_tintModeCombo->setCurrentIndex(s.tintMode);
        m_maskTypeCombo->setCurrentIndex(s.maskType);
    }
}

void DisplayAudioPanel::addSection(QVBoxLayout *layout, const QString &title)
{
    auto *label = new QLabel(title);
    label->setStyleSheet("font-size: 13pt; font-weight: bold; color: gray;");
    layout->addWidget(label);
}

void DisplayAudioPanel::addSubsection(QVBoxLayout *layout, const QString &title)
{
    layout->addSpacing(4);
    auto *label = new QLabel(title);
    label->setStyleSheet("font-size: 11pt; font-weight: 600; color: darkgray;");
    layout->addWidget(label);
}

void DisplayAudioPanel::addSlider(QVBoxLayout *layout, const QString &key, const QString &label,
                                   float min, float max, int steps)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(8);

    auto *nameLabel = new QLabel(label);
    nameLabel->setFixedWidth(100);
    nameLabel->setStyleSheet("font-size: 11pt;");
    row->addWidget(nameLabel);

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, steps);
    slider->setObjectName(key);

    float value = currentValue(key);
    slider->setValue(static_cast<int>((value - min) / (max - min) * steps));

    connect(slider, &QSlider::valueChanged, this, &DisplayAudioPanel::onSliderChanged);
    row->addWidget(slider, 1);

    auto *valueLabel = new QLabel;
    valueLabel->setFixedWidth(40);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setFont(QFont("monospace"));
    valueLabel->setStyleSheet("color: gray;");
    valueLabel->setText(QString::number(value, 'f', 2));
    row->addWidget(valueLabel);

    layout->addLayout(row);

    m_sliders[key] = {slider, valueLabel, min, max};
}

float DisplayAudioPanel::currentValue(const QString &key)
{
    if (key == "volume") return m_connection->audioPlayer()->volume();
    if (key == "balance") return m_connection->audioPlayer()->balance();

    auto s = m_renderer ? m_renderer->crtSettings() : CrtSettings{};
    if (key == "scanlineIntensity") return s.scanlineIntensity;
    if (key == "scanlineWidth") return s.scanlineWidth;
    if (key == "blurRadius") return s.blurRadius;
    if (key == "bloomIntensity") return s.bloomIntensity;
    if (key == "bloomRadius") return s.bloomRadius;
    if (key == "afterglowStrength") return s.afterglowStrength;
    if (key == "afterglowDecaySpeed") return s.afterglowDecaySpeed;
    if (key == "tintStrength") return s.tintStrength;
    if (key == "maskIntensity") return s.maskIntensity;
    if (key == "curvatureAmount") return s.curvatureAmount;
    if (key == "vignetteStrength") return s.vignetteStrength;
    return 0.0f;
}

float DisplayAudioPanel::sliderToValue(const QString &key, int sliderVal)
{
    auto it = m_sliders.find(key);
    if (it == m_sliders.end()) return 0;
    int steps = it->slider->maximum();
    return it->min + (it->max - it->min) * sliderVal / steps;
}

int DisplayAudioPanel::valueToSlider(const QString &key, float value)
{
    auto it = m_sliders.find(key);
    if (it == m_sliders.end()) return 0;
    int steps = it->slider->maximum();
    return static_cast<int>((value - it->min) / (it->max - it->min) * steps);
}

void DisplayAudioPanel::onSliderChanged(int sliderVal)
{
    auto *slider = qobject_cast<QSlider *>(sender());
    if (!slider) return;

    QString key = slider->objectName();
    float value = sliderToValue(key, sliderVal);

    // Update label
    auto it = m_sliders.find(key);
    if (it != m_sliders.end())
        it->valueLabel->setText(QString::number(value, 'f', 2));

    // Apply
    if (key == "volume") {
        m_connection->audioPlayer()->setVolume(value);
        return;
    }
    if (key == "balance") {
        m_connection->audioPlayer()->setBalance(value);
        return;
    }

    if (!m_renderer) return;

    auto settings = m_renderer->crtSettings();
    if (key == "scanlineIntensity") settings.scanlineIntensity = value;
    else if (key == "scanlineWidth") settings.scanlineWidth = value;
    else if (key == "blurRadius") settings.blurRadius = value;
    else if (key == "bloomIntensity") settings.bloomIntensity = value;
    else if (key == "bloomRadius") settings.bloomRadius = value;
    else if (key == "afterglowStrength") settings.afterglowStrength = value;
    else if (key == "afterglowDecaySpeed") settings.afterglowDecaySpeed = value;
    else if (key == "tintStrength") settings.tintStrength = value;
    else if (key == "maskIntensity") settings.maskIntensity = value;
    else if (key == "curvatureAmount") settings.curvatureAmount = value;
    else if (key == "vignetteStrength") settings.vignetteStrength = value;

    m_renderer->setCrtSettings(settings);
}

void DisplayAudioPanel::onPresetChanged(int index)
{
    auto presets = CrtPreset::builtInPresets();
    if (index < 0 || index >= presets.size())
        return;

    if (m_renderer)
        m_renderer->setCrtSettings(presets[index].settings);

    refreshSliders();
}

void DisplayAudioPanel::onTintModeChanged(int index)
{
    if (!m_renderer) return;
    auto settings = m_renderer->crtSettings();
    settings.tintMode = index;
    m_renderer->setCrtSettings(settings);
}

void DisplayAudioPanel::onMaskTypeChanged(int index)
{
    if (!m_renderer) return;
    auto settings = m_renderer->crtSettings();
    settings.maskType = index;
    m_renderer->setCrtSettings(settings);
}

void DisplayAudioPanel::refreshSliders()
{
    for (auto it = m_sliders.begin(); it != m_sliders.end(); ++it) {
        float value = currentValue(it.key());
        it->slider->blockSignals(true);
        it->slider->setValue(valueToSlider(it.key(), value));
        it->slider->blockSignals(false);
        it->valueLabel->setText(QString::number(value, 'f', 2));
    }

    if (m_renderer) {
        auto s = m_renderer->crtSettings();
        m_tintModeCombo->blockSignals(true);
        m_tintModeCombo->setCurrentIndex(s.tintMode);
        m_tintModeCombo->blockSignals(false);
        m_maskTypeCombo->blockSignals(true);
        m_maskTypeCombo->setCurrentIndex(s.maskType);
        m_maskTypeCombo->blockSignals(false);
    }
}
