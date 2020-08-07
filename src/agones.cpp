#include <QDateTime>
#include <QMetaType>
#include <iostream>
#include "agones.h"

Q_DECLARE_METATYPE(agones::dev::sdk::GameServer)

CAgonesHelper::CAgonesHelper() : QObject(0)
{
    qRegisterMetaType<agones::dev::sdk::GameServer>("agones::dev::sdk::GameServer");

    QObject::connect(this, &CAgonesHelper::Updated, this, &CAgonesHelper::OnUpdate);

    healthTimer = new QTimer(this);
    QObject::connect(healthTimer, &QTimer::timeout, this, &CAgonesHelper::OnHealthTimer);
    reservationTimer = new QTimer(this);
    QObject::connect(reservationTimer, &QTimer::timeout, this, &CAgonesHelper::OnReservationTimer);
    reservationTimer->setSingleShot(true);

    // First we just initialize ourselves with the SDK
    std::cout << "Reporting GameServer to Agones" << std::endl;
    if (!agonesSDK.Connect())
    {
        std::cerr << "Unable to connect to Agones" << std::endl;
        exit(1);
    }

    // starting health timer
    std::cout << "Starting health timer" << std::endl
              << std::flush;
    healthTimer->start(3000);

    grpc::Status status = agonesSDK.Ready();
    if (!status.ok())
    {
        std::cerr << "Could not run Agones Ready(): " << status.error_message() << ". Exiting";
        exit(1);
    }
    std::cout << "Marked Ready" << std::endl;

    watchThread = QThread::create([this] { doWatch(); });
    watchThread->start();
}

CAgonesHelper::~CAgonesHelper()
{
    reservationTimer->stop();
    healthTimer->stop();

    agonesSDK.Shutdown();

    delete (healthTimer);
    delete (reservationTimer);
}

void CAgonesHelper::Start()
{
    grpc::Status status = agonesSDK.Allocate();
    if (!status.ok())
    {
        std::cerr << "Unable to mark this server as allocated" << std::endl;
    }
}

void CAgonesHelper::Stop()
{
    agones::dev::sdk::GameServer gameserver;
    grpc::Status status = agonesSDK.GameServer(&gameserver);
    if (!status.ok())
    {
        std::cout << "Unable to retrieve GameServer status\n"
                  << std::flush;
        return;
    }
    CheckDeallocate(gameserver);
}

void CAgonesHelper::CheckDeallocate(const agones::dev::sdk::GameServer &gameserver)
{
    auto &annotations = gameserver.object_meta().annotations();
    auto it = annotations.find("llcon.sf.net/until");
    const QDateTime curDateTime = QDateTime::currentDateTimeUtc();
    QDateTime end;
    if (it == annotations.end())
    {
        // No end time was specified - let's set one
        end = curDateTime.addSecs(10 * 60);
        agonesSDK.SetAnnotation("llcon.sf.net/until", end.toString(Qt::ISODate).toStdString());
    }
    else
    {
        // There's an end time for a reservation - see how long it has left
        end = QDateTime::fromString(QString::fromStdString(it->second), Qt::ISODate);
    }
    qint64 remaining = curDateTime.secsTo(end);
    std::cout << "Remaining time: " << remaining << std::endl
              << std::flush;
    if (remaining > 0 && remaining < (8 * 60 * 60))
    {
        std::chrono::seconds rsv = (std::chrono::seconds)remaining;
        std::cout << "Setting server to allocated status for " << remaining << " seconds" << std::endl
                  << std::flush;
        // There's time remaining, so convert our status to a reservation for that amount of time
        grpc::Status status = agonesSDK.Allocate();
        if (!status.ok())
        {
            std::cerr << "Unable to mark this server as allocated" << std::endl;
        }
        reservationTimer->start(curDateTime.msecsTo(end));
        return;
    }
    // otherwise, fall through and go back to deallocated / ready
    std::cout << "Deallocating server (going back to Ready)" << std::endl
              << std::flush;
    grpc::Status status = agonesSDK.Ready();
    if (!status.ok())
    {
        std::cerr << "Unable to mark this server as deallocated" << std::endl;
    }
}

void CAgonesHelper::OnUpdate(const agones::dev::sdk::GameServer &gameserver)
{
    std::cout << "GameServer Update:\n"                                //
              << "\tname: " << gameserver.object_meta().name() << "\n" //
              << "\tstate: " << gameserver.status().state() << "\n"
              << std::flush;
    if (gameserver.status().state() == "Allocated")
    {
        // we're allocated, but with no clients. Let's translate that into a "Reserved" for the indicated time
        emit CheckDeallocate(gameserver);
    }
}

void CAgonesHelper::OnHealthTimer()
{
    bool ok = agonesSDK.Health();
    std::cout << "Health ping " << (ok ? "sent" : "failed") << "\n"
              << std::flush;
}

void CAgonesHelper::OnReservationTimer()
{
    agones::dev::sdk::GameServer gameserver;
    grpc::Status status = agonesSDK.GameServer(&gameserver);
    if (!status.ok())
    {
        std::cout << "Unable to retrieve GameServer status\n"
                  << std::flush;
        return;
    }
    CheckDeallocate(gameserver);
}

void CAgonesHelper::doWatch()
{
    // this call blocks, listening for updates on our state
    agonesSDK.WatchGameServer([this](const agones::dev::sdk::GameServer &gameserver) {
        emit Updated(gameserver);
    });
}