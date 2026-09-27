# GPU推論バックエンドのセットアップ

Status: 2026-07-26

CPU推論では1回の生成に約77秒、1セッションで7〜8分かかっていた。
エディタ作業中はGPUが空いているため、そのときだけGPUへ逃がせるようにする。

---

## 1. 仕組み

llama.cpp は計算バックエンドを **DLLとして動的に読み込む**。

```
llama_backend_init()
  └─ ggml_backend_load_all()      ← exeと同じ場所のDLLを探して登録する
```

`ggml_backend_load_all()` は `llama_backend_init()` の内部で呼ばれる
（`Backends/llama/src/llama.cpp` の実装参照）。したがってエンジン側で追加の
呼び出しは不要で、**exeの隣にDLLを置くだけで登録される**。

現在置かれているのは以下だけで、GPU用が無い。

```
ggml-base.dll
ggml-cpu.dll
ggml.dll
llama.dll
```

`ggml-vulkan.dll` を作って同じ場所へ置けば、BRAINのStatusタブでGPUが
選べるようになる。

> **注意**: `ggml_backend_*` をエンジン側から直接呼んではいけない。
> このプロジェクトがリンクしているのは `llama.lib` のみで
> （`LLAMAService.cpp` の `#pragma comment(lib, "llama.lib")`）、
> ggml側のインポートライブラリが存在しないため未解決の外部シンボルになる。
> バックエンドの状態取得は `llama_supports_gpu_offload()` /
> `llama_print_system_info()` など `llama.h` のAPIで行うこと。

---

## 2. 事前に必要なもの

| | 用途 |
|---|---|
| **Vulkan SDK**（LunarG） | ヘッダ・ローダ・`glslc`・SPIRV-Headers |
| **CMake** | 3.14以上 |
| **Visual Studio 2022** | ビルドツールチェーン |

### Vulkan SDK は Core だけでよい

`ggml-vulkan/CMakeLists.txt` が要求するのは次の2つだけで、
どちらも既定のインストールに含まれる。

```cmake
find_package(Vulkan COMPONENTS glslc REQUIRED)   # 9行目
find_package(SPIRV-Headers CONFIG REQUIRED)      # 14行目
```

インストーラの追加コンポーネントはいずれも不要。

| 選択項目 | 要否 |
|---|---|
| Vulkan SDK Core | **必要** |
| Volk / VMA | 不要 |
| SDL2 | 不要 |
| 32-bit libraries | 不要（x64ビルドのため） |
| Crash diagnostic layers | 不要 |

インストール後の確認：

```bat
echo %VULKAN_SDK%
"%VULKAN_SDK%\Bin\glslc.exe" --version
```

> `find_package(SPIRV-Headers CONFIG ...)` はCMakeのconfigファイルを探すため、
> SDKのバージョンによっては見つけられないことがある。その場合は
> `-DCMAKE_PREFIX_PATH="%VULKAN_SDK%"` を追加して場所を教える。

ビルド中に `vulkan-shaders-gen` というホスト用ツールが生成されるが、
これはVSのコンパイラで作られるため追加の準備は要らない。

CUDAではなくVulkanを選ぶ理由は、CUDA Toolkit（数GB）を要求せず、
NVIDIA以外のGPUでも動くため。純粋な速度ではCUDAが上回る。

---

## 3. ビルド手順

リポジトリルートの **`BuildVulkanBackend.bat` を実行する**（ダブルクリックで可）。

事前チェック → configure → ビルド → DLLの配置までを一括で行い、
どの段階で失敗したかを日本語で表示する。

```
BuildVulkanBackend.bat
```

> コマンドを手で打つ場合、`^`（行継続）を使った複数行のコマンドを
> **1行に貼り付けると引数の区切りが壊れる**。さらに configure の
> エラーを見落としたまま次へ進むと
> `build-vulkan is not a directory` という分かりにくい失敗になる。
> バッチを使えばこの罠を踏まない。

> **バッチを編集するときの注意**:
> `BuildVulkanBackend.bat` は **ASCIIのみ・CRLF改行**で書いてある。
> cmd.exe はバッチファイルをシステムのANSIコードページ（日本語Windowsでは
> CP932）で読むため、UTF-8で日本語を書くと文字化けするだけでなく、
> 壊れたバイト列で**行の途中からコマンドとして解釈され始める**。
> 実際に `'縺ｮ' は、内部コマンドまたは外部コマンド...` という形で失敗した。
> 日本語の説明はこのドキュメント側に置き、バッチは英語のままにしておくこと。

> **configure は数分かかる。途中で無反応に見えても中断しないこと。**
> `-- The CXX compiler identification is MSVC ...` の直後から
> コンパイラのABI検出に入り、ここで長く沈黙する。
> 中断すると `CMakeCache.txt` が作られず、次の段階で
> `Error: could not load cache` という無関係に見える失敗になる。

バッチが行っているのは実質次の2コマンド（1行ずつ）。

```bat
cmake -S <llamaのパス>\ggml -B C:\ggml-vulkan-build -G "Visual Studio 17 2022" -A x64 -DGGML_VULKAN=ON -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DCMAKE_PREFIX_PATH="%VULKAN_SDK%"

cmake --build C:\ggml-vulkan-build --config Release --target ggml-vulkan
```

**対象は llama 全体ではなく `ggml` サブディレクトリ**にしている。
`ggml/CMakeLists.txt` は `project("ggml" C CXX ASM)` を持つ独立した
プロジェクトで、単体で構成できる。必要なのは `ggml-vulkan` だけなので、
llama側のCMake処理をまるごと省ける（llama全体3003ファイルに対しggmlは1103）。

ビルド先を `C:\ggml-vulkan-build` にしているのは意図的で、
OneDrive同期下の日本語パスで中間ファイルを大量生成すると、
同期とビルドが競合して不可解な失敗をすることがあるため。

各オプションの意味：

| オプション | なぜ必要か |
|---|---|
| `GGML_VULKAN=ON` | Vulkanバックエンドを対象に含める |
| `BUILD_SHARED_LIBS=ON` | DLLとして出力する。`GGML_BACKEND_DL` の前提条件 |
| `GGML_BACKEND_DL=ON` | バックエンドを**独立したDLL**として切り出す。これが無いと`ggml.dll`本体へ静的に取り込まれ、既存のDLLと差し替える羽目になる |
| `LLAMA_BUILD_*=OFF` | CLIやサーバは不要。ビルド時間の節約 |

`--target ggml-vulkan` を指定して、必要なDLLだけを作る。

---

## 4. 配置

バッチが自動で行うため、通常は何もしなくてよい。

手動で行う場合は、出力された DLL を **exeと同じ場所**（リポジトリルート）へ置く。
出力先はCMakeの設定で変わるため、決め打ちせず探すのが確実。

```bat
dir /s /b C:\ggml-vulkan-build\ggml-vulkan.dll
copy /Y <見つかったパス> .
```

---

## 5. 確認

エンジンを起動し、BRAIN の **Status タブ** を開く。

配置前:

```
Inference Backend: CPU only (no GPU backend found)
▸ Detected devices
    • GPU offload: not available
[ ] Offload to GPU        ← 無効
```

配置後:

```
Inference Backend: CPU
▸ Detected devices
    • GPU offload: supported
    • Max devices: 1
    • Vulkan : ...
[ ] Offload to GPU        ← 有効になる
```

チェックを入れるとモデルが再ロードされ、GPU推論に切り替わる。

---

## 6. 設計上の制約

**切り替えにはモデルの再ロードが必要。**
`llama_model_params::n_gpu_layers` はモデルのロード時にしか指定できないため、
CPU⇔GPUの切り替えは数GBの読み直しを伴う。生成中は変更できないようにしてある。

**既定はCPU。**
ゲームエンジンに埋め込む以上、何もしなければVRAMを描画に残す側へ倒す。
GPUを使うのはエディタ作業中の明示的な選択とする。

**メモリの二重確保に注意。**
`ResourceLoader` のキャッシュキーには引数（GPU層数）が含まれるため、
CPU用とGPU用の同一モデルが別エントリとして共存しうる。
`LLAMAService::ReloadModel()` は切り替え時に旧エントリを明示的に
`Unload` している。

---

## 7. うまくいかないとき

**`Offload to GPU` が有効にならない**
`ggml-vulkan.dll` がexeと同じ場所にあるか確認する。パスが違うと
`ggml_backend_load_all()` が見つけられない。

**DLLはあるのに認識されない**
依存DLLの不足が多い。Dependencies等でVulkanランタイムの解決を確認する。
またDebug/Releaseのランタイム不一致でも読み込みに失敗する。

**未解決の外部シンボル `ggml_*`**
エンジン側から ggml のAPIを直接呼んでいる。§1の注意を参照し、
`llama.h` のAPIへ置き換える。

**configure が終了コード `-1073740791` で落ちる**
`0xC0000409`（fail-fast）で、**CMake自体がクラッシュしている**。
設定の誤りではない。この環境では次の切り分けを行った。

| 試したこと | 結果 |
|---|---|
| CMake 4.0.3 → VS同梱の3.x へ変更 | 同じ場所で同じクラッシュ |
| ソースを非ASCIIパスから `C:\ggml-src` へ複製 | 同じ場所で同じクラッシュ |
| ジェネレータを Ninja へ変更 | ← 現在の構成 |

止まる位置は `C`/`CXX` のコンパイラ識別の直後で、
`ASM` の識別行が出る前。ggml は `project("ggml" C CXX ASM)` を宣言している。
CMakeのバージョンでもパスでも再現したため、
Visual Studio ジェネレータ側の経路を疑い、Ninja へ切り替えた。

Ninjaでも同じなら、CMake外の要因を疑う。

- リアルタイム保護（ウイルス対策）を一時的に無効化する
- Visual Studio の「修復」を実行する
- イベントビューアー（Windowsログ → Application）で
  `cmake.exe` の障害モジュール名を確認する

**GPUにしたのに速くならない**
`n_gpu_layers` が0のままの可能性がある。UIのチェックボックスは -1（全層）を
渡す。ログの `LLAMA モデルのロードを開始します: ... (gpuLayers=-1)` を確認する。
