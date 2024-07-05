#pragma once

#include "../concurrent/LinearWorker.h"
#include "../utility/Logger.h"
#include "SocketContext.h"

class NetworkEngine;

class NetworkEngineWorker : public LinearWorker<NetworkContext*>
{

public:
	// virtual bool Update(const WorkerTimeUnit delta_time) override;
	virtual void UpdateContext(const std::vector<NetworkContext*>& contexts, const WorkerTimeUnit current_time, const WorkerTimeUnit delta_time) override
	{
		for (NetworkContext* context : contexts)
		{
			while (context != nullptr)
			{
				switch (context->network_subject)
				{
				case NetworkSubject::Socket:
				{
					SocketContext* socket_context = static_cast<SocketContext*>(context);
					switch (context->context_type)
					{
					case ContextType::SessionAccepted:
						OnSocketSessionAccepted(socket_context);
						break;
					case ContextType::SessionConnected:
						OnSocketSessionConnected(socket_context);
						break;
					case ContextType::SessionAbandoned:
						OnSocketSessionAbandoned(socket_context);
						if (!socket_context->session->ChangeSessionState(SessionState::Abandoned, SessionState::Wait))
							assert(false);

						break;
					case ContextType::SessionAlived:
						OnSocketSessionAlived(socket_context);
						break;
					case ContextType::SessionClosed:
						OnSocketSessionClosed(socket_context);
						socket_context->Release();
						context = nullptr;
						continue;
					case ContextType::SessionData:
						assert(socket_context->session->GetSessionState() == SessionState::Opened);
						OnSocketSessionData(socket_context);
						break;
					}

					socket_context->Release();

					break;
				}

				}

				context = context->next;
			}
		}
	}

	virtual void OnSocketSessionConnected(SocketContext* context) {}
	virtual void OnSocketSessionAccepted(SocketContext* context) {}
	virtual void OnSocketSessionAbandoned(SocketContext* context) {}
	virtual void OnSocketSessionAlived(SocketContext* context) {}
	virtual void OnSocketSessionClosed(SocketContext* context) {}
	virtual void OnSocketSessionData(SocketContext* context) {}

	/*
	virtual void OnRedisSessionConnected(RedisContext* context) {}
	virtual void OnRedisSessionAccepted(RedisContext* context) {}
	virtual void OnRedisSessionAbandoned(RedisContext* context) {}
	virtual void OnRedisSessionClosed(RedisContext* context) {}
	virtual void OnRedisSessionData(RedisContext* context) {}
	*/
};