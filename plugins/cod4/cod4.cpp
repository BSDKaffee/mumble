#define _USE_MATH_DEFINES 

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <tlhelp32.h>
#include <math.h>

#include "../mumble_plugin.h"

HANDLE h = NULL;

static DWORD getProcess(const wchar_t *exename) {
	PROCESSENTRY32 pe;
	DWORD pid = 0;

	pe.dwSize = sizeof(pe);
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE) {
		BOOL ok = Process32First(hSnap, &pe);

		while (ok) {
			if (wcscmp(pe.szExeFile, exename)==0) {
				pid = pe.th32ProcessID;
				break;
			}
			ok = Process32Next(hSnap, &pe);
		}
		CloseHandle(hSnap);
	}
	return pid;
}

static bool peekProc(VOID *base, VOID *dest, SIZE_T len) {
	SIZE_T r;
	BOOL ok=ReadProcessMemory(h, base, dest, len, &r);
	return (ok && (r == len));
}

static void about(HWND h) {
	::MessageBox(h, L"Reads audio position information from COD4 (v 1.7.568)", L"Mumble COD4 Plugin", MB_OK);
}


static int fetch(float *pos, float *front, float *top) {
	float viewHor, viewVer;
	for (int i=0;i<3;i++)
		pos[i]=front[i]=top[i]=0.0;

	bool ok;

	/*
		This plugin uses the following Variables:

			Address			Type	Description
			===================================
			0x0032AFD0		float	Z-Coordinate
			0x0032AFE0		float	X-Coordinate
			0x0032AFF0		float	Y-Coordinate
			0x0032AF3C		float	Horizontal view (degrees)
			0x0032AF38		float	Vertical view (degrees)
	*/
	ok = peekProc((BYTE *) 0x0072AFD0, pos+2, 4) &&	//Z
		 peekProc((BYTE *) 0x0072AFE0, pos, 4) &&	//X
		 peekProc((BYTE *) 0x0072AFF0, pos+1, 4) && //Y
		 peekProc((BYTE *) 0x0072AF3C, &viewHor, 4) && //Hor
		 peekProc((BYTE *) 0x0072AF38, &viewVer, 4); //Ver

	if (! ok)
		return false;

	// Scale Coordinates
	/* 
	   Z-Value is increasing when heading north
				  decreasing when heading south
	   X-Value is increasing when heading west
				  decreasing when heading east
	   Y-Value is increasing when going up
				  decreasing when going down
	   40 units = 1 meter (not confirmed)
	*/
	for (int i=0;i<3;i++)
		pos[i]/=40; // Scale to meters
	pos[0]*=(-1); // Convert right to left handed

	// Fake top vector
	top[2] = -1; // Head movement is in front vector
	// Calculate view unit vector
	/*
	   Vertical view 0� when centered
					85�	when looking down
				   275� when looking up
	   Decreasing when looking up.
	   
	   Horizontal is 0� when facing North
					90� when facing West
				   180� when facing South
				   270� when facing East
	   Increasing when turning left.
	*/
	viewVer *= (float)M_PI/180;
	viewHor *= (float)M_PI/180;

	front[0] = -sin(viewHor) * cos(viewVer);
	front[1] = -sin(viewVer);
	front[2] = cos(viewHor) * cos(viewVer);

	return ok;
}

static int trylock() {
	h = NULL;
	DWORD pid=getProcess(L"iw3mp.exe");
	if (!pid)
		return false;

	h=OpenProcess(PROCESS_VM_READ, false, pid);
	if (!h)
		return false;

	float pos[3], front[3], top[3];
	if (fetch(pos, front, top))
		return true;

	CloseHandle(h);
	h = NULL;
	return false;
}

static void unlock() {
	if (h) {
		CloseHandle(h);
		h = NULL;
	}
}

static MumblePlugin cod4plug = {
	MUMBLE_PLUGIN_MAGIC,
	L"Call of Duty 4 v1.7.568",
	L"Call of Duty 4",
	about,
	NULL,
	trylock,
	unlock,
	fetch
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &cod4plug;
}
