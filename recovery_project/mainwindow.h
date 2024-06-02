#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QProcess>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include<QListWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QClipboard>
#include <QGuiApplication>
#include<QListWidgetItem>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QTimer>
#include <QHeaderView>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void runCommand();
    void keyPressEvent(QKeyEvent *event);
    void selectOutputPath();
    void startRecovery();
    void updateProgressBar();
    void recoveryFinished(int exitCode);
    void cancelRecovery();
    void stopProgressBar();
    void readProcessOutput();

private:
    Ui::MainWindow *ui;
    QTableWidget *tableWidget;
    QListWidget *listWidget;
    QProgressBar *progressBar;
    QProcess *process;
    QTimer *timer;
    QPushButton *recoverButton;
    QPushButton *cancelButton;
    QLabel *progressLabel;
    QString outputPath;
};

#endif // MAINWINDOW_H
