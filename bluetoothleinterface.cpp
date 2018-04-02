#include "bluetoothleinterface.h"
#include <QLowEnergyCharacteristic>
#include "qaesencryption.h"

BluetoothLEInterface::BluetoothLEInterface()
{
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(60000);

    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &BluetoothLEInterface::addDevice);
    connect(m_deviceDiscoveryAgent, static_cast<void (QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error),
            this, &BluetoothLEInterface::scanError);

    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, &BluetoothLEInterface::scanFinished);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled, this, &BluetoothLEInterface::scanFinished);
}

void BluetoothLEInterface::startScan()
{
    m_deviceDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}
void BluetoothLEInterface::scanError()
{
    qDebug() << "Scan Error";
}

void BluetoothLEInterface::scanFinished()
{
    qDebug() << "Scan Finished";
}

void BluetoothLEInterface::connectToDevice(const QString &address)
{

}

void BluetoothLEInterface::addDevice(const QBluetoothDeviceInfo &device)
{
    qDebug() << "Found" << device.address() << device.name();
    // If device is LowEnergy-device, add it to the list
    if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        m_devices.append(device);
        qDebug() << "Low Energy device found. Scanning more...";

        if (device.name().toLower().contains("amazfit")) {
            qDebug() << "Found an amazfit, scanning services";

            m_control = new QLowEnergyController(device, this);
            connect(m_control, &QLowEnergyController::serviceDiscovered,
                    this, &BluetoothLEInterface::serviceDiscovered);

            connect(m_control, &QLowEnergyController::discoveryFinished,
                    this, [this] {
                qDebug() << "Service scan done.";
            });

            connect(m_control, static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
                    this, [this](QLowEnergyController::Error error) {
                Q_UNUSED(error);
                qDebug() << "Cannot connect to remote device.";
            });
            connect(m_control, &QLowEnergyController::connected, this, [this]() {
                qDebug() << "Controller connected. Search services...";
                m_control->discoverServices();
            });
            connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
                qDebug() << "LowEnergy controller disconnected";
            });

            // Connect
            m_control->connectToDevice();
            m_control->discoverServices();

        }
    }
}

void BluetoothLEInterface::serviceDiscovered(const QBluetoothUuid &gatt)
{
    qDebug() << "Service discovered:" << gatt.toString();

    if (gatt == QBluetoothUuid(UUID_SERVICE_BIP_AUTH)) {
        qDebug() << "Creating service object";
        m_service = m_control->createServiceObject(QBluetoothUuid(UUID_SERVICE_BIP_AUTH), this);
        qDebug() << m_service->serviceName();
        if (m_service) {
            connect(m_service, &QLowEnergyService::stateChanged, this, &BluetoothLEInterface::serviceStateChanged);
            connect(m_service, &QLowEnergyService::characteristicChanged, this, &BluetoothLEInterface::authCharacteristicChanged);
            //connect(m_service, &QLowEnergyService::descriptorWritten, this, &DeviceHandler::confirmedDescriptorWrite);
            m_service->discoverDetails();
        } else {
            qDebug() << "Service not found";
        }
    }
}

void BluetoothLEInterface::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue)
{
    qDebug() << "Changed:" << characteristic.uuid() << "(" << characteristic.name() << "):" << newValue;
}

void BluetoothLEInterface::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::DiscoveringServices:
        qDebug() << "xDiscovering services...";
        break;
    case QLowEnergyService::ServiceDiscovered:
    {
        qDebug() << "xService discovered.";

        Q_FOREACH(QLowEnergyCharacteristic c, m_service->characteristics()) {
            qDebug() << "Characteristic:" << c.uuid() << c.name();
        }

        qDebug() << "Getting the auth characteristic";
        m_authChar = m_service->characteristic(QBluetoothUuid(UUID_CHARACTERISTEC_AUTH));
        if (!m_authChar.isValid()) {
            qDebug() << "Auth service not found.";
            break;
        }

        qDebug() << "Registering for notifications on the auth service";
        m_notificationDesc = m_authChar.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
        if (m_notificationDesc.isValid())
            m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));

        qDebug() << "Writing special value to trigger pair dialog";
        m_service->writeCharacteristic(m_authChar, QByteArray(&AUTH_SEND_KEY, 1) + QByteArray(&AUTH_BYTE, 1) + AUTH_SECRET_KEY);

        break;
    }
    default:
        //nothing for now
        break;
    }
}

void BluetoothLEInterface::authCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    // ignore any other characteristic change -> shouldn't really happen though
    if (c.uuid() != QBluetoothUuid(UUID_CHARACTERISTEC_AUTH)) {
        qDebug() << "Expecting auth UUID but got" << c.uuid();
        return;
    }

    qDebug() << "Received data:" << value.size() << value.toHex();
    if (value.size() < 3) {
        return;
    }

    if (value[0] == AUTH_RESPONSE && value[1] == AUTH_SEND_KEY && value[2] == AUTH_SUCCESS) {
        qDebug() << "Received initial auth success, requesting random auth number";
        m_service->writeCharacteristic(m_authChar, QByteArray(&AUTH_REQUEST_RANDOM_AUTH_NUMBER, 1) + QByteArray(&AUTH_BYTE, 1));
    } else  if (value[0] == AUTH_RESPONSE && value[1] == AUTH_REQUEST_RANDOM_AUTH_NUMBER && value[2] == AUTH_SUCCESS) {
        qDebug() << "Received random auth number, sending encrypted auth number";
        m_service->writeCharacteristic(m_authChar, QByteArray(&AUTH_SEND_ENCRYPTED_AUTH_NUMBER, 1) + QByteArray(&AUTH_BYTE, 1) + handleAesAuth(value.mid(3, 17), AUTH_SECRET_KEY));
    } else  if (value[0] == AUTH_RESPONSE && value[1] == AUTH_SEND_ENCRYPTED_AUTH_NUMBER && value[2] == AUTH_SUCCESS) {
        qDebug() << "Authenticated, go read the device information!";
    } else {
        qDebug() << "Unexpected data";
    }

}

QByteArray BluetoothLEInterface::handleAesAuth(QByteArray data, QByteArray secretKey)
{
    return QAESEncryption::Crypt(QAESEncryption::AES_128, QAESEncryption::ECB, data, secretKey, QByteArray(), QAESEncryption::ZERO);
}

void BluetoothLEInterface::requestDeviceInfo()
{
    m_infoService = m_control->createServiceObject(QBluetoothUuid::DeviceInformation, this);
    if (m_infoService) {
        qDebug() << m_infoService->serviceName();

        connect(m_infoService, &QLowEnergyService::stateChanged, this, &BluetoothLEInterface::infoServiceStateChanged);
        connect(m_infoService, &QLowEnergyService::characteristicRead, this, &BluetoothLEInterface::characteristicRead);
        m_infoService->discoverDetails();
    } else {
        qDebug() << "Service not found";
    }
}

void BluetoothLEInterface::characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qDebug() << "Read:" << characteristic.uuid() << "(" << characteristic.name() << "):" << value;
}

void BluetoothLEInterface::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qDebug() << "Written:" << characteristic.uuid() << "(" << characteristic.name() << "):" << value;
}

void BluetoothLEInterface::infoServiceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::DiscoveringServices:
        qDebug() << "Discovering services...";
        break;
    case QLowEnergyService::ServiceDiscovered:
    {
        qDebug() << "Service discovered.";

        Q_FOREACH(QLowEnergyCharacteristic c, m_infoService->characteristics()) {
            qDebug() << "Characteristic:" << c.uuid() << c.name();
        }

        QLowEnergyCharacteristic c = m_infoService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_INFO_SERIAL_NO));
        m_infoService->readCharacteristic(c);
        c = m_infoService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_INFO_HARDWARE_REV));
        m_infoService->readCharacteristic(c);
        c = m_infoService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_INFO_SOFTWARE_REV));
        m_infoService->readCharacteristic(c);
        c = m_infoService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_INFO_SYSTEM_ID));
        m_infoService->readCharacteristic(c);
        c = m_infoService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_INFO_PNP_ID));
        m_infoService->readCharacteristic(c);

        break;
    }
    default:
        //nothing for now
        break;
    }
}

void BluetoothLEInterface::alertServiceStateChanged(QLowEnergyService::ServiceState s)
{
    switch (s) {
    case QLowEnergyService::DiscoveringServices:
        qDebug() << "Discovering services...";
        break;
    case QLowEnergyService::ServiceDiscovered:
    {
        qDebug() << "Service discovered.";

        Q_FOREACH(QLowEnergyCharacteristic c, m_alertService->characteristics()) {
            qDebug() << "Characteristic:" << c.uuid() << c.name();
        }

        QLowEnergyCharacteristic control = m_alertService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_ALERT_CONTROL));
        m_alertService->writeCharacteristic(control, QByteArray::fromHex("0000"));

        QLowEnergyCharacteristic c = m_alertService->characteristic(QBluetoothUuid(UUID_CHARACTERISTIC_NEW_ALERT));
        m_alertService->writeCharacteristic(c, QByteArray::fromHex("01") + QByteArray::fromHex("01") + "TEST\nThis is a long message hope it displays ok");

        break;
    }
    default:
        //nothing for now
        break;
    }
}

void BluetoothLEInterface::sendMessage()
{
    m_alertService = m_control->createServiceObject(QBluetoothUuid(UUID_SERVICE_ALERT_NOTIFICATION), this);
    if (m_alertService) {
        qDebug() << m_alertService->serviceName();

        connect(m_alertService, &QLowEnergyService::stateChanged, this, &BluetoothLEInterface::alertServiceStateChanged);
        connect(m_alertService, &QLowEnergyService::characteristicRead, this, &BluetoothLEInterface::characteristicRead);
        connect(m_alertService, &QLowEnergyService::characteristicWritten, this, &BluetoothLEInterface::characteristicWritten);
        //connect(m_alertService, &QLowEnergyService::error, this, &BluetoothLEInterface::error);

        m_alertService->discoverDetails();
    } else {
        qDebug() << "Service not found";
    }
}

void BluetoothLEInterface::error(QLowEnergyService::ServiceError newError)
{
    qDebug() << "Error" << newError;
}
