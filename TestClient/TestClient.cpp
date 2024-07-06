
#include <iostream>
#include <engine/NetworkEngine.h>
#include <engine/CloudConfigManager.h>
#include <network/iocp/IocpSocketServer.h>
#include <game/coordinate/Fixture.h>
#include <utility/Sampling.h>
#include <random>

struct SessionFixture
{
    Fixture fixture;
    SocketSessionPtr session;
};

class ClientWorker : public NetworkEngineWorker
{
private:
    std::unordered_map<uint64_t, SessionFixture> _sessions;

    utility::Timer<utility::Milliseconds> _timer = (utility::Milliseconds(500));

public:
    virtual void OnSocketSessionConnected(SocketContext* context)
    {
        _sessions[context->session->GetSessionId()] = { {}, context->session };
    }

    virtual void OnSocketSessionClosed(SocketContext* context)
    {
        _sessions.erase(context->session->GetSessionId());
    }

    virtual void OnSocketSessionData(SocketContext* context)
    {
        if (context->header.packet_type == PacketType::packet_game_enter_request_rs)
        {
            packet_game_enter_request_rs packet;
            DeserializePacketBody(context->buffer.Data(), context->buffer.RemainBytes(), packet);

            packet.object_info.fixture.user_data = reinterpret_cast<uint64_t>(context->session._UnsafePtr());

            _sessions[context->session->GetSessionId()].fixture = packet.object_info.fixture;
        }
        else if (context->header.packet_type == PacketType::packet_object_list_ps)
        {
            int a = 10;
        }
    }

    virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time)
    {
        for (auto& pair : _sessions)
        {
            if (pair.second.fixture.id != 0)
                pair.second.fixture.Move(delta_time);
        }
        if (_timer)
        {
            for (auto& pair : _sessions)
            {
                SocketSessionPtr ptr = pair.second.session;

                if (pair.second.fixture.id == 0)
                    continue;

                Direction random_direction = static_cast<Direction>(utility::RandomUint64() % 8);
                pair.second.fixture.direction = random_direction;

                packet_character_move_rq packet;
                packet.direction = pair.second.fixture.direction;
                packet.object_id = pair.second.fixture.id;
                packet.starting_position = pair.second.fixture.position;

                reinterpret_cast<SocketSession*>(pair.second.fixture.user_data)->Send(packet);
            }
            

        }

        return true;
    }
};
#include <deque>

int main()
{
    atomic_queue::AtomicQueueB<int*>* q = new atomic_queue::AtomicQueueB<int*>(1024);

    utility::Logger::InitializeLogger(4096, 4);
    InitializeSocketService();

    std::this_thread::sleep_for(utility::Seconds(3));
    SocketServerConfig config;
    config.read_buffer_capacity = 131072;
    config.read_buffer_pool_size = 8;
    config.max_connectable_socket_count = 1024;
    config.update_tick = 500;
    config.worker_count = 2;

    NetworkEngineOption option;
    option.max_session_count = 1024;
    option.session_idle_timeout_ms = 5000;
    option.session_heartbeat_interval_ms = 2000;
    option.socket_idle_timeout_ms = 5000;
    option.use_session_reconnect = false;
    option.worker_update_tick_ms = 33;

    IocpSocketServer* user_server = new IocpSocketServer();

    if (!user_server->Initialize(config))
        throw std::exception("failed to initialize user server");

    NetworkEngine* user_server_engine = new NetworkEngine();

    user_server_engine->Initialize(option, user_server, new ClientWorker());

    if (user_server_engine->Start() != 0)
        throw std::exception("failed to start user server engine");

    for (int index = 0; index < 200; index++)
        user_server_engine->RegisterConnectorSocket(SocketAddress::New("127.0.0.1", 9997), 0);

    while (true)
        std::this_thread::sleep_for(utility::Seconds(60));
}