#ifndef COMMON_H
#define COMMON_H

#include <QtNetwork>
#include <QJsonObject>
#include <QSettings>

// The app name and version number to comfirm when communicating in JSON
extern const char *appname;
extern const int appver;

extern const int udpPort;

//**************************

class KnownDevice{
public:
    QString name;
    quint32 ip;
    QString mac;

    KnownDevice();
    KnownDevice(QString _name, quint32 _ip, QString _mac);

    QString toString() const;
    static bool isValidMac(QString mac);
    static KnownDevice fromString(QString str);
};

bool operator ==(const KnownDevice &d1, const KnownDevice &d2);
uint qHash(const KnownDevice &key, uint seed);

//***************************

QJsonObject newJsonMsg(QString msg);

QHostAddress getMyIP();
QString getMyMac();

QVariant syncSetting(QSettings &settings,
                     const QString &key,
                     const QVariant &defaultValue = QVariant());

#endif // COMMON_H

/****************************
 * 经验汇总：    //<tag:exp>
 * 1.列表的排序：
 *     QList不自带排序函数，排序可以用<QtAlgorithm>的qSort()。
 * 但该函数已Obsolete，改用stl中的qsort()，参数和用法完全一样。
 * 不过如果用于自定义类型的排序，编排比较函数不是很方便直观（qt中默认
 * 不开启c++11，所以没有lambda表达式。不过要开启也很方便），文档中
 * 建议使用QMap的特性，将列表对象依次插入即完成排序。实例详见本项目
 * 中MainWindow::continueUpdating()的实现
 *
 * 2.函数内static变量的使用
 *     static变量声明时的赋值符号只赋初值，不用担心第二次执行到此处
 * 时会被重新复制覆盖。详见MainWindow::continueUpdating()的实现。
 *
 * 3.QValue储存自定义对象时需在class的头文件中类声明后加上如下的宏
 *     Q_DECLARE_METATYPE(MyClass)
 *
 * 4.QSet储存自定义类
 *     QSet储存的类需要是可复制的（有默认构造函数、复制构造函数、
 * 赋值=号），且在类声明外声明：
 * operator==(const MyClass &obj1, const MyClass &obj2);
 * uint qHash(const MyClass &obj, uint seed);
 *
 * 5.声明类的static函数时，头文件的类声明里的函数声明里加static，
 * 而cpp里的定义不加static
 *
 * 6.编译错误：discards qualifiers
 *     是在定义为const的变量被用作了没有声明const的函数实参
 *
 * 7.编译错误：multiple definition
 *     排查是否确实重复定义、头文件中函数声明是否包括在条件编译内、
 * 清理已编译出的.o文件重新生成。
 *     如果仍有问题，是因为自定义的头文件的内容没有将声明和定义分开
 * 写在.h和.cpp中，要确保.h中没有定义只有声明。如果是全局变量，在.h中
 * 声明 extern int globalVar;，在cpp中定义int globalVar=0;
 *
 * 8.JsonValue的字符串里不能有转义字符？
 *     （未经验证在传入的字符串里有"\t"没有转换成tab，而是直接出来\t
 *
****************************/
