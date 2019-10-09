#include "gamebuzz.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <QTextStream>
#include <QDebug>
#include <QFile>
#include <QDirIterator>

#include <errno.h>

GameBuzz::GameBuzz(QObject *parent) :
    QObject(parent),
    m_enabled(true),
    input_fd(-1)
{
    m_device="";
    m_buffer.clear();

    QDirIterator it("/sys/class/leds", QStringList() << "*buzz?", QDir::NoFilter, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString lpath = it.next();
        QChar nc=lpath.back();
        int n=nc.digitValue();
        if (n==-1)
            continue;

        m_ledmap.insert(n, lpath);
    }
    qDebug() << "LEDS" << m_ledmap;
}

bool GameBuzz::start(const QString device) {
    QStringList devices;

    m_device=device;

    udev = udev_new();
    if (!udev) {        
        qWarning("Failed to init udev, unable to probe or monitor for buzz device changes");
        have_udev=false;        
    } else {
        have_udev=true;
        enableUdevMonitoring();
    }

    if (m_device.isEmpty() && have_udev) {
        devices=findInputDevices();
        if (devices.count()>0) {
            m_device=devices.at(0);
            qDebug() << "Probed device: " << m_device;
        }
    }

    if (!m_device.isEmpty())
        return openDevice();

    emit hasDeviceChanged(false);

    return false;
}

bool GameBuzz::stop()
{
    closeDevice();

    return true;
}

GameBuzz::~GameBuzz() {
    closeDevice();
    if (have_udev) {
        udev_monitor_unref(mon);
        mon=nullptr;
    }
}

void GameBuzz::disableDevice(bool disable) {
    m_enabled=!disable;
}


void GameBuzz::enableUdevMonitoring() {
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
    udev_monitor_enable_receiving(mon);
    udev_fd = udev_monitor_get_fd(mon);
    m_udev_notifier = new QSocketNotifier(udev_fd, QSocketNotifier::Read, this);
    connect(m_udev_notifier, SIGNAL(activated(int)), this, SLOT(readUdevMonitor()));
}

bool GameBuzz::isSuitableInputDevice(int fd) {
    struct libevdev *evdev = libevdev_new();
    bool ret=false;
    int err;

    err = libevdev_set_fd(evdev, fd);
    if (err<0)
        return ret;

    if (libevdev_has_event_code(evdev, EV_KEY, BTN_TRIGGER_HAPPY1) && libevdev_has_event_code(evdev, EV_KEY, BTN_TRIGGER_HAPPY20) ) {
        qDebug() << "Is very trigger happy, probably a buzz!";
        ret=true;
    }

    libevdev_free(evdev);

    return ret;
}

QStringList GameBuzz::findInputDevices() {
    QStringList foundDevices;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    if (!have_udev)
        return foundDevices;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path;
        const char *devpath;
        const char *pdevss;
        struct udev_device *dev;
        struct udev_device *pdev;
        int fd;

        path=udev_list_entry_get_name(dev_list_entry);
        dev=udev_device_new_from_syspath(udev, path);
        devpath=udev_device_get_devnode(dev);
        if (!devpath)
            continue;

        // Skip non-usb devices
        pdev=udev_device_get_parent_with_subsystem_devtype(dev, "usb", NULL);
        // printDevice(pdev);
        pdevss=udev_device_get_subsystem(pdev);
        if (QString(pdevss)!="usb") {
            udev_device_unref(dev);
            continue;
        }

        fd=open(devpath, O_RDONLY);
        if (fd<0) {
            perror("Open failed");
            qDebug() << "Failed to open device for probe " << devpath;
            continue;
        }

        if (isSuitableInputDevice(fd))
            foundDevices << devpath;

        udev_device_unref(dev);
        close(fd);
    }

    qDebug() << "Found devices: "  << foundDevices;

    return foundDevices;
}

void GameBuzz::readUdevMonitor() {
    struct udev_device *dev;

    dev = udev_monitor_receive_device(mon);
    if (dev) {
        const char *action=udev_device_get_action(dev);
        const char *devpath=udev_device_get_devnode(dev);

        if (input_fd<0 && strcmp(action, "add")==0 && devpath) {
            qDebug() << "New device, checking: [" << devpath;
            int fd=open(devpath, O_RDONLY);
            if (fd>0) {
                bool isKbd=isSuitableInputDevice(fd);
                close(fd);

                if (isKbd) {
                    qDebug("Looks like a keyboard");
                    m_device=devpath;
                    openDevice();
                }
            } else {
                qWarning("Failed to open new device, check permissions.");
            }
        }
        udev_device_unref(dev);
    } else {
        qWarning("No Device from receive_device(). An error occured.");
    }
}

bool GameBuzz::openDevice() {
    const char *tmp = m_device.toLocal8Bit().data();
    int r=0;

    if (input_fd>0) {
        qWarning("Input device already open");
        return false;
    }

    input_fd=open(tmp, O_RDONLY | O_NONBLOCK);
    if (input_fd<0) {
        qWarning() << "Failed to open input device: " << m_device;
        return false;
    }
    qDebug() << "Input device opened: " << m_device;

    r=libevdev_new_from_fd(input_fd, &edev);
    if (r<0) {
        qWarning("Failed to create new evdev");
        close(input_fd);
        input_fd=-1;
        return false;
    }

    libevdev_grab(edev, LIBEVDEV_GRAB);

    m_ev_notifier = new QSocketNotifier(input_fd, QSocketNotifier::Read, this);
    connect(m_ev_notifier, SIGNAL(activated(int)), this, SLOT(readDevice()));
    emit hasDeviceChanged(true);

    return true;
}

bool GameBuzz::hasDevice() {
    return (input_fd>-1) ? false : true;
}

void GameBuzz::closeDevice() {
    if (input_fd>0) {
        qDebug("Closing input device");
        m_ev_notifier->disconnect();
        libevdev_grab(edev, LIBEVDEV_UNGRAB);
        close(input_fd);
        libevdev_free(edev);
        input_fd=-1;
        emit hasDeviceChanged(hasDevice());
    } else {
        qDebug("Device not open, doing nothing.");
    }
}

void GameBuzz::readDevice() {
    struct input_event ev;
    int r;

    do {
        r = libevdev_next_event(edev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (r!=LIBEVDEV_READ_STATUS_SUCCESS) {
            if (r==-EINTR || r == -EAGAIN) {
                return;
            }
            if (r==-ENODEV) {
                qWarning("Device lost, closing");
                closeDevice();
            }
            qWarning() << "libevdev read: " << r;
            return;
        }

        if (libevdev_event_is_type(&ev, EV_SYN))
            continue;

        if (!m_enabled)
            continue;

        if (libevdev_event_is_type(&ev, EV_KEY) && ev.value==0) {
            qDebug() << "Trigger: " << ev.code << BTN_TRIGGER_HAPPY1;
            switch (ev.code) {
            // Player 1
            case BTN_TRIGGER_HAPPY1:
                emit buzzer(1);
                break;
            case BTN_TRIGGER_HAPPY2:
                emit button(1, PlayerYellow);
                break;
            case BTN_TRIGGER_HAPPY3:
                emit button(1, PlayerGreen);
                break;
            case BTN_TRIGGER_HAPPY4:
                emit button(1, PlayerOrange);
                break;
            case BTN_TRIGGER_HAPPY5:
                emit button(1, PlayerBlue);
                break;

                // Player 2
            case BTN_TRIGGER_HAPPY6:
                emit buzzer(2);
                break;
            case BTN_TRIGGER_HAPPY7:
                emit button(2, PlayerYellow);
                break;
            case BTN_TRIGGER_HAPPY8:
                emit button(2, PlayerGreen);
                break;
            case BTN_TRIGGER_HAPPY9:
                emit button(2, PlayerOrange);
                break;
            case BTN_TRIGGER_HAPPY10:
                emit button(2, PlayerBlue);
                break;
                // Player 3
            case BTN_TRIGGER_HAPPY11:
                emit buzzer(3);
                break;
            case BTN_TRIGGER_HAPPY12:
                emit button(3, PlayerYellow);
                break;
            case BTN_TRIGGER_HAPPY13:
                emit button(3, PlayerGreen);
                break;
            case BTN_TRIGGER_HAPPY14:
                emit button(3, PlayerOrange);
                break;
            case BTN_TRIGGER_HAPPY15:
                emit button(3, PlayerBlue);

                break;
            case BTN_TRIGGER_HAPPY16:
                emit buzzer(4);
                break;
            case BTN_TRIGGER_HAPPY17:
                emit button(4, PlayerYellow);
                break;
            case BTN_TRIGGER_HAPPY18:
                emit button(4, PlayerGreen);
                break;
            case BTN_TRIGGER_HAPPY19:
                emit button(4, PlayerOrange);
                break;
            case BTN_TRIGGER_HAPPY20:
                emit button(4, PlayerBlue);
                break;
            default:
                qWarning() << "Unhandled scancode: " << ev.code;
            }

        }
    } while (r==LIBEVDEV_READ_STATUS_SUCCESS);
}

bool GameBuzz::led(int player, bool on)
{
    bool ret=true;

    if (player<1 || player>4)
        return false;

    QString lpath=m_ledmap.value(player);
    QFile f(lpath+"/brightness");
    if (f.open(QIODevice::WriteOnly)==false) {
        return false;
    }
    if (f.write(on ? "1" : "0")==-1) {
        ret=false;
    }
    f.close();

    return ret;
}

