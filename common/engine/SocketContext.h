#pragma once

#include "NetworkContext.h"
#include "SocketSession.h"

struct SocketContext : public NetworkContext
{
	PacketHeader header;
	DynamicBufferCursor<SocketBuffer> buffer;
	SocketSessionPtr session;
	uint64_t attachment;

	void Release()
	{
		if (session.Valid() && buffer != nullptr)
		{
			if (static_cast<SocketBuffer*>(buffer)->Release() <= 0)
			{
				assert(buffer->Ref() >= 0);

				session->ReleaseReadBuffer(buffer);

				if (context_type == ContextType::SessionClosed)
					 session->ReleaseStream();
			}

			session.Release();
		}
	}
};