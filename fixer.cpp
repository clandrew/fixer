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

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void OnTick();

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
    unsigned long long int ticksPerFrame = qpf.QuadPart / 60;
    unsigned long long int ticksLastFrame = 0;
    unsigned long long int target = ticksLastFrame + ticksPerFrame;

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

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }


   D2D1_FACTORY_OPTIONS factoryOptions = {};
   factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
   if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &factoryOptions, &g_d2dFactory)))
   {
       return FALSE;
   }

   RECT clientRect;
   if (!GetClientRect(hWnd, &clientRect))
   {
       return FALSE;
   }
   int clientWidth = clientRect.right - clientRect.left;
   int clientHeight = clientRect.bottom - clientRect.top;

   D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(
       D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0);

   D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTargetProperties = D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(clientWidth, clientHeight));

   if (FAILED(g_d2dFactory->CreateHwndRenderTarget(&renderTargetProperties, &hwndRenderTargetProperties, &g_renderTarget)))
   {
       return FALSE;
   }

   CoInitialize(nullptr);

   EnsureWicImagingFactory();

   g_demoImage = TryLoadAsRaster(L"local/reference.png");

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

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

void OnTick()
{
}