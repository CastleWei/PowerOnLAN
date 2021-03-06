#ifndef COMMON_H
#define COMMON_H

#include <QtNetwork>
#include <QJsonObject>

// The app name and version number to comfirm when communicating in JSON
const char *appname = "PowerOnLAN";
const int appver = 1;

const int udpPort = 45445;

//**************************

struct KnownDevice{
    QString name;
    quint32 ip;
    QString mac;

    KnownDevice(){
        name = "";
        ip = 0;
        mac = "";
    }

    KnownDevice(QString _name, quint32 _ip, QString _mac){

        _name = _name.trimmed();
        _mac = _mac.trimmed();

        if(_name!="" && _ip!=0 && isValidMac(_mac)){
            this->name = _name;
            this->ip = _ip;
            this->mac = _mac;
        }
    }

    QString toString() const {
        //储存时用tab分隔
        return name + "\t" + QHostAddress(ip).toString() + "\t" + mac;
    }

    static bool isValidMac(QString mac){
        QRegExp rx("([A-Fa-f0-9]{2}[:：-]){5}[A-Fa-f0-9]{2}"); //带分隔符
        QRegExp rx2("([A-Fa-f0-9]{12}"); //无分隔符

        mac = mac.trimmed();
        return rx.exactMatch(mac)||rx2.exactMatch(mac);
    }

    static KnownDevice fromString(QString str, bool *ok = nullptr){
        QStringList lst = str.split('\t'); //用tab分隔
        if (lst.count() == 3){
            KnownDevice d;
            d.name = lst[0].trimmed();
            d.ip = QHostAddress(lst[1].trimmed()).toIPv4Address();
            d.mac = lst[2].trimmed();

            if(d.name!="" && d.ip!=0 && isValidMac(str)){
                *ok = true;
                return d;
            }
        }

        //若上面匹配不成功
        *ok = false;
        return KnownDevice();
    }
};

bool operator ==(const KnownDevice &d1, const KnownDevice &d2)
{
    return d1.name == d2.name
            && d1.ip == d2.ip
            && d1.mac == d2.mac;
}

uint qHash(const KnownDevice &key, uint seed)
{
    return qHash(key.name, seed)
            ^ qHash(key.mac, seed)
            ^ key.ip;
}

//***************************

QJsonObject newJsonMsg(QString msg){
    QJsonObject json;
    json.insert(QString("appname"),QJsonValue(appname));
    json.insert(QString("appver"),QJsonValue(appver));
    json.insert(QString("msg"),QJsonValue(msg));

    return json;
}

QHostAddress getMyIP()
{
  QList<QHostAddress> AddressList = QNetworkInterface::allAddresses();
  QHostAddress result;
  foreach(QHostAddress address, AddressList){
      if(address.protocol() == QAbstractSocket::IPv4Protocol &&
         address != QHostAddress::Null &&
         address != QHostAddress::LocalHost){
          if (address.toString().contains("127.0.")){
            continue;
          }
          result = address;
          break;
      }
  }
  return result;
}

QString getMyMac()
{
    QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();// 获取所有网络接口列表
    int nCnt = nets.count();
    QString strMacAddr = "";
    for(int i = 0; i < nCnt; i ++)
    {
        // 如果此网络接口被激活并且正在运行并且不是回环地址，则就是我们需要找的Mac地址
        if(nets[i].flags().testFlag(QNetworkInterface::IsUp)
                && nets[i].flags().testFlag(QNetworkInterface::IsRunning)
                && !nets[i].flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            strMacAddr = nets[i].hardwareAddress();
            break;
        }
    }
    return strMacAddr;
}

#endif // COMMON_H
