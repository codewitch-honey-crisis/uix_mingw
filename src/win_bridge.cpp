#include "win_bridge.hpp"
#define UNICODE
/////////////////////////////////////////////////////
#include <assert.h>
#include <conio.h>
#include <d2d1.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>

using namespace gfx;
/////////////////////////////////////////////////////
#pragma comment(lib, "d2d1.lib")
/////////////////////////////////////////////////////
LARGE_INTEGER counter_freq;
static LARGE_INTEGER start_time;
static volatile DWORD frames = 0;
static volatile DWORD seconds = 0;
static HANDLE quit_event = NULL;
static HANDLE render_thread = NULL;
static HANDLE render_mutex = NULL;
static bool should_quit = false;
static ID2D1HwndRenderTarget* render_target = nullptr;
static ID2D1Factory* d2d_factory = nullptr;
static ID2D1Bitmap* render_bitmap = nullptr;

static point16 mouse_loc;
static int mouse_state = 0;  // 0 = released, 1 = pressed
static int old_mouse_state = 0;
static int mouse_req = 0;

static void update_title(HWND hwnd) {
    wchar_t wsztitle[64];
    wcscpy(wsztitle, L"UIX rendering @ ");
    DWORD f = frames;
    _itow((int)f, wsztitle + wcslen(wsztitle), 10);
    wcscat(wsztitle, L" FPS");

    if (mouse_state) {
        wcscat(wsztitle, L" (");
        f = mouse_loc.x;
        _itow((int)f, wsztitle + wcslen(wsztitle), 10);
        wcscat(wsztitle, L", ");
        f = mouse_loc.y;
        _itow((int)f, wsztitle + wcslen(wsztitle), 10);
        wcscat(wsztitle, L")");
    }
    SetWindowTextW(hwnd, wsztitle);
}

/////////////////////////////////////////////////////
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_SIZE) {
        if (render_target) {
            D2D1_SIZE_U size = D2D1::SizeU(LOWORD(lParam), HIWORD(lParam));
            render_target->Resize(size);
        }
    }
    if (uMsg == WM_CLOSE) {
        SetEvent(quit_event);
        should_quit = true;
    }
    /*if(uMsg == WM_LBUTTONDOWN) {
        if(WAIT_OBJECT_0 == WaitForSingleObject(
            render_mutex,    // handle to mutex
            INFINITE)) {  // no time-out interval)

            old_mouse_state = mouse_state;
            mouse_state = 1;
            mouse_loc.x = (uint16_t)LOWORD(lParam);
            mouse_loc.y = (uint16_t)HIWORD(lParam);
            mouse_req = 1;
            ReleaseMutex(render_mutex);
        }
        update_title(hWnd);
    }
    if(uMsg == WM_MOUSEMOVE) {
        if(WAIT_OBJECT_0 == WaitForSingleObject(
            render_mutex,    // handle to mutex
            INFINITE)) {  // no time-out interval)

            if(mouse_state==1 && MK_LBUTTON==wParam) {
                mouse_req = 1;
                mouse_loc.x = (uint16_t)LOWORD(lParam);
                mouse_loc.y = (uint16_t)HIWORD(lParam);

            }
            ReleaseMutex(render_mutex);
        }
        //update_title(hWnd);
    }
    if(uMsg == WM_LBUTTONUP) {
        if(WAIT_OBJECT_0 == WaitForSingleObject(
            render_mutex,    // handle to mutex
            INFINITE)) {  // no time-out interval)


            old_mouse_state = mouse_state;
            mouse_req = 1;
            mouse_state = 0;
            mouse_loc.x = (uint16_t)LOWORD(lParam);
            mouse_loc.y = (uint16_t)HIWORD(lParam);
            ReleaseMutex(render_mutex);
        }
        //update_title(hWnd);
    } */

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
DWORD thread_proc(void* state) {
    bool quit = false;
    while (!quit) {
        loop();
        if (render_target && render_bitmap) {
            if (WAIT_OBJECT_0 == WaitForSingleObject(
                                     render_mutex,  // handle to mutex
                                     INFINITE)) {   // no time-out interval)

                render_target->BeginDraw();
                D2D1_RECT_F rect_dest = {
                    0,
                    0,
                    (float)screen_size.width,
                    (float)screen_size.height};
                render_target->DrawBitmap(render_bitmap,
                                          rect_dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, NULL);
                render_target->EndDraw();
                ReleaseMutex(render_mutex);
                InterlockedIncrement(&frames);
            }
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            quit = true;
        }
    }
    return 0;
}
bool read_mouse(point16* out_location) {
    if (WAIT_OBJECT_0 == WaitForSingleObject(
                             render_mutex,  // handle to mutex
                             INFINITE)) {   // no time-out interval)

        if (mouse_req) {
            if (mouse_state) {
                *out_location = mouse_loc;
            }
            mouse_req = 0;
            ReleaseMutex(render_mutex);
            return true;
        }
        ReleaseMutex(render_mutex);
    }
    return false;
}
void flush_bitmap(const rect16& bounds, const void* bmp) {
    if (render_bitmap != NULL) {
        D2D1_RECT_U b;
        b.top = bounds.y1;
        b.left = bounds.x1;
        b.bottom = bounds.y2;
        b.right = bounds.x2;
        render_bitmap->CopyFromMemory(&b, bmp, bounds.width() * 4);
    }
}
uint32_t millis() {
    LARGE_INTEGER end_time;
    QueryPerformanceCounter(&end_time);
    return (uint32_t)((double)(end_time.QuadPart - start_time.QuadPart) / counter_freq.QuadPart) * 1000;
}
/////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
    // CoInitializeEx(0, COINIT_MULTITHREADED);
    CoInitialize(0);
    QueryPerformanceFrequency(&counter_freq);
    QueryPerformanceCounter(&start_time);

    WNDCLASSW wc;
    HINSTANCE hInstance = GetModuleHandle(NULL);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"UIX";

    RegisterClassW(&wc);

    HRESULT hr = S_OK;
    RECT r = {0, 0, screen_size.width - 1, screen_size.height - 1};

    AdjustWindowRectEx(&r, WS_CAPTION | WS_SYSMENU | WS_BORDER, FALSE, WS_EX_APPWINDOW);
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, L"UIX", L"UIX",
        WS_CAPTION | WS_SYSMENU,
        0, 0,
        r.right + r.left + 1,
        r.bottom - r.top + 1,
        NULL, NULL, hInstance, NULL);
    assert(IsWindow(hwnd));
    if (!IsWindow(hwnd)) goto exit;
    quit_event = CreateEvent(
        NULL,              // default security attributes
        TRUE,              // manual-reset event
        FALSE,             // initial state is nonsignaled
        TEXT("QuitEvent")  // object name
    );
    if (quit_event == NULL) {
        goto exit;
    }
    render_mutex = CreateMutex(NULL, FALSE, NULL);
    if (render_mutex == NULL) {
        goto exit;
    }
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
    assert(hr == S_OK);
    if (hr != S_OK) goto exit;
    {
        setup();
    }
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top);

        hr = d2d_factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &render_target);
        assert(hr == S_OK);
        if (hr != S_OK) goto exit;
    }
    {
        D2D1_SIZE_U size = {0};
        D2D1_BITMAP_PROPERTIES props;
        render_target->GetDpi(&props.dpiX, &props.dpiY);
        D2D1_PIXEL_FORMAT pixelFormat = D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_IGNORE);
        props.pixelFormat = pixelFormat;
        size.width = screen_size.width;
        size.height = screen_size.height;

        hr = render_target->CreateBitmap(size,
                                         props,
                                         &render_bitmap);
        assert(hr == S_OK);
        if (hr != S_OK) goto exit;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    SetTimer(hwnd, 0, 1000, NULL);
    render_thread = CreateThread(NULL, 4000 * 4, thread_proc, NULL, 0, NULL);
    if (render_thread == NULL) {
        goto exit;
    }

    while (!should_quit) {
        DWORD result = 0;

        MSG msg = {0};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                should_quit = true;
                break;
            }
            if (msg.message == WM_TIMER) {
                update_title(hwnd);
                InterlockedExchange(&frames, 0);
            }
            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_ESCAPE) {
                    should_quit = true;
                    break;
                }
            }

            if (msg.message == WM_LBUTTONDOWN) {
                SetCapture(hwnd);
                
                if (WAIT_OBJECT_0 == WaitForSingleObject(
                                         render_mutex,  // handle to mutex
                                         INFINITE)) {   // no time-out interval)

                    old_mouse_state = mouse_state;
                    mouse_state = 1;
                    mouse_loc.x = (uint16_t)LOWORD(msg.lParam);
                    mouse_loc.y = (uint16_t)HIWORD(msg.lParam);
                    mouse_req = 1;
                    ReleaseMutex(render_mutex);
                }
                update_title(hwnd);
            }
            if (msg.message == WM_MOUSEMOVE) {
                if (WAIT_OBJECT_0 == WaitForSingleObject(
                                         render_mutex,  // handle to mutex
                                         INFINITE)) {   // no time-out interval)

                    if (mouse_state == 1 && MK_LBUTTON == msg.wParam) {
                        mouse_req = 1;
                        mouse_loc.x = (uint16_t)LOWORD(msg.lParam);
                        mouse_loc.y = (uint16_t)HIWORD(msg.lParam);
                    }
                    ReleaseMutex(render_mutex);
                }
                update_title(hwnd);
            }
            if (msg.message == WM_LBUTTONUP) {
                ReleaseCapture();
                if (WAIT_OBJECT_0 == WaitForSingleObject(
                                         render_mutex,  // handle to mutex
                                         INFINITE)) {   // no time-out interval)

                    old_mouse_state = mouse_state;
                    mouse_req = 1;
                    mouse_state = 0;
                    mouse_loc.x = (uint16_t)LOWORD(msg.lParam);
                    mouse_loc.y = (uint16_t)HIWORD(msg.lParam);
                    ReleaseMutex(render_mutex);
                }
                // update_title(hWnd);
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(quit_event, 0)) {
            should_quit = true;
        }
    }
exit:
    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }
    if (render_thread != NULL) {
        CloseHandle(render_thread);
    }
    if (quit_event != NULL) {
        CloseHandle(quit_event);
    }
    if (render_mutex != NULL) {
        CloseHandle(render_mutex);
    }
    // SetEvent(g_hReady);
    // SetConsoleCtrlHandler(ConsoleHandlerRoutine, FALSE);

    render_target->Release();
    render_bitmap->Release();
    d2d_factory->Release();
    CoUninitialize();

    // CloseHandle(g_hQuit);
    // CloseHandle(g_hReady);
}
/////////////////////////////////////////////////////