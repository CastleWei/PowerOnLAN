#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork>
#include <QHash>
#include <QList>
#include <QSettings>
#include "common.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void say(QString string);
    void sendJsonUdp(QJsonObject json);

private slots:
    void gotUdpMsg();
    void continueUpdating();
//    void sendWolAgain();
    void on_btnUpdateDevices_clicked();
    void on_cmbTarget_activated(int index);
    virtual void closeEvent(QCloseEvent *event);

    void on_btnChangeMyName_clicked();

    void on_btnPowerOff_clicked();

    void on_btnReset_clicked();

    void on_btnPowerOn_clicked();

private:
    Ui::MainWindow *ui;
    QSettings settings;

    QUdpSocket *udpSocket;

    const int updatingDelay = 1000;

    KnownDevice myself;

    bool updateOnStart;
    bool alwaysUpdateOnStart;

    QSet<KnownDevice> knownDevicesSet; //updating过程中搜到的设备
    QList<KnownDevice> knownDevicesList;
};

#endif // MAINWINDOW_H
