#ifndef WINDOWS_UTIL_H
#define WINDOWS_UTIL_H

//windows
#include <Windowsx.h>
#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>

//this program
#include "common.cpp"

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

/*
	Copied from FolderBrowser.cpp from Kalles Fraktaler: https://code.mathr.co.uk/kalles-fraktaler-2/blob/b8234f690ff44cd3f6af7d90d2c6c44695ccb4f0:/common/FolderBrowser.cpp
	Function to open a file browser to select a filename or destination for a file to be read of written
*/
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

class Win32BitmapManager : public BitmapManager {
public:
	ARGB* ptPixels;
	HBITMAP screenBMP;
	int screenWidth;
	int screenHeight;
	HWND* hWnd;

	Win32BitmapManager() {}
	Win32BitmapManager(HWND* hWnd) {
		this->hWnd = hWnd;
	}

	ARGB* realloc(int newScreenWidth, int newScreenHeight) {
		DeleteObject(screenBMP);
		HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
		BITMAPINFO RGB32BitsBITMAPINFO;
		ZeroMemory(&RGB32BitsBITMAPINFO, sizeof(BITMAPINFO));
		RGB32BitsBITMAPINFO.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		RGB32BitsBITMAPINFO.bmiHeader.biWidth = newScreenWidth;
		RGB32BitsBITMAPINFO.bmiHeader.biHeight = newScreenHeight;
		RGB32BitsBITMAPINFO.bmiHeader.biPlanes = 1;
		RGB32BitsBITMAPINFO.bmiHeader.biBitCount = 32;
		screenBMP = CreateDIBSection(
			hdc,
			(BITMAPINFO*)&RGB32BitsBITMAPINFO,
			DIB_RGB_COLORS,
			(void**)&ptPixels,
			NULL, 0
		);
		screenWidth = newScreenWidth;
		screenHeight = newScreenHeight;
		return ptPixels;
	}

	void draw() {
		HDC hdc = GetDC(*hWnd);
		DrawBitmap(hdc, 0, 0, screenBMP, SRCCOPY, screenWidth, screenHeight);
		ReleaseDC(*hWnd, hdc);
	}
};

#endif