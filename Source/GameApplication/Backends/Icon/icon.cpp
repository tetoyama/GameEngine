#include "icon.h"

#include <memory>
#include <windows.h>
#include <wincodec.h>
#include <commdlg.h>

static HICON g_hIcon = nullptr;

static HICON CreateIconFromWicBitmap(IWICBitmapSource* pBitmapSource) {
    if (!pBitmapSource) { return nullptr; }

    UINT width = 0, height = 0;

    HRESULT hr = pBitmapSource->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) { return nullptr; }

    const int cbStride = width * 4;
    const int cbBufferSize = cbStride * height;

    std::unique_ptr<BYTE[]> pixels(new BYTE[cbBufferSize]);
    hr = pBitmapSource->CopyPixels(nullptr, cbStride, cbBufferSize, pixels.get());
    if (FAILED(hr)) { return nullptr; }

    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = width;
    bi.bV5Height = -static_cast<LONG>(height);  // Top-down DIB
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* lpBits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hBitmap = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &lpBits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hBitmap) { return nullptr; }

    memcpy(lpBits, pixels.get(), cbBufferSize);

    HBITMAP hMonoMask = CreateBitmap(width, height, 1, 1, nullptr);

    ICONINFO iconInfo{};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = hBitmap;
    iconInfo.hbmMask = hMonoMask;

    HICON hIcon = CreateIconIndirect(&iconInfo);

    DeleteObject(hBitmap);
    DeleteObject(hMonoMask);

    return hIcon;
}


HRESULT InitIcon(const HWND hWnd) {
    g_hIcon = nullptr;
    return SetIcon(hWnd, DEFAULT_ICON);
}

HRESULT SetIcon(const HWND hWnd, const std::wstring& FileName) {

    if (g_hIcon) {
        DestroyIcon(g_hIcon);
    }

    IWICImagingFactory* pWICFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWICFactory));
    if (FAILED(hr)) {
        return hr;
    }

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pWICFactory->CreateDecoderFromFilename(FileName.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        pWICFactory->Release();
        return hr;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) {
        pDecoder->Release();
        pWICFactory->Release();
        return hr;
    }

    // フォーマットの変換
    IWICFormatConverter* pConverter = nullptr;
    hr = pWICFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) {
        pFrame->Release();
        pDecoder->Release();
        pWICFactory->Release();
        return hr;
    }

    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pWICFactory->Release();
        return hr;
    }

    // アイコンの作成
    g_hIcon = nullptr;
    g_hIcon = CreateIconFromWicBitmap(pConverter);
    if (!g_hIcon) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pWICFactory->Release();
        return E_FAIL;
    }

    // アイコンの設定
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);

    // リソースの解放
    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pWICFactory->Release();

    return S_OK;
}

void UninitIcon() {
    if (g_hIcon) {
        DestroyIcon(g_hIcon);
    }
}
