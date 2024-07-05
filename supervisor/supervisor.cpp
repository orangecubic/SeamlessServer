
#include <iostream>
#include <engine/NetworkEngine.h>
#include <network/iocp/IocpSocketServer.h>
#include "supervisor/SupervisorWorker.h"

int main()
{
    utility::Logger::InitializeLogger(4096, 4);
    InitializeSocketService();

    SocketServerConfig config;
    config.read_buffer_capacity = 65536;
    config.read_buffer_pool_size = 16;
    config.max_acceptable_socket_count = 32;
    config.max_connectable_socket_count = 32;
    config.parallel_acceptor_count = 8;
    config.update_tick = 500;
    config.worker_count = 4;

    IocpSocketServer* server = new IocpSocketServer();
    if (!server->Initialize(config))
        throw std::exception("failed to initialize supervisor server");

    if (!server->InitializeAcceptor(SocketAddress::New("0.0.0.0", 9987), { {TCP_NODELAY, 1} }))
        throw std::exception("failed to initialize supervisor server acceptor");

    NetworkEngineOption option;
    option.max_session_count = 100;
    option.session_idle_timeout_ms = 180000;
    option.socket_idle_timeout_ms = 180000;
    option.session_heartbeat_interval_ms = 100;
    option.socket_idle_timeout_ms = 50000;
    option.use_session_reconnect = false;
    option.worker_update_tick_ms = 100;

    NetworkEngine* engine = new NetworkEngine();
    
    SupervisorWorker* supervisor = new SupervisorWorker(engine, SocketAddress::New("127.0.0.1", 9987));

    engine->Initialize(option, server, supervisor);

    if (engine->Start() != 0)
        throw std::exception("failed to start supervisor server engine");

    while (true)
        std::this_thread::sleep_for(utility::Seconds(60));
}