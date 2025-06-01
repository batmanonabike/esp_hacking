#include "gatt_server.h"

GattServer::GattServer(QObject *parent) : QObject(parent)
{
}

GattServer::~GattServer()
{
    stopServer();
}

void GattServer::startServer()
{
    if (m_controller)
    {
        qWarning() << "Server already started.";
        return;
    }

    m_controller = QLowEnergyController::createPeripheral(this);

    connect(m_controller, &QLowEnergyController::connected, this, &GattServer::handleClientConnection);
    connect(m_controller, &QLowEnergyController::disconnected, this, &GattServer::handleClientDisconnection);
    connect(m_controller, &QLowEnergyController::errorOccurred, this, &GattServer::handleError); // Qt 6
    
    // Remove this line - signal doesn't exist in Qt 6.9.0
    // connect(m_controller, &QLowEnergyController::advertisingStateChanged, this, &GattServer::handleAdvertisingStateChanged);

    setupService();

    QLowEnergyAdvertisingData advertisingData;
    advertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    advertisingData.setIncludePowerLevel(true);
    advertisingData.setLocalName("QtGattServer_esp32_ble_connect");
    advertisingData.setServices(QList<QBluetoothUuid>() << m_customServiceUuid);
    
    qInfo() << "Starting advertising for service:" << m_customServiceUuid.toString();

    // Start advertising
    m_controller->startAdvertising(QLowEnergyAdvertisingParameters(), advertisingData,
                                 advertisingData); // Second advertisingData is for scan response
    m_advertising = true;
    qInfo() << "GATT Server started, advertising...";
    
    // Add this code to log the Bluetooth address
    QBluetoothLocalDevice localDevice;
    if (localDevice.isValid()) {
        QBluetoothAddress localAddress = localDevice.address();
        qInfo() << "Server Bluetooth address:" << localAddress.toString();
    } else {
        qWarning() << "Could not get local Bluetooth adapter address";
    }
}

void GattServer::stopServer()
{
    if (m_controller && m_advertising)
    {
        m_controller->stopAdvertising();
        m_advertising = false;
        qInfo() << "GATT Server stopped advertising.";
    }
    if (m_controller)
    {
        delete m_controller;
        m_controller = nullptr;
        qInfo() << "GATT Server stopped.";
    }
}

void GattServer::setupService()
{
    m_serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    m_serviceData.setUuid(m_customServiceUuid);

    // Define a custom characteristic
    m_customCharacteristic.setUuid(m_customCharacteristicUuid);
    m_customCharacteristic.setValue(QByteArray("InitialValue"));
    m_customCharacteristic.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::Notify);
    
    // Add CCCD for notifications - use the correct UUID for CCCD (0x2902)
    QLowEnergyDescriptorData cccd;
    cccd.setUuid(QBluetoothUuid(QUuid("00002902-0000-1000-8000-00805f9b34fb")));
    cccd.setValue(QByteArray(2, 0)); // Initial value (notifications disabled)
    m_customCharacteristic.addDescriptor(cccd);
    qDebug() << "Adding CCCD with UUID:" << cccd.uuid().toString();

    m_serviceData.addCharacteristic(m_customCharacteristic);

    // Add the service to the controller
    if (m_controller)
    {
        // https://doc.qt.io/qt-6/qlowenergycontroller.html#addService
        QLowEnergyService * service = m_controller->addService(m_serviceData);
        if (service == nullptr)
        {
            qWarning() << "Could not add service - implementation may be missing";
            qWarning() << "Error: Your Bluetooth adapter likely doesn't support peripheral/advertising mode";
            qWarning() << "This is a common limitation with Windows PC Bluetooth adapters";
            return;
        }

        if (service->error() != QLowEnergyService::NoError)
        {
            QString error = GattServer::ServiceErrorToString(service->error());
            qWarning() << "Service is not valid. Error:" << error;
            return;
        }

        qInfo() << "Service Name:" << service->serviceName();
        qInfo() << "Service UUID:" << service->serviceUuid().toString();
        qInfo() << "Custom Service UUID:" << m_customServiceUuid.toString();
    }
    else
    {
        qWarning() << "Controller not initialized, cannot add service.";
    }
}

QString GattServer::ServiceErrorToString(QLowEnergyService::ServiceError error)
{
    switch (error) {
        case QLowEnergyService::NoError:
            return "NoError";
        case QLowEnergyService::OperationError:
            return "OperationError - Generic operation failure";
        case QLowEnergyService::CharacteristicWriteError:
            return "CharacteristicWriteError - Failed to write characteristic";
        case QLowEnergyService::DescriptorWriteError:
            return "DescriptorWriteError - Failed to write descriptor";
        case QLowEnergyService::CharacteristicReadError:
            return "CharacteristicReadError - Failed to read characteristic";
        case QLowEnergyService::DescriptorReadError:
            return "DescriptorReadError - Failed to read descriptor";
        case QLowEnergyService::UnknownError:
        default:
            return "UnknownError";
    }
}

void GattServer::handleClientConnection()
{
    qInfo() << "Client connected!";
    // You can get client details via m_controller->remoteDevice()
}

void GattServer::handleClientDisconnection()
{
    qInfo() << "Client disconnected.";
    // If you want to re-advertise after a disconnect:
    if (m_controller && !m_advertising) { // Check if controller exists and not already advertising
        QLowEnergyAdvertisingData advertisingData;
        advertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
        advertisingData.setIncludePowerLevel(true);
        advertisingData.setLocalName("QtGattServer_esp32_ble_connect");
        advertisingData.setServices(QList<QBluetoothUuid>() << m_customServiceUuid);
        m_controller->startAdvertising(QLowEnergyAdvertisingParameters(), advertisingData, advertisingData);
        m_advertising = true;
        qInfo() << "Restarted advertising after client disconnect.";
    }
}

void GattServer::handleError(QLowEnergyController::Error error)
{
    // Note: Windows requires admin privileges for BLE advertising!!

    qWarning() << "Controller Error:" << error;
    
    switch (error) {
    case QLowEnergyController::AdvertisingError:
    {
        qWarning() << "Failed to start advertising. Possible causes:";
        qWarning() << "- Bluetooth adapter is off or unavailable";
        qWarning() << "- Another application is using Bluetooth advertising";
        qWarning() << "- Insufficient permissions or hardware limitations";
        
        // Check if Bluetooth is available and powered on
        QBluetoothLocalDevice localDevice;
        if (!localDevice.isValid()) {
            qWarning() << "No valid Bluetooth adapter found!";
        } else if (localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
            qWarning() << "Bluetooth adapter is powered off. Please enable Bluetooth.";
        } else {
            qWarning() << "Bluetooth adapter appears to be on, but advertising failed.";
        }
        
        // Attempt recovery - wait and try again
        QTimer::singleShot(3000, this, [this]() {
            qInfo() << "Attempting to restart advertising...";
            stopServer();
            startServer();
        });

    }
    break;
        
    default:
        qWarning() << "Unhandled Bluetooth error. Check hardware and permissions.";
        break;
    }
}

void GattServer::handleAdvertisingStateChanged(int state)
{
    if (state == 1) // 1 typically represents active advertising
    {
        m_advertising = true;
        qInfo() << "Controller now advertising.";
    }
    else
    {
        m_advertising = false;
        qInfo() << "Controller stopped advertising. State:" << state;
    }
}

