# Engine Overview

Status: **Current architecture — develop / 2026-09-27**

## 目的

GameEngineの現在構成を、過去のStep番号やMigration順序に依存せず説明するCanonical文書。

## 大きな境界

GameEngineは概ね次の層で構成される。

```
Application / Game
        |
        v
Scene + ECS World
        |
        +--> SystemTask / Scheduler / JobSystem
        |
        +--> Script / Physics / Gameplay Systems
        |
        +--> RenderWorld extraction
                  |
                  v
          Renderer / RHI
                  |
                  v
              D3D11
```

EditorとAgentOSはEngine Serviceとして同じ実行環境へ統合されるが、ゲームロジックそのものとは分離する。

## EngineContext / Service

`EngineContextBuilder` がServiceを構築し、`EngineContext` が寿命を管理する。

現在の主要Serviceには次がある。

- Window / Input
- Audio
- Graphics / RHI
- Resource
- SceneManager
- Editor
- LLAMA
- AgentOS

Shutdownは構築順の逆順で行う。

## Scene

`SceneManager` がActive Sceneを管理し、各SceneがECS Worldを持つ。

Scene切替・追加ロードのうち、ECS Schedule中にStorage構造へ影響する処理は即時実行しない。

MiniGameCollectionなどのAdditive Scene Loadでは、
`QueueAdditiveSceneLoadFromFilePath()` で予約し、
Engineの安全なフレーム境界から `ProcessQueuedAdditiveSceneLoads()` を実行する。

## ECS / Systems

Systemは直接Update順を持つのではなく、`SystemTask` をSchedulerへ登録する。

Domainは現在次の4種類。

- Fixed
- Frame
- Editor
- Render

Task間依存はPhase / Priority / Registration Orderと、宣言されたRead / Write Accessから構築される。

詳細は [ECS and Scheduling](ECS_and_Scheduling.md)。

## Rendering

RendererはECS Worldを描画Passから直接読む構造を減らし、
`RenderWorld` と `RenderPacket` を中間表現として使用する。

詳細は [Rendering](Rendering.md)。

## Script / Physics

ScriptはDLL境界を持ち、PhysicsはPhysX simulation lifecycleをSystemTaskへ分割している。

詳細は [Scripting and Physics](Scripting_and_Physics.md)。

## Editor

EditorはImGuiベースで、Hierarchy / Inspector / Assets / Debug Log / Performance Monitor / ViewなどのPanelをService経由で管理する。

UIの現在方針は [Modern ImGui Editor](../Editor/ModernImGui.md)。

## AgentOS

AgentOSは調査・ツール実行・Evidence管理を担うLLM Agent基盤。

Engine本体へ直接推論ロジックを埋め込まず、Service / Tool境界から接続する。

詳細は [AgentOS / B.R.A.I.N.](../AgentOS/README.md)。

## Current development branch

- `master`: バックアップ / 安定スナップショット
- `develop`: 現在の統合開発地点
- feature / refactor branch: `develop` から短命で作り、検証後に戻す

詳細は [Testing and Branch Policy](../Testing_Policy.md)。
