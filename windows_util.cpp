//Visual Studio requires this for no clear reason
#include "stdafx.h"

#include <Windowsx.h>
#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>

#include "common.cpp"


#ifndef _windows_util_
#define _windows_util_

HWND WINAPI CreateTrackbar(
	HWND hwndDlg,  // handle of dialog box (parent window) 
	UINT selMin,     // minimum value in trackbar range 
	UINT selMax,     // maximum value in trackbar range
	int xPos, int yPos,
	int hSize, int vSize,
	int identifier,
	HINSTANCE hInst)
{
	HWND hwndTrack = CreateWindowEx(
		0,                               // no extended styles 
		TRACKBAR_CLASS,                  // class name 
		_T("Trackbar Control"),              // title (caption) 
		WS_CHILD |
		WS_VISIBLE |
		TBS_AUTOTICKS |
		TBS_ENABLESELRANGE,              // style 
		xPos, yPos,                          // position 
		hSize, vSize,                         // size 
		hwndDlg,                         // parent window 
		(HMENU)identifier,                     // control identifier 
		hInst,                         // instance 
		NULL                             // no WM_CREATE parameter 
	);
	SendMessage(hwndTrack, TBM_SETRANGE,
		(WPARAM)TRUE,                   // redraw flag 
		(LPARAM)MAKELONG(selMin, selMax));  // min. & max. positions
	SendMessage(hwndTrack, TBM_SETPAGESIZE,
		0, (LPARAM)4);                  // new page size 
	SendMessage(hwndTrack, TBM_SETPOS,
		(WPARAM)TRUE,                   // redraw flag 
		(LPARAM)selMin);
	SetFocus(hwndTrack);
	return hwndTrack;
}

int BrowseFile(HWND hwParent, BOOL bOpen, const char *szTitle, const char *szExt, std::string &szFile) {
	char buffer[1024] = { 0 };
	strncpy(buffer, szFile.c_str(), sizeof(buffer));
	buffer[sizeof(buffer) - 1] = 0;
	OPENFILENAMEA ofn = { sizeof(OPENFILENAME) };
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.lpstrFile = buffer;
	ofn.lpstrTitle = szTitle;
	ofn.nMaxFile = sizeof(buffer);
	ofn.hwndOwner = hwParent;
	ofn.lpstrFilter = szExt;
	ofn.nFilterIndex = 1;
	if (bOpen) {
		ofn.Flags = OFN_SHOWHELP | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		int ret = GetOpenFileNameA(&ofn);
		if (ret)
		{
			szFile = buffer;
		}
		return ret;
	}
	else {
		ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
		if (GetSaveFileNameA(&ofn)) {
			szFile = buffer;
			char *s = strrchr(buffer, '\\');
			if (!s)
				s = buffer;
			else
				s++;
			if (!strrchr(s, '.')) {
				const char *e = szExt;
				e += strlen(e) + 1;
				if (*e) {
					for (int i = 1; i < (int)ofn.nFilterIndex; i++) {
						e += strlen(e) + 1;
						if (!*e)
							break;
						e += strlen(e) + 1;
						if (!*e)
							break;
					}
				}
				if (strstr(e, "."))
					e = strstr(e, ".");
				szFile += e;
			}
			return 1;
		}
		else
			return 0;
	}
}

BOOL DrawBitmap(HDC hDC, int x, int y, HBITMAP& hBitmap, DWORD dwROP, int screenWidth, int screenHeight) {
	if(debug) cout << "drawing" << endl;
	HDC hDCBits;
	BOOL bResult;
	if (!hDC || !hBitmap)
		return FALSE;
	hDCBits = CreateCompatibleDC(hDC);
	SelectObject(hDCBits, hBitmap);

	bResult = BitBlt(hDC, x, y, screenWidth, screenHeight, hDCBits, 0, 0, dwROP);

	DeleteDC(hDCBits);
	return bResult;
}

#endif