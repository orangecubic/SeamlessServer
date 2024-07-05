#pragma once

#include "NetworkEngine.h"

class CloudConfigManager
{
private:

	class CloudConfigWorker : public NetworkEngineWorker
	{
	private:
		NetworkEngine* _engine;
		CloudConfigManager* _manager;
	public:
		CloudConfigWorker(CloudConfigManager* manager, NetworkEngine* const engine);
		virtual void OnSocketSessionConnected(SocketContext* context) override;
		virtual void OnSocketSessionData(SocketContext* context) override;
		virtual bool Update(const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) { return true; }
	};

	ServerType _server_type;
	SocketAddress _config_server_address;

	NetworkEngine* _engine;
	CloudConfigWorker* _worker;

	uint64_t _connector_id = 0;

	LinearWorkQueue<ServerConfig*, 64> _result_queue;
public:

	CloudConfigManager(const ServerType server_type, const SocketAddress& config_server_address);

	void RequestCloudConfig(const uint32_t max_retry_count);
	ServerConfig GetCloudConfig();
};