#!/usr/bin/env python3
import os
import re
from pathlib import Path

# GameApplication public members that need fixing
game_renames = {
    # ViewWindow.h
    "m_cameraMoveSpeed": "cameraMoveSpeed",
    "m_mouseOnEditor": "mouseOnEditor",
    
    # InputSystem.h
    "m_inFocus": "inFocus",
    "m_lastX": "lastX",
    "m_lastY": "lastY",
    "m_relativeX": "relativeX",
    "m_relativeY": "relativeY",
    "m_window": "window",
    
    # LLAMAService.h
    "m_agentMutex": "agentMutex",
    "m_completedMutex": "completedMutex",
    "m_jobCV": "jobCV",
    "m_jobMutex": "jobMutex",
    "m_modelMutex": "modelMutex",
    "m_scrollToBottom": "scrollToBottom",
    "m_workerThread": "workerThread",
    
    # llamaModelData.h
    "m_path": "path",
}

def replace_in_file(filepath, old_name, new_name):
    """Replace member variable name in a file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        original_content = content
        
        # Replace patterns like this->m_name or obj.m_name or ptr->m_name
        # Pattern 1: this->m_name
        content = re.sub(r'this->(' + re.escape(old_name) + r')\b', f'this->{new_name}', content)
        # Pattern 2: \.m_name (member access through object)
        content = re.sub(r'\.(' + re.escape(old_name) + r')\b', f'.{new_name}', content)
        # Pattern 3: ->m_name (member access through pointer)
        content = re.sub(r'->(' + re.escape(old_name) + r')\b', f'->{new_name}', content)
        # Pattern 4: member variable in constructor initializer lists
        content = re.sub(r',\s*(' + re.escape(old_name) + r')\(', f', {new_name}(', content)
        # Pattern 5: assignment patterns
        content = re.sub(r'(' + re.escape(old_name) + r')\s*=\s*', f'{new_name} = ', content)
        # Pattern 6: member variable declarations (type varname =)
        content = re.sub(r'\b(' + re.escape(old_name) + r')\b(?=\s*[=;,)\]])', new_name, content)
        
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    except Exception as e:
        return False

# Find all C++ files
root_dir = "/home/runner/work/GameEngine/GameEngine"
cpp_files = list(Path(root_dir).rglob("*.cpp")) + list(Path(root_dir).rglob("*.h"))

print(f"Processing {len(game_renames)} GameApplication member renames...")
total_replacements = 0

for old_name, new_name in game_renames.items():
    count = 0
    for cpp_file in cpp_files:
        if replace_in_file(str(cpp_file), old_name, new_name):
            count += 1
    if count > 0:
        print(f"  {old_name} -> {new_name}: {count} files")
        total_replacements += count

print(f"\nTotal file changes: {total_replacements}")
print("GameApplication refactoring complete!")
