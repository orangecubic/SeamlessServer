// game.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <deque>
#include <vector>
#include <stack>
#include <cassert>
#include <concepts>
#include <memory>
#include <network/iocp/IocpNetworkEngine.h>
#include <network/iocp/IocpTransferStream.h>

#pragma comment(lib, "common.lib")

class EventHandler : public NetworkEventHandler
{
private:
	IocpNetworkEngine* _engine = nullptr;
public:
	void SetEngine(IocpNetworkEngine* engine)
	{
		_engine = engine;
	}
	virtual void OnAccept(TransferStream* stream) override
	{
		NetBuffer* buffer = _engine->AllocateBuffer();

		stream->TransmitRead(buffer, 10, 999);
	}

	virtual void OnRead(TransferStream* const stream, NetBuffer* const buffer, const uint32_t bytes_transffered, const uint64_t attachment) override
	{
		std::cout << bytes_transffered << std::endl;
		if (bytes_transffered == 0)
		{
			_engine->DeallocateBuffer(buffer);
			return;
		}
		stream->TransmitWrite(buffer, 0, buffer->length, attachment);
		
	}

	virtual void OnWrite(TransferStream* const stream, NetBuffer* const buffer, const uint32_t bytes_transffered, const uint64_t attachment) override
	{
		stream->TransmitRead(buffer, 10, 999);
	}

	virtual void OnDisconnect(TransferStream* const stream, const uint64_t attachment) override
	{
		std::cout << attachment << std::endl;
	}
};

int main()
{
	EventHandler* handler = new EventHandler;
	InitializeSocketService();
	IocpNetworkEngine* engine = new IocpNetworkEngine(handler);

	handler->SetEngine(engine);

	IocpEngineOption option;
	option.buffer_capacity = 65536;
	option.buffer_pool_expanding_size = 32;
	option.initial_buffer_pool_size = 64;
	option.max_acceptable_socket_count = 64;
	option.max_connectable_socket_count = 64;
	option.parallel_acceptor_count = 1;
	option.worker_count = 1;

	assert(engine->Initialize(option));
	assert(engine->InitializeAcceptor(SocketAddress::New("0.0.0.0", 10021), { {SO_REUSEADDR, 1} }));

	assert(engine->Start() != -1);

	while (true)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

}