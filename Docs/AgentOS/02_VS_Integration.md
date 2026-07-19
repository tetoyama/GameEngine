# AgentOS Visual Studio 統合手順

本書は、AgentOSのエンジン側統合レイヤー（`Source/GameApplication/AgentOS/EngineTools`,
`Source/GameApplication/AgentOS/Service`, `Source/GameApplication/AgentOS/UI`）と、
並行実装された `Source/GameApplication/AgentOS/Core` を、既存の `GameEngine.vcxproj` /
`engineContext.cpp` / `editorService.cpp` へ統合するための正確な手順書である。

この文書を書いたエージェントはLinuxサンドボックス上で作業しており、
**MSVCでの実ビルド確認は行えていない**。すべてのエンジンAPIはヘッダを実読して裏取り
した上でコードを書いたが、最終的なコンパイル確認は必ずVisual Studio上で行うこと。

---

## 1. `engineContext.cpp`（`EngineContextBuilder::Build()`）への追加

### 1.1 現状（読み取り済みの実コード、`Source/GameApplication/Engine/engineContext.cpp`）

```cpp
#ifdef _EDITOR
	context->Register<EditorService>(std::make_unique<EditorService>());
#endif

	context->Register<LLAMAService>(
		std::make_unique<LLAMAService>(debugLogSystem)
	);

	return context;
}
```

### 1.2 追加するコード

`#include` セクション（既存の `#include "LlamaService/LLAMAService.h"` の下）に追加:

```cpp
#include "AgentOS/Service/AgentOSService.h"
```

`context->Register<LLAMAService>(...)` の直後、`return context;` の前に追加:

```cpp
#ifdef _EDITOR
	// AgentOSServiceはLLAMAServiceより後に登録する。
	// EngineContext::Shutdown()は登録の逆順にShutdown()を呼ぶため、こうすることで
	// AgentOSServiceが保持するLLAMAAgent(shared_ptr)/CommandPipelineを、
	// LLAMAService::Shutdown()より先に解放できる（Agent停止の後始末を安全にするため）。
	//
	// 現状AgentOSPanelはEditorServiceにのみ登録されるため、_EDITORビルド限定にしている。
	// 将来Editor無しでもAgentOSを動かしたい場合は、この#ifdefを外し、
	// AgentOSPanelへの依存だけをEditor側に残す形へ分離すること。
	context->Register<agentos::AgentOSService>(
		std::make_unique<agentos::AgentOSService>()
	);
#endif
```

> **Docs/AgentOS/00_Architecture.md §12 との差異について**: 設計ドキュメントには
> 「LLAMAServiceの後・EditorServiceの前に登録」とあるが、現在の実コードは
> `EditorService` → `LLAMAService` の順で登録されている（ドキュメントと実装が既に乖離している）。
> 本統合では **Shutdown順序の安全性**（AgentOSServiceがLLAMAAgentへのshared_ptrを持つため、
> LLAMAServiceより先にShutdownされてほしい）を優先し、「LLAMAServiceの直後」に登録する。
> EditorServiceとの前後関係はAgentOSServiceの構築（`Register`）自体には影響しない
> （実際の初期化は後述の`engine.cpp`側で個別に行うため）。

---

## 2. `engine.cpp`（`Engine::Initialize`）への追加

### 2.1 現状（読み取り済みの実コード、`Source/GameApplication/Engine/engine.cpp`）

サービス取得部（54〜77行目付近）:

```cpp
	auto scenes = context->Get<SceneManager>();
	auto imgui = context->Get<ImGuiService>();
#ifdef _EDITOR
	auto editor = context->Get<EditorService>();
#else
	ServiceRef<EditorService> editor{};
#endif
	auto llama = context->Get<LLAMAService>();
```

Editor初期化部（135〜142行目付近）:

```cpp
	if(editor){
		EditorServiceContext editorContext{};
		editorContext.debugLogSystem = debug.get();
		editorContext.llamaService = llama.get();
		editorContext.resourceService = resources.get();
		editorContext.sceneManager = scenes.get();
		editor->Initialize(editorContext);
	}
```

関数末尾（178〜182行目付近、`return true;` の直前）:

```cpp
	LLAMAServiceContext llamaContext{};
	llamaContext.resourceService = resources.get();
	llama->Initialize(llamaContext);

	return true;
}
```

### 2.2 追加するコード

`#include` セクションに追加:

```cpp
#include "AgentOS/Service/AgentOSService.h"
#include "AgentOS/UI/AgentOSPanel.h"
```

サービス取得部（`auto llama = context->Get<LLAMAService>();` の直後）に追加:

```cpp
#ifdef _EDITOR
	auto agentOS = context->Get<agentos::AgentOSService>();
#else
	ServiceRef<agentos::AgentOSService> agentOS{};
#endif
```

`if(!llama) return FailInitialize(...)` チェック群のあたりに追加（任意。無くても
`agentOS` が無効ならAgentOS初期化ブロックがスキップされるだけで安全に落ちる）:

```cpp
#ifdef _EDITOR
	if(!agentOS) return FailInitialize(debug.get(), "AgentOSService is not registered");
#endif
```

`llama->Initialize(llamaContext);` の直後、`return true;` の前に追加:

```cpp
#ifdef _EDITOR
	if(agentOS && editor){
		agentos::AgentOSServiceContext agentOSContext{};
		agentOSContext.sceneManager = scenes.get();
		agentOSContext.debugLog = debug.get();
		agentOSContext.llamaService = llama.get();
		// BRAIN.h の BRAIN_MODEL_PATH と同じモデルを再利用する想定。
		// BRAIN.h をincludeしたくない場合はここに直接文字列を書く（下記7節参照）。
		agentOSContext.modelPath = "Asset/BRAIN/model/Qwen3.5-9B-Q4_K_M.gguf";
		agentOSContext.dbPath = "Logs/AgentOS/agentos.db";
		agentOS->Initialize(agentOSContext);

		// AgentOSPanelはEditorService::Initialize()内で生成済みのはず（3節参照）。
		// EditorServiceContextにAgentOSServiceを渡す経路が無いため、ここで明示的に注入する。
		if(agentos::AgentOSPanel* panel = editor->GetUI<agentos::AgentOSPanel>()){
			panel->SetService(agentOS.get());
		} else if(debug){
			debug->LOG_WARNING(
				"AgentOSPanel is not registered in EditorService; "
				"AgentOSService will run without UI.");
		}
	}
#endif

	return true;
}
```

`agentOS->Initialize()` は `editor->Initialize(editorContext)` より後で呼ぶこと
（`editor->GetUI<agentos::AgentOSPanel>()` がEditorService初期化後でないと解決できないため）。

---

## 3. `editorService.cpp` へのパネル登録

### 3.1 現状（読み取り済みの実コード、`Source/GameApplication/Engine/Editor/editorService.cpp`）

```cpp
#include "UI/BRAIN/BRAIN.h"
#include "UI/CB41.h"
```//（includeブロック）

```cpp
	UIs.push_back({"SceneStorageSettings", new SceneStorageSettingsPanel()});
	//UIs.push_back({"BRAIN", new BRAIN()});
	//UIs.push_back({"CB41", new CB41()});
```

### 3.2 追加するコード

`#include "UI/BRAIN/BRAIN.h"` の下あたりに追加:

```cpp
#include "AgentOS/UI/AgentOSPanel.h"
```

コメントアウトされた `BRAIN` / `CB41` 登録行のすぐ下に追加（BRAIN自体は今回もコメントアウトのまま
で構わない。有効化するかはこの統合と無関係な既存の判断に委ねる）:

```cpp
	UIs.push_back({"AgentOS", new agentos::AgentOSPanel()});
```

`AgentOSPanel::SetService()` はここでは呼べない（この時点では `AgentOSService` が
まだ生成されていない可能性がある／`EditorServiceContext` にAgentOSServiceへの参照が無い）。
2節で説明した通り、`engine.cpp` 側で `editor->Initialize()` の後に
`editor->GetUI<agentos::AgentOSPanel>()->SetService(agentOS.get())` を呼ぶ。

---

## 4. `EditorServiceContext` の変更について

**変更は不要。** 当初案として `EditorServiceContext` に `AgentOSService* agentOSService` を
追加する経路も検討したが、以下の理由で見送り、`AgentOSPanel::SetService()` による
事後注入方式を採用した。

- `EditorService::Initialize()` の実行順序上、`AgentOSService` はまだ存在しないタイミングで
  `EditorService::Initialize()` が呼ばれる可能性がある（`engine.cpp` の現在の呼び出し順序では
  Editor初期化 → LLAMAService初期化なので、AgentOSServiceをその間に挟むには
  既存コードの呼び出し順序自体を大きく変える必要がある）。
- `EditorServiceContext` は他の多くのパネル（Hierarchy, Inspector等）が依存する共通構造体であり、
  AgentOS専用の依存を混ぜたくない。

もし将来的に `EditorServiceContext` 経由の受け渡しへ統一したい場合は、
`EditorServiceContext` に `agentos::AgentOSService* agentOSService = nullptr;` を追加し、
`EditorService::Initialize()` 内で `UIs.push_back(...)` の直後に
`if(auto* panel = GetUI<agentos::AgentOSPanel>()) panel->SetService(context.agentOSService);`
を呼ぶ形に変更できる（ただしこの場合、`AgentOSService::Initialize()` を
`EditorService::Initialize()` より前に完了させる必要がある）。

---

## 5. vcxprojへ追加する新規ファイル一覧

`GameEngine.vcxproj` には現時点で **AgentOS配下のファイルが1つも登録されていない**
（`Core`側も含め、並行実装されたファイルは今回のVS統合が最初の追加になる）。
`GameEngine.vcxproj.filters` は `ソース ファイル` / `ヘッダー ファイル` /
`リソース ファイル` の3フィルタのみのフラット構成なので、それに倣い全ファイルを
`ソース ファイル` / `ヘッダー ファイル` フィルタへ割り当てればよい
（ネストしたフィルタを作りたい場合は任意で `AgentOS\Core` 等を追加してよいが必須ではない）。

`<ClInclude Include="..." />` / `<ClCompile Include="..." />` の書式は既存エントリ
（例: `Source\GameApplication\Engine\engineContext.h` / `.cpp`）に倣うこと。

### 5.1 AgentOS/Core（並行実装。今回のエージェントは中身を変更していない）

ヘッダ:
```
Source\GameApplication\AgentOS\Core\AgentOsTypes.h
Source\GameApplication\AgentOS\Core\Json.h
Source\GameApplication\AgentOS\Core\Budget\Budget.h
Source\GameApplication\AgentOS\Core\Command\CapabilitySet.h
Source\GameApplication\AgentOS\Core\Command\CommandPipeline.h
Source\GameApplication\AgentOS\Core\Command\CommandSchema.h
Source\GameApplication\AgentOS\Core\Command\CommandTypes.h
Source\GameApplication\AgentOS\Core\Evidence\Evidence.h
Source\GameApplication\AgentOS\Core\Evidence\EvidenceBuilder.h
Source\GameApplication\AgentOS\Core\Llm\ILlmBackend.h
Source\GameApplication\AgentOS\Core\Llm\JsonExtractor.h
Source\GameApplication\AgentOS\Core\Llm\MockLlmBackend.h
Source\GameApplication\AgentOS\Core\Llm\PromptTemplates.h
Source\GameApplication\AgentOS\Core\Logic\LogicGraph.h
Source\GameApplication\AgentOS\Core\Store\SqliteDb.h
Source\GameApplication\AgentOS\Core\Store\TaskStore.h
Source\GameApplication\AgentOS\Core\Agents\AgentContext.h
Source\GameApplication\AgentOS\Core\Agents\IntakeAgent.h
Source\GameApplication\AgentOS\Core\Agents\PlannerAgent.h
Source\GameApplication\AgentOS\Core\Agents\RetrievalWorker.h
Source\GameApplication\AgentOS\Core\Agents\ReasoningAgent.h
Source\GameApplication\AgentOS\Core\Agents\CriticAgent.h
Source\GameApplication\AgentOS\Core\Agents\SynthesisAgent.h
Source\GameApplication\AgentOS\Core\Orchestrator\EarlyStopping.h
Source\GameApplication\AgentOS\Core\Orchestrator\Supervisor.h
Source\GameApplication\AgentOS\Core\Orchestrator\Orchestrator.h
```

ソース:
```
Source\GameApplication\AgentOS\Core\AgentOsTypes.cpp
Source\GameApplication\AgentOS\Core\Budget\Budget.cpp
Source\GameApplication\AgentOS\Core\Command\CapabilitySet.cpp
Source\GameApplication\AgentOS\Core\Command\CommandPipeline.cpp
Source\GameApplication\AgentOS\Core\Command\CommandSchema.cpp
Source\GameApplication\AgentOS\Core\Evidence\Evidence.cpp
Source\GameApplication\AgentOS\Core\Evidence\EvidenceBuilder.cpp
Source\GameApplication\AgentOS\Core\Llm\JsonExtractor.cpp
Source\GameApplication\AgentOS\Core\Llm\MockLlmBackend.cpp
Source\GameApplication\AgentOS\Core\Llm\PromptTemplates.cpp
Source\GameApplication\AgentOS\Core\Logic\LogicGraph.cpp
Source\GameApplication\AgentOS\Core\Store\SqliteDb.cpp
Source\GameApplication\AgentOS\Core\Store\TaskStore.cpp
Source\GameApplication\AgentOS\Core\Agents\AgentContext.cpp
Source\GameApplication\AgentOS\Core\Agents\IntakeAgent.cpp
Source\GameApplication\AgentOS\Core\Agents\PlannerAgent.cpp
Source\GameApplication\AgentOS\Core\Agents\RetrievalWorker.cpp
Source\GameApplication\AgentOS\Core\Agents\ReasoningAgent.cpp
Source\GameApplication\AgentOS\Core\Agents\CriticAgent.cpp
Source\GameApplication\AgentOS\Core\Agents\SynthesisAgent.cpp
Source\GameApplication\AgentOS\Core\Orchestrator\EarlyStopping.cpp
Source\GameApplication\AgentOS\Core\Orchestrator\Supervisor.cpp
Source\GameApplication\AgentOS\Core\Orchestrator\Orchestrator.cpp
```

> `Core`配下のファイル数・分割は本エージェントの担当外であり、上記はVS統合作業時点で
> ディレクトリを実走査して確認した実ファイル一覧である。統合作業直前に再度
> `Source/GameApplication/AgentOS/Core` を走査し、この一覧との差分が無いか確認すること。

### 5.2 AgentOS/EngineTools（本エージェントが新規作成）

ヘッダ:
```
Source\GameApplication\AgentOS\EngineTools\EngineToolContext.h
Source\GameApplication\AgentOS\EngineTools\MainThreadDispatcher.h
Source\GameApplication\AgentOS\EngineTools\EntityIntrospection.h
Source\GameApplication\AgentOS\EngineTools\SystemIntrospection.h
Source\GameApplication\AgentOS\EngineTools\WriteTrace.h
Source\GameApplication\AgentOS\EngineTools\EngineToolRegistry.h
```

ソース:
```
Source\GameApplication\AgentOS\EngineTools\MainThreadDispatcher.cpp
Source\GameApplication\AgentOS\EngineTools\EntityIntrospection.cpp
Source\GameApplication\AgentOS\EngineTools\SystemIntrospection.cpp
Source\GameApplication\AgentOS\EngineTools\WriteTrace.cpp
Source\GameApplication\AgentOS\EngineTools\EngineToolRegistry.cpp
```

（`EngineToolContext.h` はヘッダオンリーのため対応する`.cpp`は無い）

### 5.3 AgentOS/Service（本エージェントが新規作成）

ヘッダ:
```
Source\GameApplication\AgentOS\Service\LlamaLlmBackend.h
Source\GameApplication\AgentOS\Service\AgentOSService.h
```

ソース:
```
Source\GameApplication\AgentOS\Service\LlamaLlmBackend.cpp
Source\GameApplication\AgentOS\Service\AgentOSService.cpp
```

### 5.4 AgentOS/UI（本エージェントが新規作成）

ヘッダ:
```
Source\GameApplication\AgentOS\UI\AgentOSPanel.h
```

ソース:
```
Source\GameApplication\AgentOS\UI\AgentOSPanel.cpp
```

### 5.5 sqlite3（TaskStoreが依存。現状vcxproj未登録）

```
Source\GameApplication\Backends\sqlite\sqlite3.h   (ClInclude)
Source\GameApplication\Backends\sqlite\sqlite3.c   (ClCompile。5節参照)
```

### 5.6 vendor nlohmann/json（任意・IDE表示用）

`Source/GameApplication/AgentOS/Core/Json.h` が
`Source/GameApplication/Backends/llama/vendor/nlohmann/json.hpp` を相対includeしている。
このヘッダは既にllama関連ビルドの一部として存在するファイルであり、コンパイルに
`ClInclude`登録は不要（6節参照）だが、VS上でファイルツリーに表示したい場合のみ
`Source\GameApplication\Backends\llama\vendor\nlohmann\json.hpp` /
`json_fwd.hpp` を`ClInclude`として追加してよい。

---

## 6. sqlite3.c のMSVCコンパイル設定

`GameEngine.vcxproj` は現在 `WarningLevel=Level3` かつ `SDLCheck=true` を全体設定として
持つ（`_CRT_SECURE_NO_WARNINGS` は x64 Debug/Release 両方に既に定義済み）。
sqlite3.cはCで書かれた巨大な単一ファイルであり、`/sdl` が有効な状態では
（`_CRT_SECURE_NO_WARNINGS`の対象にならない）警告がエラー化する可能性がある。
`/WX` はプロジェクト全体で設定されていない（確認済み）ため必須ではないが、
安全のため sqlite3.c だけに対して個別のプロパティを設定することを推奨する。

`.vcxproj`内、`sqlite3.c`の`ClCompile`エントリを以下のように書くこと
（グローバルの`ItemDefinitionGroup`は変更せず、この1ファイルだけメタデータで上書きする）:

```xml
<ClCompile Include="Source\GameApplication\Backends\sqlite\sqlite3.c">
  <WarningLevel Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">TurnOffAllWarnings</WarningLevel>
  <WarningLevel Condition="'$(Configuration)|$(Platform)'=='Release|x64'">TurnOffAllWarnings</WarningLevel>
  <SDLCheck Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">false</SDLCheck>
  <SDLCheck Condition="'$(Configuration)|$(Platform)'=='Release|x64'">false</SDLCheck>
</ClCompile>
```

`_CRT_SECURE_NO_WARNINGS`はプロジェクト全体で既に定義済みのため、
sqlite3.c用に追加のPreprocessorDefinitionsは不要（実際に両Configurationの
`PreprocessorDefinitions`へ`_CRT_SECURE_NO_WARNINGS`が入っていることを確認済み）。

Win32構成（`Debug|Win32` / `Release|Win32`）にAgentOSを対応させる予定が無ければ、
上記Condition属性はx64のみで問題ない。対応させる場合は同様の行を追加すること。

---

## 7. include解決の確認（本エージェントが実際に検証した内容）

`AdditionalIncludeDirectories`（Debug|x64 / Release|x64、実際に読んだ値）:

```
$(ProjectDir)/Source/GameApplication/BackEnds/llama/ggml/include
$(ProjectDir)/Source/GameApplication/BackEnds/llama/include
$(ProjectDir)/Source/GameApplication/BackEnds/llama
$(ProjectDir)/Source/GameApplication/Engine/Scene
$(ProjectDir)/Source/GameApplication/Engine
$(ProjectDir)/Source/GameApplication/BackEnds
$(ProjectDir)/Source/GameApplication/Service
$(ProjectDir)/Source/GameApplication
$(ProjectDir)/Source
$(ProjectDir)
```

このリストは変更不要である。本エージェントが新規作成した全ファイルのincludeは、
このリストと相対include（`../Core/...` 等、Core自身が使っているのと同じ形式）だけで
解決できることを、書いた`#include`一つひとつについて実際に確認した:

- `Scene/sceneManager.h`, `Scene/scene.h`, `Scene/Entity/Entity.h`,
  `Scene/Registry/componentRegistry.h`, `Scene/Registry/entityRegistry.h`,
  `Scene/Registry/systemRegistry.h`, `Scene/Component/entityNameComponent.h`,
  `Scene/Interface/IComponentStorage.h`
  → `$(ProjectDir)/Source/GameApplication/Engine` 経由で解決
  （既存コード側でも `Scene/scene.h` 形式のincludeが使われていることを
  `Inspector.cpp` / `engine.cpp` 等で確認済み）。
- `Editor/editorService.h`, `Editor/InterFace/IEditorUI.h`
  → 同じく `.../Engine` 経由で解決（`MenuBar.h`が同形式）。
- `Service/LlamaService/LLAMAAgent.h`, `Service/LlamaService/LLAMAService.h`,
  `Service/LlamaService/AgentConfig.h`, `DebugTools/DebugSystem.h`
  → `.../GameApplication` 経由（前者3つ）、`.../Service` 経由（`DebugTools`）で解決
  （`BRAIN.cpp`, `engine.cpp`が同形式）。
- `Backends/ImGui/imgui.h`
  → `.../GameApplication` 経由で解決（`BRAIN.cpp`と同形式）。
- `<yaml-cpp/yaml.h>`
  → `.../Backends` 経由で解決（`componentRegistry.h`と同じ書式、`<>`形式）。
- `AgentOS/Service/AgentOSService.h`, `AgentOS/UI/AgentOSPanel.h`
  （`engineContext.cpp` / `engine.cpp` / `editorService.cpp` から追加するinclude）
  → `.../GameApplication` 経由で解決。
- Core側 (`../Core/...`, `../../Backends/llama/vendor/nlohmann/json.hpp`)
  → Core自身の既存includeパターンをそのまま流用しているだけなので追加設定不要。

**結論**: 追加の`AdditionalIncludeDirectories`は不要。ただし上記はヘッダの構文を
実読しての確認であり、`d3d11.h` / `Windows.h` に依存する箇所（`scene.h`,
`sceneManager.h`経由）はLinuxサンドボックスでは実際にコンパイルできていない
（8節のスモークテストで最終確認すること）。

---

## 8. VS上での動作確認手順（スモークテスト）

1. `_EDITOR`定義付きの構成（Debug|x64を想定）でソリューションを開き、
   5節のファイルを全て追加し、1〜3節の差分を適用してビルドする。
   - まず新規ファイルだけを対象に `Source/GameApplication/AgentOS/**` を
     ビルドが通るところまで潰す（特に`d3d11.h`/`Windows.h`関連の到達性、
     yaml-cppの`YAML::Node::Type()` / `NodeType::Scalar`等のシンボルが
     期待通り見えるか）。
   - `sqlite3.c`が単独でコンパイルできることを確認する（6節の設定を適用後）。
2. `Asset/BRAIN/model/` に `.gguf` モデルファイルが1つ以上存在することを確認する
   （無い場合はAgentOSServiceの`EnsureLlmReady()`が失敗し、Chatタブで
   `LLM model failed to load` が表示される。これは想定内の失敗モードであり、
   クラッシュしないことを確認できればこの段階のテストとしては十分）。
3. エディタを起動し、メニューにAgentOSパネルの表示切り替えが無いことを確認する
   （今回の実装では`MenuBar`に`showAgentOS`のようなフラグを追加していない。
   `AgentOSPanel`はウィンドウ自体の閉じるボタン(×)で非表示にできる自前の`m_show`
   フラグを持つのみ。`MenuBar`への統合は本書のスコープ外。必要なら
   `MenuBar.h`に`bool showAgentOS = IMGUI_SHOW_DEFAULT;`を追加し、
   `AgentOSPanel::Draw()`の先頭を`BRAIN::Draw()`と同様に
   `m_editor->GetUI<MenuBar>()->showAgentOS`を見る形へ変更することで対応できる）。
4. AgentOSパネル（"AgentOS"ウィンドウ）が表示されることを確認する。
   `AgentOSService is not attached to this panel.` が出ていないこと
   （出ている場合は2節の`SetService()`呼び出しが効いていない）。
5. Statusタブで `Busy: false` / `Stage: idle` (または`loading_model`等)が表示され、
   クラッシュしないことを確認する。
6. Chatタブの入力欄へ適当な文字列（例:「Systemの一覧を教えて」）を入力し `Send`。
   - モデルがロードされていれば `Stage` が `loading_model` → 何らかの実行段階 →
     `completed`/`stopped` と遷移し、Assistant側の返答がチャットログへ追記されることを確認する。
   - モデルが無い/ロード失敗の場合は `Stage: error` になり、
     `Error: LLM model failed to load: ...` がStatusタブに出ることを確認する
     （クラッシュしないことが本質的な確認事項）。
7. Auditタブに、実行された `ListSystems` / `ExportSystemDescriptors` 等の
   Tool呼び出し（Orchestratorがどのツールを呼ぶかは`Core`側のPlanner次第だが、
   最低限何らかのToolエントリが記録されることを確認する）が一覧表示されることを確認する。
8. 実行後、`Logs/AgentOS/systems.json` が生成されていること（`ExportSystemDescriptors`
   が呼ばれた場合のみ）、および `Logs/AgentOS/agentos.db`（TaskStoreのSQLite DB）が
   生成されていることをファイルシステム上で確認する。
9. Hypothesesタブに、Orchestratorが生成した仮説（`{"hypotheses":[...]}`形式）が
   あれば表示されることを確認する（セッションによっては空のままでも異常ではない）。

上記のうち6〜9は実モデルとCore側Agentロジックの完成度に依存するため、
本エージェントの担当範囲（EngineTools/Service/UI）としては
「クラッシュしない」「パネルが表示される」「Statusタブが正しく状態を反映する」
までが最低限の受け入れ基準であり、Orchestratorの推論品質そのものはスコープ外である。
