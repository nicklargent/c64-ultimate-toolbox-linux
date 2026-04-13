#pragma once

#include <QWidget>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include <QVBoxLayout>

class C64Connection;
class CrtRenderer;

class DisplayAudioPanel : public QWidget {
    Q_OBJECT
public:
    explicit DisplayAudioPanel(C64Connection *connection, CrtRenderer *renderer, QWidget *parent = nullptr);

private slots:
    void onSliderChanged(int value);
    void onPresetChanged(int index);
    void onTintModeChanged(int index);
    void onMaskTypeChanged(int index);

private:
    void addSection(QVBoxLayout *layout, const QString &title);
    void addSubsection(QVBoxLayout *layout, const QString &title);
    void addSlider(QVBoxLayout *layout, const QString &key, const QString &label,
                   float min, float max, int steps = 100);
    void refreshSliders();
    void applySettings();
    float sliderToValue(const QString &key, int sliderVal);
    int valueToSlider(const QString &key, float value);
    float currentValue(const QString &key);

    C64Connection *m_connection;
    CrtRenderer *m_renderer;

    QComboBox *m_presetCombo;
    QComboBox *m_tintModeCombo;
    QComboBox *m_maskTypeCombo;

    struct SliderInfo {
        QSlider *slider;
        QLabel *valueLabel;
        float min;
        float max;
    };
    QMap<QString, SliderInfo> m_sliders;
};
