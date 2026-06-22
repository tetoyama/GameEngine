from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8-sig")


def save(relative_path: str, text: str) -> None:
    (ROOT / relative_path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)
