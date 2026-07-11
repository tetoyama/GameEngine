// =======================================================================
//
// CameraPostEffectRuntime.h
//
// Step 18-AでCameraComponent所有のD3D11 Texture / RTV / SRV Runtimeを撤去した。
// Runtime実装はPostEffectPass/CameraPostEffectRuntimeStorage.hが所有する。
//
// このHeaderは既存Project Itemと外部includeの互換性だけを維持する。
// CameraPostEffectへNative API操作を再追加しないこと。
//
// =======================================================================
#pragma once
