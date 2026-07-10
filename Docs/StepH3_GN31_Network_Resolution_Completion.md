# H3 GN31 Network Resolution Completion

## 状態

**コード実装完了・CI確認中（2026-07-10）**

`Docs/ECS_Scheduler_Migration_Plan.md` §2.5 H3で残っていた、`GN31`の`gethostbyname`依存とIPv4固定処理を置き換えた。

## 旧実装の問題

- `gethostbyname`を使用するIPv4専用経路
- `hostent::h_addr_list[0]`を前提とする単一アドレス選択
- `inet_ntoa`の静的バッファ依存
- `GN31.h`がWinsock型を直接保持し、巨大な`componentList.h`経由で全Component利用箇所へWinsock include契約を漏らす
- `accept`がBlocking Socketのまま`OnUpdate`から呼ばれ、接続前にメインスレッドを停止する
- 各失敗経路でSocket / `WSAStartup`状態の解放処理が分散する

## 実装内容

### ヘッダ分離

`GN31.h`から次を除去した。

- `Windows.h`
- Winsock構造体
- `SOCKET`
- `WSADATA`
- `sockaddr_in`
- ネットワーク処理本体

Socket Handleはヘッダ上では`std::uintptr_t`のOpaque Valueとして保持する。
Winsock2を必要とする処理は新設した`GN31.cpp`だけへ閉じ込めた。

これにより`componentList.h`をincludeするコードがWinsock include順序へ依存しない。

### 名前解決

- `gethostbyname`を廃止
- `getaddrinfo(AF_UNSPEC)`へ移行
- `getnameinfo(NI_NUMERICHOST)`でIPv4 / IPv6を共通表示
- Local Hostの全候補アドレスを走査
- Listener生成でも`getaddrinfo(nullptr, port, AI_PASSIVE)`の候補を順番に試行

### Listener

- IPv6候補では`IPV6_V6ONLY=0`を要求しDual Stackを試行
- bind / listenに失敗した候補はcloseして次候補へ進む
- 利用可能なIPv6 / IPv4候補を最初に成功した時点で採用
- ListenerをNon-blockingへ変更

### Runtime State

```text
Closed
  -> Starting
  -> WaitingForClient
  -> Receiving
  -> WaitingForClient
```

`accept` / `recv`の`WSAEWOULDBLOCK`は正常な未完了状態として扱い、Game Threadを停止しない。

### Lifetime

`ShutdownNetwork()`へ次を集約した。

- Client Socket shutdown / close
- Listener Socket close
- `WSACleanup`
- State / Buffer reset

`OnStart`、`OnStop`、destructor、初期化失敗経路から同じ処理を使用する。

## 変更ファイル

- `Source/GameApplication/Engine/Scene/Script/GN31.h`
- `Source/GameApplication/Engine/Scene/Script/GN31.cpp`
- `Directory.Build.targets`

## 完了条件

- [x] `gethostbyname`を使用しない
- [x] IPv4固定の`hostent` / `sockaddr_in`処理を使用しない
- [x] IPv4 / IPv6候補を`getaddrinfo`で解決する
- [x] `GN31.h`からWinsock型を除去する
- [x] 接続待ちでGame ThreadをBlockingしない
- [x] Socket / Winsock Cleanupを一元化する
- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] IPv4 Client接続の実機確認
- [ ] IPv6またはDual Stack接続の実機確認
- [ ] Stop -> Start後の再待受確認

## 補足

`isServer`は従来から通信分岐に使用されていないため、本工程では既存挙動を変えずServer Echo Runtimeのみを移行対象とした。Client Mode設計はNetworkSystem本実装時の別工程とする。
