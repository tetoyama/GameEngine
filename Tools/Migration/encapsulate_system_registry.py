from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


header = "Source/GameApplication/Engine/Scene/sceneManager.h"
text = read(header)
text = replace_once(
    text,
    "\tSceneManagerContext* GetContext(){\n\t\treturn &m_SceneContext;\n\t}\n",
    "\tSceneManagerContext* GetContext() noexcept {\n\t\treturn &m_SceneContext;\n\t}\n\n"
    "\tconst SceneManagerContext* GetContext() const noexcept {\n\t\treturn &m_SceneContext;\n\t}\n\n"
    "\tSystemRegistry* GetSystemRegistry() noexcept {\n\t\treturn m_systemRegistry.get();\n\t}\n\n"
    "\tconst SystemRegistry* GetSystemRegistry() const noexcept {\n\t\treturn m_systemRegistry.get();\n\t}\n",
    "SceneManager accessors",
)
text = replace_once(
    text,
    "\n\t// SceneManagerが唯一の所有者。SceneManagerContextへは非所有ポインタを公開する。\n"
    "\tstd::unique_ptr<SystemRegistry> systemRegistry;\n\nprivate:\n",
    "\nprivate:\n",
    "public SystemRegistry owner",
)
text = replace_once(
    text,
    "\tvoid TempSave();\n\tvoid TempLoad();\n\n",
    "\tvoid TempSave();\n\tvoid TempLoad();\n\n"
    "\t// SceneManagerが唯一の所有者。ContextとUIへは非所有ポインタだけを公開する。\n"
    "\tstd::unique_ptr<SystemRegistry> m_systemRegistry;\n\n",
    "private SystemRegistry owner",
)
write(header, text)

source = "Source/GameApplication/Engine/Scene/sceneManager.cpp"
text = read(source)
text = replace_once(
    text,
    "\tsystemRegistry = std::make_unique<SystemRegistry>();",
    "\tm_systemRegistry = std::make_unique<SystemRegistry>();",
    "SystemRegistry construction",
)
text = text.replace("systemRegistry->", "m_systemRegistry->")
text = replace_once(
    text,
    "\tm_SceneContext.systemRegistry = systemRegistry.get();",
    "\tm_SceneContext.systemRegistry = m_systemRegistry.get();",
    "Context registry publication",
)
text = replace_once(text, "\tif(systemRegistry){", "\tif(m_systemRegistry){", "shutdown guard")
text = replace_once(text, "\t\tsystemRegistry.reset();", "\t\tm_systemRegistry.reset();", "owner reset")
if "systemRegistry->" in text or "if(systemRegistry)" in text:
    raise RuntimeError("obsolete SceneManager ownership access remains")
write(source, text)

settings = "Source/GameApplication/Engine/Editor/UI/SystemSettingImplementation.inl"
text = read(settings)
text = replace_once(
    text,
    "inline void DrawSystemSettings(SceneManager* sceneManager) {\n"
    "\tif(!sceneManager || !sceneManager->systemRegistry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n\t}\n\n"
    "\tbool hasSettings = false;\n"
    "\tfor(auto& system : sceneManager->systemRegistry->GetSystems()) {",
    "inline void DrawSystemSettings(SceneManager* sceneManager) {\n"
    "\tSystemRegistry* registry = sceneManager ? sceneManager->GetSystemRegistry() : nullptr;\n"
    "\tif(!registry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n\t}\n\n"
    "\tbool hasSettings = false;\n"
    "\tfor(auto& system : registry->GetSystems()) {",
    "system settings accessor",
)
text = replace_once(
    text,
    "\tif(!sceneManager || !sceneManager->systemRegistry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n\t}\n\n"
    "\tSystemRegistry& registry = *sceneManager->systemRegistry;\n"
    "\tScheduleProfileYamlExportUI::Draw(registry, viewState);\n"
    "\tScheduleProfilerView::Draw(registry, viewState);",
    "\tSystemRegistry* registry = sceneManager ? sceneManager->GetSystemRegistry() : nullptr;\n"
    "\tif(!registry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n\t}\n\n"
    "\tScheduleProfileYamlExportUI::Draw(*registry, viewState);\n"
    "\tScheduleProfilerView::Draw(*registry, viewState);",
    "schedule settings accessor",
)
text = replace_once(
    text,
    "\t\tif(sceneManager && sceneManager->systemRegistry) {\n"
    "\t\t\tsceneManager->systemRegistry->EncodeAll(config->editorConfig);\n"
    "\t\t}",
    "\t\tif(SystemRegistry* registry =\n"
    "\t\t\tsceneManager ? sceneManager->GetSystemRegistry() : nullptr) {\n"
    "\t\t\tregistry->EncodeAll(config->editorConfig);\n"
    "\t\t}",
    "save settings accessor",
)
if "sceneManager->systemRegistry" in text:
    raise RuntimeError("direct SystemRegistry access remains in settings UI")
write(settings, text)

view = "Source/GameApplication/Engine/Editor/UI/ViewWindow.cpp"
text = read(view)
text = replace_once(
    text,
    "\tGraphicsContext* graphicsContext = m_editor->sceneManager->GetContext()->graphics;\n"
    "\tID3D11DeviceContext* deviceContext = graphicsContext->GetDeviceContext();\n"
    "\tRenderSystem* renderSystem = m_editor->sceneManager->systemRegistry->GetSystem<RenderSystem>();",
    "\tGraphicsContext* graphicsContext = m_editor->sceneManager->GetContext()->graphics;\n"
    "\tID3D11DeviceContext* deviceContext = graphicsContext ? graphicsContext->GetDeviceContext() : nullptr;\n"
    "\tSystemRegistry* registry = m_editor->sceneManager->GetSystemRegistry();\n"
    "\tRenderSystem* renderSystem = registry ? registry->GetSystem<RenderSystem>() : nullptr;\n"
    "\tif(!graphicsContext || !deviceContext || !renderSystem || !renderSystem->m_EditorPass) return;",
    "ViewWindow registry accessor",
)
if "sceneManager->systemRegistry" in text:
    raise RuntimeError("direct SystemRegistry access remains in ViewWindow")
write(view, text)

print("SystemRegistry ownership encapsulated")
