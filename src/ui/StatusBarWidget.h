#pragma once

#include <QWidget>
#include <QLabel>

class StatusBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatusBarWidget(QWidget *parent = nullptr);

    void update(bool isRecording, bool isKeyboardActive);

private:
    QLabel *m_recBadge;
    QLabel *m_kbBadge;
};
