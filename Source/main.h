// =======================================================================
// 
// main.h
// 
// =======================================================================
#pragma once
/*

・設計図

GameApplication
│
├── Services（Service/）
│   ├── IService						（サービス基底インターフェース）			【実装済み】
│   ├── ConfigService					（yaml-cpp）								【実装済み】
│   │   └── APPCONFIG					（アプリケーション設定構造体）				【実装済み】
│   ├── AudioContext					（XAudio2）									【実装済み】
│   ├── Runtime
│   │   ├── TimeService					（QueryPerformanceCounter）				【実装済み】
│   │   └── JobSystem					（マルチスレッドタスク管理）				【未実装】
│   ├── Platform
│   │   ├── WindowSystem
│   │   │   ├── IWindow					（ウィンドウインターフェース）			【実装済み】
│   │   │   ├── MainWindow				（メインウィンドウ実装）				【実装済み】
│   │   │   ├── WindowService			（ウィンドウ管理サービス）					【実装済み】
│   │   │   └── SubWindow															【未実装】
│   │   ├── InputSystem					（キーボード・マウス・ゲームパッド）	【実装済み】
│   │   │   └── KeyState / MouseState	（入力状態構造体）							【実装済み】
│   │   └── NetworkSystem				（ASIO, Winsock2）							【未実装】
│   ├── Graphics
│   │   ├── GraphicsContext				（DirectX 11 デバイス・コンテキスト）	【実装済み】
│   │   ├── MainRenderer				（MainWindow用 DirectX11 SwapChain）		【実装済み】
│   │   ├── SubRenderer					（SubWindow用 DirectX11 SwapChain）		【未実装】
│   │   ├── D2DRenderer					（Direct2D テキスト描画）				【実装済み】
│   │   └── RenderEffectSystem			（Effekseer）								【実装済み】
│   ├── DebugTools
│   │   ├── DebugSystem					（デバッグログ管理）					【実装済み】
│   │   └── ImGuiService				（Dear ImGui）								【実装済み】
│   │ 		【Depends On】 → GraphicsContext
│   └── LLAMAService
│       ├── AgentConfig					（エージェント設定）						【実装済み】
│       ├── LLAMAAgent					（推論エージェント）						【実装済み】
│       └── LLAMAService				（サービス管理）							【実装済み】
│
└── Engine（Engine/）
    ├── EngineContext					（DIコンテナ・サービス管理）				【実装済み】
    ├── Resources
    │   └── ResourceService				（ローダー管理・依存解決）					【実装済み】
    │       ├── Data
    │       │   ├── TextureData													【実装済み】
    │       │   ├── ModelData				（Assimp使用）							【実装済み】
    │       │   ├── ShaderData / VertexShaderData / PixelShaderData				【実装済み】
    │       │   ├── AudioData														【実装済み】
    │       │   ├── EffectData														【実装済み】
    │       │   ├── PrefabData														【実装済み】
    │       │   └── LlamaModelData													【実装済み】
    │       └── Loader
    │           ├── IResourceLoader			（ローダー基底）						【実装済み】
    │           ├── TextureLoader													【実装済み】
    │           ├── ModelLoader														【実装済み】
    │           ├── ShaderLoader													【実装済み】
    │           ├── AudioLoader														【実装済み】
    │           ├── EffectLoader													【実装済み】
    │           ├── PrefabLoader													【実装済み】
    │           ├── LlamaModelLoader												【実装済み】
    │           └── ResourceLoader			（汎用ローダー）						【実装済み】
    ├── EditorService
    │   ├── InterFace
    │   │   ├── IEditorUI					（エディタUI基底）						【実装済み】
    │   │   └── IAnalyzer					（解析基底インターフェース）			【実装済み】
    │   ├── Command
    │   │   ├── ICommand					（コマンド基底）						【実装済み】
    │   │   ├── CommandManager				（Undo/Redo管理）						【実装済み】
    │   │   ├── ComponentCommand													【実装済み】
    │   │   ├── EntityCommand														【実装済み】
    │   │   ├── EntitySnapshotHelper												【実装済み】
    │   │   ├── PrefabCommand														【実装済み】
    │   │   ├── PropertyChangeCommand												【実装済み】
    │   │   └── TransformChangeCommand												【実装済み】
    │   ├── Analysis
    │   │   ├── AnalyzerManager				（解析マネージャ）					【実装済み】
    │   │   └── SourceAnalyzer				（ソースコード解析）					【実装済み】
    │   └── UI
    │       ├── AssetsBrowser				（アセットブラウザ）					【実装済み】
    │       ├── BRAIN						（AIチャットUI）						【実装済み】
    │       ├── DebugLogWindow				（デバッグログ表示）					【実装済み】
    │       ├── Hierarchy					（ヒエラルキー表示）					【実装済み】
    │       ├── Inspector					（インスペクタ）						【実装済み】
    │       ├── MenuBar						（メニューバー）						【実装済み】
    │       ├── PerformanceMonitor			（パフォーマンスモニタ）				【実装済み】
    │       ├── SystemSetting				（システム設定）						【実装済み】
    │       └── ViewWindow					（ビューウィンドウ）					【実装済み】
    ├── Scene
    │   ├── SceneManager					（シーン管理）							【実装済み】
    │   ├── Scene							（シーンデータ）						【実装済み】
    │   ├── Entity							（エンティティ定義）					【実装済み】
    │   ├── Interface
    │   │   ├── IComponent					（コンポーネント基底）					【実装済み】
    │   │   ├── IComponentStorage			（ストレージ基底）						【実装済み】
    │   │   ├── IScriptComponent				（スクリプト基底）					【実装済み】
    │   │   └── ISystem						（システム基底）					【実装済み】
    │   ├── Reference
    │   │   ├── ComponentRef					（コンポーネント参照）				【実装済み】
    │   │   └── EntityRef					（エンティティ参照）					【実装済み】
    │   ├── Prefab
    │   │   └── PrefabSystem					（プレハブ管理）					【実装済み】
    │   ├── Registry
    │   │   ├── SystemRegistry				（システム登録管理）					【実装済み】
    │   │   │   ├── TransformSystem												【実装済み】
    │   │   │   ├── FollowSystem													【実装済み】
    │   │   │   ├── CameraSystem													【実装済み】
    │   │   │   ├── RenderSystem													【実装済み】
    │   │   │   │   ├── RenderPass
    │   │   │   │   │   ├── IRenderPass			（レンダーパス基底）			【実装済み】
    │   │   │   │   │   ├── RenderPassContext									【実装済み】
    │   │   │   │   │   ├── GBufferPass											【実装済み】
    │   │   │   │   │   ├── LightingPass										【実装済み】
    │   │   │   │   │   ├── ForwardPass											【実装済み】
    │   │   │   │   │   ├── ShadowMapPass										【実装済み】
    │   │   │   │   │   ├── PostEffectPass										【実装済み】
    │   │   │   │   │   ├── EditorPass											【実装済み】
    │   │   │   │   │   ├── PlayerPass											【実装済み】
    │   │   │   │   │   ├── OverlayUIPass										【実装済み】
    │   │   │   │   │   └── PhysXDebugPass										【実装済み】
    │   │   │   │   ├── Renderable
    │   │   │   │   │   ├── IRenderable			（描画オブジェクト基底）		【実装済み】
    │   │   │   │   │   ├── RenderableMesh										【実装済み】
    │   │   │   │   │   ├── RenderableModel										【実装済み】
    │   │   │   │   │   ├── RenderableSprite									【実装済み】
    │   │   │   │   │   ├── RenderableBillBoard									【実装済み】
    │   │   │   │   │   ├── RenderableTerrain									【実装済み】
    │   │   │   │   │   ├── RenderableWave										【実装済み】
    │   │   │   │   │   ├── RenderableParticle									【実装済み】
    │   │   │   │   │   └── RenderableEffect									【実装済み】
    │   │   │   │   └── RenderTarget			（描画ターゲット管理）				【実装済み】
    │   │   │   ├── AudioSystem													【実装済み】
    │   │   │   ├── ParticleSystem												【実装済み】
    │   │   │   ├── EffectSystem													【実装済み】
    │   │   │   ├── TerrainSystem													【実装済み】
    │   │   │   ├── WaveSystem													【実装済み】
    │   │   │   ├── PhysicSystem													【実装済み】
    │   │   │   ├── ScriptSystem				（CSharp DLL）						【実装済み】
    │   │   │   └── CustomScriptSystem											【実装済み】
    │   │   ├── EntityRegistry				（エンティティ登録管理）				【実装済み】
    │   │   └── ComponentRegistry			（コンポーネント登録管理）				【実装済み】
    │   ├── Component
    │   │   ├── NameComponent														【実装済み】
    │   │   ├── TransformComponent													【実装済み】
    │   │   ├── ColliderComponent													【実装済み】
    │   │   ├── HitInfo						（衝突情報）						【実装済み】
    │   │   ├── AudioComponent														【実装済み】
    │   │   ├── RenderLayerComponent												【実装済み】
    │   │   ├── OrderInLayerComponent												【実装済み】
    │   │   ├── MaterialComponent													【実装済み】
    │   │   ├── TextureComponent													【実装済み】
    │   │   ├── BumpMapComponent													【実装済み】
    │   │   ├── EnvironmentMapComponent											【実装済み】
    │   │   ├── LightComponent														【実装済み】
    │   │   ├── MeshRendererComponent												【実装済み】
    │   │   ├── ModelRendererComponent												【実装済み】
    │   │   ├── BillBoardRendererComponent											【実装済み】
    │   │   ├── SpriteRendererComponent											【実装済み】
    │   │   ├── TerrainComponent													【実装済み】
    │   │   ├── WaveComponent														【実装済み】
    │   │   ├── OutlineComponent													【未実装】
    │   │   ├── ParticleComponent													【実装済み】
    │   │   ├── EffectComponent													【実装済み】
    │   │   ├── CameraComponent													【実装済み】
    │   │   ├── FollowComponent													【実装済み】
    │   │   ├── PrefabComponent													【実装済み】
    │   │   ├── CustomScriptComponent												【実装済み】
    │   │   └── ScriptComponent				（CSharp DLL）						【実装済み】
    │   └── Script（CustomScript）
    │       ├── SetScene															【実装済み】
    │       ├── ScoreManager														【実装済み】
    │       ├── ScoreSprite															【実装済み】
    │       ├── PlayerController													【実装済み】
    │       ├── CharacterController													【実装済み】
    │       ├── GameTimeManager														【実装済み】
    │       ├── TimerSprite															【実装済み】
    │       ├── CameraController													【実装済み】
    │       ├── BallController														【実装済み】
    │       ├── EnemyController														【実装済み】
    │       ├── FadeInSprite														【実装済み】
    │       ├── FadeOutSprite														【実装済み】
    │       ├── FadeSetScene														【実装済み】
    │       └── GN31																【実装済み】
    ├── Scripting（将来導入）														【未実装】
    └── EditorExtension（将来導入）													【未実装】
*/
