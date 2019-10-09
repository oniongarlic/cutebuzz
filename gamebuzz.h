#ifndef GameBuzz_H
#define GameBuzz_H

#include <QObject>
#include <QtNetwork>
#include <QSocketNotifier>
#include <QMap>
#include <QPair>
#include <QHash>
#include <QRegularExpression>
#include <libudev.h>

class GameBuzz : public QObject
{
    Q_OBJECT    
    Q_PROPERTY(bool hasDevice READ hasDevice NOTIFY hasDeviceChanged)

public:
    explicit GameBuzz(QObject *parent = nullptr);
    ~GameBuzz();

    enum Keys {
        PlayerBlue,
        PlayerOrange,
        PlayerGreen,
        PlayerYellow,
    };
    Q_ENUMS(Keys)

    bool hasDevice();

    Q_INVOKABLE bool start(const QString device);
    Q_INVOKABLE bool stop();
    Q_INVOKABLE void disableDevice(bool disable);      
    Q_INVOKABLE bool led(int player, bool on);

signals:
    void hasDeviceChanged(bool hasDevice);

    void buzzer(uint player);
    void button(uint player, Keys color);

private slots:
    void readDevice();
    void readUdevMonitor();    

private:
     bool openDevice();
     void closeDevice();

     QStringList findInputDevices();
     void enableUdevMonitoring();
     bool isSuitableInputDevice(int fd);

     bool m_enabled;

     QSocketNotifier *m_notifier;
     QSocketNotifier *m_ev_notifier;

     QString m_device;

     bool have_udev;
     struct udev *udev;
     struct udev_monitor *mon;
     struct libevdev *edev;
     int udev_fd;
     QSocketNotifier *m_udev_notifier;
     int input_fd;
     QString m_buffer;

     // Leds
     QMap <int, QString> m_ledmap;
};

#endif // GameBuzz_H
