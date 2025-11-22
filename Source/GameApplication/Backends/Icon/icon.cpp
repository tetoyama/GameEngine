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

HRESULT SetIcon(const HWND hWnd, const std::wstring& FileName){
	if(g_hIcon){
		DestroyIcon(g_hIcon);
		g_hIcon = nullptr;
	}

	IWICImagingFactory* pFactory = nullptr;
	IWICBitmapDecoder* pDecoder = nullptr;
	IWICBitmapFrameDecode* pFrame = nullptr;
	IWICFormatConverter* pConverter = nullptr;

	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pFactory)
	);
	if(FAILED(hr)) goto Cleanup;

	hr = pFactory->CreateDecoderFromFilename(
		FileName.c_str(), nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnDemand, &pDecoder
	);
	if(FAILED(hr)) goto Cleanup;

	hr = pDecoder->GetFrame(0, &pFrame);
	if(FAILED(hr)) goto Cleanup;

	hr = pFactory->CreateFormatConverter(&pConverter);
	if(FAILED(hr)) goto Cleanup;

	hr = pConverter->Initialize(
		pFrame, GUID_WICPixelFormat32bppBGRA,
		WICBitmapDitherTypeNone, nullptr, 0.f,
		WICBitmapPaletteTypeCustom
	);
	if(FAILED(hr)) goto Cleanup;

	g_hIcon = CreateIconFromWicBitmap(pConverter);
	if(!g_hIcon){
		hr = E_FAIL;
		goto Cleanup;
	}

	SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
	SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);
#ifdef ICON_SMALL2
	SendMessageW(hWnd, WM_SETICON, ICON_SMALL2, (LPARAM)g_hIcon);
#endif

Cleanup:
	if(pConverter) pConverter->Release();
	if(pFrame)     pFrame->Release();
	if(pDecoder)   pDecoder->Release();
	if(pFactory)   pFactory->Release();

	return hr;
}

void UninitIcon() {
    if (g_hIcon) {
        DestroyIcon(g_hIcon);
    }
}
