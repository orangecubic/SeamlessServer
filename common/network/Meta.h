#pragma once

#include "../memory/Buffer.h"
#include "../memory/RefCounter.h"
#include <string>
#include <cassert>
#include <vector>
#include <array>

struct SocketBuffer : public DynamicBuffer, RefCounter<false>
{
	std::array<unsigned char, 8192> reserved;
};

using SocketOptions = std::vector<std::pair<int32_t, int32_t>>;

/*
enum class ErrorCode : uint32_t
{
	ACTIVE,
	PEER_RESET,
	CLOSED_STREAM,
	TIMEOUT,
};
*/

enum class IoOperation : uint8_t
{
	None = 0,
	Accept = 1,
	Read = 2,
	Write = 3,
	Disconnect = 4,
	Connect = 5,
	Tick = 6,
};


void InitializeSocketService();

#ifdef _WIN32
#include <WinSock2.h>
#include <MSWSock.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
using Socket = SOCKET;

int SetSocketOptions(Socket socket, const SocketOptions& options);

class WinSockFnTable
{
public:
	static LPFN_CONNECTEX ConnectEx;
	static LPFN_ACCEPTEX AcceptEx;
	static LPFN_DISCONNECTEX DisconnectEx;
	static LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockAddrs;

	static void Initialize();
};

bool WSAOperationResult(int result);

#else

using Socket = int32_t;

int SetSocketOptions(Socket socket, const SocketOptions& options)
{
	/*
	int result = 0;
	for (const auto& option : options)
	{
		result += setsockopt(socket, SOL_SOCKET, option.first, reinterpret_cast<const char*>(&option.second), sizeof(decltype(option.second)));
	}
	*/
}

#endif

struct SocketAddress
{
	std::array<char, 16> ip;
	uint16_t port;

	static SocketAddress New(const std::string_view& ip, const uint16_t port);
	static const SocketAddress& Empty();

	sockaddr_in ToInetSocketAddress() const;
};

