#include <scheme/scheme.h>
#include <windows.h>

#include "commonview.h"

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