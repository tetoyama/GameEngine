from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


# SceneManager owns SystemRegistry privately and publishes only non-owning access.
path = "Source/GameApplication/Engine/Scene/sceneManager.h"
text = load(path)
text = replace_once(
    text,
    "\tSceneManagerContext* GetContext(){\n"
    "\t\treturn &m_SceneContext;\n"
    "\t}\n",
    "\tSceneManagerContext* GetContext() noexcept {\n"
    "\t\treturn &m_SceneContext;\n"
    "\t}\n\n"
    "\tconst SceneManagerContext* GetContext() const noexcept {\n"
    "\t\treturn &m_SceneContext;\n"
    "\t}\n\n"
    "\tSystemRegistry* GetSystemRegistry() noexcept {\n"
    "\t\treturn m_systemRegistry.get();\n"
    "\t}\n\n"
    "\tconst SystemRegistry* GetSystemRegistry() const noexcept {\n"
    "\t\treturn m_systemRegistry.get();\n"
    "\t}\n",
    "SceneManager accessors",
)
text = replace_once(
    text,
    "\n\t// SceneManagerが唯一の所有者。SceneManagerContextへは非所有ポインタを公開する。\n"
    "\tstd::unique_ptr<SystemRegistry> systemRegistry;\n\n"
    "private:\n",
    "\nprivate:\n",
    "public SystemRegistry owner",
)
text = replace_once(
    text,
    "\tstd::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;\n"
    "\tSceneManagerContext m_SceneContext;\n",
    "\t// SceneManagerが唯一の所有者。ContextとUIへは非所有ポインタだけを公開する。\n"
    "\tstd::unique_ptr<SystemRegistry> m_systemRegistry;\n\n"
    "\tstd::unordered_map<std::string, std::shared_ptr<Scene>> m_activeScenes;\n"
    "\tSceneManagerContext m_SceneContext;\n",
    "private SystemRegistry owner",
)
save(path, text)


# Rename internal ownership references without touching SceneManagerContext::systemRegistry.
path = "Source/GameApplication/Engine/Scene/sceneManager.cpp"
text = load(path)
replacements = [
    ("\tsystemRegistry = std::make_unique<SystemRegistry>();",
     "\tm_systemRegistry = std::make_unique<SystemRegistry>();",
     "SystemRegistry construction"),
    ("\tm_SceneContext.systemRegistry = systemRegistry.get();",
     "\tm_SceneContext.systemRegistry = m_systemRegistry.get();",
     "Context registry publication"),
    ("\tif(systemRegistry){",
     "\tif(m_systemRegistry){",
     "SystemRegistry shutdown guard"),
    ("\t\tsystemRegistry.reset();",
     "\t\tm_systemRegistry.reset();",
     "SystemRegistry reset"),
]
for old, new, label in replacements:
    text = replace_once(text, old, new, label)

count = text.count("systemRegistry->")
if count < 1:
    raise RuntimeError("internal SystemRegistry calls: expected at least one match")
text = text.replace("systemRegistry->", "m_systemRegistry->")

# Verify the only remaining token is the SceneManagerContext field publication/clear.
remaining = [
    line.strip()
    for line in text.splitlines()
    if "systemRegistry" in line
]
expected = {
    "m_SceneContext.systemRegistry = m_systemRegistry.get();",
    "m_SceneContext.systemRegistry = nullptr;",
}
if set(remaining) != expected:
    raise RuntimeError(f"unexpected sceneManager.cpp registry references: {remaining}")
save(path, text)


# Editor UI resolves a non-owning registry through the accessor.
path = "Source/GameApplication/Engine/Editor/UI/SystemSettingImplementation.inl"
text = load(path)
text = replace_once(
    text,
    "inline void DrawSystemSettings(SceneManager* sceneManager) {\n"
    "\tif(!sceneManager || !sceneManager->systemRegistry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n"
    "\t}\n\n"
    "\tbool hasSettings = false;\n"
    "\tfor(auto& system : sceneManager->systemRegistry->GetSystems()) {",
    "inline void DrawSystemSettings(SceneManager* sceneManager) {\n"
    "\tSystemRegistry* registry = sceneManager ? sceneManager->GetSystemRegistry() : nullptr;\n"
    "\tif(!registry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n"
    "\t}\n\n"
    "\tbool hasSettings = false;\n"
    "\tfor(auto& system : registry->GetSystems()) {",
    "System settings registry access",
)
text = replace_once(
    text,
    "\tif(!sceneManager || !sceneManager->systemRegistry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n"
    "\t}\n\n"
    "\tSystemRegistry& registry = *sceneManager->systemRegistry;",
    "\tSystemRegistry* registry = sceneManager ? sceneManager->GetSystemRegistry() : nullptr;\n"
    "\tif(!registry) {\n"
    "\t\tImGui::TextDisabled(\"SystemRegistry is not available.\");\n"
    "\t\treturn;\n"
    "\t}\n\n"
    "\tSystemRegistry& registryRef = *registry;",
    "Schedule settings registry access",
)
text = replace_once(
    text,
    "\tScheduleProfileYamlExportUI::Draw(registry, viewState);\n"
    "\tScheduleProfilerView::Draw(registry, viewState);",
    "\tScheduleProfileYamlExportUI::Draw(registryRef, viewState);\n"
    "\tScheduleProfilerView::Draw(registryRef, viewState);",
    "Schedule registry references",
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
    "Save settings registry access",
)
if "sceneManager->systemRegistry" in text:
    raise RuntimeError("direct SystemRegistry ownership access remains in settings UI")
save(path, text)


path = "Source/GameApplication/Engine/Editor/UI/ViewWindow.cpp"
text = load(path)
text = replace_once(
    text,
    "\tRenderSystem* renderSystem = m_editor->sceneManager->systemRegistry->GetSystem<RenderSystem>();",
    "\tSystemRegistry* registry = m_editor->sceneManager->GetSystemRegistry();\n"
    "\tRenderSystem* renderSystem = registry ? registry->GetSystem<RenderSystem>() : nullptr;\n"
    "\tif(!graphicsContext || !deviceContext || !renderSystem || !renderSystem->m_EditorPass) {\n"
    "\t\treturn;\n"
    "\t}",
    "ViewWindow registry access",
)
if "sceneManager->systemRegistry" in text:
    raise RuntimeError("direct SystemRegistry ownership access remains in ViewWindow")
save(path, text)

print("SystemRegistry ownership encapsulated successfully")
