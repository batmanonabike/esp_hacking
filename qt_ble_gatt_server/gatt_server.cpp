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
    
    // Add CCCD (Client Characteristic Configuration Descriptor) for notifications
    const QLowEnergyDescriptorData cccd(QBluetoothUuid(QStringLiteral("2902")), QByteArray(2, 0));
    m_customCharacteristic.addDescriptor(cccd);

    m_serviceData.addCharacteristic(m_customCharacteristic);

    // Add the service to the controller
    if (m_controller)
    {
        auto service = m_controller->addService(m_serviceData);
        if (!service)  // Check if service pointer is valid
        {
            qWarning() << "Could not add service";
            return;
        }
        qInfo() << "Service added with UUID:" << m_customServiceUuid.toString();
    }
    else
    {
        qWarning() << "Controller not initialized, cannot add service.";
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
    qWarning() << "Controller Error:" << error;
    // Consider stopping or restarting the server based on the error
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

