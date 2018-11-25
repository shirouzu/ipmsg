/*	@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logviewitem.h	Ver4.50 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer items
	Create					: 2015-04-10(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGVIEWITEM_H
#define LOGVIEWITEM_H

#include "logchildview.h"

struct Item {
	// 座標は CVW_GAPX を引いたクライアント座標 = MEM座標
	TRect	rc;			// マウスオーバー領域
	TRect	drawRc;		// BMP描画領域
	int64	msg_id;

	// 登録時に2以上の属性が重複する場合は、なるべくアクション用代表1つに。
	enum Kind {	HEADBOX		= 0x00000001,
				//HEADFOCUS	= 0x00000002,
				DATE		= 0x00000004,
				FROMTO		= 0x00000008,
				USER		= 0x00000010,
				USERMULTI	= 0x00000020,
				USERMULOLD	= 0x00000040,
				UNOPEN		= 0x00000080,
				UNOPENBODY	= 0x00000100,
				CROPUSER	= 0x00000200,
				REPLY		= 0x00000400,
				FILE		= 0x00000800,
				UNOPENFILE	= 0x00001000,
				CLIP		= 0x00002000,
				CMNT		= 0x00004000,
				FAV			= 0x00008000,
				IMENU		= 0x00010000,
				BODY		= 0x00020000,
				BODYBLANK	= 0x00040000,
				LINK		= 0x00080000,
				MARK		= 0x00100000,
				SEL			= 0x00200000,
				FIND		= 0x00400000,
				CLIPBODY	= 0x00800000,
				CMNTBODY	= 0x01000000,
				FOLDBTN		= 0x02000000,
	};
	DWORD	kind;
	union {
		int			user_idx;
		int			file_idx;
		int			clip_idx;
		BOOL		is_from; // for Item::FROMTO
		struct {
			int		body_pos;
			int		body_len;
			int		link_idx;
			int		mark_idx;
			int		find_idx;
			int		dummy;
		};
		BYTE		buf[24];
	};
	Item() {
		msg_id = 0;
	}
	Item(const Item &org) {
		*this = org;
	}
	bool operator ==(const Item &item) {
		return	(msg_id == item.msg_id) && (rc == item.rc) && (kind == item.kind);
	}
	bool operator !=(const Item &item) {
		return !(*this == item);
	}
};

class ItemMgr {
	ULONG	genericNo;
	Item	*items;
	Item	*lastProcItem;
	int		savedIdx;
	Item	savedProcItem;
	int		maxCy;
	int		num;
	int		maxNum;
	int64	msgId;
	BOOL	isSkip;

public:
	ItemMgr();
	~ItemMgr();
	void Reset() {
		num = 0;
		genericNo++;
		msgId = -1;
		lastProcItem = NULL;
	}
	void SetCy(int cy) {
		maxCy = cy;
	}
	int GenericNo() {
		return genericNo;
	}
	void SetMsgIdx(int64 msg_id) {
		msgId = msg_id;
	}
	Item *SetItem(DWORD kind, TRect *rc);
	Item *FindItem(int x, int y);
	Item *FindItemByKind(int64 msg_id, Item::Kind kind, Item *start=NULL);
	Item *GetLastItem() {
		return	(num > 0) ? (items + num - 1) : NULL;
	}
	Item *GetPrevItem(Item *cur) {
		return	(cur > items) ? (cur - 1) : NULL;
	}
	void SwapItem(Item *i1, Item *i2) {
		Item tmp = *i1;
		*i1 = *i2;
		*i2 = tmp;
	}

	void SetLastProcItem(Item *item) { lastProcItem = item; }
	Item *GetLastProcItem() {
		if (lastProcItem) return lastProcItem;
		if (Item *item = GetSavedProcItem()) {
			ClearSavedProcItem();
			return item;
		}
		return	NULL;
	}

	void SaveProcItem(Item *item) {
		savedIdx = int(item - items);
		if (savedIdx >= num) {
			savedIdx = -1;
			return;
		}
		savedProcItem = *item;
	}
	Item *GetSavedProcItem() {
		if (savedIdx < 0 || savedIdx >= num || savedProcItem != items[savedIdx]) {
			return	NULL;
		}
		return	items + savedIdx;
	}
	void ClearSavedProcItem() {
		savedIdx = -1;
	}
	int Num() {
		return num;
	}
	void SetSkip(BOOL _is_skip) {
		isSkip = _is_skip;
	}
};

#define LOGTB_OFFSET  3100
#define LOGTB_TITLE   (LOGTB_OFFSET + 0)
#define LOGTB_REV     (LOGTB_OFFSET + 1)
// UserCombo
#define LOGTB_TAIL    (LOGTB_OFFSET + 2)
// SearchCombo
#define LOGTB_NARROW  (LOGTB_OFFSET + 3)
// StatusStatic
#define LOGTB_FAV     (LOGTB_OFFSET + 4)
#define LOGTB_CLIP    (LOGTB_OFFSET + 5)
// Sep
#define LOGTB_MEMO    (LOGTB_OFFSET + 6)
#define LOGTB_MENU    (LOGTB_OFFSET + 7)
#define LOGTB_MAX     8

#define LOGTBICO_TITLE_OFF 0
#define LOGTBICO_TITLE_ON  1
#define LOGTBICO_REV_OFF   2
#define LOGTBICO_REV_ON    3
#define LOGTBICO_TAIL_OFF  4
#define LOGTBICO_TAIL_ON   5
#define LOGTBICO_NARROW    6
#define LOGTBICO_FAV       7
#define LOGTBICO_CLIP      8
#define LOGTBICO_MEMO      9
#define LOGTBICO_MENU      10
#define LOGTBICO_MAX       11


/*
// USER, CROPUSER, FILE, CLIP, FAV, HEADBOX, BODY, LINK, IMENU, ITEM

class MsgItem {
public:
	Kind				kind;
	TRect				rc;
	vector<MsgItem *>	childs;
	MsgItem				*parent;
	LogMsg				*msg;
	HBITMAP				hBmp;

public:
	MsgItem(Kind _kind=ITEM, MsgItem *_parent=NULL, LogMsg *msg=NULL) {
		kind	= _kind;
		parnet	= _parent;
		msg		= _msg;
		hBmp	= NULL;
	}
	virtual ~MsgItem() {}
	virtual BOOL PtInRect() {}
};

class MsgText : public MsgItem {
	TRect				rc;
};

class MsgBmp {
public:
	LogMsg		*msg;
	TRect		rc;
	HBITMPAP	hBmp;
	MsgItem		head;
	MsgItem		body;
};
*/

#endif

