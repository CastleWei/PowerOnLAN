#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMap>
#include <QMessageBox>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    settings("settings.ini", QSettings::IniFormat, this)
{
    ui->setupUi(this);

    udpSocket = new QUdpSocket(this);
    udpSocket->bind(udpPort, QUdpSocket::ShareAddress);
    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(gotUdpMsg()));

    // 初始化信息
    settings.setValue("appname",appname);
    settings.setValue("ver",appver);
    settings.setValue("description","settings file");

    myself.ip = syncSetting(settings, "host_ip", getMyIP().toIPv4Address()).toInt();
    myself.mac = syncSetting(settings, "host_mac", getMyMac()).toString();
    myself.name = syncSetting(settings, "host_name", myself.mac.right(6)).toString();  //默认本机名称为Mac地址后两组
    updateOnStart = syncSetting(settings, "update_on_start", true).toBool();
    alwaysUpdateOnStart = syncSetting(settings, "always_update_on_start", false).toBool();

    // 如果之前已经有已知设备则直接读取
//    QByteArray known_devices_ja_str = settings.value("known_devices_list").toByteArray();
//    QJsonArray known_devices_ja = QJsonDocument::fromJson(known_devices_ja_str).array();

//    if(!known_devices_ja.isEmpty()){
//        this->knownDevicesList.clear();
//        foreach (QJsonValue val, known_devices_ja)
//            knownDevicesList.append(KnownDevice::fromString(val.toString()));

//        ui->cmbTarget->clear();
//        foreach(const KnownDevice &d, knownDevicesList)
//            ui->cmbTarget->addItem(d.name);
//        emit ui->cmbTarget->activated(0);
//    }
    knownDevicesList.clear();
    int size = settings.beginReadArray("known_devices_list");
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        KnownDevice d;
        d.name = settings.value("name").toString();
        d.ip = settings.value("ip").toInt();
        d.mac = settings.value("mac").toString();
        knownDevicesList.append(d);
    }
    settings.endArray();

    ui->cmbTarget->clear();
    foreach(const KnownDevice &d, knownDevicesList)
        ui->cmbTarget->addItem(d.name);

    if(ui->cmbTarget->count()>0)
        emit ui->cmbTarget->activated(0); //注意currentIndex==-1时这样会越界

    // 启动时更新设备列表
    if(updateOnStart){
        emit ui->btnUpdateDevices->clicked();
    }

    // 显示信息到界面
    ui->edtMyName->setText(myself.name);
    ui->lblMyIP->setText(QHostAddress(myself.ip).toString());
    ui->lblMyMac->setText(myself.mac);

    // 上线通知
    sendJsonUdp(newJsonMsg(tr("%1 has gone online!").arg(myself.name)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::say(QString string)
{

    ui->listLog->addItem(QTime::currentTime().toString("m:ss.zzz  ") + string);
    ui->listLog->scrollToBottom();
}

void MainWindow::sendJsonUdp(QJsonObject json)
{

    udpSocket->writeDatagram(QJsonDocument(json).toJson(QJsonDocument::Compact)
                             ,QHostAddress::Broadcast,udpPort);
}

void MainWindow::gotUdpMsg()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        QHostAddress address;
        quint16 port;

        datagram.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(datagram.data(), datagram.size(), &address, &port);

        //排除 其他端口的广播 和 自己发出的广播
        if(port !=udpPort || myself.ip == address.toIPv4Address()) return;

//        say("ip: " + address.toString());
//        say(tr("recv: ") + datagram.data());

        QJsonObject json = QJsonDocument::fromJson(datagram).object();
        //确认信息来源及格式正确
        if (!json.isEmpty() && json.value("appname").toString()==appname)
        {
            //say("got msg");
            //解读指令
            QString msg = json.value("msg").toString();
            if(msg == "updating"){
                //若主机已收到自己上次的信息
                if(json.value("known_mac").toArray().contains(myself.mac))
                    return;

                //若没收到
                QJsonObject rep = newJsonMsg("reply_updating");
                rep.insert("mac",myself.mac);
                rep.insert("name",myself.name);
                sendJsonUdp(rep);
            }
            else if(msg == "reply_updating"){
                knownDevicesSet.insert(
                            KnownDevice(json.value("name").toString(),
                                        address.toIPv4Address(),
                                        json.value("mac").toString())
                            );
            }
            else if(msg == "sync"){
                //say("recv sync: "+datagram);
                this->knownDevicesList.clear();
                QJsonArray list = json.value("known_devices").toArray();
                foreach (QJsonValue val, list)
                    knownDevicesList.append(KnownDevice::fromString(val.toString()));

                ui->cmbTarget->clear();
                foreach(const KnownDevice &d, knownDevicesList)
                    ui->cmbTarget->addItem(d.name);
                emit ui->cmbTarget->activated(0);

                say(tr("sync with %1, update %2 devices")
                    .arg(json.value("name").toString())
                    .arg(list.count()));

                //TELL THE WORLD THAT I'VE UPDATED!!
                sendJsonUdp(newJsonMsg(tr("%1 has updated!").arg(myself.name)));
            }
            else if(msg == "shutdown"){
                QString mac = json.value("mac").toString();
                if (mac==myself.mac){
                    //***********
                    say(tr("got shutdown command.. shutting down..."));
                    sendJsonUdp(newJsonMsg(tr("%1 got shutdown command.").arg(myself.name)));
                    QProcess::execute("shutdown /s /t 3");
                    QTimer::singleShot(2000,this,SLOT(close()));
                }
            }
            else if(msg == "reset"){
                QString mac = json.value("mac").toString();
                if (mac==myself.mac){
                    //***********
                    say(tr("got reset command.. resetting..."));
                    sendJsonUdp(newJsonMsg(tr("%1 got reset command.").arg(myself.name)));
                    QProcess::execute("shutdown /r /t 3");
                    QTimer::singleShot(2000,this,SLOT(close()));
                }
            }
            else {
                //有其他直接发的广播消息，如上线、下线
                say(msg);
            }
        }
        else
            say("got other udp datagram");
    }
}

void MainWindow::continueUpdating()
{
    const int updatingTimes = 2; //设定重复搜索次数
    static int n = updatingTimes; //剩余次数
    static int lastCount = 0; //上次收集到的个数
    //（放心，static声明时赋的值只是初值）

    if(--n > 0 && knownDevicesSet.count() > lastCount)
    {
        //若上一次搜索设备数没有增加 且 没够刷新次数，则再搜一次

        QJsonObject json = newJsonMsg("updating");
        //告诉已知设备不重复发
        QJsonArray arr;
        foreach(const KnownDevice &d, knownDevicesSet)
            arr.append(d.mac);
        json.insert("known_mac", arr);

        lastCount = knownDevicesSet.count();
        sendJsonUdp(json); //再广播更新请求，确保没有落下某些机子

        //定时下一次刷新
        QTimer::singleShot(updatingDelay, this, SLOT(continueUpdating()));
    }
    else
    {
        //扫描完成
        n = updatingTimes; //重置扫描次数
        lastCount = 0;

        //以下利用QMap特性插入排序，按设备名称排序。该方法见qSort()文档
        QMap<QString, KnownDevice> sortingMap;
        foreach(const KnownDevice &d, knownDevicesSet)
            sortingMap.insert(d.name, d);
        sortingMap.insert(myself.name, myself); //记得包含自己
        knownDevicesList = sortingMap.values();

        ui->cmbTarget->clear();
        foreach(const KnownDevice &d, knownDevicesList)
            ui->cmbTarget->addItem(d.name);

        ui->btnUpdateDevices->setEnabled(true);
        emit ui->cmbTarget->activated(0);
        say(tr("found %1 devices.").arg(knownDevicesSet.count()));

        //向其他设备同步本次搜索的数据
        QJsonObject json = newJsonMsg("sync");
        json.insert("name",myself.name);
        QJsonArray arr;
        foreach(const KnownDevice &d, knownDevicesList)
            arr.append(d.toString());
        json.insert("known_devices", arr);
        //say("data to sync: " + QJsonDocument(json).toJson());
        sendJsonUdp(json);

        //定时，搜集列表同步情况**********//算了。。麻烦而且没有必要，不做了
        //QTimer::singleShot(updatingDelay, this, SLOT(continueSync());
    }
}

void MainWindow::on_btnUpdateDevices_clicked()
{
    ui->btnUpdateDevices->setEnabled(false);
    say(tr("Searching devices..."));
    knownDevicesSet.clear();

    sendJsonUdp(newJsonMsg("updating"));
    QTimer::singleShot(updatingDelay, this, SLOT(continueUpdating()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    //如果选项为一直启动更新，则下次启动时更新设为true，否则下次不更新
    settings.setValue("update_on_start", alwaysUpdateOnStart);

    //记录本次更新的已知设备，以免每一次启动都更新
//    QJsonArray arr;
//    foreach(const KnownDevice &d, knownDevicesList)
//        arr.append(d.toString());
//    QString arr_str = QJsonDocument(arr).toJson();
//    settings.setValue("known_devices_list", arr_str);
    settings.beginWriteArray("known_devices_list");
    settings.remove(""); //清空原有文件中的列表信息，否则设备变少的时候不会自己清除
    for(int i=0;i<knownDevicesList.count();i++){
        settings.setArrayIndex(i);
        const KnownDevice &d = knownDevicesList.at(i);
        settings.setValue("name",d.name);
        settings.setValue("ip",d.ip);
        settings.setValue("mac",d.mac);
    }
    settings.endArray();

    // 下线通知
    sendJsonUdp(newJsonMsg(tr("%1 has gone offline!").arg(myself.name)));

    QMainWindow::closeEvent(event);
}

void MainWindow::on_cmbTarget_activated(int index)
{
    if(index == -1 || index > knownDevicesList.count()){
        ui->lblTargetIP->setText("");
        ui->lblTargetMac->setText("");
        qDebug()<<"combobox index out of boundry";
        return;
    }
    else{
        KnownDevice d = knownDevicesList.at(index);
        ui->lblTargetIP->setText(QHostAddress(d.ip).toString());
        ui->lblTargetMac->setText(d.mac);
    }
}

void MainWindow::on_btnChangeMyName_clicked()
{
    if(ui->edtMyName->isReadOnly()){
        ui->btnChangeMyName->setText(tr("确定"));
        ui->edtMyName->setReadOnly(false);
        ui->edtMyName->selectAll();
        ui->edtMyName->setFocus();
    }
    else{
        QString name = ui->edtMyName->text().trimmed();
        if(name==""){
            QMessageBox::information(this,tr("错误"),tr("名字不能为空"));
            return;
        }
        if(name.contains(' ')){
            QMessageBox::information(this,tr("错误"),tr("名字不能含有空格"));
            return;
        }
        myself.name = name;
        settings.setValue("host_name",myself.name);
        ui->edtMyName->setText(name);
        ui->btnChangeMyName->setText(tr("更改"));
        ui->edtMyName->setReadOnly(true);
        say(tr("rename to \"%1\".. syncing with other devices").arg(name));
        on_btnUpdateDevices_clicked();
    }
}

void MainWindow::on_btnPowerOff_clicked()
{
    int i=ui->cmbTarget->currentIndex();
    if(i < 0) {
        say(tr("error: no known device"));
        return;
    }
    KnownDevice d = knownDevicesList.at(i);
    if(!KnownDevice::isValidMac(d.mac)){
        say(tr("error: invalid target device"));
        return;
    }
    QJsonObject jo = newJsonMsg("shutdown");
    jo.insert("mac",d.mac); //用mac地址来唯一识别某个设备
    sendJsonUdp(jo);
    say(tr("shutdown command sending to \"%1\"").arg(d.name));
}

void MainWindow::on_btnReset_clicked()
{
    int i=ui->cmbTarget->currentIndex();
    if(i < 0) {
        say(tr("error: no known device"));
        return;
    }
    KnownDevice d = knownDevicesList.at(i);
    if(!KnownDevice::isValidMac(d.mac)){
        say(tr("error: invalid target device"));
        return;
    }
    QJsonObject jo = newJsonMsg("reset");
    jo.insert("mac",d.mac); //用mac地址来唯一识别某个设备
    sendJsonUdp(jo);
    say(tr("resetting command sending to \"%1\"").arg(d.name));
}

void MainWindow::on_btnPowerOn_clicked()
{
    int i=ui->cmbTarget->currentIndex();
    if(i < 0) {
        say(tr("error: no known device"));
        return;
    }
    KnownDevice d = knownDevicesList.at(i);
    if(!KnownDevice::isValidMac(d.mac)){
        say(tr("error: invalid target device"));
        return;
    }

    //将mac地址转为二进制
    QStringList macs = d.mac.split(QRegExp("[:：-]"));
    qDebug()<<macs;
    QByteArray macByte;
    macByte.resize(6);
    for(int i=0;i<6;i++){
        bool ok;
        macByte[i] = macs.at(i).toUInt(&ok, 16);
        if(!ok){
            say(tr("error: converting mac error!"));
            return;
        }
    }

    //WOL开机的魔术封包
    QByteArray magicPacket;
    magicPacket.resize(6 * 17);
    //第一段是六个FF
    for(int i=0;i<6;i++)
        magicPacket[i] = 0xff;
    //接着是mac地址重复16次
    for(int i=1;i<17;i++)
        for(int j=0;j<6;j++)
            magicPacket[i*6+j] = macByte.at(j);
    qDebug()<<magicPacket.toHex();

    //!!不知道为什么从平板和笔记本（WiFi连接）上发不出这个包？？！！台式可以
    udpSocket->writeDatagram(magicPacket, QHostAddress::Broadcast,udpPort);
//    QTimer::singleShot(1000,this,SLOT(sendWolAgain())); //再来一次冗余
    say(tr("power on command to %1 has sent.").arg(d.name));
}
