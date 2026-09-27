# Step 17-E Wave CPU Vertex Build / GPU Upload Completion

## 状態

**コード実装完了・Windows Build / 実機描画確認待ち**

Waveの毎Frame頂点更新を、純CPU BuildとMainThread GPU Uploadへ分離した。
初回Topology生成は従来どおり`WaveSystem::Initialize()`内で同期完了する。

## 旧実装

`WaveSystem.Vertex.Upload`という単一MainThread Taskの中で、次をすべて実行していた。

1. Resolution変更時のTopology頂点・Index生成
2. 毎Frameの波形頂点計算
3. D3D11 Vertex / Index Buffer生成
4. `Map(D3D11_MAP_WRITE_DISCARD)`による頂点Upload
5. Wave Time更新

CPU計算もMainThreadへ固定され、Task名と実処理の責務も一致していなかった。

## 新しいTask構造

### `WaveSystem.Vertex.Build`

- Domain: `Render`
- Phase: `Earliest`
- Affinity: `AnyWorker`
- Access:
  - `Write<WaveComponent>`
  - `Read<SceneManager>`

D3D11へ触れず、次をCPU stagingへ生成する。

- Resolution変更・Buffer喪失時のTopology頂点とIndex
- 毎FrameのAnimated Vertex
- Resolution / Amplitude / Wavelength / Timeから生成した入力Signature

### `WaveSystem.Vertex.Upload`

- Domain: `Render`
- Phase: `Early`
- Affinity: `MainThread`
- Access:
  - `Write<WaveComponent>`
  - `Read<SceneManager>`
  - `Write<GraphicsContext>`

Build済みstagingをGPUへ反映する。

- Topology更新: 一時Vertex / Index Bufferを作り、両方成功した後だけ既存Meshと交換
- 通常更新: DYNAMIC Vertex Bufferへ`Map(WRITE_DISCARD)`
- Upload成功後だけ`Time += 0.02f * Speed`

## 初期生成契約

`WaveSystem::Initialize()`は`InitializeWaveMeshes()`を同期実行する。

- 初回Render FrameまでWaveが未生成になる回帰を防ぐ
- 初期Topologyは従来同様の平面グリッド
- 最初のRender TaskでTime 0のAnimated Vertexへ更新

## 失敗時契約

### Topology Buffer生成失敗

- 新Bufferは一時`ComPtr`へ生成
- Vertex / Indexの両方が成功するまで既存Meshへ触れない
- 失敗時は以前の正常なWave Meshを表示し続ける
- stagingを維持し、次FrameでUploadだけ再試行する

### Vertex Map失敗

- stagingを維持する
- Timeを進めない
- 次Frameで同じ入力を再Uploadする

### GPU Buffer喪失

- Vertex / Index BufferまたはCountが失われた状態を検出
- `CurrentResolution = -1`へ戻す
- 次FrameのBuildでTopology再生成へ移行する

### Build後の入力変更

Upload直前にSignatureを再計算する。
Resolution / Amplitude / Wavelength / Timeが変化していた場合、古いstagingを破棄する。

## 描画側防御

`RenderableWave`は次を確認してから`DrawIndexed`する。

- `WaveComponent`
- `MeshRendererComponent`
- Vertex Buffer
- Index Buffer
- `indexCount > 0`
- `GraphicsContext`
- `ID3D11DeviceContext`

初期生成失敗中や再構築中にnull BufferをBindしない。

## 回帰テスト

`Tests/WaveMeshTaskSmokeTest.cpp`と
`.github/workflows/wave-mesh-task.yml`を追加した。

検証内容:

- TopologyのVertex / Index数と座標契約
- Animated Vertexの有限値
- Resolution / Amplitude / Time変更時のSignature差分
- 無効Resolution / Wavelengthの拒否
- Build / Upload TaskのDomain・Phase・Affinity・Access
- `Initialize()`での同期Topology生成
- Build側がD3D11 Device / Mapへ触れないこと
- Upload成功後だけTimeを進めること
- 一時Buffer生成後のTransactional Commit
- RenderableのBuffer Guard

## 実機確認

- [ ] Windows Debug x64 Build
- [ ] Windows Release x64 Build
- [ ] 起動直後からWaveが表示される
- [ ] Resolution変更後も旧Meshを維持しつつ切り替わる
- [ ] Amplitude / Wavelength / Speed変更が反映される
- [ ] Play / Stop / Scene Reload後に二重解放がない
- [ ] Performance MonitorでMainThread Render時間を変更前後比較
- [ ] D3D11 Debug LayerでMap / Buffer Bind警告がない
