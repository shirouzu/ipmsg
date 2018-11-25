/*	@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logchildview.h	Ver4.60 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child Pane
	Create					: 2015-04-10(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGCHILDVIEW_H
#define LOGCHILDVIEW_H

#include <vector>
#include <list>
#include <unordered_map>
#include "logviewitem.h"
#include "logeditdlg.h"
#include "logdb.h"

class TLogView;

#define VIEWCHILD_CLASS	"TLogChildView"

#define CVW_GAPX		0
#define CVW_GAPY		0
#define CMNT_GAP		8
#define FOLD_GAP		4
#define MINI_GAP		2
#define MULTI_GAP		4
#define LINE_GAP		6
#define BODY_GAP		7
#define ITEM_GAP		5
#define ITEMW_GAP		10

#define FIND_TIMER		1000
#define FIND_TICK 100

#define FINDNEXT_TIMER	1001
#define CAPTURE_TIMER	1002
#define FINDNEXT_TICK	200

#define UPDATE_TIMER	1003
#define UPDATE_TICK		5000

#define SCROLL_TIMER	1004
#define SCROLL_TICK		10

struct BodyAttr {
	int	pos;
	int	len;
};

struct ViewMsg;

struct MsgBmp : TListObj {
	HBITMAP	hBmp;
	ViewMsg	*msg;

	MsgBmp() {
		hBmp = NULL;
		msg = NULL;
	}
	~MsgBmp() {
		UnInit();
	}
	void Init() {
		UnInit();
	}
	void UnInit() {
		if (hBmp) {
			::DeleteObject(hBmp);
			hBmp = NULL;
		}
	}
};

struct Tep {	// GetTextExtentExPointW cache
	int		offset;
	int		maxLen;
	int		rlen;
	short	width;
	short	cx;
};

struct ViewMsg : TListObj {
	LogMsg	msg;
//	MsgBmp	bmp;

	// for disp cache
	DWORD					dispFlags;
	TSize					sz;			// cx: 描画時のvrc.cx(), cy: 描画結果cy
	std::vector<BodyAttr>	link;
	std::vector<BodyAttr>	linkCmnt;
	std::vector<BodyAttr>	find;
	std::vector<Tep>		tep;
	DWORD					findGenNo;
	DWORD					dispGenNo;
	DWORD					tepGenNo;
	int						dispYOff;	// 最終描画時の yOffset
	DWORD					foldGenNo;	// 折り畳みを解除するか（view->foldGenNo以上で有効）
	bool					foldMsg;	// 折りたたむ必要のあるメッセージ
	bool					unOpenR;
	int						quotePos;

	ViewMsg() {
		Init(FALSE);
	}
	void Init(BOOL re_init=TRUE) {
		if (re_init) {
			msg.Init();
			sz.Init();
			link.clear();
			linkCmnt.clear();
			find.clear();
			tep.clear();
		}
		dispFlags = 0;
		findGenNo = 0;
		dispGenNo = 0;
		tepGenNo  = 0;
		dispYOff  = 0;
		foldGenNo = 0;
		foldMsg   = false;
		unOpenR   = false;
		quotePos  = 0;
	}
};

class TChildView : public TWin {
public:
	// main state
	enum DispFlags {
			DISP_NARROW			= 0x0001,
			DISP_TITLE_ONLY		= 0x0002,
			DISP_FILE_ONLY		= 0x0004,
			DISP_CLIP_ONLY		= 0x0008,
			DISP_FAV_ONLY		= 0x0010,
			DISP_MARK_ONLY		= 0x0020,
			DISP_UNOPENR_ONLY	= 0x0040,
			DISP_OVERLAPPED		= 0x0100, // by mouse over & click
			DISP_OVERLAPPED_EX	= 0x0200, // by click only
			DISP_OVERLAPPED_ALL	= 0x0300,
			DISP_REV			= 0x1000,
			DISP_NODRAW			= 0x2000,
			DISP_FOLD_QUOTE		= 0x4000,
	};
	bool IsRealDraw() {
		return (dispFlags & DISP_NODRAW) == 0;
	}

protected:
	struct SelItem {
		int64	msg_id;
		int		pos;
		BOOL	inc_head;
		SelItem() {
			Init();
		}
		void Init() {
			msg_id = 0;
			pos = 0;
			inc_head = FALSE;
		}
		BOOL IsValid() {
			return	msg_id ? TRUE : FALSE;
		}
	};
	struct BreakLineInfo {
		int		break_pos;
		int		link_idx;
		int		find_idx;
		int		sel_idx;
		int		mark_idx;
	};

	struct DownloadLinkStat {
		BOOL	link_ok;
		BOOL	targ_ok;
		BOOL	is_dir;
		BOOL	is_direct;
		WCHAR	wtarg[MAX_PATH];
		WCHAR	wlink[MAX_PATH];
		DownloadLinkStat() {
			Init();
		}
		void Init() {
			link_ok = FALSE;
			targ_ok = FALSE;
			is_dir		= FALSE;
			is_direct	= FALSE;
			*wtarg = 0;
			*wlink = 0;
		}
	};

protected:
	Cfg			*cfg;
	TLogView	*parentView;
	BOOL		isMain;
	int			range = -1;
	LogMng		*logMng;
	MsgMng		*msgMng;
	LogDb		*logDb;
	ItemMgr		itemMgr;
	DWORD		findGenNo;	// find generic no
	DWORD		dispGenNo;	// disp generic no
	DWORD		tepGenNo;
	DWORD		foldGenNo;
	int64		beginMsgId;
	SYSTEMTIME	curTime;

	VBVec<MsgVec>		msgIds;
	VBVec<MsgVec>		msgFindIds;	// 検索するが絞り込みなし
	VBVec<LogHost>		hostList;

	std::unordered_map<int64, ViewMsg *>	msgMap;
	TListEx<ViewMsg>						msgLru;	// 扱う実体は ViewMsg *
	TListEx<MsgBmp>							bmpLru;	// 扱う実体は ViewMsg *

	enum {	// UpdateUserList flags
		UF_NONE				= 0x0000,
		UF_FORCE			= 0x0001,
		UF_NO_FINDMOVE		= 0x0002,
		UF_NO_INITSEL		= 0x0004,
		UF_NO_FITSIZE		= 0x0008,
		UF_TO_DSC			= 0x0100,
		UF_TO_ASC			= 0x0200,
		UF_UPDATE_BODYLRU	= 0x1000,
	};

	DWORD		dispFlags;
	int			curIdx;	// index of msgIds (both implicit and explicit)
	int			focusIdx;	// focusRendered Cur Index
	int64		userMsgId; // explicit user select msgIds
	int64		savedCurMsgId;	// for reopen viewer
	int64		lastReplyBtnId;

	int			scrHiresTopPix;
	int			scrHiresTopIdx;
	int			scrHiresEndPix;
	int			scrHiresEndIdx;
	int			scrAllRangePix;
	DWORD		scrHresReduce;
	int			scrCurPos;
	int			scrTrackNpos;
	enum { MOUSE_SCR_INIT, MOUSE_SCR_REQ, MOUSE_SCR_DONE } mouseScrMode;

	int			offsetPix;
	int			scrOffPix;

	int			startDispIdx;
	int			startDispPix;
	int			endDispIdx;
	int			endDispPix;
	enum		ScrollMode { NONE_SCROLL, AUTO_SCROLL };
	ScrollMode	scrollMode;

	Wstr		findUser;
	Wstr		findBody;
	Wstr		findBodyOrg;
	std::vector<Wstr> findBodyVec;
	DWORD		findTick;
	DWORD		findUpdateFlags;

	Item		*cropUserItem;
	Item		*fileItem;
	Item		*itemMenu;

	SelItem		selTop;
	SelItem		selMid;
	SelItem		selEnd;
	BOOL		isCapture;
	TPoints		lastMousePos;
	DWORD		dblClickStart; // ヘッダ行ダブルクリック用

	// measure
	int			toolBarCy;
	int			statusCtrlCy;
	int			fontCy;
	int			fontLdCy;
	int			lineCy;
	int			lineDiff;
	int			lineDiff3;
	int			headCy;
	int			headDiff;
	int			headDiff2;
	int			headOnlyCy;
	int			headNormCy;
	int			maxLineChars;
	int			maxLines;
	int			maxFoldCy;

	TRect		crc;
	TRect		vrc;

	// Resources
	HBITMAP		hMemBmp;
	HBITMAP		hMemBmpSave;

	HBITMAP		hToMsgBmp;
	HBITMAP		hToMsg2Bmp;
	HBITMAP		hFromMsgBmp;
	HBITMAP		hReplyMsgBmp;
	HBITMAP		hCropUserBmp;
	HBITMAP		hReplyBmp;
	HBITMAP		hReplyHideBmp;
	HBITMAP		hFileBmp;
	HBITMAP		hClipBmp;
	HBITMAP		hFavBmp;
	HBITMAP		hFavMidBmp;
	HBITMAP		hFavRevBmp;
	HBITMAP		hMarkBmp;
	HBITMAP		hMenuBmp;
	HBITMAP		hImenuBmp;
	HBITMAP		hImenuMidBmp;
	HBITMAP		hBaseHeadBmp;
	HBITMAP		hAscBmp;
	HBITMAP		hDscBmp;
	HBITMAP		hCmntBmp;
	HBITMAP		hBottomBmp;
	HBITMAP		hTopBmp;
	HBITMAP		hBottomHotBmp;	// autoscroll
	HBITMAP		hTopHotBmp;		// autoscroll
	HBITMAP		hNoClipBmp;

	HBITMAP		hUnOpenSBmp;
	HBITMAP		hUnOpenRBmp;
	HBITMAP		hUnOpenBigBmp;

	HBITMAP		hFoldFBmp;
	HBITMAP		hFoldTBmp;

	HPEN		hShinePen;
	HPEN		hMidShinePen;
	HPEN		hShadowPen;
	HPEN		hGrayPen;
	HPEN		hSelPen;
	HPEN		hMarkPen;
	HPEN		hFindPen;
	HPEN		hWhitePen;
	HPEN		hNullPen;

	HBRUSH		hGrayBrush;
	HBRUSH		hMidGrayBrush;
	HBRUSH		hShineBrush;
	HBRUSH		hMidShineBrush;
	HBRUSH		hShadowBrush;
	HBRUSH		hSelBrush;
	HBRUSH		hMarkBrush;
	HBRUSH		hCmntBrush;
	HBRUSH		hFindBrush;
	HBRUSH		hWhiteBrush;

	HCURSOR		hArrowCur;
	HCURSOR		hIBeamCur;
	HCURSOR		hHandCur;
	HCURSOR		hLoupeCur;

	HFONT		hFont;
	HFONT		hBoldFont;
	HFONT		hMiniFont;
	HFONT		hLinkFont;
//	HFONT		hTinyFont;

	// Device Context
	HDC			hMemDc;
	HDC			hTmpDc;

	BOOL	InitGDI();
	BOOL	UnInitGDI();
	int		PaintDate(ViewMsg *msg, int *off_x, int top_y);
	int		PaintFromTo(ViewMsg *msg, int *off_x, int top_y, BOOL is_rev=FALSE);
	int		PaintUsers(ViewMsg *msg, int top_y, int *off_x, int lim_x);
	enum BoxType { NORMAL_BOX, SELECT_BOX, CLEAR_BOX };
	BOOL	PaintPushBox(TRect rc, BoxType type, int round=0, BOOL is_direct=FALSE);
	int		PaintUnOpenBox(int x, int y, int cx, int cy);
	int		PaintBodyInHead(ViewMsg *msg, int top_y, int *off_x, int *lim_x);
	int		PaintFileInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x);
	int		PaintBmpIcon(HBITMAP hBmp, TRect rc, BOOL is_direct=FALSE);
	int		PaintComment(int64 msg_id, TRect rc, BOOL is_direct=TRUE);
	int		PaintCommentInBody(ViewMsg *msg, int top_y);
	int		PaintFoldBtn(ViewMsg *msg, int top_y, int cnt, int body_btm);
	int		PaintCommentInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x);
	int		PaintFavInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x);
	int		PaintImenuInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x);
	int		PaintReplyInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x);
	int		PaintClipInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x);
	int		PaintHeadBox(ViewMsg *msg, int top_y);
	int		PaintHead(ViewMsg *msg, int top_y);
	int		PaintFullFiles(ViewMsg *msg, TRect rc);
	int		PaintFullClip(int64 msg_id, LogClip *clip, int top_y);
	int		PaintThumb(ViewMsg *msg, int top_y, int *_width, BOOL is_direct=FALSE);
	void	CalcBreakLineInfo(ViewMsg *msg, int pos, int sel_pos, int sel_len, BreakLineInfo *bi);
	BOOL	SetBodyItem(int pos, int len, BreakLineInfo *bi, TRect *rc);
	BOOL	GetTep(HDC hDc, int tep_idx, ViewMsg *msg, const WCHAR *s, int max_len, int width,
				int *rlen, int *rcx);
	int		PaintBodyLine(ViewMsg *msg, int *tep_idx, const WCHAR *s, int len, int sel_pos,
				int sel_len, int top_y, int width);
	int		PaintBodyLines(ViewMsg *msg, int top_y, int *_width, int thum_cy);
	int		PaintBody(ViewMsg *msg, int top_y);
	BOOL	PaintUnOpenPict(TRect rc, BOOL is_direct);
	int		PaintMsg(ViewMsg *msg);
	BOOL	AlignCurPaintIdx();
	BOOL	CalcStartPrintIdx(int *idx, int *pix);
	BOOL	PaintFocus(BOOL is_direct=FALSE);
	BOOL	PaintMemBmp();
	BOOL	GetPaintMsgCy(int64 msg_id);

	BOOL	SelNormalization(SelItem *top, SelItem *end);
	BOOL	IsSelected(BOOL is_strict=FALSE);
	BOOL	IsMsgSelected(int64 msg_id, int pos, int len, int *_sel_pos, int *_sel_len);
	BOOL	GetPosToIdx(ViewMsg *msg, Item *item, POINTS pos, int *idx);
	BOOL	SetSelectedPos(Item *item, POINTS pos, SelItem *sel, BOOL auto_capture=TRUE);
	void	AllSelInit(BOOL auto_capture=TRUE) {
		selTop.Init();
		selMid.Init();
		selEnd.Init();
		if (auto_capture) {
			EndCapture();
		}
		//Debug("AllSelInit\n");
	}
	BOOL	StartCapture() {
		::SetCapture(hWnd);
		if (!isCapture) {
			isCapture = TRUE;
			SetTimer(CAPTURE_TIMER, 100);
			//Debug("start capture\n");
		}
		else {
			//Debug("call start capture and force start\n");
		}
		return	TRUE;
	}
	BOOL	EndCapture() {
		if (!isCapture) {
			//Debug("call end capture, but no captured\n");
			return	FALSE;
		}
		KillTimer(CAPTURE_TIMER);
		::ReleaseCapture();
		isCapture = FALSE;
		//Debug("end capture\n");
		return	TRUE;
	}

	BOOL	ButtonDownItem(Item *item, POINTS pos);
	BOOL	RButtonDownItem(Item *item, POINTS pos);
	BOOL	ExpandSelArea();
	BOOL	LinkSelArea(Item *item);
	BOOL	ButtonUpItem(Item *item, POINTS pos);
	BOOL	ButtonDblClickItem(Item *item, POINTS pos);
	BOOL	MouseOver(Item *item, POINTS pos);
	int		GetMsgIdx(int64 msg_id);
	void	SetNewCurIdxByMsgId(int64 msg_id);
	void	SetNewCurIdx(int idx);
	void	SetDispedCurIdx();
	BOOL	UpdateMsgFlags(ViewMsg *msg);
	BOOL	UpdateMsg(ViewMsg *msg);
	BOOL	DeleteMsg(int64 msg_id, BOOL reset_request=TRUE);
	void	GetNotFoundStatStr(WCHAR *s, int max_size);
	BOOL	UpdateMsgList(DWORD flags=UF_NONE);
	void	ToggleFindUser(const Wstr &uid);

	BOOL	CropUserMenu(Item *item);
	void	CropUserItem(int idx);

	BOOL	CheckDownloadLink(LogMsg *msg, int idx, BOOL is_update, DownloadLinkStat *dls);
	BOOL	LinkFileMenu(ViewMsg *msg, Item *item);
	void	FileMenuItem(int idx);

	BOOL	ItemMenu(Item *item);
	BOOL	ItemRMenu(Item *item);
	BOOL	JumpLink(Item *item, BOOL is_dblclick);
	BOOL	LinkLabel(Item *item, WCHAR *label);
	BOOL	MsgToClip(ViewMsg *msg, CopyMsgFlags flags);
	BOOL	ImgToClip(const WCHAR *img);
	BOOL	GetLinkStr(Item *item, Wstr *w);
	BOOL	GetLinkAttr(Item *item, BodyAttr *attr);
	BOOL	LinkToClip(Item *item);
	BOOL	ItemToClip(Item *item);
	BOOL	CopyClipPath(Item *item);
	BOOL	SaveClip(Item *item);
	BOOL	CmntToClip(int64 msg_id);
	BOOL	SelToClip();
	BOOL	SelToMark();
	BOOL	SelToMarkOff();

	BOOL	PosInMark(int64 msg_id, int body_pos, int *mark_idx=NULL);
	BOOL	SelToFind();
	BOOL	SelToSearchEng();

	BOOL	GetOneSelStr(Wstr *wbuf, BOOL oneline_only=FALSE);
	BOOL	StartFindTimer(DWORD flags=UF_NONE, DWORD tick=FIND_TICK);

	ViewMsg *GetMsg(int64 msg_id);
	BOOL	ClearMsgCacheAll();
	BOOL	ClearMsgCache(int64 msg_id);
	int		NearFindIdx(int64 msg_id, BOOL is_asc=TRUE);
	BOOL	SetStatusMsg(int pos=0, const WCHAR *msg=NULL, DWORD duration=0, BOOL use_stat=FALSE);
	BOOL	MainMenuEvent();
	void	InitBtnBmp();
	void	UpdateBtnBmp();
	int		DispToDbFlags(DispFlags flag);
	BOOL	FilterBtnAction(DispFlags flag);

	int		AnalyzeUrlMsgCore(std::vector<BodyAttr> *link, const WCHAR *s);
	int		AnalyzeUrlMsg(ViewMsg *msg);
	int		AnalyzeFindMsg(ViewMsg *msg);
	int		AnalyzeQuoteMsg(ViewMsg *msg);
	int		AnalyzeMsg(ViewMsg *msg);
	void	SetFindBody(const WCHAR *s);
	BOOL	ReplyMsg(ViewMsg *msg, int idx=-1);
	BOOL	EditBody(int64 msg_id);
	BOOL	DelMsg(int64 msg_id);
	BOOL	DelSelMsgs(int top_idx, int end_idx);
	BOOL	DelClip(int64 msg_id, int clip_idx);
	BOOL	EditMsgHead(int64 msg_id);
	BOOL	AddMemoCore(LogMsg *msg);
	BOOL	AddMemo();
	BOOL	PasteImage(ViewMsg *msg, UINT cf_type);
	BOOL	PasteTextComment(ViewMsg *msg);
	BOOL	InsertImageCore(ViewMsg *msg, VBuf *vbuf);
	BOOL	InsertImage(int64 msg_id);
	BOOL	PasteComment(int mode);
	BOOL	SetComment(int64 msg_id, const Wstr &comment, BOOL is_append=FALSE);
	BOOL	EditComment(int64 msg_id, const Wstr *append=NULL);
	BOOL	EvMouseMoveCore(UINT fwKeys, POINTS pos);

	int		FindNonHiresIdx(int pos, int top_idx, int end_idx, int offset_pix);
	int		IdxToScrollPos(int idx, int pix_off);
	int		ScrollPosToIdx(int pos, int *new_offpix);
	int		CalcTotalPix(int idx, int off_pix);
	int		SetScrollPos(BOOL is_redraw=TRUE);
	int		CalcMaxLineChars();
	void	SetFoldMeasure();
	BOOL	SelectMainFont();
	BOOL	UpdateFindComboList();
	BOOL	UpdateFindLru();
	BOOL	IsUnOpenCore(LogMsg *msg, bool is_receive, int idx=-1);
	BOOL	IsUnOpenR(LogMsg *msg) {
		return	IsUnOpenCore(msg, TRUE, 0);
	}
	BOOL	IsUnOpenS(LogMsg *msg, int idx=-1) {
		return	IsUnOpenCore(msg, FALSE, idx);
	}
	BOOL	IsFindComboActive();
	int		CalcScrOutYOff(ViewMsg *msg);

public:
	TChildView(Cfg *_cfg, LogMng *_logMng, BOOL is_main, TLogView *_parent);
	virtual ~TChildView();

	BOOL	CreateU8();
	BOOL	FitSize();
	BOOL	SetFont();
	HFONT	GetFont() { return hFont; }
	BOOL	UserPopupMenu();

	enum FindMode { CUR_IDX, NEXT_IDX, PREV_IDX, LAST_IDX, TOP_IDX };
	enum {	// SetFindedIx flags
			SF_NONE				= 0x0000,
			SF_REDRAW			= 0x0001,
			SF_SAVEHIST			= 0x0002,
			SF_FASTSCROLL		= 0x0004,
			SF_NOMOVE			= 0x0008,
	};

	BOOL	SetFindedIdx(FindMode mode, DWORD flags=SF_REDRAW);
	BOOL	UserComboSetup();
	BOOL	UserComboClose();
	BOOL	UpdateUserCombo();

	// TLogView 以外からも利用可能
	BOOL	UpdateLog(int64 msg_id);
	time_t	GetStatusCheckEpoch();
	BOOL	SetStatusCheckEpoch(time_t t);
	BOOL	SetUser(const Wstr &uid);
	BOOL	RequestResetCache();
	BOOL	ResetCache();
	BOOL	ClearFindURL();
	BOOL	GetDispFlasgs() { return dispFlags; }
	BOOL	LastView();
	void	SetUserSelected(int64 msg_id=0) {
		if (msg_id > 0) {
			userMsgId = msg_id;
		}
		else if (curIdx >= 0 && curIdx < msgIds.UsedNumInt()) {
			userMsgId = msgIds[curIdx].msg_id;
		}
	}
	BOOL	IsFold(ViewMsg *msg) {
		return	msg->foldGenNo < foldGenNo ? TRUE : FALSE;
	}
	BOOL	IsScrollOut(int top_y);
	int		PaintBmpIconCore(HDC hDc, HBITMAP hBmp, TRect rc);

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvPaint();
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
	virtual BOOL	EvNcHitTest(POINTS pos, LRESULT *result);
	virtual BOOL	EvNcDestroy(void);
//	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
//	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvMouseWheel(WORD fwKeys, short zDelta, short xPos, short yPos);
//	virtual BOOL	EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis);

	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventScroll(UINT uMsg, int nScrollCode, int nPos, HWND hScroll);
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

inline void ClientToMemPos(POINTS *pos) {
	pos->x -= CVW_GAPX;
	pos->y -= CVW_GAPY;
}

inline void MemToClientPos(POINTS *pos) {
	pos->x += CVW_GAPX;
	pos->y += CVW_GAPY;
}

inline void ClientToMemPt(POINT *pos) {
	pos->x -= CVW_GAPX;
	pos->y -= CVW_GAPY;
}

inline void MemToClientPt(POINT *pos) {
	pos->x += CVW_GAPX;
	pos->y += CVW_GAPY;
}

inline void ClientToMemRect(TRect *rc) {
	rc->left   -= CVW_GAPX;
	rc->top    -= CVW_GAPY;
	rc->right  -= CVW_GAPX;
	rc->bottom -= CVW_GAPY;
}

inline void MemToClinetRect(TRect *rc) {
	rc->left   += CVW_GAPX;
	rc->top    += CVW_GAPY;
	rc->right  += CVW_GAPX;
	rc->bottom += CVW_GAPY;
}

inline void ToUpperANSI(const WCHAR *s, WCHAR *d) {
	for ( ; *d = *s; d++, s++) {
		if (*d >= 'a' && *d <= 'z') {
			*d -= 'a' - 'A';
		}
	}
}

//     +------------------+ --+
//     |                  |   +---> OffPix
//     | +--------------+ | --+
//     | |   CenterBox  | |
//     | +--------------+ |
//     |                  |
//     +------------------+

inline int OffPix(int total_pix, int targ_pix) {
	return	(total_pix - targ_pix + 1) / 2;
}

#endif

