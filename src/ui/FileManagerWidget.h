#pragma once

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QLabel>
#include <QProgressBar>
#include <QMenu>

#include "models/FtpFileEntry.h"

class C64Connection;
class FtpClient;

class FileManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileManagerWidget(C64Connection *connection, QWidget *parent = nullptr);

private slots:
    void onItemExpanded(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);
    void onCustomContextMenu(const QPoint &pos);
    void onSelectionChanged();

    // FTP result handlers
    void onFtpConnected();
    void onDirectoryListed(const QString &path, const QList<FtpFileEntry> &entries);
    void onFtpOperationCompleted(const QString &operation);
    void onFtpOperationFailed(const QString &operation, const QString &error);
    void onTransferProgress(qint64 transferred, qint64 total);

    // Actions
    void uploadFiles();
    void downloadSelected();
    void deleteSelected();
    void renameSelected();
    void createNewFolder();
    void copyPath();

    // File runners
    void runPrg();
    void loadPrg();
    void playSid();
    void playMod();
    void runCrt();
    void runDisk();
    void mountDriveA();
    void mountDriveB();
    void viewTextFile();

    // Disk image creation
    void createD64();
    void createD71();
    void createD81();
    void createDnp();

    void refreshDirectory();

private:
    enum ItemRole {
        PathRole = Qt::UserRole,
        IsDirRole,
        SizeRole,
        LoadedRole,
    };

    void connectAndLoad();
    void loadDirectory(const QString &path, QStandardItem *parentItem);
    QStandardItem *selectedItem() const;
    QStandardItem *selectedDirItem() const;
    QString selectedPath() const;
    QString selectedDirPath() const;
    void runFileByPath(const QString &ext, const QString &path, const QString &name);
    void createDiskImage(const QString &ext, const QString &label);
    static QString formatFileSize(uint64_t bytes);

    C64Connection *m_connection;
    FtpClient *m_ftpClient = nullptr;

    QTreeView *m_tree;
    QStandardItemModel *m_model;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;

    // Track pending directory load target
    QString m_pendingLoadPath;
    QStandardItem *m_pendingLoadItem = nullptr;
};
