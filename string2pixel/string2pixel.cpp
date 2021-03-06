#include "stdafx.h"
#include "string2pixel.h"
#include <Windows.h>
#include <shellapi.h>

#pragma comment(lib,   "shell32.lib")

#define MAX_LOADSTRING 100

const wchar_t *lpszName = L"fux2string2pixel";
const wchar_t *szAppName = L"String2Pixel";

const wchar_t *defaultFontName = L"宋体";
const int defaultFontSize = 12;

DWORD whiteColor = 0xFFFFFFFF;
DWORD blackColor = 0xFF000000;

HMENU hmenu;
NOTIFYICONDATA nid;
int HotKeyId;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HINSTANCE gHInstance;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	HANDLE hMutex = NULL;
	gHInstance = hInstance;

	hMutex = CreateMutex(NULL, FALSE, lpszName);
	DWORD dwRet = GetLastError();
	if(hMutex)
	{
		if(ERROR_ALREADY_EXISTS == dwRet)
		{
			CloseHandle(hMutex);
			return 1;
		}
		HWND hwnd;
		MSG msg;
		WNDCLASS wndclass;

		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = hInstance;
		wndclass.hIcon = LoadIcon(gHInstance, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(gHInstance, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = szAppName;

		if (!RegisterClass(&wndclass))
		{
			return 3;
		}

		hwnd = CreateWindowEx(WS_EX_TOOLWINDOW,
			szAppName, szAppName,
			WS_POPUP,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			NULL, NULL, hInstance, NULL);

		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);

		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return msg.wParam;
	}
	else
	{
		return 2;
	}
	CloseHandle(hMutex);
    return 0;
}

void drawPixelWord(int fx, int fy, int srcWidth, int srcHeight, int dstWidth, int realWidth, BYTE *pBuffer, DWORD *pdst, DWORD rcolor,int mw,int mh) {
	int startX = fx > 0 ? 0 : -fx;
	int startY = fy > 0 ? 0 : -fy;
	int endX = fx + srcWidth > mw ? mw - fx : srcWidth;
	int endY = fy + srcHeight > mh ? mh - fy : srcHeight;
	int index = (fy + startY) * 2 * dstWidth + (fx + startX) * 2;
	for (int dy = startY; dy<endY; dy++) {
		BYTE *srcPtr = pBuffer + dy * realWidth;
		for (int dx = startX; dx<endX; dx++) {
			BYTE btCode = srcPtr[dx / 8];
			DWORD *dstPtr = pdst + index + dx * 2;
			if (btCode & (1 << 7 - (dx % 8))) {
				dstPtr[0] = rcolor;
				dstPtr[1] = rcolor;
				dstPtr[dstWidth] = rcolor;
				dstPtr[dstWidth+1] = rcolor;
			}
		}
		index += dstWidth * 2;
	}
}

void convertTextToPixel(wchar_t *text, HWND hwnd) {
	if (OpenClipboard(NULL) && wcslen(text)>0) {
		EmptyClipboard();
		DWORD needBytesMax = 0;
		HDC hScrDC;
		HBITMAP hBitmap, hOldBitmap;
		TEXTMETRIC tm;
		GLYPHMETRICS gm;
		HFONT myFont = NULL;
		LOGFONT fontRect;
		MAT2 mat2 = { { 0,1 },{ 0, 0 },{ 0, 0 },{ 0, 1 } };
		int rectSize[2] = { 2,0 };

		memset(&fontRect, 0, sizeof(LOGFONT));
		fontRect.lfCharSet = DEFAULT_CHARSET;
		fontRect.lfHeight = defaultFontSize;
		lstrcpy(fontRect.lfFaceName, defaultFontName);

		HDC hdc = GetDC(NULL);
		hScrDC = CreateCompatibleDC(hdc);
		ReleaseDC(NULL, hdc);
		GetTextMetrics(hScrDC, &tm);
		DeleteObject(myFont);
		myFont = CreateFontIndirect(&fontRect);
		SelectObject(hScrDC, myFont);
		GetTextMetrics(hScrDC, &tm);
		int needWidth = 0;
		int needHeight = 0;
		int stringLength = wcslen(text);
		
		for (int i = 0; i < stringLength; i++) {
			DWORD needBytes = GetGlyphOutline(hScrDC, text[i], GGO_GRAY8_BITMAP, &gm, 0, 0, &mat2);
			if (needBytes > needBytesMax) needBytesMax = needBytes;
			if (needBytes && gm.gmptGlyphOrigin.x + rectSize[0] < 0) rectSize[0] = -gm.gmptGlyphOrigin.x;
			if (i >= stringLength - 1) {
				if (gm.gmBlackBoxX + gm.gmptGlyphOrigin.x <= gm.gmCellIncX) {
					rectSize[0] += gm.gmCellIncX;
				}
				else {
					rectSize[0] += gm.gmBlackBoxX + gm.gmptGlyphOrigin.x;
				}
			}
			else {
				rectSize[0] += gm.gmCellIncX;
			}
			if (rectSize[1] < tm.tmHeight) rectSize[1] = tm.tmHeight;
		}

		BITMAPINFO bi;
		PVOID buffer;
		memset(&bi, 0, sizeof(BITMAPINFO));
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = rectSize[0]*2+8;
		bi.bmiHeader.biHeight = rectSize[1]*2+8;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		bi.bmiHeader.biSizeImage = 0;

		hBitmap = CreateDIBSection(hScrDC, &bi, DIB_RGB_COLORS, (void**)&buffer, NULL, NULL);
		hOldBitmap = (HBITMAP)SelectObject(hScrDC, hBitmap);
		RECT rc = { 0, 0, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight };
		HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 255));
		FillRect(hScrDC, &rc, hbr);
		DWORD *dst = (DWORD*)buffer;
		BYTE *pBuffer = new BYTE[needBytesMax];
		int adaptNum = 2;
		
		int pitch = bi.bmiHeader.biWidth;

		for (int i = 0; i < stringLength; i++) {
			memset(pBuffer, 0, needBytesMax);
			GetGlyphOutline(hScrDC, text[i], GGO_METRICS, &gm, NULL, NULL, &mat2);
			GetGlyphOutline(hScrDC, text[i], GGO_BITMAP, &gm, needBytesMax, pBuffer, &mat2);
			if (gm.gmptGlyphOrigin.x + adaptNum < 0) adaptNum = -gm.gmptGlyphOrigin.x;
			int singleWidth = gm.gmBlackBoxX;
			int singleHeight = gm.gmBlackBoxY;
			int realWidth = ((singleWidth + 31) >> 5) << 2;

			int fx = adaptNum + gm.gmptGlyphOrigin.x; // + draw_x
			int fy = tm.tmAscent - gm.gmptGlyphOrigin.y + 2; // + draw_y

			drawPixelWord(fx-1, fy, singleWidth, singleHeight, pitch, realWidth, pBuffer, dst, blackColor, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);
			drawPixelWord(fx+1, fy, singleWidth, singleHeight, pitch, realWidth, pBuffer, dst, blackColor, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);
			drawPixelWord(fx, fy-1, singleWidth, singleHeight, pitch, realWidth, pBuffer, dst, blackColor, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);
			drawPixelWord(fx, fy+1, singleWidth, singleHeight, pitch, realWidth, pBuffer, dst, blackColor, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);

			drawPixelWord(fx, fy, singleWidth, singleHeight, pitch, realWidth, pBuffer, dst, whiteColor, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);

			adaptNum += gm.gmCellIncX;
		}
		hBitmap = (HBITMAP)SelectObject(hScrDC, hOldBitmap);
		HBITMAP outputBitmap = CreateBitmap(bi.bmiHeader.biWidth, bi.bmiHeader.biHeight, 1, 32, (void*)dst);
		SetClipboardData(CF_BITMAP, outputBitmap);
		DeleteDC(hScrDC);
		DeleteObject(hBitmap);
		DeleteObject(outputBitmap);
		CloseClipboard();
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT WM_TASKBARCREATED;
	POINT pt;
	int popMenu;

	WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
	switch (message)
	{
	case WM_CREATE:
		nid.cbSize = sizeof(nid);
		nid.hWnd = hwnd;
		nid.uID = 0;
		nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		nid.uCallbackMessage = WM_USER;
		nid.hIcon = LoadIcon(gHInstance, MAKEINTRESOURCE(IDI_SMALL));
		lstrcpy(nid.szTip, szAppName);
		Shell_NotifyIcon(NIM_ADD, &nid);
		hmenu = CreatePopupMenu();
		AppendMenu(hmenu, MF_STRING, 10086, L"退出");
		HotKeyId = GlobalAddAtom(L"fux2string2pixelhotkey");
		RegisterHotKey(hwnd, HotKeyId, MOD_CONTROL | MOD_ALT, 'D');
		break;
	case WM_USER:
		if (lParam == WM_LBUTTONDOWN)
			MessageBox(hwnd, TEXT("按Ctrl+Alt+D转换剪贴板内容"), szAppName, MB_OK);
		if (lParam == WM_RBUTTONDOWN)
		{
			GetCursorPos(&pt);
			SetForegroundWindow(hwnd);
			popMenu = TrackPopupMenu(hmenu, TPM_RETURNCMD, pt.x, pt.y, NULL, hwnd, NULL);
			if (popMenu == 10086) {
				SendMessage(hwnd, WM_CLOSE, wParam, lParam);
			}
			if (popMenu == 0) PostMessage(hwnd, WM_LBUTTONDOWN, NULL, NULL);
		}
		break;
	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;
	case WM_HOTKEY:
		if (OpenClipboard(NULL))
		{
			HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
			if (NULL != hMem)
			{
				wchar_t* lpStr = (wchar_t*)GlobalLock(hMem);
				if (NULL != lpStr)
				{
					convertTextToPixel(lpStr,hwnd);
					GlobalUnlock(hMem);
				}
			}
			CloseClipboard();
		}
		break;
	default:
		if (message == WM_TASKBARCREATED)
			SendMessage(hwnd, WM_CREATE, wParam, lParam);
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}