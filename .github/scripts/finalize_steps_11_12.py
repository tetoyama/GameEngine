from pathlib import Path

plan_path = Path("Docs/ECS_Scheduler_Migration_Plan.md")
text = plan_path.read_text(encoding="utf-8-sig")

start = text.find("### Step 11: DenseComponentPool")
end = text.find("### Step 13: Job System", start)
if start < 0 or end < 0:
    raise RuntimeError("Step 11-12 block not found")

replacement = """### Step 11: DenseComponentPool

状態: **完了（高頻度データComponent移行段階）**

- [x] Dense Component配列
- [x] Dense Entity配列
- [x] Entity IndexベースのSparse Index
- [x] Component Generation
- [x] Structure Version
- [x] `std::span`アクセス
- [x] `IComponentStorage` の `IComponent*` 依存を除去
- [x] `ComponentView` / `ConstComponentView` による型消去アクセス
- [x] 高頻度データComponentをDense Poolへ移行
- [x] Dense対象Componentから `IComponent` 継承とvirtual overrideを除去

Dense移行対象:

- NameComponent
- TransformComponent
- RenderLayerComponent
- OrderInLayerComponent
- MaterialComponent
- LightComponent
- ParticleComponent
- CameraComponent
- FollowComponent
- EnvironmentMapComponent

Script、Collider、ModelRendererなど、DLL境界・ネイティブ資源・ポインタ安定性を優先する型はSparse Storageを維持する。これらの継承除去は各所有権移行Stepで行う。

検証: Windows Build run #335でScript / GameEngineのDebug・Release x64が成功。

### Step 12: 無確保Query

状態: **完了**

- [x] `ComponentMask` による遅延フィルタQuery View
- [x] Query生成時の結果vector確保を除去
- [x] `Read<T>` / `Write<T>` から `SystemAccess` を自動生成
- [x] `SystemScheduleBuilder::AddQueryTask<QueryType>()`
- [x] Query外の構造変更を `StructuralAccess` で明示
- [x] TransformSystemを初期移行
- [x] Schedule外の即時削除経路はスナップショット列挙でiterator無効化を防止

検証: Windows Build run #358でScript / GameEngineのDebug・Release x64が成功。

"""

plan_path.write_text(text[:start] + replacement + text[end:], encoding="utf-8")

route_workflow = Path(".github/workflows/route-component-operations.yml")
if route_workflow.exists():
    route_workflow.unlink()
