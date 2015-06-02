static char *miscdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   miscdlg.cpp	Ver3.00";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc Dialog
	Create					: 1996-12-15(Sun)
	Update					: 2011-04-20(Wed)
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

int TMsgDlg::createCnt = 0;

#define PNG_SIG_SIZE 8

void png_vbuf_wfunc(png_struct *png, png_byte *buf, png_size_t size)
{
	VBuf	*vbuf = (VBuf *)png_get_io_ptr(png);

	if (!vbuf) return;

	if (vbuf->RemainSize() < (int)size) {
		int	grow_size = size - vbuf->RemainSize();
		if (!vbuf->Grow(grow_size)) return;
	}
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
	CHARFORMAT	cf;
	memset(&cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
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
	if (change_select) SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr);

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

DWORD CALLBACK RichStreamCallback(DWORD dwCookie, BYTE *buf, LONG cb, LONG *pcb)
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

void TEditSub::InsertPng(VBuf *vbuf, int pos)
{
	HBITMAP	hBmp = PngByteToBmpHandle(vbuf);

	if (hBmp) {
		InsertBitmapByHandle(hBmp, pos);
		DeleteObject(hBmp);
	}
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

/*
	listview control の subclass化
	Focus を失ったときにも、選択色を変化させないための小細工
*/
#define INVALID_INDEX	-1
TListViewEx::TListViewEx(TWin *_parent) : TSubClassCtl(_parent)
{
	focus_index = INVALID_INDEX;
}

BOOL TListViewEx::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	LV_ITEM	lvi;

	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED;
	int	maxItem = SendMessage(LVM_GETITEMCOUNT, 0, 0);
	int itemNo;

	for (itemNo=0; itemNo < maxItem; itemNo++) {
		if (SendMessage(LVM_GETITEMSTATE, itemNo, (LPARAM)LVIS_FOCUSED) & LVIS_FOCUSED)
			break;
	}

	if (uMsg == WM_SETFOCUS)
	{
		if (itemNo == maxItem) {
			lvi.state = LVIS_FOCUSED;
			if (focus_index == INVALID_INDEX)
				focus_index = 0;
			SendMessage(LVM_SETITEMSTATE, focus_index, (LPARAM)&lvi);
			SendMessage(LVM_SETSELECTIONMARK, 0, focus_index);
		}
		return	FALSE;
	}
	else {	// WM_KILLFOCUS
		if (itemNo != maxItem) {
			SendMessage(LVM_SETITEMSTATE, itemNo, (LPARAM)&lvi);
			focus_index = itemNo;
		}
		return	TRUE;	// WM_KILLFOCUS は伝えない
	}
}

BOOL TListViewEx::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_RBUTTONDOWN:
		return	TRUE;
	case WM_RBUTTONUP:
		::PostMessage(parent->hWnd, uMsg, nHitTest, *(LPARAM *)&pos);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListViewEx::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (focus_index == INVALID_INDEX)
		return	FALSE;

	switch (uMsg) {
	case LVM_INSERTITEM:
		if (((LV_ITEM *)lParam)->iItem <= focus_index)
			focus_index++;
		break;
	case LVM_DELETEITEM:
		if ((int)wParam == focus_index)
			focus_index = INVALID_INDEX;
		else if ((int)wParam < focus_index)
			focus_index--;
		break;
	case LVM_DELETEALLITEMS:
		focus_index = INVALID_INDEX;
		break;
	}
	return	FALSE;
}


TListHeader::TListHeader(TWin *_parent) : TSubClassCtl(_parent)
{
	memset(&logFont, 0, sizeof(logFont));
}

BOOL TListHeader::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == HDM_LAYOUT) {
		HD_LAYOUT *hl = (HD_LAYOUT *)lParam;
		::CallWindowProcW((WNDPROC)oldProc, hWnd, uMsg, wParam, lParam);

//		Debug("HDM_LAYOUT(USER)2 top:%d/bottom:%d diff:%d cy:%d y:%d\n", hl->prc->top, hl->prc->bottom, hl->prc->bottom - hl->prc->top, hl->pwpos->cy, hl->pwpos->y);

		if (logFont.lfHeight) {
			int	height = abs(logFont.lfHeight) + 4;
			hl->prc->top = height;
			hl->pwpos->cy = height;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListHeader::ChangeFontNotify()
{
	HFONT	hFont;

	if (!(hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0)))
		return	FALSE;

	if (::GetObject(hFont, sizeof(LOGFONT), (void *)&logFont) == 0)
		return	FALSE;

//	Debug("lfHeight=%d\n", logFont.lfHeight);
	return	TRUE;
}


/*
	static control の subclass化
*/
TSeparateSub::TSeparateSub(TWin *_parent) : TSubClassCtl(_parent)
{
	hCursor = ::LoadCursor(NULL, IDC_SIZENS);
}

BOOL TSeparateSub::EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg)
{
	::SetCursor(hCursor);
	return	TRUE;
}

BOOL TSeparateSub::EvNcHitTest(POINTS pos, LRESULT *result)
{
	*result = HTCLIENT;
	return	TRUE;	// きちんとイベントを飛ばすため(win3.1/nt3.51)
}

BOOL TSeparateSub::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
		parent->SendMessage(WM_SENDDLG_RESIZE, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}


/*
	不在通知文設定ダイアログ
*/
extern char	*DefaultAbsence[], *DefaultAbsenceHead[];

TAbsenceDlg::TAbsenceDlg(Cfg *_cfg, TWin *_parent) : TDlg(ABSENCE_DIALOG, _parent)
{
	cfg = _cfg;
	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
}

TAbsenceDlg::~TAbsenceDlg()
{
}

BOOL TAbsenceDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	if (rect.left == CW_USEDEFAULT)
	{
		DWORD	val = GetMessagePos();
		POINTS	pos = MAKEPOINTS(val);

		GetWindowRect(&rect);
		int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int x = pos.x - xsize / 2, y = pos.y - ysize;

		if (x + xsize > cx)
			x = cx - xsize;
		if (y + ysize > cy)
			y = cy - ysize;

		MoveWindow(x > 0 ? x : 0, y > 0 ? y : 0, xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	typedef char MaxBuf[MAX_PATH_U8];
	typedef char MaxHead[MAX_NAMEBUF];
	tmpAbsenceStr = new MaxBuf[cfg->AbsenceMax];
	tmpAbsenceHead = new MaxHead[cfg->AbsenceMax];

	SetData();
	return	TRUE;
}

BOOL TAbsenceDlg::EvNcDestroy(void)
{
	delete [] tmpAbsenceHead;
	delete [] tmpAbsenceStr;
	return	TRUE;
}

BOOL TAbsenceDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetData();
		cfg->WriteRegistry(CFG_ABSENCE);
		cfg->AbsenceCheck = FALSE;
		::PostMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		EndDialog(TRUE);
		return	TRUE;

	case SET_BUTTON:
		GetData();
		cfg->WriteRegistry(CFG_ABSENCE);
		if (cfg->AbsenceCheck)
		{
			cfg->AbsenceCheck = FALSE;
			::PostMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		}
		EndDialog(FALSE);
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		return	TRUE;

	case ABSENCEINIT_BUTTON:
		SetDlgItemTextU8(ABSENCEHEAD_EDIT, DefaultAbsenceHead[currentChoice]);
		SetDlgItemTextU8(ABSENCE_EDIT, DefaultAbsence[currentChoice]);
		return	TRUE;

	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_HIDE_CHILDWIN, 0, 0);
		return	TRUE;

	case ABSENCE_LIST:
		if (wNotifyCode == LBN_SELCHANGE)
		{
			int		index;

			if ((index = (int)SendDlgItemMessage(ABSENCE_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR)
			{
				char	oldAbsenceHead[MAX_NAMEBUF];
				strcpy(oldAbsenceHead, tmpAbsenceHead[currentChoice]);
				GetDlgItemTextU8(ABSENCEHEAD_EDIT, tmpAbsenceHead[currentChoice], MAX_NAMEBUF);
				GetDlgItemTextU8(ABSENCE_EDIT, tmpAbsenceStr[currentChoice], MAX_PATH_U8);
				if (strcmp(oldAbsenceHead, tmpAbsenceHead[currentChoice]))
				{
					SendDlgItemMessage(ABSENCE_LIST, LB_DELETESTRING, currentChoice, 0);
					Wstr	head_w(tmpAbsenceHead[currentChoice], BY_UTF8);
					SendDlgItemMessageW(ABSENCE_LIST, LB_INSERTSTRING, currentChoice, (LPARAM)head_w.Buf());
					if (currentChoice == index)
					{
						SendDlgItemMessage(ABSENCE_LIST, LB_SETCURSEL, currentChoice, 0);
						return	TRUE;
					}
				}
				currentChoice = index;
				PostMessage(WM_DELAYSETTEXT, 0, 0);
			}
		}
		else if (wNotifyCode == LBN_DBLCLK)
			PostMessage(WM_COMMAND, IDOK, 0);
		return	TRUE;
	}

	return	FALSE;
}

/*
	User定義 Event CallBack
*/
BOOL TAbsenceDlg::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DELAYSETTEXT:
		{
			int		len = strlen(tmpAbsenceStr[currentChoice]);

			SetDlgItemTextU8(ABSENCEHEAD_EDIT, tmpAbsenceHead[currentChoice]);
			SetDlgItemTextU8(ABSENCE_EDIT, tmpAbsenceStr[currentChoice]);
			SendDlgItemMessage(ABSENCE_EDIT, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		}
		return	TRUE;
	}

	return	FALSE;
}

void TAbsenceDlg::SetData(void)
{
	for (int cnt=0; cnt < cfg->AbsenceMax; cnt++)
	{
		strcpy(tmpAbsenceHead[cnt], cfg->AbsenceHead[cnt]);
		strcpy(tmpAbsenceStr[cnt], cfg->AbsenceStr[cnt]);
		Wstr	head_w(cfg->AbsenceHead[cnt], BY_UTF8);
		SendDlgItemMessageW(ABSENCE_LIST, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)head_w.Buf());
	}
	currentChoice = cfg->AbsenceChoice;
	PostMessage(WM_DELAYSETTEXT, 0, 0);
	SendDlgItemMessage(ABSENCE_LIST, LB_SETCURSEL, currentChoice, 0);
}

void TAbsenceDlg::GetData(void)
{
	GetDlgItemTextU8(ABSENCEHEAD_EDIT, tmpAbsenceHead[currentChoice], MAX_NAMEBUF);
	GetDlgItemTextU8(ABSENCE_EDIT, tmpAbsenceStr[currentChoice], MAX_PATH_U8);
	for (int cnt=0; cnt < cfg->AbsenceMax; cnt++)
	{
		strcpy(cfg->AbsenceHead[cnt], tmpAbsenceHead[cnt]);
		strcpy(cfg->AbsenceStr[cnt], tmpAbsenceStr[cnt]);
	}
	cfg->AbsenceChoice = currentChoice;
}


/*
	ソート設定ダイアログ
*/
TSortDlg *TSortDlg::exclusiveWnd = NULL;

TSortDlg::TSortDlg(Cfg *_cfg, TWin *_parent) : TDlg(SORT_DIALOG, _parent)
{
	cfg = _cfg;
}

int TSortDlg::Exec(void)
{
	if (exclusiveWnd)
		return	exclusiveWnd->SetForegroundWindow(), FALSE;

	exclusiveWnd = this;
	return	TDlg::Exec();
}

BOOL TSortDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case SET_BUTTON:
		GetData();
		cfg->WriteRegistry(CFG_GENERAL);
		if (wID == IDOK)
			EndDialog(TRUE), exclusiveWnd = NULL;
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		exclusiveWnd = NULL;
		return	TRUE;
	}

	return	FALSE;
}

BOOL TSortDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int x = rect.left + 50, y = rect.top + 20;
	int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	MoveWindow(x, y, xsize, ysize, FALSE);
	SetData();

	return	TRUE;
}

void TSortDlg::SetData(void)
{
	SendDlgItemMessage(GROUPDISP_CHECK, BM_SETCHECK, GetItem(cfg->ColumnItems, SW_GROUP), 0);
	SendDlgItemMessage(HOST_CHECK, BM_SETCHECK, GetItem(cfg->ColumnItems, SW_HOST), 0);
	SendDlgItemMessage(LOGON_CHECK, BM_SETCHECK, GetItem(cfg->ColumnItems, SW_USER), 0);
	SendDlgItemMessage(PRIORITY_CHECK, BM_SETCHECK, GetItem(cfg->ColumnItems, SW_PRIORITY), 0);
	SendDlgItemMessage(IPADDR_CHECK, BM_SETCHECK, GetItem(cfg->ColumnItems, SW_IPADDR), 0);
	SendDlgItemMessage(GLIDLINE_CHECK, BM_SETCHECK, cfg->GlidLineCheck, 0);

	SendDlgItemMessage(GROUP_CHECK, BM_SETCHECK, !(cfg->Sort & IPMSG_NOGROUPSORTOPT), 0);
	SendDlgItemMessage(GROUPREV_CHECK, BM_SETCHECK, (cfg->Sort & IPMSG_GROUPREVSORTOPT) != 0, 0);
	SendDlgItemMessage(USER_RADIO, BM_SETCHECK, GET_MODE(cfg->Sort) == IPMSG_NAMESORT, 0);
	SendDlgItemMessage(HOST_RADIO, BM_SETCHECK, GET_MODE(cfg->Sort) == IPMSG_HOSTSORT, 0);
	SendDlgItemMessage(IPADDR_RADIO, BM_SETCHECK, GET_MODE(cfg->Sort) == IPMSG_IPADDRSORT, 0);
	SendDlgItemMessage(SUBREV_CHECK, BM_SETCHECK, (cfg->Sort & IPMSG_SUBREVSORTOPT) != 0, 0);
	SendDlgItemMessage(ICMP_CHECK, BM_SETCHECK, (cfg->Sort & IPMSG_ICMPSORTOPT) != 0, 0);
	SendDlgItemMessage(KANJI_CHECK, BM_SETCHECK, !(cfg->Sort & IPMSG_NOKANJISORTOPT), 0);
}

void TSortDlg::GetData(void)
{
	SetItem(&cfg->ColumnItems, SW_GROUP, SendDlgItemMessage(GROUPDISP_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_HOST, SendDlgItemMessage(HOST_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_USER, SendDlgItemMessage(LOGON_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_PRIORITY, SendDlgItemMessage(PRIORITY_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_IPADDR, SendDlgItemMessage(IPADDR_CHECK, BM_GETCHECK, 0, 0));
	cfg->GlidLineCheck = SendDlgItemMessage(GLIDLINE_CHECK, BM_GETCHECK, 0, 0);

	cfg->Sort = 0;

	if (SendDlgItemMessage(GROUP_CHECK, BM_GETCHECK, 0, 0) == 0)
		cfg->Sort |= IPMSG_NOGROUPSORTOPT;
	if (SendDlgItemMessage(GROUPREV_CHECK, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_GROUPREVSORTOPT;
	if (SendDlgItemMessage(USER_RADIO, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_NAMESORT;
	if (SendDlgItemMessage(HOST_RADIO, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_HOSTSORT;
	if (SendDlgItemMessage(IPADDR_RADIO, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_IPADDRSORT;
	if (SendDlgItemMessage(SUBREV_CHECK, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_SUBREVSORTOPT;
	if (SendDlgItemMessage(ICMP_CHECK, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_ICMPSORTOPT;
	if (SendDlgItemMessage(KANJI_CHECK, BM_GETCHECK, 0, 0) == 0)
		cfg->Sort |= IPMSG_NOKANJISORTOPT;
}

/*
	クリッカブルURL設定ダイアログ
*/
TUrlDlg::TUrlDlg(Cfg *_cfg, TWin *_parent) : TDlg(URL_DIALOG, _parent)
{
	cfg = _cfg;
	*currentProtocol = 0;
}

TUrlDlg::~TUrlDlg()
{
}

BOOL TUrlDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int x = rect.left + 10, y = rect.top + 50;
	int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	MoveWindow(x, y, xsize, ysize, FALSE);
	SetData();
	SendDlgItemMessage(URL_LIST, LB_SETCURSEL, 0, 0);
	EvCommand(0, URL_LIST, 0);

	return	TRUE;
}

BOOL TUrlDlg::EvNcDestroy(void)
{
	UrlObj *urlObj;

	while ((urlObj = (UrlObj *)tmpUrlList.TopObj()))
	{
		tmpUrlList.DelObj(urlObj);
		delete urlObj;
	}
	return	TRUE;
}

BOOL TUrlDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case SET_BUTTON:
		GetData();
		cfg->WriteRegistry(CFG_CLICKURL);
		if (wID == IDOK)
			EndDialog(TRUE);
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		return	TRUE;

	case ADD_BUTTON:
		{
			char	protocol[MAX_LISTBUF], buf[MAX_LISTBUF];
			int		index;
			UrlObj	*obj;

			if ((index = (int)SendDlgItemMessage(URL_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR && SendDlgItemMessage(URL_LIST, LB_GETTEXT, (WPARAM)index, (LPARAM)protocol) != LB_ERR && (obj = SearchUrlObj(&tmpUrlList, protocol)))
			{
				wsprintf(buf, GetLoadStrU8(IDS_EXECPROGRAM), protocol);
				OpenFileDlg(this).Exec(URL_EDIT, buf, GetLoadStrAsFilterU8(IDS_OPENFILEPRGFLTR));
			}
		}
		return	TRUE;

	case URL_LIST:
		{
			char	protocol[MAX_LISTBUF];
			int		index;
			UrlObj	*obj;

			if ((index = (int)SendDlgItemMessage(URL_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR && SendDlgItemMessage(URL_LIST, LB_GETTEXT, (WPARAM)index, (LPARAM)protocol) != LB_ERR && (obj = SearchUrlObj(&tmpUrlList, protocol)))
			{
				SetCurrentData();
				SetDlgItemTextU8(URL_EDIT, obj->program);
				strncpyz(currentProtocol, protocol, sizeof(currentProtocol));
			}
		}
		return	TRUE;
	}

	return	FALSE;
}

void TUrlDlg::SetCurrentData(void)
{
	UrlObj	*obj;

	if ((obj = SearchUrlObj(&tmpUrlList, currentProtocol)))
		GetDlgItemTextU8(URL_EDIT, obj->program, sizeof(obj->program));
}

void TUrlDlg::SetData(void)
{
	for (UrlObj *obj = (UrlObj *)cfg->urlList.TopObj(); obj; obj = (UrlObj *)cfg->urlList.NextObj(obj))
	{
		UrlObj *tmp_obj = new UrlObj;
		strcpy(tmp_obj->protocol, obj->protocol);
		strcpy(tmp_obj->program, obj->program);
		tmpUrlList.AddObj(tmp_obj);

		SendDlgItemMessage(URL_LIST, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)obj->protocol);
	}
	SendDlgItemMessage(DEFAULTURL_CHECK, BM_SETCHECK, cfg->DefaultUrl, 0);
	SendDlgItemMessage(SHELLEXEC_CHECK, BM_SETCHECK, cfg->ShellExec, 0);
}

void TUrlDlg::GetData(void)
{
	SetCurrentData();

	for (UrlObj *tmp_obj = (UrlObj *)tmpUrlList.TopObj(); tmp_obj; tmp_obj = (UrlObj *)tmpUrlList.NextObj(tmp_obj))
	{
		UrlObj *obj = SearchUrlObj(&cfg->urlList, tmp_obj->protocol);

		if (obj == NULL)
		{
			obj = new UrlObj;
			cfg->urlList.AddObj(obj);
			strcpy(obj->protocol, tmp_obj->protocol);
		}
		strcpy(obj->program, tmp_obj->program);
	}
	cfg->DefaultUrl = (int)SendDlgItemMessage(DEFAULTURL_CHECK, BM_GETCHECK, 0, 0);
	cfg->ShellExec = (int)SendDlgItemMessage(SHELLEXEC_CHECK, BM_GETCHECK, 0, 0);
}


// パスワード確認用
TPasswordDlg::TPasswordDlg(Cfg *_cfg, TWin *_parent) : TDlg(PASSWORD_DIALOG, _parent)
{
	cfg = _cfg;
	outbuf = NULL;
}

// 単なるパスワード入力用
TPasswordDlg::TPasswordDlg(char *_outbuf, TWin *_parent) : TDlg(PASSWORD_DIALOG, _parent)
{
	cfg = NULL;
	outbuf = _outbuf;
}

BOOL TPasswordDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		{
			char	_buf[MAX_NAMEBUF];
			char	*buf = outbuf ? outbuf : _buf;

			GetDlgItemTextU8(PASSWORD_EDIT, buf, MAX_NAMEBUF);
			if (cfg)
			{
				if (CheckPassword(cfg->PasswordStr, buf))
					EndDialog(TRUE);
				else
					SetDlgItemTextU8(PASSWORD_EDIT, ""), MessageBoxU8(GetLoadStrU8(IDS_CANTAUTH));
			}
			else EndDialog(TRUE);
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TPasswordDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	if (parent)
		MoveWindow(rect.left +100, rect.top +100, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	else
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	x = (::GetSystemMetrics(SM_CXFULLSCREEN) - xsize)/2;
		int y = (::GetSystemMetrics(SM_CYFULLSCREEN) - ysize)/2;
		MoveWindow(x, y, xsize, ysize, FALSE);
	}

	return	TRUE;
}

TPasswdChangeDlg::TPasswdChangeDlg(Cfg *_cfg, TWin *_parent) : TDlg(PASSWDCHANGE_DIALOG, _parent)
{
	cfg = _cfg;
}

BOOL TPasswdChangeDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		char	buf[MAX_NAMEBUF], buf2[MAX_NAMEBUF];

		GetDlgItemTextU8(OLDPASSWORD_EDIT, buf, sizeof(buf));
		if (CheckPassword(cfg->PasswordStr, buf))
		{
			GetDlgItemTextU8(NEWPASSWORD_EDIT, buf, sizeof(buf));
			GetDlgItemTextU8(NEWPASSWORD_EDIT2, buf2, sizeof(buf2));
			if (strcmp(buf, buf2) == 0)
				MakePassword(buf, cfg->PasswordStr);
			else
				return	MessageBoxU8(GetLoadStrU8(IDS_NOTSAMEPASS)), TRUE;
			cfg->WriteRegistry(CFG_GENERAL);
			EndDialog(TRUE);
		}
		else
			SetDlgItemTextU8(PASSWORD_EDIT, ""), MessageBoxU8(GetLoadStrU8(IDS_CANTAUTH));
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TPasswdChangeDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	MoveWindow(rect.left +50, rect.top +100, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	if (*cfg->PasswordStr == 0)
		::EnableWindow(GetDlgItem(OLDPASSWORD_EDIT), FALSE);

	return	TRUE;
}


/*
	TMsgDlgは、制約の多いMessageBoxU8()の代用として作成
*/
TMsgDlg::TMsgDlg(TWin *_parent) : TListDlg(MESSAGE_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
}

/*
	終了ルーチン
*/
TMsgDlg::~TMsgDlg()
{
	createCnt--;
}

/*
	Window生成ルーチン
*/
BOOL TMsgDlg::Create(char *text, char *title, int _showMode)
{
	showMode = _showMode;

	if (!TDlg::Create()) return	FALSE;

	SetDlgItemTextU8(MESSAGE_TEXT, text);
	SetWindowTextU8(title);
	if (showMode == SW_SHOW) {
		DWORD	key =	GetAsyncKeyState(VK_LBUTTON) |
						GetAsyncKeyState(VK_RBUTTON) |
						GetAsyncKeyState(VK_MBUTTON);
		::EnableWindow(hWnd, TRUE);
		::ShowWindow(hWnd, (key & 0x8000) ? SW_SHOWNOACTIVATE : SW_SHOW);
	}
	else {
		::ShowWindow(hWnd, SW_SHOWMINNOACTIVE);
		::EnableWindow(hWnd, TRUE);
	}

	if (strstr(text, "\r\n"))
	{
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		::GetWindowPlacement(GetDlgItem(MESSAGE_TEXT), &wp);
		wp.rcNormalPosition.top -= ::GetSystemMetrics(SM_CYCAPTION) / 4;
		::SetWindowPlacement(GetDlgItem(MESSAGE_TEXT), &wp);
	}
	return	TRUE;
}

/*
	WM_COMMAND CallBack
*/
BOOL TMsgDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case IDOK:
	case IDCANCEL:
		EndDialog(FALSE);
		::PostMessage(GetMainWnd(), WM_MSGDLG_EXIT, (WPARAM)0, (LPARAM)this);
		return	TRUE;
	}
	return	FALSE;
}

/*
	Window 生成時の CallBack。Screenの中心を起点として、すでに開いている
	TMsgDlgの枚数 * Caption Sizeを ずらして表示
*/
BOOL TMsgDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2 + createCnt * ::GetSystemMetrics(SM_CYCAPTION);
		int y = (cy - ysize)/2 + createCnt * ::GetSystemMetrics(SM_CYCAPTION);

		MoveWindow((x < 0) ? 0 : x % (cx - xsize), (y < 0) ? 0 : y % (cy - ysize), xsize, ysize,
					FALSE);
	}

	createCnt++;
	return	TRUE;
}

#define MAX_HISTHASH	1001
HistHash::HistHash() : THashTbl(MAX_HISTHASH)
{
	top = end = NULL;
}

HistHash::~HistHash()
{
}

BOOL HistHash::IsSameVal(THashObj *obj, const void *val)
{
	return	strcmp(((HistObj *)obj)->info, (const char *)val) == 0;
}

void HistHash::Clear()
{
	UnInit();
	Init(MAX_HISTHASH);
	top = end = NULL;
}

void HistHash::Register(THashObj *_obj, u_int hash_id)
{
	HistObj	*obj = (HistObj *)_obj;
	THashTbl::Register(obj, hash_id);
	if (top) {
		obj->next = top;
		obj->prior = NULL;
		top->prior = obj;
		top = obj;
	}
	else {
		top = end = obj;
		obj->next = obj->prior = NULL;
	}
}

void HistHash::UnRegister(THashObj *_obj)
{
	HistObj	*obj = (HistObj *)_obj;

	if (obj->next) {
		obj->next->prior = obj->prior;
	}
	if (obj == top) {
		top = obj->next;
	}
	if (obj->prior) {
		obj->prior->next = obj->next;
	}
	if (obj == end) {
		end = obj->prior;
	}
	obj->next = obj->prior = NULL;

	THashTbl::UnRegister(obj);
}


/*
	THistDlg
*/
THistDlg::THistDlg(Cfg *_cfg, THosts *_hosts, TWin *_parent) : TDlg(HISTORY_DIALOG, _parent), histListView(this), histListHeader(&histListView)
{
	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	hosts = _hosts;
	hListFont = NULL;
	detailMode = FALSE;
	unOpenedNum = 0;
}

/*
	終了ルーチン
*/
THistDlg::~THistDlg()
{
	if (hListFont) ::DeleteObject(hListFont);
}

/*
*/
BOOL THistDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	SetDlgItem(HISTORY_LIST, XY_FIT);
	SetDlgItem(IDOK, HMID_FIT);
	SetDlgItem(CLEAR_BUTTON, LEFT_FIT);
	SetDlgItem(DETAIL_CHECK, RIGHT_FIT);

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		orgRect = rect;

		int xsize = rect.right - rect.left + cfg->HistXdiff;
		int ysize = rect.bottom - rect.top + cfg->HistYdiff;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;
		MoveWindow((x < 0) ? 0 : x % (cx - xsize), (y < 0) ? 0 : y % (cy - ysize),
					xsize, ysize, FALSE);
		GetWindowTextU8(title, sizeof(title));
	}
	else {
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	}

	FitDlgItems();
	SendDlgItemMessage(DETAIL_CHECK, BM_SETCHECK, detailMode, 0);
	SetWindowTextU8(FmtStr(unOpenedNum ? "%s (%d)" : "%s", title, unOpenedNum));

	return	TRUE;
}

/*
	WM_COMMAND CallBack
*/
BOOL THistDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case IDOK:
	case IDCANCEL:
		EndDialog(FALSE);
//		::PostMessage(GetMainWnd(), WM_MSGDLG_EXIT, (WPARAM)0, (LPARAM)this);
		return	TRUE;

	case DETAIL_CHECK:
		detailMode = !detailMode;
		histListView.SendMessage(LVM_DELETEALLITEMS, 0, 0);
		SetHeader();
		SetAllData();
		return	TRUE;

	case CLEAR_BUTTON:
		histListView.SendMessage(LVM_DELETEALLITEMS, 0, 0);
		histHash.Clear();
		unOpenedNum = 0;
		SetWindowTextU8(FmtStr(unOpenedNum ? "%s (%d)" : "%s", title, unOpenedNum));
		return	TRUE;
	}
	return	FALSE;
}

/*
	WM_DESTROY CallBack
*/
BOOL THistDlg::EvDestroy()
{
	histListView.SendMessage(LVM_DELETEALLITEMS, 0, 0);
	return	FALSE;
}

BOOL THistDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED)
		return	FitDlgItems();
	return	FALSE;;
}

/*
	Window生成ルーチン
*/
BOOL THistDlg::Create(HINSTANCE hI)
{
	if (!TDlg::Create(hI)) return	FALSE;

	histListView.AttachWnd(GetDlgItem(HISTORY_LIST));
	histListHeader.AttachWnd((HWND)histListView.SendMessage(LVM_GETHEADER, 0, 0));

	DWORD dw = histListView.GetWindowLong(GWL_STYLE) | LVS_SHOWSELALWAYS;
	histListView.SetWindowLong(GWL_STYLE, dw);

// １行全カラム選択など
	DWORD style = histListView.SendMessage(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
	style |= LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	histListView.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, style);

	SetFont();
	SetHeader(); // dummy for reflect font
	SetHeader();
	SetAllData();

	return	TRUE;
}

void THistDlg::SetHeader()
{
	int	title_id[] = { IDS_HISTUSER, IDS_HISTSDATE, IDS_HISTODATE, IDS_HISTID };
	int	col_len = detailMode ? HW_ID : HW_SDATE;
	int	i;

	for (i=0; i < MAX_HISTWIDTH; i++) {
		if (!histListView.SendMessage(LVM_DELETECOLUMN, 0, 0)) break;
	}

	for (i=0; i <= col_len; i++) {
		LV_COLUMNW	lvC;
		memset(&lvC, 0, sizeof(lvC));
		lvC.fmt = LVCFMT_LEFT;
		lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvC.pszText = GetLoadStrW(title_id[i]);
		lvC.cx      = cfg->HistWidth[i];
		histListView.SendMessage(LVM_INSERTCOLUMNW, lvC.iSubItem=i, (LPARAM)&lvC);
	}
}

void THistDlg::SetAllData()
{
	for (HistObj *obj = histHash.End(); obj; obj = obj->prior) {
		if (!detailMode && *obj->odate) continue;
		InsertItem(0, obj->user, obj);
		SetItem(0, HW_SDATE, obj->sdate);
		if (detailMode) {
			SetItem(0, HW_ID, obj->pktno);
			if (*obj->odate) {
				SetItem(0, HW_ODATE, obj->odate);
			}
		}
	}
}

int THistDlg::MakeHistInfo(HostSub *hostSub, ULONG packet_no, char *buf)
{
	return	sprintf(buf, "%s:%s:%d", hostSub->userName, hostSub->hostName, packet_no);
}

void THistDlg::InsertItem(int idx, char *buf, void *obj)
{
	LV_ITEMW		lvi = {0};
	lvi.mask		= LVIF_TEXT|LVIF_PARAM;
	lvi.iItem		= idx;
	lvi.iSubItem	= HW_USER;
	lvi.pszText		= U8toW(buf);
	lvi.lParam		= (LPARAM)obj;
	histListView.SendMessage(LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

void THistDlg::SetItem(int idx, int subIdx, char *buf)
{
	LV_ITEMW		lvi = {0};
	lvi.mask		= LVIF_TEXT;
	lvi.iItem		= idx;
	lvi.iSubItem	= subIdx;
	lvi.pszText		= U8toW(buf);
	histListView.SendMessage(LVM_SETITEMW, 0, (LPARAM)&lvi);
}

void THistDlg::SendNotify(HostSub *hostSub, ULONG packetNo)
{
#define MAX_OPENHISTORY 500
	int num = histHash.GetRegisterNum();
	if (num >= MAX_OPENHISTORY) {
		HistObj *obj = histHash.End();

		if (obj) {
			if (hWnd) histListView.SendMessage(LVM_DELETEITEM, num-1, 0);
			histHash.UnRegister(obj);
			if (!*obj->odate) unOpenedNum--;
			delete obj;
			num--;
		}
	}

	HistObj	*obj = new HistObj();
	int		len = MakeHistInfo(hostSub, packetNo, obj->info);

	histHash.Register(obj, MakeHash(obj->info, len, 0));

	MakeListString(cfg, hostSub, hosts, obj->user);
	SYSTEMTIME	st;
	::GetLocalTime(&st);
	sprintf(obj->sdate, "%02d/%02d %02d:%02d", st.wMonth, st.wDay, st.wHour, st.wMinute);
	sprintf(obj->pktno, "%x", packetNo);

	unOpenedNum++;

	if (hWnd) {
		InsertItem(0, obj->user, obj);
		SetItem(0, HW_SDATE, obj->sdate);
		if (detailMode) {
			SetItem(0, HW_ID, obj->pktno);
		}
		SetWindowTextU8(FmtStr("%s (%d)", title, unOpenedNum));
	}
}

void THistDlg::OpenNotify(HostSub *hostSub, ULONG packetNo)
{
	char	buf[MAX_BUF];
	int		len;
	u_int	hash_val;
	HistObj	*obj;
	int		idx;

	len = MakeHistInfo(hostSub, packetNo, buf);
	hash_val =  MakeHash(buf, len, 0);
	if (!(obj = (HistObj *)histHash.Search(buf, hash_val))) {
		SendNotify(hostSub, packetNo);
		if (!(obj = (HistObj *)histHash.Search(buf, hash_val))) {
			return;
		}
	}

	if (*obj->odate) return;

	SYSTEMTIME	st;
	::GetLocalTime(&st);
	sprintf(obj->odate, "%02d/%02d %02d:%02d", st.wMonth, st.wDay, st.wHour, st.wMinute);
	if (--unOpenedNum < 0) unOpenedNum = 0;

	if (hWnd) {
		LV_FINDINFO	lfi = {0};
		lfi.flags = LVFI_PARAM;
		lfi.lParam = (LPARAM)obj;
		if ((idx = histListView.SendMessage(LVM_FINDITEM, -1, (LPARAM)&lfi)) < 0) {
			return;
		}

		if (detailMode) {
			SetItem(idx, HW_ODATE, obj->odate);
		}
		else {
			histListView.SendMessage(LVM_DELETEITEM, idx, 0);
		}
		SetWindowTextU8(FmtStr(unOpenedNum ? "%s (%d)" : "%s", title, unOpenedNum));
	}
}

void THistDlg::SetFont()
{
	HFONT	hDlgFont;

	if (*cfg->SendListFont.lfFaceName && (hDlgFont = ::CreateFontIndirect(&cfg->SendListFont)))
	{
		histListView.SendMessage(WM_SETFONT, (WPARAM)hDlgFont, 0);
		if (hListFont)
			::DeleteObject(hListFont);
		hListFont = hDlgFont;
 	}
	histListHeader.ChangeFontNotify();
}

void THistDlg::Show(int mode)
{
	if (IsWindowVisible() && mode == SW_HIDE) {
		GetWindowRect(&rect);
		cfg->HistXdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
		cfg->HistYdiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);

		int	col_len = detailMode ? HW_ID : HW_SDATE;
		for (int i=0; i <= detailMode; i++) {
			cfg->HistWidth[i] = histListView.SendMessage(LVM_GETCOLUMNWIDTH, i, 0);
		}
		cfg->WriteRegistry(CFG_WINSIZE);
	}

	TDlg::Show(mode);
}

/*
	About Dialog初期化処理
*/
TAboutDlg::TAboutDlg(TWin *_parent) : TDlg(ABOUT_DIALOG, _parent)
{
}

/*
	Window 生成時の CallBack
*/
BOOL TAboutDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	char	buf[512], buf2[512];
	GetDlgItemTextU8(ABOUT_TEXT, buf, sizeof(buf));
	wsprintf(buf2, buf, GetVersionStr());
	SetDlgItemTextU8(ABOUT_TEXT, buf2);

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;

		MoveWindow((x < 0) ? 0 : x % (cx - xsize), (y < 0) ? 0 : y % (cy - ysize),
					xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	return	TRUE;
}

BOOL TAboutDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(TRUE);
		return	TRUE;

	case IPMSG_ICON: case IPMSGWEB_BUTTON:
		if (wID == IPMSGWEB_BUTTON || wNotifyCode == 1)
			ShellExecuteU8(NULL, NULL, GetLoadStrU8(IDS_IPMSGURL), NULL, NULL, SW_SHOW);
		return	TRUE;
	}
	return	FALSE;
}


BOOL CheckPassword(const char *cfgPasswd, const char *inputPasswd)
{
	char	buf[MAX_NAMEBUF];

	MakePassword(inputPasswd, buf);

	return	strcmp(buf, cfgPasswd) == 0 ? TRUE : FALSE;
}

void MakePassword(const char *inputPasswd, char *outputPasswd)
{
	while (*inputPasswd)	// 可逆だぞ...DESライブラリがあればいいのに
		*outputPasswd++ = ((~*inputPasswd++) & 0x7f); //レジストリのストリング型は８ビット目を嫌う(;_;)

	*outputPasswd = 0;
}


/*
	URL検索ルーチン
*/
UrlObj *SearchUrlObj(TList *list, char *protocol)
{
	for (UrlObj *obj = (UrlObj *)list->TopObj(); obj; obj = (UrlObj *)list->NextObj(obj))
		if (stricmp(obj->protocol, protocol) == 0)
			return	obj;

	return	NULL;
}

/*
	ダイアログ用アイコン設定
*/
void SetDlgIcon(HWND hWnd)
{
	static HICON	oldHIcon = NULL;

	if (oldHIcon != TMainWin::GetIPMsgIcon())
	{
		oldHIcon = TMainWin::GetIPMsgIcon();
		::SetClassLong(hWnd, GCL_HICON, (LONG)oldHIcon);
	}
}

/*
	ログ記録用の HostEntry表示文字列
*/
void MakeListString(Cfg *cfg, HostSub *hostSub, THosts *hosts, char *buf)
{
	Host	*host;

	if ((host = hosts->GetHostByAddr(hostSub)) && IsSameHost(hostSub, &host->hostSub)
		 || GetUserNameDigestField(hostSub->userName) &&
		 	(host = cfg->priorityHosts.GetHostByName(hostSub)))
		MakeListString(cfg, host, buf);
	else {
		Host	host;

		memset(&host, 0, sizeof(host));
		host.hostSub = *hostSub;
		MakeListString(cfg, &host, buf);
	}
}

/*
	ログ記録用の HostEntry表示文字列
*/
void MakeListString(Cfg *cfg, Host *host, char *buf)
{
	char	*fmt = "%s ";

	if (*host->nickName)
	{
		if (cfg->NickNameCheck == IPMSG_NICKNAME)
			buf += wsprintf(buf, fmt, host->nickName);
		else {
			char	tmp_name[MAX_LISTBUF];
			wsprintf(tmp_name, "%s[%s]", host->nickName, host->hostSub.userName);
			buf += wsprintf(buf, fmt, tmp_name);
		}
	}
	else
		buf += wsprintf(buf, fmt, host->hostSub.userName);

	if (host->hostStatus & IPMSG_ABSENCEOPT)
		*buf++ = '*';

	buf += wsprintf(buf, "(%s%s%s", host->groupName, *host->groupName ? "/":"", host->hostSub.hostName);

	if (cfg->IPAddrCheck)
		buf += wsprintf(buf, "/%s", inet_ntoa(*(LPIN_ADDR)&host->hostSub.addr));

	strcpy(buf, ")");
}

/*
	IME 制御
*/
BOOL SetImeOpenStatus(HWND hWnd, BOOL flg)
{
	BOOL ret = FALSE;

	HIMC hImc;

	if ((hImc = ImmGetContext(hWnd)))
	{
		ret = ImmSetOpenStatus(hImc, flg);
		ImmReleaseContext(hWnd, hImc);
	}
	return	ret;
}

BOOL GetImeOpenStatus(HWND hWnd)
{
	BOOL ret = FALSE;

	HIMC hImc;

	if ((hImc = ImmGetContext(hWnd)))
	{
		ret = ImmGetOpenStatus(hImc);
		ImmReleaseContext(hWnd, hImc);
	}
	return	ret;
}

/*
	ホットキー設定
*/
BOOL SetHotKey(Cfg *cfg)
{
	if (cfg->HotKeyCheck)
	{
		RegisterHotKey(GetMainWnd(), WM_SENDDLG_OPEN, cfg->HotKeyModify, cfg->HotKeySend);
		RegisterHotKey(GetMainWnd(), WM_RECVDLG_OPEN, cfg->HotKeyModify, cfg->HotKeyRecv);
		RegisterHotKey(GetMainWnd(), WM_DELMISCDLG, cfg->HotKeyModify, cfg->HotKeyMisc);
	}
	else {
		UnregisterHotKey(GetMainWnd(), WM_SENDDLG_OPEN);
		UnregisterHotKey(GetMainWnd(), WM_RECVDLG_OPEN);
		UnregisterHotKey(GetMainWnd(), WM_DELMISCDLG);
	}
	return	TRUE;
}

/*
	Host1 と Host2 が同一かどうかを比較
*/
BOOL IsSameHost(HostSub *hostSub1, HostSub *hostSub2)
{
	if (stricmp(hostSub1->hostName, hostSub2->hostName))
		return	FALSE;

	return	stricmp(hostSub1->userName, hostSub2->userName) ? FALSE : TRUE;
}

/*
	RECT -> WINPOS
*/
void RectToWinPos(const RECT *rect, WINPOS *wpos)
{
	wpos->x = rect->left, wpos->y = rect->top;
	wpos->cx = rect->right - rect->left;
	wpos->cy = rect->bottom - rect->top;
}

/*
	host array class
*/
THosts::THosts(void)
{
	hostCnt = 0;
	memset(array, 0, sizeof(array));
	for (int kind=0; kind < MAX_ARRAY; kind++)
		enable[kind] = FALSE;
}

THosts::~THosts()
{
	for (int kind=0; kind < MAX_ARRAY; kind++) {
		if (array[kind])
			free(array[kind]);
	}
}


int THosts::Cmp(HostSub *hostSub1, HostSub *hostSub2, Kind kind)
{
	switch (kind) {
	case NAME: case NAME_ADDR:
		{
			int	cmp;
			if (cmp = stricmp(hostSub1->userName, hostSub2->userName))
				return	cmp;
			if ((cmp = stricmp(hostSub1->hostName, hostSub2->hostName)) || kind == NAME)
				return	cmp;
		}	// if cmp == 0 && kind == NAME_ADDR, through...
	case ADDR:
		if (hostSub1->addr > hostSub2->addr)
			return	1;
		if (hostSub1->addr < hostSub2->addr)
			return	-1;
		if (hostSub1->portNo > hostSub2->portNo)
			return	1;
		if (hostSub1->portNo < hostSub2->portNo)
			return	-1;
		return	0;
	}
	return	-1;
}

Host *THosts::Search(Kind kind, HostSub *hostSub, int *insertIndex)
{
	int	min = 0, max = hostCnt -1, cmp, tmpIndex;

	if (insertIndex == NULL)
		insertIndex = &tmpIndex;

	while (min <= max)
	{
		*insertIndex = (min + max) / 2;

		if ((cmp = Cmp(hostSub, &array[kind][*insertIndex]->hostSub, kind)) == 0)
			return	array[kind][*insertIndex];
		else if (cmp > 0)
			min = *insertIndex +1;
		else
			max = *insertIndex -1;
	}

	*insertIndex = min;
	return	NULL;
}

BOOL THosts::AddHost(Host *host)
{
	int		insertIndex[MAX_ARRAY], kind;

// すべてのインデックス種類での確認を先に行う
	for (kind=0; kind < MAX_ARRAY; kind++) {
		if (!enable[kind])
			continue;
		if (Search((Kind)kind, &host->hostSub, &insertIndex[kind]))
			return	FALSE;
	}

#define BIG_ALLOC	100
	for (kind=0; kind < MAX_ARRAY; kind++) {
		if (!enable[kind])
			continue;
		if ((hostCnt % BIG_ALLOC) == 0)
		{
			Host	**tmpArray = (Host **)realloc(array[kind], (hostCnt + BIG_ALLOC) * sizeof(Host *));
			if (tmpArray == NULL)
				return	FALSE;
			array[kind] = tmpArray;
		}
		memmove(array[kind] + insertIndex[kind] + 1, array[kind] + insertIndex[kind], (hostCnt - insertIndex[kind]) * sizeof(Host *));
		array[kind][insertIndex[kind]] = host;
	}
	host->RefCnt(1);
	hostCnt++;
	return	TRUE;
}

BOOL THosts::DelHost(Host *host)
{
	int		insertIndex[MAX_ARRAY], kind;

// すべてのインデックス種類での確認を先に行う
	for (kind=0; kind < MAX_ARRAY; kind++) {
		if (!enable[kind])
			continue;
		if (Search((Kind)kind, &host->hostSub, &insertIndex[kind]) == NULL)
			return	FALSE;
	}

	hostCnt--;

	for (kind=0; kind < MAX_ARRAY; kind++) {
		if (!enable[kind])
			continue;
		memmove(array[kind] + insertIndex[kind], array[kind] + insertIndex[kind] + 1, (hostCnt - insertIndex[kind]) * sizeof(Host *));
	}
	host->RefCnt(-1);

	return	TRUE;
}

BOOL THosts::PriorityHostCnt(int priority, int range)
{
	int		member = 0;

	for (int cnt=0; cnt < hostCnt; cnt++)
		if (array[NAME][cnt]->priority >= priority && array[NAME][cnt]->priority < priority + range)
			member++;
	return	member;
}

TFindDlg::TFindDlg(Cfg *_cfg, TSendDlg *_parent) : TDlg(FIND_DIALOG, _parent)
{
	cfg = _cfg;
	sendDlg = _parent;
}

BOOL TFindDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		int		cnt;
		GetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0], MAX_NAMEBUF);
		cfg->FindAll = SendDlgItemMessage(ALLFIND_CHECK, BM_GETCHECK, 0, 0);

		if (sendDlg->FindHost(cfg->FindStr[0], cfg->FindAll)) {
			for (cnt=1; cnt < cfg->FindMax; cnt++)
				if (stricmp(cfg->FindStr[cnt], cfg->FindStr[0]) == 0)
					break;
			memmove(cfg->FindStr[2], cfg->FindStr[1], (cnt == cfg->FindMax ? cnt-2 : cnt-1) * MAX_NAMEBUF);
			memcpy(cfg->FindStr[1], cfg->FindStr[0], MAX_NAMEBUF);
			DWORD	start, end;		// エディット部の選択状態の保存
			SendDlgItemMessage(FIND_COMBO, CB_GETEDITSEL, (WPARAM)&start, (LPARAM)&end);
			// CB_RESETCONTENT でエディット部がクリア
			// なお、DELETESTRING でも edit 同名stringの場合、同じくクリアされる
			SendDlgItemMessage(FIND_COMBO, CB_RESETCONTENT, 0, 0);
			for (cnt=1; cnt < cfg->FindMax && cfg->FindStr[cnt][0]; cnt++) {
				Wstr	find_w(cfg->FindStr[cnt], BY_UTF8);
				SendDlgItemMessageW(FIND_COMBO, CB_INSERTSTRING, cnt-1, (LPARAM)find_w.Buf());
			}
			SetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0]);
			SendDlgItemMessage(FIND_COMBO, CB_SETEDITSEL, 0, MAKELPARAM(start, end));
		}
		cfg->WriteRegistry(CFG_FINDHIST);
		return	TRUE;

	case IDCANCEL: case CLOSE_BUTTON:
		EndDialog(FALSE);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TFindDlg::EvCreate(LPARAM lParam)
{
	SendDlgItemMessage(ALLFIND_CHECK, BM_SETCHECK, cfg->FindAll, 0);

	for (int cnt=1; cnt < cfg->FindMax && cfg->FindStr[cnt][0]; cnt++) {
		Wstr	find_w(cfg->FindStr[cnt], BY_UTF8);
		SendDlgItemMessageW(FIND_COMBO, CB_INSERTSTRING, (WPARAM)cnt-1, (LPARAM)find_w.Buf());
	}
	if (cfg->FindStr[0][0])
		SetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0]);

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		rect.left += 140;
		rect.right += 140;
		rect.top -= 20;
		rect.bottom -= 20;
	}
	MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	Show();
	return	TRUE;
}

/*
	ファイルダイアログ用汎用ルーチン
*/
BOOL OpenFileDlg::Exec(UINT editCtl, char *title, char *filter, char *defaultDir)
{
	char buf[MAX_PATH_U8];

	if (parent == NULL)
		return FALSE;

	parent->GetDlgItemTextU8(editCtl, buf, sizeof(buf));

	if (!Exec(buf, title, filter, defaultDir))
		return	FALSE;

	parent->SetDlgItemTextU8(editCtl, buf);
	return	TRUE;
}

BOOL OpenFileDlg::Exec(char *target, char *title, char *filter, char *defaultDir)
{
	OPENFILENAME	ofn;
	char			szDirName[MAX_BUF_EX] = "", szFile[MAX_BUF_EX] = "", *fname = NULL;

	if (*target && GetFullPathNameU8(target, MAX_BUF, szDirName, &fname) != 0 && fname)
		*(fname -1) = 0, strncpyz(szFile, fname, MAX_PATH_U8);
	else if (defaultDir)
		strncpyz(szDirName, defaultDir, MAX_PATH_U8);

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = parent ? parent->hWnd : NULL;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = filter ? 1 : 0;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrTitle = title;
	ofn.lpstrInitialDir = szDirName;
	ofn.lpfnHook = hook;
	ofn.Flags = OFN_HIDEREADONLY|OFN_EXPLORER|(hook ? OFN_ENABLEHOOK : 0);
	if (mode == OPEN || mode == MULTI_OPEN)
		ofn.Flags |= OFN_FILEMUSTEXIST | (mode == MULTI_OPEN ? OFN_ALLOWMULTISELECT : 0);
	else
		ofn.Flags |= (mode == NODEREF_SAVE ? OFN_NODEREFERENCELINKS : 0);

	char	dirNameBak[MAX_PATH_U8];
	GetCurrentDirectoryU8(sizeof(dirNameBak), dirNameBak);

	BOOL	ret = (mode == OPEN || mode == MULTI_OPEN) ? GetOpenFileNameU8(&ofn) : GetSaveFileNameU8(&ofn);

	SetCurrentDirectoryU8(dirNameBak);
	if (ret)
	{
		if (mode == MULTI_OPEN)
			memcpy(target, szFile, sizeof(szFile));
		else
			strncpyz(target, ofn.lpstrFile, MAX_PATH_U8);

		if (defaultDir)
			strncpyz(defaultDir, ofn.lpstrFile, ofn.nFileOffset);
	}

	return	ret;
}

/*
	BrowseDirDlg用コールバック
*/
int CALLBACK BrowseDirDlgProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM path)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		SendMessageW(hWnd, BFFM_SETSELECTIONW, (WPARAM)TRUE, path);
		break;

	case BFFM_SELCHANGED:
		break;
	}
	return 0;
}

/*
	ディレクトリダイアログ用汎用ルーチン
*/
BOOL BrowseDirDlg(TWin *parentWin, const char *title, const char *defaultDir, char *buf)
{ 
	IMalloc			*iMalloc = NULL;
	BROWSEINFOW		brInfo;
	LPITEMIDLIST	pidlBrowse;
	BOOL			ret = FALSE;
	Wstr			buf_w(MAX_PATH), defaultDir_w(MAX_PATH), title_w(title);

	if (!SUCCEEDED(SHGetMalloc(&iMalloc)))
		return FALSE;

	U8toW(defaultDir, defaultDir_w.Buf(), MAX_PATH);
	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = buf_w.Buf();
	brInfo.lpszTitle = title_w;
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	brInfo.lpfn = BrowseDirDlgProc;
	brInfo.lParam = (LPARAM)defaultDir_w.Buf();
	brInfo.iImage = 0;

	if ((pidlBrowse = SHBrowseForFolderV((BROWSEINFO *)&brInfo)))
	{
		ret = SHGetPathFromIDListV(pidlBrowse, buf_w.Buf());
		iMalloc->Free(pidlBrowse);
		if (ret)
			WtoU8(buf_w, buf, MAX_PATH_U8);
	}

	iMalloc->Release();
	return	ret;
}

BOOL GetLastErrorMsg(char *msg, TWin *win)
{
	char	buf[MAX_BUF];
	wsprintf(buf, "%s error = %x", msg ? msg : "", GetLastError());
	return	MessageBox(win ? win->hWnd : NULL, buf, MSG_STR, MB_OK);
}

BOOL GetSockErrorMsg(char *msg, TWin *win)
{
	char	buf[MAX_BUF];
	wsprintf(buf, "%s error = %d", msg ? msg : "", WSAGetLastError());
	return	MessageBox(win ? win->hWnd : NULL, buf, MSG_STR, MB_OK);
}


/*
	パスからファイル名部分だけを取り出す
*/
BOOL PathToFname(const char *org_path, char *target_fname)
{
	char	path[MAX_BUF], *fname=NULL;

	if (GetFullPathNameU8(org_path, sizeof(path), path, &fname) == 0 || fname == NULL)
		return	FALSE;

	strncpyz(target_fname, fname, MAX_PATH_U8);
	return	TRUE;
}

/*
	パスからファイル名部分だけを取り出す（強制的に名前を作る）
*/
void ForcePathToFname(const char *org_path, char *target_fname)
{
	if (PathToFname(org_path, target_fname))
		return;

	if (org_path[1] == ':')
		wsprintf(target_fname, "(%c-drive)", *org_path);
	else if (org_path[0] == '\\' && org_path[1] == '\\') {
		if (!PathToFname(org_path + 2, target_fname))
			wsprintf(target_fname, "(root)");
	}
	else wsprintf(target_fname, "(unknown)");
}

/*
	2byte文字系でもきちんと動作させるためのルーチン
	 (*strrchr(path, "\\")=0 だと '表'などで問題を起すため)
*/
BOOL PathToDir(const char *org_path, char *target_dir)
{
	char	path[MAX_BUF], *fname=NULL;

	if (GetFullPathNameU8(org_path, sizeof(path), path, &fname) == 0 || fname == NULL)
		return	strncpyz(target_dir, org_path, MAX_PATH_U8), FALSE;

	if (fname - path > 3 || path[1] != ':')
		*(fname - 1) = 0;
	else
		*fname = 0;		// C:\ の場合

	strncpyz(target_dir, path, MAX_PATH_U8);
	return	TRUE;
}

/*
	fname にファイル名以外の要素が含まれていないことを確かめる
*/
BOOL IsSafePath(const char *fullpath, const char *fname)
{
	char	fname2[MAX_PATH_U8];

	if (!PathToFname(fullpath, fname2))
		return	FALSE;

	return	strcmp(fname, fname2) == 0 ? TRUE : FALSE;
}

#if 0
int MakePath(char *dest, const char *dir, const char *file)
{
	BOOL	separetor = TRUE;
	int		len;

	if ((len = strlen(dir)) == 0)
		return	wsprintf(dest, "%s", file);

	if (dir[len -1] == '\\')	// 表など、2byte目が'\\'で終る文字列対策
	{
		if (len >= 2 && !IsDBCSLeadByte(dir[len -2]))
			separetor = FALSE;
		else {
			for (u_char *p=(u_char *)dir; *p && p[1]; IsDBCSLeadByte(*p) ? p+=2 : p++)
				;
			if (*p == '\\')
				separetor = FALSE;
		}
	}
	return	wsprintf(dest, "%s%s%s", dir, separetor ? "\\" : "", file);
}
#endif

BOOL IsValidFileName(char *fname)
{
	return !strpbrk(fname, "\\/<>|:?*");
}



/*
	time() の代わり
*/
Time_t Time(void)
{
	SYSTEMTIME	st;
	_int64		ft;
// 1601年1月1日から1970年1月1日までの通算100ナノ秒
#define UNIXTIME_BASE	((_int64)0x019db1ded53e8000)

	::GetSystemTime(&st);
	::SystemTimeToFileTime(&st, (FILETIME *)&ft);
	return	(Time_t)((ft - UNIXTIME_BASE) / 10000000);
}

/*
	ctime() の代わり
	ただし、改行なし
*/
char *Ctime(SYSTEMTIME *st)
{
	static char	buf[] = "Mon Jan 01 00:00:00 2999";
	static char *wday = "SunMonTueWedThuFriSat";
	static char *mon  = "JanFebMarAprMayJunJulAugSepOctNovDec";
	SYSTEMTIME	_st;

	if (st == NULL)
	{
		st = &_st;
		::GetLocalTime(st);
	}
	wsprintf(buf, "%.3s %.3s %02d %02d:%02d:%02d %04d", &wday[st->wDayOfWeek * 3], &mon[(st->wMonth - 1) * 3], st->wDay, st->wHour, st->wMinute, st->wSecond, st->wYear);
	return	buf;
}

/*
	サイズを文字列に
*/
int MakeSizeString(char *buf, _int64 size, int flg)
{
	if (size >= 1024 * 1024)
	{
		if (flg & MSS_NOPOINT)
			return	wsprintf(buf, "%d%sMB", (int)(size / 1024 / 1024), flg & MSS_SPACE ? " " : "");
		else
			return	wsprintf(buf, "%d.%d%sMB", (int)(size / 1024 / 1024), (int)((size * 10 / 1024 / 1024) % 10), flg & MSS_SPACE ? " " : "");
	}
	else return	wsprintf(buf, "%d%sKB", (int)(ALIGN_BLOCK(size, 1024)), flg & MSS_SPACE ? " " : "");
}

/*
	strtok_r() の簡易版
*/
char *separate_token(char *buf, char separetor, char **handle)
{
	char	*_handle;

	if (handle == NULL)
		handle = &_handle;

	if (buf)
		*handle = buf;

	if (*handle == NULL || **handle == 0)
		return	NULL;

	while (**handle == separetor)
		(*handle)++;
	buf = *handle;

	if (**handle == 0)
		return	NULL;

	while (**handle && **handle != separetor)
		(*handle)++;

	if (**handle == separetor)
		*(*handle)++ = 0;

	return	buf;
}

void MakeHash(const BYTE *data, int len, char *hashStr)
{
	CBlowFish	bl((BYTE *)"ipmsg", 5);
	BYTE		*buf = new BYTE [len + 8];

	bl.Encrypt(data, buf, len);
	bin2hexstr(buf + len - 8, 8, hashStr);
	delete [] buf;
}

BOOL VerifyHash(const BYTE *data, int len, const char *orgHash)
{
	char	hash[MAX_NAMEBUF];

	MakeHash(data, len, hash);
	return	stricmp(hash, orgHash);
}

ULONG ResolveAddr(const char *_host)
{
	if (_host == NULL)
		return 0;

	ULONG	addr = ::inet_addr(_host);

	if (addr == 0xffffffff)
	{
		hostent	*ent = ::gethostbyname(_host);
		addr = ent ? *(ULONG *)ent->h_addr_list[0] : 0;
	}

	return	addr;
}

void TBrList::Reset()
{
	TBrObj	*obj;

	while ((obj = Top()))
	{
		DelObj(obj);
		delete obj;
	}
}

#if 0
BOOL TBrList::SetHost(const char *host)
{
	ULONG	addr = ResolveAddr(host);

	if (addr == 0 || IsExistHost(host))
		return	FALSE;

	SetHostRaw(host, addr);
	return	TRUE;
}

BOOL TBrList::IsExistHost(const char *host)
{
	for (TBrObj *obj=Top(); obj; obj=Next(obj))
		if (stricmp(obj->Host(), host) == 0)
			return	TRUE;

	return	FALSE;
}

#endif

char *GetLoadStrAsFilterU8(UINT id)
{
	char	*ret = GetLoadStrU8(id);
	if (ret) {
		for (char *p = ret; *p; p++) {
			if (*p == '!') {
				*p = '\0';
			}
		}
	}
	return	ret;
}

BOOL GetCurrentScreenSize(RECT *rect, HWND hRefWnd)
{
	rect->left = rect->top = 0;
	rect->right = ::GetSystemMetrics(SM_CXFULLSCREEN);
	rect->bottom = ::GetSystemMetrics(SM_CYFULLSCREEN);

	static BOOL (WINAPI *GetMonitorInfoV)(HMONITOR hMonitor, MONITORINFO *lpmi);
	static HMONITOR (WINAPI *MonitorFromPointV)(POINT pt, DWORD dwFlags);
	static HMONITOR (WINAPI *MonitorFromWindowV)(HWND hWnd, DWORD dwFlags);
	static HMODULE hUser32;

	if (!hUser32) {
		hUser32 = ::GetModuleHandle("user32.dll");
		GetMonitorInfoV = (BOOL (WINAPI *)(HMONITOR, MONITORINFO *))::GetProcAddress(hUser32, "GetMonitorInfoW");
		MonitorFromPointV = (HMONITOR (WINAPI *)(POINT, DWORD))::GetProcAddress(hUser32, "MonitorFromPoint");
		MonitorFromWindowV = (HMONITOR (WINAPI *)(HWND, DWORD))::GetProcAddress(hUser32, "MonitorFromWindow");
	}

	if (MonitorFromPointV && GetMonitorInfoV && MonitorFromWindowV) {
		POINT	pt;
		::GetCursorPos(&pt);

		HMONITOR	hMon = hRefWnd ? MonitorFromWindowV(hRefWnd, MONITOR_DEFAULTTONEAREST) : MonitorFromPointV(pt, MONITOR_DEFAULTTONEAREST);

		if (hMon) {
			MONITORINFO	info;
			info.cbSize = sizeof(info);

			if (GetMonitorInfoV(hMon, &info))
				*rect = info.rcMonitor;
		}
	}

	return	TRUE;
}


#define IPMSG_CLIPBOARD_PSEUDOFILE	"ipmsgclip_%s_%d.png"
#define IPMSG_CLIPBOARD_STOREDIR	"ipmsg_img"

void MakeClipFileName(int id, BOOL is_send, char *buf)
{
	sprintf(buf, IPMSG_CLIPBOARD_PSEUDOFILE, is_send ? "s" : "r", id);
}

BOOL MakeImageFolder(Cfg *cfg, char *dir)
{
	char	*fname = NULL;

	if (!GetFullPathNameU8(cfg->LogFile, MAX_PATH_U8, dir, &fname) && fname) return FALSE;
	strcpy(fname, IPMSG_CLIPBOARD_STOREDIR);
	return	TRUE;
}

BOOL SaveImageFile(Cfg *cfg, const char *fname, VBuf *buf)
{
	char	path[MAX_PATH_U8];
	DWORD	size;
	HANDLE	hFile;

	if (!MakeImageFolder(cfg, path)) return FALSE;
	CreateDirectoryU8(path, 0);
	strcat(path, "\\");
	strcat(path, fname);

	if ((hFile = CreateFileU8(path, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) return FALSE;

	WriteFile(hFile, buf->Buf(), buf->Size(), &size, 0);
	CloseHandle(hFile);

	return	TRUE;
}

int GetColorDepth()
{
	HWND	hWnd = GetDesktopWindow();
	HDC		hDc;
	int		ret = 0;

	if (!(hDc = ::GetDC(hWnd))) return 0;
	ret = ::GetDeviceCaps(hDc, BITSPIXEL);
	::ReleaseDC(hWnd, hDc);
	return	ret;
}


