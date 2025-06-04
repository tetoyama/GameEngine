#pragma once
/*
・設計図

GameApplication
│
├── Services
│   ├── IconService               （wincodec, commdlg）
│   ├── ProgressStateService      （shobjidl）
│   └── ConfigSystem              （yaml - cpp：初期設定情報を全体へDI）
│
└── Engine
   │
   ├── Runtime
   │   ├── TimeService              （QueryPerformanceCounter）
   │   ├── JobSystem                （マルチスレッドタスク管理）
   │   └── EngineContext            （全体のDIハブ）
   │
   ├── Platform
   │   ├── WindowSystem
   │   │   ├── MainWindow          （IWindow）
   │   │   └── SubWindow
   │   ├── InputSystem
   │   │   ├── Keyboard
   │   │   ├── Mouse
   │   │   └── GamePad             （XInput）
   │   ├── AudioSystem             （XAudio2）
   │   └── NetworkSystem           （ASIO, Winsock2）
   │
   ├── Graphics
   │   ├── GraphicsContext         （DirectX 11 デバイス・コンテキスト）
   │   ├── EffectSystem            （Effekseer）
   │   └── RenderPipeline
   │       ├── MainRenderer         (MainWindow用 DirectX11 SwapChain)
   │       └── SubRenderer          (SubWindow用 DirectX11 SwapChain)
   │
   ├── Resources
   │   ├── ResourceSystem          （ローダー管理・依存解決）
   │   ├── FontLoader
   │   ├── ShaderLoader
   │   ├── TextureLoader           （DirectXTex）
   │   ├── ModelLoader             （Assimp）
   │   ├── AudioLoader
   │   ├── EffectLoader
   │   └── PrefabLoader
   │
   ├── DebugTools
   │   ├── DebugSystem
   │   └── ImGuiSystem             （Dear ImGui）
   │       【Depends On】 → GraphicsContext
   │
   ├── Scene
   │   ├── SceneContext (作成予定)
   │   ├── SceneManager
   │   │   【Depends On】 → TimeService, InputSystem
   │   └── Scene（ECSWorld）
   │       ├── EntityRegistry
   │       ├── ComponentPools
   │       │   ├── TransformComponent
   │       │   ├── MeshRendererComponent
   │       │   ├── RigidbodyComponent
   │       │   ├── ScriptComponent（C#スクリプトバインディング）
   │       │   └── CustomComponents（C++側定義 or 拡張）
   │       └── Systems
   │           ├── TransformSystem
   │           ├── RenderSystem     → RenderData出力（RenderPipelineへ）
   │           ├── PhysicsSystem    （PhysX）【FixedUpdate対応】
   │           ├── ScriptSystem     （.NET ScriptEngine と連携）
   │           └── その他追加System
   │
   ├── Scripting（将来導入）
   │   ├── ScriptEngine            （.NET Runtime のホスト管理）
   │   ├── ScriptBinder            （C++ API を C# に公開）
   │   ├── ScriptLoader            （C# DLL 読み込み・Assembly管理）
   │   └── ScriptComponent         （Entityにスクリプトをアタッチ）
   │       ※ C# 側では IScript インターフェースを実装
   │
   ├── EditorExtension（将来導入）
   │   ├── ToolRegistry            （C#からUIや機能を登録可能）
   │   └── EditorBridge            （エンジンとエディタの接着）
   │
   └── Engine
        ├── Engine            （全モジュール初期化・更新・描画ループ、Update / Fixed対応）
        └── EngineContext     （EngineContextの生成・DI）
*/