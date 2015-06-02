static char *richedit_id = 
	"@(#)Copyright (C) H.Shirouzu 2011   richedit.cpp	Ver3.21";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Rich Edit Control and PNG-BMP convert
	Create					: 2011-05-03(Tue)
	Update					: 2011-06-27(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include "blowfish.h"
#include "../external/libpng/pnginfo.h"

#include <objidl.h>
#include <ole2.h>
#include <commctrl.h>
#include <richedit.h>
#include <oledlg.h>

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
	HWND		hWnd = GetDesktopWindow();
	HDC			hDc = NULL;
	VBuf		bmpVbuf;
	png_struct	*png  = NULL;
	png_info	*info = NULL;
	png_color_8	sbit;
	png_byte	**lines = NULL;
	VBuf		*vbuf = NULL, *ret = NULL;

	if (!GetObject(hBmp, sizeof(bmp), &bmp)) return NULL;

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

	if (!(hDc = GetDC(hWnd)) ||
		!GetDIBits(hDc, hBmp, 0, bmp.bmHeight, (char *)bmi + header_size, bmi, DIB_RGB_COLORS)) {
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
	if (hDc) ReleaseDC(hWnd, hDc);
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
	HWND		hWnd = GetDesktopWindow();
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

	if (!(hDc = GetDC(hWnd))) goto END;
	hBmp = CreateDIBitmap(hDc, (BITMAPINFOHEADER *)bmi, CBM_INIT, data, bmi, DIB_RGB_COLORS);
	if (hDc) ReleaseDC(hWnd, hDc);

END:
	png_destroy_read_struct(&png, &info, 0);
	return	hBmp;
}

BITMAPINFO *BmpHandleToInfo(HBITMAP hBmp, int *size)
{
	BITMAP		bmp;
	BITMAPINFO	*bmi;
	int			palette, header_size, data_size;
	HWND		hWnd = GetDesktopWindow();
	HDC			hDc;

	if (!GetObject(hBmp, sizeof(bmp), &bmp)) return NULL;

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

	if (!(hDc = GetDC(hWnd)) ||
		!GetDIBits(hDc, hBmp, 0, bmp.bmHeight, (char *)bmi + header_size, bmi, DIB_RGB_COLORS)) {
		free(bmi);
		bmi = NULL;
	}
	if (hDc) ReleaseDC(hWnd, hDc);

#if 0
	static int counter = 0;
	BITMAPFILEHEADER	bfh;
	bfh.bfType = 0x4d42;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;

	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + header_size;
	bfh.bfSize    = data_size + bfh.bfOffBits;

	FILE *fp = fopen(FmtStr("c:\\temp\\a%d.bmp", counter++), "wb");
	fwrite((void *)&bfh, 1, sizeof(bfh), fp);
	fwrite((void *)bmi, 1, *size, fp);
	fclose(fp);
#endif

	return	bmi;
}

HBITMAP BmpInfoToHandle(BITMAPINFO *bmi, int size)
{
	HBITMAP		hBmp;
	HWND		hWnd = GetDesktopWindow();
	HDC			hDc;
	int			header_size;

	if (size < sizeof(BITMAPINFOHEADER)) return NULL;

	if (!(hDc = GetDC(hWnd))) return NULL;

	header_size = bmi->bmiHeader.biSize + (bmi->bmiHeader.biClrUsed * sizeof(RGBQUAD));
	hBmp = CreateDIBitmap(hDc, (BITMAPINFOHEADER *)bmi, CBM_INIT,
								(char *)bmi + header_size, bmi, DIB_RGB_COLORS);

	if (hDc) ReleaseDC(hWnd, hDc);

	return	hBmp;
}


class TDataObject : IDataObject {
private:
	ULONG		refCnt;
	HBITMAP		hBmp;
	FORMATETC	fe;

public:
	TDataObject() { refCnt = 0; memset(&fe, 0, sizeof(fe)); }
	~TDataObject() {}

	STDMETHODIMP QueryInterface(REFIID iid, void **obj);
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
	STDMETHODIMP GetData(FORMATETC *_fe, STGMEDIUM *sm);
	STDMETHODIMP SetData(FORMATETC *_fe , STGMEDIUM *sm, BOOL fRelease) { return E_NOTIMPL; }
	STDMETHODIMP GetDataHere(FORMATETC *_fe, STGMEDIUM *sm) { return E_NOTIMPL; }
	STDMETHODIMP QueryGetData(FORMATETC *_fe) { return E_NOTIMPL; }
	STDMETHODIMP GetCanonicalFormatEtc(FORMATETC *_fei ,FORMATETC *_feo) { return E_NOTIMPL; }
	STDMETHODIMP EnumFormatEtc(DWORD direc, IEnumFORMATETC **ife) { return E_NOTIMPL; }
	STDMETHODIMP DUnadvise(DWORD conn) { return E_NOTIMPL; }
	STDMETHODIMP EnumDAdvise(IEnumSTATDATA **nav) { return E_NOTIMPL; }
	STDMETHODIMP DAdvise(FORMATETC *_fe, DWORD advf, IAdviseSink *as, DWORD *conn) {
					return E_NOTIMPL; }

	void InsertBitmap(IRichEditOle *richOle, HBITMAP hBmp);
};

HRESULT TDataObject::QueryInterface(REFIID riid, LPVOID* ppv)
{
	*ppv = NULL;

	if (IID_IUnknown == riid || IID_IDataObject == riid) {
		*ppv = this;
	}
	else return ResultFromScode(E_NOINTERFACE);

	AddRef();
	return S_OK;
}

ULONG TDataObject::AddRef(void)
{
	refCnt++;
	return refCnt;
}

ULONG TDataObject::Release(void)
{
	int	ret = --refCnt;

	if (0 == refCnt) {
		delete this;
	}
	return ret;
}

HRESULT TDataObject::GetData(FORMATETC *_fe, STGMEDIUM *sm)
{
	HBITMAP		hDupBmp = (HBITMAP)::OleDuplicateData(hBmp, CF_BITMAP, NULL);
	if (!hDupBmp) return E_HANDLE;

	sm->tymed			= TYMED_GDI;
	sm->hBitmap			= hDupBmp;
	sm->pUnkForRelease	= NULL;

	return S_OK;
}

void TDataObject::InsertBitmap(IRichEditOle *richOle, HBITMAP _hBmp)
{
	IOleClientSite	*ocs    = NULL;
	IStorage		*is     = NULL;
	LPLOCKBYTES		lb      = NULL;
	IOleObject		*oleObj = NULL;
	REOBJECT		reobj;
	CLSID			clsid;

	hBmp		= _hBmp;
	fe.cfFormat	= CF_BITMAP;
	fe.ptd		= NULL;
	fe.dwAspect	= DVASPECT_CONTENT;
	fe.lindex	= -1;
	fe.tymed	= TYMED_GDI;

	richOle->GetClientSite(&ocs);

	::CreateILockBytesOnHGlobal(NULL, TRUE, &lb);
	if (!ocs || !lb) goto END;

	::StgCreateDocfileOnILockBytes(lb, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &is);
	if (!is) goto END;

	::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT, &fe, ocs, is,
		(void **)&oleObj);
	if (!oleObj) goto END;
	::OleSetContainedObject(oleObj, TRUE);

	memset(&reobj, 0, sizeof(REOBJECT));
	reobj.cbStruct = sizeof(REOBJECT);
	oleObj->GetUserClassID(&clsid);

	reobj.clsid		= clsid;
	reobj.cp		= REO_CP_SELECTION;
	reobj.dvaspect	= DVASPECT_CONTENT;
	reobj.poleobj	= oleObj;
	reobj.polesite	= ocs;
	reobj.pstg		= is;

	richOle->InsertObject(&reobj);

END:
	if (oleObj)	oleObj->Release();
	if (lb)		lb->Release();
	if (is)		is->Release();
	if (ocs)	ocs->Release();
}


class TRichEditOleCallback : public IRichEditOleCallback
{
protected:
	ULONG		refCnt;
	Cfg			*cfg;
	TEditSub	*editWnd;
	TWin		*parentWnd;

public:
	TRichEditOleCallback(Cfg *_cfg, TEditSub *_editWnd, TWin *_parentWnd) {
		cfg       = _cfg;
		editWnd   = _editWnd;
		parentWnd = _parentWnd;
		refCnt    = 0;
		pasteMode = 0;
		memset(enabled, 0, sizeof(enabled));
	}
	virtual ~TRichEditOleCallback() {
	};

	int			pasteMode;
	u_short		enabled[2];

public:
	STDMETHODIMP 		 QueryInterface(REFIID riid, LPVOID* ppv);
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);

	STDMETHODIMP GetNewStorage(LPSTORAGE* ppStg);
	STDMETHODIMP GetInPlaceContext(LPOLEINPLACEFRAME* ppFrame, LPOLEINPLACEUIWINDOW* ppDoc,
									LPOLEINPLACEFRAMEINFO pFrameInfo);
	STDMETHODIMP ShowContainerUI(BOOL fShow);
	STDMETHODIMP QueryInsertObject(LPCLSID pclsid, LPSTORAGE pStg, LONG cp);
	STDMETHODIMP DeleteObject(LPOLEOBJECT pOleObj);
	STDMETHODIMP QueryAcceptData(LPDATAOBJECT pDataObj, CLIPFORMAT* pcfFormat, DWORD reco,
								BOOL fReally, HGLOBAL hMetaPict);
	STDMETHODIMP ContextSensitiveHelp(BOOL fEnterMode);
	STDMETHODIMP GetClipboardData(CHARRANGE* pchrg, DWORD reco, LPDATAOBJECT* ppDataObject);
	STDMETHODIMP GetDragDropEffect(BOOL fDrag, DWORD grfKeyState, LPDWORD pdwEffect);
	STDMETHODIMP GetContextMenu(WORD seltype, LPOLEOBJECT pOleObj, CHARRANGE* pchrg,
								HMENU* phMenu);
};


HRESULT TRichEditOleCallback::QueryInterface(REFIID riid, LPVOID* ppv)
{
	*ppv = NULL;

	if (IID_IUnknown == riid || IID_IRichEditOleCallback == riid) {
		*ppv = this;
	}
	else return ResultFromScode(E_NOINTERFACE);

	((LPUNKNOWN)*ppv)->AddRef();

	return NOERROR;
}

ULONG TRichEditOleCallback::AddRef(void)
{
	refCnt++;
	return refCnt;
}

ULONG TRichEditOleCallback::Release(void)
{
	int	ret = --refCnt;

	if (0 == refCnt) {
		delete this;
	}

	return ret;
}

HRESULT TRichEditOleCallback::GetNewStorage(LPSTORAGE* ppStg)
{
	if (!ppStg) return E_INVALIDARG;

	*ppStg = NULL;

	LPLOCKBYTES	lb;
	HRESULT hr = ::CreateILockBytesOnHGlobal(NULL, TRUE, &lb);
	if (FAILED(hr))	return hr;

	hr = ::StgCreateDocfileOnILockBytes(lb, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE,
			0, ppStg);
	lb->Release();

	return hr;
}


HRESULT TRichEditOleCallback::GetInPlaceContext(LPOLEINPLACEFRAME* ppFrame,
			LPOLEINPLACEUIWINDOW* ppDoc, LPOLEINPLACEFRAMEINFO pFrameInfo)
{
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::ShowContainerUI(BOOL fShow)
{
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::QueryInsertObject(LPCLSID pclsid, LPSTORAGE pStg, LONG cp)
{
	return S_OK;
}


HRESULT TRichEditOleCallback::DeleteObject(LPOLEOBJECT obj)
{
	return S_OK;
}

HRESULT TRichEditOleCallback::QueryAcceptData(LPDATAOBJECT dataObj, CLIPFORMAT *cf, DWORD reco,
			BOOL fReally, HGLOBAL hMetaPict)
{
	HRESULT		ret = E_FAIL;
	FORMATETC	fe;
	STGMEDIUM	sm;
	WCHAR		*delay_text = NULL;
	HBITMAP		hDelayBmp = NULL;
	int			idx = 0;

	memset(enabled, 0, sizeof(enabled));

	memset(&fe, 0, sizeof(fe));
	fe.cfFormat = CF_HDROP;
	fe.dwAspect = DVASPECT_CONTENT;
	fe.lindex = -1;
	fe.tymed = TYMED_HGLOBAL;
	if (SUCCEEDED(dataObj->QueryGetData(&fe))) return ret;

	IEnumFORMATETC	*enum_fe = NULL;
	DWORD			num;

	if (FAILED(dataObj->EnumFormatEtc(DATADIR_GET, &enum_fe))) return S_FALSE;

	while (SUCCEEDED(enum_fe->Next(num=1, &fe, &num)) && num == 1) {
//		editWnd->MessageBoxU8(FmtStr("cf=%d",fe.cfFormat));
		switch (fe.cfFormat) {
		case CF_TEXT:
			if (idx < 2 && enabled[idx] == 0 && (idx == 0 || enabled[0] != CF_TEXT)) {
				enabled[idx++] = CF_TEXT;
			}
			if (!fReally) break;

			if (!delay_text) {
				fe.cfFormat = CF_UNICODETEXT;
				if (SUCCEEDED(dataObj->GetData(&fe, &sm))) {
					WCHAR	*org_str = (WCHAR *)::GlobalLock(sm.hGlobal);
					if (org_str) { // RichEdit は paste で末尾の改行が消えるので暫定 hack
						delay_text = wcsdup(org_str);
						::GlobalUnlock(org_str);
					}
					::ReleaseStgMedium(&sm);
				}
			}
			break;

		case CF_BITMAP:
		case CF_DIBV5:
		case CF_DIB:
			if (cfg->ClipMode & CLIP_ENABLE) {
				if (idx < 2 && enabled[idx] == 0 && (idx == 0 || enabled[0] == CF_TEXT)) {
					enabled[idx++] = fe.cfFormat;
				}
				if (!fReally) break;

				if (editWnd->GetImageNum() < cfg->ClipMax) {
					if (!hDelayBmp && SUCCEEDED(dataObj->GetData(&fe, &sm))) {
						if (fe.cfFormat == CF_BITMAP) {
							hDelayBmp = sm.hBitmap; // WM_DELAY_BITMAP で開放
						} else {
							BITMAPINFO	*bmi = (BITMAPINFO *)::GlobalLock(sm.hGlobal);
							if (bmi) {
								hDelayBmp = BmpInfoToHandle(bmi, sizeof(BITMAPINFOHEADER));
								::GlobalUnlock(sm.hGlobal);
							}
							::ReleaseStgMedium(&sm);
						}
					}
				}
				else {
					editWnd->MessageBoxU8(GetLoadStrU8(IDS_TOOMANY_CLIP));
				}
			}
			break;
		}
	}
	enum_fe->Release();

	if (!fReally && idx > 0) {
		ret = S_OK;
		*cf = enabled[0];
	}

	if (fReally && ret != S_OK) {
		if (idx == 2 && (pasteMode == 1 || pasteMode == 2 && enabled[0] == CF_TEXT)) {
			u_short save = enabled[0];
			enabled[0]   = enabled[1];
			enabled[1]   = save;
		}

		if (delay_text && (enabled[0] == CF_TEXT || !hDelayBmp)) {
			editWnd->PostMessage(WM_DELAY_PASTE, 0, (LPARAM)delay_text);
			if (hDelayBmp) ::DeleteObject(hDelayBmp);
		}
		else if (hDelayBmp) {
			editWnd->PostMessage(WM_DELAY_BITMAP, 0, (LPARAM)hDelayBmp);
			if (delay_text) free(delay_text);
		}
	}
	pasteMode = 0;

	return ret;
}

HRESULT TRichEditOleCallback::ContextSensitiveHelp(BOOL fEnterMode)
{
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::GetClipboardData(CHARRANGE* pchrg, DWORD reco,
			LPDATAOBJECT* ppDataObject)

{
	*ppDataObject = NULL;
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::GetDragDropEffect(BOOL fDrag, DWORD grfKeyState, LPDWORD pdwEffect)
{
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::GetContextMenu(WORD seltype, LPOLEOBJECT pOleObj, CHARRANGE* pchrg,
			HMENU* phMenu)
{
	*phMenu = editWnd->CreatePopupMenu();
	return S_OK;
}

/*
	edit control の subclass化
*/
TEditSub::TEditSub(Cfg *_cfg, TWin *_parent) : TSubClassCtl(_parent)
{
	cfg = _cfg;
	cb = NULL;
	richOle = NULL;
}

TEditSub::~TEditSub()
{
	if (cb) cb->Release();
	if (richOle) richOle->Release();
}

#define EM_SETEVENTMASK			(WM_USER + 69)
#define EM_GETEVENTMASK			(WM_USER + 59)
#define ENM_LINK				0x04000000

BOOL TEditSub::AttachWnd(HWND _hWnd)
{
	if (!TSubClassCtl::AttachWnd(_hWnd))
		return	FALSE;

	if ((cb = new TRichEditOleCallback(cfg, this, this->parent))) {
		cb->AddRef();
		SendMessage(EM_SETOLECALLBACK, 0, (LPARAM)cb);
	}

	richOle = NULL;
	SendMessage(EM_GETOLEINTERFACE, 0, (LPARAM)&richOle);

	DWORD	evMask = SendMessage(EM_GETEVENTMASK, 0, 0) | ENM_LINK;
	SendMessage(EM_SETEVENTMASK, 0, evMask); 
	dblClicked = FALSE;
	return	TRUE;
}

HMENU TEditSub::CreatePopupMenu()
{
	HMENU	hMenu = ::CreatePopupMenu();
	BOOL	is_readonly = GetWindowLong(GWL_STYLE) & ES_READONLY;
	BOOL	is_paste = SendMessage(EM_CANPASTE, 0, 0);

	AppendMenuU8(hMenu, MF_STRING|((is_readonly || !SendMessage(EM_CANUNDO, 0, 0)) ?
				MF_DISABLED|MF_GRAYED : 0), WM_UNDO, GetLoadStrU8(IDS_UNDO));
	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING|(is_readonly ? MF_DISABLED|MF_GRAYED : 0), WM_CUT,
				GetLoadStrU8(IDS_CUT));
	AppendMenuU8(hMenu, MF_STRING, WM_COPY, GetLoadStrU8(IDS_COPY));
	AppendMenuU8(hMenu, MF_STRING|((is_readonly || !is_paste) ?
				MF_DISABLED|MF_GRAYED : 0), WM_PASTE, GetLoadStrU8(IDS_PASTE));
	if (cb && cb->enabled[1] && !is_readonly && is_paste) {
		AppendMenuU8(hMenu, MF_STRING, WM_PASTE_REV,
				GetLoadStrU8(cb->enabled[1] == CF_TEXT ? IDS_PASTE_TEXT : IDS_PASTE_BMP));
	}
	AppendMenuU8(hMenu, MF_STRING|(is_readonly ? MF_DISABLED|MF_GRAYED : 0), WM_CLEAR,
				GetLoadStrU8(IDS_DELETE));
	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING, EM_SETSEL, GetLoadStrU8(IDS_SELECTALL));

	return	hMenu;
}

BOOL TEditSub::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_CLEAR:
		SendMessage(wID, 0, 0);
		return	TRUE;

	case WM_PASTE_REV:
		if (cb) {
			cb->pasteMode = 1;
		}
		SendMessage(WM_PASTE, 0, 0);
		return	TRUE;

	case WM_PASTE_IMAGE:
		if (cb) {
			cb->pasteMode = 2;
		}
		SendMessage(WM_PASTE, 0, 0);
		return	TRUE;

	case EM_SETSEL:
		SendMessage(wID, 0, -1);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TEditSub::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_LBUTTONDBLCLK:
//		PostMessage(WM_EDIT_DBLCLK, 0, 0);
		dblClicked = TRUE;
		break;

	case WM_LBUTTONUP:
		if (dblClicked) {
			PostMessage(WM_EDIT_DBLCLK, 0, 0);
			dblClicked = FALSE;
		}
		break;
	}
	return	FALSE;
}

BOOL TEditSub::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	return	FALSE;
}

BOOL TEditSub::EvContextMenu(HWND childWnd, POINTS pos)
{
	HMENU	hMenu = CreatePopupMenu();

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	return	TRUE;
}

BOOL TEditSub::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_EDIT_DBLCLK:
		{
			CHARRANGE	cr;
			LONG		&start	= cr.cpMin;
			LONG		&end	= cr.cpMax;
	//		LONG		start = 0;
	//		LONG		end	= 0;

			SendMessageW(EM_EXGETSEL, 0, (LPARAM)&cr);
	//		SendMessageW(EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

			if (start == end || end >= MAX_UDPBUF) return TRUE;

			WCHAR	*wbuf = new WCHAR[MAX_UDPBUF];
			UrlObj	*obj = NULL;
			BOOL	modify = FALSE;
			WCHAR	*terminate_chars = L"\r\n\t \x3000";	// \x3000 ... 全角空白

			int max_len = ExGetText(wbuf, MAX_UDPBUF, GT_DEFAULT, 1200);	// 1200 == UNICODE
	//		int max_len = ::GetWindowTextW(hWnd, wbuf, MAX_UDPBUF);

			for ( ; start > 0 && !wcschr(terminate_chars, wbuf[start-1]); start--) {
				modify = TRUE;
			}
			if (!wcschr(terminate_chars, wbuf[end-1])) {
				for ( ; end < max_len && !wcschr(terminate_chars, wbuf[end]); end++) {
					modify = TRUE;
				}
			}
			if (modify) {
				SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr);
	//			SendMessageW(EM_SETSEL, start, end);
			}
			memmove(wbuf, wbuf + start, (end - start) * sizeof(WCHAR));
			wbuf[end - start] = 0;
			char	*url_ptr;
			char	*u8buf = WtoU8(wbuf, TRUE);

			if ((url_ptr = strstr(u8buf, URL_STR))) {
				char	proto[MAX_NAMEBUF];

				strncpyz(proto, u8buf, min(url_ptr - u8buf + 1, sizeof(proto)));
				for (int i=0; proto[i]; i++) {
					if ((obj = SearchUrlObj(&cfg->urlList, proto + i))) {
						url_ptr = u8buf + i;
						break;
					}
				}
			}
			if (obj && *obj->program) {
				if ((int)ShellExecuteU8(NULL, NULL, obj->program, url_ptr ? url_ptr : u8buf, NULL, SW_SHOW) <= WINEXEC_ERR_MAX)
					MessageBoxU8(obj->program, GetLoadStrU8(IDS_CANTEXEC), MB_OK|MB_ICONINFORMATION);
			}
			else if (!url_ptr && cfg->ShellExec || url_ptr && cfg->DefaultUrl) {
				ShellExecuteU8(NULL, NULL, url_ptr ? url_ptr : u8buf, NULL, NULL, SW_SHOW);
			}
			delete [] u8buf;
			delete	[] wbuf;
			return	TRUE;
		}

	case WM_DELAY_PASTE:
		if (lParam) {
			WCHAR	*delay_str = (WCHAR *)lParam;
			SendMessageW(EM_REPLACESEL, 1, (LPARAM)delay_str);
			free(delay_str);
		}
		return	TRUE;

	case WM_DELAY_BITMAP:
		if (lParam) {
			HBITMAP	hBitmap = (HBITMAP)lParam;
			InsertBitmapByHandle(hBitmap, -1);
			::DeleteObject(hBitmap);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TEditSub::SetFont(LOGFONT	*lf, BOOL dualFont)
{
	CHARFORMAT	cf = { sizeof(CHARFORMAT) };

	SendMessage(EM_GETCHARFORMAT, 0, (LPARAM)&cf);
//	cf.dwMask |= CFM_BOLD|CFM_ITALIC|CFM_UNDERLINE|CFM_STRIKEOUT|CFM_PROTECTED|CFM_SIZE|CFM_COLOR|CFM_FACE|CFM_OFFSET|CFM_CHARSET;
	cf.dwEffects |= (lf->lfItalic ? CFE_ITALIC : 0) | (lf->lfUnderline ? CFE_UNDERLINE : 0) | (lf->lfStrikeOut ? CFE_STRIKEOUT : 0);

	HDC hDc = ::GetDC(hWnd);
	cf.yHeight = abs(1440 * lf->lfHeight / ::GetDeviceCaps(hDc, LOGPIXELSY));
	::ReleaseDC(hWnd, hDc);

	cf.bCharSet = lf->lfCharSet;
	cf.bPitchAndFamily = lf->lfPitchAndFamily;
	strcpy(cf.szFaceName, lf->lfFaceName);
	SendMessage(EM_SETCHARFORMAT, 0, (LPARAM)&cf);

	DWORD	langOpts = SendMessage(EM_GETLANGOPTIONS, 0, 0);
	langOpts = dualFont ? (langOpts | IMF_DUALFONT) : (langOpts & ~IMF_DUALFONT);
	SendMessage(EM_SETLANGOPTIONS, 0, langOpts);
	return	TRUE;
}

int TEditSub::ExGetText(void *buf, int max_len, DWORD flags, UINT codepage)
{
	GETTEXTEX	ge;

	memset(&ge, 0, sizeof(ge));
	ge.cb       = max_len;
	ge.flags    = flags;
	ge.codepage = codepage;

	int	ret = SendMessageW(EM_GETTEXTEX, (WPARAM)&ge, (LPARAM)buf);

	return	ret;
}

int TEditSub::GetTextUTF8(char *buf, int max_len, BOOL force_select)
{
	int			max_wlen = SendMessageW(WM_GETTEXTLENGTH, 0, 0) + 1;
	DynBuf		cbuf(max_len);
	VBuf		wbuf(max_wlen * 3); // protect for EM_GETSELTEXT
	BOOL		change_select = FALSE;
	int			image_num = GetImageNum();
	CHARRANGE	cr;

	SendMessageW(EM_EXGETSEL, 0, (LPARAM)&cr);

	if (cr.cpMin == cr.cpMax && image_num > 0 || force_select) {
		SendMessageW(EM_SETSEL, 0, (LPARAM)-1);
		change_select = TRUE;
	}
	if (image_num == 0 && cr.cpMin == cr.cpMax) {
		SendMessageW(WM_GETTEXT, max_wlen, (LPARAM)wbuf.Buf());
	}
	else {
		SendMessageW(EM_GETSELTEXT, 0, (LPARAM)wbuf.Buf());
	}
	if (change_select) {
		SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr);
	}

	WtoU8((WCHAR *)wbuf.Buf(), cbuf.Buf(), max_len);
	u_char	*s   = (u_char *)cbuf.Buf();
	char	*d   = buf;
	char	*max = buf + max_len - 2;

	while (*s && d < max) {
		if (*s == 0xef && *(s+1) == 0xbf && *(s+2) == 0xbc) { // OLE object
			s += 3;
			continue;
		}
		if ((*d = *s) == '\r' && *(s+1) != '\n') {
			*++d = '\n';
		}
		s++, d++;
	}
	*d = 0;
//	for (int i=0; buf[i]; i++)
//		Debug("%02x ", (unsigned char)buf[i]);
//	Debug("\n");

	return	d - buf;
}

struct RichStreamObj {
	BYTE		*buf;
	int			max_len;
	int			offset;
};

DWORD CALLBACK RichStreamCallback(DWORD_PTR dwCookie, BYTE *buf, LONG cb, LONG *pcb)
{
	RichStreamObj *obj = (RichStreamObj *)dwCookie;
	int remain = obj->max_len - obj->offset;

	if (remain == 0) return 1;
	if (remain < cb) cb = remain;
	memcpy(obj->buf + obj->offset, buf, cb);
	*pcb = cb;
	obj->offset += cb;
	return 0;
}

int TEditSub::GetStreamText(void *buf, int max_len, DWORD flags)
{
	RichStreamObj	obj = { (BYTE *)buf, max_len, 0 };
	EDITSTREAM		es = { (DWORD_PTR)&obj, 0, RichStreamCallback };

	return	SendMessageW(EM_STREAMOUT, flags, (LPARAM)&es);
}

int TEditSub::ExSetText(const void *buf, int max_len, DWORD flags, UINT codepage)
{
	SETTEXTEX	se;

	se.flags    = flags;
	se.codepage = codepage;

	return	SendMessageW(EM_SETTEXTEX, (WPARAM)&se, (LPARAM)buf);
}

void TEditSub::InsertBitmapByHandle(HBITMAP hBmp, int pos)
{
	DWORD	start=0, end=0;

	if (pos >= 0) {
		SendMessageW(EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
		SendMessageW(EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
	}

	TDataObject	*tdo =  new TDataObject;

	if (tdo) {
		tdo->AddRef();
		if (richOle) {
			tdo->InsertBitmap(richOle, hBmp);
		}
		tdo->Release();
	}

	if (pos >= 0) {
		SendMessageW(EM_SETSEL, (WPARAM)start, (LPARAM)end);
	}
}

void TEditSub::InsertBitmap(BITMAPINFO	*bmi, int size, int pos)
{
	HBITMAP	hBmp = BmpInfoToHandle(bmi, size);

	if (hBmp) {
		InsertBitmapByHandle(hBmp, pos);
		DeleteObject(hBmp);
	}
}

BOOL TEditSub::InsertPng(VBuf *vbuf, int pos)
{
	HBITMAP	hBmp = PngByteToBmpHandle(vbuf);

	if (!hBmp) return FALSE;

	InsertBitmapByHandle(hBmp, pos);
	DeleteObject(hBmp);
	return	TRUE;
}

VBuf *TEditSub::GetPngByte(int idx, int *pos)
{
	if (!richOle) return NULL;

	VBuf			*buf     = NULL;
	LPDATAOBJECT	dobj     = NULL;
	REOBJECT		reobj;

	memset(&reobj, 0, sizeof(REOBJECT));
	reobj.cbStruct = sizeof(REOBJECT);

	if (SUCCEEDED(richOle->GetObject(idx, &reobj, REO_GETOBJ_POLEOBJ))) {
		*pos = reobj.cp;
		if (SUCCEEDED(reobj.poleobj->QueryInterface(IID_IDataObject, (void **)&dobj))) {
			STGMEDIUM	sm;
			FORMATETC	fe;
			memset(&fe, 0, sizeof(fe));
			fe.cfFormat = CF_BITMAP;
			fe.dwAspect	= DVASPECT_CONTENT;
			fe.lindex	= -1;
			fe.tymed	= TYMED_GDI;

			if (SUCCEEDED(dobj->GetData(&fe, &sm))) {
				buf = BmpHandleToPngByte(sm.hBitmap);
				::ReleaseStgMedium(&sm);
			}
			dobj->Release();
		}
		reobj.poleobj->Release();
	}

	return	 buf;
}

int TEditSub::GetImagePos(int idx)
{
	if (!richOle) return -1;

	LPDATAOBJECT	dobj     = NULL;
	REOBJECT		reobj;

	memset(&reobj, 0, sizeof(REOBJECT));
	reobj.cbStruct = sizeof(REOBJECT);

	if (!SUCCEEDED(richOle->GetObject(idx, &reobj, REO_GETOBJ_NO_INTERFACES))) {
		reobj.cp = -1;
	}
	return	 reobj.cp;
}

int TEditSub::GetImageNum()
{
	if (!richOle) return 0;

	return	richOle->GetObjectCount();
}


