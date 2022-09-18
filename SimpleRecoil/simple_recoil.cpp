#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <ctime>
#include "simple_recoil.h"
#include "mouse.h"
#include "keyboard.h"
#include <math.h>
#include "imgui.h"
#include <d3d9.h>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"

void recoil_thread();
void input_thread();
void ResetDevice() noexcept;
mouse m;
keyboard k;
bool mWindow = false;
bool operating = false;
bool working = false;
bool trigON = false;
bool audio = false;
COLORREF pixel;
COLORREF pixel2;
int red;
int green;
int blue;
int red2;
int green2;
int blue2;
HWND desktop = GetDesktopWindow();
HDC hdcMain = GetDC(desktop);

long long int counnter=0;

// constant window size
constexpr int WIDTH = 500;//137
constexpr int HEIGHT = 500;//35

// when this changes, exit threads
// and close menu :)
inline bool isRunning = true;

// winapi window vars
inline HWND window = nullptr;
inline WNDCLASSEX windowClass = { };

// points for window movement
inline POINTS position = { };

// direct x state vars
inline PDIRECT3D9 d3d = nullptr;
inline LPDIRECT3DDEVICE9 device = nullptr;
inline D3DPRESENT_PARAMETERS presentParameters = { };

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

LRESULT WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (device && wideParameter != SIZE_MINIMIZED)
		{
			presentParameters.BackBufferWidth = LOWORD(longParameter);
			presentParameters.BackBufferHeight = HIWORD(longParameter);
			ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(window, &rect);

			rect.left += points.x - position.x;
			rect.top += points.y - position.y;

			if (position.x >= 0 &&
				position.x <= WIDTH &&
				position.y >= 0 && position.y <= 19)
				SetWindowPos(
					window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		"class001",
		windowName,
		WS_POPUP,
		0,
		0,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);
	SetLayeredWindowAttributes(window, RGB(0, 0, 0), 0, ULW_COLORKEY);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
	
}

void DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void CreateImGui() noexcept
{
	//IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable | io.ConfigViewportsNoAutoMerge;

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
}

void Render()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	::SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	ImGui::SetNextWindowPos({ 1920, 0 });
	ImGui::SetNextWindowSize({ 137, 52 });
	ImGui::StyleColorsDark();
	ImGui::Begin(
		"Steam",
		&isRunning,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_AlwaysAutoResize
	);
	ImGui::PushItemWidth(100);
	ImGui::SliderInt("###", &settings.weapon_idx, 0, 2);
	
	ImGui::SameLine(110);ImGui::Checkbox("##", &operating);
	//ImGui::Text("Mode %d", id);
	if(working)ImGui::Text("Trigger ON");
	else ImGui::Text("Trigger OFF");
	ImGui::SameLine(90);
	if (ImGui::SmallButton("Menu")) {
		if (mWindow == false) {
			mWindow = true;
		}
		else if (mWindow == true) {
			mWindow = false;
		}
	}
	if (mWindow)
	{
		ImGui::SetNextWindowSize({ 200, 200 });
		ImGui::SetNextWindowPos({2060,0});
		ImGui::Begin(
			"Menu",
			&mWindow,
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_AlwaysAutoResize
		);
		ImGui::Text("MENU");
		ImGui::Text("Hotkeys: F3 to exit");
		ImGui::Text("Hotkeys: F2 for norecoil");
		ImGui::Text("Hotkeys: F1 for triggerbot");
		ImGui::Checkbox("Audio", &audio);

		ImGui::End();
	}

	ImGui::End();
}

enum weapon_list {
	weapon1,
	weapon2,
	weapon3
};

struct pattern {
	std::vector<short> x, y;
	int fire_rate;
	double multiplyer;
	double limit1;
	double limit2;
};

std::vector<pattern> weapon;

void initialize_patterns(std::vector<pattern>& vector, int weapon_count) { 
	for (int i = 0; i < weapon_count; i++)
		vector.push_back(pattern());

	vector[weapon1].x = { 4, -3, 3, -3, 2 }; //mp7 vector ar33
	vector[weapon1].y = { 16, 14, 14, 13, 15 };
	vector[weapon1].fire_rate = 70;
	vector[weapon1].multiplyer = 1.25; 
	vector[weapon1].limit1= 0.9;
	vector[weapon1].limit2=0.999;

	vector[weapon2].x = { 2, -1, 2, -3, 2 }; //t-5 smg11
	vector[weapon2].y = { 18, 16, 14, 16, 15 };
	vector[weapon2].fire_rate = 70;
	vector[weapon2].multiplyer = 1.4;
	vector[weapon2].limit1 = 1.4;
	vector[weapon2].limit2 = 1.999;

	vector[weapon3].x = { -4, -3, -3, -3, -4 }; //t89
	vector[weapon3].y = { 18, 16, 18, 16, 16 };
	vector[weapon3].fire_rate = 70;
	vector[weapon3].multiplyer = 1.5;
	vector[weapon3].limit1 = 1.4;
	vector[weapon3].limit2 = 1.999;
}

//int main() {
	
	//SetPriorityClass(GetCurrentProcess(), 0x00000020); //setting process priority
	//initialize_patterns(weapon, 3); //replace 'weapon_count' (2) with how many weapons you are adding, this is to improve memory usage
	//CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)input_thread, NULL, NULL, NULL); //creating 'input_thread' thread for getting key states
	//CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)recoil_thread, NULL, NULL, NULL); //creating 'recoil_thread' for the main recoil function

	//for (;;)
	//	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	//return NULL;
//}

void recoil_thread() {
	for (;;) {
		int counter = 0;
		double timecounter = 0.5;
			while (GetAsyncKeyState(VK_LBUTTON) & 0x8000 && GetAsyncKeyState(VK_RBUTTON) & 0x8000 && operating) {
				if (counter < weapon[settings.weapon_idx].x.size()) {
					for (int i = 0; i != 5; i++) {
						m.move(weapon[settings.weapon_idx].x[counter] / 4, timecounter + weapon[settings.weapon_idx].y[counter] / 4);
						std::this_thread::sleep_for(std::chrono::milliseconds(weapon[settings.weapon_idx].fire_rate / 4));
					}

					m.move(weapon[settings.weapon_idx].x[counter] % 4, timecounter + weapon[settings.weapon_idx].y[counter] % 4);
					std::this_thread::sleep_for(std::chrono::milliseconds(weapon[settings.weapon_idx].fire_rate % 4));
					counter++;
					if (counter == 4)counter = 0;
				}
				if (timecounter < weapon[settings.weapon_idx].limit1) {
					timecounter = timecounter * weapon[settings.weapon_idx].multiplyer;
				}
				else if (timecounter < weapon[settings.weapon_idx].limit2) {
					timecounter = weapon[settings.weapon_idx].limit2;
				}
				//std::cout << timecounter << "\n";
			}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void trigger_thread() {
	
	for (;;) {
		
		while (working) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			pixel2 =GetPixel(hdcMain, 960, 535);
			red2 = GetRValue(pixel2);
			green2 = GetGValue(pixel2);
			blue2 = GetBValue(pixel2);
			if (red != red2 && green != green2 && blue != blue2 && working) {
				m.click(300);
				Beep(50, 300);
				working = false;
			}
		}
		counnter++;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void input_thread() {
	for (;;) {
		if (GetAsyncKeyState(VK_F2) & 0x8000) {
			if(operating==false){
				operating = true;
				if (audio) Beep(500, 200);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
			else if (operating == true) {
				operating = false;
				if (audio) Beep(100, 200);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		}
		if (GetAsyncKeyState(VK_LEFT) & 0x8000 && settings.weapon_idx > 0) {
			settings.weapon_idx--;
			if (audio) Beep(150 + settings.weapon_idx * 100, 200);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000 && settings.weapon_idx < weapon.size() - 1) {
			settings.weapon_idx++;
			if (audio) Beep(150 + settings.weapon_idx * 100, 200);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		if (GetAsyncKeyState(VK_F3) & 0x8000) {
			Beep(50, 400);
			ExitProcess(3);
		}
		if (GetAsyncKeyState(VK_F1) & 0x8000 || trigON) {
			if (working == false) {
				working = true;
				trigON = false;
				pixel = GetPixel(hdcMain, 960, 535);
				red = GetRValue(pixel);
				green = GetGValue(pixel);
				blue = GetBValue(pixel);
				if (audio)Beep(500, 200);
				else std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
			else if (working == true) {
				working = false;
				if (audio) Beep(100, 200);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

int __stdcall wWinMain(
	HINSTANCE instance,
	HINSTANCE previousInstance,
	PWSTR arguments,
	int commandShow) {
	CreateHWindow("Steam");
	CreateDevice();
	CreateImGui();
	SetPriorityClass(GetCurrentProcess(), 0x00000020); //setting process priority
	initialize_patterns(weapon, 3); //replace 'weapon_count' (3) with how many weapons you are adding, this is to improve memory usage
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)input_thread, NULL, NULL, NULL); //creating 'input_thread' thread for getting key states
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)recoil_thread, NULL, NULL, NULL); //creating 'recoil_thread' for the main recoil function
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)trigger_thread, NULL, NULL, NULL);

	while (isRunning)
	{
		BeginRender();
		
		Render();
		EndRender();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// destroy gui
	DestroyImGui();
	DestroyDevice();
	DestroyHWindow();

	return EXIT_SUCCESS;
}