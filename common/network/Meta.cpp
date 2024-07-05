#include "Meta.h"
#include <iostream>

#ifdef _WIN32

LPFN_CONNECTEX WinSockFnTable::ConnectEx = nullptr;
LPFN_ACCEPTEX WinSockFnTable::AcceptEx = nullptr;
LPFN_DISCONNECTEX WinSockFnTable::DisconnectEx = nullptr;
LPFN_GETACCEPTEXSOCKADDRS WinSockFnTable::GetAcceptExSockAddrs = nullptr;

void InitializeSocketService()
{
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) == SOCKET_ERROR)
	{
		std::cout << WSAGetLastError() << std::endl;
		throw std::exception("wsa startup failed");
		return;
	}
	WinSockFnTable::Initialize();
}

int SetSocketOptions(Socket socket, const SocketOptions& options)
{
	int result = 0;
	for (const auto& option : options)
	{
		result += setsockopt(socket, SOL_SOCKET, option.first, reinterpret_cast<const char*>(&option.second), sizeof(decltype(option.second)));
	}

	return result;
}

void WinSockFnTable::Initialize()
{
	Socket socket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	DWORD num_bytes = 0;

	GUID guid = WSAID_CONNECTEX;

	int success = ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void*)&guid, sizeof(guid), (void*)&WinSockFnTable::ConnectEx, sizeof(WinSockFnTable::ConnectEx),
		&num_bytes, NULL, NULL);

	if (success == -1)
		throw new std::exception("cannot initialize ConnectEx function");

	guid = WSAID_ACCEPTEX;

	success = ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void*)&guid, sizeof(guid), (void*)&WinSockFnTable::AcceptEx, sizeof(WinSockFnTable::AcceptEx),
		&num_bytes, NULL, NULL);

	if (success == -1)
		throw new std::exception("cannot initialize AcceptEx function");

	guid = WSAID_GETACCEPTEXSOCKADDRS;

	success = ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void*)&guid, sizeof(guid), (void*)&WinSockFnTable::GetAcceptExSockAddrs, sizeof(WinSockFnTable::GetAcceptExSockAddrs),
		&num_bytes, NULL, NULL);

	if (success == -1)
		throw new std::exception("cannot initialize GetAcceptExSockAddrs function");

	guid = WSAID_DISCONNECTEX;

	success = ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void*)&guid, sizeof(guid), (void*)&WinSockFnTable::DisconnectEx, sizeof(WinSockFnTable::DisconnectEx),
		&num_bytes, NULL, NULL);

	if (success == -1)
		throw new std::exception("cannot initialize DisconnectEx function");

	closesocket(socket);
}

bool WSAOperationResult(int result)
{
	if (result == SOCKET_ERROR)
	{
		int last_error = WSAGetLastError();

		return last_error == WSA_IO_PENDING;
	}

	return true;
}

#endif

SocketAddress SocketAddress::New(const std::string_view& ip, const uint16_t port)
{
	assert(inet_addr(ip.data()) != -1);

	SocketAddress address{ 0, 0 };
	std::memcpy(address.ip.data(), ip.data(), ip.size());
	address.ip[ip.size()] = '\0';
	address.port = port;

	return address;
}

const SocketAddress EmptyAddress = SocketAddress::New("0.0.0.0", 0);

const SocketAddress& SocketAddress::Empty()
{
	return EmptyAddress;
}

sockaddr_in SocketAddress::ToInetSocketAddress() const
{
	sockaddr_in addr{ 0 };
	addr.sin_addr.s_addr = inet_addr(ip.data());
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	return addr;
}