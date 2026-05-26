#!/usr/bin/env python3
import os
import re
import subprocess
from pathlib import Path

# Additional renames that need special handling
additional_renames = {
    # From MenuBar.h public members - need to handle the m_editor that was in public before
    # From Inspector.h
    "m_editorUI": "editorUI",
    "m_entity": "entity",
    
    # From Hierarchy.h
    # From DebugLogWindow.h
    "m_editor": "editor",
    
    # From BRAIN.h - already has m_editor
    # From modelData.h
    "m_AnimationIndexs": "animationIndexs",
    "m_MeshDataList": "meshDataList",
    
    # From audioData.h
    "m_WaveFormat": "waveFormat",
    
    # From llamaModelData.h
    # From shaderData.h
    # From renderingCommandInfo.h
    # From Component files
    # From graphicsContext.h
    # From LightingPass.h
    # From renderSystem.h
    # From D2DRenderer.h
    # From mainRenderer.h
    # From audioContext.h
    # From Command files
    # From componentRegistry.h
    # From entityRegistry.h
}

def fix_spacing_and_replace(filepath, old_name, new_name):
    """Fix spacing issues in replacements"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # Find and fix spacing issues where replacement removed space
        # Look for patterns like bool varname= and change to bool varname =
        content = re.sub(r'(\b(bool|int|float|double|char|void|auto|std::[a-zA-Z_][a-zA-Z0-9_:]*)\s+' + re.escape(new_name) + r')=\s*', r'\1 = ', content)
        # General: any word boundary followed by = without space
        content = re.sub(r'(\b' + re.escape(new_name) + r')=\s*', r'\1 = ', content)
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    except Exception as e:
        print(f"Error fixing spacing in {filepath}: {e}")
        return False

# Find all C++ files
root_dir = "/home/runner/work/GameEngine/GameEngine"
cpp_files = list(Path(root_dir).rglob("*.cpp")) + list(Path(root_dir).rglob("*.h"))

# First, fix spacing issues in the already replaced names
already_replaced = [
    "showSceneHierarchy", "showInspector", "showConsole", "showAssetsBrowser",
    "showBRAIN", "showEditorView", "showPlayerView", "showPerformanceMonitor",
    "showSystemSetting", "showCB41", "editorCameraPosition", "editorCameraRotation",
    "mouseWheel", "wasUsingGizmo", "gizmoEntity", "gizmoStartState",
    "openRename", "renameTarget", "newNameBuffer", "autoScroll",
    "texture", "bones", "animation", "soundData", "length", "playLength",
    "format", "vertexShader", "pixelShader", "inputLayout", "deviceContext",
    "device", "buffer"
]

print("Fixing spacing issues...")
for var_name in already_replaced:
    count = 0
    for cpp_file in cpp_files:
        if fix_spacing_and_replace(str(cpp_file), "", var_name):
            count += 1
    # Don't print per-variable since it's just fixing spacing

print("Spacing fixes complete!")

# Now handle the additional renames
def replace_in_file_v2(filepath, old_name, new_name):
    """Replace member variable name in a file - improved version"""
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
        content = re.sub(r'(' + re.escape(old_name) + r')\(', f'{new_name}(', content)
        content = re.sub(r'(' + re.escape(old_name) + r'\s*=)', f'{new_name} =', content)
        
        # Pattern 5: member variable declarations
        content = re.sub(r'\b(' + re.escape(old_name) + r')\b(?=\s*[=;,)\]])', new_name, content)
        
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    except Exception as e:
        return False

print("Replacing additional members...")
for old_name, new_name in additional_renames.items():
    count = 0
    for cpp_file in cpp_files:
        if replace_in_file_v2(str(cpp_file), old_name, new_name):
            count += 1
    if count > 0:
        print(f"Renamed {old_name} -> {new_name} in {count} files")

print("Additional replacements complete!")
