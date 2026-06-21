from pathlib import Path


def load(path):
    return Path(path).read_text(encoding="utf-8-sig")


def save(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def one(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("replacement mismatch")
    return text.replace(old, new, 1)


h = "Source/GameApplication/Engine/Scene/sceneManager.h"
t = load(h)
t = one(t, "\n\t// Compatibility owner during call-site migration. External code should use GetSystemRegistry().\n\tstd::unique_ptr<SystemRegistry> systemRegistry;\n\nprivate:\n", "\nprivate:\n")
t = one(t, "\tvoid TempSave();\n\tvoid TempLoad();\n\n", "\tvoid TempSave();\n\tvoid TempLoad();\n\n\tstd::unique_ptr<SystemRegistry> m_systemRegistry;\n\n")
t = t.replace("return systemRegistry.get();", "return m_systemRegistry.get();")
save(h, t)

cpp = "Source/GameApplication/Engine/Scene/sceneManager.cpp"
t = load(cpp)
t = one(t, "\tsystemRegistry = std::make_unique<SystemRegistry>();", "\tm_systemRegistry = std::make_unique<SystemRegistry>();")
t = t.replace("systemRegistry->", "m_systemRegistry->")
t = one(t, "\tm_SceneContext.systemRegistry = systemRegistry.get();", "\tm_SceneContext.systemRegistry = m_systemRegistry.get();")
t = one(t, "\tif(systemRegistry){", "\tif(m_systemRegistry){")
t = one(t, "\t\tsystemRegistry.reset();", "\t\tm_systemRegistry.reset();")
save(cpp, t)

view = "Source/GameApplication/Engine/Editor/UI/ViewWindow.cpp"
t = load(view)
t = one(t, "\tGraphicsContext* graphicsContext = m_editor->sceneManager->GetContext()->graphics;\n\tID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();\n\tRenderSystem* renderSystem = m_editor->sceneManager->systemRegistry->GetSystem<RenderSystem>();", "\tGraphicsContext* graphicsContext = m_editor->sceneManager->GetContext()->graphics;\n\tID3D11DeviceContext* deviceContext = graphicsContext ? graphicsContext->GetDeviceContext() : nullptr;\n\tSystemRegistry* registry = m_editor->sceneManager->GetSystemRegistry();\n\tRenderSystem* renderSystem = registry ? registry->GetSystem<RenderSystem>() : nullptr;\n\tif(!graphicsContext || !deviceContext || !renderSystem || !renderSystem->m_EditorPass) return;")
save(view, t)
