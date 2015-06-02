static char *image_id = 
	"@(#)Copyright (C) H.Shirouzu 2011   image.cpp	Ver3.30";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Image
	Create					: 2011-07-24(Sun)
	Update					: 2011-07-31(Sun)
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
		int	grow_size = size - vbuf->RemainSize();
		if (!vbuf->Grow(grow_size)) return;
	}

	// 圧縮中にも、わずかにメッセージループを回して、フリーズっぽい状態を避ける
	TApp::Idle(100);

	memcpy(vbuf->Buf() + vbuf->UsedSize(), buf, size);
	vbuf->AddUsedSize(size);
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
	vbuf->AddUsedSize(size);
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

TImageWin::TImageWin(Cfg *_cfg, TSendDlg *_parent) : TWin(_parent)
{
	parentWnd	= _parent;
	cfg			= _cfg;
	hSelfDc		= NULL;
	hSelfBmp	= NULL;
	memset(area_pts, 0, sizeof(area_pts));
	memset(&last_pts, 0, sizeof(last_pts));
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

	static BOOL once = FALSE;
	if (!once) {
		TRegisterClassU8(IPMSG_CAPTURE_CLASS, CS_DBLCLKS, ::LoadIcon(TApp::GetInstance(),
						(LPCSTR)IPMSG_ICON), ::LoadCursor(NULL, IDC_CROSS));
		once = TRUE;
	}

	return	TWin::Create(IPMSG_CAPTURE_CLASS, NULL, WS_POPUP|WS_DISABLED, 0);
}

BOOL TImageWin::EvCreate(LPARAM lParam)
{
	status = INIT;
	::SetTimer(hWnd, 100, 10, NULL);
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

	if (status == START || status == END) DrawLines();

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

BOOL TImageWin::DrawLines(POINTS *pts)
{
	HDC		hDc;
	POINTS	cur = pts ? *pts : status == END ? area_pts[1] : last_pts;

	if ((hDc = ::GetDC(hWnd))) {
		HPEN	hPen = ::CreatePen(PS_DOT, 0, RGB(255, 0, 0));
		HPEN	hOldPen = (HPEN)::SelectObject(hDc, (HGDIOBJ)hPen);

		if (pts && !IS_SAME_PTS(cur, area_pts[0])) {
			RECT rc = { area_pts[0].x, area_pts[0].y, last_pts.x, last_pts.y };
			RegularRect(&rc);
			::InflateRect(&rc, 1, 1);
			::InvalidateRect(hWnd, &rc, FALSE);
		}
		::MoveToEx(hDc, area_pts[0].x, area_pts[0].y, NULL);
		::LineTo(hDc, cur.x, area_pts[0].y);
		::LineTo(hDc, cur.x, cur.y);
		::LineTo(hDc, area_pts[0].x, cur.y);
		::LineTo(hDc, area_pts[0].x, area_pts[0].y);

		if (pts) last_pts = *pts;

		::SelectObject(hDc, hOldPen);
		::DeleteObject(hPen);
		if (hDc) ::ReleaseDC(hWnd, hDc);
	}
	return	TRUE;
}

BOOL TImageWin::EvMouseMove(UINT fwKeys, POINTS pts)
{
	if (status == START) {
		DrawLines(&pts);
	}
	return	TRUE;
}

BOOL TImageWin::EventButton(UINT uMsg, int nHitTest, POINTS pts)
{
	if (status == INIT) {
		if (uMsg == WM_LBUTTONDOWN) {
			status = START;
			area_pts[0] = last_pts = pts;
		}
	}
	else if (status == START) {
		if (uMsg == WM_LBUTTONUP) {
			area_pts[1] = pts;
			if (IS_SAME_PTS(area_pts[0], area_pts[1])) {
				status = INIT;
			}
			else {
				status = END;
				BOOL	use_clip = cfg->CaptureClip;
				BOOL	with_save = cfg->CaptureSave;
				TAreaConfirmDlg	dlg(&use_clip, &with_save, this);

				int ret = dlg.Exec();

				if (ret == IDRETRY) {
					status = INIT;
					InvalidateRect(NULL, FALSE);
				}
				else {
					Show(SW_HIDE);
					parentWnd->Show();
					if (ret == IDOK) {
						::SetFocus(parentWnd->GetDlgItem(SEND_EDIT));
						CutImage(use_clip, with_save);
					}
					Destroy();
				}
			}
		}
	}
	return	TRUE;
}

BOOL TImageWin::CutImage(BOOL use_clip, BOOL with_save)
{
	RECT rc = { area_pts[0].x, area_pts[0].y, area_pts[1].x, area_pts[1].y };
	RegularRect(&rc);

	int	cx = rc.right  - rc.left + 1; // include right line pixel
	int	cy = rc.bottom - rc.top  + 1; // include bottom line pixel

	HWND	hDesktop = ::GetDesktopWindow();
	HDC		hDesktopDc = ::GetDC(hDesktop);

	HDC		hAreaDc  = ::CreateCompatibleDC(hDesktopDc);
	HBITMAP	hAreaBmp = ::CreateCompatibleBitmap(hDesktopDc, cx, cy);

	HBITMAP	hOldBmp = (HBITMAP)::SelectObject(hSelfDc, hSelfBmp);
	HBITMAP	hAreaOldBmp = (HBITMAP)::SelectObject(hAreaDc, hAreaBmp);

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
		char	fname[MAX_PATH] = "";
		OpenFileDlg	dlg(this->parent, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);

		if (dlg.Exec(fname, NULL, "PNG file(*.png)\0*.png\0\0", cfg->lastSaveDir, "png")) {
			DWORD	size = 0;
			VBuf	*buf = BmpHandleToPngByte(hAreaBmp);
			if (buf) {
				HANDLE	hFile = CreateFileU8(fname, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
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

TAreaConfirmDlg::TAreaConfirmDlg(BOOL *_useClip, BOOL *_withSave, TImageWin *_parentWin)
	: TDlg(AREACONFIRM_DIALOG, _parentWin)
{
	parentWin = _parentWin;
	useClip = _useClip;
	withSave = _withSave;
}

BOOL TAreaConfirmDlg::EvCreate(LPARAM lParam)
{
	POINT	pt;
	::GetCursorPos(&pt);
	FitMoveWindow(pt.x - 30, pt.y - 20);

	CheckDlgButton(CLIP_CHECK, *useClip);
	CheckDlgButton(SAVE_CHECK, *withSave);

	return	TRUE;
}

BOOL TAreaConfirmDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
	case IDRETRY:
	case IDCANCEL:
		if (wID == IDOK) {
			*useClip = IsDlgButtonChecked(CLIP_CHECK);
			*withSave = IsDlgButtonChecked(SAVE_CHECK);
		}
		EndDialog(wID);
		break;
	}
	return	TRUE;
}

