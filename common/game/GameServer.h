#pragma once

#include "../engine/NetworkEngine.h"
#include "SectorPostingManager.h"

constexpr uint64_t SUPERVISOR_SERVER_ID = 1;

enum class IntraServerConnectionPolicy
{
	All,
	Conditional,
	Not,
};

enum class IntraServerSectorListeningPolicy
{
	All,
	Conditional,
	Not,
};

struct IntraServerConfig
{
	ServerType server_type;
	IntraServerConnectionPolicy connection_policy;
	IntraServerSectorListeningPolicy sector_listening_policy;
};

struct UniversalSessionInfo
{
	uint64_t universal_session_id = 0;
	SocketSessionPtr session = SocketSessionPtr(nullptr);

	bool is_authorized = false;
	bool is_client = false;

};

struct IntraServerInfo : public UniversalSessionInfo
{
	ServerInfo server_info;
	uint64_t socket_connector_id = 0;

	bool is_connector = false;
};

class GameServer
{
private:
	class GameServerWorker : public NetworkEngineWorker
	{
	private:
		GameServer* _server;

		IntraServerInfo* GetServerSession(const uint64_t server_id)
		{
			auto iterator = _server->_server_sessions.find(server_id);

			return iterator == _server->_server_sessions.end() ? nullptr : &iterator->second;
		}

		bool IsConnector(const ServerType server_type, const uint64_t server_id)
		{
			auto iterator = _server->_connection_config.find(server_type);
			if (iterator == _server->_connection_config.end())
				return false;

			if (iterator->second.connection_policy == IntraServerConnectionPolicy::Conditional)
				return _server->CheckIntraServerConnectivity(server_type, server_id);
			else if (iterator->second.connection_policy == IntraServerConnectionPolicy::Not)
				return false;
			else if (server_type != _server->_my_server_info.server_type)
				return true;

			return _server->_my_server_info.server_id > server_id;
		}

		void ListenSector(const ServerType server_type, const uint64_t server_id)
		{
			auto iterator = _server->_connection_config.find(server_type);
			if (iterator == _server->_connection_config.end())
				return;

			for (const SectorInfo& sector_info : SectorGridList)
			{
				bool is_listen = true;

				if (iterator->second.sector_listening_policy == IntraServerSectorListeningPolicy::Conditional)
					is_listen = _server->CheckIntraServerSectorListening(server_type, server_id, sector_info.sector_id);
				else if (iterator->second.sector_listening_policy == IntraServerSectorListeningPolicy::Not)
					is_listen = false;

				if (is_listen)
					_server->_sector_posting_manager->AddSectorListener(sector_info.sector_id, server_id);
			}
		}

	public:
		GameServerWorker(GameServer* const server) : _server(server) {}

		// loopback data
		void ProcessLoopbackData(SocketBuffer* const buffer);

		virtual void OnSocketSessionConnected(SocketContext* context) override;
		virtual void OnSocketSessionAccepted(SocketContext* context) override;
		virtual void OnSocketSessionAbandoned(SocketContext* context) override;
		virtual void OnSocketSessionAlived(SocketContext* context) override;
		virtual void OnSocketSessionClosed(SocketContext* context) override;
		virtual void OnSocketSessionData(SocketContext* context) override;
		virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) override
		{
			return _server->Update(current_time, delta_time);
		}

		virtual void UpdateEveryTick(const WorkerTimeUnit current_time) override
		{
			_server->_current_epoch_timestamp = utility::CurrentTimeEpoch<utility::Milliseconds>();
			_server->UpdateEveryTick(current_time);
			_server->_packet_throttler->TryFlushPacket();
			static_cast<LoopbackSocketSession<GameServerWorker>*>(_server->_loopback_session->session._UnsafePtr())->ProcessLoopbackPacket();
		}
	};

	void Reset();

	IntraServerInfo* _loopback_session = nullptr;
	uint64_t _loopback_session_id = 1;

protected:
	uint64_t _current_universal_session_id = 100;

	IntraServerInfo _supervisor_server_info;
	ServerInfo _my_server_info;

	std::unordered_map<ServerType, IntraServerConfig> _connection_config;

	// key: server id
	std::unordered_map<uint64_t, IntraServerInfo> _server_sessions;

	// key: universal session id
	std::unordered_map<uint64_t, UniversalSessionInfo> _active_sessions;

	PacketThrottler* _packet_throttler = nullptr;
	GameServerWorker* _worker = nullptr;

	NetworkEngine* _user_network_engine = nullptr;
	NetworkEngine* _intra_network_engine = nullptr;

	SectorPostingManager* _sector_posting_manager = nullptr;

	GameConfig _game_config;

	utility::Milliseconds _current_epoch_timestamp;

	bool _service_ready = false;
public:

	GameServer();

	bool Initialize(
		const ServerType my_server_type,
		const SocketAddress& my_intra_server_address,
		NetworkEngine* const intra_network_engine,
		NetworkEngine* const user_network_engine,
		const std::vector<IntraServerConfig>& intra_server_config,
		const SocketAddress& supervisor_address,
		const GameConfig& game_config
	);

	virtual bool InitializeGameServer() = 0;

	operator NetworkEngineWorker* ()
	{
		return _worker;
	}

	virtual void OnSupervisorRegisteredMinionServer(const ServerInfo& my_server_info) {}
	virtual void OnSupervisorResetMinionServer() {}

	virtual void OnSupervisorSectorAllocation(const packet_sector_allocation_ps& packet) {}

	virtual void OnClientAccepted(UniversalSessionInfo* client_session) {}
	virtual void OnClientClosed(UniversalSessionInfo* client_session) {}
	virtual void OnClientAbandoned(UniversalSessionInfo* client_session) {}
	virtual void OnClientAlived(UniversalSessionInfo* client_session) {}

	virtual void OnIntraServerConnected(IntraServerInfo* server_session) {}
	virtual void OnIntraServerAccepted(IntraServerInfo* server_session) {}
	virtual void OnIntraServerClosed (IntraServerInfo* server_session) {}
	
	virtual void OnClientData(UniversalSessionInfo* client_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer) {}
	virtual void OnIntraServerData(IntraServerInfo* server_session, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer) {}

	virtual bool CheckIntraServerConnectivity(const ServerType server_type, const uint64_t server_id) { return true; }
	virtual bool CheckIntraServerSectorListening(const ServerType server_type, const uint64_t server_id, const uint64_t sector_id) { return true; }

	virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) { return true; }
	virtual void UpdateEveryTick(const WorkerTimeUnit current_time) {}
};

template <NetworkPacketConcept T>
void HandlePacketDeserializeError(UniversalSessionInfo* const session_info)
{
	// constexpr std::string 지원 시 컴파일 타임 문자열로 변환
	// constexpr
	std::string format_string = std::string("failed to deserialize ") + typeid(T).name() + " packet, session: %llu";

	LOG(LogLevel::Warn, format_string.c_str(), session_info->universal_session_id);
	session_info->session->CloseSession();
}

#pragma warning ( disable : 5103 )

#define PACKET_CASE(session, packet_type, handler) \
	case PacketType::##packet_type##:\
	{\
		packet_type packet;\
		if (!DeserializePacketBody(buffer.Data(), header.body_size, packet))\
		{\
			HandlePacketDeserializeError<decltype(packet)>(session);\
			return;\
		}\
		handler##(session, header, packet);\
		break;\
	}