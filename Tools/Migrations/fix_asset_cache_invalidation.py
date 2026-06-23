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


header = "Source/GameApplication/Engine/Editor/UI/AssetsBrowser.h"
text = load(header)
text = replace_once(
    text,
    """\tstd::unordered_map<std::string, CachedEntryList> m_directoryCache;
\tstd::unordered_map<std::string, CachedEntryList> m_assetCache;
""",
    """\tstd::unordered_map<std::string, CachedEntryList> m_directoryCache;
\tstd::unordered_map<std::string, CachedEntryList> m_assetCache;
\tbool m_fileSystemCacheInvalidationPending = true;
""",
    "asset cache pending member",
)
save(header, text)

cpp = "Source/GameApplication/Engine/Editor/UI/AssetsBrowser.cpp"
text = load(cpp)
text = replace_once(
    text,
    """void AssetsBrowser::InvalidateFileSystemCache(){
\tm_directoryCache.clear();
\tm_assetCache.clear();
}
""",
    """void AssetsBrowser::InvalidateFileSystemCache(){
\t// Context menu操作中はキャッシュ配列を走査中の場合があるため、
\t// 実際の破棄は次のDraw開始時まで遅延する。
\tm_fileSystemCacheInvalidationPending = true;
}
""",
    "deferred invalidation function",
)
text = replace_once(
    text,
    """\tif(!ImGui::Begin(\"Assets Browser\", showAssetsBrowser, 0)){
\t\tImGui::End();
\t\treturn;
\t}

\tif(ImGui::Button(\"Refresh\")){
""",
    """\tif(!ImGui::Begin(\"Assets Browser\", showAssetsBrowser, 0)){
\t\tImGui::End();
\t\treturn;
\t}

\tif(m_fileSystemCacheInvalidationPending){
\t\tm_directoryCache.clear();
\t\tm_assetCache.clear();
\t\tm_fileSystemCacheInvalidationPending = false;
\t}

\tif(ImGui::Button(\"Refresh\")){
""",
    "apply deferred invalidation",
)
text = replace_once(
    text,
    """\t\tImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
\t\t\tImGuiTreeNodeFlags_SpanAvailWidth |
\t\t\tImGuiTreeNodeFlags_DefaultOpen;
""",
    """\t\t// Nested folders are lazy-opened. DefaultOpen here would enumerate the
\t\t// complete Asset tree on the first frame and create a startup spike.
\t\tImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
\t\t\tImGuiTreeNodeFlags_SpanAvailWidth;
""",
    "lazy asset tree",
)
save(cpp, text)
