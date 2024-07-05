#include "CloudConfigManager.h"
#include "../network/iocp/IocpSocketServer.h"
#include "protocol/Protocol.h"
#include "../utility/Time.h"

CloudConfigManager::CloudConfigWorker::CloudConfigWorker(CloudConfigManager* manager, NetworkEngine* const engine)
	: _manager(manager), _engine(engine)
{

}
void CloudConfigManager::CloudConfigWorker::OnSocketSessionConnected(SocketContext* context)
{
	packet_load_server_config_rq packet;
	packet.server_type = _manager->_server_type;

	context->session->Send(packet);
}

void CloudConfigManager::CloudConfigWorker::OnSocketSessionData(SocketContext* context)
{
	if (static_cast<PacketType>(context->header.packet_type) != PacketType::packet_load_server_config_rs)
		throw new std::exception("connected server is not a cloud config server");

	packet_load_server_config_rs packet;
	
	bool result = DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet);
	if (!result)
	{
		throw new std::exception("cloud config server packet mismatch");
	}

	_manager->_result_queue.TryPush(new ServerConfig(packet.server_config));
}

CloudConfigManager::CloudConfigManager(const ServerType server_type, const SocketAddress& config_server_address) : _server_type(server_type), _config_server_address(config_server_address)
{
	_engine = nullptr;
	_worker = nullptr;
}

void CloudConfigManager::RequestCloudConfig(const uint32_t max_retry_count)
{
	SocketServer* socket_server = new IocpSocketServer();
	_engine = new NetworkEngine();
	_worker = new CloudConfigWorker(this, _engine);

	NetworkEngineOption engine_option;
	engine_option.max_session_count = 2;

	if (!_engine->Initialize(engine_option, socket_server, _worker))
		throw std::exception("failed to initialize socket server");

	SocketServerConfig server_config;
	
	server_config.read_buffer_pool_size = 4;
	server_config.write_buffer_pool_size = 4;
	server_config.parallel_acceptor_count = 1;
	server_config.worker_count = 1;

	server_config.max_acceptable_socket_count = 2;
	server_config.max_connectable_socket_count = 2;
	
	if (!socket_server->Initialize(server_config))
		throw std::exception("failed to initialize socket server");

	socket_server->SetSocketEventHandler(_engine);

	if (_engine->Start() != 0)
		throw std::exception("failed to start network engine");

	_connector_id = _engine->RegisterConnectorSocket(_config_server_address, 0);
}

ServerConfig CloudConfigManager::GetCloudConfig()
{
	ServerConfig* config;

	while (!_result_queue.TryPop(config))
	{
		std::this_thread::sleep_for(utility::Milliseconds(100));
		_result_queue.LockSubmissionQueue();
	}

	_engine->UnregisterConnectorSocket(_connector_id);

	ServerConfig result = *config;

	delete config;

	return result;
}