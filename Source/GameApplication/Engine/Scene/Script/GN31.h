#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS // 古いWinSock関数の警告を無効化

#include <stdio.h>

#include <Windows.h>
//#include <WinSock2.h>

#include "Scene/sceneManager.h"
#include "Component/CustomScriptComponent.h"
#include "DebugTools/DebugSystem.h"

#pragma comment (lib, "ws2_32.lib") // WinSockライブラリをリンク

enum NetworkState {
	NETWORKSTATE_CLOSE = 0,
	NETWORKSTATE_START,
	NETWORKSTATE_RECV,
};

class GN31 : public CustomScriptComponent {
public:

	BEGIN_REFLECT(GN31)
		REFLECT_FIELD(int, port, 1234)
		REFLECT_FIELD_INIT(bool,isServer,false,REFLECT_INSPECTOR)

	YAML::Node encode() override {
		YAML::Node node;
		ENCODE_FIELDS(node);

		return node;
	}

	bool decode(SceneContext* context, const YAML::Node& node) override {
		DECODE_FIELDS(node);
		return true;
	}

	void inspector(SceneContext* context) override {
		INSPECTOR_FIELDS();
	}

private:

	WSADATA wsaData;
	hostent* hostInfo;
	IN_ADDR ipAddr;

	NetworkState m_NetWorkState = NETWORKSTATE_CLOSE;

	char hostName[256], ipAddress[256];

	SOCKET listenSocket;
	char recvBuffer[1024];
	SOCKADDR_IN serverAddr;

	SOCKADDR_IN clientAddr; // クライアントのアドレス情報
	int clientAddrLen = sizeof(clientAddr);
	SOCKET clientSocket; // クライアントとの通信に使うソケット



	void OnStart() override {

		m_NetWorkState = NETWORKSTATE_CLOSE;
	}

	void OnUpdate(float dt) override {

		char debugString[1024];
		
		switch (m_NetWorkState) {
			case NETWORKSTATE_CLOSE:

				m_NetWorkState = NETWORKSTATE_START;

				break;
			case NETWORKSTATE_START:
				m_context->manager->debug->LOG_DEBUG("GN31 Started");

				m_context->manager->debug->LOG_DEBUG(("ポート番号 : " + std::to_string(port)));

				if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
					m_context->manager->debug->LOG_ERROR("WSAStartup failed");
					return;
				}
				// 初期化情報を表示（学習用）
				sprintf_s(debugString, "wVersion = %d.%d", LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
				m_context->manager->debug->LOG_DEBUG(debugString);
				sprintf_s(debugString, "wHighVersion = %d.%d", LOBYTE(wsaData.wHighVersion), HIBYTE(wsaData.wHighVersion));
				m_context->manager->debug->LOG_DEBUG(debugString);
				sprintf_s(debugString, "szDescription = %s", wsaData.szDescription);
				m_context->manager->debug->LOG_DEBUG(debugString);
				sprintf_s(debugString, "szSystemStatus = %s", wsaData.szSystemStatus);
				m_context->manager->debug->LOG_DEBUG(debugString);
				sprintf_s(debugString, "iMaxSockets= %d", wsaData.iMaxSockets);
				m_context->manager->debug->LOG_DEBUG(debugString);
				sprintf_s(debugString, "iMaxUdpDg = %d", wsaData.iMaxUdpDg);
				m_context->manager->debug->LOG_DEBUG(debugString);
				m_context->manager->debug->LOG_DEBUG("WSAStartup成功です");

				// 自分のホスト名とIPアドレスを取得（確認用）
				gethostname(hostName, (int)sizeof(hostName));
				hostInfo = (struct hostent*)gethostbyname(hostName);
				sprintf_s(debugString, "ホスト名=%s", hostName);
				m_context->manager->debug->LOG_DEBUG(debugString);

				memcpy(&ipAddr, hostInfo->h_addr_list[0], 4);
				strcpy_s(ipAddress, inet_ntoa(ipAddr));
				sprintf_s(debugString, "IPアドレス=%s", ipAddress);
				m_context->manager->debug->LOG_DEBUG(debugString);

				// ③ ソケット作成（TCP通信）
				// AF_INET: IPv4, SOCK_STREAM: TCP, 0: プロトコル自動選択
				listenSocket = socket(AF_INET, SOCK_STREAM, 0);
				if (listenSocket == INVALID_SOCKET
					) {
					m_context->manager->debug->LOG_DEBUG("ソケットオープンエラー\n");
					WSACleanup();
					exit(-5);
				}

				// ④ ソケットとポートの結びつけ（bind）
				memset(&serverAddr, 0, sizeof(SOCKADDR_IN)); // 構造体をゼロクリア
				serverAddr.sin_family = AF_INET; // IPv4を使用
				serverAddr.sin_port = htons(port); // ポート番号をネットワークバイトオーダーに変換
				serverAddr.sin_addr.s_addr = INADDR_ANY; // どのIPからの接続も受け付ける
				if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
					m_context->manager->debug->LOG_DEBUG("バインドエラー\n");
					closesocket(listenSocket);
					WSACleanup();
					m_NetWorkState = NETWORKSTATE_CLOSE;
					return;
				}
				// ⑤ ソケットを待機状態に変更（listen）
				 // SOMAXCONN: 最大接続数をOSに任せる
				if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
					m_context->manager->debug->LOG_DEBUG("待機状態変更失敗\n");
					closesocket(listenSocket);
					WSACleanup();
					m_NetWorkState = NETWORKSTATE_CLOSE;
					return;
				}

				// ⑥ クライアントから接続受け入れ（accept）
				clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
				if (clientSocket == INVALID_SOCKET
					) {

					m_context->manager->debug->LOG_DEBUG("accept error\n");
					closesocket(listenSocket);
					WSACleanup();
					m_NetWorkState = NETWORKSTATE_CLOSE;
					return;
				}
				sprintf_s(debugString, "%sが接続してきました\n", inet_ntoa(clientAddr.sin_addr));
				m_context->manager->debug->LOG_DEBUG(debugString);
				m_context->manager->debug->LOG_DEBUG("受信を開始します\n終了は end です\n");

				m_NetWorkState = NETWORKSTATE_RECV;

				break;
			case NETWORKSTATE_RECV:
				{
					// ⑦ データ受信ループ
					u_long mode = 1;
					ioctlsocket(clientSocket, FIONBIO, &mode);

					int recvSize = recv(clientSocket, recvBuffer, sizeof(recvBuffer) - 1, 0);
					if (recvSize == SOCKET_ERROR) {
						//m_context->manager->debug->LOG_DEBUG("受信エラーです\n");
						return;
					}
					recvBuffer[recvSize] = '\0'; // 文字列終端を追加
					if (strcmp(recvBuffer, "end") == 0) {
						m_context->manager->debug->LOG_DEBUG("クライアントが接続を切りました\n");
						m_NetWorkState = NETWORKSTATE_CLOSE;
						return;
					}
					sprintf_s(debugString, "受信:%s\n", recvBuffer);
					m_context->manager->debug->LOG_DEBUG(debugString);

					send(clientSocket, recvBuffer, (int)strlen(recvBuffer), 0);
					break;
				}
			default:
				break;
		}
	}

	void OnFixedUpdate(float dt)override {}

	void OnDraw() override {
		ImGui::Begin("TCP");
		ImGui::End();
	}

	void OnEditorUpdate(float dt)override {}

	void OnStop() override {
		if (m_NetWorkState == NETWORKSTATE_RECV) {
			// ⑧ ソケットの送受信停止
			shutdown(clientSocket, 0);
			// ⑨ ソケットを閉じる
			closesocket(clientSocket);
			closesocket(listenSocket);
			WSACleanup();

			m_context->manager->debug->LOG_DEBUG("GN31 Stopped");
		}
	}
};
