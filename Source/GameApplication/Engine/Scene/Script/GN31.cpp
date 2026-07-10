// =======================================================================
//
// GN31.cpp
//
// =======================================================================

// Winsock2 must be included before every header that may include Windows.h.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#include "GN31.h"

#include <cstdio>
#include <cstring>
#include <string_view>

#include "Backends/ImGuiFunc.h"
#include "DebugTools/DebugSystem.h"
#include "Scene/scene.h"
#include "Scene/sceneManager.h"

#pragma comment(lib, "Ws2_32.lib")

namespace {

SOCKET ToSocket(std::uintptr_t value) noexcept {
	return static_cast<SOCKET>(value);
}

std::uintptr_t FromSocket(SOCKET socket) noexcept {
	return static_cast<std::uintptr_t>(socket);
}

const char* NetworkStateName(NetworkState state) noexcept {
	switch(state){
		case NetworkState::Closed: return "Closed";
		case NetworkState::Starting: return "Starting";
		case NetworkState::WaitingForClient: return "Waiting for client";
		case NetworkState::Receiving: return "Receiving";
	}
	return "Unknown";
}

} // namespace

GN31::~GN31(){
	ShutdownNetwork();
}

void GN31::LogDebug(const std::string& message) const {
	SceneContext* context = m_ref.GetScene();
	if(context && context->manager && context->manager->debug){
		context->manager->debug->LOG_DEBUG(message);
	}else{
		OutputDebugStringA((message + "\n").c_str());
	}
}

void GN31::LogError(const std::string& message) const {
	SceneContext* context = m_ref.GetScene();
	if(context && context->manager && context->manager->debug){
		context->manager->debug->LOG_ERROR(message);
	}else{
		OutputDebugStringA(("GN31 ERROR: " + message + "\n").c_str());
	}
}

void GN31::CloseClientSocket() noexcept {
	if(m_clientSocket == InvalidSocketValue){
		return;
	}

	const SOCKET socket = ToSocket(m_clientSocket);
	shutdown(socket, SD_BOTH);
	closesocket(socket);
	m_clientSocket = InvalidSocketValue;
}

void GN31::CloseListeningSocket() noexcept {
	if(m_listenSocket == InvalidSocketValue){
		return;
	}

	closesocket(ToSocket(m_listenSocket));
	m_listenSocket = InvalidSocketValue;
}

void GN31::ShutdownNetwork() noexcept {
	CloseClientSocket();
	CloseListeningSocket();

	if(m_wsaStarted){
		WSACleanup();
		m_wsaStarted = false;
	}

	m_networkState = NetworkState::Closed;
	m_receiveBuffer[0] = '\0';
}

void GN31::LogLocalAddresses(){
	char hostName[256]{};
	if(gethostname(hostName, static_cast<int>(sizeof(hostName))) == SOCKET_ERROR){
		LogError("gethostname failed: " + std::to_string(WSAGetLastError()));
		return;
	}

	LogDebug("ホスト名=" + std::string(hostName));

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	addrinfo* addresses = nullptr;
	const int result = getaddrinfo(hostName, nullptr, &hints, &addresses);
	if(result != 0){
		LogError("getaddrinfo(local host) failed: " + std::to_string(result));
		return;
	}

	bool addressLogged = false;
	for(const addrinfo* address = addresses; address; address = address->ai_next){
		char numericHost[NI_MAXHOST]{};
		if(getnameinfo(
			address->ai_addr,
			static_cast<socklen_t>(address->ai_addrlen),
			numericHost,
			static_cast<DWORD>(sizeof(numericHost)),
			nullptr,
			0,
			NI_NUMERICHOST
		) == 0){
			LogDebug("ローカルIP=" + std::string(numericHost));
			addressLogged = true;
		}
	}
	freeaddrinfo(addresses);

	if(!addressLogged){
		LogDebug("ローカルIPを列挙できませんでした");
	}
}

bool GN31::CreateListeningSocket(){
	if(port <= 0 || port > 65535){
		LogError("ポート番号が範囲外です: " + std::to_string(port));
		return false;
	}

	char service[16]{};
	sprintf_s(service, "%d", port);

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	addrinfo* candidates = nullptr;
	const int resolveResult = getaddrinfo(nullptr, service, &hints, &candidates);
	if(resolveResult != 0){
		LogError("getaddrinfo(listener) failed: " + std::to_string(resolveResult));
		return false;
	}

	SOCKET listener = INVALID_SOCKET;
	int selectedFamily = AF_UNSPEC;
	for(const addrinfo* candidate = candidates; candidate; candidate = candidate->ai_next){
		SOCKET socket = ::socket(
			candidate->ai_family,
			candidate->ai_socktype,
			candidate->ai_protocol
		);
		if(socket == INVALID_SOCKET){
			continue;
		}

		const BOOL reuseAddress = TRUE;
		setsockopt(
			socket,
			SOL_SOCKET,
			SO_REUSEADDR,
			reinterpret_cast<const char*>(&reuseAddress),
			sizeof(reuseAddress)
		);

		if(candidate->ai_family == AF_INET6){
			const DWORD dualStack = 0;
			setsockopt(
				socket,
				IPPROTO_IPV6,
				IPV6_V6ONLY,
				reinterpret_cast<const char*>(&dualStack),
				sizeof(dualStack)
			);
		}

		if(bind(
			socket,
			candidate->ai_addr,
			static_cast<int>(candidate->ai_addrlen)
		) == SOCKET_ERROR || listen(socket, SOMAXCONN) == SOCKET_ERROR){
			closesocket(socket);
			continue;
		}

		u_long nonBlocking = 1;
		if(ioctlsocket(socket, FIONBIO, &nonBlocking) == SOCKET_ERROR){
			closesocket(socket);
			continue;
		}

		listener = socket;
		selectedFamily = candidate->ai_family;
		break;
	}
	freeaddrinfo(candidates);

	if(listener == INVALID_SOCKET){
		LogError("利用可能なIPv4/IPv6待受アドレスへbindできませんでした: " +
			std::to_string(WSAGetLastError()));
		return false;
	}

	m_listenSocket = FromSocket(listener);
	LogDebug(
		"ポート " + std::to_string(port) + " で待受を開始しました (" +
		(selectedFamily == AF_INET6 ? "IPv6 dual-stack" : "IPv4") + ")"
	);
	return true;
}

bool GN31::StartNetwork(){
	ShutdownNetwork();

	WSADATA wsaData{};
	const int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(startupResult != 0){
		LogError("WSAStartup failed: " + std::to_string(startupResult));
		return false;
	}
	m_wsaStarted = true;

	LogDebug(
		"Winsock " + std::to_string(LOBYTE(wsaData.wVersion)) + "." +
		std::to_string(HIBYTE(wsaData.wVersion)) + " initialized"
	);
	LogLocalAddresses();

	if(!CreateListeningSocket()){
		ShutdownNetwork();
		return false;
	}

	m_networkState = NetworkState::WaitingForClient;
	return true;
}

void GN31::PollAccept(){
	if(m_listenSocket == InvalidSocketValue){
		m_networkState = NetworkState::Starting;
		return;
	}

	sockaddr_storage clientAddress{};
	int clientAddressLength = sizeof(clientAddress);
	SOCKET client = accept(
		ToSocket(m_listenSocket),
		reinterpret_cast<sockaddr*>(&clientAddress),
		&clientAddressLength
	);
	if(client == INVALID_SOCKET){
		const int error = WSAGetLastError();
		if(error != WSAEWOULDBLOCK){
			LogError("accept failed: " + std::to_string(error));
			ShutdownNetwork();
			m_networkState = NetworkState::Starting;
		}
		return;
	}

	u_long nonBlocking = 1;
	if(ioctlsocket(client, FIONBIO, &nonBlocking) == SOCKET_ERROR){
		LogError("accepted socket non-blocking setup failed: " +
			std::to_string(WSAGetLastError()));
		closesocket(client);
		return;
	}

	char numericHost[NI_MAXHOST]{};
	char numericService[NI_MAXSERV]{};
	if(getnameinfo(
		reinterpret_cast<const sockaddr*>(&clientAddress),
		clientAddressLength,
		numericHost,
		static_cast<DWORD>(sizeof(numericHost)),
		numericService,
		static_cast<DWORD>(sizeof(numericService)),
		NI_NUMERICHOST | NI_NUMERICSERV
	) == 0){
		LogDebug(
			std::string(numericHost) + ":" + numericService +
			" が接続しました"
		);
	}else{
		LogDebug("クライアントが接続しました");
	}

	m_clientSocket = FromSocket(client);
	m_networkState = NetworkState::Receiving;
}

void GN31::PollReceive(){
	if(m_clientSocket == InvalidSocketValue){
		m_networkState = NetworkState::WaitingForClient;
		return;
	}

	const int received = recv(
		ToSocket(m_clientSocket),
		m_receiveBuffer,
		static_cast<int>(sizeof(m_receiveBuffer) - 1),
		0
	);
	if(received == SOCKET_ERROR){
		const int error = WSAGetLastError();
		if(error != WSAEWOULDBLOCK){
			LogError("recv failed: " + std::to_string(error));
			CloseClientSocket();
			m_networkState = NetworkState::WaitingForClient;
		}
		return;
	}
	if(received == 0){
		LogDebug("クライアントが接続を終了しました");
		CloseClientSocket();
		m_networkState = NetworkState::WaitingForClient;
		return;
	}

	m_receiveBuffer[received] = '\0';
	const std::string_view message(m_receiveBuffer, static_cast<std::size_t>(received));
	std::string_view command = message;
	while(!command.empty() &&
		(command.back() == '\r' || command.back() == '\n')){
		command.remove_suffix(1);
	}

	if(command == "end"){
		LogDebug("クライアントから切断要求を受信しました");
		CloseClientSocket();
		m_networkState = NetworkState::WaitingForClient;
		return;
	}

	LogDebug("受信:" + std::string(message));
	const int sent = send(
		ToSocket(m_clientSocket),
		m_receiveBuffer,
		received,
		0
	);
	if(sent == SOCKET_ERROR){
		const int error = WSAGetLastError();
		if(error != WSAEWOULDBLOCK){
			LogError("send failed: " + std::to_string(error));
			CloseClientSocket();
			m_networkState = NetworkState::WaitingForClient;
		}
	}else if(sent != received){
		LogDebug(
			"echo response was partial: " + std::to_string(sent) + "/" +
			std::to_string(received)
		);
	}
}

void GN31::OnStart(){
	ShutdownNetwork();
	m_networkState = NetworkState::Starting;
}

void GN31::OnUpdate(float dt){
	(void)dt;

	switch(m_networkState){
		case NetworkState::Closed:
			m_networkState = NetworkState::Starting;
			break;
		case NetworkState::Starting:
			if(!StartNetwork()){
				m_networkState = NetworkState::Closed;
			}
			break;
		case NetworkState::WaitingForClient:
			PollAccept();
			break;
		case NetworkState::Receiving:
			PollReceive();
			break;
	}
}

void GN31::OnFixedUpdate(float dt){
	(void)dt;
}

void GN31::OnDraw(){
	if(ImGui::Begin("TCP")){
		ImGui::Text("State: %s", NetworkStateName(m_networkState));
		ImGui::Text("Port: %d", port);
	}
	ImGui::End();
}

void GN31::OnEditorUpdate(float dt){
	(void)dt;
}

void GN31::OnStop(){
	ShutdownNetwork();
	LogDebug("GN31 Stopped");
}
