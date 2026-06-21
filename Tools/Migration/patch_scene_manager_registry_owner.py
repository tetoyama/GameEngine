from pathlib import Path

path = Path("Source/GameApplication/Engine/Scene/sceneManager.cpp")
text = path.read_text(encoding="utf-8-sig")

old = '#include <Editor/UI/Hierarchy.h>\n\nvoid SceneManager::Initialize'
new = '#include <Editor/UI/Hierarchy.h>\n\nSceneManager::~SceneManager() = default;\n\nvoid SceneManager::Initialize'
if text.count(old) != 1:
    raise SystemExit(f"destructor anchor mismatch: {text.count(old)}")
text = text.replace(old, new, 1)

old = 'systemRegistry = std::make_shared<SystemRegistry>();'
new = 'systemRegistry = std::make_unique<SystemRegistry>();'
if text.count(old) != 1:
    raise SystemExit(f"SystemRegistry construction mismatch: {text.count(old)}")
text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8", newline="\n")
