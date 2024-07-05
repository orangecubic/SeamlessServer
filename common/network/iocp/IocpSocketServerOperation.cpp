#include "IocpSocketServer.h"
#include "IocpTransferStream.h"
#include "IocpAcceptorStream.h"
#include "IocpConnectorStream.h"
#include <iostream>

thread_local char address_buffer[32];

void IocpSocketServer::IoAccept(IoContext* const io_context, const DWORD bytes_transferred)
{
	SOCKADDR* local_addr,* remote_addr;
	int local_addr_len = 0, remote_addr_len = 0;
	WinSockFnTable::GetAcceptExSockAddrs(io_context->ptr, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &local_addr, &local_addr_len, &remote_addr, &remote_addr_len);

	DWORD address_buffer_length = sizeof(SOCKADDR);
	SOCKADDR_IN* inet_addr = reinterpret_cast<SOCKADDR_IN*>(remote_addr);

	PCSTR result = inet_ntop(AF_INET, &(inet_addr->sin_addr), address_buffer, sizeof(address_buffer));
	if (result != nullptr)
	{
		io_context->operation_stream->SetSocketAddress(
			SocketAddress::New(address_buffer, htons(inet_addr->sin_port)
		));

		io_context->operation_stream->SetConnectionStatus(true);
		io_context->operation_stream._UnsafeRetain();

		return;
	}

	throw new std::exception("cannot convert address to ip");

}

void IocpSocketServer::IoRead(IoContext* const io_context, const DWORD bytes_transferred)
{
	io_context->length += bytes_transferred;

	if (bytes_transferred > 0)
	{
		_event_handler->OnRead(io_context->operation_stream, io_context, io_context->attachment);
		return;
	}

	io_context->operation_stream->TransmitDisconnect(io_context->attachment);
	io_context->operation_stream->ReleaseReadBuffer(io_context);
}

void IocpSocketServer::IoWrite(IoContext* const io_context, const DWORD bytes_transferred)
{	
	_event_handler->OnWrite(io_context->operation_stream, io_context, io_context->attachment);
}

void IocpSocketServer::IoDisconnect(IoContext* const io_context, const DWORD bytes_transferred)
{
	_event_handler->OnDisconnect(io_context->operation_stream, io_context->attachment);

	// release stream
	io_context->operation_stream._UnsafeRelease();
}

void IocpSocketServer::IoConnect(IoContext* const io_context, const DWORD bytes_transferred)
{
	// DWORD connection_time = 0;
	// ::setsockopt(io_context->operation_stream->GetSocket(), SOL_SOCKET, SO_CONNECT_TIME, reinterpret_cast<const char*>(&connection_time), sizeof(connection_time));

	int32_t seconds;
	int32_t bytes = sizeof(seconds);

	int32_t result = getsockopt(io_context->operation_stream->GetSocket(), SOL_SOCKET, SO_CONNECT_TIME, (char*)&seconds, (PINT)&bytes);
	
	if (result == SOCKET_ERROR || seconds == SOCKET_ERROR)
	{
		_event_handler->OnConnect(TransferStreamPtr(nullptr), io_context->attachment);

		// io_context->operation_stream->TransmitDisconnect(io_context->attachment);
		return;
	}

	io_context->operation_stream->SetConnectionStatus(true);

	::setsockopt(io_context->operation_stream->GetSocket(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);

	io_context->operation_stream._UnsafeRetain();
	_event_handler->OnConnect(io_context->operation_stream, io_context->attachment);

}