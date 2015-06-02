static char *image_id = 
	"@(#)Copyright (C) H.Shirouzu 2011-2012   image.cpp	Ver3.40";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Image
	Create					: 2011-07-24(Mon)
	Update					: 2012-04-02(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include "../external/libpng/pnginfo.h"

#define PNG_SIG_SIZE 8

void png_vbuf_wfunc(png_struct *png, png_byte *buf, png_size_t size)
{
	VBuf	*vbuf = (VBuf *)png_get_io_ptr(png);

	if (!vbuf) return;

	if (vbuf->RemainSize() < (int)size) {
		int	grow_size = (int)size - vbuf->RemainSize();
		if (!vbuf->Grow(grow_size)) return;
	}

	// 圧縮中にも、わずかにメッセージループを回して、フリーズっぽい状態を避ける
	TApp::Idle(100);

	memcpy(vbuf->Buf() + vbuf->UsedSize(), buf, size);
	vbuf->AddUsedSize((int)size);
}

void png_vbuf_wflush(png_struct *png)
{
}

VBuf *BmpHandleToPngByte(HBITMAP hBmp)
{
	BITMAP		bmp;
	BITMAPINFO	*bmi = NULL;
	int			palette, total_size, header_size, data_size, line_size;
	HWND		hWnd = ::GetDesktopWindow();
	HDC			hDc = NULL;
	VBuf		bmpVbuf;
	png_struct	*png  = NULL;
	png_info	*info = NULL;
	png_color_8	sbit;
	png_byte	**lines = NULL;
	VBuf		*vbuf = NULL, *ret = NULL;

	if (!::GetObject(hBmp, sizeof(bmp), &bmp)) return NULL;

	//if (bmp.bmBitsPixel < 24)
	bmp.bmBitsPixel = 24;

	line_size   = bmp.bmWidth * ALIGN_SIZE(bmp.bmBitsPixel, 8) / 8;
	line_size   = ALIGN_SIZE(line_size, 4);
	data_size   = line_size * bmp.bmHeight;
	palette     = bmp.bmBitsPixel <= 8 ? 1 << bmp.bmBitsPixel : 0;
	header_size = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * palette;
	total_size	= header_size + data_size;

	if (!bmpVbuf.AllocBuf(total_size)) return	NULL;
	bmi = (BITMAPINFO *)bmpVbuf.Buf();

	bmi->bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth        = bmp.bmWidth;
	bmi->bmiHeader.biHeight       = bmp.bmHeight;
	bmi->bmiHeader.biPlanes       = 1;
	bmi->bmiHeader.biBitCount     = bmp.bmBitsPixel;
	bmi->bmiHeader.biCompression  = BI_RGB;
	bmi->bmiHeader.biClrUsed      = palette;
	bmi->bmiHeader.biClrImportant = palette;

	if (!(hDc = ::GetDC(hWnd)) ||
		!::GetDIBits(hDc, hBmp, 0, bmp.bmHeight, (char *)bmi + header_size, bmi, DIB_RGB_COLORS)) {
		goto END;
	}

	if (!(png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0))) return NULL;
	if (!(info = png_create_info_struct(png))) goto END;
	if (!(vbuf = new VBuf(0, total_size))) goto END;

	png_set_write_fn(png, (void *)vbuf, (png_rw_ptr)png_vbuf_wfunc,
					(png_flush_ptr)png_vbuf_wflush);

	if (palette) {
		png_color	png_palette[256];
		for (int i=0; i < palette; i++) {
			png_palette[i].red		= bmi->bmiColors[i].rgbRed;
			png_palette[i].green	= bmi->bmiColors[i].rgbGreen;
			png_palette[i].blue		= bmi->bmiColors[i].rgbBlue;
		}
		png_set_IHDR(png, info, bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel,
					PNG_COLOR_TYPE_PALETTE,
					PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_PLTE(png, info, png_palette, palette);
	}
	else {
		png_set_IHDR(png, info, bmp.bmWidth, bmp.bmHeight, 8,
					bmp.bmBitsPixel > 24 ? PNG_COLOR_TYPE_RGB_ALPHA  : PNG_COLOR_TYPE_RGB,
					PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	}
	sbit.red = sbit.green = sbit.blue = 8;
	sbit.alpha = bmp.bmBitsPixel > 24 ? 8 : 0;
	png_set_sBIT(png, info, &sbit);

	if (setjmp(png_jmpbuf(png))) {
		goto END;
	}
	else {
		png_write_info(png, info);
		png_set_bgr(png);

		lines = (png_byte **)malloc(sizeof(png_bytep *) * bmp.bmHeight);

		for (int i = 0; i < bmp.bmHeight; i++) {
			lines[i] = bmpVbuf.Buf() + header_size + line_size * (bmp.bmHeight - i - 1);
		}
		png_write_image(png, lines);
		png_write_end(png, info);
		ret = vbuf;
	}

END:
	if (png) png_destroy_write_struct(&png, &info);
	if (hDc) ::ReleaseDC(hWnd, hDc);
	if (lines) free(lines);
	if (!ret && vbuf) delete vbuf;
	return	ret;
}

void png_vbuf_rfunc(png_struct *png, png_byte *buf, png_size_t size)
{
	VBuf	*vbuf = (VBuf *)png_get_io_ptr(png);

	if (!vbuf) return;

	u_int remain = vbuf->Size() - vbuf->UsedSize();

	if (remain < size) size = remain;
	memcpy(buf, vbuf->Buf() + vbuf->UsedSize(), size);
	vbuf->AddUsedSize((int)size);
}

HBITMAP PngByteToBmpHandle(VBuf *vbuf)
{
	png_struct	*png  = NULL;
	png_info	*info = NULL;
	HBITMAP		hBmp  = NULL;
	BITMAPINFO	*bmi  = NULL;
	png_byte	**row = NULL;
	BYTE		*data = NULL;
	HWND		hWnd = ::GetDesktopWindow();
	HDC			hDc   = NULL;
	int			line_size, aligned_line_size, header_size;
	VBuf		bmpVbuf;

	if (vbuf->Size() < PNG_SIG_SIZE || !png_check_sig(vbuf->Buf(), PNG_SIG_SIZE)) return NULL;

	if (!(png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0))) return NULL;
	if (!(info = png_create_info_struct(png))) goto END;

	if (setjmp(png_jmpbuf(png))) goto END;

	png_set_user_limits(png, 15000, 10000); // 15,000 * 10,000 pix
	png_set_read_fn(png, (void *)vbuf, (png_rw_ptr)png_vbuf_rfunc);
	png_read_png(png, info, PNG_TRANSFORM_BGR, NULL);

	if (info->bit_depth > 8) goto END; // not support

	line_size = info->width * info->channels * ALIGN_SIZE(info->bit_depth, 8) / 8;
	aligned_line_size = ALIGN_SIZE(line_size, 4);
	header_size = sizeof(BITMAPV5HEADER) + sizeof(RGBQUAD) * info->num_palette;

	if (!bmpVbuf.AllocBuf(header_size + aligned_line_size * info->height)) goto END;
	bmi = (BITMAPINFO *)bmpVbuf.Buf();

	bmi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biSizeImage   = aligned_line_size * info->height;
	bmi->bmiHeader.biWidth       = info->width;
	bmi->bmiHeader.biHeight      = -(int)info->height;
	bmi->bmiHeader.biPlanes      = 1;
	bmi->bmiHeader.biCompression = BI_RGB;

	if (info->color_type == PNG_COLOR_TYPE_PALETTE) {
		bmi->bmiHeader.biBitCount = info->bit_depth;
		bmi->bmiHeader.biClrUsed = info->num_palette;
		for (int i=0; i < info->num_palette; i++) {
			bmi->bmiColors[i].rgbRed	= info->palette[i].red;
			bmi->bmiColors[i].rgbGreen	= info->palette[i].green;
			bmi->bmiColors[i].rgbBlue	= info->palette[i].blue;
		}
	}
	else  {
		bmi->bmiHeader.biBitCount = info->bit_depth * info->channels;
		if (info->channels == 4) {
			bmi->bmiHeader.biSize	= sizeof(BITMAPV5HEADER);
			BITMAPV5HEADER *bm5		= (BITMAPV5HEADER*)bmi;
			bm5->bV5Compression		= BI_BITFIELDS;
			bm5->bV5RedMask			= 0x00FF0000;
			bm5->bV5GreenMask		= 0x0000FF00;
			bm5->bV5BlueMask		= 0x000000FF;
			bm5->bV5AlphaMask		= 0xFF000000;
		}
	}

	if (!(row = png_get_rows(png, info))) goto END;

	data = bmpVbuf.Buf() + header_size;
	u_int i;
	for (i=0; i < info->height; i++) {
		memcpy(data + aligned_line_size * i, row[i], line_size);
	}

	if (!(hDc = ::GetDC(hWnd))) goto END;
	hBmp = ::CreateDIBitmap(hDc, (BITMAPINFOHEADER *)bmi, CBM_INIT, data, bmi, DIB_RGB_COLORS);
	if (hDc) ::ReleaseDC(hWnd, hDc);

END:
	png_destroy_read_struct(&png, &info, 0);
	return	hBmp;
}

BITMAPINFO *BmpHandleToInfo(HBITMAP hBmp, int *size)
{
	BITMAP		bmp;
	BITMAPINFO	*bmi;
	int			palette, header_size, data_size;
	HWND		hWnd = ::GetDesktopWindow();
	HDC			hDc;

	if (!::GetObject(hBmp, sizeof(bmp), &bmp)) return NULL;

	data_size   = (bmp.bmWidth * ALIGN_SIZE(bmp.bmBitsPixel, 8) / 8) * bmp.bmHeight;
	palette     = bmp.bmBitsPixel <= 8 ? 1 << bmp.bmBitsPixel : 0;
	header_size = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * palette;
	*size		= header_size + data_size;

	if (!(bmi = (BITMAPINFO *)malloc(*size))) return NULL;

	memset(bmi, 0, sizeof(BITMAPINFO));
	bmi->bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth        = bmp.bmWidth;
	bmi->bmiHeader.biHeight       = bmp.bmHeight;
	bmi->bmiHeader.biPlanes       = 1;
	bmi->bmiHeader.biBitCount     = bmp.bmBitsPixel;
	bmi->bmiHeader.biCompression  = BI_RGB;
	bmi->bmiHeader.biClrUsed      = palette;
	bmi->bmiHeader.biClrImportant = palette;

	if (!(hDc = ::GetDC(hWnd)) ||
		!::GetDIBits(hDc, hBmp, 0, bmp.bmHeight, (char *)bmi + header_size, bmi, DIB_RGB_COLORS)) {
		free(bmi);
		bmi = NULL;
	}
	if (hDc) ::ReleaseDC(hWnd, hDc);

#if 0
	static int counter = 0;
	BITMAPFILEHEADER	bfh;
	bfh.bfType = 0x4d42;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;

	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + header_size;
	bfh.bfSize    = data_size + bfh.bfOffBits;

	FILE *fp = fopen(Fmt("c:\\temp\\a%d.bmp", counter++), "wb");
	fwrite((void *)&bfh, 1, sizeof(bfh), fp);
	fwrite((void *)bmi, 1, *size, fp);
	fclose(fp);
#endif

	return	bmi;
}

HBITMAP BmpInfoToHandle(BITMAPINFO *bmi, int size)
{
	HBITMAP		hBmp;
	HWND		hWnd = ::GetDesktopWindow();
	HDC			hDc;
	int			header_size;

	if (size < sizeof(BITMAPINFOHEADER)) return NULL;

	if (!(hDc = ::GetDC(hWnd))) return NULL;

	header_size = bmi->bmiHeader.biSize + (bmi->bmiHeader.biClrUsed * sizeof(RGBQUAD));
	hBmp = ::CreateDIBitmap(hDc, (BITMAPINFOHEADER *)bmi, CBM_INIT,
								(char *)bmi + header_size, bmi, DIB_RGB_COLORS);

	if (hDc) ::ReleaseDC(hWnd, hDc);

	return	hBmp;
}

TAreaConfirmDlg::TAreaConfirmDlg(Cfg *_cfg, TImageWin *_parentWin)
	: TDlg(AREACONFIRM_DIALOG, _parentWin)
{
	parentWin	= _parentWin;
	cfg			= _cfg;
	useClip		= NULL;
	withSave	= NULL;
	color		= 0;
	hToolBar	= NULL;
}


/*
	画面キャプチャ用ツールバーウィンドウ
*/
BOOL TAreaConfirmDlg::Create(BOOL *_useClip, BOOL *_withSave)
{
	useClip = _useClip;
	withSave = _withSave;
	return	TDlg::Create();
}

#define MARKER_OFFSET    3000
#define MARKER_RED       (MARKER_OFFSET + 0)
#define MARKER_GREEN     (MARKER_OFFSET + 1)
#define MARKER_BLUE      (MARKER_OFFSET + 2)
#define MARKER_YELLOW    (MARKER_OFFSET + 3)
#define MARKER_UNDO      (MARKER_OFFSET + 4)
#define MARKER_TB_MAX    5

BOOL TAreaConfirmDlg::EvCreate(LPARAM lParam)
{
	POINT	pt;
	::GetCursorPos(&pt);
	FitMoveWindow(pt.x - 20, pt.y - 20);

	CheckDlgButton(CLIP_CHECK, *useClip);
	CheckDlgButton(SAVE_CHECK, *withSave);

	TBBUTTON tbb[MARKER_TB_MAX] = {{0}};

	for (int i=0; i < MARKER_TB_MAX; i++) {
		tbb[i].iBitmap   = i;
		tbb[i].idCommand = MARKER_OFFSET + i;
		tbb[i].fsState   = TBSTATE_ENABLED;
		tbb[i].fsStyle   = TBSTYLE_CHECKGROUP;
	};
	tbb[4].fsState  = 0;
	tbb[4].fsStyle  = TBSTYLE_BUTTON;

	hToolBar = CreateToolbarEx(hWnd, WS_CHILD|WS_VISIBLE|TBSTYLE_TOOLTIPS, AREA_TOOLBAR,
								MARKER_TB_MAX, TApp::GetInstance(), MARKERTB_BITMAP, tbb,
								MARKER_TB_MAX, 0, 0, 16, 16, sizeof(TBBUTTON));

	TBBUTTON tb = {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0};
	::SendMessage(hToolBar, TB_INSERTBUTTON, 4, (LPARAM)&tb);

	Show();

	return	TRUE;
}

BOOL TAreaConfirmDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
	case IDRETRY:
	case IDCANCEL:
		if (wID == IDOK) {
			if (useClip)  *useClip  = IsDlgButtonChecked(CLIP_CHECK);
			if (withSave) *withSave = IsDlgButtonChecked(SAVE_CHECK);
		}
		EndDialog(wID);
		parentWin->Notify(wID);
		break;

	case MARKER_UNDO:
		parentWin->PopDrawHist();
		Notify();
		break;

	default:
		if (wID >= MARKER_RED && wID <= MARKER_YELLOW) {
			switch (wID) {
			case MARKER_RED:	color = RGB(255,0,0);	break;
			case MARKER_GREEN:	color = RGB(0,255,0);	break;
			case MARKER_BLUE:	color = RGB(0,0,255);	break;
			case MARKER_YELLOW:	color = RGB(255,255,0);	break;
			}
			parentWin->SetMode(TRUE);
		}
		break;
	}
	return	TRUE;
}

BOOL TAreaConfirmDlg::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	if (ctlID == AREA_TOOLBAR) {
		if (pNmHdr->code == TBN_GETINFOTIPW) {
			NMTBGETINFOTIPW	*itip = (NMTBGETINFOTIPW *)pNmHdr;
			itip->pszText = NULL;

			switch (itip->iItem) {
			case MARKER_RED:	itip->pszText = GetLoadStrW(IDS_MARKER_RED);	break;
			case MARKER_GREEN:	itip->pszText = GetLoadStrW(IDS_MARKER_GREEN);	break;
			case MARKER_BLUE:	itip->pszText = GetLoadStrW(IDS_MARKER_BLUE);	break;
			case MARKER_YELLOW:	itip->pszText = GetLoadStrW(IDS_MARKER_YELLOW);	break;
			case MARKER_UNDO:	itip->pszText = GetLoadStrW(IDS_MARKER_UNDO);	break;
			}
			if (itip->pszText) itip->cchTextMax = (int)wcslen(itip->pszText);
			return	TRUE;
		}
	}
	return	FALSE;
}

void TAreaConfirmDlg::Notify()
{
	::SendMessage(hToolBar, TB_SETSTATE, MARKER_UNDO, parentWin->DrawHistNum() ? TBSTATE_ENABLED : 0);
}


/*
	画面キャプチャウィンドウ
*/
CursorMap TImageWin::cursorMap;
#define ARROW_ID	RGB(1,1,1) // dummy color
#define CROSS_ID	RGB(2,2,2) // dummy color

TImageWin::TImageWin(Cfg *_cfg, TSendDlg *_parent) : TWin(_parent), areaDlg(_cfg, this)
{
	parentWnd	= _parent;
	cfg			= _cfg;
	hSelfDc		= NULL;
	hSelfBmp	= NULL;
	useClip		= cfg->CaptureClip;
	withSave	= cfg->CaptureSave;
	memset(areaPts, 0, sizeof(areaPts));
	memset(&lastPts, 0, sizeof(lastPts));
}

TImageWin::~TImageWin()
{
}

BOOL TImageWin::Create()
{
	int	x  = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
	int	y  = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
	int	cx = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int	cy = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

	rect.left	= x;
	rect.top	= y;
	rect.right	= cx + rect.left;
	rect.bottom	= cy + rect.top;

	HWND	hDesktop = ::GetDesktopWindow();
	HDC		hDesktopDc = ::GetDC(hDesktop);

	hSelfDc  = ::CreateCompatibleDC(hDesktopDc);
	hSelfBmp = ::CreateCompatibleBitmap(hDesktopDc, cx, cy);

	HBITMAP	hOldBmp = (HBITMAP)::SelectObject(hSelfDc, hSelfBmp);
	::BitBlt(hSelfDc, 0, 0, cx, cy, hDesktopDc, x, y, SRCCOPY);
	::SelectObject(hSelfDc, hOldBmp);

	::ReleaseDC(hDesktop, hDesktopDc);

	if (cursorMap.empty()) {
		cursorMap[CROSS_ID]			= ::LoadCursor(NULL, (LPCSTR)IDC_CROSS);
		cursorMap[ARROW_ID]			= ::LoadCursor(NULL, (LPCSTR)IDC_ARROW);
		cursorMap[RGB(255,0,0)]		= ::LoadCursor(TApp::GetInstance(), (LPCSTR)RED_CUR);
		cursorMap[RGB(0,255,0)]		= ::LoadCursor(TApp::GetInstance(), (LPCSTR)GREEN_CUR);
		cursorMap[RGB(0,0,255)]		= ::LoadCursor(TApp::GetInstance(), (LPCSTR)BLUE_CUR);
		cursorMap[RGB(255,255,0)]	= ::LoadCursor(TApp::GetInstance(), (LPCSTR)YELLOW_CUR);
		TRegisterClassU8(IPMSG_CAPTURE_CLASS, CS_DBLCLKS);
	}

	return	TWin::Create(IPMSG_CAPTURE_CLASS, NULL, WS_POPUP|WS_DISABLED, 0);
}

BOOL TImageWin::EvCreate(LPARAM lParam)
{
	status = INIT;
	::SetTimer(hWnd, 100, 10, NULL);
	::SetClassLong(hWnd, GCL_HCURSOR, (LONG)cursorMap[CROSS_ID]);

	return	TRUE;
}

BOOL TImageWin::EvNcDestroy()
{
	if (hSelfDc) {
		::DeleteDC(hSelfDc);
		hSelfDc = NULL;
	}
	if (hSelfBmp) {
		::DeleteObject(hSelfBmp);
		hSelfBmp = NULL;
	}
	::SetClassLong(hWnd, GCL_HCURSOR, (LONG)cursorMap[ARROW_ID]);

	return	FALSE;
}

BOOL TImageWin::EvPaint()
{
	PAINTSTRUCT	ps;
	HDC		hDc = ::BeginPaint(hWnd, &ps);
	HBITMAP	hOldBmp = (HBITMAP)::SelectObject(hSelfDc, hSelfBmp);

	::BitBlt(hDc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hSelfDc, 0, 0, SRCCOPY);
	::SelectObject(hSelfDc, hOldBmp);
	::EndPaint(hWnd, &ps);

	if (status >= START && status <= DRAW_END) {
		DrawLines();
		DrawMarker();
	}

	return	TRUE;
}

BOOL TImageWin::EvTimer(WPARAM _timerID, TIMERPROC proc)
{
	KillTimer(hWnd, _timerID);
	SetWindowLong(GWL_STYLE, (GetWindowLong(GWL_STYLE) & ~WS_DISABLED)|WS_VISIBLE);
	SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW);
	InvalidateRect(NULL, TRUE);
	return	TRUE;
}

BOOL TImageWin::EvChar(WCHAR code, LPARAM keyData)
{
	Destroy();
	return	TRUE;
}

void RegularRect(RECT *rc)
{
	if (rc->left > rc->right) {
		long tmp = rc->left;
		rc->left = rc->right;
		rc->right = tmp;
	}
	if (rc->top > rc->bottom) {
		long tmp = rc->top;
		rc->top = rc->bottom;
		rc->bottom = tmp;
	}
}

BOOL TImageWin::DrawMarker(HDC hDc)
{
	if (status >= DRAW_INIT && status <= DRAW_END) {
		HDC		hDcSv = hDc;
		HRGN	hRgn = NULL;

		if (!hDc && !(hDc = ::GetDC(hWnd))) return FALSE;

		if (!hDcSv) {
			RECT rc = { areaPts[0].x, areaPts[0].y, lastPts.x, lastPts.y };
			RegularRect(&rc);

			hRgn = ::CreateRectRgn(rc.left, rc.top, rc.right+1, rc.bottom+1);
			::SelectClipRgn(hDc, hRgn);
		}
		for (VColPtsItr i=drawPts.begin(); i != drawPts.end(); i++) {
			if (i->pts.size() <= 1) continue;

			HPEN	hPen = ::CreatePen(PS_SOLID, cfg->MarkerThick, i->color);
			HPEN	hOldPen = (HPEN)::SelectObject(hDc, (HGDIOBJ)hPen);

			VPtsItr	vp = i->pts.begin();
			::MoveToEx(hDc, vp->x, vp->y, NULL);

			for (; vp != i->pts.end(); vp++) {
				::LineTo(hDc, vp->x, vp->y);
			}
			::SelectObject(hDc, hOldPen);
			::DeleteObject(hPen);
		}
		if (!hDcSv) {
			::SelectClipRgn(hDc, NULL);
			::DeleteObject(hRgn);
			::ReleaseDC(hWnd, hDc);
		}
	}
	return	TRUE;
}

BOOL TImageWin::DrawLines(POINTS *pts)
{
	HDC		hDc;
	POINTS	cur = pts ? *pts : status == END ? areaPts[1] : lastPts;

	if (!(hDc = ::GetDC(hWnd))) return FALSE;

	HPEN	hPen = ::CreatePen(PS_DOT, 0, RGB(255, 0, 0));
	HPEN	hOldPen = (HPEN)::SelectObject(hDc, (HGDIOBJ)hPen);

	if (pts && !IS_SAME_PTS(cur, areaPts[0])) {
		RECT rc = { areaPts[0].x, areaPts[0].y, lastPts.x, lastPts.y };
		RegularRect(&rc);
		::InflateRect(&rc, 1, 1);
		::InvalidateRect(hWnd, &rc, FALSE);
	}
	::MoveToEx(hDc, areaPts[0].x, areaPts[0].y, NULL);
	::LineTo(hDc, cur.x, areaPts[0].y);
	::LineTo(hDc, cur.x, cur.y);
	::LineTo(hDc, areaPts[0].x, cur.y);
	::LineTo(hDc, areaPts[0].x, areaPts[0].y);

	if (pts) lastPts = *pts;

	::SelectObject(hDc, hOldPen);
	::DeleteObject(hPen);

	::ReleaseDC(hWnd, hDc);

	return	TRUE;
}

BOOL TImageWin::EvMouseMove(UINT fwKeys, POINTS pts)
{
	if (status == START) {
		DrawLines(&pts);
	}
	else if (status == DRAW_START) {
		if (!IS_SAME_PTS(drawPts.back().pts.back(), pts)) {
			drawPts.back().pts.push_back(pts);
		}
		DrawMarker();
	}
	return	TRUE;
}

BOOL TImageWin::EventButton(UINT uMsg, int nHitTest, POINTS pts)
{
	if (status == INIT) {
		if (uMsg == WM_LBUTTONDOWN) {
			status = START;
			areaPts[0] = lastPts = pts;
		}
	}
	else if (status == START && !areaDlg.hWnd) {
		if (uMsg == WM_LBUTTONUP) {
			areaPts[1] = pts;
			if (IS_SAME_PTS(areaPts[0], areaPts[1])) {
				status = INIT;
			}
			else {
				status = END;
				areaDlg.Create(&useClip, &withSave);
			}
		}
	}
	else if (status == DRAW_INIT) {
		if (uMsg == WM_LBUTTONDOWN) {
			status = DRAW_START;
			drawPts.push_back(ColPts());
			drawPts.back().color = areaDlg.GetColor();
			drawPts.back().pts.push_back(pts);
			areaDlg.Notify();
		}
	}
	else if (status == DRAW_START) {
		if (uMsg == WM_LBUTTONUP) {
			status = DRAW_INIT;
			if (!IS_SAME_PTS(drawPts.back().pts.back(), pts)) {
				drawPts.back().pts.push_back(pts);
			}
			else if (drawPts.back().pts.size() <= 1) {
				drawPts.pop_back();
			}
		}
	}

	return	TRUE;
}

void TImageWin::Notify(int result)
{
	if (result == IDRETRY) {
		::SetClassLong(hWnd, GCL_HCURSOR, (LONG)cursorMap[CROSS_ID]);
		status = INIT;
		InvalidateRect(NULL, FALSE);
		drawPts.clear();
	}
	else if (result == IDCANCEL || result == IDOK) {
		Show(SW_HIDE);
		parentWnd->Show();
		if (result == IDOK) {
			::SetFocus(parentWnd->GetDlgItem(SEND_EDIT));
			CutImage(useClip, withSave);
		}
		Destroy();
	}
}

void TImageWin::SetMode(BOOL is_draw)
{
	if (is_draw) {
		if (status < DRAW_INIT) {
			status = DRAW_INIT;
			drawPts.clear();
		}
		COLORREF		color = areaDlg.GetColor();
		CursorMapItr	itr = cursorMap.find(color);
		if (itr != cursorMap.end()) {
			::SetClassLong(hWnd, GCL_HCURSOR, (LONG)itr->second);
		}
	}
	else {
		status = END;
		::SetClassLong(hWnd, GCL_HCURSOR, (LONG)cursorMap[ARROW_ID]);
	}
}

void TImageWin::PopDrawHist()
{
	if (!drawPts.empty()) {
		drawPts.pop_back();
		InvalidateRect(NULL, TRUE);
	}
}

BOOL TImageWin::CutImage(BOOL use_clip, BOOL with_save)
{
	RECT rc = { areaPts[0].x, areaPts[0].y, areaPts[1].x, areaPts[1].y };
	RegularRect(&rc);

	int	cx = rc.right  - rc.left + 1; // include right line pixel
	int	cy = rc.bottom - rc.top  + 1; // include bottom line pixel

	HWND	hDesktop = ::GetDesktopWindow();
	HDC		hDesktopDc = ::GetDC(hDesktop);

	HDC		hAreaDc  = ::CreateCompatibleDC(hDesktopDc);
	HBITMAP	hAreaBmp = ::CreateCompatibleBitmap(hDesktopDc, cx, cy);

	HBITMAP	hOldBmp = (HBITMAP)::SelectObject(hSelfDc, hSelfBmp);
	HBITMAP	hAreaOldBmp = (HBITMAP)::SelectObject(hAreaDc, hAreaBmp);

	DrawMarker(hSelfDc);
	::BitBlt(hAreaDc, 0, 0, cx, cy, hSelfDc, rc.left, rc.top, SRCCOPY);

	::SelectObject(hAreaDc, hAreaOldBmp);
	::SelectObject(hSelfDc, hOldBmp);

	if (use_clip && ::OpenClipboard(hWnd)) {
		::EmptyClipboard();
		::SetClipboardData(CF_BITMAP, hAreaBmp);
		::CloseClipboard();
	}

	parentWnd->InsertBitmapByHandle(hAreaBmp);

	if (with_save) {
		char	fname[MAX_PATH_U8] = "";
		OpenFileDlg	dlg(this->parent, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);

		if (dlg.Exec(fname, sizeof(fname), NULL, "PNG file(*.png)\0*.png\0\0",
						cfg->lastSaveDir, "png")) {
			DWORD	size = 0;
			VBuf	*buf = BmpHandleToPngByte(hAreaBmp);
			if (buf) {
				HANDLE	hFile = CreateFileU8(fname, GENERIC_WRITE,
											 FILE_SHARE_READ|FILE_SHARE_WRITE,
											 NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
				if (hFile != INVALID_HANDLE_VALUE) {
					WriteFile(hFile, buf->Buf(), buf->Size(), &size, 0);
					CloseHandle(hFile);
				}
				delete buf;
			}
		}
	}

	::DeleteObject(hAreaBmp);
	::DeleteDC(hAreaDc);
	::ReleaseDC(hDesktop, hDesktopDc);

	return	TRUE;
}

