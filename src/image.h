/*	@(#)Copyright (C) H.Shirouzu 2013-2016   image.h	Ver4.00 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Image
	Create					: 2013-03-03(Sun)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IMAGE_H
#define IMAGE_H

#define IPMSG_CAPTURE_CLASS "ipmsg_capture"

#define IS_SAME_PTS(pts1, pts2) (pts1.x == pts2.x && pts1.y == pts2.y)

typedef std::vector<POINTS>		VPts;
typedef VPts::iterator			VPtsItr;

struct ColPts{
	COLORREF	color;
	COLORREF	bgColor;
	DWORD		mode;	// MARKER_PEN/RECT/ARROW
	VPts		pts;
	std::string	memo; // for arrow
};

typedef std::vector<ColPts>	VColPts;
typedef VColPts::iterator	VColPtsItr;

#define COLOR_RED		RGB(255,0,0)
#define COLOR_GREEN		RGB(0,255,0)
#define COLOR_BLUE		RGB(0,0,255)
#define COLOR_YELLOW	RGB(255,255,0)

#define BGCOLOR_RED		RGB(255,255,255)
#define BGCOLOR_GREEN	RGB( 21,108, 48)
#define BGCOLOR_BLUE	RGB(255,255,255)
#define BGCOLOR_YELLOW	RGB(130,123,  0)

#define MARKER_OFFSET    3000
#define MARKER_UNDO      (MARKER_OFFSET + 0)
#define MARKER_PEN       (MARKER_OFFSET + 1)
#define MARKER_ARROW     (MARKER_OFFSET + 2)
#define MARKER_RECT      (MARKER_OFFSET + 3)
#define MARKER_COLOR     (MARKER_OFFSET + 4)
#define MARKER_TB_MAX    4

// ex is rect/arrow
inline DWORD CURSOR_IDX(COLORREF color, int ex=0) { return color + ex; }

class TImageWin;
class TAreaConfirmDlg : public TDlg {
protected:
	TImageWin	*parentWin;
	HWND		hToolBar;
	Cfg			*cfg;
	BOOL		*useClip;
	BOOL		*withSave;
	BOOL		reEdit;
	COLORREF	color;
	COLORREF	bgColor;
	DWORD		mode;

public:
	TAreaConfirmDlg(Cfg *_cfg, TImageWin *_parentWin);
	virtual BOOL		Create(BOOL *_useClip, BOOL *_withSave, BOOL _reEdit=FALSE);
	virtual BOOL		EvCreate(LPARAM lParam);
	virtual BOOL		EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL		EvNotify(UINT ctlID, NMHDR *pNmHdr);

	virtual COLORREF	GetColor() { return color; }
	virtual COLORREF	GetBgColor() { return bgColor; }
	virtual void		SetColor(COLORREF _color, COLORREF _bg_color);
	virtual DWORD		GetMode() { return mode; }
	virtual void		Notify();
};

typedef std::map<DWORD, HCURSOR>	CursorMap;
typedef CursorMap::iterator			CursorMapItr;

class TImageWin : public TWin {
protected:
	HDC				hSelfDc;
	HBITMAP			hSelfBmp;
	HBITMAP			hSelfBmpOld;
	HDC				hDarkDc;
	HBITMAP			hDarkBmp;
	HBITMAP			hDarkBmpOld;
	static CursorMap cursorMap;
	POINTS			areaPts[2];
	POINTS			lastPts;
	VColPts			drawPts;
	Cfg				*cfg;
	BOOL			useClip;
	BOOL			withSave;
	TAreaConfirmDlg	areaDlg;
	TSendDlg		*parentWnd;
	TInputDlg		inputDlg;
	TEditSub		*reEdit;
	int				sX, sY, sCx, sCy;
	TRect			reRc;
	TRect			DrawMarkerMemo(HDC hDc, const ColPts& col, POINT *pt, TRect max_rc);
	enum { INIT, START, END, DRAW_INIT, DRAW_START, DRAW_INPUT, DRAW_END } status;

public:
	TImageWin(Cfg *_cfg, TSendDlg *_parent);
	virtual ~TImageWin();
	virtual void	UnInit();
	virtual BOOL	Create(TEditSub *re_edit=NULL);
	virtual BOOL	ReEdit();
	virtual BOOL	MakeDarkBmp(int level);
	virtual void	Notify(int result);
	virtual void	SetMode(BOOL is_draw);
	virtual void	PopDrawHist();
	virtual size_t	DrawHistNum() { return drawPts.size(); }
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy();
	virtual BOOL	EvChar(WCHAR code, LPARAM keyData);
	virtual BOOL	EvPaint();
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EvTimer(WPARAM _timerID, TIMERPROC proc);
	virtual BOOL	DrawArea(POINTS *pts=NULL);
	virtual BOOL	DrawMarker(HDC hDc=NULL);
	virtual BOOL	CutImage(BOOL use_clip=TRUE, BOOL with_save=FALSE);
};

BITMAPINFO *BmpHandleToInfo(HBITMAP hBmp, int *size);
HBITMAP BmpInfoToHandle(BITMAPINFO *bmi, int size);
HBITMAP PngByteToBmpHandle(VBuf *vbuf);
VBuf *BmpHandleToPngByte(HBITMAP hBmp);
int GetColorDepth();

#endif
