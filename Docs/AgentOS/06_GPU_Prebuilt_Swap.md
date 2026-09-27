# GPU推論：ビルドせずDLL差し替えで対応する

Status: 2026-07-26

`ggml-vulkan.dll` をソースからビルドする試みは、CMakeのクラッシュ
（`0xC0000409`）で行き詰まった。切り分けの結果、原因は環境ではなく
ggml側のCMakeコードにあることまでは分かったが、そこから先は割に合わない。

**ビルドせずに済む道がある。** それがこの文書。

---

## 1. なぜ差し替えが可能なのか

`GameEngine.vcxproj` を調べたところ、llama関連のソース参照が
**すべて存在しないパスを指していた**。

```
vcxproj の参照 : Source\GameApplication\Backends\llama\models\afmoe.cpp
実際のファイル : Source\GameApplication\Backends\llama\src\models\afmoe.cpp
                                                  ^^^^ src が抜けている
```

`models\` に入っているのは `.gguf` の語彙ファイルだけで、`.cpp` は1つも無い。
104件すべてが同じ状態。

つまり **エンジンは llama のソースを1行もコンパイルしていない。**
依存しているのは次の2つだけ。

| | 役割 |
|---|---|
| `llama.lib` | リンク時のインポートライブラリ |
| `llama.dll` / `ggml*.dll` | 実行時の本体 |

したがって、**整合の取れた一式に差し替えれば、そのままGPU対応になる**。
ビルドは要らない。

> 補足: vcxproj の104件の壊れた参照自体は無害（VSは存在しないファイルを
> 無視する）だが、紛らわしいので整理する価値はある。

---

## 2. エンジンが使っているAPI

差し替え後も動くことを確認するための一覧。いずれも安定APIで、
llama.cpp の最近のリリースなら揃っている。

```
llama_backend_init / llama_backend_free
llama_model_default_params / llama_model_load_from_file / llama_model_free
llama_model_get_vocab / llama_model_chat_template
llama_context_default_params / llama_init_from_model / llama_free
llama_batch_init / llama_batch_free / llama_decode
llama_get_memory / llama_memory_clear / llama_memory_seq_rm
llama_sampler_chain_init / llama_sampler_chain_add / llama_sampler_free
llama_sampler_init_dist / _temp / _top_k / _penalties / llama_sampler_accept
llama_chat_apply_template / llama_n_batch
llama_supports_gpu_offload / llama_max_devices / llama_print_system_info
```

`llama_kv_cache_clear` は古い名前で、新しい版では `llama_memory_clear` に
置き換わっている。コード内に両方が現れるため、**差し替え後にリンクエラーが
出たらここを疑う**（該当箇所を新しい名前へ寄せる）。

---

## 3. 現在の構成

```
llama.dll        Jun 20 13:32
ggml.dll         Jun 20 13:32
ggml-base.dll    Jun 20 13:32
ggml-cpu.dll     Jun 20 13:32
llama.lib
```

vendored ggml のバージョンは `0.15.1`。

---

## 4. 手順

### 4.1 バックアップ

**必ず先に退避する。** 失敗したら戻せるようにしておく。

```bat
mkdir Backup_DLL_CPU
copy llama.dll   Backup_DLL_CPU\
copy llama.lib   Backup_DLL_CPU\
copy ggml.dll    Backup_DLL_CPU\
copy ggml-base.dll Backup_DLL_CPU\
copy ggml-cpu.dll  Backup_DLL_CPU\
```

### 4.2 取得

llama.cpp の GitHub Releases から、Windows x64 の **Vulkan** 版を取る。

```
llama-<build番号>-bin-win-vulkan-x64.zip
```

`vulkan` と付いたものを選ぶこと。`cpu` 版には `ggml-vulkan.dll` が入っていない。

### 4.3 配置

zip の中から次をリポジトリのルート（exeと同じ場所）へコピーする。

```
llama.dll
llama.lib          （zipに含まれない場合は §5 を参照）
ggml.dll
ggml-base.dll
ggml-cpu.dll
ggml-vulkan.dll    ← これが目的のもの
```

**一式まとめて入れ替えること。** 新しい `ggml-vulkan.dll` だけを古い
`ggml-base.dll` と混ぜると、ABIが合わずに読み込みに失敗する。

### 4.4 確認

エンジンを起動し、BRAIN の Status タブを開く。

```
Inference Backend: CPU
▸ Detected devices
    • GPU offload: supported     ← これが出れば成功
    • Max devices: 1
[x] Offload to GPU
```

チェックを入れるとモデルを再ロードしてGPU推論に切り替わる。

---

## 5. うまくいかないとき

**`llama.lib` が zip に入っていない**
リリースzipはランタイム用でインポートライブラリを含まないことがある。
その場合は既存の `llama.lib` をそのまま使う。エクスポートされている
シンボル名が変わっていなければリンクは通る。通らなければ §2 の一覧と
新しい `llama.h` を突き合わせる。

**リンクエラー `llama_kv_cache_clear` が見つからない**
新しい版で `llama_memory_clear` に改名されている。呼び出し側を寄せる。

**起動時にDLLの読み込みで落ちる**
新旧のDLLが混在している。`llama.dll` と `ggml*.dll` を
**同じzipのもので統一**する。

**`GPU offload: not available` のまま**
`ggml-vulkan.dll` がexeと同じ場所に無いか、Vulkanランタイムが
入っていない。GPUドライバを更新する。

**戻したい**
`Backup_DLL_CPU\` の中身をルートへ上書きコピーすれば元通り。

---

## 6. 参考：ソースからのビルドを断念した経緯

`Docs/AgentOS/05_GPU_Backend_Setup.md` に手順は残してある。
切り分けの結果は次のとおりで、原因はggml側のCMakeコードにあると分かったが、
その先の特定には至らなかった。

| 試したこと | 結果 |
|---|---|
| CMake 4.0.3 → VS同梱の3.x | 同じ地点で同じクラッシュ |
| ソースを `C:\ggml-src` へ複製（非ASCII回避） | 同じ |
| ジェネレータを Visual Studio → Ninja | 同じ |
| 最小プロジェクト `project(t C CXX ASM)` | **正常**（環境は健全） |
| ggml をオプション無しで構成 | クラッシュ |
| ggml に Vulkanオプション付きで構成 | クラッシュ |

最後の2つが同じ結果になったため、**Vulkan関連の設定は無関係**で、
ggml の基本構成の時点で落ちていることが確定した。
