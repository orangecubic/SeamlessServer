
#include <iostream>
#include <engine/NetworkEngine.h>
#include <engine/CloudConfigManager.h>
#include <network/iocp/IocpSocketServer.h>
#include "agency/AgencyWorker.h"
#include "agency/PacketHandler.h"

int main()
{
    utility::Logger::InitializeLogger(4096, 4);
    InitializeSocketService();

    CloudConfigManager* config_manager = new CloudConfigManager(ServerType::Agency, SocketAddress::New("127.0.0.1", 9987));

    config_manager->RequestCloudConfig(10);

    ServerConfig config = config_manager->GetCloudConfig();

    IocpSocketServer* intra_server = new IocpSocketServer();
    IocpSocketServer* user_server = new IocpSocketServer();

    if (!intra_server->Initialize(config.intra_socket_server_config))
        throw std::exception("failed to initialize intra server");

    if (!intra_server->InitializeAcceptor(SocketAddress::New("0.0.0.0", config.intra_server_listen_port), { {TCP_NODELAY, 1} }))
        throw std::exception("failed to initialize intra server acceptor");

    if (!user_server->Initialize(config.user_socket_server_config))
        throw std::exception("failed to initialize user server");

    if (!user_server->InitializeAcceptor(SocketAddress::New("0.0.0.0", config.user_listen_port), { {TCP_NODELAY, 1} }))
        throw std::exception("failed to initialize user server acceptor");

    NetworkEngine* intra_server_engine = new NetworkEngine();
    NetworkEngine* user_server_engine = new NetworkEngine();

    AgencyWorker* agency = new AgencyWorker;
    PacketHandler* packet_handler = new PacketHandler(agency);

    intra_server_engine->Initialize(config.intra_network_engine_option, intra_server, *agency);
    user_server_engine->Initialize(config.user_network_engine_option, user_server, *agency);

    agency->SetPacketHandler(packet_handler);
    if (!agency->Initialize(
        ServerType::Agency,
        SocketAddress::New("127.0.0.1", config.intra_server_listen_port),
        intra_server_engine,
        user_server_engine,
        {
            IntraServerConfig{ ServerType::Play, IntraServerConnectionPolicy::All, IntraServerSectorListeningPolicy::Not }
        },
        SocketAddress::New("127.0.0.1", 9987),
        config.game_config
    ))
        throw std::exception("failed to initialize agency worker");

    if (intra_server_engine->Start() != 0)
        throw std::exception("failed to start intra server engine");

    if (user_server_engine->Start() != 0)
        throw std::exception("failed to initialize user server engine");

    while (true)
        std::this_thread::sleep_for(utility::Seconds(60));
}