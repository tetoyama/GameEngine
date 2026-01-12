#pragma once
/*

・設計図

GameApplication
│
├── Services
│   ├── ConfigService				（yaml-cpp）							【実装済み】
│   ├── IconService					（wincodec, commdlg）					【実装済み】
│   └── ProgressStateService		（shobjidl）							【実装済み】
└── Engine
   ├── Runtime
   │   ├── TimeService				（QueryPerformanceCounter）				【実装済み】
   │   └── JobSystem				（マルチスレッドタスク管理）			【実装済み】
   ├── Platform
   │   ├── WindowSystem
   │   │   ├── MainWindow			（IWindow）								【実装済み】
   │   │   └── SubWindow													【未実装】
   │   ├── InputSystem														【実装済み】
   │   ├── AudioSystem				（XAudio2）								【実装済み】
   │   └── NetworkSystem			（ASIO, Winsock2）						【未実装】
   ├── Graphics
   │   ├── GraphicsContext			（DirectX 11 デバイス・コンテキスト）	【実装済み】
   │   ├── RenderEffectSystem		（Effekseer）							【実装済み】
   │   └── RenderPipeline
   │       ├── MainRenderer			（MainWindow用 DirectX11 SwapChain）	【実装済み】
   │       └── SubRenderer			（SubWindow用 DirectX11 SwapChain）		【未実装】
   ├── Resources
   │   └── ResourceService			（ローダー管理・依存解決）				【実装済み】
   │       ├── TextureData													【実装済み】
   │       ├── ModelData			（Assimp使用）							【実装済み】
   │       ├── VertexShaderData												【実装済み】
   │       ├── PixelShaderData												【実装済み】
   │       ├── AudioData													【実装済み】
   │       └── EffectData													【実装済み】
   ├── DebugTools
   │   ├── DebugSystem														【実装済み】
   │   └── ImGuiService（Dear ImGui）										【実装済み】
   │ 	【Depends On】 → GraphicsContext
   ├── Scene
   │   ├── SceneManagerContext												【実装済み】
   │   └── SceneManager
   │        ├── SystemRegistry												【実装済み】
   │        │   ├── TransformSystem										【実装済み】
   │        │   ├── CameraSystem											【実装済み】
   │        │   ├── RenderSystem											【実装済み】
   │        │   ├── AudioSystem											【実装済み】
   │        │   ├── ParticleSystem											【実装済み】
   │        │   ├── EffectSystem											【実装済み】
   │        │   ├── TerrainSystem											【実装済み】
   │        │   ├── PhysicSystem											【実装済み】
   │        │   ├── CSharpScriptSystem										【実装済み】
   │        │   ├── CustomScriptSystem										【実装済み】
   │        │   └── WaveSystem												【実装済み】
   │        └── Active Scenes												【実装済み】
   │            ├── EntityRegistry											【実装済み】
   │            └── ComponentRegistry										【実装済み】
   │                ├── NameComponent										【実装済み】
   │                ├── TransformComponent									【実装済み】
   │                ├── ColliderComponent									【実装済み】
   │                ├── AudioComponent										【実装済み】
   │                ├── RenderLayerComponent								【実装済み】
   │                ├── OrderInLayerComponent								【実装済み】
   │                ├── MaterialComponent									【実装済み】
   │                ├── TextureComponent									【実装済み】
   │                ├── BumpMapComponent									【実装済み】
   │                ├── LightComponent										【実装済み】
   │                ├── MeshRendererComponent								【実装済み】
   │                ├── ModelRendererComponent								【実装済み】
   │                ├── BillBoardRendererComponent							【実装済み】
   │                ├── SpriteRendererComponent								【実装済み】
   │                ├── TerrainComponent									【実装済み】
   │                ├── WaveComponent										【実装済み】
   │                ├── OutlineComponent									【未実装】
   │                ├── ParticleComponent									【実装済み】
   │                ├── EffectComponent										【実装済み】
   │                ├── CameraComponent										【実装済み】
   │                ├── CustomScriptComponent								【実装済み】
   │                ├── CSharpScriptComponent								【実装済み】
   │                ├── SetScene											【実装済み】
   │                ├── ScoreManager										【実装済み】
   │                ├── ScoreSprite											【実装済み】
   │                ├── PlayerController									【実装済み】
   │                ├── GameTimeManager										【実装済み】
   │                ├── TimerSprite											【実装済み】
   │                ├── CameraController									【実装済み】
   │                ├── BallController										【実装済み】
   │                ├── EnemyController										【実装済み】
   │                ├── FadeInSprite										【実装済み】
   │                ├── FadeOutSprite										【実装済み】
   │                ├── FadeSetScene										【実装済み】
   │                └── GN31												【実装済み】
   ├── Scripting（将来導入）													【未実装】
   └── EditorExtension（将来導入）											【未実装】
*/
