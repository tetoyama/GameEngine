# ElemenTactics

Status: **Integrated foundation — develop / 2026-09-27**

## Purpose

GameEngine上で動くボードゲーム実装。

Engine APIの実制作検証と、ルールエンジン・AI・LLM境界の検証を兼ねる。

## Structure

主な領域:

- `Rules/`: pure rules
- `AI/`: heuristic AI / LLM decision adapter
- `Flow/`: deck setup / match flow
- `Runtime/`: board interaction / game controller / presentation

## Rules

Rule engineが保持する主要契約:

- Element matchup
- Deck validation
- Piece / cell state
- legal action generation
- move / battle
- scout
- turn progression
- result

純粋ルールはEngine rendererに依存しない。

## Runtime

Game runtimeはECS componentとして登録される。

現在登録されている主なRuntime component:

- `ElemenTacticsGameController`
- `ElemenTacticsKeyboardNavigator`
- `ElemenTacticsVisualGuide`
- `ElemenTacticsTabletopPresentation`

## Text / presentation

Engine側へ `RuntimeTextComponent` と `RuntimeTextSystem` を追加している。

Game codeからD3D11 objectを直接扱わず、Engine-owned rendering boundaryを使う。

## AI

Heuristic AIを持つ。

LLM integrationでは、LLMに任意行動を生成させず、Engine側で列挙したlegal action集合から選択させる。

LLM responseはparse / validateし、合法集合外のactionを拒否する。

## Current validation

自動CIでは `ElemenTacticsRulesSmokeTest` をGame Rules gateとして実行する。

Flow、LLM lifecycle、Runtime interactionの詳細テストは手動診断用として残す。

## Remaining work

Foundationは統合済みだが、最終的な完成ゲームとしては以下を手動検証・調整する。

- presentation timing
- final authored assets
- audio / effects
- LLM runtime model-loading behavior
- pause policyとの整合
- actual play balance

旧ImplementationPlan / Progress / Audit / BalanceFindingsはArchiveへ移動済み。
