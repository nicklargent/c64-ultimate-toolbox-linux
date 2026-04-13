#include "ui/SystemPanel.h"
#include "network/C64Connection.h"
#include "network/C64ApiClient.h"
#include "app/Log.h"

#include <QScrollArea>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonObject>
#include <QJsonArray>

SystemPanel::SystemPanel(C64Connection *connection, QWidget *parent)
    : QWidget(parent)
    , m_connection(connection)
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget;
    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);

    // --- Device Info ---
    auto info = connection->deviceInfo();
    if (!info.product.isEmpty()) {
        auto *infoGroup = new QGroupBox("Device", content);
        auto *infoLayout = new QVBoxLayout(infoGroup);
        addInfoRow(infoLayout, "Product", info.product);
        addInfoRow(infoLayout, "Firmware", "v" + info.firmwareVersion);
        addInfoRow(infoLayout, "FPGA", info.fpgaVersion);
        addInfoRow(infoLayout, "Hostname", info.hostname);
        mainLayout->addWidget(infoGroup);
    }

    // --- Streams ---
    auto *streamGroup = new QGroupBox("Streams", content);
    auto *streamLayout = new QVBoxLayout(streamGroup);

    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(new QLabel("Status:"));
    m_streamStatusLabel = new QLabel(connection->streamsActive() ? "Active" : "Inactive");
    m_streamStatusLabel->setStyleSheet(connection->streamsActive() ? "color: green;" : "color: gray;");
    statusRow->addWidget(m_streamStatusLabel);
    statusRow->addStretch();
    streamLayout->addLayout(statusRow);

    auto *fpsRow = new QHBoxLayout;
    fpsRow->addWidget(new QLabel("Video FPS:"));
    m_streamFpsLabel = new QLabel("0 fps");
    m_streamFpsLabel->setFont(QFont("monospace"));
    fpsRow->addWidget(m_streamFpsLabel);
    fpsRow->addStretch();
    streamLayout->addLayout(fpsRow);

    mainLayout->addWidget(streamGroup);

    m_streamTimer = new QTimer(this);
    connect(m_streamTimer, &QTimer::timeout, this, &SystemPanel::updateStreamStatus);
    m_streamTimer->start(500);

    // --- Drives ---
    auto *driveGroup = new QGroupBox("Drives", content);
    auto *driveLayout = new QVBoxLayout(driveGroup);

    auto *driveARow = new QHBoxLayout;
    driveARow->addWidget(new QLabel("A:"));
    m_driveALabel = new QLabel("\u2014");
    m_driveALabel->setStyleSheet("color: gray;");
    driveARow->addWidget(m_driveALabel, 1);
    driveLayout->addLayout(driveARow);

    auto *driveBRow = new QHBoxLayout;
    driveBRow->addWidget(new QLabel("B:"));
    m_driveBLabel = new QLabel("\u2014");
    m_driveBLabel->setStyleSheet("color: gray;");
    driveBRow->addWidget(m_driveBLabel, 1);
    driveLayout->addLayout(driveBRow);

    mainLayout->addWidget(driveGroup);

    // --- Configuration ---
    auto *configGroup = new QGroupBox("Configuration", content);
    auto *configLayout = new QVBoxLayout(configGroup);

    auto *configToolbar = new QHBoxLayout;
    m_searchField = new QLineEdit(content);
    m_searchField->setPlaceholderText("Search settings...");
    connect(m_searchField, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_configTree->topLevelItemCount(); i++) {
            auto *cat = m_configTree->topLevelItem(i);
            bool catVisible = false;
            for (int j = 0; j < cat->childCount(); j++) {
                auto *item = cat->child(j);
                bool match = text.isEmpty() ||
                    item->text(0).contains(text, Qt::CaseInsensitive) ||
                    item->text(1).contains(text, Qt::CaseInsensitive);
                item->setHidden(!match);
                if (match) catVisible = true;
            }
            cat->setHidden(!catVisible && !text.isEmpty());
            if (catVisible && !text.isEmpty())
                cat->setExpanded(true);
        }
    });
    configToolbar->addWidget(m_searchField);

    auto *saveBtn = new QPushButton("Save to Flash", content);
    connect(saveBtn, &QPushButton::clicked, this, &SystemPanel::onSaveToFlash);
    configToolbar->addWidget(saveBtn);
    configLayout->addLayout(configToolbar);

    m_configTree = new QTreeWidget(content);
    m_configTree->setHeaderLabels({"Setting", "Value"});
    m_configTree->header()->setStretchLastSection(true);
    m_configTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_configTree->setAlternatingRowColors(true);
    m_configTree->setRootIsDecorated(true);
    connect(m_configTree, &QTreeWidget::itemDoubleClicked, this, &SystemPanel::onConfigItemClicked);
    configLayout->addWidget(m_configTree);

    m_configStatus = new QLabel("Loading configuration...", content);
    m_configStatus->setStyleSheet("color: gray; font-size: 10pt;");
    configLayout->addWidget(m_configStatus);

    mainLayout->addWidget(configGroup, 1);

    scrollArea->setWidget(content);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);

    // Load data
    if (connection->apiClient()) {
        refreshDriveStatus();
        loadAllConfigs();
    }
}

void SystemPanel::addInfoRow(QVBoxLayout *layout, const QString &label, const QString &value)
{
    auto *row = new QHBoxLayout;
    auto *nameLabel = new QLabel(label + ":");
    nameLabel->setFixedWidth(70);
    nameLabel->setStyleSheet("font-weight: 500;");
    row->addWidget(nameLabel);
    auto *valLabel = new QLabel(value);
    valLabel->setStyleSheet("color: gray;");
    row->addWidget(valLabel);
    row->addStretch();
    layout->addLayout(row);
}

void SystemPanel::updateStreamStatus()
{
    bool active = m_connection->streamsActive();
    m_streamStatusLabel->setText(active ? "Active" : "Inactive");
    m_streamStatusLabel->setStyleSheet(active ? "color: green;" : "color: gray;");
    m_streamFpsLabel->setText(QStringLiteral("%1 fps").arg(m_connection->framesPerSecond()));
}

void SystemPanel::refreshDriveStatus()
{
    auto *api = m_connection->apiClient();
    if (!api)
        return;

    connect(api, &C64ApiClient::drivesReceived, this, [this](const QJsonArray &drives) {
        // API returns: [ {"a": {enabled, type, image_file, ...}}, {"b": {...}}, ... ]
        auto updateLabel = [](QLabel *label, const QJsonObject &info) {
            bool enabled = info["enabled"].toBool();
            QString imageFile = info["image_file"].toString();
            QString type = info["type"].toString();

            if (!enabled) {
                label->setText("Disabled");
                label->setStyleSheet("color: gray;");
            } else if (imageFile.isEmpty()) {
                label->setText(QStringLiteral("[%1] Empty").arg(type));
                label->setStyleSheet("color: gray;");
            } else {
                // Show just the filename
                QString name = imageFile.section('/', -1);
                label->setText(QStringLiteral("[%1] %2").arg(type, name));
                label->setStyleSheet("");
            }
        };

        for (const auto &driveVal : drives) {
            QJsonObject driveObj = driveVal.toObject();
            if (driveObj.contains("a"))
                updateLabel(m_driveALabel, driveObj["a"].toObject());
            else if (driveObj.contains("b"))
                updateLabel(m_driveBLabel, driveObj["b"].toObject());
        }
    }, Qt::SingleShotConnection);

    api->fetchDrives();
}

void SystemPanel::loadAllConfigs()
{
    auto *api = m_connection->apiClient();
    if (!api)
        return;

    connect(api, &C64ApiClient::configCategoriesReceived,
            this, &SystemPanel::onConfigCategoriesReceived, Qt::SingleShotConnection);
    api->fetchConfigCategories();
}

void SystemPanel::onConfigCategoriesReceived(const QStringList &categories)
{
    auto *api = m_connection->apiClient();
    if (!api)
        return;

    connect(api, &C64ApiClient::configReceived,
            this, &SystemPanel::onConfigReceived);

    for (const auto &cat : categories)
        api->fetchConfig(cat);
}

void SystemPanel::onConfigReceived(const QString &category, const QJsonObject &config)
{
    auto *catItem = new QTreeWidgetItem(m_configTree);
    catItem->setText(0, category);
    catItem->setFlags(catItem->flags() & ~Qt::ItemIsSelectable);

    // The config object has category as key, value is object of items
    QJsonObject items;
    if (config.contains(category))
        items = config[category].toObject();
    else
        items = config; // fallback

    QStringList keys = items.keys();
    keys.sort(Qt::CaseInsensitive);

    for (const auto &key : keys) {
        auto *item = new QTreeWidgetItem(catItem);
        item->setText(0, key);
        item->setText(1, items[key].toVariant().toString());
        item->setData(0, Qt::UserRole, category);
        item->setData(1, Qt::UserRole, key);
    }

    m_configStatus->setText(QStringLiteral("%1 categories loaded").arg(m_configTree->topLevelItemCount()));
}

void SystemPanel::onConfigItemClicked(QTreeWidgetItem *item, int)
{
    if (!item->parent())
        return; // category header

    QString category = item->data(0, Qt::UserRole).toString();
    QString key = item->data(1, Qt::UserRole).toString();
    QString currentValue = item->text(1);

    // Simple inline edit — fetch details first, then show editor
    // For now, use a simple input dialog
    auto *api = m_connection->apiClient();
    if (!api)
        return;

    // Fetch item details for proper editing
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(api, &C64ApiClient::configDetailReceived, this,
        [this, item, category, key, currentValue, conn, api](const QString &cat, const QString &itm, const QJsonObject &detail) {
            if (cat != category || itm != key)
                return;
            QObject::disconnect(*conn);

            // Check if it has a values list (dropdown)
            QJsonObject inner;
            if (detail.contains(category))
                inner = detail[category].toObject().value(key).toObject();
            else
                inner = detail;

            QStringList values;
            auto valArr = inner["values"].toArray();
            for (const auto &v : valArr)
                values.append(v.toString());

            QString newValue;
            if (!values.isEmpty()) {
                // Dropdown
                bool ok;
                newValue = QInputDialog::getItem(this, key, "Select value:", values,
                    values.indexOf(currentValue), false, &ok);
                if (!ok) return;
            } else {
                // Text field
                bool ok;
                newValue = QInputDialog::getText(this, key, "Value:",
                    QLineEdit::Normal, currentValue, &ok);
                if (!ok || newValue == currentValue) return;
            }

            api->setConfig(category, key, newValue);
            item->setText(1, newValue);
        });

    api->fetchConfigDetail(category, key);
}

void SystemPanel::onSaveToFlash()
{
    auto ret = QMessageBox::question(this, "Save to Flash",
        "Write current configuration to non-volatile memory?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes && m_connection->apiClient())
        m_connection->apiClient()->saveConfigToFlash();
}
