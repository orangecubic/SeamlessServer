#include "NetworkEngine.h"
#include "protocol/Protocol.h"
#include "SocketContext.h"
#include "../utility/Time.h"
#include "../utility/Logger.h"

enum class StreamType : uint8_t
{
	Connector = 1,
	Acceptor = 2
};

struct SocketStreamExtension
{
	SocketSessionPtr session;
	uint64_t connector_id = 0;
	utility::Nanoseconds connection_created_time;
	uint32_t ping_ms = 100;
	uint16_t last_error_code;

	StreamType stream_type;
	bool graceful_shutdown_flag = false;
};

void SocketSessionInitializer::Initialize(SocketSession* const session, const uint64_t id, const uint64_t param)
{
	NetworkEngine* engine = reinterpret_cast<NetworkEngine*>(param);

	session->Initialize(id);
	session->SetNetworkEngine(engine);
}

bool NetworkEngine::Initialize(const NetworkEngineOption& option, SocketServer* const socket_server, NetworkEngineWorker* const worker)
{
	assert(sizeof(SocketContext) <= sizeof(SocketBuffer::reserved));

	for (uint64_t index = 0; index < socket_server->GetServerConfig().worker_count; index++)
	{
		_worker_infos.push_back(new SocketWorkerInfo());
	}
	socket_server->SetSocketEventHandler(this);

	_socket_session_pool = new SocketSessionPool(option.max_session_count, 64, reinterpret_cast<uint64_t>(this));

	_option = option;
	_socket_server = socket_server;
	_worker = worker;

	return true;
}

int NetworkEngine::Start()
{
	int result = _socket_server->Start();
	if (result == 0)
		_worker->Start(utility::Milliseconds(_option.worker_update_tick_ms));

	return result;
}

void NetworkEngine::SendSocketContext(const DynamicBufferCursor<SocketBuffer>& buffer, const ContextType& context_type, const PacketHeader& header, const SocketSessionPtr& session, const uint64_t attachment)
{
	SocketContext* context = PrepareSocketContext(buffer, context_type, header, session, attachment);
	if (!_worker->Submit(context))
	{
		LOG(LogLevel::Error, "worker queue is full, reduce load to disconnect this session %llu", session->GetSessionId());
		
		buffer->ReleaseN(buffer->Ref());
		session->ReleaseReadBuffer(buffer);
		session->Send(packet_server_is_busy_ps{}, ErrorCode::None, true);
		session->CloseSession();
	}
}

SocketContext* NetworkEngine::PrepareSocketContext(const DynamicBufferCursor<SocketBuffer>& buffer, const ContextType& context_type, const PacketHeader& header, const SocketSessionPtr& session, const uint64_t attachment, uint32_t context_sequence)
{
	// reserved ������ �ּ� SocketContext 1���� ������ �� �־����
	assert(sizeof(SocketBuffer::reserved) >= sizeof(SocketContext));

	if (sizeof(SocketContext) * (context_sequence + 1) > sizeof(SocketBuffer::reserved))
		return nullptr;

	SocketContext* context = reinterpret_cast<SocketContext*>(static_cast<SocketBuffer*>(buffer)->reserved.data()) + context_sequence;
	context->network_subject = NetworkSubject::Socket;
	context->context_type = context_type;
	context->header = header;
	context->session = session;
	context->attachment = attachment;
	context->buffer = buffer;
	context->next = nullptr;

	buffer->Retain();

	return context;
}

uint64_t NetworkEngine::RegisterConnectorSocket(const SocketAddress& remote_address, const uint64_t attachment)
{
	ConnectorInfo connector_info;
	connector_info.connector_id = _current_connector_id++;
	connector_info.remote_address = remote_address;
	connector_info.attachment = attachment;

	_connector_list.Insert(connector_info.connector_id, connector_info);

	// connector id�� Connect Operation�� attachment�� ����
	_socket_server->Connect(remote_address, {}, connector_info.connector_id);

	return connector_info.connector_id;
}

void NetworkEngine::UnregisterConnectorSocket(uint64_t connector_id, bool delete_attachment)
{
	ConnectorInfo connector_info;
	if (!_connector_list.Remove(connector_id, connector_info))
		return;

	if (connector_info.stream.Valid())
		connector_info.stream->TransmitDisconnect(GRACEFUL_SHUTDOWN_CODE);

	if (delete_attachment)
		delete reinterpret_cast<void*>(connector_info.attachment);
}

void NetworkEngine::OnAccept(const TransferStreamPtr& stream)
{
	SocketWorkerInfo* worker_info = GetWorkerInfo(stream);

	worker_info->_activated_sockets[stream->GetId()] = stream;

	SocketStreamExtension* extension_info = reinterpret_cast<SocketStreamExtension*>(stream->GetUserData());
	if (extension_info == nullptr)
	{
		extension_info = new SocketStreamExtension();
		stream->SetUserData(reinterpret_cast<uint64_t>(extension_info));
	}

	extension_info->session.Release();
	extension_info->stream_type = StreamType::Acceptor;
	extension_info->connection_created_time = utility::CurrentTick();
	extension_info->ping_ms = 100;

	SocketBuffer* read_buffer = stream->AllocateReadBuffer();
	stream->TransmitRead(read_buffer, stream->GetId());

	LOG(LogLevel::Info, "accept client %s:%d, id: %llu", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());
}

void NetworkEngine::OnConnect(const TransferStreamPtr& stream, const uint64_t attachment)
{
	ConnectorInfo connector_info;
	bool valid_connector = _connector_list.TryGet(attachment, connector_info);

	if (!stream.Valid())
	{
		if (valid_connector)
		{
			LOG(LogLevel::Warn, "failed to connect remote server, try reconnect connector: %lld", attachment);
			_socket_server->Connect(connector_info.remote_address, {}, connector_info.connector_id);
		}

		return;
	}

	SocketWorkerInfo* worker_info = GetWorkerInfo(stream);

	worker_info->_activated_sockets[stream->GetId()] = stream;

	SocketStreamExtension* extension_info = reinterpret_cast<SocketStreamExtension*>(stream->GetUserData());
	if (extension_info == nullptr)
	{
		extension_info = new SocketStreamExtension();
		stream->SetUserData(reinterpret_cast<uint64_t>(extension_info));
	}

	if (!valid_connector)
	{
		stream->TransmitDisconnect(attachment);
		return;
	}

	connector_info.stream = stream;
	_connector_list.Insert(connector_info.connector_id, connector_info);
	
	extension_info->session.Release();
	extension_info->stream_type = StreamType::Connector;
	extension_info->connector_id = attachment; // attachment = connector id
	extension_info->connection_created_time = utility::CurrentTick();
	extension_info->ping_ms = 100;

	SocketBuffer* buffer = stream->AllocateWriteBuffer();

	// session reconnect ��� Ȱ��ȭ ��
	if (connector_info.session_id != 0)
	{
		packet_session_auth_rq packet;
		packet.session_id = connector_info.session_id;
		packet.session_key = connector_info.session_key._buffer;

		SerializePacket(packet, ErrorCode::None, buffer->ptr, buffer->capacity, buffer->length);

		stream->TransmitWrite(buffer, 0, 0);
	}
	else
	{
		// �ű� session ���� ��û
		packet_session_create_rq packet;

		SerializePacket(packet, ErrorCode::None, buffer->ptr, buffer->capacity, buffer->length);

		stream->TransmitWrite(buffer, 0, 0);
	}

	SocketBuffer* read_buffer = stream->AllocateReadBuffer();
	stream->TransmitRead(read_buffer, stream->GetId());

	LOG(LogLevel::Info, "connect successfully %s:%d, id: %llu", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());
}

ContextType NetworkEngine::ProcessSessionPacket(const TransferStreamPtr& stream, const PacketHeader& header, const DynamicBufferCursor<SocketBuffer>& buffer, ConnectorInfo* connector_info)
{
	SocketWorkerInfo* worker_info = GetWorkerInfo(stream);
	SocketStreamExtension* extension_info = reinterpret_cast<SocketStreamExtension*>(stream->GetUserData());

	SocketSessionPtr& session = extension_info->session;

	ContextType context_type = ContextType::SessionError;

	switch (header.packet_type)
	{
	case PacketType::packet_session_create_rq:
	{
		packet_session_create_rq packet;
		if (!DeserializePacketBody(buffer.Data(), header.body_size, packet))
			return context_type;

		session = _socket_session_pool->Pop();

		// Session ���� ����
		if (!session.Valid())
			return context_type;

		session->ChangeSessionState(SessionState::Closed, SessionState::Opened);

		context_type = ContextType::SessionAccepted;
		session->ResetSessionKey();
		session->ResetSocketStream(stream);

		packet_session_create_rs response;
		response.session_id = session->GetSessionId();
		response.session_key = session->GetSessionKey()._buffer;

		SocketBuffer* new_buffer = stream->AllocateWriteBuffer();

		SerializePacket(response, ErrorCode::None, new_buffer->ptr, new_buffer->capacity, new_buffer->length);

		stream->TransmitWrite(new_buffer, 0, 0);

		break;
	}
	// if connector socket
	case PacketType::packet_session_create_rs:
	{
		// connector ����
		if (extension_info->stream_type != StreamType::Connector)
			return context_type;

		packet_session_create_rs packet;
		if (!DeserializePacketBody(buffer.Data(), header.body_size, packet))
			return context_type;

		bool success = _connector_list.TryGet(extension_info->connector_id, *connector_info);

		// network engine ���� ���� �ܺ� �����忡 ���� connector�� ��� ������ ���
		if (!success)
			return context_type;

		session = _socket_session_pool->Pop();

		// Session ���� ����
		if (!session.Valid())
			return context_type;

		session->ChangeSessionState(SessionState::Closed, SessionState::Opened);

		session->ResetSessionKey(packet.session_key);
		session->ResetSocketStream(stream);

		if (_option.use_session_reconnect)
		{
			connector_info->session_id = session->GetSessionId();
			connector_info->session_key = session->GetSessionKey();
		}

		context_type = ContextType::SessionConnected;
		break;
	}
	case PacketType::packet_session_auth_rq:
	{
		packet_session_auth_rq packet;
		if (!DeserializePacketBody(buffer.Data(), header.body_size, packet))
			return context_type;

		bool key_equal = false, state_valid = false;

		// ������ ���� ����Ʈ���� session id�� ��ġ�ϴ� ������ �ִ� �� Ȯ�� + session key�� �����ϰ� state ���濡 �������� �� session Ȱ��ȭ
		bool success = _abandoned_session_list.RemoveIf(packet.session_id, session, [&](const SocketSessionPtr& session) {
			key_equal = session->GetSessionKey() == packet.session_key;
			state_valid = session->ChangeSessionState(SessionState::Wait, SessionState::Opened);

			return key_equal && state_valid;
		});

		if (!success)
		{
			SocketBuffer* buffer = stream->AllocateWriteBuffer();
			packet_session_auth_rq response;
			SerializePacket(response, key_equal ? ErrorCode::SessionIsNotReady : ErrorCode::SessionAuthFailed, buffer->ptr, buffer->capacity, buffer->length);
			stream->TransmitWrite(buffer, 0, 0);

			return context_type;
		}

		session->ResetSocketStream(stream);

		worker_info->_abandoned_sessions.erase(session->GetSessionId());
		context_type = ContextType::SessionAlived;

		break;
	}
	case PacketType::packet_session_auth_rs:
	{
		packet_session_auth_rs packet;
		if (!DeserializePacketBody(buffer.Data(), header.body_size, packet))
			return context_type;

		bool success = _connector_list.TryGet(extension_info->connector_id, *connector_info);

		// network engine ���� ���� �ܺ� �����忡 ���� connector�� ��� ������ ���
		if (!success)
			return context_type;

		if (header.error_code != ErrorCode::None)
		{
			// session�� �غ�� �� ���� ��õ�
			if (header.error_code == ErrorCode::SessionIsNotReady)
			{
				packet_session_auth_rq packet;
				packet.session_id = connector_info->session_id;
				packet.session_key = connector_info->session_key._buffer;

				SerializePacket(packet, ErrorCode::None, buffer->ptr, buffer->capacity, buffer->length);

				stream->TransmitWrite(buffer, 0, 0);
			}
			else
			{
				// ���� ���д� ���� ������ �ݰ� �� ���� ���� ��û
				success = _abandoned_session_list.Remove(connector_info->session_id, session);
				if (success)
				{
					SendSocketContext(buffer, ContextType::SessionClosed, header, session, 0);
					session.Release();
				}

				connector_info->session_id = 0;
				connector_info->session_key = SessionKey::Empty();

				// �ű� session ���� ��û
				packet_session_create_rq packet;

				SerializePacket(packet, ErrorCode::None, buffer->ptr, buffer->capacity, buffer->length);

				stream->TransmitWrite(buffer, 0, 0);
			}

			return context_type;
		}

		// _abandoned_session_list���� �̹� ���ŵ�����(=timeout���� ���� SessionClose �޼����� ���� Worker�� ���޵� ��Ȳ) ���� �������� �����ϰ� �ű� ���� ����
		success = _abandoned_session_list.RemoveIf(connector_info->session_id, session, [](const SocketSessionPtr& session) {
			return session->ChangeSessionState(SessionState::Wait, SessionState::Opened);
		});

		if (!success)
		{
			connector_info->session_id = 0;
			connector_info->session_key = SessionKey::Empty();

			// �ű� session ���� ��û
			packet_session_create_rq packet;

			SerializePacket(packet, ErrorCode::None, buffer->ptr, buffer->capacity, buffer->length);

			stream->TransmitWrite(buffer, 0, 0);
			return context_type;
		}

		session->ResetSocketStream(stream);

		worker_info->_abandoned_sessions.erase(session->GetSessionId());
		context_type = ContextType::SessionAlived;

		break;
	}

	default:
		return context_type;
	}

	worker_info->_opened_sessions[session->GetSessionId()] = session;

	session->UpdateHeartbeatSendingTime(utility::CurrentTick<utility::Milliseconds>());
	session->UpdateHeartbeatReceivingTime(utility::CurrentTick<utility::Milliseconds>());

	session->ResetUserData();

	return context_type;
}

void NetworkEngine::OnRead(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment)
{
	SocketWorkerInfo* worker_info = GetWorkerInfo(stream);
	SocketStreamExtension* extension_info = reinterpret_cast<SocketStreamExtension*>(stream->GetUserData());

	SocketContext* first_context = reinterpret_cast<SocketContext*>(buffer->reserved.data());
	SocketContext* last_context = nullptr;

	DynamicBufferCursor<SocketBuffer> buffer_cursor(buffer);

	uint32_t context_sequence = 0;

	while (buffer_cursor.RemainBytes() > PACKET_LENGTH_SIZE)
	{
		PacketLengthType packet_length;
		std::memcpy(&packet_length, buffer_cursor.Data(), PACKET_LENGTH_SIZE);

		if (buffer_cursor.RemainBytes() - PACKET_LENGTH_SIZE < packet_length)
			break;
		
		buffer_cursor.Seek(PACKET_LENGTH_SIZE);

		PacketHeader header = DeserializePacketHeader(buffer_cursor.Data(), PACKET_HEADER_SIZE);

		buffer_cursor.Seek(PACKET_HEADER_SIZE);

		if (buffer_cursor.RemainBytes() < header.body_size)
		{
			LOG(LogLevel::Warn, "packet header size mismatch %s:%d, %u", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());

			if (extension_info->session.Valid())
				extension_info->session->CloseSession();
			else
				stream->TransmitDisconnect(GRACEFUL_SHUTDOWN_CODE);

			buffer_cursor->ReleaseN(buffer_cursor->Ref());
			stream->ReleaseReadBuffer(buffer_cursor);

			return;
		}

		if (!extension_info->session.Valid())
		{
			ConnectorInfo connector_info;
			ContextType session_context = ProcessSessionPacket(stream, header, buffer_cursor, &connector_info);

			if (session_context == ContextType::SessionError)
			{
				LOG(LogLevel::Warn, "failed to authorize session %s:%d, %u", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());
				buffer_cursor.Seek(header.body_size);
				continue;
			}

			last_context = PrepareSocketContext(buffer_cursor, session_context, header, extension_info->session, connector_info.attachment, context_sequence++);
			if (last_context == nullptr)
			{
				LOG(LogLevel::Warn, "cannot create socket context %s:%d, %u", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());

				stream->TransmitDisconnect(GRACEFUL_SHUTDOWN_CODE);
				stream->ReleaseReadBuffer(buffer_cursor);

				return;
			}

			buffer_cursor.Seek(header.body_size);

			continue;
		}

		if (header.packet_type == PacketType::packet_heartbeat_rq)
		{
			utility::Milliseconds now = utility::CurrentTick<utility::Milliseconds>();

			extension_info->session->UpdateHeartbeatReceivingTime(now);
			
			extension_info->session->Send(packet_heartbeat_rs{});
			
			buffer_cursor.Seek(header.body_size);

			continue;
		}
		else if (header.packet_type == PacketType::packet_heartbeat_rs)
		{
			utility::Milliseconds now = utility::CurrentTick<utility::Milliseconds>();
			extension_info->session->UpdateHeartbeatReceivingTime(now);

			buffer_cursor.Seek(header.body_size);

			continue;
		}
		else if (header.packet_type == PacketType::packet_session_create_rq || header.packet_type == PacketType::packet_session_create_rs)
		{
			extension_info->session->CloseSession();
			stream->ReleaseReadBuffer(buffer_cursor);

			return;
		}
		else if (header.packet_type == PacketType::packet_session_close_rq)
		{
			extension_info->session->CloseSession();
			stream->ReleaseReadBuffer(buffer_cursor);

			return;
		}

		SocketContext* current_context = PrepareSocketContext(buffer_cursor, ContextType::SessionData, header, extension_info->session, attachment, context_sequence);
		
		if (current_context == nullptr)
		{
			SocketBuffer* new_buffer = extension_info->session->TryAllocateReadBuffer();
			if (new_buffer == nullptr)
			{
				stream->ReleaseReadBuffer(buffer_cursor);
				return;
			}

			new_buffer->length = buffer_cursor.RemainBytes();
			std::memcpy(new_buffer->ptr, buffer_cursor.Data(), new_buffer->length);

			if (!_worker->Submit(first_context))
			{
				LOG(LogLevel::Error, "worker queue is full, reduce load to disconnect this socket %s:%d, %u", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());
				
				extension_info->session->CloseSession();

				stream->ReleaseReadBuffer(buffer_cursor);
				stream->ReleaseReadBuffer(new_buffer);
				return;
			}

			context_sequence = 0;

			first_context = reinterpret_cast<SocketContext*>(new_buffer->reserved.data());
			last_context = nullptr;
			buffer_cursor = DynamicBufferCursor<SocketBuffer>(new_buffer);

			current_context = PrepareSocketContext(buffer_cursor, ContextType::SessionData, header, extension_info->session, attachment, context_sequence);
		}

		if (last_context != nullptr)
			last_context->next = current_context;

		last_context = current_context;

		buffer_cursor.Seek(header.body_size);
		context_sequence++;
	}

	// ��Ŷ�� ������ ���� ���� ��� ���� ��Ȱ��
	if (buffer_cursor.Cursor() <= PACKET_LENGTH_SIZE + PACKET_HEADER_SIZE)
		stream->TransmitRead(buffer_cursor, attachment);
	else
	{
		SocketBuffer* new_buffer = extension_info->session->TryAllocateReadBuffer();
		if (new_buffer == nullptr)
		{
			stream->ReleaseReadBuffer(buffer_cursor);
			return;
		}

		if (buffer_cursor.RemainBytes() > 0)
		{
			std::memcpy(new_buffer->ptr, buffer_cursor.Data(), buffer_cursor.RemainBytes());
			new_buffer->length = buffer_cursor.RemainBytes();
		}

		if (last_context != nullptr)
		{
			if (!_worker->Submit(first_context))
			{
				LOG(LogLevel::Error, "worker queue is full, reduce load to disconnect this socket %s:%d, %u", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());
				
				stream->ReleaseReadBuffer(buffer_cursor);
				stream->ReleaseReadBuffer(new_buffer);

				extension_info->session->Send(packet_server_is_busy_ps{}, ErrorCode::None, true);
				extension_info->session->CloseSession();
				return;
			}
		}
		else
			stream->ReleaseReadBuffer(buffer_cursor);

		stream->TransmitRead(new_buffer, attachment);
	}
}

void NetworkEngine::OnWrite(const TransferStreamPtr stream, SocketBuffer* const buffer, const uint64_t attachment)
{
	SocketWorkerInfo* worker_info = GetWorkerInfo(stream);
	
	stream->ReleaseWriteBuffer(buffer);
}

void NetworkEngine::OnDisconnect(const TransferStreamPtr& stream, const uint64_t attachment)
{
	SocketWorkerInfo* worker_info = GetWorkerInfo(stream);
	SocketStreamExtension* extension_info = reinterpret_cast<SocketStreamExtension*>(stream->GetUserData());

	ContextType context_type = ContextType::SessionClosed;

	worker_info->_activated_sockets.erase(stream->GetId());

	LOG(LogLevel::Info, "socket disconnect %s:%d, %llu", stream->GetSocketAddress().ip.data(), stream->GetSocketAddress().port, stream->GetId());

	if (extension_info->stream_type == StreamType::Connector)
	{
		ConnectorInfo connector_info;
		bool valid_connector = _connector_list.Process(extension_info->connector_id, [&](ConnectorInfo& connector) {
			connector_info = connector;
			connector.stream.Release();
		});

		if (valid_connector)
		{
			LOG(LogLevel::Warn, "connector disconnected stream: %llu, try reconnect connector: %llu", stream->GetId(), connector_info.connector_id);
			connector_info.stream.Release();
			_socket_server->Connect(connector_info.remote_address, {}, extension_info->connector_id);
		}

	}

	if (!extension_info->session.Valid())
		return;

	// ���� ���ᰡ �ƴ� stream�̰�
	// ���� �������� Ȱ��ȭ�� ���¶��
	if (!(attachment & GRACEFUL_SHUTDOWN_CODE) && _option.use_session_reconnect)
	{
		// ���� ���� ���� �õ�
		if (!extension_info->session->ChangeSessionState(SessionState::Opened, SessionState::Abandoned))
		{
			assert(false);
			throw std::exception("fatal error");
			// fatal
		}

		_abandoned_session_list.Insert(extension_info->session->GetSessionId(), extension_info->session);
		worker_info->_abandoned_sessions[extension_info->session->GetSessionId()] = extension_info->session;
		context_type = ContextType::SessionAbandoned;
	}

	SocketBuffer* buffer = stream->AllocateReadBuffer(true);

	worker_info->_opened_sessions.erase(extension_info->session->GetSessionId());
	
	SendSocketContext(buffer, context_type, {}, extension_info->session, 0);

	extension_info->connector_id = 0;
	extension_info->session.Release();
}

thread_local std::vector<uint64_t> deleted_session;

void NetworkEngine::OnTick(const uint32_t worker_index)
{
	SocketWorkerInfo* worker_info = GetWorkerInfo(worker_index);

	utility::Nanoseconds now = utility::CurrentTick();

	for (const auto& pair : worker_info->_activated_sockets)
	{
		SocketStreamExtension* extension_info = reinterpret_cast<SocketStreamExtension*>(pair.second->GetUserData());
		if (!extension_info->session.Valid() && now >= extension_info->connection_created_time + utility::Milliseconds(_option.socket_idle_timeout_ms))
		{
			LOG(LogLevel::Info, "close idle socket %u", pair.first);
			// idle socket timeout
			pair.second->TransmitDisconnect(GRACEFUL_SHUTDOWN_CODE);
		}

		if (extension_info->session.Valid() && extension_info->stream_type == StreamType::Acceptor)
		{
			if (now >= extension_info->session->GetLastHeartbeatSendingTime() + utility::Milliseconds(_option.session_heartbeat_interval_ms))
			{
				packet_heartbeat_rq packet;
				packet.ping = extension_info->ping_ms;

				SocketBuffer* buffer = pair.second->AllocateWriteBuffer(true);

				SerializePacket(packet, ErrorCode::None, buffer->ptr, buffer->capacity, buffer->length);

				pair.second->TransmitWrite(buffer, 0, 0);

				extension_info->session->UpdateHeartbeatSendingTime(utility::TimeCast<utility::Milliseconds>(now));
			}
		}
	}

	for (const auto& pair : worker_info->_opened_sessions)
	{
		if (now >= pair.second->GetLastHeartbeatReceivingTime() + utility::Milliseconds(_option.session_idle_timeout_ms))
		{
			LOG(LogLevel::Info, "close idle session %u", pair.first);
			deleted_session.push_back(pair.first);
			pair.second->CloseSession();
		}
	}

	for (uint64_t id : deleted_session)
		worker_info->_opened_sessions.erase(id);

	deleted_session.clear();

	for (const auto& pair : worker_info->_abandoned_sessions)
	{
		SessionState state = pair.second->GetSessionState();

		if (state != SessionState::Wait)
			continue;

		if (state == SessionState::Opened || state == SessionState::Closed)
		{
			deleted_session.push_back(pair.first);
			continue;
		}

		if (now >= pair.second->GetAbandonedTime() + utility::Milliseconds(_option.session_reconnect_timeout_ms))
		{
			deleted_session.push_back(pair.first);

			SocketSessionPtr session;

			// _abandoned_session_list �� ���ų�, state ���濡 �����ϴ� ���(=�ٸ� �����忡�� �� ������ ��Ȱ��ȭ �߰ų� ���� ���� ���ѹ��� ���), ����Ʈ���� ���Ÿ� ����
			bool success = _abandoned_session_list.RemoveIf(pair.first, session, [](const SocketSessionPtr& session) {
				return session->ChangeSessionState(SessionState::Wait, SessionState::Closed);
			});

			if (!success)
				break;

			LOG(LogLevel::Info, "abandoned session timeout %d", pair.first);

			// sesson reconnect timeout
			SendSocketContext(session->_socket_stream->AllocateReadBuffer(true), ContextType::SessionClosed, {}, pair.second, 0);
		}
	}
	
	for (uint64_t id : deleted_session)
		worker_info->_abandoned_sessions.erase(id);

	deleted_session.clear();
}

SocketServer* NetworkEngine::GetSocketServer()
{
	return _socket_server;
}