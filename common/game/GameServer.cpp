#include "GameServer.h"
#include "../utility/Defer.h"

constexpr uint64_t SERVER_ID_KEY = UINT64_MAX - 1;
constexpr uint64_t UNIVERSAL_SESSION_INFO_KEY = UINT64_MAX;

GameServer::GameServer()
{
	_worker = new GameServerWorker(this);
}

void GameServer::GameServerWorker::OnSocketSessionConnected(SocketContext* context) 
{
	assert(context->session->GetNetworkEngine() == _server->_intra_network_engine);

	// context->attachment = ServerId
	IntraServerInfo* intra_server_info = GetServerSession(context->attachment);

	if (intra_server_info == nullptr)
	{
		context->session->CloseSession();
		return;
	}

	if (intra_server_info->server_info.server_type == ServerType::Supervisor)
	{
		_server->_supervisor_server_info.session = context->session;
		_server->_server_sessions[SUPERVISOR_SERVER_ID] = _server->_supervisor_server_info;
		_server->_active_sessions[_server->_supervisor_server_info.universal_session_id] = _server->_supervisor_server_info;

		// Supervisor에 재연결한 경우
		if (_server->_service_ready)
		{
			packet_recovery_minion_server_rq packet;
			packet.server_info = _server->_my_server_info;

			for (const auto& sector_pair : _server->_sector_posting_manager->GetOwnershipSectors(_server->_my_server_info.server_id))
				packet.sectors.push_back(sector_pair.first);

			for (const auto& server_pair : _server->_server_sessions)
				packet.intra_servers.push_back(server_pair.second.server_info);

			context->session->Send(packet);
		}
		else
		{
			packet_register_minion_server_rq packet;
			packet.server_type = _server->_my_server_info.server_type;
			packet.address = _server->_my_server_info.address;

			context->session->Send(packet);
		}

		context->session->SetUserData(UNIVERSAL_SESSION_INFO_KEY, &_server->_supervisor_server_info);

		return;
	}

	uint64_t session_id = _server->_current_universal_session_id;

	UniversalSessionInfo& session_info = _server->_active_sessions[_server->_current_universal_session_id++];

	session_info.session = context->session;
	session_info.universal_session_id = session_id;

	context->session->SetUserData(UNIVERSAL_SESSION_INFO_KEY, &session_info);

	packet_authorize_server_rq packet;
	packet.server_id = _server->_my_server_info.server_id;
	packet.server_type = _server->_my_server_info.server_type;

	context->session->Send(packet);
}

void GameServer::GameServerWorker::OnSocketSessionAccepted(SocketContext* context) 
{
	uint64_t session_id = _server->_current_universal_session_id;

	UniversalSessionInfo& session_info = _server->_active_sessions[_server->_current_universal_session_id++];
	session_info.session = context->session;
	session_info.universal_session_id = session_id;

	context->session->SetUserData(UNIVERSAL_SESSION_INFO_KEY, &session_info);

	if (context->session->GetNetworkEngine() == _server->_user_network_engine)
	{
		if (!_server->_service_ready)
		{
			context->session->CloseSession();
			return;
		}

		session_info.is_client = true;
		_server->OnClientAccepted(&session_info);
		return;
	}
}

void GameServer::GameServerWorker::OnSocketSessionClosed(SocketContext* context)
{
	UniversalSessionInfo* session_info = context->session->GetUserData<uint64_t, UniversalSessionInfo*>(UNIVERSAL_SESSION_INFO_KEY);

	if (session_info == nullptr)
		return;

	_server->_packet_throttler->CleanUp(context->session);

	DEFER({ _server->_active_sessions.erase(session_info->universal_session_id); });

	_server->_sector_posting_manager->UnsetAll(session_info->universal_session_id);

	if (session_info->is_client)
	{
		_server->OnClientClosed(session_info);
		
		return;
	}

	IntraServerInfo* server_session = GetServerSession(session_info->session->GetUserData<uint64_t, uint64_t>(SERVER_ID_KEY));

	if (server_session == nullptr)
		return;

	_server->_sector_posting_manager->UnsetAll(server_session->server_info.server_id);

	if (server_session->server_info.server_type == ServerType::Supervisor)
	{
		_server->_supervisor_server_info.session.Release();
		return;
	}

	_server->OnIntraServerClosed(server_session);
}

void GameServer::GameServerWorker::OnSocketSessionAbandoned(SocketContext* context)
{
	UniversalSessionInfo* session_info = context->session->GetUserData<uint64_t, UniversalSessionInfo*>(UNIVERSAL_SESSION_INFO_KEY);

	if (session_info == nullptr)
		return;

	if (session_info->is_client)
	{
		_server->OnClientAbandoned(session_info);
		return;
	}


}
void GameServer::GameServerWorker::OnSocketSessionAlived(SocketContext* context)
{
	UniversalSessionInfo* session_info = context->session->GetUserData<uint64_t, UniversalSessionInfo*>(UNIVERSAL_SESSION_INFO_KEY);

	if (session_info == nullptr)
		return;

	if (session_info->is_client)
	{
		_server->OnClientAlived(session_info);
		return;
	}
}

void GameServer::GameServerWorker::OnSocketSessionData(SocketContext* context)
{
	UniversalSessionInfo* session_info = context->session->GetUserData<uint64_t, UniversalSessionInfo*>(UNIVERSAL_SESSION_INFO_KEY);

	if (session_info->is_client)
	{
		if (!_server->_service_ready)
		{
			context->session->CloseSession();
			return;
		}

		_server->OnClientData(session_info, context->header, context->buffer);
		return;
	}

	switch (context->header.packet_type)
	{
	case PacketType::packet_authorize_server_rq:
	{
		packet_authorize_server_rq packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			HandlePacketDeserializeError<decltype(packet)>(session_info);
			return;
		}

		assert(_server->_my_server_info.server_id != packet.server_id);

		packet_authorize_server_rs response;
		response.server_id = _server->_my_server_info.server_id;

		IntraServerInfo* server_session = GetServerSession(packet.server_id);

		if (server_session == nullptr)
		{
			context->session->Send(response, ErrorCode::UnknownServerId);
			return;
		}

		if (server_session->is_connector)
		{
			LOG(LogLevel::Warn, "packet_authorize_server_rq inbound from connector server %llu, logic error", server_session->server_info.server_id);
			session_info->session->CloseSession();
			return;
		}

		server_session->session = context->session;
		session_info->is_authorized = true;
		server_session->is_authorized = true;

		_server->_sector_posting_manager->LinkSession(packet.server_id, context->session);

		context->session->SetUserData(SERVER_ID_KEY, packet.server_id);
		ListenSector(server_session->server_info.server_type, packet.server_id);

		_server->OnIntraServerAccepted(server_session);

		context->session->Send(response);

		return;
	}
	case PacketType::packet_authorize_server_rs:
	{
		packet_authorize_server_rs packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			HandlePacketDeserializeError<decltype(packet)>(session_info);
			return;
		}

		if (context->header.error_code == UnknownServerId)
		{
			packet_authorize_server_rq packet;
			packet.server_id = _server->_my_server_info.server_id;
			packet.server_type = _server->_my_server_info.server_type;

			context->session->Send(packet);

			return;
		}

		assert(_server->_my_server_info.server_id != packet.server_id);

		IntraServerInfo* server_session = GetServerSession(packet.server_id);

		if (server_session == nullptr)
		{
			session_info->session->CloseSession();
			return;
		}

		if (!server_session->is_connector)
		{
			LOG(LogLevel::Warn, "packet_authorize_server_rs inbound from acceptor server %llu, logic error", server_session->server_info.server_id);
			session_info->session->CloseSession();
			return;
		}

		server_session->session = context->session;
		server_session->is_authorized = true;
		session_info->is_authorized = true;

		context->session->SetUserData(SERVER_ID_KEY, packet.server_id);

		_server->_sector_posting_manager->LinkSession(packet.server_id, context->session);

		ListenSector(server_session->server_info.server_type, packet.server_id);

		_server->OnIntraServerConnected(server_session);

		return;
	}
	case PacketType::packet_register_minion_server_rs:
	{
		packet_register_minion_server_rs packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			HandlePacketDeserializeError<decltype(packet)>(session_info);
			return;
		}

		_server->_my_server_info = packet.server_info;
		_server->_loopback_session->server_info = packet.server_info;

		_server->_sector_posting_manager->LinkSession(packet.server_info.server_id, _server->_loopback_session->session);
		_server->_current_universal_session_id = packet.server_info.server_id * 1000000000000000 + 100000000000001;

		_server->_service_ready = true;
		return;
	}
	case PacketType::packet_recovery_minion_server_rs:
	{
		packet_recovery_minion_server_rs packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			HandlePacketDeserializeError<decltype(packet)>(session_info);
			return;
		}

		if (context->header.error_code != ErrorCode::None)
		{
			LOG(LogLevel::Warn, "recovery failed try to register minion server");

			_server->Reset();

			packet_register_minion_server_rq request;
			request.server_type = _server->_my_server_info.server_type;
			request.address = _server->_my_server_info.address;

			context->session->Send(request);
		}

		return;
	}
	case PacketType::packet_update_intra_server_info_ps:
	{
		packet_update_intra_server_info_ps packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			HandlePacketDeserializeError<decltype(packet)>(session_info);
			return;
		}

		for (const ServerInfo& leaved_server : packet.leaved_server_info)
		{

			_server->_intra_network_engine->UnregisterConnectorSocket(_server->_server_sessions[leaved_server.server_id].socket_connector_id);

			_server->_server_sessions.erase(leaved_server.server_id);

			_server->_sector_posting_manager->UnsetAll(leaved_server.server_id);
		}

		for (const ServerInfo& joined_server : packet.joined_server_info)
		{
			if (joined_server.server_id == _server->_my_server_info.server_id)
			{
				_server->_server_sessions[joined_server.server_id] = *_server->_loopback_session;
				continue;
			}

			IntraServerInfo& intra_server = _server->_server_sessions[joined_server.server_id];

			intra_server.server_info = joined_server;
			intra_server.is_connector = IsConnector(joined_server.server_type, joined_server.server_id);

			if (intra_server.is_connector)
				intra_server.socket_connector_id = _server->_intra_network_engine->RegisterConnectorSocket(joined_server.address, joined_server.server_id);
		}

		return;
	}
	case PacketType::packet_sector_allocation_ps:
	{
		packet_sector_allocation_ps packet;
		if (!DeserializePacketBody(context->buffer.Data(), context->header.body_size, packet))
		{
			HandlePacketDeserializeError<decltype(packet)>(session_info);
			return;
		}

		for (const SectorAllocationInfo& info : packet.deallocation_info)
		{
			if (info.server_info.server_id == _server->_my_server_info.server_id)
			{
				// _server->_sector_posting_manager->
			}
			
		}

		for (const SectorAllocationInfo& info : packet.allocation_info)
		{
			_server->_sector_posting_manager->AllocateSector(info.sector_id, info.server_info.server_id);
		}
		
		_server->OnSupervisorSectorAllocation(packet);

		return;
	}
	}

	if (!session_info->is_authorized)
	{
		LOG(LogLevel::Warn, "receive session data from unauthorized, session_id: %llu", session_info->universal_session_id);
		session_info->session->CloseSession();
		return;
	}

	IntraServerInfo* server_session = GetServerSession(session_info->session->GetUserData<uint64_t, uint64_t>(SERVER_ID_KEY));

	if (server_session == nullptr)
	{
		LOG(LogLevel::Warn, "unauthorized server, %d", session_info->universal_session_id);
		context->session->CloseSession();
		return;
	}

	_server->OnIntraServerData(server_session, context->header, context->buffer);
}

bool GameServer::Initialize(const ServerType my_server_type, const SocketAddress& my_intra_server_address, NetworkEngine* const intra_network_engine, NetworkEngine* const user_network_engine, const std::vector<IntraServerConfig>& intra_server_config, const SocketAddress& supervisor_address, const GameConfig& game_config)
{
	if (_intra_network_engine != nullptr)
		return false;

	assert(intra_network_engine != nullptr);
	_my_server_info = ServerInfo{ my_server_type, 0, my_intra_server_address };

	_packet_throttler = new PacketThrottler(utility::Milliseconds(game_config.packet_batch_process_time_ms));
	_sector_posting_manager = new SectorPostingManager(_packet_throttler);

	_user_network_engine = user_network_engine;
	_intra_network_engine = intra_network_engine;
	_supervisor_server_info.server_info = ServerInfo{ ServerType::Supervisor, SUPERVISOR_SERVER_ID, supervisor_address };
	
	_game_config = game_config;
	for (const IntraServerConfig& config : intra_server_config)
	{
		auto iterator = _connection_config.find(config.server_type);
		if (iterator != _connection_config.end())
			return false;

		_connection_config[config.server_type] = config;
	}

	_loopback_session = new IntraServerInfo{};
	_loopback_session->universal_session_id = 1;
	_loopback_session->is_authorized = true;
	_loopback_session->session = SocketSessionPtr(
		new LoopbackSocketSession<GameServerWorker>(_worker, intra_network_engine->GetSocketServer()->GetServerConfig().read_buffer_pool_size, intra_network_engine->GetSocketServer()->GetServerConfig().read_buffer_capacity),
		nullptr
	);

	_supervisor_server_info.universal_session_id = 2;
	_supervisor_server_info.is_authorized = true;
	_intra_network_engine->RegisterConnectorSocket(supervisor_address, SUPERVISOR_SERVER_ID);

	_server_sessions[SUPERVISOR_SERVER_ID] = _supervisor_server_info;

	return InitializeGameServer();
}

void GameServer::GameServerWorker::ProcessLoopbackData(SocketBuffer* const buffer)
{
	DynamicBufferCursor<SocketBuffer> cursor(buffer);

	while (cursor.RemainBytes() >= PACKET_HEADER_SIZE)
	{
		cursor.Seek(PACKET_LENGTH_SIZE);

		PacketHeader header = DeserializePacketHeader(cursor.Data(), buffer->capacity - cursor.Cursor());

		cursor.Seek(PACKET_HEADER_SIZE);

		_server->OnIntraServerData(_server->_loopback_session, header, cursor);

		cursor.Seek(header.body_size);
	}

	assert(cursor.RemainBytes() == 0);

	_server->_loopback_session->session->ReleaseReadBuffer(buffer);
}

void GameServer::Reset()
{
	for (const auto& pair : _server_sessions)
	{
		if (pair.second.socket_connector_id > 0)
			_intra_network_engine->UnregisterConnectorSocket(pair.second.socket_connector_id);

		_sector_posting_manager->UnsetAll(pair.second.server_info.server_id);
	}

	for (const auto& pair : _active_sessions)
	{
		pair.second.session->CloseSession();
	}

	_active_sessions.clear();
	_server_sessions.clear();

	_sector_posting_manager->UnsetAll(_my_server_info.server_id);
	
	_service_ready = false;

	OnSupervisorResetMinionServer();

	_server_sessions[SUPERVISOR_SERVER_ID] = _supervisor_server_info;
}