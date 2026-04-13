#pragma once

#include <QWidget>
#include <QPushButton>
#include <QList>

class KeyboardForwarder;

class KeyStripWidget : public QWidget {
    Q_OBJECT
public:
    explicit KeyStripWidget(KeyboardForwarder *forwarder, QWidget *parent = nullptr);

signals:
    void menuButtonClicked();

private slots:
    void onKeyTapped();

private:
    QPushButton *makeKeyButton(const QString &label, uint8_t petscii);

    KeyboardForwarder *m_forwarder;
    QList<QPushButton *> m_keyButtons;
};
