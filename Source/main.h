#pragma once
/*

・設計図

GameApplication
│
├── Services
│   ├── ConfigService             （yaml-cpp）【未実装】
│   ├── IconService               （wincodec, commdlg）【未実装】
│   └── ProgressStateService      （shobjidl）【未実装】
│
└── Engine
   │
   ├── Runtime
   │   ├── TimeService              （QueryPerformanceCounter）【実装済み】
   │   └── JobSystem                （マルチスレッドタスク管理）【未実装】
   │
   ├── Platform
   │   ├── WindowSystem
   │   │   ├── MainWindow          （IWindow）【実装済み】
   │   │   └── SubWindow           【未実装】
   │   ├── InputSystem
   │   │   ├── Keyboard            【実装済み】
   │   │   ├── Mouse               【実装済み】
   │   │   └── GamePad             （XInput）【実装済み】
   │   ├── AudioSystem             （XAudio2）【実装済み】
   │   └── NetworkSystem           （ASIO, Winsock2）【未実装】
   │
   ├── Graphics
   │   ├── GraphicsContext		（DirectX 11 デバイス・コンテキスト）	【実装済み】
   │   ├── RenderEffectSystem	(Effekseer）							【実装済み】
   │   └── RenderPipeline
   │       ├── MainRenderer		(MainWindow用 DirectX11 SwapChain)		【実装済み】
   │       └── SubRenderer		(SubWindow用 DirectX11 SwapChain)		【未実装】
   │
   ├── Resources
   │   └── ResourceService          （ローダー管理・依存解決）【不完全実装】
   │       ├── FontLoader								【未実装】
   │       ├── ShaderLoader								【実装済み】
   │       ├── TextureLoader(DirectXTex）				【実装済み】
   │       ├── ModelLoader(Assimp）						【実装済み】
   │       ├── AudioLoader								【実装済み】
   │       ├── EffectLoader								【実装済み】
   │       └── PrefabLoader								【未実装】
   │
   ├── DebugTools
   │   ├── DebugSystem									【実装済み】
   │   └── ImGuiService（Dear ImGui）					【実装済み】
   │       【Depends On】 → GraphicsContext
   │
   ├── Scene
   │   ├── SceneManagerContext							【実装済み】
   │   ├── SceneManager
   │   │   【Depends On】 → TimeService, InputSystem		【実装済み】
   │   └── Scene（ECSWorld）
   │       ├── SceneContext								【実装済み】
   │       ├── EntityRegistry							【実装済み】
   │       ├── ComponentPools
   │       │   ├── TransformComponent					【実装済み】
   │       │   ├── MeshRendererComponent				【実装済み】
   │       │   ├── RigidbodyComponent					【未実装】
   │       │   ├── ScriptComponent（C#バインド）		【未実装】
   │       │   └── CustomComponents					【未実装】
   │       └── Systems
   │           ├── TransformSystem						【実装済み】
   │           ├── RenderSystem							【実装済み】
   │           ├── PhysicsSystem（PhysX）				【実装済み】
   │           ├── ScriptSystem（.NET連携）				【未実装】
   │           └── その他追加System						【未実装】
   │
   ├── Scripting（将来導入）
   │   ├── ScriptEngine			（.NET Runtime ホスト）	【未実装】
   │   ├── ScriptBinder			（C++ API → C#）		【未実装】
   │   ├── ScriptLoader			（C# DLL読込）			【未実装】
   │   └── ScriptComponent		（Entityにアタッチ）	【未実装】
   │       ※ C# 側では IScript 実装
   │
   ├── EditorExtension（将来導入）
   │   ├── ToolRegistry	（C#からUIや機能登録）			【未実装】
   │   └── EditorBridge	（エンジンとエディタの接着）	【未実装】
   │
   └── Engine
		├── Engine		（初期化・更新・描画ループ）	【実装済み】
		└── EngineContext（EngineContext生成・DI）		【実装済み】
*/
