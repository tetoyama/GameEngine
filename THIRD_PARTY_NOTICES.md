# Third-Party Notices

GameEngine uses third-party libraries, SDK components, and external materials that remain subject to their respective licenses and terms.

The Apache License 2.0 applied to project-authored GameEngine source code and related documentation does not replace or override those licenses. When redistributing GameEngine, retain the copyright notices, license texts, attribution notices, and other conditions required by each included component.

This document is a summary of the principal dependencies currently identified by the project. The license files and notices distributed with each component are authoritative.

| Component | Purpose | License / terms | Upstream |
|---|---|---|---|
| Dear ImGui | Editor UI | MIT | https://github.com/ocornut/imgui |
| ImGuizmo | 3D transform gizmo | MIT | https://github.com/CedricGuillemet/ImGuizmo |
| imnodes | Node editor UI | MIT | https://github.com/Nelarius/imnodes |
| yaml-cpp | YAML serialization | MIT | https://github.com/jbeder/yaml-cpp |
| Assimp | Model import | BSD 3-Clause | https://github.com/assimp/assimp |
| Effekseer | Effect rendering | MIT | https://github.com/effekseer/Effekseer |
| NVIDIA PhysX | Physics | BSD 3-Clause | https://github.com/NVIDIA-Omniverse/PhysX |
| llama.cpp | Local LLM inference | MIT | https://github.com/ggml-org/llama.cpp |
| DirectXTex | Texture processing | MIT | https://github.com/microsoft/DirectXTex |
| DirectXMath and Windows SDK APIs | Math, graphics, audio, input, and platform integration | Microsoft / Windows SDK terms | https://learn.microsoft.com/windows/ |

## External assets and content

Models, textures, audio, fonts, sample content, and other external materials are not automatically covered by the project Apache License 2.0. Their individual source and license information must be preserved wherever applicable.

## Maintenance

When adding or vendoring a dependency:

1. record its name, source, version or revision where practical, and license;
2. retain its original license and copyright notices;
3. document modifications when required by the upstream license;
4. update this file before distributing the resulting source or binaries.
