/*
 * Copyright 2018 Roman Gilg <subdiff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kwin_wl_backend.h"
#include "kwin_wl_device.h"

#include <algorithm>

//#include <KLocalizedString>

#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusReply>
#include <QStringList>
#include <QDBusConnection>

#include "logging.h"

KWinWaylandBackend::KWinWaylandBackend(QObject *parent) :
    InputBackend(parent)
{
    m_mode = InputBackendMode::KWinWayland;  //服务端为KWinWayland.

    m_deviceManager = new QDBusInterface (QStringLiteral("org.kde.KWin"), //提供的dbus接口
                                          QStringLiteral("/org/kde/KWin/InputDevice"),
                                          QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                          QDBusConnection::sessionBus(),
                                          this);

    findDevices();

    //如果有设备增加，调用onDeviceAdded函数.
    m_deviceManager->connection().connect(QStringLiteral("org.kde.KWin"), //连结这些dbus server
                                          QStringLiteral("/org/kde/KWin/InputDevice"),
                                          QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                          QStringLiteral("deviceAdded"),
                                          this,
                                          SLOT(onDeviceAdded(QString)));

    //如果有设备删除，调用onDeviceRemoved函数.
    m_deviceManager->connection().connect(QStringLiteral("org.kde.KWin"),
                                          QStringLiteral("/org/kde/KWin/InputDevice"),
                                          QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                          QStringLiteral("deviceRemoved"),
                                          this,
                                          SLOT(onDeviceRemoved(QString)));
}

KWinWaylandBackend::~KWinWaylandBackend() //析构函数
{
    qDeleteAll(m_devices);
    delete m_deviceManager;
}

void KWinWaylandBackend::findDevices()  //通过changeSignal获取全部的input设备。
{
    QStringList devicesSysNames;
    const QVariant reply = m_deviceManager->property("devicesSysNames");
    if (reply.isValid()) {
        qCDebug(KCM_MOUSE) << "Devices list received successfully from KWin.";
        devicesSysNames = reply.toStringList();
    }
    else {
        qCCritical(KCM_MOUSE) << "Error on receiving device list from KWin.";
        //m_errorString = i18n("Querying input devices failed. Please reopen this settings module.");
        return;
    }

    for (QString sn : devicesSysNames) {
        qDebug()<<"sn==="<<sn;
        QDBusInterface deviceIface(QStringLiteral("org.kde.KWin"),
                                    QStringLiteral("/org/kde/KWin/InputDevice/") + sn,  //sn为device，如event0等。
                                    QStringLiteral("org.kde.KWin.InputDevice"),
                                    QDBusConnection::sessionBus(),
                                    this);
        QVariant reply = deviceIface.property("pointer"); //获取pointer属性。
        qDebug()<<"reply 11=="<<reply;
        if (reply.isValid() && reply.toBool()) {
            //reply = deviceIface.property("touchpad");
            //qDebug()<<"reply 22=="<<reply;
            //if (reply.isValid() && reply.toBool()) {
            //    continue;
            //}
            qDebug()<<"name=="<<deviceIface.property("name");
            KWinWaylandDevice* dev = new KWinWaylandDevice(sn); //获取某个dev。
            dev->setLeftHanded(1);
            qDebug()<<"now is"<<dev->isLeftHanded();
            if (!dev->init()) { //初始化失败。
                qDebug() << "Error on creating device object" << sn;
                //m_errorString = i18n("Critical error on reading fundamental device infos of %1.", sn);
                return;
            }
            qDebug()<<"";
            m_devices.append(dev); //将dev添加到m_devices
            qCDebug(KCM_MOUSE).nospace() <<  "Device found: " <<  dev->name() << " (" << dev->sysName() << ")";
        }
    }
}

bool KWinWaylandBackend::applyConfig()
{
    return std::all_of(m_devices.constBegin(), m_devices.constEnd(),
        [] (QObject *t) { return static_cast<KWinWaylandDevice*>(t)->applyConfig(); });
}

bool KWinWaylandBackend::getConfig()
{
    return std::all_of(m_devices.constBegin(), m_devices.constEnd(),
        [] (QObject *t) { return static_cast<KWinWaylandDevice*>(t)->getConfig(); });
}

bool KWinWaylandBackend::getDefaultConfig()
{
    return std::all_of(m_devices.constBegin(), m_devices.constEnd(),
        [] (QObject *t) { return static_cast<KWinWaylandDevice*>(t)->getDefaultConfig(); });
}

bool KWinWaylandBackend::isChangedConfig() const
{
    return std::any_of(m_devices.constBegin(), m_devices.constEnd(),
        [] (QObject *t) { return static_cast<KWinWaylandDevice*>(t)->isChangedConfig(); });
}

void KWinWaylandBackend::onDeviceAdded(QString sysName)
{
    if (std::any_of(m_devices.constBegin(), m_devices.constEnd(),
                    [sysName] (QObject *t) { return static_cast<KWinWaylandDevice*>(t)->sysName() == sysName; })) {
        return;
    }

    QDBusInterface deviceIface(QStringLiteral("org.kde.KWin"),
                                QStringLiteral("/org/kde/KWin/InputDevice/") + sysName,
                                QStringLiteral("org.kde.KWin.InputDevice"),
                                QDBusConnection::sessionBus(),
                                this);
    QVariant reply = deviceIface.property("pointer");

    if (reply.isValid() && reply.toBool()) {
        reply = deviceIface.property("touchpad");
        if (reply.isValid() && reply.toBool()) {
            return;
        }

        KWinWaylandDevice* dev = new KWinWaylandDevice(sysName);
        if (!dev->init() || !dev->getConfig()) {
            emit deviceAdded(false);
            return;
        }

        m_devices.append(dev);
        qCDebug(KCM_MOUSE).nospace() <<  "Device connected: " <<  dev->name() << " (" << dev->sysName() << ")";
        emit deviceAdded(true);
    }
}

void KWinWaylandBackend::onDeviceRemoved(QString sysName)
{
    QVector<QObject*>::const_iterator it = std::find_if(m_devices.constBegin(), m_devices.constEnd(),
                                            [sysName] (QObject *t) { return static_cast<KWinWaylandDevice*>(t)->sysName() == sysName; });
    if (it == m_devices.cend()) {
        return;
    }

    KWinWaylandDevice *dev = static_cast<KWinWaylandDevice*>(*it);
    qCDebug(KCM_MOUSE).nospace() <<  "Device disconnected: " <<  dev->name() << " (" << dev->sysName() << ")";

    int index = it - m_devices.cbegin();
    m_devices.removeAt(index);
    emit deviceRemoved(index);
}
