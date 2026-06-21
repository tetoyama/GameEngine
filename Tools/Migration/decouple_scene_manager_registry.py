from pathlib import Path

header = Path("Source/GameApplication/Engine/Scene/sceneManager.h")
text = header.read_text(encoding="utf-8-sig")
text = text.replace('#include "Registry/systemRegistry.h"\n', '')
text = text.replace('class EditorService;\n', 'class EditorService;\nclass SystemRegistry;\n')
text = text.replace('\tSceneManager() = default;', '\tSceneManager();')
header.write_text(text, encoding="utf-8", newline="\n")

source = Path("Source/GameApplication/Engine/Scene/sceneManager.cpp")
text = source.read_text(encoding="utf-8-sig")
old = 'SceneManager::~SceneManager() = default;'
new = 'SceneManager::SceneManager() = default;\n\nSceneManager::~SceneManager() = default;'
if text.count(old) != 1:
    raise RuntimeError("SceneManager destructor anchor mismatch")
source.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
