static char *richedit_id = 
	"@(#)Copyright (C) H.Shirouzu 2011-2017   richedit.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Rich Edit Control and PNG-BMP convert
	Create					: 2011-05-03(Tue)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

#include <objidl.h>
#include <ole2.h>
#include <commctrl.h>
#include <richedit.h>
#include <oledlg.h>
#include <gdiplus.h>
#include <regex>
using namespace Gdiplus;
using namespace std;

class TDataObject : IDataObject {
private:
	ULONG		refCnt;
	HBITMAP		hBmp;
	HBITMAP		hBmpDdb;
	FORMATETC	fe;

public:
	TDataObject() {
		refCnt = 0;
		memset(&fe, 0, sizeof(fe));
		hBmpDdb = NULL;
	}
	~TDataObject() {
		if (hBmpDdb) {
			::DeleteObject(hBmpDdb);
		}
	}

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

	void InsertBitmap(IRichEditOle *richOle, HBITMAP _hBmp);
};

HRESULT TDataObject::QueryInterface(REFIID riid, LPVOID* ppv)
{
	*ppv = NULL;

	if (IID_IUnknown == riid || IID_IDataObject == riid) {
		*ppv = this;
	}
	else {
		Debug("TDataObject::QueryInterface noif %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n",
			riid.Data1, riid.Data2, riid.Data3,
			riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3],
			riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
		return ResultFromScode(E_NOINTERFACE);
	}

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
	if (!hDupBmp) {
		Debug("TDataObject::GetData OleDuplicateData failed\n");
		return E_HANDLE;
	}

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
	if (!ocs || !lb) {
		Debug("InsertBitmap: CreateILockBytesOnHGlobal failed %x\n", GetLastError());
		goto END;
	}

	::StgCreateDocfileOnILockBytes(lb, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &is);
	if (!is) {
		Debug("InsertBitmap: StgCreateDocfileOnILockBytes failed %x\n", GetLastError());
		goto END;
	}

	::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT, &fe, ocs, is,
		(void **)&oleObj);

	if (!oleObj) {		// 16bit環境用フォールバックルーチン
//		is->Release();	// （本来は DIB -> DDB変換ルーチンを用意する）
//		lb->Release();	// （DIBSECTIONが非対応の 8bit colorで問題）
//		ocs->Release();
//		richOle->GetClientSite(&ocs);
//		::CreateILockBytesOnHGlobal(NULL, TRUE, &lb);
//		::StgCreateDocfileOnILockBytes(lb, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &is);

		if (hBmpDdb = TDIBtoDDB(hBmp)) {
			hBmp = hBmpDdb;
		}
		::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT, &fe, ocs, is,
			(void **)&oleObj);

		if (!oleObj) {
			Debug("InsertBitmap: OleCreateStaticFromData failed %x\n", GetLastError());
			goto END;
		}
	}
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
	else {
		Debug("TRichEditOleCallback::QueryInterface failed\n");
		return ResultFromScode(E_NOINTERFACE);
	}

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
	if (!ppStg) {
		Debug("TRichEditOleCallback::GetNewStorage ppStg is NULL\n");
		return E_INVALIDARG;
	}

	*ppStg = NULL;

	LPLOCKBYTES	lb;
	HRESULT hr = ::CreateILockBytesOnHGlobal(NULL, TRUE, &lb);
	if (FAILED(hr))	{
		Debug("CreateILockBytesOnHGlobal err=%x\n", hr);
		return hr;
	}

	hr = ::StgCreateDocfileOnILockBytes(lb, STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE,
			0, ppStg);

	if (FAILED(hr))	{
		Debug("CreateILockBytesOnHGlobal err=%x\n", hr);
	}
	lb->Release();

	return hr;
}


HRESULT TRichEditOleCallback::GetInPlaceContext(LPOLEINPLACEFRAME* ppFrame,
			LPOLEINPLACEUIWINDOW* ppDoc, LPOLEINPLACEFRAMEINFO pFrameInfo)
{
	Debug("GetInPlaceContext not implement\n");
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::ShowContainerUI(BOOL fShow)
{
	Debug("ShowContainerUI not implement\n");
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

	if (pasteMode == 0 && (::GetKeyState(VK_SHIFT) & 0x8000)) {
		pasteMode = 1;
	}

	memset(enabled, 0, sizeof(enabled));

	memset(&fe, 0, sizeof(fe));
	fe.cfFormat = CF_HDROP;
	fe.dwAspect = DVASPECT_CONTENT;
	fe.lindex = -1;
	fe.tymed = TYMED_HGLOBAL;
	if (SUCCEEDED(dataObj->QueryGetData(&fe))) {
		return ret;
	}

	IEnumFORMATETC	*enum_fe = NULL;
	DWORD			num;

	HRESULT	hr;
	if (FAILED((hr = dataObj->EnumFormatEtc(DATADIR_GET, &enum_fe)))) {
		Debug("QueryAcceptData: EnumFormatEtc failed %x\n", hr);
		return S_FALSE;
	}

	while (SUCCEEDED(enum_fe->Next(num=1, &fe, &num)) && num == 1) {
//		editWnd->MessageBoxU8(Fmt("cf=%d",fe.cfFormat));
		switch (fe.cfFormat) {
		case CF_TEXT: case CF_UNICODETEXT:
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
			if ((cfg->ClipMode & CLIP_ENABLE)
			) {
				if (idx < 2 && enabled[idx] == 0 && (idx == 0 || enabled[0] == CF_TEXT)) {
					enabled[idx++] = fe.cfFormat;
				}
				if (!fReally) {
					break;
				}
				if (editWnd->GetImageNum() < cfg->ClipMax) {
					if (!hDelayBmp && SUCCEEDED(dataObj->GetData(&fe, &sm))) {
						if (fe.cfFormat == CF_BITMAP) {
							hDelayBmp = sm.hBitmap; // WM_DELAY_BITMAP で開放
						}
						else {
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
					editWnd->MessageBoxU8(LoadStrU8(IDS_TOOMANY_CLIP));
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
			if (hDelayBmp) {
				::DeleteObject(hDelayBmp);
			}
		}
		else if (hDelayBmp) {
			editWnd->PostMessage(WM_DELAY_BITMAP, 0, (LPARAM)hDelayBmp);
			if (delay_text) {
				free(delay_text);
			}
		}
	}
	pasteMode = 0;

	return ret;
}

HRESULT TRichEditOleCallback::ContextSensitiveHelp(BOOL fEnterMode)
{
	Debug("ContextSensitiveHelp not implement\n");
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::GetClipboardData(CHARRANGE* pchrg, DWORD reco,
			LPDATAOBJECT* ppDataObject)

{
	*ppDataObject = NULL;
	Debug("GetClipboardData not implement\n");
	return E_NOTIMPL;
}

HRESULT TRichEditOleCallback::GetDragDropEffect(BOOL fDrag, DWORD grfKeyState, LPDWORD pdwEffect)
{
	Debug("GetDragDropEffect not implement\n");
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

#ifndef AURL_ENABLEURL
#define AURL_ENABLEURL			1
#define AURL_ENABLEEMAILADDR	2
#define AURL_ENABLETELNO		4
#define AURL_ENABLEEAURLS		8
#define AURL_ENABLEDRIVELETTERS	16
#endif

BOOL TEditSub::AttachWnd(HWND _hWnd)
{
	// Protection for Visual C++ Resource editor problem...
	// RICHEDIT20W is correct, but VC++ changes to RICHEDIT20A, sometimes.
#ifdef _DEBUG
#define RICHED20A_TEST
#ifdef RICHED20A_TEST
	char	cname[64];
	if (::GetClassName(_hWnd, cname, sizeof(cname)) && stricmp(cname, "RICHEDIT20A") == 0) {
		MessageBox("Change RichEdit20A to RichEdit20W in ipmsg.rc", "IPMSG Resource file problem");
	}
#endif
#endif

	if (!TSubClassCtl::AttachWnd(_hWnd)) return	FALSE;

	if ((cb = new TRichEditOleCallback(cfg, this, this->parent))) {
		cb->AddRef();
		if (!SendMessage(EM_SETOLECALLBACK, 0, (LPARAM)cb)) {
			Debug("EM_SETOLECALLBACK err=%x\n", GetLastError());
		}
	}

	richOle = NULL;
	if (!SendMessage(EM_GETOLEINTERFACE, 0, (LPARAM)&richOle)) {
		Debug("EM_GETOLEINTERFACE err=%x\n", GetLastError());
	}

	if (IsWin8()) {
		SendMessage(EM_AUTOURLDETECT,
			AURL_ENABLEURL			|
			AURL_ENABLEEAURLS		|
			((cfg->linkDetectMode & 1) ? 0 : AURL_ENABLEDRIVELETTERS)
			, 0);
	}
	else {
		SendMessage(EM_AUTOURLDETECT, AURL_ENABLEURL, 0);
	}
	SendMessage(EM_SETTARGETDEVICE, 0, 0);		// 折り返し

	LRESULT	evMask = SendMessage(EM_GETEVENTMASK, 0, 0) | ENM_LINK | ENM_CHANGE;
	SendMessage(EM_SETEVENTMASK, 0, evMask); 

	dblClicked = FALSE;
	selStart = selEnd = 0;
	return	TRUE;
}

BOOL TEditSub::EvChar(WCHAR code, LPARAM keyData)
{
//	Debug("EvChar code=%x keyData=%zx\n", code, keyData);
	return	FALSE;
}

BOOL TEditSub::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
//	Debug("EvKey(msg=%x) key=%x keyData=%zx\n", uMsg, nVirtKey, lKeyData);
	return	FALSE;
}

BOOL TEditSub::PreProcMsg(MSG *msg)
{
	return	TSubClassCtl::PreProcMsg(msg);
}

int TEditSub::SelectedImageIndex()
{
	int		image_num = GetImageNum();
	DWORD	start  = 0;
	DWORD	end    = 0;

	SendMessageW(EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

	if ((end - start) == 1) {
		for (int i=0; i < image_num; i++) {
			if (GetImagePos(i) == start) {
				return	i;
			}
		}
	}

	return	-1;
}

void TEditSub::SaveSelectedImage()
{
	int	idx = SelectedImageIndex();
	if (idx == -1) return;

	VBuf	*vbuf = GetPngByte(idx);
	if (!vbuf) return;

	char	fname[MAX_PATH_U8];
	MakeImageTmpFileName(cfg->lastSaveDir, fname);

	OpenFileDlg	dlg(this->parent, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);

	if (dlg.Exec(fname, sizeof(fname), NULL, "PNG file(*.png)\0*.png\0\0", cfg->lastSaveDir, "png")) {
		HANDLE	hFile = CreateFileWithDirU8(fname, GENERIC_WRITE,
									 FILE_SHARE_READ|FILE_SHARE_WRITE,
									 NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		DWORD	size = 0;

		if (hFile != INVALID_HANDLE_VALUE) {
			WriteFile(hFile, vbuf->Buf(), (DWORD)vbuf->Size(), &size, 0);
			CloseHandle(hFile);
			GetParentDirU8(fname, cfg->lastSaveDir);
			cfg->WriteRegistry(CFG_GENERAL);
			TOpenExplorerSelOneW(U8toWs(fname));
		}
	}
	delete vbuf;
}

void TEditSub::InsertImage()
{
	char		fname[MAX_PATH_U8] = "";
	char		dir[MAX_PATH_U8] = "";
	OpenFileDlg	dlg(this->parent, OpenFileDlg::OPEN);

	MakeImageFolderName(cfg, dir);

	if (dlg.Exec(fname, sizeof(fname), NULL,
		"Image\0*.png;*.jpg;*.jpeg;*.gif;*.tiff;*.ico;*.bmp;*.wmf;*.wmz;*.emf;*.emz\0\0", dir)) {
		Bitmap	*bmp = Bitmap::FromFile(Wstr(fname).s());
		HBITMAP	hBmp = NULL;
		if (bmp && !bmp->GetHBITMAP(Color::White, &hBmp) && hBmp) {
			InsertBitmapByHandle(hBmp, -1);
			::DeleteObject(hBmp);
		}
		delete bmp;
	}
}

HMENU TEditSub::CreatePopupMenu()
{
	HMENU	hMenu = ::CreatePopupMenu();
	BOOL	is_readonly = BOOL(GetWindowLong(GWL_STYLE) & ES_READONLY);
	BOOL	is_paste = BOOL(SendMessage(EM_CANPASTE, 0, 0));
	BOOL	is_clip_enable  = (cfg->ClipMode & CLIP_ENABLE)
				? TRUE : FALSE;
	BOOL	is_cbimg = (cb && cb->enabled[0] != CF_TEXT) ? TRUE : FALSE;

	AppendMenuU8(hMenu, MF_STRING|((is_readonly || !SendMessage(EM_CANUNDO, 0, 0)) ?
				MF_DISABLED|MF_GRAYED : 0), WM_UNDO, LoadStrU8(IDS_UNDO));
	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING|(is_readonly ? MF_DISABLED|MF_GRAYED : 0), WM_CUT,
				LoadStrU8(IDS_CUT));
	AppendMenuU8(hMenu, MF_STRING, WM_COPY, LoadStrU8(IDS_COPY));
	AppendMenuU8(hMenu, MF_STRING|((is_readonly || !is_paste || (!is_clip_enable && is_cbimg)) ?
				MF_DISABLED|MF_GRAYED : 0), WM_PASTE, LoadStrU8(IDS_PASTE));
	if (cb && cb->enabled[1] && !is_readonly && is_paste && is_clip_enable) {
		AppendMenuU8(hMenu, MF_STRING, WM_PASTE_REV,
				LoadStrU8(cb->enabled[1] == CF_TEXT ? IDS_PASTE_TEXT : IDS_PASTE_BMP));
	}
	AppendMenuU8(hMenu, MF_STRING|(is_readonly ? MF_DISABLED|MF_GRAYED : 0), WM_CLEAR,
				LoadStrU8(IDS_DELETE));
	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING, EM_SETSEL, LoadStrU8(IDS_SELECTALL));

	/* 画像保存 */
	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING|(SelectedImageIndex() < 0 ? MF_DISABLED|MF_GRAYED : 0),
					WM_SAVE_IMAGE, LoadStrU8(IDS_SAVE_IMAGE));
	if (!is_readonly && is_clip_enable) {
		AppendMenuU8(hMenu, MF_STRING|(SelectedImageIndex() < 0 ? MF_DISABLED|MF_GRAYED : 0),
						WM_EDIT_IMAGE, LoadStrU8(IDS_EDIT_IMAGE));
		AppendMenuU8(hMenu, MF_STRING, WM_INSERT_IMAGE, LoadStrU8(IDS_INSERT_IMAGE));
	}

	return	hMenu;
}

BOOL TEditSub::SetHyperLink()
{
//static BOOL is_using;
//	if (is_using) {
//		Wstr	wbuf(MAX_UDPBUF);
//		ExGetText(wbuf.Buf(), MAX_UDPBUF, GT_DEFAULT, 1200);
//		DebugW(L"* %s\n", wbuf.s());
//		return FALSE;
//	}
//	is_using = TRUE;
//
//	SendMessage(WM_SETREDRAW, FALSE, 0);
//	Wstr	wbuf(MAX_UDPBUF);
//
//	if (ExGetText(wbuf.Buf(), MAX_UDPBUF, GT_DEFAULT, 1200) <= 0) {
//		is_using = FALSE;
//		SendMessage(WM_SETREDRAW, TRUE, 0);
//		return	FALSE;
//	}
//	DebugW(wbuf.s());
//
//	std::vector<LinkAttr>	tmpAttr;
//
////	LRESULT	evMask = SendMessage(EM_GETEVENTMASK, 0, 0) | ENM_LINK | ENM_CHANGE;
////	SendMessage(EM_SETEVENTMASK, 0, evMask); 
//
//	POINT		pt;
//	SendMessageW(EM_GETSCROLLPOS, 0, (LPARAM)&pt);
//	CHARRANGE	cr;
//	SendMessageW(EM_EXGETSEL, 0, (LPARAM)&cr);
//
//	wcmatch	match;
//	wregex	&main_re = (cfg->linkDetectMode == 0) ? *cfg->urlex_re : *cfg->url_re;
//
//	CHARFORMAT2W	cf_def = {};
//	cf_def.cbSize	= sizeof(cf_def);
//	cf_def.dwMask	= CFM_LINK;
//
//	CHARFORMAT2W	cf_lnk = cf_def;
//	cf_lnk.dwEffects	= CFE_LINK;
//
//	const WCHAR	*s = wbuf.s();
//	int			prev_i = 0;
//	for (int i=0; s[i]; i=prev_i) {
//		if (!regex_search(s+i, match, main_re)) {
//			break;
//		}
//		if (cfg->linkDetectMode && !regex_search(s+i, match, *cfg->fileurl_re)) {
//			break;
//		}
//		LinkAttr	attr = { i + (int)match.position(0), (int)match.length(0) };
//		tmpAttr.push_back(attr);
//
//		if (prev_i < attr.pos) {
//			CHARRANGE	cr_def = { prev_i, attr.pos };
//			SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr_def);
//			SendMessageW(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf_def);
//		}
//		CHARRANGE	cr_lnk = { attr.pos, attr.pos + attr.len };
//		SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr_lnk);
//		SendMessageW(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf_lnk);
//		prev_i = cr_lnk.cpMax;
//		break;
//	}
////	CHARRANGE	cr_def = { prev_i, -1 };
////	SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr_def);
////	SendMessageW(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf_def);
//
////	SendMessageW(EM_SETSCROLLPOS, 0, (LPARAM)&pt);
//	SendMessageW(EM_EXSETSEL, 0, (LPARAM)&cr);
//
//	linkAttr = tmpAttr;
//
//	is_using = FALSE;
//	SendMessage(WM_SETREDRAW, TRUE, 0);
//	InvalidateRect(0, TRUE);

	return	TRUE;
}

BOOL TEditSub::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_CLEAR:
		SendMessage(wID, 0, 0);
		return	TRUE;

	case WM_PASTE:
		if (cb) {
			cb->pasteMode = 3; // ignore shift check
		}
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

	case WM_SAVE_IMAGE:
		SaveSelectedImage();
		return	TRUE;

	case WM_INSERT_IMAGE:
		InsertImage();
		return	TRUE;

	case WM_EDIT_IMAGE:
		parent->PostMessage(WM_EDIT_IMAGE, 0, -1);
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
	if (uMsg == WM_SETFOCUS) {
		SendMessageW(EM_SETSEL, (WPARAM)selStart, (LPARAM)selEnd);
	}
	else if (uMsg == WM_KILLFOCUS) {
		SendMessageW(EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
	}
	return	FALSE;
}

BOOL TEditSub::EvContextMenu(HWND childWnd, POINTS pos)
{
	HMENU	hMenu = CreatePopupMenu();

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	return	TRUE;
}

BOOL TEditSub::JumpLink(ENLINK *el, BOOL is_dblclick)
{
	Wstr	wbuf(MAX_UDPBUF);
	if (ExGetText(wbuf.Buf(), MAX_UDPBUF, GT_DEFAULT, 1200) <= 0) {
		return	FALSE;
	}

	LONG	top = el->chrg.cpMin;
	LONG	end = el->chrg.cpMax;

	Wstr	link(wbuf.s() + top, (end >= 0) ? end - top : -1);

	return	JumpLinkWithCheck(parent, cfg, link.s(), is_dblclick ? JLF_DBLCLICK : 0) == JLR_OK;
}

BOOL TEditSub::LinkSel(ENLINK *el)
{
	SendMessageW(EM_EXSETSEL, 0, (LPARAM)&el->chrg);
	return	TRUE;
}

BOOL TEditSub::ExpandSel()
{
	CHARRANGE	cr;
	LONG		&start	= cr.cpMin;
	LONG		&end	= cr.cpMax;
//	LONG		start = 0;
//	LONG		end	= 0;

	SendMessageW(EM_EXGETSEL, 0, (LPARAM)&cr);
//	SendMessageW(EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

	if (start >= end || end >= MAX_UDPBUF) {
		return TRUE;
	}

	unique_ptr<WCHAR []> wbuf = make_unique<WCHAR[]>(MAX_UDPBUF);
	BOOL	modify = FALSE;
	WCHAR	*terminate_chars = L"\r\n\t \x3000";	// \x3000 ... 全角空白

	int max_len = ExGetText(wbuf.get(), MAX_UDPBUF, GT_DEFAULT, 1200);	// 1200 == UNICODE
//	int max_len = ::GetWindowTextW(hWnd, wbuf, MAX_UDPBUF);
	max_len = min(max_len, MAX_UDPBUF-1);
	end = min(end, MAX_UDPBUF - 1);

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
//		SendMessageW(EM_SETSEL, start, end);
	}
	memmove(wbuf.get(), wbuf.get() + start, (end - start) * sizeof(WCHAR));
	wbuf[end - start] = 0;
	auto u8buf = scope_raii(WtoU8(wbuf.get()), [](auto p) { delete[] p; });

	UrlObj	*obj = NULL;
	char	*url_ptr = NULL;
	if ((url_ptr = strstr(u8buf.get(), URL_STR))) {
		char	proto[MAX_NAMEBUF];

		strncpyz(proto, u8buf.get(), int(min(url_ptr - u8buf.get() + 1, sizeof(proto))));
		for (int i=0; proto[i]; i++) {
			if ((obj = SearchUrlObj(&cfg->urlList, proto + i))) {
				url_ptr = u8buf.get() + i;
				break;
			}
		}
	}
	if (obj && *obj->program) {
		if (LONG_RDC(ShellExecuteU8(NULL, NULL, obj->program, url_ptr ? url_ptr : u8buf.get(),
				NULL, SW_SHOW)) <= WINEXEC_ERR_MAX) {
			MessageBoxU8(obj->program, LoadStrU8(IDS_CANTEXEC), MB_OK|MB_ICONINFORMATION);
		}
	}
	else if (!url_ptr && cfg->ShellExec || url_ptr && cfg->DefaultUrl) {
		ShellExecuteU8(NULL, NULL, url_ptr ? url_ptr : u8buf.get(), NULL, NULL, SW_SHOW);
	}
	return	TRUE;
}



BOOL TEditSub::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_EDIT_DBLCLK:
//		ExpandSel();
		return	TRUE;

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
	SendMessage(EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

	LRESULT	langOpts = SendMessage(EM_GETLANGOPTIONS, 0, 0);
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

	int	ret = (int)SendMessageW(EM_GETTEXTEX, (WPARAM)&ge, (LPARAM)buf);

	return	ret;
}

void TEditSub::GetCurSel(DWORD *start, DWORD *end)
{
	CHARRANGE	cr;

	SendMessageW(EM_EXGETSEL, 0, (LPARAM)&cr);

	*start = cr.cpMin;
	*end   = cr.cpMax;
}

int TEditSub::GetTextUTF8(char *buf, int max_len, BOOL force_select)
{
	int			max_wlen = (int)SendMessageW(WM_GETTEXTLENGTH, 0, 0) + 1;
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

	WtoU8(wbuf.WBuf(), cbuf, max_len);
	u_char	*s   = cbuf;
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

	return	int(d - buf);
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

	return	(int)SendMessageW(EM_STREAMOUT, flags, (LPARAM)&es);
}

//DWORD CALLBACK RichStreamCallback2(DWORD_PTR dwCookie, BYTE *buf, LONG cb, LONG *pcb)
//{
//	if (*(DWORD *)dwCookie) return 1;
//	*(DWORD *)dwCookie = 1;
//
//	char data[] =  "RTF";
//
// 	memcpy(buf, data, sizeof(data));
//	*pcb = sizeof(data);
//
//	return 0;
//}
//
//int TEditSub::SetStreamText()
//{
//	DWORD		end = 0;
//	EDITSTREAM	es = { (DWORD_PTR)&end, 0, RichStreamCallback2 };
//
//	return	(int)SendMessageW(EM_STREAMIN, SF_RTF, (LPARAM)&es);
//}

int TEditSub::ExSetText(const void *buf, int max_len, DWORD flags, UINT codepage)
{
	SETTEXTEX	se;

	se.flags    = flags;
	se.codepage = codepage;

	return	(int)SendMessageW(EM_SETTEXTEX, (WPARAM)&se, (LPARAM)buf);
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

#include <Shlwapi.h>

void TEditSub::InsertBitmap(BITMAPINFO	*bmi, int size, int pos)
{
	HBITMAP	hBmp = BmpInfoToHandle(bmi, size);

	if (hBmp) {
		InsertBitmapByHandle(hBmp, pos);
		::DeleteObject(hBmp);
	}
}

BOOL TEditSub::InsertPng(VBuf *vbuf, int pos)
{
	HBITMAP	hBmp = NULL;

	hBmp = PngByteToBmpHandle(vbuf);
	if (!hBmp) return FALSE;

	InsertBitmapByHandle(hBmp, pos);
	::DeleteObject(hBmp);

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

	__try {
		if (SUCCEEDED(richOle->GetObject(idx, &reobj, REO_GETOBJ_POLEOBJ))) {
			if (reobj.poleobj) {
				if (pos) {
					*pos = reobj.cp;
				}
				if (SUCCEEDED(reobj.poleobj->QueryInterface(IID_IDataObject, (void **)&dobj))) {
					STGMEDIUM	sm;
					FORMATETC	fe;
					memset(&fe, 0, sizeof(fe));
					fe.cfFormat	= CF_BITMAP;
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
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
		Debug("exception in GetPngByte (for wine)\n");
	}

	return	 buf;
}

HBITMAP TEditSub::GetBitmap(int idx, int *pos)
{
	if (!richOle) return NULL;

	HBITMAP			hBmp = NULL;
	LPDATAOBJECT	dobj = NULL;
	REOBJECT		reobj;

	memset(&reobj, 0, sizeof(REOBJECT));
	reobj.cbStruct = sizeof(REOBJECT);

	HRESULT	hr;
	if (SUCCEEDED((hr = richOle->GetObject(idx, &reobj, REO_GETOBJ_POLEOBJ)))) {
		if (pos) *pos = reobj.cp;
		if (SUCCEEDED((hr = reobj.poleobj->QueryInterface(IID_IDataObject, (void **)&dobj)))) {
			STGMEDIUM	sm = {};
			FORMATETC	fe;
			memset(&fe, 0, sizeof(fe));
			fe.cfFormat	= CF_BITMAP;
			fe.dwAspect	= DVASPECT_CONTENT;
			fe.lindex	= -1;
			fe.tymed	= TYMED_GDI;

			if (SUCCEEDED((hr = dobj->GetData(&fe, &sm)))) {
				hBmp = (HBITMAP)::CopyImage(sm.hBitmap, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
				::ReleaseStgMedium(&sm);
			}
			else {
				Debug("GetBitmap GetData failed %x\n", hr);
			}
			dobj->Release();
		}
		else {
			Debug("GetBitmap QueryInterface failed %x\n", hr);
		}
		reobj.poleobj->Release();
	}
	else {
		Debug("GetBitmap failed %x\n", hr);
	}

	return	 hBmp;
}

int TEditSub::GetImagePos(int idx)
{
	if (!richOle) return -1;

	LPDATAOBJECT	dobj     = NULL;
	REOBJECT		reobj;

	memset(&reobj, 0, sizeof(REOBJECT));
	reobj.cbStruct = sizeof(REOBJECT);

	HRESULT	hr;
	if (!SUCCEEDED((hr = richOle->GetObject(idx, &reobj, REO_GETOBJ_NO_INTERFACES)))) {
		Debug("GetImagePos failed %x\n", hr);
		reobj.cp = -1;
	}
	return	 reobj.cp;
}

int TEditSub::GetImageNum()
{
	if (!richOle) return 0;

	return	richOle->GetObjectCount();
}


