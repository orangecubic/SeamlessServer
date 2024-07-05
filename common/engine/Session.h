#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include <atomic>
#include "../utility/Random.h"
#include "../utility/Time.h"
#include "../memory/RefCounter.h"

constexpr uint8_t SessionKeySize = 64;
using SessionKey = utility::RandomBuffer<SessionKeySize>;

enum class SessionState : uint8_t
{
	Opened,
	Abandoned,
	Wait,
	Closed,
};

class Session : public RefCounter<false>
{
private:
	std::atomic<SessionState> _session_state = SessionState::Closed;

	bool _initialized = false;
	
	uint16_t _ping;

	utility::Milliseconds _last_heartbeat_receiving_time = utility::Milliseconds(0);
	utility::Milliseconds _last_heartbeat_sending_time = utility::Milliseconds(0);
	utility::Milliseconds _abandoned_time = utility::Milliseconds(0);

	alignas(8) SessionKey _session_key;

	std::array<uint8_t, 24> __padding;

	std::unordered_map<uint64_t, uint64_t> _user_data;

	uint64_t _session_id;
public:

	void Initialize(const uint64_t session_id)
	{
		if (_initialized)
			throw std::exception("double initialized");

		_initialized = true;
		_session_id = session_id;
	}
	
	void ResetSessionKey(const SessionKey& key)
	{
		_session_key = key;
	}

	void ResetSessionKey()
	{
		_session_key = SessionKey();
	}

	const SessionKey& GetSessionKey()
	{
		return _session_key;
	}

	bool ChangeSessionState(SessionState expected, const SessionState desired)
	{
		bool success = _session_state.compare_exchange_strong(expected, desired, std::memory_order_acq_rel);
		if (success)
		{
		}

		return success;
	}

	void UpdateHeartbeatSendingTime(const utility::Milliseconds heartbeat_time)
	{
		_last_heartbeat_sending_time = heartbeat_time;
	}

	void UpdateHeartbeatReceivingTime(const utility::Milliseconds heartbeat_time)
	{
		_last_heartbeat_receiving_time = heartbeat_time;
		_ping = static_cast<uint16_t>(((_last_heartbeat_receiving_time - _last_heartbeat_sending_time) / 2).count());
	}

	void UpdateAbandonedTime(const utility::Milliseconds abandoned_time)
	{
		_abandoned_time = abandoned_time;
	}

	const uint64_t GetSessionId()
	{
		return _session_id;
	}

	SessionState GetSessionState()
	{
		return _session_state.load(std::memory_order_acquire);
	}

	uint16_t GetPing()
	{
		return _ping;
	}

	const utility::Milliseconds& GetLastHeartbeatSendingTime()
	{
		return _last_heartbeat_sending_time;
	}

	const utility::Milliseconds& GetLastHeartbeatReceivingTime()
	{
		return _last_heartbeat_receiving_time;
	}

	const utility::Milliseconds& GetAbandonedTime()
	{
		return _abandoned_time;
	}

	template <typename T1, typename T2>
	T2 GetUserData(const T1 key)
	{
		return (T2)(_user_data[static_cast<uint64_t>(key)]);
	}

	template <typename T1, typename T2> requires std::is_scalar_v<T2>
	void SetUserData(const T1 key, const T2 value)
	{
		_user_data[static_cast<uint64_t>(key)] = (uint64_t)(value);
	}

	void ResetUserData()
	{
		_user_data.clear();
	}
};