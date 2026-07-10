// =======================================================================
//
// GN31.h
//
// =======================================================================
#pragma once

#include <cstdint>
#include <string>

#include "Component/CustomScriptComponent.h"

enum class NetworkState : std::uint8_t {
	Closed = 0,
	Starting,
	WaitingForClient,
	Receiving
};

// GN31スクリプト。
// Winsock型は共通Component一覧へ漏らさず、実装側GN31.cppだけで扱う。
class GN31 : public CustomScriptComponent {
public:
	GN31() = default;
	~GN31() override;

	BEGIN_REFLECT(GN31)
		REFLECT_FIELD(int, port, 1234)
		REFLECT_FIELD_INIT(bool, isServer, false, REFLECT_INSPECTOR)

	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);
		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		(void)context;
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		(void)context;
		INSPECTOR_FIELDS();
	}

private:
	static constexpr std::uintptr_t InvalidSocketValue =
		static_cast<std::uintptr_t>(~std::uintptr_t{0});

	void OnStart() override;
	void OnUpdate(float dt) override;
	void OnFixedUpdate(float dt) override;
	void OnDraw() override;
	void OnEditorUpdate(float dt) override;
	void OnStop() override;

	bool StartNetwork();
	bool CreateListeningSocket();
	void LogLocalAddresses();
	void PollAccept();
	void PollReceive();
	void CloseClientSocket() noexcept;
	void CloseListeningSocket() noexcept;
	void ShutdownNetwork() noexcept;
	void LogDebug(const std::string& message) const;
	void LogError(const std::string& message) const;

	NetworkState m_networkState = NetworkState::Closed;
	bool m_wsaStarted = false;
	std::uintptr_t m_listenSocket = InvalidSocketValue;
	std::uintptr_t m_clientSocket = InvalidSocketValue;
	char m_receiveBuffer[1024]{};
};
