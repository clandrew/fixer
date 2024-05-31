// fixer.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "framework.h"
#include "fixer.h"

using namespace Microsoft::WRL;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

struct LoadedImageFile
{
    D2D1_SIZE_U Size;
    std::vector<UINT> DestBuffer;
    ComPtr<ID2D1Bitmap> Bitmap;
};

ComPtr<ID2D1Factory7> g_d2dFactory;
ComPtr<ID2D1HwndRenderTarget> g_renderTarget;
ComPtr<IWICImagingFactory> g_wicImagingFactory;
LoadedImageFile g_demoImage;
bool g_loaded{};
bool g_isRunning{};
HANDLE hData = NULL;  // handle of waveform data memory 

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void OnTick();
void InitSound();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_FIXER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FIXER));

    LARGE_INTEGER qpf;
    QueryPerformanceFrequency(&qpf);
    long long int ticksPerFrame = qpf.QuadPart / 60;
    long long int ticksLastFrame = 0;
    long long int target = ticksLastFrame + ticksPerFrame;

    g_loaded = true;

    // Main message loop:
    g_isRunning = true;
    MSG msg;
    while (g_isRunning)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        if (qpc.QuadPart >= target)
        {
            // Draw
            OnTick();
            target = qpc.QuadPart + ticksPerFrame;
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FIXER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_FIXER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void EnsureWicImagingFactory()
{
    if (g_wicImagingFactory)
        return;

    if (FAILED(CoCreateInstance(
        CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,
        (LPVOID*)&g_wicImagingFactory)))
    {
        return;
    }
}

LoadedImageFile TryLoadAsRaster(std::wstring fileName)
{
    LoadedImageFile r{};

    ComPtr<IWICBitmapDecoder> spDecoder;
    if (FAILED(g_wicImagingFactory->CreateDecoderFromFilename(
        fileName.c_str(),
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &spDecoder)))
    {
        return r;
    }

    ComPtr<IWICBitmapFrameDecode> spSource;
    if (FAILED(spDecoder->GetFrame(0, &spSource)))
    {
        return r;
    }

    // Convert the image format to 32bppPBGRA, equiv to DXGI_FORMAT_B8G8R8A8_UNORM
    ComPtr<IWICFormatConverter> spConverter;
    if (FAILED(g_wicImagingFactory->CreateFormatConverter(&spConverter)))
    {
        return r;
    }

    if (FAILED(spConverter->Initialize(
        spSource.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0.f,
        WICBitmapPaletteTypeMedianCut)))
    {
        return r;
    }

    if (FAILED(spConverter->GetSize(&r.Size.width, &r.Size.height)))
    {
        return r;
    }

    r.DestBuffer.resize(r.Size.width * r.Size.height);
    if (FAILED(spConverter->CopyPixels(
        NULL,
        r.Size.width * sizeof(UINT),
        static_cast<UINT>(r.DestBuffer.size()) * sizeof(UINT),
        (BYTE*)&r.DestBuffer[0])))
    {
        return r;
    }

    D2D1_BITMAP_PROPERTIES bitmapProperties = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(g_renderTarget->CreateBitmap(
        r.Size,
        &r.DestBuffer[0],
        r.Size.width * sizeof(UINT),
        bitmapProperties,
        &r.Bitmap)))
    {
        return r;
    }

    return r;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   RECT clientRect;
   if (!GetClientRect(hWnd, &clientRect))
   {
       return FALSE;
   }

   int scalingFactor = 3;

   // Resize the window
   RECT desiredClientRect{};
   desiredClientRect.left = 100;
   desiredClientRect.top = 100;
   desiredClientRect.right = desiredClientRect.left + (256 * scalingFactor);
   desiredClientRect.bottom = desiredClientRect.top + (240 * scalingFactor);
   AdjustWindowRect(&desiredClientRect, WS_CHILD | WS_OVERLAPPEDWINDOW, TRUE); // Menu bar

   int width = clientRect.right - clientRect.left;
   int height = clientRect.bottom - clientRect.top;
   height += GetSystemMetrics(SM_CYMENU);

   MoveWindow(hWnd, 
       desiredClientRect.left,
       desiredClientRect.top,
       desiredClientRect.right - desiredClientRect.left,
       desiredClientRect.bottom - desiredClientRect.top, FALSE);

   // Check client area
   if (!GetClientRect(hWnd, &clientRect))
   {
       return FALSE;
   }

   D2D1_FACTORY_OPTIONS factoryOptions = {};
   factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
   if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &factoryOptions, &g_d2dFactory)))
   {
       return FALSE;
   }

   D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(
       D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0);

   D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTargetProperties = D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(256, 240));

   if (FAILED(g_d2dFactory->CreateHwndRenderTarget(&renderTargetProperties, &hwndRenderTargetProperties, &g_renderTarget)))
   {
       return FALSE;
   }

   (void)CoInitialize(nullptr);

   EnsureWicImagingFactory();

   g_demoImage = TryLoadAsRaster(L"local/reference.png");

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   InitSound();

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            if (!g_loaded)
            {
                break;
            }

            g_renderTarget->BeginDraw();
            g_renderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            g_renderTarget->Clear(D2D1::ColorF(0.5f, 0.5f, 0.5f, 1.0f));
            g_renderTarget->DrawBitmap(g_demoImage.Bitmap.Get(), nullptr, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, nullptr);
            g_renderTarget->EndDraw();
        }
        break;
    case WM_DESTROY:
        g_isRunning = false;
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Sound needs its own thread.

DWORD WINAPI SoundThreadProc(LPVOID)
{
    HWAVEOUT    hWaveOut{};
    HGLOBAL     hWaveHdr{};
    LPWAVEHDR   lpWaveHdr{};
    HMMIO       hmmio{};
    UINT        wResult{};
    HANDLE      hFormat{};
    WAVEFORMAT* pFormat{};
    DWORD       dwDataSize{};

    HWND hwndApp = nullptr;


    int samplesPerSecond = 111320;

    WAVEFORMATEX wfx =
    {
        WAVE_FORMAT_PCM,  // wFormatTag
        1,                // nChannels
        samplesPerSecond,             // nSamplesPerSec
        samplesPerSecond,             // nAvgBytesPerSec
        1,                // nBlockAlign
        8,                // wBitsPerSample
        0                 // cbSize
    };

    HANDLE soundDonePlaying = CreateEvent(0, FALSE, FALSE, 0);

    if (waveOutOpen(
        (LPHWAVEOUT)&hWaveOut,
        WAVE_MAPPER,
        (LPWAVEFORMATEX)&wfx,
        (LONG)soundDonePlaying,
        0L, 
        CALLBACK_EVENT))
    {
        MessageBoxA(hwndApp,
            "Failed to open waveform output device.",
            NULL, MB_OK | MB_ICONEXCLAMATION);
        LocalUnlock(hFormat);
        LocalFree(hFormat);
        mmioClose(hmmio, 0);
        return 0;
    }

    std::vector<int> notes;
    for (int i = 0; i < 2; ++i)
    {
        notes.push_back(424);
        notes.push_back(282);
        notes.push_back(212);
        notes.push_back(142);

        notes.push_back(424);
        notes.push_back(302);
        notes.push_back(212);
        notes.push_back(152);

        notes.push_back(424);
        notes.push_back(282);
        notes.push_back(212);
        notes.push_back(142);

        notes.push_back(404);
        notes.push_back(262);
        notes.push_back(202);
        notes.push_back(132);
    }
    for (int i = 0; i < 2; ++i)
    {
        notes.push_back(382);
        notes.push_back(252);
        notes.push_back(192);
        notes.push_back(126);

        notes.push_back(382);
        notes.push_back(272);
        notes.push_back(192);
        notes.push_back(126);

        notes.push_back(382);
        notes.push_back(252);
        notes.push_back(192);
        notes.push_back(126);

        notes.push_back(362);
        notes.push_back(238);
        notes.push_back(182);
        notes.push_back(120);

    }
    for (int i = 0; i < 4; ++i)
    {
        notes.push_back(342);
        notes.push_back(222);
        notes.push_back(168);
        notes.push_back(112);

        notes.push_back(342);
        notes.push_back(238);
        notes.push_back(168);
        notes.push_back(118);

        notes.push_back(342);
        notes.push_back(248);
        notes.push_back(168);
        notes.push_back(126);

        notes.push_back(342);
        notes.push_back(238);
        notes.push_back(168);
        notes.push_back(118);
    }

    std::vector<char> buffer;
    int noteLength = 22000;
    int amp = 5;
    for (int i = 0; i < notes.size(); ++i)
    {
        int note = notes[i];

        for (int j = 0; j < noteLength; ++j)
        {
            byte b;

            int v = j / note; // wave-peak length
            if (v % 2 == 0)
            {
                b = 128 + amp;
            }
            else
            {
                b = 128 - amp;
            }

            buffer.push_back(b);
        }
    }

    while (true)
    {
        ResetEvent(soundDonePlaying);

        WAVEHDR header = { buffer.data(), buffer.size(), 0, 0, 0, 0, 0, 0 };
        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        if (WaitForSingleObject(soundDonePlaying, INFINITE) != WAIT_OBJECT_0)
        {
            return 0; // Error
        }
    }

    return 0;
}

void InitSound(void)
{
    CreateThread(nullptr, 0, SoundThreadProc, nullptr, 0, nullptr);
}

void OnTick()
{
}