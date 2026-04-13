#pragma once

#include <QMainWindow>
#include <QLabel>

class BasicEditorWidget;
class C64Connection;

class BasicEditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit BasicEditorWindow(C64Connection *connection, QWidget *parent = nullptr);

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void uploadToDevice();
    void onTemplateSelected(int index);

private:
    void updateTitle();
    void updateLineCount();

    C64Connection *m_connection;
    BasicEditorWidget *m_editor;
    QLabel *m_statusLabel;
    QLabel *m_lineCountLabel;

    QString m_currentFilePath;
    bool m_dirty = false;
};
