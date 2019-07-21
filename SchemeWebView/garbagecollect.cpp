#include <scheme/scheme.h>
#include <windows.h>

#define CALL0(who) Scall0(Stop_level_value(Sstring_to_symbol(who)))
#define CALL1(who, arg) Scall1(Stop_level_value(Sstring_to_symbol(who)), arg)
#define CALL2(who, arg, arg2) Scall2(Stop_level_value(Sstring_to_symbol(who)), arg, arg2)
extern HANDLE g_script_mutex;

// spare a second to take out the garbage.
DWORD WINAPI  garbage_collect(LPVOID cmd)
{
	while (true) {
		// every second; wait briefly; if engine not busy; do gc 
		const auto dw_wait_result = WaitForSingleObject(g_script_mutex, 25);
		if (dw_wait_result != WAIT_TIMEOUT) {
			
			CALL0("gc"); 
			ReleaseMutex(g_script_mutex);
		}
		Sleep(1000);
	}
 
}