# Platformer Tech Demo

Status: **Integrated tech demo**

## Purpose

GameEngineの一般ゲーム制作能力を検証するためのPlatformer実装。

Renderer機能そのもののデモではなく、Game codeがEngine APIだけでどこまで組めるかを見る。

## Current game components

現在Component registryへ統合されている主なPlatformer component:

- CharacterController
- AnimationController
- GameManager
- Coin
- Checkpoint
- Enemy
- MovingPlatform
- CameraController
- CameraZone
- Boss
- HUD
- StageBuilder
- PlayerFeedback
- PlayerAmbientFeedback
- ClearFeedback

## Engine usage

主に次のEngine機能を利用する。

- ECS component / safe reference
- Script runtime
- Physics / collider
- animation
- camera
- runtime UI
- resource loading
- scene lifecycle

## Role in repository

Platformer固有の仕様をEngine Coreへ逆流させない。

Tech demoから一般化可能な問題が見つかった場合だけShared Service / Engine APIへ昇格する。

## Validation

Platformer専用Workflowは廃止し、通常のWindows Debug x64全体Buildでcompile regressionを検出する。

操作感、Camera、Animation、stage feelは自動Smokeより実プレイを優先する。

旧 `Platformer_Tech_Demo_Plan.md` はArchiveへ移動済み。
