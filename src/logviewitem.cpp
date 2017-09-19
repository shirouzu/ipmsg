static char *logviewitem_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2016 logviewitem.cpp	Ver4.10";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child item
	Create					: 2015-04-10(Sat)
	Update					: 2016-11-01(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
using namespace std;
#include "logviewitem.h"

/* ===============================================================
                          ItemMgr
   =============================================================== */
ItemMgr::ItemMgr()
{
	genericNo = 0;
	isSkip = FALSE;
	savedIdx = -1;
	maxCy = 65535;
	maxNum = 3000;	// temp
	items = new Item[maxNum];
	Reset();
}

ItemMgr::~ItemMgr()
{
	delete [] items;
}

Item *ItemMgr::SetItem(DWORD kind, TRect *rc)
{
	if (isSkip) {
		return	FALSE;
	}

	if (rc->bottom < 0 || rc->top > maxCy) {
		return	FALSE;
	}

	if (num >= maxNum) {
		Debug("too many items in itemMgr\n");
		return FALSE;
	}

	Item &item = items[num++];

	item.msg_id = msgId;
	item.rc = *rc;
	item.kind = kind;
	memset(item.buf, 0, sizeof(item.buf));

	return	&item;
}

Item *ItemMgr::FindItem(int x, int y)
{
	Item	*item = NULL;
	POINT	pt = { x, y };

	// 必要があれば二分探索に、検索開始位置を決定する
	for (int i=num-1; i >= 0; i--) {
		if (PtInRect(&items[i].rc, pt)) {
			item = &items[i];
			break;
		}
//		if (items[i].rc.top > y) break;
	}
	return	item;
}

Item *ItemMgr::FindItemByKind(int64 msg_id, Item::Kind kind, Item *start)
{
	int		idx = 0;
	Item	*item = NULL;

	if (start) {
		idx = int(start - items);
	}
	else {
		for (idx=0; idx < num && items[idx].msg_id != msg_id; idx++)
			;
	}

	for (; idx < num && items[idx].msg_id == msg_id; idx++) {
		if (items[idx].kind & kind) {
			item = &items[idx];
			break;
		}
	}

	return	item;
}


