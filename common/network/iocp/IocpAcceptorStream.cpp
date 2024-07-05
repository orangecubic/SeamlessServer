#include "IocpAcceptorStream.h"
#include "IocpSocketServer.h"
#include "IoContext.h"

bool IocpAcceptorStream::TransmitAccept(TransferStream* const transfer_stream)
{
	IoContext* io_context = static_cast<IoContext*>(transfer_stream->AllocateReadBuffer());

	assert(io_context != nullptr);

	Socket accepted_socket = static_cast<Socket>(transfer_stream->GetSocket());

	io_context->operation = IoOperation::Accept;
	io_context->operation_stream = IntrusivePtr<TransferStream>(transfer_stream, SocketObjectDeleter);

	// TransferStream is already allocated in context->stream
	// context->stream

	DWORD bytes_read = 0;
	bool result = WSAOperationResult(
		WinSockFnTable::AcceptEx(GetSocket(), accepted_socket, io_context->ptr, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes_read, static_cast<OVERLAPPED*>(io_context))
	);

	if (result)
		return true;

	transfer_stream->ReleaseReadBuffer(io_context);

	return false;
}