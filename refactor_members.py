#!/usr/bin/env python3
import os
import re
import subprocess
from pathlib import Path

# Dictionary of member variable renames (old_name -> new_name)
renames = {
    # MenuBar.h
    "m_showSceneHierarchy": "showSceneHierarchy",
    "m_showInspector": "showInspector",
    "m_showConsole": "showConsole",
    "m_showAssetsBrowser": "showAssetsBrowser",
    "m_showBRAIN": "showBRAIN",
    "m_showEditorView": "showEditorView",
    "m_showPlayerView": "showPlayerView",
    "m_showPerformanceMonitor": "showPerformanceMonitor",
    "m_showSystemSetting": "showSystemSetting",
    "m_showCB41": "showCB41",
    
    # ViewWindow.h
    "m_EditorCameraPosition": "editorCameraPosition",
    "m_editorCameraRotation": "editorCameraRotation",
    "m_MouseWheel": "mouseWheel",
    "m_wasUsingGizmo": "wasUsingGizmo",
    "m_gizmoEntity": "gizmoEntity",
    "m_gizmoStartState": "gizmoStartState",
    
    # Inspector.h
    "m_selectedComponent": "selectedComponent",
    "m_componentDataEditors": "componentDataEditors",
    "m_selectedComponentType": "selectedComponentType",
    
    # Hierarchy.h
    "m_rootEntity": "rootEntity",
    "m_selectedEntities": "selectedEntities",
    "m_folderStates": "folderStates",
    
    # AssetsBrowser.h
    "m_openRename": "openRename",
    "m_renameTarget": "renameTarget",
    "m_newNameBuffer": "newNameBuffer",
    
    # DebugLogWindow.h
    "m_autoScroll": "autoScroll",
    
    # BRAIN.h
    "m_chatHistory": "chatHistory",
    
    # modelData.h
    "m_Texture": "texture",
    "m_Bones": "bones",
    "m_Animation": "animation",
    "m_AnimationIndexs": "animationIndexs",
    "m_MeshDataList": "meshDataList",
    
    # audioData.h
    "m_SoundData": "soundData",
    "m_Length": "length",
    "m_PlayLength": "playLength",
    "m_Format": "format",
    "m_WaveFormat": "waveFormat",
    
    # llamaModelData.h
    "m_Model": "model",
    "m_Vocab": "vocab",
    
    # shaderData.h
    "m_VertexShader": "vertexShader",
    "m_PixelShader": "pixelShader",
    "m_InputLayout": "inputLayout",
    "m_DeviceContext": "deviceContext",
    "m_Device": "device",
    "m_Buffer": "buffer",
    
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
    "m_VertexBuffer": "vertexBuffer",
    "m_SamplerState": "samplerState",
    "m_RasterizerState": "rasterizerState",
    
    # graphicsContext.h
    "m_width": "width",
    "m_height": "height",
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
    
    # ArchetypeStorage.h
}

def replace_in_file(filepath, old_name, new_name):
    """Replace member variable name in a file"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Replace patterns like this->m_name or obj.m_name or ptr->m_name
        # We need to be careful to only replace member accesses
        original_content = content
        
        # Pattern 1: this->m_name
        content = re.sub(r'this->(' + re.escape(old_name) + r')\b', f'this->{new_name}', content)
        # Pattern 2: \.m_name (member access through object)
        content = re.sub(r'\.(' + re.escape(old_name) + r')\b', f'.{new_name}', content)
        # Pattern 3: ->m_name (member access through pointer)
        content = re.sub(r'->(' + re.escape(old_name) + r')\b', f'->{new_name}', content)
        # Pattern 4: declaration or assignment (e.g., in constructors)
        content = re.sub(r'\b(' + re.escape(old_name) + r')\s*=', f'{new_name}=', content)
        # Pattern 5: member variable declarations
        content = re.sub(r'\b(' + re.escape(old_name) + r')\b(?=[,;)\]])' , new_name, content)
        
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return False

# Find all C++ files
root_dir = "/home/runner/work/GameEngine/GameEngine"
cpp_files = list(Path(root_dir).rglob("*.cpp")) + list(Path(root_dir).rglob("*.h"))

print(f"Found {len(cpp_files)} C++ files")
print("Starting refactoring...")

for old_name, new_name in renames.items():
    count = 0
    for cpp_file in cpp_files:
        if replace_in_file(str(cpp_file), old_name, new_name):
            count += 1
    if count > 0:
        print(f"Renamed {old_name} -> {new_name} in {count} files")

print("Refactoring complete!")
