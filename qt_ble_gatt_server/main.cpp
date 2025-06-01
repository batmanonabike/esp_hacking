#include <QCoreApplication>
#include "gatt_server.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    GattServer server;
    server.startServer();

    return a.exec();
}
