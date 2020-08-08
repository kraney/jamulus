#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <agones/sdk.h>
#include <grpc++/grpc++.h>

class CServer;

class CAgonesHelper : public QObject
{
    Q_OBJECT
public:
    CAgonesHelper(CServer &server);
    virtual ~CAgonesHelper();
    void Start();
    void Stop();

    static bool toBool(const std::string &strval);

protected:
    agones::SDK agonesSDK;
    CServer &server;

    QThread *watchThread;
    QTimer *healthTimer;
    QTimer *reservationTimer;

signals:
    void Updated(const agones::dev::sdk::GameServer &gameserver);

public slots:
    void CheckDeallocate(const agones::dev::sdk::GameServer &gameserver);
    void OnUpdate(const agones::dev::sdk::GameServer &gameserver);
    void OnHealthTimer();
    void OnReservationTimer();

private slots:
    void doWatch();
};