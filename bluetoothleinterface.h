#ifndef BLUETOOTHLEINTERFACE_H
#define BLUETOOTHLEINTERFACE_H

#include <QObject>
#include <QLowEnergyController>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QStringLiteral>

class BluetoothLEInterface : public QObject
{
    Q_OBJECT
public:
    BluetoothLEInterface();

    Q_INVOKABLE void connectToDevice(const QString &address);
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void requestDeviceInfo();
    Q_INVOKABLE void sendMessage();

    const QString UUID_SERVICE_BIP_AUTH = "{0000fee1-0000-1000-8000-00805f9b34fb}";
    const QString UUID_SERVICE_ALERT_NOTIFICATION = "{00001811-0000-1000-8000-00805f9b34fb}";

    const QString UUID_CHARACTERISTEC_AUTH = "{00000009-0000-3512-2118-0009af100700}";
    const QString UUID_CHARACTERISTIC_INFO_SERIAL_NO = "{00002a25-0000-1000-8000-00805f9b34fb}";
    const QString UUID_CHARACTERISTIC_INFO_HARDWARE_REV = "{00002a27-0000-1000-8000-00805f9b34fb}";
    const QString UUID_CHARACTERISTIC_INFO_SOFTWARE_REV = "{00002a28-0000-1000-8000-00805f9b34fb}";
    const QString UUID_CHARACTERISTIC_INFO_SYSTEM_ID = "{00002a23-0000-1000-8000-00805f9b34fb}";
    const QString UUID_CHARACTERISTIC_INFO_PNP_ID = "{00002a50-0000-1000-8000-00805f9b34fb}";
    const QString UUID_CHARACTERISTIC_NEW_ALERT = "{00002a46-0000-1000-8000-00805f9b34fb}";
    const QString UUID_CHARACTERISTIC_ALERT_CONTROL = "{00002a44-0000-1000-8000-00805f9b34fb}";

    const char AUTH_SEND_KEY = 0x01;
    const char AUTH_REQUEST_RANDOM_AUTH_NUMBER = 0x02;
    const char AUTH_SEND_ENCRYPTED_AUTH_NUMBER = 0x03;
    const QByteArray AUTH_SECRET_KEY = "0123456789@ABCDE";
    const char AUTH_RESPONSE = 0x10;
    const char AUTH_SUCCESS = 0x01;
    const char AUTH_FAIL = 0x04;
    const char AUTH_BYTE = 0x08;

private:
    QLowEnergyController *m_LEController;
    QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent;
    QList<QBluetoothDeviceInfo> m_devices;
    QLowEnergyController *m_control;
    QLowEnergyService *m_service;
    QLowEnergyService *m_infoService;
    QLowEnergyService *m_alertService;
    QLowEnergyDescriptor m_notificationDesc;
    QLowEnergyCharacteristic m_authChar;

    Q_SLOT void scanFinished();
    Q_SLOT void addDevice(const QBluetoothDeviceInfo &device);
    Q_SLOT void scanError();
    Q_SLOT void serviceDiscovered(const QBluetoothUuid &gatt);
    Q_SLOT void characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue);
    Q_SLOT void serviceStateChanged(QLowEnergyService::ServiceState s);
    Q_SLOT void infoServiceStateChanged(QLowEnergyService::ServiceState s);
    Q_SLOT void alertServiceStateChanged(QLowEnergyService::ServiceState s);

    Q_SLOT void authCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);
    Q_SLOT void characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
    Q_SLOT void characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
    Q_SLOT void error(QLowEnergyService::ServiceError newError);

    QByteArray handleAesAuth(QByteArray data, QByteArray secretKey);
};

#endif // BLUETOOTHLEINTERFACE_H
