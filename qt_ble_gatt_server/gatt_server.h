#ifndef GATT_SERVER_H
#define GATT_SERVER_H

#include <QObject>
#include <QLowEnergyController>
#include <QLowEnergyServiceData>
#include <QLowEnergyCharacteristicData>
#include <QLowEnergyDescriptorData>
#include <QLowEnergyAdvertisingData>
#include <QLowEnergyAdvertisingParameters>
#include <QBluetoothUuid>
#include <QDebug>

// Forward declaration
class QLowEnergyController;

class GattServer : public QObject
{
    Q_OBJECT
public:
    explicit GattServer(QObject *parent = nullptr);
    ~GattServer();

    void startServer();
    void stopServer();

private slots:
    void handleClientConnection();
    void handleClientDisconnection();
    void handleError(QLowEnergyController::Error error);
    // Fix for Qt version compatibility
    void handleAdvertisingStateChanged(int state);

private:
    void setupService();

    QLowEnergyController *m_controller = nullptr;
    QLowEnergyServiceData m_serviceData;
    QLowEnergyCharacteristicData m_customCharacteristic;

    // Use the macro defined in CMakeLists.txt
    const QBluetoothUuid m_customServiceUuid = QBluetoothUuid(QStringLiteral(CUSTOM_SERVICE_UUID));

    // Example characteristic UUID.
    const QBluetoothUuid m_customCharacteristicUuid = QBluetoothUuid(QStringLiteral("BF2A449A-3B7C-4E0D-8B9A-52ADD8DA44A4")); 

    bool m_advertising = false;
};

#endif // GATT_SERVER_H
