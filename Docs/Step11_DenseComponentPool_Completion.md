# Step 11: DenseComponentPool 完了記録

## 完了内容

- Dense Component配列
- Dense Entity配列
- Entity Indexを使うSparse Index
- Component Generation
- Structure Version
- `std::span`アクセス
- `IComponentStorage`の型非依存化
- `ComponentView` / `ConstComponentView`による型消去アクセス
- 高頻度データComponentのDense Pool移行
- Dense対象Componentから`IComponent`継承とvirtual overrideを除去

## Dense移行対象

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

Script、Collider、ModelRendererなど、DLL境界・ネイティブ資源・ポインタ安定性を優先する型はSparse Storageを維持する。

## 検証

Windows Build run #335で次の全構成が成功した。

- Script Debug x64
- Script Release x64
- GameEngine Debug x64
- GameEngine Release x64

## 後続

Sparse側に残る`IComponent`継承は、各型の所有権移行に合わせて段階的に除去する。StorageとQueryの設計はすでに`IComponent`へ依存しない。
