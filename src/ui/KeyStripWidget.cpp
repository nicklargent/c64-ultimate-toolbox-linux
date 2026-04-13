#include "ui/KeyStripWidget.h"
#include "network/KeyboardForwarder.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

KeyStripWidget::KeyStripWidget(KeyboardForwarder *forwarder, QWidget *parent)
    : QWidget(parent)
    , m_forwarder(forwarder)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(2);

    // Top row: F1-F8
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(2);
    struct FKey { const char *label; uint8_t petscii; };
    FKey fkeys[] = {
        {"F1", 0x85}, {"F2", 0x89}, {"F3", 0x86}, {"F4", 0x8A},
        {"F5", 0x87}, {"F6", 0x8B}, {"F7", 0x88}, {"F8", 0x8C}
    };
    for (auto &fk : fkeys)
        topRow->addWidget(makeKeyButton(fk.label, fk.petscii));
    mainLayout->addLayout(topRow);

    // Bottom row: special keys
    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(2);

    // MENU button (not a PETSCII key)
    auto *menuBtn = new QPushButton("MENU", this);
    menuBtn->setFont(QFont("monospace", 9, QFont::Medium));
    menuBtn->setFixedHeight(24);
    connect(menuBtn, &QPushButton::clicked, this, &KeyStripWidget::menuButtonClicked);
    bottomRow->addWidget(menuBtn);

    bottomRow->addSpacing(4);

    struct SpecialKey { const char *label; uint8_t petscii; };
    SpecialKey keys[] = {
        {"R/S",  0x03}, {"HOME", 0x13}, {"CLR",  0x93},
        {"INST", 0x94}, {"DEL",  0x14},
    };
    for (auto &k : keys)
        bottomRow->addWidget(makeKeyButton(k.label, k.petscii));

    bottomRow->addSpacing(4);

    SpecialKey cursorKeys[] = {
        {"\xe2\x96\xb2", 0x91}, {"\xe2\x96\xbc", 0x11},  // ▲ ▼
        {"\xe2\x97\x80", 0x9D}, {"\xe2\x96\xb6", 0x1D},  // ◀ ▶
    };
    for (auto &k : cursorKeys)
        bottomRow->addWidget(makeKeyButton(k.label, k.petscii));

    bottomRow->addSpacing(4);

    SpecialKey extraKeys[] = {
        {"\xc2\xa3", 0x1C},   // £
        {"\xe2\x86\x91", 0x5E}, // ↑
        {"\xe2\x86\x90", 0x5F}, // ←
        {"S+RET", 0x8D},
    };
    for (auto &k : extraKeys)
        bottomRow->addWidget(makeKeyButton(k.label, k.petscii));

    mainLayout->addLayout(bottomRow);
}

QPushButton *KeyStripWidget::makeKeyButton(const QString &label, uint8_t petscii)
{
    auto *btn = new QPushButton(label, this);
    btn->setFont(QFont("monospace", 9, QFont::Medium));
    btn->setFixedHeight(24);
    btn->setProperty("petscii", petscii);
    connect(btn, &QPushButton::clicked, this, &KeyStripWidget::onKeyTapped);
    m_keyButtons.append(btn);
    return btn;
}

void KeyStripWidget::onKeyTapped()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn || !m_forwarder)
        return;

    uint8_t petscii = btn->property("petscii").toUInt();
    m_forwarder->sendKey(petscii);
}
