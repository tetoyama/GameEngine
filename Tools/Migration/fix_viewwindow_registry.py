from pathlib import Path

path = Path("Source/GameApplication/Engine/Editor/UI/ViewWindow.cpp")
text = path.read_text(encoding="utf-8-sig")
old = "\tRenderSystem* renderSystem = m_editor->sceneManager->systemRegistry->GetSystem<RenderSystem>();"
new = (
    "\tSystemRegistry* registry = m_editor->sceneManager->GetSystemRegistry();\n"
    "\tRenderSystem* renderSystem =\n"
    "\t\tregistry ? registry->GetSystem<RenderSystem>() : nullptr;\n"
    "\tif(!renderSystem) return;"
)
if text.count(old) != 1:
    raise RuntimeError(f"expected one remaining direct registry access, found {text.count(old)}")
text = text.replace(old, new, 1)
if "sceneManager->systemRegistry" in text:
    raise RuntimeError("direct SceneManager registry access remains")
path.write_text(text, encoding="utf-8", newline="\n")
