#include "ui/StatusBarWidget.h"

#include <QHBoxLayout>

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(24);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(4);

    m_recBadge = new QLabel("REC", this);
    m_recBadge->setAlignment(Qt::AlignCenter);
    m_recBadge->setMinimumWidth(30);
    m_recBadge->setStyleSheet(
        "QLabel { background-color: rgba(255, 59, 48, 204); color: white; "
        "font-size: 10pt; font-weight: bold; border-radius: 3px; padding: 1px 4px; }");
    m_recBadge->hide();

    layout->addWidget(m_recBadge);
    layout->addStretch();

    m_kbBadge = new QLabel("KB", this);
    m_kbBadge->setAlignment(Qt::AlignCenter);
    m_kbBadge->setMinimumWidth(24);
    m_kbBadge->setStyleSheet(
        "QLabel { background-color: rgba(0, 122, 255, 204); color: white; "
        "font-size: 10pt; font-weight: bold; border-radius: 3px; padding: 1px 4px; }");
    m_kbBadge->hide();

    layout->addWidget(m_kbBadge);
}

void StatusBarWidget::update(bool isRecording, bool isKeyboardActive)
{
    m_recBadge->setVisible(isRecording);
    m_kbBadge->setVisible(isKeyboardActive);
}
