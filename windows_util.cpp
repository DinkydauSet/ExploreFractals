#ifndef WINDOWS_UTIL_H
#define WINDOWS_UTIL_H

//windows
#include <windows.h>
#include <commdlg.h>

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

#endif