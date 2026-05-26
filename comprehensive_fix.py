#!/usr/bin/env python3
import os
import re
from pathlib import Path

# Complete list of all remaining renames needed
all_renames = {
    # Inspector.h
    "m_editorUI": "editorUI",
    "m_selectedComponent": "selectedComponent",
    "m_componentDataEditors": "componentDataEditors",
    "m_selectedComponentType": "selectedComponentType",
    
    # Hierarchy.h
    "m_rootEntity": "rootEntity",
    "m_selectedEntities": "selectedEntities",
    "m_folderStates": "folderStates",
    
    # DebugLogWindow.h (m_editor already done)
    
    # BRAIN.h (m_chatHistory not in public yet?)
    "m_chatHistory": "chatHistory",
    
    # modelData.h - more comprehensive
    "m_AnimationIndexs": "animationIndexs",
    "m_MeshDataList": "meshDataList",
    
    # audioData.h
    "m_WaveFormat": "waveFormat",
    
    # llamaModelData.h
    "m_Model": "model",
    "m_Vocab": "vocab",
    
    # shaderData.h - more comprehensive
    "m_InputLayout": "inputLayout",
    "m_DeviceContext": "deviceContext",
    "m_Device": "device",
    
    # renderingCommandInfo.h
    "m_CommandType": "commandType",
    "m_Command": "command",
    
    # EffectComponent.h
    "m_EffectData": "effectData",
    "m_Handle": "handle",
    
    # AudioComponent.h
    "m_AudioData": "audioData",
    "m_SourceVoice": "sourceVoice",
    
    # TextureComponent.h
    "m_TextureData": "textureData",
    
    # BumpMapComponent.h
    "m_BumpMapTextureData": "bumpMapTextureData",
    
    # MeshRendererComponent.h
    "m_SamplerState": "samplerState",
    "m_RasterizerState": "rasterizerState",
    
    # graphicsContext.h
    "m_CurrentSRV": "currentSRV",
    "m_CurrentRTV": "currentRTV",
    "m_DepthStencilView": "depthStencilView",
    "m_RenderTargets": "renderTargets",
    "m_PostProcessRenderTargets": "postProcessRenderTargets",
    
    # LightingPass.h
    "m_ShadowMapTexture": "shadowMapTexture",
    "m_ShadowMapRTV": "shadowMapRTV",
    "m_ShadowMapSRV": "shadowMapSRV",
    
    # renderSystem.h
    "m_graphicsContext": "graphicsContext",
    "m_renderer": "renderer",
    "m_lighting": "lighting",
    
    # D2DRenderer.h
    "m_d2dFactory": "d2dFactory",
    "m_dwriteFactory": "dwriteFactory",
    
    # mainRenderer.h
    "m_d2dRenderer": "d2dRenderer",
    "m_fullscreen": "fullscreen",
    "m_vsyncEnabled": "vsyncEnabled",
    "m_renderThread": "renderThread",
    "m_threadRunner": "threadRunner",
    
    # audioContext.h
    "m_audioContext": "audioContext",
    "m_masterVoice": "masterVoice",
    "m_sourceVoices": "sourceVoices",
    
    # ComponentCommand.h
    "m_component": "component",
    "m_componentData": "componentData",
    
    # EntityCommand.h
    "m_transform": "transform",
    "m_parent": "parent",
    "m_children": "children",
    
    # TransformChangeCommand.h
    "m_prevTransform": "prevTransform",
    "m_newTransform": "newTransform",
    
    # componentRegistry.h
    "m_components": "components",
    "m_archetypes": "archetypes",
    
    # entityRegistry.h
    "m_entities": "entities",
    "m_transforms": "transforms",
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

print(f"Processing {len(all_renames)} member renames across {len(cpp_files)} files...")
total_replacements = 0

for old_name, new_name in all_renames.items():
    count = 0
    for cpp_file in cpp_files:
        if replace_in_file(str(cpp_file), old_name, new_name):
            count += 1
    if count > 0:
        print(f"  {old_name} -> {new_name}: {count} files")
        total_replacements += count

print(f"\nTotal file changes: {total_replacements}")
print("Comprehensive refactoring complete!")
