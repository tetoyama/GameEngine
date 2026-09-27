# Step 16-A Backend Config Completion

## 状態

**完了**

## 目的

起動前に描画Backendを確定し、GraphicsContextやRendererが初期化後にBackendを差し替えない契約を作る。

## 実装

- `EngineConfig.yaml`の`Graphics.Backend`を型付き`EngineGraphicsConfig`へ読み込む
- `ConfigService::Initialize()`でGraphicsContextより先に要求Backendを公開する
- `BackendType`の保存名を一か所で変換する
- 未知のBackend名はDirect3D 11へ安全にフォールバックする
- Project SettingsからRendering APIを保存できる
- Backend変更は次回起動時に反映する
- 未実装のDirect3D 12 / VulkanはUI上で選択不可にする
- Null BackendはRHI単体試験専用として通常起動では選択不可にする

## 起動契約

1. `ConfigService`が`Asset/EngineConfig.yaml`を読む
2. `Graphics.Backend`を`BackendType`へ変換する
3. `RHI::SetRequestedBackend()`へ設定する
4. GraphicsContextが要求Backendを確認して初期化する
5. 現在正式対応しているDirect3D 11以外は、未実装のまま暗黙にD3D11へ差し替えず初期化失敗とする

## 設定例

```yaml
Graphics:
  Backend: Direct3D11
```

有効な保存名:

- `Null`
- `Direct3D11`
- `Direct3D12`
- `Vulkan`

ただし通常のProject Settingsで選択可能なのは、Renderer接続が完了しているBackendだけとする。

## 検証

- Migration Plan Validation run #195: success
- RHI Smoke Test run #244: success
- Windows Build run #764: success

## 完了条件

- 起動ConfigがGraphics初期化より先に解決される
- 設定値がProject Settingsから永続化できる
- 未実装Backendを実装済みとして扱わない
- Backend選択がNative API型を上位層へ追加しない
