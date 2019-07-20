// compile with: /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /c

#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/result.h>
#include <scheme/scheme.h>
#include <fmt/format.h>

#pragma comment(lib, "csv952.lib")

#include "WebView2.h"
#include <locale>
#include <codecvt>
#include "resource.h"

using namespace Microsoft::WRL;

#ifndef ABNORMAL_EXIT
#define ABNORMAL_EXIT ((void (*)(void))0)
#endif /* ABNORMAL_EXIT */

// Global variables
#define CALL0(who) Scall0(Stop_level_value(Sstring_to_symbol(who)))
#define CALL1(who, arg) Scall1(Stop_level_value(Sstring_to_symbol(who)), arg)
#define CALL2(who, arg, arg2) Scall2(Stop_level_value(Sstring_to_symbol(who)), arg, arg2)

int start_scheme_engine();
int init_web_server();
int start_web_server(int port, const std::string& base);
DWORD WINAPI  garbage_collect(LPVOID cmd);
std::string get_exe_folder();
extern std::wstring navigate_first;

// The main window class name.
static TCHAR szWindowClass[] = _T("SchemeShell");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("Scheme");

HINSTANCE hInst;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Pointer to WebView window
static ComPtr<IWebView2WebView> web_view_window;

std::wstring s2_ws(const std::string& str)
{
	using convert_type_x = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type_x, wchar_t> converter_x;
	return converter_x.from_bytes(str);
}

HRESULT web_view_navigate(const std::string& url) {
	const auto wide_url = s2_ws(url);
	if (web_view_window != nullptr) {
		return web_view_window->Navigate(wide_url.c_str());
	}
	return -1;
}

void web_view_exec(const std::wstring& script) {
	if (web_view_window == nullptr) return;
	web_view_window->ExecuteScript(script.c_str(), Callback<IWebView2ExecuteScriptCompletedHandler>(
		[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
		LPCWSTR S = resultObjectAsJson;
		//doSomethingWithURL(URL);
		return S_OK;
	}).Get());
}

std::wstring wide_get_exe_folder()
{
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(nullptr, buffer, MAX_PATH);
	const auto pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}

size_t get_size_of_file(const std::wstring& path)
{
	struct _stat fileinfo {};
	_wstat(path.c_str(), &fileinfo);
	return fileinfo.st_size;
}

std::wstring load_utf8_file_to_string(const std::wstring& filename)
{
	std::wstring buffer;            // stores file contents
	FILE* f;
	const auto err = _wfopen_s(&f, filename.c_str(), L"rtS, ccs=UTF-8");
	// Failed to open file
	if (f == nullptr || err != 0)
	{
		return buffer;
	}
	const auto file_size = get_size_of_file(filename);
	// Read entire file contents in to memory
	if (file_size > 0)
	{
		buffer.resize(file_size);
		const auto chars_read = fread(&(buffer.front()), sizeof(wchar_t), file_size, f);
		buffer.resize(chars_read);
		buffer.shrink_to_fit();
	}
	fclose(f);
	return buffer;
}

// we control the horizontal and the vertical..
void GetDesktopResolution(int& horizontal, int& vertical)
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);
	horizontal = desktop.right;
	vertical = desktop.bottom;
}


int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
)
{

	WNDCLASSEX window_class;
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = WndProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = hInstance;
	window_class.hIcon = LoadIcon(hInstance, IDI_ASTERISK);
	window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
	window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	window_class.lpszMenuName = nullptr;
	window_class.lpszClassName = szWindowClass;
	window_class.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

	if (!RegisterClassEx(&window_class))
	{
		MessageBox(nullptr,
			_T("Call to RegisterClassEx failed!"),
			_T("Windows Scheme WebView2 Shell"),
			NULL);

		return 1;
	}


	int h = 0;
	int v = 0;
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	GetDesktopResolution(h, v);
	hInst = hInstance;

	HWND hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		300, 200,
		h - 400, v - 400,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hWnd)
	{
		MessageBox(nullptr,
			_T("Call to CreateWindow failed!"),
			_T("SchemeWebView"),
			NULL);

		return 1;
	}


	auto s = init_web_server();
	// Locate the browser and set up the environment for WebView
	CreateWebView2EnvironmentWithDetails(nullptr, nullptr, WEBVIEW2_RELEASE_CHANNEL_PREFERENCE_CANARY, nullptr,
		Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
			[hWnd](HRESULT result, IWebView2Environment* env) -> HRESULT {

		// Create a WebView, whose parent is the main window hWnd
		env->CreateWebView(hWnd, Callback<IWebView2CreateWebViewCompletedHandler>(
			[hWnd](HRESULT result, IWebView2WebView* webview) -> HRESULT {
			if (webview != nullptr) {
				web_view_window = webview;
			}

			IWebView2Settings* Settings;
			web_view_window->get_Settings(&Settings);
			Settings->put_IsScriptEnabled(TRUE);
			Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
			Settings->put_IsWebMessageEnabled(TRUE);
			Settings->put_AreDevToolsEnabled(TRUE);
			Settings->put_IsStatusBarEnabled(TRUE);


			// Resize WebView to fit the bounds of the parent window
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			web_view_window->put_Bounds(bounds);

			EventRegistrationToken token;
			web_view_window->add_NavigationStarting(Callback<IWebView2NavigationStartingEventHandler>(
				[](IWebView2WebView* webview, IWebView2NavigationStartingEventArgs* args) -> HRESULT {
				PWSTR uri;
				args->get_Uri(&uri);
				const std::wstring source(uri);
				// perhaps let the engine know we navigated..

				CoTaskMemFree(uri);
				return S_OK;
			}).Get(), &token);

			// optionally loads base java script library into every document.
			const auto locate_script = wide_get_exe_folder() + L"//scripts//startup.js";
			if (!(INVALID_FILE_ATTRIBUTES == GetFileAttributes(locate_script.c_str()) &&
				GetLastError() == ERROR_FILE_NOT_FOUND))
			{
				const auto startup_script = load_utf8_file_to_string(locate_script);
				web_view_window->AddScriptToExecuteOnDocumentCreated(startup_script.c_str(), nullptr);
			}

			// get this thing on its way
			web_view_window->Navigate(navigate_first.c_str());
			return S_OK;
		}).Get());
		return S_OK;
	}).Get());


	UpdateWindow(hWnd);
	ShowWindow(hWnd, nCmdShow);
	auto engine_started = start_scheme_engine();

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TCHAR greeting[] = _T("WebView2andScheme");

	switch (message)
	{
	case WM_SIZE:
		if (web_view_window != nullptr) {
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			web_view_window->put_Bounds(bounds);
		};
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}
