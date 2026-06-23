from pathlib import Path

path = Path("Source/GameApplication/Engine/Editor/UI/AssetsBrowser.cpp")
text = path.read_text(encoding="utf-8-sig")
old = "\tconst CachedEntryList& directories = GetCachedDirectories(directory);\n"
new = (
    "\t// Recursive expansion may insert another cache entry and rehash the map.\n"
    "\t// Copy cached metadata so the current traversal remains valid.\n"
    "\tconst CachedEntryList directories = GetCachedDirectories(directory);\n"
)
if text.count(old) != 1:
    raise RuntimeError(f"expected one directory cache reference, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
