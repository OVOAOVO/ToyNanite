#include <Windows.h>
#include <stdio.h>
#include "Vulkan.h"
#include "Scene.h"
#if _DEBUG
#pragma comment( linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#else
#endif
#pragma comment(lib, "winmm.lib")
//handle -> ptr
LRESULT CALLBACK VulkanRenderWindowProc(HWND inHWND, UINT inMessage,
	WPARAM inWParam, LPARAM inLParam) {
	switch (inMessage)
	{
	case WM_CLOSE:
		PostQuitMessage(0);//WM_QUIT
		break;
	}
	return DefWindowProc(inHWND, inMessage, inWParam, inLParam);
}
INT WINAPI WinMain(HINSTANCE inHINSTANCE, HINSTANCE, LPSTR inCmdLineStr, int) {
	//register window class
	WNDCLASSEX wndClassEx = { 0 };
	wndClassEx.cbSize = sizeof(WNDCLASSEX);
	wndClassEx.cbClsExtra = 0;
	wndClassEx.cbWndExtra = 0;
	wndClassEx.hbrBackground = nullptr;
	wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClassEx.hIcon = nullptr;
	wndClassEx.hIconSm = nullptr;
	wndClassEx.hInstance = inHINSTANCE;
	wndClassEx.lpfnWndProc = VulkanRenderWindowProc;//function
	wndClassEx.lpszMenuName = nullptr;
	wndClassEx.lpszClassName = L"Apple";
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
	//create window
	ATOM atom = RegisterClassEx(&wndClassEx);
	RECT rect = { 0 };
	rect.right = 1280;
	rect.bottom = 720;
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	HWND hwnd = CreateWindowEx(
		NULL, L"Apple", L"App Render Window", WS_OVERLAPPEDWINDOW,
		200, 200, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, inHINSTANCE, NULL);
	GetClientRect(hwnd, &rect);//render region
	int canvasWidth = rect.right - rect.left;
	int canvasHeight = rect.bottom - rect.top;
	printf("canvs : %d x %d\n", canvasWidth, canvasHeight);
	GetWindowRect(hwnd, &rect);//render region
	printf("window : %d x %d\n", rect.right - rect.left, rect.bottom - rect.top);
	//init vulkan
	InitVulkanUserData initVulkanUserData = { inHINSTANCE, hwnd };
	bool vulkanInited = InitVulkan(&initVulkanUserData, canvasWidth, canvasHeight);
	if (vulkanInited) {
		InitScene(canvasWidth, canvasHeight);
	}
	//show window
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	MSG msg;
	DWORD last_time = timeGetTime();
	while (true) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			if (vulkanInited) {
				//render
				DWORD current_time = timeGetTime();//ms
				DWORD frameTime = current_time - last_time;
				last_time = current_time;
				float frameTimeInSecond = float(frameTime) / 1000.0f;
				RenderOneFrame(frameTimeInSecond);
			}
		}
	}
	return 0;
}