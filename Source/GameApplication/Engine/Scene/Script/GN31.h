#pragma once
#define _CRT_SECURE_NO_WARNINGS // セキュリティ警告
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 古いWinSock関数の警告を無効化

#include <stdio.h>

#include <Windows.h>
//#include <WinSock2.h>

#include "GameApplication/Engine/Scene/sceneManager.h"
#include "Component/CustomScriptComponent.h"
#include "GameApplication/Engine/DebugTools/DebugSystem.h"

#pragma comment (lib, "ws2_32.lib") // WinSockライブラリをリンク

class GN31 : public CustomScriptComponent {
public:

	BEGIN_REFLECT(GN31)
		REFLECT_FIELD(int, port, 1234)

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

	char hostName[256], ipAddress[256];

	SOCKET listenSocket;
	char recvBuffer[1024];
	//SOCKADDR_IN serverAddr;

	//SOCKADDR_IN clientAddr; // クライアントのアドレス情報
	//int clientAddrLen = sizeof(clientAddr);
	//SOCKET clientSocket; // クライアントとの通信に使うソケット

	void OnStart() override {
		//m_context->manager->debug->LOG_DEBUG("GN31 Started");

		//m_context->manager->debug->LOG_DEBUG(("ポート番号 : " + std::to_string(port)));


		//if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		//	m_context->manager->debug->LOG_ERROR("WSAStartup failed");
		//	return;
		//}
		//m_context->manager->debug->LOG_DEBUG("WSAStartup成功です");

		//hostInfo = (struct hostent*)gethostbyname(hostName);
		//memcpy(&ipAddr, hostInfo->h_addr_list[0], 4);
		//strcpy(ipAddress, inet_ntoa(ipAddr));
		//printf("IPアドレス=%s\n", ipAddress);

		// 初期化情報を表示（学習用）
		//printf("wVersion = %d.%d\n", LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
		//printf("wHighVersion = %d.%d\n", LOBYTE(wsaData.wHighVersion), HIBYTE(wsaData.wHighVersion));
		//printf("szDescription = %s\n", wsaData.szDescription);
		//printf("szSystemStatus = %s\n", wsaData.szSystemStatus);
		//printf("iMaxSockets= %d\n", wsaData.iMaxSockets);
		//printf("iMaxUdpDg = %d\n", wsaData.iMaxUdpDg);
		//printf("WSAStartup成功です\n");
		//// 自分のホスト名とIPアドレスを取得（確認用）
		//gethostname(hostName, (int)sizeof(hostName));
		//printf("\nホスト名=%s\n", hostName);
		//hostInfo = (struct hostent*)gethostbyname(hostName);
		//memcpy(&ipAddr, hostInfo->h_addr_list[0], 4);
		//strcpy(ipAddress, inet_ntoa(ipAddr));
		//printf("IPアドレス=%s\n", ipAddress);
		//// ③ ソケット作成（TCP通信）
		//// AF_INET: IPv4, SOCK_STREAM: TCP, 0: プロトコル自動選択
		//listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		//if (listenSocket == INVALID_SOCKET
		//	) {
		//	printf("ソケットオープンエラー\n");
		//	getchar();
		//	WSACleanup();
		//	exit(-5);
		//}
		//// ④ ソケットとポートの結びつけ（bind）
		//memset(&serverAddr, 0, sizeof
		//(SOCKADDR_IN)); // 構造体をゼロクリア
		//serverAddr.sin_family = AF_INET; // IPv4を使用
		//serverAddr.sin_port = htons(port); // ポート番号をネットワークバイトオーダーに変換
		//serverAddr.sin_addr.s_addr = INADDR_ANY; // どのIPからの接続も受け付ける
		//if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR
		//	) {
		//	printf("バインドエラー\n");
		//	getchar();
		//	closesocket(listenSocket);
		//	WSACleanup();
		//	exit(-1);
		//}
		//// ⑤ ソケットを待機状態に変更（listen）
		// // SOMAXCONN: 最大接続数をOSに任せる
		//if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR
		//	) {
		//	printf("待機状態変更失敗\n");
		//	getchar();
		//	closesocket(listenSocket);
		//	WSACleanup();
		//	exit(-2);
		//}

		//	 // ⑥ クライアントから接続受け入れ（accept）
		//clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
		//if (clientSocket == INVALID_SOCKET
		//	) {

		//	printf("accept error\n");
		//	closesocket(listenSocket);
		//	WSACleanup();
		//	exit(-3);
		//}
		//printf("%sが接続してきました\n", inet_ntoa(clientAddr.sin_addr));
		//printf("受信を開始します\n終了は end です\n");
		// // ⑦ データ受信ループ
		//while (1) {
		//	int recvSize = recv(clientSocket, recvBuffer, sizeof(recvBuffer) - 1, 0);
		//	if (recvSize == SOCKET_ERROR
		//		) {
		//		printf("受信エラーです\n");
		//		break;
		//	}
		//	recvBuffer[recvSize] = '\0'; // 文字列終端を追加
		//	if (strcmp(recvBuffer, "end") == 0) {
		//		printf("クライアントが接続を切りました\n");
		//		break;
		//	}
		//	printf("受信:%s\n", recvBuffer);
		//}

	}


	void OnUpdate(float dt) override {}

	void OnFixedUpdate(float dt)override {}

	void OnDraw() override {}

	void OnEditorUpdate(float dt)override {}

	void OnStop() override {

		//// ⑧ ソケットの送受信停止
		//shutdown(clientSocket, SD_BOTH);
		//// ⑨ ソケットを閉じる
		//closesocket(clientSocket);
		//closesocket(listenSocket);
		WSACleanup();

		m_context->manager->debug->LOG_DEBUG("GN31 Stopped");
	}
};
