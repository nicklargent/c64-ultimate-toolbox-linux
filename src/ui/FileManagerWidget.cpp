#include "ui/FileManagerWidget.h"
#include "network/C64Connection.h"
#include "network/C64ApiClient.h"
#include "network/FtpClient.h"
#include "models/FtpFileEntry.h"
#include "app/Log.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QKeyEvent>

FileManagerWidget::FileManagerWidget(C64Connection *connection, QWidget *parent)
    : QWidget(parent)
    , m_connection(connection)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto *header = new QLabel("File System", this);
    header->setStyleSheet("font-weight: bold; font-size: 11pt; color: gray; padding: 4px 16px;");
    layout->addWidget(header);

    // Tree view
    m_model = new QStandardItemModel(this);
    m_model->setColumnCount(1);

    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setHeaderHidden(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragDropMode(QAbstractItemView::DropOnly);
    m_tree->setAcceptDrops(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(16);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    connect(m_tree, &QTreeView::expanded, this, &FileManagerWidget::onItemExpanded);
    connect(m_tree, &QTreeView::doubleClicked, this, &FileManagerWidget::onItemDoubleClicked);
    connect(m_tree, &QTreeView::customContextMenuRequested, this, &FileManagerWidget::onCustomContextMenu);
    connect(m_tree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { onSelectionChanged(); });

    layout->addWidget(m_tree, 1);

    // Status bar
    auto *statusBar = new QWidget(this);
    statusBar->setFixedHeight(24);
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 0, 8, 0);
    statusLayout->setSpacing(8);

    m_statusLabel = new QLabel("Connecting...", statusBar);
    m_statusLabel->setStyleSheet("font-size: 11pt; color: gray;");
    statusLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(statusBar);
    m_progressBar->setMinimumWidth(80);
    m_progressBar->setMaximumWidth(150);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->hide();
    statusLayout->addWidget(m_progressBar);

    layout->addWidget(statusBar);

    // Add root node
    auto *rootItem = new QStandardItem("C64");
    rootItem->setData("/", PathRole);
    rootItem->setData(true, IsDirRole);
    rootItem->setData(false, LoadedRole);
    rootItem->setEditable(false);

    // Add placeholder child so it's expandable
    auto *placeholder = new QStandardItem("Loading...");
    placeholder->setEnabled(false);
    placeholder->setEditable(false);
    rootItem->appendRow(placeholder);

    m_model->appendRow(rootItem);

    connectAndLoad();
}

void FileManagerWidget::connectAndLoad()
{
    auto *api = m_connection->apiClient();
    if (!api) {
        m_statusLabel->setText("Not connected to device");
        return;
    }

    // FTP client is owned by C64Connection
    m_ftpClient = m_connection->ftpClient();
    if (!m_ftpClient) {
        m_statusLabel->setText("FTP not available");
        return;
    }

    connect(m_ftpClient, &FtpClient::connected, this, &FileManagerWidget::onFtpConnected);
    connect(m_ftpClient, &FtpClient::directoryListed, this, &FileManagerWidget::onDirectoryListed);
    connect(m_ftpClient, &FtpClient::operationCompleted, this, &FileManagerWidget::onFtpOperationCompleted);
    connect(m_ftpClient, &FtpClient::operationFailed, this, &FileManagerWidget::onFtpOperationFailed);
    connect(m_ftpClient, &FtpClient::transferProgress, this, &FileManagerWidget::onTransferProgress);
}

void FileManagerWidget::onFtpConnected()
{
    m_statusLabel->setText("Connected");
    // Load root directory
    loadDirectory("/", m_model->item(0));
}

void FileManagerWidget::loadDirectory(const QString &path, QStandardItem *parentItem)
{
    if (!m_ftpClient)
        return;

    m_pendingLoadPath = path;
    m_pendingLoadItem = parentItem;
    m_ftpClient->listDirectory(path);
}

void FileManagerWidget::onDirectoryListed(const QString &path, const QList<FtpFileEntry> &entries)
{
    QStandardItem *parentItem = m_pendingLoadItem;
    if (!parentItem || m_pendingLoadPath != path) {
        // Try to find the item by path
        // For simplicity, just use pending
        return;
    }

    // Remove placeholder children
    parentItem->removeRows(0, parentItem->rowCount());

    for (const auto &entry : entries) {
        auto *item = new QStandardItem(entry.name);
        item->setData(entry.path, PathRole);
        item->setData(entry.isDirectory, IsDirRole);
        item->setData(static_cast<qulonglong>(entry.size), SizeRole);
        item->setData(false, LoadedRole);
        item->setEditable(false);

        if (entry.isDirectory) {
            // Add placeholder for expandability
            auto *placeholder = new QStandardItem("Loading...");
            placeholder->setEnabled(false);
            placeholder->setEditable(false);
            item->appendRow(placeholder);
        }

        parentItem->appendRow(item);
    }

    parentItem->setData(true, LoadedRole);

    int count = entries.size();
    m_statusLabel->setText(QStringLiteral("%1 item%2 in %3")
        .arg(count).arg(count == 1 ? "" : "s", parentItem->text()));

    m_pendingLoadItem = nullptr;
    m_pendingLoadPath.clear();
}

void FileManagerWidget::onItemExpanded(const QModelIndex &index)
{
    auto *item = m_model->itemFromIndex(index);
    if (!item)
        return;

    bool isDir = item->data(IsDirRole).toBool();
    bool loaded = item->data(LoadedRole).toBool();

    if (isDir && !loaded) {
        QString path = item->data(PathRole).toString();
        loadDirectory(path, item);
    }
}

void FileManagerWidget::onItemDoubleClicked(const QModelIndex &index)
{
    auto *item = m_model->itemFromIndex(index);
    if (!item)
        return;

    bool isDir = item->data(IsDirRole).toBool();
    if (isDir)
        return; // Tree handles expand/collapse

    QString name = item->text();
    QString path = item->data(PathRole).toString();
    QString ext = name.section('.', -1).toLower();

    if (ext == "prg") runFileByPath("prg", path, name);
    else if (ext == "sid") runFileByPath("sid", path, name);
    else if (ext == "mod") runFileByPath("mod", path, name);
    else if (ext == "crt") runFileByPath("crt", path, name);
    else if (ext == "d64" || ext == "d71" || ext == "d81" || ext == "g64" || ext == "g71")
        mountDriveA();
}

void FileManagerWidget::onCustomContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto *item = selectedItem();
    bool isDir = item && item->data(IsDirRole).toBool();
    bool isFile = item && !isDir;

    if (isFile) {
        QString ext = item->text().section('.', -1).toLower();
        QStringList diskExts = {"d64", "d71", "d81", "g64", "g71"};

        if (ext == "prg") {
            menu.addAction("Run", this, &FileManagerWidget::runPrg);
            menu.addAction("Load", this, &FileManagerWidget::loadPrg);
            menu.addSeparator();
        } else if (ext == "sid") {
            menu.addAction("Play", this, &FileManagerWidget::playSid);
            menu.addSeparator();
        } else if (ext == "mod") {
            menu.addAction("Play", this, &FileManagerWidget::playMod);
            menu.addSeparator();
        } else if (ext == "crt") {
            menu.addAction("Run", this, &FileManagerWidget::runCrt);
            menu.addSeparator();
        } else if (diskExts.contains(ext)) {
            menu.addAction("Mount on Drive A", this, &FileManagerWidget::mountDriveA);
            menu.addAction("Mount on Drive B", this, &FileManagerWidget::mountDriveB);
            menu.addSeparator();
        }

        menu.addAction("Download", this, &FileManagerWidget::downloadSelected);
        menu.addSeparator();
    }

    menu.addAction("Upload Files...", this, &FileManagerWidget::uploadFiles);
    menu.addAction("New Folder", this, &FileManagerWidget::createNewFolder);

    auto *diskMenu = menu.addMenu("New Disk Image");
    diskMenu->addAction("D64 Image...", this, &FileManagerWidget::createD64);
    diskMenu->addAction("D71 Image...", this, &FileManagerWidget::createD71);
    diskMenu->addAction("D81 Image...", this, &FileManagerWidget::createD81);
    diskMenu->addAction("DNP Image...", this, &FileManagerWidget::createDnp);

    if (item) {
        menu.addSeparator();
        menu.addAction("Copy Path", this, &FileManagerWidget::copyPath);
        menu.addAction("Rename", this, &FileManagerWidget::renameSelected);
        menu.addAction("Delete", this, &FileManagerWidget::deleteSelected);
    }

    menu.addSeparator();
    menu.addAction("Refresh", this, &FileManagerWidget::refreshDirectory);

    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void FileManagerWidget::onSelectionChanged()
{
    // Could show file size in status bar for selected file
}

// --- FTP callbacks ---

void FileManagerWidget::onFtpOperationCompleted(const QString &operation)
{
    m_statusLabel->setText(operation + " completed");
    m_progressBar->hide();

    // Refresh after modifying operations
    if (operation == "createDirectory" || operation == "deleteFile" ||
        operation == "deleteDirectory" || operation == "rename" ||
        operation == "uploadFile" || operation == "uploadDirectory") {
        refreshDirectory();
    }
}

void FileManagerWidget::onFtpOperationFailed(const QString &operation, const QString &error)
{
    m_statusLabel->setText(QStringLiteral("%1 failed: %2").arg(operation, error));
    m_progressBar->hide();
}

void FileManagerWidget::onTransferProgress(qint64 transferred, qint64 total)
{
    m_progressBar->show();
    if (total > 0) {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(static_cast<int>(transferred * 100 / total));
    } else {
        m_progressBar->setRange(0, 0); // indeterminate
    }
}

// --- Helpers ---

QStandardItem *FileManagerWidget::selectedItem() const
{
    auto idx = m_tree->currentIndex();
    return idx.isValid() ? m_model->itemFromIndex(idx) : nullptr;
}

QStandardItem *FileManagerWidget::selectedDirItem() const
{
    auto *item = selectedItem();
    if (!item)
        return m_model->item(0); // root

    if (item->data(IsDirRole).toBool())
        return item;

    // Parent of selected file
    return item->parent() ? item->parent() : m_model->item(0);
}

QString FileManagerWidget::selectedPath() const
{
    auto *item = selectedItem();
    return item ? item->data(PathRole).toString() : "/";
}

QString FileManagerWidget::selectedDirPath() const
{
    auto *item = selectedDirItem();
    return item ? item->data(PathRole).toString() : "/";
}

// --- Actions ---

void FileManagerWidget::uploadFiles()
{
    QStringList paths = QFileDialog::getOpenFileNames(this, "Upload Files");
    if (paths.isEmpty())
        return;

    QString targetDir = selectedDirPath();
    for (const auto &localPath : paths) {
        QString fileName = QFileInfo(localPath).fileName();
        QString remotePath = targetDir;
        if (!remotePath.endsWith('/')) remotePath += '/';
        remotePath += fileName;

        m_statusLabel->setText("Uploading " + fileName + "...");
        m_ftpClient->uploadFile(localPath, remotePath);
    }
}

void FileManagerWidget::downloadSelected()
{
    auto *item = selectedItem();
    if (!item || item->data(IsDirRole).toBool())
        return;

    QString name = item->text();
    QString remotePath = item->data(PathRole).toString();

    QString localPath = QFileDialog::getSaveFileName(this, "Download", name);
    if (localPath.isEmpty())
        return;

    m_statusLabel->setText("Downloading " + name + "...");
    m_ftpClient->downloadFile(remotePath, localPath);
}

void FileManagerWidget::deleteSelected()
{
    auto *item = selectedItem();
    if (!item || !item->data(PathRole).isValid())
        return;

    QString name = item->text();
    auto ret = QMessageBox::question(this, "Delete",
        QStringLiteral("Delete %1?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    QString path = item->data(PathRole).toString();
    if (item->data(IsDirRole).toBool())
        m_ftpClient->deleteDirectory(path);
    else
        m_ftpClient->deleteFile(path);
}

void FileManagerWidget::renameSelected()
{
    auto *item = selectedItem();
    if (!item)
        return;

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename", "New name:",
        QLineEdit::Normal, item->text(), &ok);
    if (!ok || newName.isEmpty() || newName == item->text())
        return;

    QString oldPath = item->data(PathRole).toString();
    QString parentPath = oldPath.section('/', 0, -2);
    if (parentPath.isEmpty()) parentPath = "/";
    QString newPath = parentPath + "/" + newName;

    m_ftpClient->rename(oldPath, newPath);
}

void FileManagerWidget::createNewFolder()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:",
        QLineEdit::Normal, {}, &ok);
    if (!ok || name.isEmpty())
        return;

    QString dirPath = selectedDirPath();
    if (!dirPath.endsWith('/')) dirPath += '/';
    m_ftpClient->createDirectory(dirPath + name);
}

void FileManagerWidget::copyPath()
{
    QString path = selectedPath();
    QApplication::clipboard()->setText(path);
    m_statusLabel->setText("Copied: " + path);
}

void FileManagerWidget::refreshDirectory()
{
    auto *dirItem = selectedDirItem();
    if (!dirItem)
        dirItem = m_model->item(0);

    dirItem->setData(false, LoadedRole);
    loadDirectory(dirItem->data(PathRole).toString(), dirItem);
}

// --- File runners ---

void FileManagerWidget::runFileByPath(const QString &ext, const QString &path, const QString &name)
{
    auto *api = m_connection->apiClient();
    if (!api)
        return;

    m_statusLabel->setText("Running " + name + "...");

    if (ext == "prg") api->runPrgFromPath(path);
    else if (ext == "sid") api->playSidFromPath(path);
    else if (ext == "mod") api->playModFromPath(path);
    else if (ext == "crt") api->runCrtFromPath(path);
}

void FileManagerWidget::runPrg()
{
    auto *item = selectedItem();
    if (item) runFileByPath("prg", item->data(PathRole).toString(), item->text());
}

void FileManagerWidget::loadPrg()
{
    auto *item = selectedItem();
    if (!item || !m_connection->apiClient()) return;
    m_statusLabel->setText("Loading " + item->text() + "...");
    m_connection->apiClient()->loadPrgFromPath(item->data(PathRole).toString());
}

void FileManagerWidget::playSid()
{
    auto *item = selectedItem();
    if (item) runFileByPath("sid", item->data(PathRole).toString(), item->text());
}

void FileManagerWidget::playMod()
{
    auto *item = selectedItem();
    if (item) runFileByPath("mod", item->data(PathRole).toString(), item->text());
}

void FileManagerWidget::runCrt()
{
    auto *item = selectedItem();
    if (item) runFileByPath("crt", item->data(PathRole).toString(), item->text());
}

void FileManagerWidget::mountDriveA()
{
    auto *item = selectedItem();
    if (!item || !m_connection->apiClient()) return;
    m_statusLabel->setText("Mounting " + item->text() + " on Drive A...");
    m_connection->apiClient()->mountDrive("a", item->data(PathRole).toString());
}

void FileManagerWidget::mountDriveB()
{
    auto *item = selectedItem();
    if (!item || !m_connection->apiClient()) return;
    m_statusLabel->setText("Mounting " + item->text() + " on Drive B...");
    m_connection->apiClient()->mountDrive("b", item->data(PathRole).toString());
}

// --- Disk image creation ---

void FileManagerWidget::createD64() { createDiskImage("d64", "D64"); }
void FileManagerWidget::createD71() { createDiskImage("d71", "D71"); }
void FileManagerWidget::createD81() { createDiskImage("d81", "D81"); }
void FileManagerWidget::createDnp() { createDiskImage("dnp", "DNP"); }

void FileManagerWidget::createDiskImage(const QString &ext, const QString &label)
{
    auto *api = m_connection->apiClient();
    if (!api)
        return;

    bool ok;
    QString name = QInputDialog::getText(this, "New " + label + " Image",
        "Filename:", QLineEdit::Normal, "Untitled." + ext, &ok);
    if (!ok || name.isEmpty())
        return;

    if (!name.toLower().endsWith("." + ext))
        name += "." + ext;

    QString dirPath = selectedDirPath();
    if (!dirPath.endsWith('/')) dirPath += '/';
    QString fullPath = dirPath + name;

    if (ext == "d64") {
        int tracks = QInputDialog::getInt(this, "D64 Tracks", "Number of tracks:", 35, 35, 40, 5, &ok);
        if (!ok) return;
        api->createD64(fullPath, tracks);
    } else if (ext == "d71") {
        api->createD71(fullPath);
    } else if (ext == "d81") {
        api->createD81(fullPath);
    } else if (ext == "dnp") {
        int tracks = QInputDialog::getInt(this, "DNP Tracks", "Number of tracks (1-255):", 255, 1, 255, 1, &ok);
        if (!ok) return;
        api->createDnp(fullPath, tracks);
    }

    m_statusLabel->setText("Creating " + name + "...");
}

QString FileManagerWidget::formatFileSize(uint64_t bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024ULL * 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
}
