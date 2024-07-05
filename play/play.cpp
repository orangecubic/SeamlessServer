
#include <iostream>
#include <engine/NetworkEngine.h>
#include <engine/CloudConfigManager.h>
#include <network/iocp/IocpSocketServer.h>
#include "play/PlayWorker.h"
#include "play/PacketHandler.h"

int main()
{
    utility::Logger::InitializeLogger(4096, 4);
    InitializeSocketService();

    CloudConfigManager* config_manager = new CloudConfigManager(ServerType::Play, SocketAddress::New("127.0.0.1", 9987));

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

    PlayWorker* play = new PlayWorker;
    PacketHandler* packet_handler = new PacketHandler(play);

    intra_server_engine->Initialize(config.intra_network_engine_option, intra_server, *play);
    user_server_engine->Initialize(config.user_network_engine_option, user_server, *play);

    play->SetPacketHandler(packet_handler);
    if (!play->Initialize(
        ServerType::Play,
        SocketAddress::New("127.0.0.1", config.intra_server_listen_port),
        intra_server_engine,
        user_server_engine,
        {
            IntraServerConfig{ ServerType::Play, IntraServerConnectionPolicy::All, IntraServerSectorListeningPolicy::Not },
            IntraServerConfig{ ServerType::Agency, IntraServerConnectionPolicy::Not, IntraServerSectorListeningPolicy::All }
        },
        SocketAddress::New("127.0.0.1", 9987),
        config.game_config
    ))
        throw std::exception("failed to initialize play worker");

    if (intra_server_engine->Start() != 0)
        throw std::exception("failed to start intra server engine");

    if (user_server_engine->Start() != 0)
        throw std::exception("failed to start user server engine");

    while (true)
        std::this_thread::sleep_for(utility::Seconds(60));
}