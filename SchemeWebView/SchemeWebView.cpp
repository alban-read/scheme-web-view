// compile with: /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /c
// Web View Control
// app embeds own web view control.


#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/result.h>
#include <scheme/scheme.h>
#include <fmt/format.h>
#include <Shlwapi.h>
#include <CommCtrl.h>

#pragma comment(lib, "csv952.lib")

#include "WebView2.h"
#include <locale>
#include <codecvt>
#include "resource.h"
#include "commonview.h"
#include <deque>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "comctl32.lib")



using namespace Microsoft::WRL;

HANDLE g_messages_mutex;
HWND main_window;

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

std::string ws_2s(const std::wstring& wstr)
{
	using convert_type_x = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type_x, wchar_t> converter_x;
	return converter_x.to_bytes(wstr);
}

// wait at least ms while also feeding the windows event loop.
void wait(const long ms)
{
	const auto end = clock() + ms;
	do
	{
		MSG msg;
		// main window
		if (::PeekMessage(&msg, main_window, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		// any window in thread
		if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		::Sleep(0);
	} while (clock() < end);
}


void do_events(int turns)
{
	for (int i = 0; i < turns; i++) {
		::Sleep(0);
		MSG msg;
		// main window
		if (::PeekMessage(&msg, main_window, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		// any window in thread
		if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}
}


ptr scheme_wait(int ms)
{
	wait(ms);
	return Strue;
}


ptr scheme_yield(int ms)
{
	// yield and wait
	ReleaseMutex(g_script_mutex);
	wait(ms);
	// now we try to get back to life
	auto dw_wait_result = WaitForSingleObject(g_script_mutex, 5);
	while (dw_wait_result == WAIT_TIMEOUT)
	{
		wait(10);
		dw_wait_result = WaitForSingleObject(g_script_mutex, 5);
	}
	// finally we can run again.
	return Strue;
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

std::deque<std::wstring> post_messages;

ptr scheme_post_message(const char* msg) {
	WaitForSingleObject(g_messages_mutex, INFINITE);
	post_messages.emplace_back(s2_ws(msg));
	ReleaseMutex(g_messages_mutex);
	return Strue;
}

DWORD WINAPI process_postmessages(LPVOID x) {

	while (true) {

		while (post_messages.empty())
		{
			Sleep(20);
		};
		WaitForSingleObject(g_messages_mutex, INFINITE);
		while (!post_messages.empty()) {
			web_view_window->PostWebMessageAsString(post_messages.front().c_str());
			post_messages.pop_front();
		}
		ReleaseMutex(g_messages_mutex);
	}
}

// post back in; from web server event channel.
std::deque<std::string> messages;

ptr scheme_post_message_eventsource(const char* msg) {
	WaitForSingleObject(g_messages_mutex, INFINITE);
	messages.emplace_back(msg);
	ReleaseMutex(g_messages_mutex);
	return Strue;
}


// scheme call into web view.
ptr scheme_web_view_exec(const char* cmd, char* cbname)
{
	std::wstring script = s2_ws(cmd);
	std::string callback = cbname;

	if (web_view_window == nullptr) return Snil;
	web_view_window->ExecuteScript(script.c_str(), Callback<IWebView2ExecuteScriptCompletedHandler>(
		[callback](HRESULT errorCode, LPCWSTR resultObjectAsJson)->HRESULT {

		LPCWSTR S = resultObjectAsJson;
		if (!callback.empty() && S != nullptr && wcslen(S) > 0) {
			const auto param = _strdup(ws_2s(S).c_str());
			const auto command = fmt::format("({0} {1})", callback, param);
			eval_text(_strdup(command.c_str()));
		}
		return S_OK;

	}).Get());

	return Strue;
}

ptr scheme_web_view_value(const char* cmd, char* vname)
{
	std::wstring script = s2_ws(cmd);
	std::string value_name = vname;

	if (web_view_window == nullptr) return Snil;
	web_view_window->ExecuteScript(script.c_str(), Callback<IWebView2ExecuteScriptCompletedHandler>(
		[value_name](HRESULT errorCode, LPCWSTR resultObjectAsJson)->HRESULT {

		LPCWSTR S = resultObjectAsJson;

		if (!value_name.empty() && S != nullptr && wcslen(S) > 0) {
			const auto param = _strdup(ws_2s(S).c_str());
			WaitForSingleObject(g_script_mutex, INFINITE);
			{
				eval_text(fmt::format("(define {0} \"{1}\") \"{1}\"", value_name, param).c_str());
			}
			ReleaseMutex(g_script_mutex);
		}
		return S_OK;
	}).Get());

	return Strue;
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
	std::wstring buffer;  // stores file contents
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



ptr scheme_load_document_from_file(const char* relative_file_name)
{
	std::wstring file_name = wide_get_exe_folder() + L"/" + s2_ws(relative_file_name);
	std::wstring document = load_utf8_file_to_string(file_name);
	web_view_window->NavigateToString(document.c_str());
	return Strue;
}

// capture screen to file.
ptr scheme_capture_screen(const char* relative_file_name)
{
	const auto file_name = s2_ws(relative_file_name);

	ComPtr<IStream> stream;
	SHCreateStreamOnFileEx(
		file_name.c_str(),
		STGM_READWRITE | STGM_CREATE,
		FILE_ATTRIBUTE_NORMAL,
		TRUE,
		nullptr,
		&stream);

	web_view_window->CapturePreview(
		WEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.Get(),
		Microsoft::WRL::Callback<IWebView2CapturePreviewCompletedHandler>(
			[](HRESULT error_code) -> HRESULT
	{	// pointless callback
		return S_OK;
	}).Get());

	return Strue;
}


DWORD WINAPI  update_status(LPVOID x)
{
	RECT rect;
	int last_pending_commands = -1;
	int last_pending_messages = -1;
	while (true) {
		if (main_window != nullptr) {
			int pending_commands;
			int pending_messages;
			WaitForSingleObject(g_commands_mutex, INFINITE);
			pending_commands = commands.size();
			ReleaseMutex(g_commands_mutex);
			WaitForSingleObject(g_messages_mutex, INFINITE);
			pending_messages = messages.size() + post_messages.size();
			std::string text_message = fmt::format("Scheme ({0}.{1})",
				pending_commands, pending_messages);
			ReleaseMutex(g_messages_mutex);

			if (pending_commands != last_pending_commands
				|| pending_messages != last_pending_messages) {
				SetWindowTextA(main_window, text_message.c_str());
				last_pending_commands = pending_commands;
				last_pending_messages = pending_messages;
			}

		}
		Sleep(250);
	}
}




ptr scheme_get_source()
{
	wil::unique_cotaskmem_string uri;
	web_view_window->get_Source(&uri);
	const std::wstring source = uri.get();
	return Assoc::constUTF8toSstring(ws_2s(source).c_str());
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


bool check_valid_uri()
{
	wil::unique_cotaskmem_string uri;
	web_view_window->get_Source(&uri);
	const std::wstring source = uri.get();
	auto len = navigate_first.find_last_of(L'/');
	const auto base = navigate_first.substr(0, len);
	std::size_t found = source.find(base);
	return found != std::string::npos;
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
	int padh = h / 8;
	int padv = v / 8;


	HWND hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		300, 200,
		h - padh, v - padv,
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
	main_window = hWnd;


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
			Settings->put_IsScriptEnabled(true);
			Settings->put_AreDefaultScriptDialogsEnabled(true);
			Settings->put_IsWebMessageEnabled(true);
			Settings->put_AreDevToolsEnabled(true);
			Settings->put_IsStatusBarEnabled(true);


			// Resize WebView to fit the bounds of the parent window
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			bounds.bottom = bounds.bottom - 45;

			web_view_window->put_Bounds(bounds);

			EventRegistrationToken token;
			web_view_window->add_NavigationStarting(Callback<IWebView2NavigationStartingEventHandler>(
				[](IWebView2WebView* webview, IWebView2NavigationStartingEventArgs* args) -> HRESULT {
				PWSTR uri;
				args->get_Uri(&uri);
				const std::wstring source(uri);
				// ...
				CoTaskMemFree(uri);
				return S_OK;
			}).Get(), &token);

			web_view_window->add_ProcessFailed(Callback<IWebView2ProcessFailedEventHandler>(
				[](IWebView2WebView* webview, IWebView2ProcessFailedEventArgs* args) -> HRESULT {
				WEBVIEW2_PROCESS_FAILED_KIND kind;
				args->get_ProcessFailedKind(&kind);
				auto new_web_view_needed =
					(kind == WEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED);
				if (new_web_view_needed)
				{
					MessageBoxW(nullptr, L"This web view\r\n has sadly crashed..", L"Fatal Error", MB_OK);
					DestroyWindow(main_window);
				}
				else
				{	// might work we guess,.
					webview->Reload();
				}
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

			// messages between web view 2 and scheme app
			// I am sticking with text; and using a prefix in the request and response.
			web_view_window->add_WebMessageReceived(Callback<IWebView2WebMessageReceivedEventHandler>(
				[](IWebView2WebView* webview, IWebView2WebMessageReceivedEventArgs* args) -> HRESULT {


				if (!check_valid_uri())
				{
					return S_OK;
				}

				PWSTR message;
				args->get_WebMessageAsString(&message);

				std::string text = ws_2s(message);
				std::string result;

				// may be an eval message..
				const char* eval_cmd = "::eval:";
				if (text.rfind(eval_cmd, 0) == 0) {
					// eval in own thread.
					std::string command = text.c_str() + strlen(eval_cmd);

					eval_text(command.c_str());
					Sleep(0);
					return S_OK;
				}

				// may be an api message..
				const char* api_cmd = "::api:";
				if (text.rfind(api_cmd, 0) == 0) {
					char* end_ptr;
					int n = static_cast<int>(strtol(text.c_str() + strlen(api_cmd), &end_ptr, 10));

					if (n < 63) {

						// we have one thread at a time in the scheme engine and it could be busy.
						if (spin(20))
						{
							webview->PostWebMessageAsString(L"::busy_reply:");

							CoTaskMemFree(message);
							return S_OK;
						}

						std::string param = end_ptr;
						// browser to scheme api call

						const ptr scheme_string = CALL2("api-call", Sfixnum(n), Sstring(param.c_str()));

						std::string result;
						if (scheme_string != Snil && Sstringp(scheme_string))
						{
							result = Assoc::Sstring_to_charptr(scheme_string);
						}
						std::wstring response;
						if (result.rfind("::", 0) == std::string::npos)
							response = s2_ws(fmt::format("::api_reply:{0}:", n));
						response += s2_ws(result);
						webview->PostWebMessageAsString(response.c_str());
						ReleaseMutex(g_script_mutex);
						return S_OK;

					}
					// `internal` api calls handled here
					switch (n)
					{
					case 1064: // cancel queued tasks
						cancel_commands();
						return S_OK;
						break;

					default:
						break;
					}
				}

				std::wstring response = L"::invalid_request:";
				response += message;
				webview->PostWebMessageAsString(response.c_str());
				CoTaskMemFree(message);
				wait(25);
				return S_OK;
			}).Get(), &token);

			// get this thing on its way
			web_view_window->Navigate(navigate_first.c_str());
			return S_OK;
		}).Get());
		return S_OK;
	}).Get());


	UpdateWindow(hWnd);
	ShowWindow(hWnd, nCmdShow);
	auto engine_started = start_scheme_engine();

	CreateThread(
		nullptr,
		0,
		update_status,
		nullptr,
		0,
		nullptr);


	CreateThread(
		nullptr,
		0,
		process_postmessages,
		nullptr,
		0,
		nullptr);

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
	HDC         hdc;
	PAINTSTRUCT ps;
	RECT        rect;

	switch (message)
	{
	case WM_SIZE:
		if (web_view_window != nullptr) {
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			bounds.bottom = bounds.bottom - 45;
			web_view_window->put_Bounds(bounds);
		};
		break;



	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// post from another thread	
	case WM_USER + 501:
	{
		web_view_window->PostWebMessageAsString(reinterpret_cast<wchar_t*>(lParam));
		return 0;
	}


	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}
