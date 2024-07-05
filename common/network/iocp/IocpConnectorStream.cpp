#include "IocpSocketServer.h"
#include "IocpConnectorStream.h"
#include "IoContext.h"

void IocpConnectorStream::TransmitConnect(const uint64_t attachment)
{
	IoContext* io_context = static_cast<IoContext*>(AllocateWriteBuffer());

	assert(io_context != nullptr);

	sockaddr_in addr = GetSocketAddress().ToInetSocketAddress();

	io_context->operation = IoOperation::Connect;
	io_context->attachment = attachment;
	io_context->operation_stream = IntrusivePtr<TransferStream>(this, SocketObjectDeleter);

	DWORD bytes_read = 0;

	BOOL result = WinSockFnTable::ConnectEx(GetSocket(), (sockaddr*)&addr, sizeof(addr), nullptr, 0, &bytes_read, static_cast<OVERLAPPED*>(io_context));
	DWORD err = GetLastError();
	int error = WSAGetLastError();
	if (result || WSAGetLastError() == WSA_IO_PENDING)
		return;

	PostQueuedCompletionStatus(GetIocpHandle(), 0, reinterpret_cast<ULONG_PTR>(this), static_cast<OVERLAPPED*>(io_context));
}