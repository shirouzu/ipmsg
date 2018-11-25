static char *logchildview_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017 logchildview.cpp	Ver4.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child Pane
	Create					: 2015-04-10(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "logchilddef.h"
#include "ipmsg.h"
#include <gdiplus.h>

using namespace Gdiplus;
using namespace std;

#include "logdbconv.h"
#include "logimportdlg.h"

/* ===============================================================
                          ChildView
   =============================================================== */
TChildView::TChildView(Cfg *_cfg, LogMng *_logMng, BOOL is_main, TLogView *_parent)
	: TWin(_parent), cfg(_cfg), logMng(_logMng), isMain(is_main), parentView(_parent)
{
	hAccel	= ::LoadAccelerators(TApp::hInst(), (LPCSTR)LOGVIEW_ACCEL);

	hMemDc	= NULL;
	hTmpDc	= NULL;
	hMemBmp = NULL;
	hMemBmpSave = NULL;
	logDb = NULL;

	curIdx = 0;
	focusIdx = -1;
	offsetPix = 0;
	scrOffPix = 0;
	userMsgId = 0;
	lastReplyBtnId = 0;

	startDispIdx = 0;
	startDispPix = 0;
	endDispIdx = 0;
	endDispPix = 0;
	maxLines = 0;
	scrollMode = NONE_SCROLL;

	scrHiresTopPix = 0;
	scrHiresTopIdx = 0;
	scrHiresEndPix = 0;
	scrHiresEndIdx = 0;
	scrAllRangePix = 0;
	scrHresReduce  = 0;
	scrCurPos      = 0;
	scrTrackNpos   = 0;

	maxLineChars = 10000; // 初期値。正確な値は CalcMaxLineChars にて。

	::GetLocalTime(&curTime);

	dispFlags = 0;
	findGenNo = 1;
	dispGenNo = 1;
	tepGenNo  = 1;
	foldGenNo = 1;
	SetStatusCheckEpoch(cfg->viewEpoch ? cfg->viewEpoch : time(NULL));

	savedCurMsgId = 0;

	memset(&hMemBmp, 0, offsetof(TChildView, hMemDc) - offsetof(TChildView, hMemBmp));
}

TChildView::~TChildView()
{
	ClearMsgCacheAll();
}

BOOL TChildView::CreateU8()
{
	static bool once = []() {
		TRegisterClassU8(VIEWCHILD_CLASS, CS_DBLCLKS, NULL, NULL, 0);
		return	true;
	}();

	return	TWin::CreateU8(VIEWCHILD_CLASS, VIEWCHILD_CLASS,
							WS_CHILD|WS_VISIBLE|WS_TABSTOP/*|WS_HSCROLL*/|WS_VSCROLL);
}

BOOL TChildView::SelNormalization(SelItem *top, SelItem *end)
{
	if (!top->IsValid() || !end->IsValid()) {
		return	FALSE;
	}

	if (top->msg_id > end->msg_id || (top->msg_id == end->msg_id && end->pos < top->pos)) {
		SelItem	tmp = *top;
		*top = *end;
		*end = tmp;
	}
	return	TRUE;
}

BOOL TChildView::IsSelected(BOOL is_strict)
{
	if (!selTop.IsValid()) {
		return	FALSE;
	}
	return	selEnd.IsValid() || (!is_strict && selMid.IsValid());
}

BOOL TChildView::IsMsgSelected(int64 msg_id, int pos, int len, int *_sel_pos, int *_sel_len)
{
	if (!IsSelected()) {
		return	FALSE;
	}

	// End/Mid 判定および、起点と終点の逆転判定
	SelItem sel_top = selTop;
	SelItem sel_end = selEnd.IsValid() ? selEnd : selMid;
	SelNormalization(&sel_top, &sel_end);

	if (msg_id < sel_top.msg_id || msg_id > sel_end.msg_id) {
		return	FALSE;
	}

	int		tmp_pos = 0;
	int		tmp_len = 0;
	int		&sel_pos = _sel_pos ? *_sel_pos : tmp_pos;
	int		&sel_len = _sel_len ? *_sel_len : tmp_len;

	if (msg_id > sel_top.msg_id && msg_id < sel_end.msg_id) {
		sel_pos = pos;
		sel_len = len;
		return	TRUE;
	}
	if (sel_top.msg_id == sel_end.msg_id) {
		if (pos + len < sel_top.pos || pos >= sel_end.pos) {
			return	FALSE;
		}
		sel_pos = max(pos, sel_top.pos);
		sel_len = min(pos + len, sel_end.pos) - sel_pos;
		//Debug("spos=%d len=%d pos=%d, sel_top.pos=%d, len=%d\n",
		//	sel_pos, sel_len, pos, sel_top.pos, len, sel_end.pos);
		return	TRUE;
	}
	else if (msg_id == sel_top.msg_id) {
		if (pos + len <= sel_top.pos) {
			return	FALSE;
		}
		sel_pos = max(pos, sel_top.pos);
		sel_len = len;
		return	TRUE;
	}
	else if (msg_id == sel_end.msg_id) {
		if (pos >= sel_end.pos) {
			return	FALSE;
		}
		sel_pos = pos;
		sel_len = min(pos+len, sel_end.pos) - pos;
		return	TRUE;
	}
	Debug("Internal Error (IsMsgSelected)\n");		// Not reached
	return	FALSE;
}

BOOL TChildView::GetPosToIdx(ViewMsg *msg, Item *item, POINTS pos, int *idx)
{
	if (!msg) {
		return	FALSE;
	}
	if (item->body_len == 0) {
		// item に幅が無い == 改行のみ or 行終了～横幅の空白部分
		*idx = 0;
		return	TRUE;
	}

	int		rlen = 0;
	int		rcx = 0;
	int		off_x = pos.x - item->rc.x();

	TGetTextWidth(hMemDc, msg->msg.body.s() + item->body_pos, item->body_len, off_x, &rlen, &rcx);

	if (item->body_pos + rlen >= msg->msg.body.Len()) {
		*idx = msg->msg.body.Len() -1;
		return	TRUE;
	}

	int		char_len = IsSurrogateL(*(msg->msg.body.s() + item->body_pos + rlen)) ? 2 : 1;
	int		ivs_len = 0;

	if (IsIVS(msg->msg.body.s() + item->body_pos + rlen + char_len, &ivs_len)) {
		char_len += ivs_len;
	}

	int		rcx2 = 0;
	int		tmp_len = 0;

	if (!TGetTextWidth(hMemDc, msg->msg.body.s() + item->body_pos + rlen, char_len,
		item->rc.cx(), &tmp_len, &rcx2)) {
		Debug(" GetTextExtentExPoint2 err\n");
		return	FALSE;
	}

	if ((pos.x - item->rc.x() - rcx + 1) >= (rcx2 / 2)) { // +1 == size表現とindex表現の差
		rlen += char_len;
	}
//	Debug("pos=%d rcx=%d sz=%d sz2=%d (%d %d) rlen=%d\n", pos.x, item->rc.x(), sz.cx, sz2.cx, (pos.x - item->rc.x() - sz.cx), (sz2.cx/2), rlen);

	*idx = rlen;

	return	TRUE;
}

BOOL TChildView::SetSelectedPos(Item *item, POINTS pos, SelItem *sel, BOOL auto_capture)
{
	if (!item
		|| (item->kind & (Item::BODY|Item::HEADBOX|Item::DATE|Item::FROMTO|Item::USER)) == 0) {
		if (sel != &selEnd) {
			return FALSE;
		}
		if (selMid.IsValid() && !selEnd.IsValid()) {
			selEnd = selMid;
			SelNormalization(&selTop, &selEnd);
			EndCapture();
			return	TRUE;
		}
		else {
			AllSelInit();
			return	FALSE;
		}
	}

	ViewMsg	*msg = GetMsg(item->msg_id);
	int		idx = 0;

	if (!msg) {
		return	FALSE;
	}
	if ((item->kind & Item::BODY) && !GetPosToIdx(msg, item, pos, &idx)) {
		return	FALSE;
	}
	if (sel == &selTop) {
		if (auto_capture) {
			StartCapture();
		}
	}
	else {
	//	if (item->body_pos + msg->msg.body.Len()
	}

//	if (sel == &selEnd) {
//		Debug("SetSelectedPos(%s) rlen=%d pos.x=%d sz.cx=%d itemoff=%d\n",
//			sel == &selTop ? "top" : sel == &selMid ? "mid" : "end",
//			rlen, pos.x, sz.cx, item->body_len);
//	}

	sel->msg_id   = item->msg_id;
	sel->pos      = (item->kind & Item::BODY) ? item->body_pos + idx : 0;
	sel->inc_head = (item->kind & (Item::HEADBOX|Item::DATE)) ? TRUE : FALSE;

	if (sel == &selEnd) {
		if (selTop.msg_id == selEnd.msg_id && selTop.pos == selEnd.pos) {
			AllSelInit();
			return	FALSE;
		}
		SelNormalization(&selTop, &selEnd);
		EndCapture();
/*
		int	end = GetMsgIdx(selEnd.msg_id);
		for (int i = GetMsgIdx(selTop.msg_id); i <= end; i++) {
			ViewMsg	*msg = GetMsg(msgIds[i].msg_id);
			int		sel_pos = 0;
			int		sel_len = msg->msg.body.Len();
			if (IsMsgSelected(msgIds[i].msg_id, sel_pos, sel_len, &sel_pos, &sel_len)) {
			//	DebugW(L"sel(%d sel(%d/%d)) = <%.*s>\n", i, sel_pos, sel_len,
			//		sel_len, msg->msg.body.s() + sel_pos);
			}
			else {
				Debug("Illegal selected status\n");
			}
		}
*/
	}
	return	TRUE;
}

BOOL is_kanji(WCHAR ch) {
	return	(ch >= 0x4000 && ch <= 0x9fff)
			? true : false;
}

BOOL is_katakana(WCHAR ch) {
	return	(ch >= 0x309b && ch <= 0x30fe) ||
			(ch >= 0xff10 && ch <= 0xff5d)
			? true : false;
}

BOOL is_kana(WCHAR ch) {
	return	(ch >= 0x3041 && ch <= 0x309e) ||
			(ch >= 0x30fc && ch <= 0x30fe) ||
			(ch >= 0xff10 && ch <= 0xff5d)
			? true : false;
}

BOOL is_alnum(WCHAR ch)
{
	return	(ch >= '0' && ch <= '9') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= 'a' && ch <= 'z') ||
			(ch == '_')
			? true : false;
}

BOOL is_mark(WCHAR ch)
{
	return	(ch >= '!' && ch <= '/') ||
			(ch >= ':' && ch <= '@') ||
			(ch >= '[' && ch <= '^') ||
			(ch == '`')              ||
			(ch >= '{' && ch <= '~')
			? true : false;
}

// ダブルクリック文字列の拡大
BOOL TChildView::ExpandSelArea()
{
	ViewMsg *msg = GetMsg(selTop.msg_id);

	if (!msg || selTop.pos >= msg->msg.body.Len()) {
		return	FALSE;
	}

	const WCHAR	*top = msg->msg.body.s() + selTop.pos;
	const WCHAR *end = top;
	WCHAR	ch = *top;
	BOOL	(*is_group)(WCHAR ch) =
		is_kanji(ch)    ? is_kanji :
		is_katakana(ch) ? is_katakana :
		is_kana(ch)     ? is_kana :
		is_alnum(ch)    ? is_alnum :
		is_mark(ch)     ? is_mark :
		IsSurrogate(ch) ? IsSurrogate : NULL;

	selEnd.msg_id = selTop.msg_id;

	if (!is_group) {
		selEnd.pos = selTop.pos + (IsSurrogateL(ch) ? 2 : 1);
		return	TRUE;
	}

	for ( ; *end; end++) {	// 末尾拡大
		if (!is_group(*end)) {
			if (!IsSurrogateR(*end) && !IsIVSAny(*end)) {
				break;
			}
		}
	}
	for ( ; top >= msg->msg.body.s(); top--) { // 先頭拡大
		if (!is_group(*top)) {
			if (!IsSurrogateL(*top)) {
				top++;
				break;
			}
		}
	}

	if (top < msg->msg.body.s()) {
		top = msg->msg.body.s();
	}

	selTop.pos = int(top - msg->msg.body.s());
	selEnd.pos = int(end - msg->msg.body.s());
	EndCapture();

	return	TRUE;
}

// リンク文字列を選択状態に
BOOL TChildView::LinkSelArea(Item *item)
{
	ViewMsg *msg = GetMsg(item->msg_id);

	if (!msg || (item->kind & Item::LINK) == 0) {
		return	FALSE;
	}

	if (item->link_idx >= 0 && item->link_idx < msg->link.size()) {
		auto link = &msg->link[item->link_idx];

		selEnd.msg_id = selTop.msg_id = item->msg_id;
		selEnd.inc_head = selTop.inc_head = FALSE;
		selTop.pos = link->pos;
		selEnd.pos = link->pos + link->len;

		return	TRUE;
	}

	return	FALSE;
}

BOOL TChildView::SetStatusMsg(int pos, const WCHAR *msg, DWORD duration, BOOL use_stat)
{
	static DWORD lastTick;
	static DWORD lastDuration;

	DWORD cur = (lastTick || duration) ? ::GetTick() : 0;

	if (lastTick) {
		if (cur - lastTick < lastDuration) {
			return TRUE;
		}
		lastTick = 0;
	}
	if (duration) {
		lastTick = cur;
		lastDuration = duration;
	}

	if (pos != 1 || !use_stat) {
		return	parentView->SetStatusMsg(pos, msg ? msg : L"");
	}

	WCHAR	buf[256];
	WCHAR	*p = buf;

	p += swprintf(p, L"pos=%d ", curIdx);

	if ((dispFlags & DISP_NARROW) == 0 && msgFindIds.UsedNumInt() > 0) {
		p += swprintf(p, L"find=%d ", msgFindIds.UsedNumInt());
	}
	p += swprintf(p, L"total=%d ", msgIds.UsedNumInt());
	if (msg) {
		wcsncpyz(p, msg, int(wsizeof(buf) - (p - buf)));
	}

	return	parentView->SetStatusMsg(pos, buf);
}

int TChildView::NearFindIdx(int64 msg_id, BOOL is_asc)
{
	int 	min_idx = 0;
	int		max_idx = msgFindIds.UsedNumInt() -1;
	int		idx;
	bool	is_rev = (dispFlags & DISP_REV) ? true : false;

	if (max_idx < 0) {
		return 0;
	}

	while (min_idx <= max_idx) {
		idx = (min_idx + max_idx) / 2;

		int64	f_msgid = msgFindIds[idx].msg_id;
		if (f_msgid == msg_id) {
			return idx;
		}
		if (is_rev ? (f_msgid < msg_id) : (f_msgid > msg_id)) {
			max_idx = idx - 1;
		} else {
			min_idx = idx + 1;
		}
	}


	if (is_asc) {
		idx = max(min_idx, max_idx);
	} else {
		idx = min(min_idx, max_idx);
	}
	if (idx >= msgFindIds.UsedNumInt()) {
		idx = msgFindIds.UsedNumInt() -1;
	}
	if (idx < 0) idx = 0;

	return	idx;
}

BOOL TChildView::UpdateFindComboList()
{
	auto bodyCombo = &parentView->bodyCombo;

	DWORD	start, end;		// エディット部の選択状態の保存
	bodyCombo->SendMessage(CB_GETEDITSEL, (WPARAM)&start, (LPARAM)&end);
	// CB_RESETCONTENT でエディット部がクリア
	bodyCombo->SendMessage(CB_RESETCONTENT, 0, 0);

	for (auto obj=cfg->LvFindList.TopObj(); obj; obj=cfg->LvFindList.NextObj(obj)) {
		bodyCombo->SendMessageW(CB_ADDSTRING, 0, (LPARAM)obj->w.s());
	}
	bodyCombo->SetWindowTextW(findBodyOrg.s());
	bodyCombo->SendMessage(CB_SETEDITSEL, 0, MAKELPARAM(start, end));

	cfg->WriteRegistry(CFG_LVFINDHIST);

	return	TRUE;
}

BOOL TChildView::UpdateFindLru()
{
	if (auto obj = cfg->LvFindList.TopObj()) {
		if (obj->w == findBodyOrg) { // not need update
			return	TRUE;
		}
	}

	for (auto obj=cfg->LvFindList.TopObj(); obj; obj=cfg->LvFindList.NextObj(obj)) {
		if (obj->w == findBodyOrg) {
			cfg->LvFindList.DelObj(obj);
			cfg->LvFindList.PushObj(obj);	// update LRU
			UpdateFindComboList();
			return	TRUE;
		}
	}
	if (cfg->LvFindList.Num() >= cfg->LvFindMax) {
		if (auto obj = cfg->LvFindList.EndObj()) {
			cfg->LvFindList.DelObj(obj);
			cfg->LvFindList.PushObj(obj);	// update LRU
			obj->w = findBodyOrg;
			UpdateFindComboList();
			return	TRUE;
		}
	}
	TWstrObj *obj = new TWstrObj;
	obj->w = findBodyOrg;
	cfg->LvFindList.PushObj(obj);
	UpdateFindComboList();

	return	TRUE;
}

BOOL TChildView::IsFindComboActive()
{
	HWND	hCombo = parentView->bodyCombo.hWnd;

	for (HWND hTmp=::GetFocus(); hTmp && hTmp != hWnd; hTmp=::GetParent(hTmp)) {
		if (hTmp == hCombo) {
			return	TRUE;
		}
	}

	return	FALSE;
}

BOOL TChildView::SetFindedIdx(TChildView::FindMode mode, DWORD flags)
{
	int	idx = curIdx;
	int	new_offsetpix = max(offsetPix, 0);

	if (flags & SF_SAVEHIST) {
		Wstr	buf(FIND_CHAR_MAX);
		parentView->bodyCombo.GetWindowTextW(buf.Buf(), FIND_CHAR_MAX);
		if (buf != findBodyOrg) {
			StartFindTimer(UF_UPDATE_BODYLRU, 1);
			return	TRUE;
		}
	}

	switch (mode) {
	case CUR_IDX:
		break;

	case NEXT_IDX:
		idx++;
		break;

	case PREV_IDX:
		idx--;
		break;

	case TOP_IDX:
		idx = 0;
		new_offsetpix = 0;
		break;

	case LAST_IDX:
		idx = msgIds.UsedNumInt() - 1;
		break;
	}

	if (idx >= msgIds.UsedNumInt()) {
		idx = msgIds.UsedNumInt() - 1;
	}
	if (idx < 0) {
		idx = 0;
	}

	if (msgFindIds.UsedNumInt()) {
		if (IsFindComboActive()) {
			if (findBody) {
				int	find_idx = NearFindIdx(msgIds[idx].msg_id,
					(mode == CUR_IDX || mode == NEXT_IDX));
				idx = GetMsgIdx(msgFindIds[find_idx].msg_id);
				if (flags & SF_SAVEHIST) {
					UpdateFindLru();
				}
				if (flags & SF_FASTSCROLL) {
					scrHresReduce = ::GetTick();
					SetTimer(FINDNEXT_TIMER, FINDNEXT_TICK);
				}
			} else {
				if (idx < 0 || idx >= msgIds.UsedNumInt()) {
					return FALSE;
				}
			}
		}
	}
	SetNewCurIdx(idx);

	if (mode == LAST_IDX) {
		int	cy = GetPaintMsgCy(msgIds[curIdx].msg_id);
		new_offsetpix = vrc.cy() - cy - 50;
	}

	offsetPix = new_offsetpix;

	if (flags & SF_REDRAW) {
		SetScrollPos();
	}

	SetStatusMsg(1, 0, TRUE);

	return	TRUE;
}

int TChildView::GetMsgIdx(int64 msg_id)
{
	int		min_idx = 0;
	int		max_idx = msgIds.UsedNumInt() - 1;
	int		ans_idx = 0;
	bool	is_rev = (dispFlags & DISP_REV) ? true : false;

	while (min_idx <= max_idx) {
		int 	idx = (min_idx + max_idx) / 2;
		int64	f_msgid = msgIds[idx].msg_id;

		if (f_msgid == msg_id) {
			ans_idx = idx;
			return	ans_idx;
		}
		if (is_rev ? (f_msgid < msg_id) : (f_msgid > msg_id)) {
			max_idx = idx - 1;
		} else {
			min_idx = idx + 1;
		}
	}
	ans_idx = (max_idx >= 0) ? max_idx : min_idx;

	if (ans_idx < 0) {
		ans_idx = 0;
	}

	return	ans_idx;
}

void TChildView::SetNewCurIdxByMsgId(int64 msg_id)
{
	SetNewCurIdx(GetMsgIdx(msg_id));
}

void TChildView::SetNewCurIdx(int idx)
{
	if (idx < 0 || idx >= msgIds.UsedNumInt()) {
		Debug("Illegal curIdx=%d\n", idx);
		return;
	}
	curIdx = idx;
}

void TChildView::SetDispedCurIdx()
{
	if (offsetPix == 0) {
		return;
	}
	if (offsetPix > 0) {
/*
		while (curIdx > 0) {
			int64	msg_id = msgIds[curIdx - 1].msg_id;
			int		cy = GetPaintMsgCy(msg_id);
			if (offsetPix - cy < 0) {
				break;
			}
			offsetPix -= cy;
			curIdx--;
		}
*/
		return;
	}
	if (curIdx + 1 < msgIds.UsedNumInt()) {
		int64	msg_id = msgIds[curIdx].msg_id;
		int		cy = GetPaintMsgCy(msg_id);
		offsetPix += cy;
		curIdx++;
	}
}

BOOL TChildView::UpdateMsgFlags(ViewMsg *msg)
{
	BOOL ret = logDb->UpdateOneData(msg->msg.msg_id, msg->msg.flags);

	RequestResetCache();

	return	ret;
}

BOOL TChildView::UpdateMsg(ViewMsg *msg)
{
	if (!logDb->DeleteOneData(msg->msg.msg_id)) {
		return	FALSE;
	}

	// flag will be modified in InsertOneData (DB_FLAG_MARK or etc)

	BOOL ret = logDb->InsertOneData(&msg->msg);

	RequestResetCache();

	return	ret;
}

BOOL TChildView::DeleteMsg(int64 msg_id, BOOL reset_request)
{
	BOOL ret = logDb->DeleteOneData(msg_id);

	if (reset_request) {
		RequestResetCache();
	}

	return	ret;
}

void TChildView::SetFindBody(const WCHAR *s)
{
	findBody = s;
	findBodyOrg = s;

	ToUpperANSI(findBody.s(), findBody.Buf());

	findBodyVec.clear();
	Wstr	w = findBody;
	WCHAR	*p;
	BOOL	is_normal_token = FALSE;

	for (WCHAR *tok=quote_tok(w.Buf(), ' ', &p) ; tok; tok=quote_tok(NULL, ' ', &p)) {
		if (is_normal_token && IsDbOperatorToken(tok)) {
			is_normal_token = FALSE;
		}
		else {
			findBodyVec.push_back(tok);
			is_normal_token = TRUE;
		}
	}

	if (!findBodyOrg && (dispFlags & DISP_NARROW)) {
		dispFlags &= ~DISP_NARROW;
	}

//	DebugW(L"set findbody =%s (%s)\n", findBody.s(), findBodyOrg.s());

	findGenNo++;
}

static int add_stat_str(WCHAR *dest, int offset, const WCHAR *key)
{
	int		len = 0;

	if (offset) {
		if (*key) {
			len = wcscpyz(dest + offset, L"/");
		}
		else {
			len = wcscpyz(dest + offset, L")");
		}
	}
	else {
		len = wcscpyz(dest + offset, L"(");
	}

	return	wcscpyz(dest + offset + len, key) + len;
}

void TChildView::GetNotFoundStatStr(WCHAR *s, int max_size)
{
	int	len = wcsncpyz(s, L"Not Found ", max_size);

	WCHAR	add_str[200];
	int		add_len = 0;

	if (findUser) {
		add_len += add_stat_str(add_str, add_len, L"User");
	}
	if ((dispFlags & DISP_NARROW) && findBody) {
		add_len += add_stat_str(add_str, add_len, L"Search");
	}

	if (dispFlags & DISP_UNOPENR_ONLY) {
		add_len += add_stat_str(add_str, add_len, L"UnOpen");
	}
	if (dispFlags & DISP_MARK_ONLY) {
		add_len += add_stat_str(add_str, add_len, L"Mark");
	}
	if (dispFlags & DISP_CLIP_ONLY) {
		add_len += add_stat_str(add_str, add_len, L"Clip");
	}
	if (dispFlags & DISP_FILE_ONLY) {
		add_len += add_stat_str(add_str, add_len, L"File");
	}
	if (dispFlags & DISP_FAV_ONLY) {
		add_len += add_stat_str(add_str, add_len, L"Fav");
	}

	if (add_len) {
		add_len += add_stat_str(add_str, add_len, L"");
		wcsncpyz(s + len, add_str, max_size - len);
	}
}

BOOL TChildView::UpdateMsgList(DWORD flags)
{
	int64	cur_msgid = 0;

//	Debug("UpdateMsgList(%x)\n", flags);
	userMsgId = 0;

	if ((flags & UF_NO_INITSEL) == 0) {
		AllSelInit();
	}

	int	last_msgnum = msgIds.UsedNumInt();

	if (last_msgnum > 0) {
		cur_msgid = msgIds[curIdx].msg_id;
	}
	else if (savedCurMsgId) {
		cur_msgid = savedCurMsgId;
		savedCurMsgId = 0;
	}

	if (findTick) {
		Wstr	buf(FIND_CHAR_MAX);
		parentView->bodyCombo.GetWindowTextW(buf.Buf(), FIND_CHAR_MAX);
		if (buf != findBodyOrg) {
			SetFindBody(buf.s());
			if (flags & UF_UPDATE_BODYLRU) {
				UpdateFindLru();
			}
		}
	}

	logDb->SetCondUser(findUser.s(), parentView->IsUserComboEx() ? FALSE : TRUE);
	logDb->SetCondBody((dispFlags & DISP_NARROW) ? findBody.s() : NULL);
	logDb->SetCondFlags(
		((dispFlags & DISP_CLIP_ONLY)    ? DB_FLAG_CLIP : 0) |
		((dispFlags & DISP_FILE_ONLY)    ? DB_FLAG_FILE : 0) |
		((dispFlags & DISP_MARK_ONLY)    ? (DB_FLAG_MARK|DB_FLAG_CMNT) : 0) |
		((dispFlags & DISP_FAV_ONLY)     ? DB_FLAG_FAV  : 0) |
		((dispFlags & DISP_UNOPENR_ONLY) ? DB_FLAG_UNOPENR|DB_FLAG_UNOPENRTMP : 0)
	);
	logDb->SetUseHostFts(parentView->IsUserComboEx());
	logDb->SetUseHostFts(parentView->IsUserComboEx());
	logDb->SetRange(range);

//	TTick	tick;
	BOOL	ret = FALSE;
	BOOL	find_ret = TRUE;

	if ((flags & UF_TO_ASC) && (dispFlags & DISP_REV)) {
		dispFlags &= ~DISP_REV;
	}
	else if ((flags & UF_TO_DSC) && !(dispFlags & DISP_REV)) {
		dispFlags |= DISP_REV;
	}
	BOOL	is_rev = (dispFlags & DISP_REV) ? TRUE : FALSE;

	if ((ret = logDb->SelectMsgIdList(&msgIds, is_rev, 0))) {
		SetNewCurIdxByMsgId(cur_msgid);

//		Debug("select=%d\n", tick.elaps());

		if (findBody && (dispFlags & DISP_NARROW) == 0) {
			logDb->SetCondBody(findBody.s());
			find_ret = logDb->SelectMsgIdList(&msgFindIds, is_rev, 0);
			if ((flags & UF_NO_FINDMOVE) == 0) {
				SetFindedIdx(CUR_IDX, SF_NONE);
			}
//			Debug("find=%d\n", tick.elaps());
		}
		else {
			msgFindIds.SetUsedNum(0);
		}
	}
	else if (flags & UF_FORCE) {
		msgIds.SetUsedNum(0);
		msgFindIds.SetUsedNum(0);
	}

	if ((flags & UF_NO_FITSIZE) == 0) {
		FitSize();
	}

	// redraw が不十分となるための暫定対処（後日調査）
	parentView->toolBar.UpdateTitleStr(STATUS_STATIC, L"");
	WCHAR	status[256];
	if (!ret) {
		GetNotFoundStatStr(status, sizeof(status));
		parentView->toolBar.UpdateTitleStr(STATUS_STATIC, status);
	}
	else if (!find_ret) {
		parentView->toolBar.UpdateTitleStr(STATUS_STATIC, L"Not Found");
	}

	SetStatusMsg(1, ret ? NULL : status, 0, TRUE);

	findTick = 0;

	UpdateBtnBmp();
/*	Debug("start draw(%d)=%d\n", msgIds.UsedNumInt(), tick.elaps());

	for (int i=0; i < msgIds.UsedNumInt(); i++) {
		GetPaintMsgCy(i);
	}
	Debug("end draw=%d\n", tick.elaps());
*/
	return	ret;
}


void TChildView::ToggleFindUser(const Wstr &uid)
{
	if (parentView->IsUserComboEx()) {
		parentView->SwitchUserCombo();
	}

	if (findUser.IsEmpty() || findUser != uid) {
		findUser = uid;
	}
	else {
		findUser = NULL;
	}
	UpdateMsgList(UF_NO_FINDMOVE);
	UpdateUserCombo();
}

BOOL TChildView::JumpLink(Item *item, BOOL is_dblclk)
{
	Wstr	wbuf;

	if (!GetLinkStr(item, &wbuf)) {
		return	FALSE;
	}

	if (JumpLinkWithCheck(this, cfg, wbuf.s(), JLF_VIEWER|(is_dblclk ? JLF_DBLCLICK : 0))
		== JLR_FAIL) {
		SetStatusMsg(0, FmtW(L"%s (%d)", LoadStrW(IDS_OPENLINK_ERR), GetLastError()), 500);
	}

	return	TRUE;
}

BOOL TChildView::MsgToClip(ViewMsg *msg, CopyMsgFlags flags)
{
	Wstr	wbuf;

	if (logMng->MakeMsgStr(&msg->msg, &wbuf, flags)) {
		TSetClipBoardTextW(hWnd, wbuf.s());
	}
	return	TRUE;
}

BOOL VBufAddStr(VBuf *vbuf, const WCHAR *s, int len, BOOL is_lf=FALSE)
{
	int	max_len = (is_lf ? (len * 2) : len) + 1; // UnixNewLineToLocal

	if (vbuf->RemainSize() < max_len * sizeof(WCHAR)) {
		if (!vbuf->Grow(max_len * sizeof(WCHAR))) {
			return	FALSE;
		}
	}

	int	used_len;

	if (is_lf) {
		used_len = UnixNewLineToLocalW(s, (WCHAR *)vbuf->UsedEnd(), max_len, len);
	} else {
		used_len = len;
		memcpy(vbuf->UsedEnd(), s, len * sizeof(WCHAR));
	}

	vbuf->AddUsedSize(used_len * sizeof(WCHAR));
	memset(vbuf->UsedEnd(), 0, sizeof(WCHAR));

	return	TRUE;
}

BOOL TChildView::ImgToClip(const WCHAR *img)
{
	BOOL	ret = FALSE;
	WCHAR	clip_path[MAX_PATH];

	MakeClipPathW(clip_path, cfg, img);
	Bitmap	*bmp = Bitmap::FromFile(clip_path);
	HBITMAP	hBmp = NULL;

	if (bmp) {
		if (!bmp->GetHBITMAP(Color::White, &hBmp) && hBmp) {
			if (HBITMAP hDdbBmp = TDIBtoDDB(hBmp)) {
				if (::OpenClipboard(hWnd)) {
					::EmptyClipboard();
					if (::SetClipboardData(CF_BITMAP, hDdbBmp)) {
						ret = TRUE;
					}
					::CloseClipboard();
				}
				::DeleteObject(hDdbBmp);
			}
			::DeleteObject(hBmp);
		}
		delete bmp;
	}

	return	ret;
}

BOOL TChildView::GetLinkStr(Item *item, Wstr *w)
{
	if (!item) {
		return	FALSE;
	}

	BodyAttr	link;

	if (ViewMsg *msg = GetMsg(item->msg_id)) {
		if (GetLinkAttr(item, &link)) {
			w->Init(msg->msg.body.s() + link.pos, link.len);
			return	TRUE;
		}
	}

	return	FALSE;
}

BOOL TChildView::GetLinkAttr(Item *item, BodyAttr *attr)
{
	if (!item) {
		return	FALSE;
	}
	if (ViewMsg *msg = GetMsg(item->msg_id)) {
		if (item->kind & Item::LINK) {
			if (item->link_idx >= 0 && item->link_idx < msg->link.size()) {
				*attr = msg->link[item->link_idx];
				return	TRUE;
			}
		}
	}
	return	FALSE;
}

BOOL TChildView::LinkToClip(Item *item)
{
	Wstr	w;

	if (GetLinkStr(item, &w)) {
		TSetClipBoardTextW(hWnd, w.s());
		return	TRUE;
	}
	return	FALSE;
}

BOOL TChildView::ItemToClip(Item *item)
{
	if (!item) {
		return	FALSE;
	}
	if (ViewMsg *msg = GetMsg(item->msg_id)) {
		if (item->clip_idx >= 0 && item->clip_idx < msg->msg.clip.size()) {
			ImgToClip(msg->msg.clip[item->clip_idx].fname.s());
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL TChildView::CopyClipPath(Item *item)
{
	if (!item) {
		return	FALSE;
	}
	if (ViewMsg *msg = GetMsg(item->msg_id)) {
		if (item->clip_idx >= 0 && item->clip_idx < msg->msg.clip.size()) {
			WCHAR	clip_path[MAX_PATH]={};
			MakeClipPathW(clip_path, cfg, msg->msg.clip[item->clip_idx].fname.s());
			TSetClipBoardTextW(hWnd, clip_path);
		}
	}
	return	TRUE;
}

BOOL TChildView::SaveClip(Item *item)
{
	if (!item) {
		return	FALSE;
	}
	if (ViewMsg *msg = GetMsg(item->msg_id)) {
		if (item->clip_idx >= 0 && item->clip_idx < msg->msg.clip.size()) {
			WCHAR	clip_path[MAX_PATH]={};
			MakeClipPathW(clip_path, cfg, msg->msg.clip[item->clip_idx].fname.s());
	
			char	fname[MAX_PATH_U8];
			MakeImageTmpFileName(cfg->lastSaveDir, fname);
			OpenFileDlg	dlg(this->parent, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);

			if (dlg.Exec(fname, sizeof(fname), NULL, "PNG file(*.png)\0*.png\0\0",
				cfg->lastSaveDir, "png")) {
				::CopyFileW(clip_path, U8toWs(fname), FALSE);
				GetParentDirU8(fname, cfg->lastSaveDir);
				cfg->WriteRegistry(CFG_GENERAL);
				TOpenExplorerSelOneW(U8toWs(fname));
			}

		}
	}
	return	TRUE;
}

BOOL TChildView::SelToClip()
{
	if (!IsSelected(TRUE)) {
		return	FALSE;
	}

	ViewMsg	*msg = GetMsg(selTop.msg_id);
	if (!msg) {
		return	FALSE;
	}

	VBuf	vbuf(1024, 10*1024*1024);
	int		top = GetMsgIdx(selTop.msg_id);
	int		end = GetMsgIdx(selEnd.msg_id);

	for (int i=top; i <= end && i < msgIds.UsedNumInt(); i++) {
		if (!(msg = GetMsg(msgIds[i].msg_id))) {
			return	FALSE;
		}
		if (i > top) {
			if (vbuf.UsedSize() >= 4 && ((WCHAR *)vbuf.UsedEnd())[-1] != '\n') {
				VBufAddStr(&vbuf, L"\r\n\r\n", 4);
			}
			else {
				VBufAddStr(&vbuf, L"\r\n", 2);
			}
		}
		if (i > top || selTop.inc_head) {
#define HEADER_MAX	8192
			Wstr	wstr(HEADER_MAX);
			int		hlen = logMng->MakeMsgHead(&msg->msg, wstr.Buf(), HEADER_MAX);
			VBufAddStr(&vbuf, wstr.s(), hlen);
		}
		int	len = msg->msg.body.Len();
		int	off = 0;
		if (i == top) {
			off = selTop.pos;
			if (i == end) {
				len = selEnd.pos - selTop.pos;
			} else {
				len -= selTop.pos;
			}
		}
		else if (i == end) {
			len = selEnd.pos;
		}
		VBufAddStr(&vbuf, msg->msg.body.s() + off, len, TRUE);
	}
	TSetClipBoardTextW(hWnd, (WCHAR *)vbuf.Buf());

	return	TRUE;
}

BOOL TChildView::CmntToClip(int64 msg_id)
{
	ViewMsg	*msg = GetMsg(msg_id);
	if (!msg || !msg->msg.comment) {
		return	FALSE;
	}

	TSetClipBoardTextW(hWnd, msg->msg.comment.s());

	return	TRUE;
}

void AddMarkWithCompaction(vector<LogMark> *_log_vec, int pos, int len)
{
	vector<LogMark>	&log_vec = *_log_vec;
	bool			done = false;

	for (auto itr=log_vec.begin(); itr != log_vec.end(); itr++) {
		if (pos > itr->pos + itr->len || itr->pos > pos + len) {
			continue;
		}
		int tmp_pos = min(pos,       itr->pos);
		int tmp_end = max(pos + len, itr->pos + itr->len);

		LogMark &mark = *itr;
		mark.pos = tmp_pos;
		mark.len = tmp_end - tmp_pos;

		itr++;
		while (itr != log_vec.end()) {
			if (pos > itr->pos + itr->len || itr->pos > pos + len) {
				break;
			}
			int tmp_pos = min(itr->pos,            mark.pos);
			int tmp_end = max(itr->pos + itr->len, mark.pos + mark.len);

			mark.pos = tmp_pos;
			mark.len = tmp_end - tmp_pos;

			itr = log_vec.erase(itr);
		}
		done = true;
		break;
	}

	if (!done) {
		auto itr = find_if(log_vec.begin(), log_vec.end(), [&](auto &l) {
			return	pos < l.pos;
		});

		if (itr != log_vec.end()) {
			LogMark	mark;
			mark.kind = 0;
			mark.pos = pos;
			mark.len = len;
			log_vec.insert(itr, mark);
		}
		LogMark	mark;
		mark.kind = 0;
		mark.pos = pos;
		mark.len = len;
		log_vec.insert(itr, mark);
	}
}

BOOL TChildView::PosInMark(int64 msg_id, int body_pos, int *mark_idx)
{
	ViewMsg	*msg = GetMsg(msg_id);

	if (!msg) {
		return	FALSE;
	}

	for (int i=0; i < msg->msg.mark.size(); i++) {
		LogMark	&mark = msg->msg.mark[i];

		if (body_pos >= mark.pos && body_pos < mark.pos + mark.len) {
			if (mark_idx) {
				*mark_idx = i;
			}
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL TChildView::SelToMark()
{
	if (!IsSelected(TRUE)) {
		return	FALSE;
	}

	int		top = GetMsgIdx(selTop.msg_id);
	int		end = GetMsgIdx(selEnd.msg_id);
	BOOL	need_update = FALSE;

	for (int i=top; i <= end && i < msgIds.UsedNumInt(); i++) {
		if (ViewMsg *msg = GetMsg(msgIds[i].msg_id)) {
			int		pos = (i == top) ? selTop.pos : 0;
			int		len = (i == end) ?
				selEnd.pos - (i == top ? selTop.pos : 0) :
				msg->msg.body.Len();

			if (len > 0) {
				AddMarkWithCompaction(&msg->msg.mark, pos, len);
				UpdateMsg(msg);
				need_update = TRUE;
			}
		}
	}

	if (need_update) {
		AllSelInit();
		InvalidateRect(0, 0);
	}

	return	TRUE;
}

BOOL TChildView::SelToMarkOff()
{
	BOOL	need_update = FALSE;

	if (IsSelected(TRUE)) {
		int		top = GetMsgIdx(selTop.msg_id);
		int		end = GetMsgIdx(selEnd.msg_id);

		for (int i=top; i <= end && i < msgIds.UsedNumInt(); i++) {
			if (ViewMsg *msg = GetMsg(msgIds[i].msg_id)) {
				msg->msg.mark.clear();
				UpdateMsg(msg);
				need_update = TRUE;
			}
		}
	}
	else if (itemMenu && (itemMenu->kind & Item::BODY)) {
		if (ViewMsg *msg = GetMsg(itemMenu->msg_id)) {
			int	idx;
			if (PosInMark(itemMenu->msg_id, itemMenu->body_pos, &idx)) {
				msg->msg.mark.erase(msg->msg.mark.begin() + idx);
				UpdateMsg(msg);
				need_update = TRUE;
			}
		}
	}

	if (need_update) {
		AllSelInit();
		InvalidateRect(0, 0);
	}

	return	TRUE;
}

BOOL TChildView::GetOneSelStr(Wstr *wbuf, BOOL oneline_only)
{
	if (!IsSelected(TRUE) || selEnd.msg_id != selTop.msg_id) {
		return	FALSE;
	}

	ViewMsg	*msg = GetMsg(selTop.msg_id);
	if (!msg) {
		return	FALSE;
	}

	const WCHAR	*top = msg->msg.body.s() + selTop.pos;
	int			len  = selEnd.pos - selTop.pos;

	if (len == 0) {
		return	FALSE;	// 無い筈
	}
	if (oneline_only) {
		const WCHAR *p = wcsnchr(top, '\n', len);
		if (p) {
			if (len > 1 && (p - top) == len-1) {
				len--;
			}
			else {
				return	FALSE;
			}
		}
	}

	wbuf->Init(top, len);
	return	TRUE;
}

BOOL TChildView::SelToFind()
{
	Wstr	wbuf;

	if (!GetOneSelStr(&wbuf, TRUE)) {
		return	FALSE;
	}
	parentView->bodyCombo.SetWindowTextW(wbuf.s());
	parentView->bodyCombo.SetFocus();
	StartFindTimer(/*UF_NO_FINDMOVE|*/UF_NO_INITSEL);

	return	TRUE;
}

#define SEARCH_URL	L"https://www.google.com/search?q="

BOOL TChildView::SelToSearchEng()
{
	Wstr	wbuf;

	if (!GetOneSelStr(&wbuf, TRUE)) {
		return	FALSE;
	}

	Wstr	url(wbuf.Len() + 1024);
	int		len = 0;

	len = wcscpyz(url.Buf(), SEARCH_URL);
	wcscpyz(url.Buf() + len, wbuf.s());	// 検索語のescapeは後日

	ShellExecuteW(NULL, NULL, url.s(), 0, 0, SW_SHOW);

	return	TRUE;
}

BOOL TChildView::FitSize()
{
	if (!hWnd) {
		return FALSE;
	}

	TRect	prc;
	parentView->GetClientRect(&prc);
	if (prc.cy() == 0) {
		return FALSE;
	}

	SetScrollPos(FALSE);

	MoveWindow(0, toolBarCy, prc.cx(), prc.cy() - toolBarCy - statusCtrlCy, TRUE);
	GetClientRect(&crc);
	vrc = crc;
	vrc.Inflate(-CVW_GAPX, -CVW_GAPY);

	SetFoldMeasure();

	itemMgr.SetCy(crc.cy());

	if (hMemBmpSave) {
		::SelectObject(hMemDc, hMemBmpSave);
		::DeleteObject(hMemBmp);
		hMemBmp = hMemBmpSave = NULL;
	}

	HWND	hDesktop   = ::GetDesktopWindow();
	HDC		hDesktopDc = ::GetDC(hDesktop);
	if (!hDesktopDc) {
		return FALSE;
	}

	HBITMAP hMemBmpNew = ::CreateCompatibleBitmap(hDesktopDc, vrc.cx(), vrc.cy());
	::ReleaseDC(hDesktop, hDesktopDc);

	if (!hMemBmpNew) {
		return FALSE;
	}
	hMemBmp = hMemBmpNew;
	hMemBmpSave	= (HBITMAP)::SelectObject(hMemDc, hMemBmp);

	CalcMaxLineChars();

	InvalidateRect(0, 0);

	return	TRUE;
}

BOOL TChildView::IsUnOpenCore(LogMsg *msg, bool is_receive, int idx)
{
	if (msg->msg_id >= beginMsgId &&
		((msg->IsRecv() ^ is_receive) == 0)) {
		if (idx >= 0) {
			return	(msg->host.size() > idx &&
				(msg->host[idx].flags & DB_FLAGMH_UNOPEN)) ? TRUE : FALSE;
		}
		for (auto &h: msg->host) {
			if ((h.flags & DB_FLAGMH_UNOPEN)) {
				return	TRUE;
			}
		}
	}
	return	FALSE;
}

void TChildView::SetFoldMeasure()
{
	maxFoldCy = max(vrc.cy() * 2 / 3, 50);
	if (lineCy > 0) {
		maxLines = max((maxFoldCy / lineCy) - FOLD_MARGIN, 2); // foldしない最大行
	}
	else {
		maxLines = 0;
	}
}

BOOL TChildView::SetFont()
{
	tepGenNo++;

	// フォントセット
	HDC hDc = ::GetDC(hWnd);

	if (*cfg->LogViewFont.lfFaceName == 0) {
		char	*font_list = strdup(LoadStrA(IDS_LOGVIEWFONT));
		char	*p;

		for (char *tok = sep_tok(font_list, '/', &p); tok; tok=sep_tok(NULL, '/', &p)) {
			char	*p2;
			char	*height	= sep_tok(tok, ':', &p2);
			char	*face	= sep_tok(NULL, 0,  &p2);

			if (!height || !face) {
				break;
			}
			cfg->LogViewFont.lfCharSet = DEFAULT_CHARSET;
			POINT pt={}, pt2={};
			pt.y = ::MulDiv(::GetDeviceCaps(hDc, LOGPIXELSY), atoi(height), 72);
			::DPtoLP(hDc, &pt,  1);
			::DPtoLP(hDc, &pt2, 1);
			cfg->LogViewFont.lfHeight = int(-abs(pt.y - pt2.y));

			strcpy(cfg->LogViewFont.lfFaceName, face);

			// 正確なフォントが見つかったら終了
			if (IsInstalledFont(hDc, AtoWs(face))) {
				break;
			}
		}
		free(font_list);
	}

	// 特殊フォント類
	if (*cfg->LogViewFont.lfFaceName) {
		if (hFont) ::DeleteObject(hFont);
		hFont = ::CreateFontIndirect(&cfg->LogViewFont);
		::GetObject(hFont, sizeof(LOGFONT), &cfg->LogViewFont);

		if (hBoldFont) ::DeleteObject(hBoldFont);
		LOGFONT	bold = cfg->LogViewFont;
		bold.lfWeight = 700;
		hBoldFont = ::CreateFontIndirect(&bold);

		if (hLinkFont) ::DeleteObject(hLinkFont);
		LOGFONT	link = cfg->LogViewFont;
	//	link.lfWeight = 600;
	//	link.lfUnderline = TRUE;
		hLinkFont = ::CreateFontIndirect(&link);

		if (hMiniFont) ::DeleteObject(hMiniFont);
		LOGFONT	mini = cfg->LogViewFont;
		mini.lfHeight = lround(mini.lfHeight * 0.9);
		hMiniFont = ::CreateFontIndirect(&mini);

//		if (hTinyFont) ::DeleteObject(hTinyFont);
//		LOGFONT	tiny = cfg->LogViewFont;
//		tiny.lfHeight = 11;
//		hTinyFont = ::CreateFontIndirect(&tiny);
	}

	::SelectObject(hMemDc, hFont);

	// 設定フォントに基づく、表示行の計算
	HFONT		hOldFont = (HFONT)::SelectObject(hDc, hFont);
	SIZE		size	 = {};
	TEXTMETRIC	tm		 = {};

	::GetTextMetrics(hDc, &tm);
	fontCy    = tm.tmHeight;
	fontLdCy  = tm.tmInternalLeading;	// フォント自体に余白を入れるとは。
	lineDiff  = max(lround((fontCy * 3.0 / 8) - fontLdCy -2), 1);
	lineDiff3 = lineDiff / 3;
	lineCy    = fontCy + lineDiff;

	headCy		= lround(lineCy * 1.5);
	headDiff	= lround(headCy / 3.0); // 0.6
	headDiff2	= lround(headDiff / 2.0);
	headOnlyCy	= (headCy + 1);	// タイトル用cy
	headNormCy	= (headCy * 2);	// 通常用cy with margin
//lineCy=24 lineDiff=7 lineDiff3=2
//headCy=36 headDiff=12 headDiff2=6

//	Debug("lineCy=%d lineDiff=%d lineDiff3=%d\n", lineCy, lineDiff, lineDiff3);
//	Debug("headCy=%d headDiff=%d headDiff2=%d\n", headCy, headDiff, headDiff2);
//	Debug("I-Lead=%d E-Lead=%d\n", tm.tmInternalLeading, tm.tmExternalLeading);
	SetFoldMeasure();

	::SelectObject(hDc, hOldFont);
	::ReleaseDC(hWnd, hDc);

	return	TRUE;
}

BOOL TChildView::SelectMainFont()
{
	LOGFONT		tmpFont;
	CHOOSEFONT	cf = {sizeof(cf)};

	cf.hwndOwner	= hWnd;
	cf.lpLogFont	= &(tmpFont = cfg->LogViewFont);
	cf.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_SHOWHELP;
	cf.nFontType	= SCREEN_FONTTYPE;

	if (::ChooseFont(&cf)) {
		cfg->LogViewFont = tmpFont;
		SetFont();
		cfg->WriteRegistry(CFG_FONT);
		ClearMsgCacheAll();
		InvalidateRect(0, 0);
	}
	return	TRUE;
}

int TChildView::AnalyzeUrlMsgCore(std::vector<BodyAttr> *link, const WCHAR *s)
{
	BodyAttr	attr;
	wcmatch		wm;
	wregex		&main_re = (cfg->linkDetectMode & 1) ? *cfg->url_re : *cfg->urlex_re;

	for (int i=0; s[i]; i=attr.pos+attr.len) {
		if (!regex_search(s+i, wm, main_re)) {
			break;
		}
		attr.pos = i + (int)wm.position(0);
		attr.len =     (int)wm.length(0);

		// ref. cfg.cpp InitLinkRe
		WCHAR ch = s[attr.pos];
		if (attr.len >= 2 && (ch == ' ' || ch == '\t')) {
			attr.pos++;
			attr.len--;
		}

		if ((cfg->linkDetectMode & 1) && attr.len >= 7) {
			if (wcsnicmp(s + attr.pos, L"file://", 7) == 0) {
				continue;
			}
		}
		link->push_back(attr);

	//	DebugW(L"URL=<%.*s>\n", attr.len, s + attr.pos);
	}
	return	(int)link->size();
}

int TChildView::AnalyzeUrlMsg(ViewMsg *msg)
{
	msg->link.clear();
	AnalyzeUrlMsgCore(&msg->link, msg->msg.body.s());

	msg->linkCmnt.clear();
/*	if (msg->msg.comment.s()) {
		AnalyzeUrlMsgCore(&msg->linkCmnt, msg->msg.comment.s());
	} */

	return	int(msg->link.size() + msg->linkCmnt.size());
}

int calc_charnum(const WCHAR *s, WCHAR ch) {
	int	num = 0;

	for ( ; (s = wcschr(s, ch)); s++) {
		num++;
	}
	return	num;
}

#define TAB_NUM 8
int tab_to_space(Wstr *str) {
	int	tab_num = calc_charnum(str->s(), '\t');

	if (tab_num == 0) {
		return 0;
	}

	Wstr	tmp(str->Len() + tab_num*TAB_NUM + 1);

	const WCHAR	*s = str->s();
	WCHAR		*p = tmp.Buf();

	for ( ; (*p = *s); p++, s++) {
		if (*p == '\t') {
			*p = ' ';
			for (int i=1; i < TAB_NUM; i++) {
				*++p= ' ';
			}
		}
	}
	*str = tmp;
	return	tab_num;
}

static const WCHAR *wcsstr_vec(const WCHAR *buf, const vector<Wstr> &key_vec, int *idx)
{
	const WCHAR *ret = NULL;

	for (auto itr=key_vec.begin(); itr != key_vec.end(); itr++) {
		if (const WCHAR *p = wcsstr(buf, itr->s())) {
			if (!ret || p < ret) {
				ret = p;
				if (idx) {
					*idx = int(itr - key_vec.begin());
				}
			}
		}
	}
	return	ret;
}

int TChildView::AnalyzeFindMsg(ViewMsg *msg)
{
	msg->find.clear();

	if (findBody) {
		Wstr		body(msg->msg.body.Len() + 1);
		ToUpperANSI(msg->msg.body.s(), body.Buf());
		const WCHAR	*s = body.s();
		const WCHAR	*p = s;

		while (p && *p) {
			int		idx = 0;
			if (!(p = wcsstr_vec(p, findBodyVec, &idx))) {
				break;
			}
			BodyAttr attr;

			attr.pos = int(p - s);
			attr.len = findBodyVec[idx].Len();

			int	ivs_len = 0;
			if (IsIVS(p + attr.len, &ivs_len)) {
				attr.len += ivs_len;
			}

			msg->find.push_back(attr);
			p+= attr.len;
		}
	}
	msg->findGenNo = findGenNo;

	return	(int)msg->find.size();
}

int TChildView::AnalyzeQuoteMsg(ViewMsg *msg)
{
	if ((dispFlags & DISP_FOLD_QUOTE) == 0) {
		msg->quotePos = 0;
		return 0;
	}

	msg->quotePos = FindTailQuoteIdx(cfg, msg->msg.body);

	return	msg->quotePos;
}

int TChildView::AnalyzeMsg(ViewMsg *msg)
{
	int	num = 0;

	tab_to_space(&msg->msg.body);

	num = AnalyzeUrlMsg(msg);

	if (findBody) {
		AnalyzeFindMsg(msg);
	}

	AnalyzeQuoteMsg(msg);

	return	num;
}

ViewMsg *TChildView::GetMsg(int64 msg_id)
{
	ViewMsg	*msg = NULL;
	auto	itr = msgMap.find(msg_id);

	if (itr == msgMap.end()) {
		if (msgMap.size() >= 10000) {		// 最大msgキャッシュサイズ（後日可変に）
			msg = msgLru.TopObj();	//  小さくしても意外と体感速度はさほど低下せず
			msgMap.erase(msg->msg.msg_id);
			msgLru.DelObj(msg);
			msg->Init();
		}
		else {
			msg = new ViewMsg;
		}
		if (!logDb->SelectOneData(msg_id, &msg->msg)) {
			delete msg;
			return NULL;
		}
		msgMap[msg_id] = msg;
		msgLru.AddObj(msg);
		AnalyzeMsg(msg);
		msg->unOpenR = IsUnOpenR(&msg->msg) ? true : false;
	}
	else {
		msg = itr->second;
		msgLru.DelObj(msg);	// update LRU
		msgLru.AddObj(msg);
		if (msg->findGenNo != findGenNo) {
			AnalyzeFindMsg(msg);
		}
	}

	return	msg;
}

BOOL TChildView::ClearMsgCacheAll()
{
	while (ViewMsg *msg = msgLru.TopObj()) {
		msgLru.DelObj(msg);
		delete msg;
	}
	msgMap.clear();

	return	TRUE;
}

BOOL TChildView::ClearMsgCache(int64 msg_id)
{
	auto	itr = msgMap.find(msg_id);

	if (itr == msgMap.end()) {
		return	FALSE;
	}
	ViewMsg	*msg = itr->second;

	msgMap.erase(itr);
	msgLru.DelObj(msg);
	delete msg;

	return	TRUE;
}

BOOL TChildView::UpdateLog(int64 msg_id)
{
	if (!logDb) {
		return	FALSE;
	}

	if (msg_id == 0) {
		ClearMsgCacheAll();
		UpdateMsgList(UF_NO_FINDMOVE);
	}
	else {
		ClearMsgCache(msg_id);
		UpdateMsgList(UF_NO_FINDMOVE);

		if (hWnd && scrollMode == AUTO_SCROLL && msgIds.UsedNumInt() > 0) {
			BOOL	need_scroll = TRUE;

			if (dispFlags & DISP_REV) {
				need_scroll = (msgIds[0].msg_id == msg_id) ? TRUE : FALSE;
			}
			else {
				need_scroll = (msgIds[msgIds.UsedNumInt() - 1].msg_id == msg_id) ? TRUE : FALSE;

				if (need_scroll && endDispIdx < msgIds.UsedNumInt()) {
					int	margin = endDispPix;
					for (int i=endDispIdx+1; i < msgIds.UsedNumInt(); i++) {
						if ((margin -=GetPaintMsgCy(msg_id)) < 0) {
							break;
						}
					}
					if (margin > 0) {
						need_scroll = FALSE;
					}
				}
			}
			if (need_scroll) {
				SetFindedIdx((dispFlags & DISP_REV) ? TOP_IDX : LAST_IDX, SF_NONE);
//				Debug("run autoscroll(%lld)\n", msg_id);
			}
		}
	}
	InvalidateRect(0, 0);

	return	TRUE;
}

BOOL TChildView::UserComboSetup()
{
	int		idx = (int)parentView->userCombo.SendMessage(CB_GETCURSEL, 0, 0);
	Wstr	last;

	if (idx > 0 && idx <= hostList.UsedNum()) {
		last = hostList[idx-1].uid;
	}

	if (!logDb->SelectHostList(&hostList)) {
		return	FALSE;
	}

	parentView->userCombo.SendMessage(CB_RESETCONTENT, 0, 0);
	parentView->userCombo.SendMessageW(CB_INSERTSTRING, 0, (LPARAM)L"All Users");
	parentView->userCombo.SendMessage(CB_SETCURSEL, 0, 0);

	for (int i=0; i < hostList.UsedNum(); i++) {
		LogHost	&host = hostList[i];
		parentView->userCombo.SendMessageW(CB_INSERTSTRING, i+1, (LPARAM)host.nick.s());
		if (last && host.uid == last) {
			parentView->userCombo.SendMessage(CB_SETCURSEL, i+1, 0);
		}
	}
	return	TRUE;
}

BOOL TChildView::UpdateUserCombo()
{
	if (!findUser) {
		parentView->userCombo.SendMessage(CB_SETCURSEL, 0, 0);
		return	TRUE;
	}

	for (int i=0; i < hostList.UsedNum(); i++) {
		LogHost	&host = hostList[i];
		if (host.uid == findUser) {
			parentView->userCombo.SendMessage(CB_SETCURSEL, i+1, 0);
			break;
		}
	}
	return	TRUE;
}

BOOL TChildView::UserComboClose()
{
	int		idx = (int)parentView->userCombo.SendMessage(CB_GETCURSEL, 0, 0);

	SetFocus();

	if (idx > 0 && idx <= hostList.UsedNum()) {
		ToggleFindUser(hostList[idx-1].uid);
	}
	else {
		Wstr	empty_uid;
		ToggleFindUser(empty_uid);
	}
	return	TRUE;
}

BOOL TChildView::ReplyMsg(ViewMsg *msg, int idx)
{
	if (msg->msg.flags & DB_FLAG_MEMO) {
		AddMemo();
		return	TRUE;
	}

	vector<HostSub>	replyList;
	ReplyInfo		rInfo(ReplyInfo::POS_RIGHT);
	Wstr			pre_body;
	U8str			body;
	BOOL			is_selected = IsSelected(TRUE);

	if (is_selected) {
		int		pos = 0;
		int		len = msg->msg.body.Len();

		if (selTop.msg_id == msg->msg.msg_id) {
			pos = selTop.pos;
			if (selEnd.msg_id == msg->msg.msg_id) {
				len = selEnd.pos - selTop.pos;
			}
		}
		else {
			if (selEnd.msg_id == msg->msg.msg_id) {
				len = selEnd.pos;
			}
		}
		pre_body.Init(len * 2 + 1); // \n -> \r\n
		UnixNewLineToLocalW(msg->msg.body.s() + pos, pre_body.Buf(), len * 2, len);
	}
	else {
		int	len = msg->msg.body.Len();
		pre_body.Init(len * 2); // \n -> \r\n
		UnixNewLineToLocalW(msg->msg.body.s(), pre_body.Buf(), len * 2, len);
	}
	if ((cfg->QuoteCheck & 0x2) == 0 && !is_selected) {
		int	len = pre_body.Len() * 4 + 1; // MAX utf-8
		body.Init(pre_body);
		TruncTailQuote(cfg, body.s(), body.Buf(), len);
	}
	else {
		body.Init(pre_body);
	}

	rInfo.replyList		= &replyList;
	rInfo.foreDuration	= SENDMSG_FOREDURATION;
	rInfo.body			= (::GetKeyState(VK_SHIFT) & 0x8000) ? NULL : &body;
	rInfo.isMultiRecv	= (msg->msg.flags & DB_FLAG_MULTI) ? TRUE : FALSE;

	for (int i=0; i < msg->msg.host.size(); i++) {
		if (idx == i || idx == -1) {
			LogHost		&host = msg->msg.host[i];
			HostSub		hs;
			WtoU8(host.uid.s(),  hs.u.userName, sizeof(hs.u.userName));
			WtoU8(host.host.s(), hs.u.hostName, sizeof(hs.u.hostName));

			if (i >= 1 && msg->msg.IsRecv() && idx != i &&
				(host.IsSameUidHost(msg->msg.host[0]) ||
				 IsSameHost(&hs, MsgMng::GetLocalHost()))) { // 自分自身は除外
				continue;
			}
			hs.addr.Set(WtoU8s(host.addr.s()));
			replyList.push_back(hs);
		}
	}

	::SendMessage(GetMainWnd(), WM_SENDDLG_OPEN, 0, (LPARAM)&rInfo);
	return	TRUE;
}

BOOL TChildView::EditBody(int64 msg_id)
{
	ViewMsg *msg = GetMsg(msg_id);

	if (!msg) {
		return	FALSE;
	}

	Wstr		org_body = msg->msg.body;
	Wstr		out_body;
	TMsgEditDlg	dlg(cfg, MSGBODY_DIALOG, this);

	if (dlg.Exec(&org_body, &out_body, (msg->msg.flags & DB_FLAG_MEMO) ? IDS_MEMOMOD : 0) != IDOK
		|| org_body == out_body) {
		return	FALSE;
	}

	if ((msg = GetMsg(msg_id))) {	// modal中の update対策
		msg->msg.body = out_body;
		msg->msg.alter_date = time(NULL);
		UpdateMsg(msg);
		ClearMsgCache(msg->msg.msg_id);
		UpdateMsgList(UF_NO_FINDMOVE);
		if (msgIds[curIdx].msg_id == msg_id && offsetPix < 0) {
			offsetPix = 0;
		}
		InvalidateRect(0, 0);
	}
	return	TRUE;
}

BOOL TChildView::DelMsg(int64 msg_id)
{
	ViewMsg *msg = GetMsg(msg_id);

	if (!msg) {
		return	FALSE;
	}

	if (TMsgBox(this).Exec(LoadStrU8(IDS_DELMSG)) != IDOK) {
		return	FALSE;
	}

	if ((msg = GetMsg(msg_id))) {	// modal中の update対策
		DeleteMsg(msg_id);
		ClearMsgCache(msg_id);
		UpdateMsgList(UF_NO_FINDMOVE);
		if (msgIds[curIdx].msg_id == msg_id && offsetPix < 0) {
			offsetPix = 0;
		}
		InvalidateRect(0, 0);
	}
	return	TRUE;
}

BOOL TChildView::DelSelMsgs(int top_idx, int end_idx)
{
	if (TMsgBox(this).Exec(Fmt(LoadStrU8(IDS_DELALLMSG_FMT), end_idx - top_idx + 1)) != IDOK) {
		return	FALSE;
	}

	logDb->Begin();
	for (int i=end_idx; i >= top_idx && i < msgIds.UsedNumInt(); i--) {
		int64	msg_id = msgIds[i].msg_id;
		DeleteMsg(msg_id, FALSE);
		ClearMsgCache(msg_id);
	}
	logDb->Commit();
	RequestResetCache();
	UpdateMsgList(UF_NO_FINDMOVE);
	InvalidateRect(0, 0);

	return	TRUE;
}

BOOL TChildView::DelClip(int64 msg_id, int clip_idx)
{
	if (TMsgBox(this).Exec(LoadStrU8(IDS_DELCLIPFULL)) != IDOK) {
		return	FALSE;
	}

	if (ViewMsg *msg = GetMsg(msg_id)) {
		int	idx = clip_idx;
		if (idx >= 0 && idx < msg->msg.clip.size()) {
			WCHAR	clip_path[MAX_PATH]={};
			MakeClipPathW(clip_path, cfg, msg->msg.clip[idx].fname.s());
			if (!::DeleteFileW(clip_path)) {
				DebugW(L"DeleteFileW(%s) %d\n", clip_path, GetLastError());
			}

			msg->msg.clip.erase(msg->msg.clip.begin() + idx);
			UpdateMsg(msg);

			ClearMsgCache(msg->msg.msg_id);
			UpdateMsgList(UF_NO_FINDMOVE);
			InvalidateRect(0, 0);
		}
	}
	return	TRUE;
}

BOOL TChildView::EditMsgHead(int64 msg_id)
{
	ViewMsg *msg = GetMsg(msg_id);

	if (!msg) {
		return	FALSE;
	}

	int	idx = 0;

	if (findUser) {
		for (int i=0; i < msg->msg.host.size(); i++) {
			if (findUser == msg->msg.host[i].uid) {
				idx = i;
				break;
			}
		}
	}

	LogHost		org_host = msg->msg.host[idx];
	LogHost		out_host;

	TMsgHeadEditDlg	dlg(cfg, this);
	if (dlg.Exec(&org_host, &out_host) != IDOK || org_host == out_host) {
		return	FALSE;
	}

	if ((msg = GetMsg(msg_id)) &&
		idx < msg->msg.host.size() && org_host == msg->msg.host[idx]) {
		logDb->UpdateHostInfo(&msg->msg.host[idx], &out_host);
		ClearMsgCacheAll();
		UpdateMsgList(UF_NO_FINDMOVE);
		InvalidateRect(0, 0);
		RequestResetCache();
	}
	return	TRUE;
}

BOOL TChildView::AddMemoCore(LogMsg *msg)
{
	LogHost	host;

	if (!logDb->SelectHost(MEMO_HOST_ID, &host)) {
		return	FALSE;
	}

	msg->host.push_back(host);
	msg->flags = DB_FLAG_MEMO;
	msg->date = time(NULL);

	BOOL ret = logDb->InsertOneData(msg);

	RequestResetCache();

	return	ret;
}

BOOL TChildView::AddMemo()
{
	Wstr	body;
	LogMsg	log_msg;

	log_msg.Init();

	TMsgEditDlg	dlg(cfg, MSGBODY_DIALOG, this);

	if (dlg.Exec(&log_msg.body, &body, IDS_MEMO) == IDOK && log_msg.body != body) {
		log_msg.body = body;

		if (AddMemoCore(&log_msg)) {
			UpdateMsgList(UF_NO_FINDMOVE);
			offsetPix = 0;
			if ((dispFlags & DISP_REV) == 0) {
				offsetPix = vrc.cy() - GetPaintMsgCy(log_msg.msg_id);
			}
			SetNewCurIdxByMsgId(log_msg.msg_id);
			SetScrollPos();
			return	TRUE;
		}
	}
	return	FALSE;
}

// mode: 0:auto, 1:text, 2:image
BOOL TChildView::PasteComment(int mode)
{
	ViewMsg *msg = GetMsg(userMsgId);
	int		idx = GetMsgIdx(userMsgId);
	UINT	cf_type = 0;

	if (!msg || idx != focusIdx || msg->msg.msg_id != msgIds[idx].msg_id) {
		return	FALSE;
	}

	BOOL is_shift = (::GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
	BOOL is_img = IsImageInClipboard(hWnd, &cf_type);
	BOOL is_txt = IsTextInClipboard(hWnd);

	if (is_img && ((mode == 0 && !is_shift) || mode == 2)) {
		return	PasteImage(msg, cf_type);
	}
	else if (is_txt && (mode == 0 || mode == 1)) {
		PasteTextComment(msg);
	}

	return	TRUE;
}

BOOL TChildView::PasteTextComment(ViewMsg *msg)
{
	Wstr	wstr;

	if (::OpenClipboard(hWnd)) {
		if (HANDLE hClip = ::GetClipboardData(CF_UNICODETEXT)) {
			if (WCHAR *data = (WCHAR *)::GlobalLock(hClip)) {
				wstr = data;
				::GlobalUnlock(hClip);
			}
		}
		::CloseClipboard();
	}
	if (!wstr) {
		return	FALSE;
	}
	int	len = wstr.Len() + 1;
	LocalNewLineToUnixW(wstr.s(), wstr.Buf(), len);

	if (get_linenum(wstr.s()) > 50 || wstr.Len() > 1000) {
		//EditComment(userMsgId, &wstr);
		SetStatusMsg(0, L"Sorry, too long comment");
	}
	else {
		SetComment(userMsgId, wstr, TRUE);
	}
	return	TRUE;
}

BOOL GenNewClipFileName(Cfg *cfg, int64 msg_id, char *fname)
{
	int		id = (int)msgid_to_time(msg_id);
	char	path[MAX_PATH_U8];

	if (!MakeImageFolderName(cfg, path)) {
		return	FALSE;
	}
	strcat(path, "\\");
	int		len = (int)strlen(path);

	for (int i=0; i < 1000; i++) {
		MakeClipFileName(id, i, 2, path + len);
		if (::GetFileAttributesU8(path) == 0xffffffff) {
			strcpy(fname, path + len);
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL TChildView::PasteImage(ViewMsg *msg, UINT cf_type)
{
	BOOL ret = FALSE;

	if (::OpenClipboard(hWnd)) {
		if (HBITMAP hClip = (HBITMAP)::GetClipboardData(CF_BITMAP)) {
			if (VBuf *vbuf = BmpHandleToPngByte(hClip)) {
				ret = InsertImageCore(msg, vbuf);
				delete vbuf;
			}
		}
		::CloseClipboard();
	}
	if (!ret) {
		SetStatusMsg(0, L"Can't paste image");
	}

	return	ret;
}

BOOL TChildView::InsertImageCore(ViewMsg *msg, VBuf *vbuf)
{
	char	fname[MAX_PATH_U8];

	if (!GenNewClipFileName(cfg, msg->msg.msg_id, fname)) {
		return	FALSE;
	}
	if (!SaveImageFile(cfg, fname, vbuf)) {
		return	FALSE;
	}

	LogClip	clip;
	clip.fname = fname;
	msg->msg.clip.push_back(clip);
	msg->msg.flags |= DB_FLAG_CLIP;
	UpdateMsg(msg);
	SetNewCurIdxByMsgId(msg->msg.msg_id);
	ClearMsgCache(msg->msg.msg_id);
	SetScrollPos();

	return	TRUE;
}

BOOL TChildView::InsertImage(int64 msg_id)
{
	BOOL	ret = FALSE;
	char	fname[MAX_PATH_U8] = "";
	char	dir[MAX_PATH_U8] = "";
	OpenFileDlg	dlg(this->parent, OpenFileDlg::OPEN);

	MakeImageFolderName(cfg, dir);

	if (dlg.Exec(fname, sizeof(fname), NULL,
		"Image\0*.png;*.jpg;*.jpeg;*.gif;*.tiff;*.ico;*.bmp;*.wmf;*.wmz;*.emf;*.emz\0\0", dir)) {
		if (Bitmap *bmp = Bitmap::FromFile(Wstr(fname).s())) {
			HBITMAP	hBmp = NULL;
			ViewMsg	*msg = GetMsg(msg_id);

			if (msg && !bmp->GetHBITMAP(Color::White, &hBmp) && hBmp) {
				if (VBuf *vbuf = BmpHandleToPngByte(hBmp)) {
					ret = InsertImageCore(msg, vbuf);
					delete vbuf;
				}
				::DeleteObject(hBmp);
			}
			delete bmp;
		}
	}
	return	ret;
}

BOOL TChildView::SetComment(int64 msg_id, const Wstr &comment, BOOL is_append)
{
	if (ViewMsg	*msg = GetMsg(msg_id)) {
		if (is_append) {
			if (msg->msg.comment) {
				msg->msg.comment += L"\n";
			}
			msg->msg.comment += comment.s();
		}
		else {
			msg->msg.comment = comment;
		}
		UpdateMsg(msg);

		if (msg->dispGenNo == msg->dispGenNo) {
			offsetPix = msg->dispYOff;
		}

		SetNewCurIdxByMsgId(msg_id);
		ClearMsgCache(msg_id);
		int	cy = GetPaintMsgCy(msg_id);
		if (offsetPix + cy > vrc.cy()) {
			offsetPix = vrc.cy() - cy;
		}
		else if (offsetPix + cy < lineCy) {
			offsetPix = 0;
		}
		SetScrollPos();
	}

	return	TRUE;
}

BOOL TChildView::EditComment(int64 msg_id, const Wstr *append)
{
	ViewMsg	*msg = GetMsg(msg_id);
	Wstr	comment;

	if (!msg) {
		return	FALSE;
	}
	Wstr	org = msg->msg.comment;

	if (append) {	// 長文編集時用（予約）
		if (org) {
			org += L"\n";
		}
		org += append->s();
	}

	TMsgEditDlg	dlg(cfg, COMMENT_DIALOG, this);

	if (dlg.Exec(&org, &comment, IDS_COMMENT) == IDOK &&
		msg->msg.comment != comment) {
		return	SetComment(msg_id, comment);
	}
	return	FALSE;
}

int TChildView::IdxToScrollPos(int idx, int pix_off)
{
	int ret = (idx == 0 && pix_off >= 0) ? 0 : ((idx * 1000) + min(pix_off + 500, 999));

//	Debug("IdxToScrollPos(%d idx=%d off=%d pix_off=%d)\n", ret, idx, offsetPix, pix_off);

	return ret;
}

int TChildView::FindNonHiresIdx(int pos, int top_idx, int end_idx, int offset_pix)
{
	int	min_idx = top_idx;
	int	max_idx = end_idx;
	int	idx = 0;
	int	offset_ln = (top_idx > 0) ? msgIds[top_idx-1].totalln : 0;

	pos -= offset_pix;

	while (min_idx <= max_idx) {
		idx = (min_idx + max_idx) / 2;
		int cy = ((msgIds[idx].totalln - offset_ln) * lineCy)
				+ (idx - top_idx) * headNormCy;
		if (pos > cy) {
			min_idx = idx + 1;
		}
		else if (pos < cy) {
			max_idx = idx - 1;
		}
		else {
			break;
		}
	}
//	Debug("FindNonHiresIdx=%d\n", idx);
	return	idx;
}

int TChildView::ScrollPosToIdx(int pos, int *new_offpix)
{
	if (dispFlags & DISP_TITLE_ONLY) {
		if (new_offpix) {
			*new_offpix = 0;
		}
		return	pos / headOnlyCy;
	}
	if (pos < scrHiresTopPix) {
//		Debug("pos < scrHiresTopPix pos=%d topIdx=%d topPix=%d endIdx=%d endPix=%d\n", pos, scrHiresTopIdx, scrHiresTopPix, scrHiresEndIdx, scrHiresEndPix);
		return	FindNonHiresIdx(pos, 0, scrHiresTopIdx, 0);
	}
	if (pos > scrHiresEndPix) {
//		Debug("pos > scrHiresTopPix pos=%d topIdx=%d topPix=%d endIdx=%d endPix=%d\n", pos, scrHiresTopIdx, scrHiresTopPix, scrHiresEndIdx, scrHiresEndPix);
		int idx = FindNonHiresIdx(pos, min(scrHiresEndIdx+1, msgIds.UsedNumInt()-1),
			msgIds.UsedNumInt()-1, scrHiresEndPix);
		if (idx == msgIds.UsedNumInt() -1) {	// 最終要素が表示しきれない場合は末尾に
			int	cy = GetPaintMsgCy(msgIds[idx].msg_id);
			if (cy > vrc.cy()) {
				*new_offpix = vrc.cy() - cy;
			}
		}
		return	idx;
	}

	int		total_pix = scrHiresTopPix;
	for (int i=scrHiresTopIdx; i <= scrHiresEndIdx; i++) {
		int	cy = GetPaintMsgCy(msgIds[i].msg_id);
		if (pos <= total_pix + cy) {
			if (new_offpix) {
				*new_offpix = total_pix - pos;
			}
//			Debug("hres pos=%d topIdx=%d topPix=%d endIdx=%d endPix=%d\n", pos, scrHiresTopIdx, scrHiresTopPix, scrHiresEndIdx, scrHiresEndPix);
			return	i;
		}
		total_pix += cy;
	}
//	Debug("hres2 pos=%d topIdx=%d topPix=%d endIdx=%d endPix=%d\n", pos, scrHiresTopIdx, scrHiresTopPix, scrHiresEndIdx, scrHiresEndPix);

	return	scrHiresEndIdx;
}

int calc_curpos(int idx, int off_pix, int *cy_array, int num)
{
	if (off_pix > 0) {
		for (int i=idx-1; i >= 0; i--) {
			off_pix -= cy_array[i];
			if (off_pix <= 0) {
				int	cy = 0;
				for (int j=0; j < i; j++) {
					cy += cy_array[j];
				}
				return	cy - off_pix;
			}
		}
		return	-off_pix;
	}
	else {
		for (int i=idx; i < num; i++) {
			off_pix += cy_array[i];
			if (off_pix >= 0) {
				int	cy = 0;
				for (int j=0; j <= i; j++) {
					cy += cy_array[j];
				}
				return	cy - off_pix;
			}
		}
		return	off_pix;
	}
}

int TChildView::CalcTotalPix(int idx, int off_pix)
{
	int		hres_num = scrHresReduce ? SCR_HIRES_MIN : SCR_HIRES_MAX;

	if (scrTrackNpos == -1) {
		scrAllRangePix = 0;

		scrHiresTopPix = 0;
		scrHiresEndPix = 0;
		scrHiresTopIdx = 0;
		scrHiresEndIdx = 0;
		scrCurPos = 0;
	}

	int	ids_num = msgIds.UsedNumInt();
	if (ids_num == 0) {
		return	0;
	}

	if (dispFlags & DISP_TITLE_ONLY) {
		scrAllRangePix = ids_num * headOnlyCy + (vrc.cy() - headCy);
		int	diff_idx = off_pix / headOnlyCy;
		int	diff_pix = off_pix % headOnlyCy;
		if (curIdx < diff_idx) {
			scrCurPos = 0;
		}
		else {
			scrCurPos = (curIdx - diff_idx) * headOnlyCy + diff_pix;
		}
		return	scrAllRangePix;
	}
	if (scrTrackNpos >= 0) {
		scrCurPos = scrTrackNpos;
	}
	else {
		scrHiresTopIdx = max(idx - (hres_num/2),   0);
		scrHiresEndIdx = min(scrHiresTopIdx + hres_num -1, ids_num - 1);

		if (scrHiresTopIdx > 0) {
			scrAllRangePix += scrHiresTopIdx * headNormCy;
			scrAllRangePix += msgIds[scrHiresTopIdx - 1].totalln * lineCy;
			scrHiresTopPix = scrAllRangePix;
		}
		int		cy_array[SCR_HIRES_MAX];
		int		num = scrHiresEndIdx - scrHiresTopIdx + 1;
		for (int i=0; i < num; i++) {
			int cy = GetPaintMsgCy(msgIds[i+scrHiresTopIdx].msg_id);
			cy_array[i]     = cy;
			scrAllRangePix += cy;
		}
		scrCurPos = scrHiresTopPix +
			calc_curpos(curIdx - scrHiresTopIdx, off_pix, cy_array, num);

		// 先頭メッセージの先に、空白を許容する場合あり
		if (scrHiresTopIdx == 0 && scrCurPos < 0) {
			scrAllRangePix -= scrCurPos;
			scrCurPos = 0;
		}
		scrHiresEndPix = scrAllRangePix;

		if (scrHiresEndIdx + 1 >= ids_num) {
			// 最終メッセージは、最上段 or メッセージ最下部までスクロール許容
			int	last_cy = GetPaintMsgCy(msgIds[ids_num - 1].msg_id);
			if (last_cy < vrc.cy()) {
				scrAllRangePix += vrc.cy() - last_cy;
			}
		}
		else {
			scrAllRangePix += (ids_num - scrHiresEndIdx - 1) * headNormCy;
			int	total_ln = msgIds[ids_num - 1].totalln - msgIds[scrHiresEndIdx].totalln;
			scrAllRangePix += total_ln * lineCy;
			int last_pix = headNormCy +					// 必ず hres_num以上の要素存在
				(msgIds[ids_num - 1].totalln - msgIds[ids_num - 2].totalln) * lineCy;
			if (last_pix < vrc.cy()) {
				scrAllRangePix += vrc.cy() - last_pix;
			}
		}
	}
//	Debug("calc all=%d cur=%d topIdx=%d topPix=%d endIdx=%d endPix=%d\n", scrAllRangePix, scrCurPos, scrHiresTopIdx, scrHiresTopPix, scrHiresEndIdx, scrHiresEndPix);

	return	scrAllRangePix;
}

int TChildView::SetScrollPos(BOOL is_invalidate)
{
//	Debug("SetScrollPos start tick=%d\n", tick.elaps());
	CalcTotalPix(curIdx, offsetPix);

//	Debug("scrAllRangePix=%d cur=%d\n", scrAllRangePix, scrCurPos);
	SCROLLINFO si = {
		sizeof(si), SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL,
		0,  max(scrAllRangePix, vrc.cy())+1, UINT(vrc.cy()), scrCurPos, 0
	};
	::SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
	//::SetScrollPos(hWnd, SB_VERT, IdxToScrollPos(curIdx, offsetPix), TRUE);

//	Debug("SetScrollPos end tick=%d\n", tick.elaps());
	if (is_invalidate) {
		InvalidateRect(0, 0);
	}

	return	scrAllRangePix;
}

time_t TChildView::GetStatusCheckEpoch()
{
	return	msgid_to_time(beginMsgId);
}

BOOL TChildView::SetStatusCheckEpoch(time_t t)
{
//	t = time(NULL) - 1000000;
	beginMsgId = time_to_msgid(t);
	return	TRUE;
}

BOOL TChildView::SetUser(const Wstr &uid)
{
	if (!logDb || !hWnd) {
		return	FALSE;
	}

	findUser = uid;
	UpdateMsgList();
	UpdateUserCombo();

	BOOL check = (dispFlags & DISP_REV) ? TRUE : FALSE;
	SetFindedIdx(check  ? TOP_IDX : LAST_IDX);
	return	TRUE;
}

BOOL TChildView::ResetCache()
{
	if (!logDb || !hWnd) {
		return	FALSE;
	}

	ClearMsgCacheAll();
	UpdateMsgList(UF_NO_FINDMOVE|UF_NO_INITSEL);
	UpdateUserCombo();
	InvalidateRect(0, 0);
	return	TRUE;
}

BOOL TChildView::RequestResetCache()
{
	return	::PostMessage(GetMainWnd(), WM_LOGVIEW_RESETCACHE, 0, (LPARAM)parent);
}

BOOL TChildView::ClearFindURL()
{
	while (TWstrObj *obj = cfg->LvFindList.TopObj()) {
		cfg->LvFindList.DelObj(obj);
		delete obj;
	}
	parentView->bodyCombo.SendMessage(CB_RESETCONTENT, 0, 0);

	findBodyOrg = findBody = "";
	UpdateFindComboList();

	return	TRUE;
}

BOOL TChildView::LastView()
{
	if (!logDb || !hWnd) {
		return	FALSE;
	}
	BOOL cur_flags = dispFlags;
	dispFlags &= ~(DISP_NARROW|DISP_UNOPENR_ONLY|DISP_FILE_ONLY|
					DISP_CLIP_ONLY|DISP_FAV_ONLY|DISP_MARK_ONLY);

	if (findUser &&
		(!logDb->SelectUnOpenHosts(beginMsgId, &hostList) ||
		hostList.UsedNum() != 1 || hostList[0].uid != findUser)) {
		ToggleFindUser(findUser);
	}
	else if (cur_flags != dispFlags) {
		UpdateMsgList(UF_NO_FINDMOVE);
	}

	SetFindedIdx(LAST_IDX);
	return	TRUE;
}

