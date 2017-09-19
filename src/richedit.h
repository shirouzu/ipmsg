/*	@(#)Copyright (C) H.Shirouzu 2013-2017   richedit.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: RichEdit
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef RICHEDIT_H
#define RICHEDIT_H

class TRichEditOleCallback;

struct LinkAttr {
	int	pos;
	int	len;
};

class TEditSub : public TSubClassCtl {
protected:
	Cfg						*cfg;
	BOOL					dblClicked;
	TRichEditOleCallback	*cb;
	IRichEditOle			*richOle;
	DWORD					selStart;
	DWORD					selEnd;
	std::vector<LinkAttr>	linkAttr;

public:
	TEditSub(Cfg *_cfg, TWin *_parent);
	virtual ~TEditSub();

	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	PreProcMsg(MSG *msg);

	virtual	HMENU	CreatePopupMenu();
	virtual BOOL	SetFont(LOGFONT *lf, BOOL dualFont=FALSE);

	virtual void	InsertBitmapByHandle(HBITMAP hBmp, int pos);
	virtual void	InsertBitmap(BITMAPINFO	*bmi, int size, int pos);
	virtual void	InsertImage();

	virtual BOOL	InsertPng(VBuf *vbuf, int pos);
	virtual VBuf	*GetPngByte(int idx, int *pos=NULL);
	virtual HBITMAP	GetBitmap(int idx, int *pos=NULL);
	virtual int		GetImagePos(int idx);
	virtual int		GetImageNum();

	virtual void	SaveSelectedImage();
	virtual int		SelectedImageIndex();

	virtual int		GetTextUTF8(char *buf, int max_len, BOOL force_select=FALSE);
	virtual int		ExGetText(void *buf, int max_len, DWORD flags=GT_USECRLF,
						UINT codepage=CP_UTF8);
	virtual BOOL	ExSetText(const void *buf, int max_len=-1, DWORD flags=ST_DEFAULT,
						UINT codepage=CP_UTF8);
	virtual BOOL	GetStreamText(void *buf, int max_len, DWORD flags=SF_TEXTIZED|SF_TEXT);
//	virtual BOOL	SetStreamText();

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
	virtual BOOL	EvChar(WCHAR code, LPARAM keyData);
	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
	virtual void	GetCurSel(DWORD *start, DWORD *end);

	virtual void	GetCurSelLast(DWORD *start, DWORD *end) {
		*start = selStart;
		*end   = selEnd;
	}
	virtual void	SetCurSelLast(DWORD start, DWORD end) {
		selStart = start;
		selEnd   = end;
	}

	BOOL	ExpandSel();
	BOOL	SetHyperLink();
	BOOL	LinkSel(ENLINK *el);
	BOOL	JumpLink(ENLINK *el, BOOL is_dblclick);
};

#endif

