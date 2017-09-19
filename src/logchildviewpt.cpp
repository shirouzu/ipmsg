static char *logchildviewpt_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logchildviewpt.cpp	Ver4.60";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child Pane for Paint
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

#define CLIPSIZE		200
#define CLIPSIZE_MIN	30
#define CLIP_FRAME		2

const WCHAR *wcschr_next(const WCHAR *s, WCHAR ch) {
	const WCHAR *p = s ? wcschr(s, ch) : NULL;

	return	p ? (p+1) : NULL;
}

int wcs_lncnt(const WCHAR *s, const WCHAR *lim_s)
{
	int			cnt = 1;
	const WCHAR	*p = s;

	while ((p = wcschr_next(p, '\n')) && p < lim_s) {
		cnt++;
	}

	return	cnt;
}

int make_files_str(ViewMsg *msg, WCHAR *wbuf, int wsize)
{
	int		len = 0;

	for (int i=0; i < msg->msg.files.size(); i++) {
		if (i > 0) {
			len += wcsncpyz(wbuf + len, L", ", wsize-len);
			if (len >= wsize-1) break;
		}
		len += wcsncpyz(wbuf + len, msg->msg.files[i].s(), wsize-len);
		if (len >= wsize-1) break;
	}
	return	len;
}


/* ===============================================================
                          ChildView
   =============================================================== */
void PaintRect(HDC hDc, HPEN hPen, HBRUSH hBrush, TRect &rc, int round_w=0, int round_h=0)
{
	HGDIOBJ svBrs = ::SelectObject(hDc, hBrush);
	HGDIOBJ svPen = ::SelectObject(hDc, hPen);

	if (round_w == 0 && round_h == 0) {
		::Rectangle(hDc, rc.x(), rc.y(), rc.right, rc.bottom);
	}
	else {
		::RoundRect(hDc, rc.x(), rc.y(), rc.right, rc.bottom, round_w, round_h);
	}

	::SelectObject(hDc, svBrs);
	::SelectObject(hDc, svPen);
}

BOOL TChildView::EvPaint()
{
	PAINTSTRUCT ps;
	HDC	hDc = BeginPaint(hWnd, &ps);

	// メイン描画
	PaintMemBmp();
	::BitBlt(hDc, CVW_GAPX, CVW_GAPY, vrc.cx(), vrc.cy(), hMemDc, 0, 0, SRCCOPY);

	// 枠線描画 ... ここもダブルバッファにしても良いが
	if (CVW_GAPY > 0) {
		HGDIOBJ hBrsSv  = ::SelectObject(hDc, hWhiteBrush);
		HGDIOBJ hPenSv = ::SelectObject(hDc, hWhitePen);

	//	::Rectangle(hDc, 0, 0, crc.cx(), CVW_GAP);
		::Rectangle(hDc, 0, CVW_GAPY, CVW_GAPX, crc.cy());
		::Rectangle(hDc, crc.cx() - CVW_GAPX, CVW_GAPY, crc.cx(), crc.cy());

		::SelectObject(hDc, hBrsSv);
		::SelectObject(hDc, hPenSv);
	}

	EndPaint(hWnd, &ps);

	return	TRUE;
}

void PaintLine(HDC hDc, HPEN hPen, int x, int y, int x2, int y2)
{
	HGDIOBJ	hOrg = ::SelectObject(hDc, hPen);

	::MoveToEx(hDc, x, y, NULL);
	::LineTo(hDc, x2, y2);

	SelectObject(hDc, hOrg);
}

BOOL TChildView::PaintPushBox(TRect rc, BoxType type, int round, BOOL is_direct)
{
	HDC 	hDc = hMemDc;

	if (is_direct) {
		hDc = ::GetDC(hWnd);
		MemToClinetRect(&rc);
	}

	HGDIOBJ	hOrg = ::SelectObject(hDc,
		type == NORMAL_BOX ? hShinePen  :
		type == SELECT_BOX ? hShadowPen : hGrayPen);

	LONG	tx = rc.left;
	LONG	ty = rc.top;
	LONG	bx = rc.right;
	LONG	by = rc.bottom;

	::MoveToEx(hDc, bx, ty + round, NULL);
	::AngleArc(hDc, bx - round, ty + round, round, 0, 90);
	::AngleArc(hDc, tx + round, ty + round, round, 90, 90);
	::AngleArc(hDc, tx + round, by - round, round, 180, 45);

//	::MoveToEx(hDc, rc.right-1, rc.top, NULL);
//	::LineTo(hDc, rc.left, rc.top);
//	::LineTo(hDc, rc.left, rc.bottom-1);

	::SelectObject(hDc,
		type == NORMAL_BOX ? hShadowPen :
		type == SELECT_BOX ? hShinePen  : hGrayPen);

	::AngleArc(hDc, tx + round, by - round, round, 225, 45);
	::AngleArc(hDc, bx - round, by - round, round, 270, 90);
	::AngleArc(hDc, bx - round, ty + round, round, 0, 45);

//	::LineTo(hDc, rc.right-1, rc.bottom-1);
//	::LineTo(hDc, rc.right-1, rc.top);

	::SelectObject(hDc, hOrg);

	if (is_direct) {
		::ReleaseDC(hWnd, hDc);
	}

	return	TRUE;
}
//  10:38 AM    (... Mar 31 at 10:38 AM)
//  Mar-30 8:08 AM
//  Fri Mar 25 17:01:46 2016
//  Fri Mar 25 17:01 2016
//  Mar-25 17:01 Fri
//  Fri Mar-25 17:01
//  Fri 17:01 Mar/01
//  Fri 17:01 Mar-25 2016
//  at 17:01 03/24
//  17:01 03/24(Fri)
//  2016/01/01 12:34
//  12:13 Fri 01
//  12:13 Fri 01/Apr
//  12:13 Fri 01/Apr/2016
//  12:34 2016/04/01
//  2016/04/01 12:34
//
// 12:13 Fri 01
// 12:13 Fri 01/Apr
// 12:13 Fri 01/Apr/2016

int TChildView::PaintDate(ViewMsg *msg, int *_off_x, int top_y)
{
	int		&off_x = *_off_x;
	SIZE	sz = {};
	WCHAR	date[64];
	int		dlen = MakeDateStrEx(msg->msg.date, date, &curTime);
	int		dtop_y = top_y + (fontLdCy ? fontLdCy/4 : 1);

	if ((msg->msg.flags & (DB_FLAG_SIGNERR))) {
		dlen += wcscpyz(date + dlen, L" ?");
	}
	else if ((msg->msg.flags & (DB_FLAG_RSA|DB_FLAG_RSA2)) == 0) {
		dlen += wcscpyz(date + dlen, L" -");
	}

	HGDIOBJ	svObj = ::SelectObject(hMemDc, hMiniFont);

	::GetTextExtentPoint32W(hMemDc, date, dlen, &sz);
	TRect	rc(off_x, dtop_y, sz.cx+1, headCy);
	DrawTextW(hMemDc, date, dlen, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
	off_x += sz.cx + ITEM_GAP;

	itemMgr.SetItem(Item::DATE, &rc);

	::SelectObject(hMemDc, svObj);
	return	off_x;
}

int TChildView::PaintFromTo(ViewMsg *msg, int *_off_x, int top_y, BOOL is_rev)
{
	int		&off_x = *_off_x;
	TRect	irc(off_x, top_y + OffPix(headCy, FROMTO_CY), FROMTO_CX, FROMTO_CY);
	BOOL	is_from = msg->msg.IsRecv();

	if (is_rev) {
		is_from = !is_from;
	}

	PaintBmpIcon((!is_rev && lastReplyBtnId == msg->msg.msg_id)
		? hReplyMsgBmp : is_from ? hFromMsgBmp : (is_rev ? hToMsg2Bmp : hToMsgBmp), irc);

	TRect	rc(off_x, top_y, FROMTO_CX, headCy);
	if (Item *item = itemMgr.SetItem(is_rev ? Item::USERMULTI : Item::FROMTO, &rc)) {
		item->drawRc = irc;
		item->is_from = is_from;
	}

	off_x += irc.cx();

	return	off_x;
}


#define CROPBTN_SIZE (CROPUSER_CX + 5)


int TChildView::PaintUsers(ViewMsg *msg, int top_y, int *_off_x, int lim_x)
{
	int		&off_x = *_off_x;
	int		i;
	BOOL	is_from = msg->msg.IsRecv();
	BOOL	need_crop = FALSE;
	TRect	rc;
	int		utop_y = top_y + 0;

	if (msg->msg.flags & DB_FLAG_FILE) lim_x -= FILEMIN_CX;
	if (msg->msg.flags & DB_FLAG_CLIP) lim_x -= CLIPMIN_CX;
	lim_x -= FAVMIN_CX + IMENUBMP_CX;	// fav/item枠は常時表示

	// users
	HGDIOBJ	svObj = ::SelectObject(hMemDc, hBoldFont);
	for (i=0; i < msg->msg.host.size(); i++) {
		LogHost	&host = msg->msg.host[i];
		Wstr	&nick = host.nick;
		BOOL	is_selected = findUser && host.uid == findUser;
		BOOL	is_unopen = (msg->msg.msg_id >= beginMsgId &&
			(host.flags & DB_FLAGMH_UNOPEN)) ? TRUE : FALSE;

		if (msg->msg.IsRecv() && i == 1) {
			off_x += MULTI_GAP;
			PaintFromTo(msg, &off_x, top_y, TRUE);
		}

		SIZE	sz = {};
		::GetTextExtentPoint32W(hMemDc, nick.s(), nick.Len(), &sz);

		// cx領域が足りないときはcrop button化
		int		btn_cy = sz.cy + ITEM_GAP + (fontLdCy ? -1 : 1);
		int		unopen_cx = is_unopen ? (UNOPEN_CX + MINI_GAP) : 0;
		int		usr_remain = int(msg->msg.host.size() - i - 1);

		rc.Init(off_x + headDiff2, utop_y + OffPix(headCy, btn_cy), sz.cx + headDiff, btn_cy);
		need_crop = (rc.left + sz.cx/3 + unopen_cx + CROPBTN_SIZE * (usr_remain ? 2 : 1))
			>= lim_x ? TRUE : FALSE;

		if (need_crop) {
			TRect	crc(rc.left, utop_y + OffPix(headCy, CROPUSER_CY), CROPUSER_CX, CROPUSER_CY);
			PaintBmpIcon(hCropUserBmp, crc);

			rc.set_cx(CROPUSER_CX);
		} else {
			int reduce = lim_x - (rc.right + unopen_cx + CROPBTN_SIZE * (usr_remain ? 2 : 1));
			if (reduce < 0) {
				rc.right += reduce;
			}
			TRect	rc2(rc.x(), utop_y, rc.cx(), headCy + (fontLdCy ? fontLdCy/2 : 1));
			DrawTextW(hMemDc, nick.Buf(), nick.Len(), &rc2,
				(reduce >= 0 ? DT_CENTER : 0)|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

			if (is_unopen) {
				TRect	urc(rc.right + MINI_GAP, rc.top + 1, UNOPEN_CX, UNOPEN_CY);
				PaintBmpIcon(is_from ? hUnOpenRBmp : hUnOpenSBmp, urc);

				off_x += MINI_GAP + UNOPEN_CX;

				TRect irc = urc;
				irc.Inflate(1, 2);
				if (Item *item = itemMgr.SetItem(Item::UNOPEN, &irc)) {
					item->user_idx = i;
					item->drawRc = urc;
				}
			}
		}
		rc.left -= 1;
	//	rc.top  -= 1;
		PaintPushBox(rc, is_selected ? SELECT_BOX : NORMAL_BOX, 4);
		off_x += rc.cx() + headDiff2;

		if (Item *item = itemMgr.SetItem(need_crop ? Item::CROPUSER : Item::USER, &rc)) {
			item->user_idx = i;
		}

		if (need_crop) break;
	}

	// 複数ユーザ（受信）での「…」表示
	if (is_from && !need_crop && (msg->msg.flags & DB_FLAG_MULTI) && msg->msg.host.size() == 1) {

		off_x += MULTI_GAP;
		PaintFromTo(msg, &off_x, top_y, TRUE);
		off_x += MULTI_GAP;

		TRect	crc(off_x + MINI_GAP, utop_y + OffPix(headCy, CROPUSER_CY),
			CROPUSER_CX, CROPUSER_CY);
		PaintBmpIcon(hCropUserBmp, crc);

		TRect	rc2(off_x + MINI_GAP-1, rc.top, CROPUSER_CX + MINI_GAP, rc.cy());
		FrameRect(hMemDc, &rc2, hMidGrayBrush);
		itemMgr.SetItem(Item::USERMULOLD, &rc2);
		off_x += CROPUSER_CX + MINI_GAP*3;
	}

	::SelectObject(hMemDc, svObj);

	return	i;
}

int TChildView::PaintBodyInHead(ViewMsg *msg, int top_y, int *_off_x, int *_lim_x)
{
	int		&off_x = *_off_x;
	int		&lim_x = *_lim_x;
	int		diff_x = lim_x - off_x - ITEM_GAP;

	if (diff_x < ITEM_GAP + 20) {
		return 0;
	}

	// 背景ボックス描画
	TRect	brc(off_x+MINI_GAP, top_y+MINI_GAP-1, diff_x, headCy-MINI_GAP*2+2);
	PaintRect(hMemDc, hShinePen, hShineBrush, brc);

	off_x  += ITEM_GAP;
	diff_x -= ITEM_GAP * 2;

	HGDIOBJ	svObj = ::SelectObject(hMemDc, hMiniFont);

	Wstr	w(msg->msg.body.s(), maxLineChars);
	ReplaceCharW(w.Buf(), '\n', ' ', maxLineChars);
	TRect	rc(off_x + LINE_GAP, top_y + OffPix(headCy, fontCy), diff_x, fontCy);

	::DrawTextW(hMemDc, w.s(), -1, &rc,
		DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);

	::SelectObject(hMemDc, svObj);

	return	rc.cy();
}

int TChildView::PaintFileInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x)
{
	int		&lim_x = *_lim_x;

	lim_x -= ITEM_GAP;

	SIZE	sz = {};
	TRect	rc(lim_x - FILEBMP_CX, top_y + OffPix(headCy, FILEBMP_CY), FILEBMP_CX, FILEBMP_CY);

	BOOL is_title = (dispFlags & DISP_TITLE_ONLY) ? TRUE : FALSE;

	WCHAR	wbuf[128];
	int	len = make_files_str(msg, wbuf, wsizeof(wbuf));

	HGDIOBJ	svObj = ::SelectObject(hMemDc, hMiniFont);
	::GetTextExtentPoint32W(hMemDc, wbuf, len, &sz);
	sz.cx = min(sz.cx, lim_x - off_x - FILEBMP_CX - USR_MINCX);
	sz.cx = min(sz.cx, is_title ? FILEMID_CX : FILEMAX_CX);
	rc.set_x(rc.x() - sz.cx - ITEM_GAP - (msg->unOpenR ? 5 : 0));

	if (msg->unOpenR) {
		TRect	rc2(rc);
		rc2.Inflate(2, 2);
		PaintRect(hMemDc, hMarkPen, hMarkBrush, rc2);
	}

	TRect	frc(rc.x() + FILEBMP_CX + ITEM_GAP, top_y + OffPix(headCy, fontCy - fontLdCy/3),
		sz.cx, fontCy);
	::DrawTextW(hMemDc, wbuf, len, &frc,
		DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
	::SelectObject(hMemDc, svObj);

	// アイコン描画
	TRect	irc(rc.x(), rc.y(), FILEBMP_CX, FILEBMP_CY);
	PaintBmpIconCore(hMemDc, hFileBmp, irc);

	if (msg->unOpenR) {
		rc.Inflate(4, 4);
		PaintPushBox(rc, NORMAL_BOX);
	}

	if (Item *item = itemMgr.SetItem(msg->unOpenR ? Item::UNOPENFILE : Item::FILE, &rc)) {
		item->drawRc   = frc;
	}

	lim_x -= rc.cx();

	return	rc.cy();
}

int TChildView::PaintClipInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x)
{
	int		&lim_x = *_lim_x;

	lim_x -= ITEM_GAP + CLIPBMP_CX;

	TRect	irc(lim_x, top_y +  OffPix(headCy, CLIPBMP_CY), CLIPBMP_CX, CLIPBMP_CY);

	PaintBmpIcon(hClipBmp, irc);

	TRect	rc(lim_x, top_y, CLIPBMP_CX, headCy);

	if (Item *item = itemMgr.SetItem(Item::CLIP, &rc)) {
		item->drawRc   = irc;
		item->clip_idx = 0;
	}

	return	rc.cy();
}

BOOL TChildView::IsScrollOut(int top_y)
{
	return	(top_y <= -headDiff2) && (dispFlags & DISP_TITLE_ONLY) == 0;
}

int TChildView::CalcScrOutYOff(ViewMsg *msg)
{
	if (msg->dispYOff >= 0 || (dispFlags & DISP_TITLE_ONLY)) {
		return	msg->dispYOff;
	}

	int	lim_y = msg->dispYOff + msg->sz.cy - (lineCy + headCy);

	if (lim_y <= -headDiff2) {
		return	lim_y;
	}

	return	max(msg->dispYOff, -headDiff2);
}

int TChildView::PaintBmpIconCore(HDC hDc, HBITMAP hBmp, TRect rc)
{
	HGDIOBJ	oldBmp = ::SelectObject(hTmpDc, hBmp);
	BOOL	is_scrout = IsScrollOut(rc.top - OffPix(headCy, rc.cy()));

	if (!is_scrout) {
		PaintRect(hDc, hGrayPen,     hGrayBrush,     rc);
	} else {
		PaintRect(hDc, hMidShinePen, hMidShineBrush, rc);
	}

	::TransparentBlt(hDc, rc.left, rc.top, rc.cx(), rc.cy(), hTmpDc, 0, 0, rc.cx(), rc.cy(),
		TRANS2_RGB);

	::SelectObject(hTmpDc, oldBmp);

	if (is_scrout) {
		PaintLine(hDc, hGrayPen, rc.left, 0, rc.right, 0);
	}

	return	rc.cy();
}

int TChildView::PaintBmpIcon(HBITMAP hBmp, TRect rc, BOOL is_direct)
{
	HDC hDc = hMemDc;

	if (is_direct) {
		hDc = ::GetDC(hWnd);
		MemToClinetRect(&rc);
	}

	int ret = PaintBmpIconCore(hDc, hBmp, rc);

	if (is_direct) {
		::ReleaseDC(hWnd, hDc);
	}

	return	ret;
}

int CalcLineCx(HDC hDc, const WCHAR *s)
{
	int		cx = 0;

	for (const WCHAR *p=s; p && *p; ) {
		const WCHAR *next_p = wcschr(p, '\n');
		if (next_p) {
			next_p++;
		}
		int	len = next_p ? int(next_p - p) : -1;
		TRect	rc;
		::DrawTextW(hDc, p, len, &rc, DT_LEFT|DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
		cx = max(cx, rc.cx());
		p = next_p;
	}
	return	cx;
}

BOOL CalcTextBoxSize(HDC hDc, const WCHAR *s, int len, TSize *sz)
{
	int		max_cx = CalcLineCx(hDc, s);
	TRect	rc(0, 0, min(max_cx, sz->cx), sz->cy);

	::DrawTextW(hDc, s, len, &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX|DT_WORDBREAK);

	sz->cx = rc.cx();
	sz->cy = rc.cy();

	return	TRUE;
}

int TChildView::PaintComment(int64 msg_id, TRect rc, BOOL is_direct)
{
	HDC		hDc  = hMemDc;
	ViewMsg *msg = GetMsg(msg_id);

	if (!msg) {
		return	0;
	}
	Wstr	&comment = msg->msg.comment;

	if (is_direct) {
		hDc = ::GetDC(hWnd);
		MemToClinetRect(&rc);
	}
	int len = comment.Len();
	if (len > 0 && comment[len-1] == '\n') {
		len--;
	}

	int		svBkm = ::SetBkMode(hDc, TRANSPARENT);
	HGDIOBJ	svObj = ::SelectObject(hDc, hMiniFont);
	TSize	sz(vrc.cx() - CMNT_GAP*3, vrc.cy() - CMNT_GAP*2);

	CalcTextBoxSize(hDc, comment.s(), len, &sz);
	rc.left -= sz.cx + rc.cx()/2;
	rc.set_cx(sz.cx);
	rc.top = (rc.top + rc.bottom) / 2;
	rc.set_cy(sz.cy);
	rc.Inflate(CMNT_GAP, CMNT_GAP);

	// 負の座標の場合は正の座標に移動
	if (rc.left < 0) {
		rc.right += -rc.left;
		rc.left = 0;
	}
	if (rc.top < 0) {
		rc.bottom += -rc.top;
		rc.top = 0;
	}

	// ビューの大きさを上限に
	if (rc.cx() > vrc.cx()) {
		rc.set_cx(vrc.cx());
	}
	if (rc.right > vrc.right) {
		rc.left -= rc.right - vrc.right;
		rc.right = vrc.right;
	}
	if (rc.cy() > vrc.cy()) {
		rc.set_cy(vrc.cy());
	}
	if (rc.bottom > vrc.bottom) {
		rc.top   -= rc.bottom - vrc.bottom;
		rc.bottom = vrc.bottom;
	}

	// 描画
	PaintRect(hDc, hMarkPen, hCmntBrush, rc, 5, 5);
	rc.Inflate(-CMNT_GAP, -CMNT_GAP);
	::DrawTextW(hDc, comment.s(), len, &rc, DT_LEFT|DT_NOPREFIX|DT_WORDBREAK|DT_END_ELLIPSIS);

	::SetBkMode(hDc, svBkm);
	::SelectObject(hDc, svObj);
	if (is_direct) {
		::ReleaseDC(hWnd, hDc);
	}

	return	rc.cy();
}

int TChildView::PaintCommentInBody(ViewMsg *msg, int top_y)
{
	HDC		hDc  = hMemDc;
	Wstr	&comment = msg->msg.comment;
	int		len = comment.Len();

	if (len > 0 && comment[len-1] == '\n') {
		len--;
	}
	top_y += CMNT_GAP;

	int		svBkm = ::SetBkMode(hDc, TRANSPARENT);
	HGDIOBJ	svObj = ::SelectObject(hDc, hMiniFont);
	TSize	sz(vrc.cx() - CMNT_GAP*3, vrc.cy() - CMNT_GAP*2);

	CalcTextBoxSize(hDc, comment.s(), len, &sz);

	TRect	rc(CMNT_GAP*2, top_y, sz.cx, sz.cy);
	rc.Inflate(CMNT_GAP, CMNT_GAP);

//	int		diff = ((vrc.cx() - rc.cx()) / 2) - rc.left;
//	rc.left  += diff;
//	rc.right += diff;

	// 描画
	PaintRect(hMemDc, hMarkPen, hCmntBrush, rc, 5, 5);

	if (IsRealDraw()) {
		itemMgr.SetItem(Item::CMNTBODY, &rc);

	// 余白は body扱い
		TRect	remain_rc(rc.right, rc.top, vrc.cx() - rc.right, rc.cy());
		if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &remain_rc)) {
			item->body_pos = msg->msg.body.Len();
			item->body_len = 0;
		}
	}

	rc.Inflate(-CMNT_GAP, -CMNT_GAP);

	::DrawTextW(hMemDc, comment.s(), len, &rc, DT_LEFT|DT_NOPREFIX|DT_WORDBREAK|DT_END_ELLIPSIS);

	::SetBkMode(hMemDc, svBkm);
	::SelectObject(hMemDc, svObj);

	return	rc.cy() + ITEM_GAP + CMNT_GAP;
}

#define FOLD_BTN_CY	(FOLD_CY * 2)

int TChildView::PaintFoldBtn(ViewMsg *msg, int top_y, int cnt, int body_btm)
{
	TSize	sz(min(vrc.cx(), FOLD_CX * 10), FOLD_BTN_CY);
	TRect	rc((vrc.cx() - sz.cx)/2, top_y + FOLD_GAP, sz.cx, sz.cy);
	int		bottom_y = top_y + FOLD_GAP + FOLD_BTN_CY + ITEM_GAP;

	if (IsRealDraw()) {
		HDC		hDc  = hMemDc;

		if (msg->unOpenR && bottom_y > body_btm && !IsFold(msg)) {
			PaintUnOpenBox(0, body_btm, msg->sz.cx, bottom_y - body_btm);
		}
		PaintRect(hMemDc, hGrayPen, hShineBrush, rc, 5, 5);

		if (Item *item = itemMgr.SetItem(Item::FOLDBTN, &rc)) {
			item->drawRc = rc;
			item->drawRc.bottom = bottom_y;	// 描画末尾設定（ギャップ込）= MSG実描画 bottom
		}

		HGDIOBJ	svObj = ::SelectObject(hTmpDc, IsFold(msg) ? hFoldFBmp : hFoldTBmp);
		int	x = rc.left + OffPix(rc.cx(), FOLD_CX);
		int	y = rc.top + OffPix(rc.cy(), FOLD_CY);
		::TransparentBlt(hMemDc, x, y, FOLD_CX, FOLD_CY, hTmpDc, 0, 0, FOLD_CX, FOLD_CY,
			TRANS_RGB);
		::SelectObject(hTmpDc, svObj);

//		int		svBkm = ::SetBkMode(hDc, TRANSPARENT);
//		svObj = ::SelectObject(hDc, hTinyFont);
//		TRect	rc2(x+FOLD_CX+ITEM_GAP, top_y, sz.cx, sz.cy);
//
//		::DrawTextW(hMemDc, FmtW(L"(%d)", cnt), -1, &rc2,
//			DT_LEFT|DT_NOPREFIX|DT_VCENTER|DT_SINGLELINE);
//
//		::SetBkMode(hMemDc, svBkm);
//		::SelectObject(hMemDc, svObj);
	}

	return	rc.cy() + ITEM_GAP;
}

int TChildView::PaintCommentInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x)
{
	int		&lim_x = *_lim_x;

	lim_x -= ITEM_GAP + CMNTBMP_CX;
	TRect	irc(lim_x, top_y + OffPix(headCy, CMNTBMP_CY), CMNTBMP_CX, CMNTBMP_CY);

	PaintBmpIcon(hCmntBmp, irc);

	TRect	rc(lim_x, top_y, CMNTBMP_CX, headCy);

	if (Item *item = itemMgr.SetItem(Item::CMNT, &rc)) {
		item->drawRc = irc;
	}

	return	rc.cy();
}

int TChildView::PaintFavInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x)
{
	int		&lim_x = *_lim_x;

	lim_x -= ITEM_GAP + FAVBMP_CX;
	TRect	irc(lim_x, top_y + OffPix(headCy, FAVBMP_CY), FAVBMP_CX, FAVBMP_CY);

	PaintBmpIcon((msg->msg.flags & DB_FLAG_FAV) ? hFavBmp : hFavRevBmp, irc);

	TRect	rc(lim_x, top_y, FAVBMP_CX, headCy);

	if (Item *item = itemMgr.SetItem(Item::FAV, &rc)) {
		item->drawRc = irc;
	}

	return	rc.cy();
}

int TChildView::PaintImenuInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x)
{
	int		&lim_x = *_lim_x;

	lim_x -= ITEM_GAP + IMENUBMP_CX;
	TRect	irc(lim_x, top_y + OffPix(headCy, IMENUBMP_CY), IMENUBMP_CX, IMENUBMP_CY);

	PaintBmpIcon(hImenuMidBmp, irc);

	TRect	rc(lim_x, top_y, IMENUBMP_CX, headCy);

	if (Item *item = itemMgr.SetItem(Item::IMENU, &rc)) {
		item->drawRc = irc;
	}

	return	rc.cy();
}

int TChildView::PaintReplyInHead(ViewMsg *msg, int top_y, int off_x, int *_lim_x)
{
	int		&lim_x = *_lim_x;

	lim_x -= ITEM_GAP + REPLYBMP_CX;
	TRect	irc(lim_x, top_y + OffPix(headCy, REPLYBMP_CY), REPLYBMP_CX, REPLYBMP_CY);

	PaintBmpIcon(lastReplyBtnId == msg->msg.msg_id ? hReplyBmp : hReplyHideBmp, irc);

	TRect	rc(lim_x, top_y, REPLYBMP_CX, headCy);

	if (Item *item = itemMgr.SetItem(Item::REPLY, &rc)) {
		item->drawRc = irc;
	}

	return	rc.cy();
}

int TChildView::PaintHeadBox(ViewMsg *msg, int top_y)
{
	BOOL	is_scrout = IsScrollOut(top_y);
	int		y  = max(0, top_y);
	int		cy = headCy - (y - top_y) - (is_scrout ? 1 : 0);

	TRect	rc(0, y, vrc.cx(), cy);

	PaintRect(hMemDc, hGrayPen, is_scrout ? hMidShineBrush : hGrayBrush, rc);

	if (is_scrout) {
		PaintLine(hMemDc, hMidShinePen, rc.left, rc.bottom-1, rc.right, rc.bottom-1);
	}
	else {
		PaintPushBox(rc, NORMAL_BOX);
	}

	itemMgr.SetItem(Item::HEADBOX, &rc);

	return	headCy;
}

int TChildView::PaintHead(ViewMsg *msg, int top_y)
{
	BOOL	is_scrout = IsScrollOut(top_y);

	if (!IsRealDraw()) {
		return	headCy;
	}

	// 背景BOX
	PaintHeadBox(msg, top_y);

	int	off_x = headDiff2 + FOCUS_WIDTH;
	int	lim_x = vrc.cx();

	// 日付（右）
	PaintDate(msg, &off_x, top_y);

	// FromTo（左）
	PaintFromTo(msg, &off_x, top_y);

	// ユーザボタン（左）
	PaintUsers(msg, top_y, &off_x, lim_x);

	// item menu画像（＋右）
	PaintImenuInHead(msg, top_y, off_x, &lim_x);

	// Fav画像（＋右）
	PaintFavInHead(msg, top_y, off_x, &lim_x);

	// コメント（＋右）
	if (msg->msg.comment) {
		PaintCommentInHead(msg, top_y, off_x, &lim_x);
	}

	// 貼付画像（＋右）
	if (dispFlags & DISP_TITLE_ONLY) {
		if (msg->msg.flags & DB_FLAG_CLIP) {
			PaintClipInHead(msg, top_y, off_x, &lim_x);
		}
	}

	// 添付（＋右）
	if (msg->msg.flags & DB_FLAG_FILE) {
		PaintFileInHead(msg, top_y, off_x, &lim_x);
	}

//	PaintReplyInHead(msg, top_y, off_x, &lim_x);

	if (dispFlags & DISP_TITLE_ONLY) {	// タイトル行のみの場合は本文を一部表示
		PaintBodyInHead(msg, top_y, &off_x, &lim_x);
	}
	else {
		if (is_scrout) {
			PaintLine(hMemDc, hGrayPen, 0, 0, vrc.cx(), 0);
		}
	}

	return	headCy;
}

int TChildView::PaintFullClip(int64 msg_id, LogClip *clip, int top_y)
{
	WCHAR	clip_path[MAX_PATH];
	MakeClipPathW(clip_path, cfg, clip->fname.s());
	Image	img(clip_path);

	int	max_cx = vrc.cx() - (CLIP_FRAME * 2);
	int	max_cy = vrc.cy() - (CLIP_FRAME * 2);
	int cx = img.GetWidth();
	int cy = img.GetHeight();

	if (cx == 0 || cy == 0) {
		return	0;
	}
	if (cx > 0 && cy > 0 && (clip->sz.cx != cx || clip->sz.cy != cy)) {
		clip->sz.cx = cx;
		clip->sz.cy = cy;
		logDb->UpdateClip(msg_id, clip);
		//RequestResetCache();
	}

	HDC	hDc = ::GetDC(hWnd);
	Graphics g(hDc);

	if (cx > max_cx) {
		float ratio = (float)max_cx / cx;
		cx = max_cx;
		cy = lround(ratio * cy);
	}
	if (cy > max_cy) {
		float ratio = (float)max_cy / cy;
		cx = lround(ratio * cx);
		cy = max_cy;
	}
	if (top_y + cy > max_cy) top_y = max_cy - cy;

	int		diff_x = max_cx - cx + CLIP_FRAME;
	Rect	rc(diff_x, top_y, cx, cy);

	g.DrawImage(&img, rc);

	Pen		pen(Color(255, 255, 128, 128), CLIP_FRAME);
	g.DrawRectangle(&pen, rc);

	::ReleaseDC(hWnd, hDc);

	return	rc.Height;
}

int TChildView::PaintFullFiles(ViewMsg *msg, TRect rc)
{
	WCHAR	buf[MAX_BUF];
	int		len = make_files_str(msg, buf, wsizeof(buf));

	HDC	hDc = ::GetDC(hWnd);
	HGDIOBJ	svObj = ::SelectObject(hDc, hMiniFont);
	int		svBkm = ::SetBkMode(hDc, TRANSPARENT);

	rc.set_cx(vrc.cx() - ITEM_GAP);
	::DrawTextW(hDc, buf, len, &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
	int	diff_x = rc.right - (vrc.right - ITEM_GAP);
	if (diff_x > 0) {
		rc.left  -= diff_x;
		rc.right -= diff_x;
	}
	::DrawTextW(hDc, buf, len, &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX);
	int	diff_y = rc.bottom - (vrc.bottom - ITEM_GAP);
	if (diff_y > 0) {
		rc.top    -= diff_y;
		rc.bottom -= diff_y;
	}
	Graphics g(hDc);
	SolidBrush	brush(Color(240, 240, 240));
	Pen			pen(Color(255, 255, 128, 128), CLIP_FRAME);
	Rect		rect(rc.x()-CLIP_FRAME, rc.y()-CLIP_FRAME, rc.cx()+CLIP_FRAME*2, rc.cy()+CLIP_FRAME*2);
	g.FillRectangle(&brush, rect);
	g.DrawRectangle(&pen, rect);

	::DrawTextW(hDc, buf, len, &rc, DT_LEFT|DT_NOPREFIX|DT_END_ELLIPSIS);

	::SelectObject(hDc, svObj);
	::SetBkMode(hDc, svBkm);
	::ReleaseDC(hWnd, hDc);

	return	rc.cy();
}

static int PaintThumbCore(TChildView *view, int cx, int cy, int clip_x, int clip_y, int width,
	Rect *_rc, int *_top_y, int *_diff_x_min, HDC hTmpDc=NULL, HBITMAP hNoClipBmp=NULL,
	Graphics *g=NULL, Image *img=NULL)
{
	int		&top_y = *_top_y;
	int		&diff_x_min = *_diff_x_min;
	Rect	&rc = *_rc;
	BOOL	use_dummy = FALSE;

	if (cx == 0 || cy == 0) {
		cx = NOCLIP_CX;
		cy = NOCLIP_CY;
		use_dummy = TRUE;
	}

	if (cx > clip_x) {
		float ratio = (float)clip_x / cx;
		cx = clip_x;
		cy = lround(ratio * cy);
	}
	if (cy > CLIPSIZE) {
		float ratio = (float)CLIPSIZE / cy;
		cx = lround(ratio * cx);
		cy = CLIPSIZE;
	}

	int	diff_x = clip_x - cx;
	rc.X = width + diff_x + BODY_GAP - 1;
	rc.Y = top_y + clip_y;
	rc.Width  = cx;
	rc.Height = cy;
	diff_x_min = min(diff_x_min, diff_x);

	if (g && img) {
		if (use_dummy) {
			TRect	drc(rc.X, rc.Y, NOCLIP_CX, NOCLIP_CY);
			view->PaintBmpIconCore(g->GetHDC(), hNoClipBmp, drc);
//			HGDIOBJ	svObj = ::SelectObject(hTmpDc, hNoClipBmp);
//			::BitBlt(g->GetHDC(), rc.X, rc.Y, rc.Width, rc.Height, hTmpDc, 0, 0, SRCCOPY);
//			::SelectObject(hTmpDc, svObj);
		}
		else {
			g->DrawImage(img, rc);
		}
		Pen		pen(Color(255, 255, 128, 128), CLIP_FRAME);
		g->DrawRectangle(&pen, rc);
	}

	return	cy;
}

int TChildView::PaintThumb(ViewMsg *msg, int top_y, int *_width, BOOL is_direct)
{
	int		&width = *_width;
	int		clip_x = CLIPSIZE;

	if ((msg->msg.flags & DB_FLAG_CLIP) == 0) {
		return 0;
	}

	if (width >= CLIPSIZE * 2) {
		width -= CLIPSIZE;
	} else if (width > CLIPSIZE + CLIPSIZE_MIN) {
		clip_x = width - CLIPSIZE;
		width = CLIPSIZE;
	} else if (width > CLIPSIZE_MIN * 2) {
		clip_x = CLIPSIZE_MIN;
		width -= CLIPSIZE_MIN;
	} else {
		return	0;
	}

	int		clip_y = 0;
	int		diff_x_min = clip_x;
	WCHAR	clip_path[MAX_PATH];
	HDC		hDc = is_direct ? ::GetDC(hWnd) : NULL;

	for (int i=0; i < msg->msg.clip.size(); i++) {
		LogClip	&clip = msg->msg.clip[i];
		Rect	rc;

		if (i > 0) {
			clip_y += ITEM_GAP;
		}

		if (IsRealDraw()) {
			MakeClipPathW(clip_path, cfg, clip.fname.s());
			Image	img(clip_path);
			Graphics g(hDc ? hDc : hMemDc);

			PaintThumbCore(this, img.GetWidth(), img.GetHeight(), clip_x, clip_y, width,
				&rc, &top_y, &diff_x_min, hTmpDc, hNoClipBmp, &g, &img);
		}
		else {
			if (clip.sz.cx == 0 && clip.sz.cy == 0) {
				if (SetClipDimension(cfg, &clip)) {
					logDb->UpdateClip(msg->msg.msg_id, &clip);
					//RequestResetCache();
				}
			}
			PaintThumbCore(this, clip.sz.cx, clip.sz.cy, clip_x, clip_y, width,
				&rc, &top_y, &diff_x_min);
		}
		clip_y += rc.Height + 1;

		if (!is_direct) {
			if (IsRealDraw()) {
				TRect	irc(rc.X, rc.Y, rc.Width, rc.Height);
				if (Item *item = itemMgr.SetItem(Item::CLIPBODY, &irc)) {
					item->clip_idx = i;
					item->drawRc   = irc;
				}
			}
		}
	}
	if (hDc) {
		::ReleaseDC(hWnd, hDc);
	}

	width += diff_x_min - CLIP_FRAME*2;
	return clip_y;
}


//int TChildView::PaintBodyBk(ViewMsg *msg, int top_y, int thum_cy, BodyKind *bk, int bk_num)
//{
//	Wstr		tmp_p;
//	const WCHAR	*top_p = tab_to_space(msg->msg.body.s(), &tmp_p);
//	const WCHAR	*p = top_p;
//	const WCHAR	*next = wcschr_next(p, '\n');
//	int			width = vrc.cx() - BODY_GAP;
//	int			org_width = width;
//	int			l_no = 0;
//	int			thum_cy = PaintThumb(msg, top_y, &width);
//	BodyKind	bk[100];
//	int			bk_num;
//	int			bk_i = 0;
//
//	bk_num = AnalyzeBody(msg, bk, sizeof(bk) / sizeof(BodyKind));
//
//	while (p && *p) {
//		int		len = (int)(next ? (next - p) : wcslen(p));
//		int		rlen = 0;
//		TSize	sz;
//
//		::GetTextExtentExPointW(hMemDc, p, len, width, &rlen, 0, &sz);
//		TRect rc(BODY_GAP, top_y + l_no * lineCy, width, lineCy);
//		::DrawTextW(hMemDc, p, rlen, &rc, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
//
//		itemMgr.SetItem(&rc, Item::BODY, int(p - top_p), rlen);
//
//		if (len == rlen) {
//			p = next;
//			next = wcschr_next(p, '\n');
//		} else {
//			p += rlen;
//		}
//		l_no++;
//		// 画像回り込み計算
//		if (width != org_width && (l_no * lineCy) >= thum_cy) {
//			width = org_width;
//		}
//	}
//	int	ln_cy = l_no * lineCy;
//
//	return	max(ln_cy, thum_cy);
//}


void TChildView::CalcBreakLineInfo(ViewMsg *msg, int pos, int sel_pos, int sel_len,
	BreakLineInfo *_bi)
{
	BreakLineInfo &bi = *_bi;

	if (sel_len > 0) {
		if (pos < sel_pos) {
			if (bi.break_pos > sel_pos) {
				bi.break_pos = sel_pos;
			}
		}
		else if (pos >= sel_pos && pos < sel_pos + sel_len) {
			if (bi.break_pos > sel_pos + sel_len) {
				bi.break_pos = sel_pos + sel_len;
			}
			bi.sel_idx = 0;
		}
	}
	for (int i=0; i < msg->link.size(); i++) {
		BodyAttr &attr = msg->link[i];
		if (pos < attr.pos) {
			if (bi.break_pos > attr.pos) {
				bi.break_pos = attr.pos;
			}
			break;
		}
		if (pos >= attr.pos && pos < attr.pos + attr.len) {
			if (bi.break_pos > attr.pos + attr.len) {
				bi.break_pos = attr.pos + attr.len;
			}
			bi.link_idx = i;
		}
	}
	for (int i=0; i < msg->find.size(); i++) {
		BodyAttr &attr = msg->find[i];
		if (pos < attr.pos) {
			if (bi.break_pos > attr.pos) {
				bi.break_pos = attr.pos;
			}
			break;
		}
		if (pos >= attr.pos && pos < attr.pos + attr.len) {
			//bi.break_pos = attr.pos + attr.len;
			if (bi.break_pos > attr.pos + attr.len) {
				bi.break_pos = attr.pos + attr.len;
			}
			bi.find_idx = i;
		}
	}
	for (int i=0; i < msg->msg.mark.size(); i++) {
		LogMark &attr = msg->msg.mark[i];
		if (pos < attr.pos) {
			if (bi.break_pos > attr.pos) {
				bi.break_pos = attr.pos;
			}
			break;
		}
		if (pos >= attr.pos && pos < attr.pos + attr.len) {
			if (bi.break_pos > attr.pos + attr.len) {
				bi.break_pos = attr.pos + attr.len;
			}
			bi.mark_idx = i;
		}
	}
}

BOOL TChildView::SetBodyItem(int pos, int len, BreakLineInfo *bi, TRect *rc)
{
	if (IsRealDraw()) {
		if (Item *item = itemMgr.SetItem(Item::BODY, rc)) {
			item->body_pos = pos;
			item->body_len = len;

			if (bi->link_idx >= 0) {
				item->kind |= Item::LINK;
				item->link_idx = bi->link_idx;
			}
			if (bi->sel_idx >= 0) {
				item->kind |= Item::SEL;
				// item->sel_idx = bi->sel_idx;
			}
			if (bi->mark_idx >= 0) {
				item->kind |= Item::MARK;
				item->mark_idx = bi->mark_idx;
			}
			if (bi->find_idx >= 0) {
				item->kind |= Item::FIND;
				item->find_idx = bi->find_idx;
			}
			//Debug("SetBodyItem %d/%d\n", rc->y(), rc->cy());
			return	TRUE;
		}
	}

	return	FALSE;
}

BOOL TChildView::GetTep(HDC hDc, int tep_idx, ViewMsg *msg, const WCHAR *s, int max_len,
	int width, int *rlen, int *rcx)
{
	Tep		*tep = NULL;
	int		offset = INT_RDC(s - msg->msg.body.s());

	if (msg->tep.size() > tep_idx) {
		tep = &msg->tep[tep_idx];
		if (msg->tepGenNo == tepGenNo && tep->offset == offset &&
			tep->maxLen == max_len && tep->width == width) {
			*rlen = tep->rlen;
			*rcx  = tep->cx;
	//		Debug(".(%llx of=%d len=%d cx=%d width=%d)", msg->msg.msg_id, offset, *rlen, sz->cx, width);
			return	TRUE;
		}
	}

	if (!TGetTextWidth(hDc, s, max_len, width, rlen, rcx)) {
		return	FALSE;
	}

	if (tep) {
		tep->offset = offset;
		tep->maxLen = max_len;
		tep->rlen   = *rlen;
		tep->width  = short(width);
		tep->cx     = short(*rcx);
	//	Debug("-(%llx of=%d len=%d cx=%d width=%d)", msg->msg.msg_id, offset, *rlen, sz->cx, width);
	}
	else {
		Tep	tep = { offset, max_len, *rlen, short(width), short(*rcx) };
		msg->tep.push_back(tep);
	//	Debug("+(%llx of=%d len=%d cx=%d width=%d)", msg->msg.msg_id, offset, *rlen, sz->cx, width);
	}
	msg->tepGenNo = tepGenNo;

	return	TRUE;
}

int TChildView::PaintBodyLine(ViewMsg *msg, int *_tep_idx, const WCHAR *s, int len, int sel_pos,
	int sel_len, int top_y, int width)
{
	const WCHAR	*top_s = msg->msg.body.s();
	TRect		gap_rc(0, top_y, BODY_GAP, lineCy);
	TRect		rc(BODY_GAP, top_y, width, lineCy);
	HGDIOBJ		svFont;
	BOOL		real_draw = IsRealDraw() && (top_y + lineCy >= 0 && top_y < vrc.cy());
	int			&tep_idx = *_tep_idx;
	int			done = 0;

	if (real_draw) {
		if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &gap_rc)) {
			item->body_pos = int(s - top_s);
			item->body_len = 0;
		}
	}

	while (done < len) {
		int	pos = int(s - top_s);
		BreakLineInfo	bi = {
			pos + (len - done), -1, -1, -1, -1,
		};
		CalcBreakLineInfo(msg, pos, sel_pos, sel_len, &bi);

		int		ilen = bi.break_pos - pos;
		int		rlen = 0;
		int		rcx = 0;

		GetTep(hMemDc, tep_idx++, msg, s, ilen, width, &rlen, &rcx);
		if (rlen == 0) {
			break;
		}
		if (ilen > rlen && s[rlen] == '\n') {	// 次の文字が改行の場合、それも含める
			rlen++;
		}
		rc.set_cx(rcx);

		if (real_draw) {
			if (bi.link_idx >= 0) {
				SetTextColor(hMemDc, LINK_RGB);
				svFont = SelectObject(hMemDc, hLinkFont);
			}
			if (bi.sel_idx >= 0) {
				SetTextColor(hMemDc, WHITE_RGB);
				TRect	rc2 = rc;
				rc2.left   -= 1;
				PaintRect(hMemDc, hSelPen, hSelBrush, rc2);
			}
			else if (bi.mark_idx >= 0) {
				TRect	rc2 = rc;
				rc2.left   -= 1;
				PaintRect(hMemDc, hMarkPen, hMarkBrush, rc2);
			}
			if (bi.find_idx >= 0) {
				TRect	rc2 = rc;
				rc2.top    += lineDiff3;	// 検索背景ボックスは
				rc2.bottom -= lineDiff3;	// 選択やマーカーより小さく
				rc2.left   -= 1;
				PaintRect(hMemDc, hFindPen, hFindBrush, rc2);
			}
			::DrawTextW(hMemDc, s, rlen - (s[rlen-1] == '\n' ? 1 : 0), &rc,
				DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

			if (bi.link_idx >= 0) {
				SetTextColor(hMemDc, BLACK_RGB);
				SelectObject(hMemDc, svFont);
			}
			if (bi.sel_idx >= 0) {
				SetTextColor(hMemDc, BLACK_RGB);
			}
			SetBodyItem(int(s - top_s), rlen, &bi, &rc);
		}

		width   -= rcx;
		rc.left += rcx;
		rc.set_cx(width);
		done += rlen;
		s += rlen;
		if (ilen != rlen) {
			break;
		}
	}
	if (width > 0) {
		if (IsRealDraw()) {
			if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &rc)) {
				item->body_pos = int(s - top_s);
				item->body_len = 0;
			}
		}
	}

	return	min(done, len);
}

int TChildView::PaintUnOpenBox(int x, int y, int cx, int cy)
{
	TRect	rc(x, y, cx, cy);
	PaintRect(hMemDc, hShinePen, hShineBrush, rc);
	return	cy;
}

int TChildView::PaintBodyLines(ViewMsg *msg, int top_y, int *_width, int thum_cy)
{
	const WCHAR	*s = msg->msg.body.s();
	const WCHAR	*p = s;
	const WCHAR	*next = wcschr_next(p, '\n');
	const WCHAR *lim_p = msg->msg.body.s() + msg->msg.body.StripLen();
	int			tep_idx = 0;
	int			sel_pos = 0;
	int			sel_len = 0;
	int			fold_cnt = 0;
	int			l_no = 0;
	int			&width = *_width;
	int			org_width = vrc.cx() - BODY_GAP;

	IsMsgSelected(msg->msg.msg_id, 0, msg->msg.body.Len(), &sel_pos, &sel_len);

	while (p && *p) {
		int		len = (int)(next ? (next - p) : wcslen(p));

		if (msg->unOpenR && IsRealDraw()) {
			PaintUnOpenBox(0, top_y + l_no * lineCy,
				width + ((width == org_width) ? BODY_GAP : 0), lineCy);
		}
		int done = PaintBodyLine(msg, &tep_idx, p, min(len, maxLineChars), sel_pos, sel_len,
			top_y + l_no * lineCy, width);

		if (done == len) {
			if (next && next < lim_p) {
				p = next;
				next = wcschr_next(p, '\n');
			} else {
				p = next = NULL;
			}
		} else if (done == 0) {
			break;	// PaintBodyLine がコケている
		} else {
			p += done;
		}
		l_no++;

		// 画像回り込み計算
		if (width != org_width && (l_no * lineCy) >= thum_cy) {
			width = org_width;
		}

		bool	is_quote = false;
		if ((dispFlags & DISP_FOLD_QUOTE) && (msg->quotePos > 0 && (p - s) > msg->quotePos)) {
			is_quote = true;
		}

		if ((is_quote || l_no >= maxLines) && p) {
			fold_cnt++;
			if (l_no == maxLines || is_quote) {
				if (IsFold(msg)) {
					int  remain_chars = int(lim_p - p);
					if (is_quote || remain_chars > maxLineChars * (FOLD_MARGIN-1) ||
						(next && (fold_cnt += wcs_lncnt(next, lim_p)) > FOLD_MARGIN)) {
						msg->foldMsg = true; // 畳み込む
						l_no--; // 最終行は、畳み込みボタンと被せる
						break;
					}
				}  // fold で FOLD_MARGIN以下の場合は、最後まで描画
				else {
					msg->foldMsg = true;
				}
			}
		}
	}
	return	l_no;
}

int TChildView::PaintBody(ViewMsg *msg, int top_y)
{
	int		width = vrc.cx() - BODY_GAP;
	int		org_width = width;
	int		thum_cy = PaintThumb(msg, top_y, &width);

	msg->foldMsg = FALSE;

	if (msg->unOpenR && IsRealDraw()) {
		SetTextColor(hMemDc, SHADOW_RGB);
	}

	if (msg->unOpenR && IsRealDraw() && width != org_width) {
		PaintUnOpenBox(0, top_y, width+BODY_GAP, ALIGN_SIZE(thum_cy, lineCy));
		if (int gap_cy = lineCy - (thum_cy % lineCy)) {
			PaintUnOpenBox(width+BODY_GAP, top_y + thum_cy, vrc.cx()-width, gap_cy);
		}
	}

	int l_no = PaintBodyLines(msg, top_y, &width, thum_cy);

	int	ln_cy = l_no * lineCy;
	int	ret_cy = max(ln_cy, thum_cy);

	if (IsRealDraw()) {
		if (ln_cy < thum_cy) {
			TRect	rc(0, top_y + ln_cy, width, thum_cy - ln_cy);
			if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &rc)) {
				item->body_pos = msg->msg.body.Len();
				item->body_len = 0;
			}
		}
	}
	if (msg->foldMsg) {
		int	btn_y  = top_y + ret_cy; // 本文の後ろ
		int	btn_cy = (FOLD_BTN_CY + ITEM_GAP);

		// 最終的なボタン位置に関わらず、fold/!fold各モードでの最長サイズを設定
		// （キャッシュCy対策）
		if (btn_y + btn_cy >= vrc.cy()) {
			btn_y = vrc.cy() - btn_cy;
		}
		if (btn_y < top_y + lineCy) {
			btn_y = top_y + lineCy;
		}
		if (IsRealDraw()) {
			PaintFoldBtn(msg, btn_y, 0, top_y + ret_cy);
		}
		ret_cy += btn_cy;
	}

	if (msg->unOpenR && IsRealDraw()) {
		SetTextColor(hMemDc, BLACK_RGB);
	}

	return	ret_cy;
}

int TChildView::PaintUnOpenPict(TRect rc, BOOL is_direct)
{
	HDC 	hDc = hMemDc;

	if (is_direct) {
		hDc = ::GetDC(hWnd);
		MemToClinetRect(&rc);
	}

	HGDIOBJ	svObj = ::SelectObject(hTmpDc, hUnOpenBigBmp);

	::TransparentBlt(hDc, OffPix(rc.cx(), UNOPENB_CX), rc.y() + OffPix(rc.cy(), UNOPENB_CY),
		UNOPENB_CX, UNOPENB_CY, hTmpDc, 0, 0, UNOPENB_CX, UNOPENB_CY, RGB(192,192,192));

	::SelectObject(hTmpDc, svObj);

	if (is_direct) {
		::ReleaseDC(hWnd, hDc);
	}

	return	TRUE;
}


int TChildView::PaintMsg(ViewMsg *msg)
{
	int&	top_y = msg->dispYOff;
	msg->dispFlags  = dispFlags;
	msg->sz.cx      = vrc.cx();

	itemMgr.SetMsgIdx(msg->msg.msg_id);

	if ((dispFlags & DISP_TITLE_ONLY)) {
		// msg->sz.cy += 1;
		msg->sz.cy = PaintHead(msg, top_y);
		return msg->sz.cy;
	}
	else {
		msg->sz.cy = headCy; // ヘッダは最後に描画（枠サイズだけ確保）
	}

	if (IsRealDraw()) {
		if (msg->unOpenR) {
			PaintUnOpenBox(0, top_y + msg->sz.cy, msg->sz.cx, headDiff2);
		}
		TRect	rc(0, top_y + msg->sz.cy, vrc.cx(), headDiff2);	//余白部分の登録
		if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &rc)) {
			item->body_pos = 0;
			item->body_len = 0;
		}
	}

	int		body_margin = headDiff2;
	int		body_cy = PaintBody(msg, top_y + msg->sz.cy + body_margin);
	TRect	unopen_rc;
	Item	*unopen_item = NULL;

	Item	*fold_item = IsRealDraw() ? itemMgr.GetLastItem() : NULL;
	if (fold_item && (fold_item->kind & Item::FOLDBTN) == 0) {
		fold_item = NULL;
	}
	if (msg->unOpenR && IsRealDraw()) {
		int		y  = top_y + msg->sz.cy;
		int		cy = body_cy + body_margin;

		if (y < 0) {
			cy += y;
			y = 0;
		}
		if (y + cy > vrc.cy()) {
			cy = vrc.cy() - y;
			if (cy < UNOPENB_CY) {
				if (y > 0) {
					cy = UNOPENB_CY;
				}
			}
		}
		unopen_rc.Init(0, y, vrc.cx(), cy);

		if ((unopen_item = itemMgr.SetItem(Item::UNOPENBODY, &unopen_rc))) {
			unopen_item->drawRc = unopen_rc;
		}
	}

	msg->sz.cy += body_margin + body_cy;

	if (msg->msg.comment) {
		msg->sz.cy += PaintCommentInBody(msg, top_y + msg->sz.cy);
	}
	int msg_margin = (msg->foldMsg && !msg->msg.comment) ? headDiff : lineCy;

	if (IsRealDraw()) {
		if (msg->unOpenR) {
			PaintUnOpenBox(0, top_y + msg->sz.cy, msg->sz.cx, msg_margin);
			if (unopen_item) {
				unopen_item->rc.bottom     = top_y + msg->sz.cy + msg_margin;
				unopen_item->drawRc.bottom = top_y + msg->sz.cy + msg_margin;
				if (fold_item) {  // Fold btn が設置済みの場合は、Foldを優先
					if (fold_item->drawRc.bottom < unopen_item->drawRc.bottom) {
						unopen_item->drawRc.bottom = fold_item->drawRc.bottom;
					}
					itemMgr.SwapItem(unopen_item, fold_item);
				}
			}
		}
		else {
			TRect	rc(0, top_y + msg->sz.cy, vrc.cx(), msg_margin);	//余白部分の登録
			if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &rc)) {
				item->body_pos = msg->msg.body.Len();
				item->body_len = 0;
			}
		}
	}
	msg->sz.cy += msg_margin;

	int		off_y = CalcScrOutYOff(msg);
	PaintHead(msg, off_y);

	return	msg->sz.cy;
}

BOOL TChildView::GetPaintMsgCy(int64 msg_id)
{
	if (ViewMsg *msg = GetMsg(msg_id)) {
		if (msg->sz.cx == vrc.cx() && msg->dispFlags == dispFlags) {
			return msg->sz.cy;
		}

		BOOL	set_no_draw = FALSE;
		if (IsRealDraw()) {
			set_no_draw = TRUE;
			dispFlags |= DISP_NODRAW;
			itemMgr.SetSkip(TRUE);
		}
		else {
			Debug("re-entrant\n");
		}

		msg->dispYOff = 0;
		BOOL ret = PaintMsg(msg);

		if (set_no_draw) {
			dispFlags &= ~DISP_NODRAW;
			itemMgr.SetSkip(FALSE);
		}
		msg->dispFlags &= ~DISP_NODRAW;

		return	ret;
	}
	return	 0;
}

BOOL TChildView::AlignCurPaintIdx()
{
// curIdx が描画範囲にない場合、curIdx/offsetPixを変更して見える範囲に設定

	if (offsetPix < 0) {
		while (curIdx < msgIds.UsedNumInt()) {
			int cur_cy = GetPaintMsgCy(msgIds[curIdx].msg_id);
			if (curIdx + 1 >= msgIds.UsedNumInt()) {
				if (cur_cy <= vrc.cy()) {
					offsetPix = 0;
				}
				else if (offsetPix <= vrc.cy() - cur_cy) {
					offsetPix = vrc.cy() - cur_cy;
				}
				break;
			}
			if (cur_cy + offsetPix >= headCy) {
				break;
			}
			offsetPix += cur_cy;
			curIdx++;
		}
	}
	else {
		while (curIdx > 0 && offsetPix >= vrc.cy() - headCy) {
			curIdx--;
			int cur_cy = GetPaintMsgCy(msgIds[curIdx].msg_id);
			offsetPix -= cur_cy;
		}
	}
	return	TRUE;
}

BOOL TChildView::CalcStartPrintIdx(int *_idx, int *_pix)
{
	int	&idx = *_idx;
	int	&pix = *_pix;

	idx = curIdx;
	pix = offsetPix;

	while (pix > 0 && idx > 0) {
		idx--;
		int cur_cy = GetPaintMsgCy(msgIds[idx].msg_id);
		pix -= cur_cy;
	}

	return	TRUE;
}

BOOL TChildView::PaintFocus(BOOL is_direct)
{
	HDC 	hDc = hMemDc;

#define FOCUS_YOFF 1
	if (ViewMsg *msg = GetMsg(msgIds[curIdx].msg_id)) {
		int		yoff = CalcScrOutYOff(msg);
		TRect	rc(1, yoff + FOCUS_YOFF, FOCUS_WIDTH, headCy - FOCUS_YOFF*2
			- (yoff < 0 ? 1 : 0));

		if (is_direct) {
			hDc = ::GetDC(hWnd);
			MemToClinetRect(&rc);

			if (focusIdx >= 0 && focusIdx < msgIds.UsedNumInt()) {
				if (ViewMsg *oldMsg = GetMsg(msgIds[focusIdx].msg_id)) {
					TRect	oldRc(1, oldMsg->dispYOff + FOCUS_YOFF, FOCUS_WIDTH,
						headCy - FOCUS_YOFF*2);
					MemToClinetRect(&oldRc);
					PaintRect(hDc, hGrayPen, hGrayBrush, oldRc);
				}
			}
		}
		PaintRect(hDc, hShadowPen, hShadowBrush, rc);

		if (is_direct) {
			::ReleaseDC(hWnd, hDc);
		}
		focusIdx = curIdx;
	}

	return	TRUE;
}

BOOL TChildView::PaintMemBmp()
{
	dispFlags &= ~DISP_OVERLAPPED_ALL;
	::GetLocalTime(&curTime);

	if (msgIds.UsedNumInt() <= 0) {
		return	FALSE;
	}
	offsetPix += scrOffPix;

	AlignCurPaintIdx();

	int		idx = curIdx;
	int		pix = offsetPix;

	if (offsetPix > 0) {
		CalcStartPrintIdx(&idx, &pix);
	}

	if (pix > 0 && idx == 0 && scrOffPix > 0) {
		offsetPix -= scrOffPix;
		scrOffPix -= pix;
		return	PaintMemBmp();
	}
	int	drawed_cy = pix;
	int	need_cy = vrc.cy();
	::PatBlt(hMemDc, 0, 0, vrc.cx(), vrc.cy(), WHITENESS);
	//Debug("p item reset(%d)\n", itemMgr.Num());
	itemMgr.Reset();
	dispGenNo++;

	ScrollMode last_scroll = scrollMode;
	int last_end = endDispIdx;
	startDispIdx = idx;
	startDispPix = offsetPix;

	ViewMsg *msg = NULL;
	for (int i=idx; i < msgIds.UsedNumInt() && drawed_cy < need_cy; i++) {
		if ((msg = GetMsg(msgIds[i].msg_id))) {
			itemMgr.SetMsgIdx(msg->msg.msg_id);
			msg->dispYOff  = drawed_cy;
			msg->dispGenNo = dispGenNo;
			int cy = PaintMsg(msg);
			drawed_cy += cy;
			endDispIdx = i;
			endDispPix = need_cy - drawed_cy;

			if (curIdx == i) {
				PaintFocus();
			}
		}
	}
	//Debug("p item end (%d)\n", itemMgr.Num());

	if (msg) {
		if (drawed_cy < need_cy) {	// 余白領域は直前メッセージとみなす
			TRect	rc(0, drawed_cy, vrc.cx(), vrc.cy() - drawed_cy);
			if (Item *item = itemMgr.SetItem(Item::BODY|Item::BODYBLANK, &rc)) {
				item->body_pos = msg->msg.body.Len();
				item->body_len = 0;
			}
		}
	}

	ScrollMode sv_scrmode = scrollMode;
	if (dispFlags & DISP_REV) {
		scrollMode = (startDispIdx == 0 && startDispPix >= -29) ?
			AUTO_SCROLL : NONE_SCROLL;
	}
	else {
		scrollMode = (endDispIdx == msgIds.UsedNumInt() -1 && endDispPix >= -50) ?
			AUTO_SCROLL : NONE_SCROLL;
	}
	if (sv_scrmode != scrollMode) {
		UpdateBtnBmp();
	}
//	Debug("end=%d -> %d(%d), scr=%d->%d\n", last_end, endDispIdx, endDispPix, last_scroll, scrollMode);

	if (scrOffPix) {
		SetScrollPos();
		scrOffPix = 0;
	}

	PostMessage(WM_LOGVIEW_MOUSE_EMU, 0, 0);
	if (mouseScrMode == MOUSE_SCR_REQ) {
		mouseScrMode = MOUSE_SCR_DONE;
	}

//Debug(" -> idx=%d  off=%d\n", curIdx, offsetPix);

	return	0;
}


#define MAX_LINECHARS 1024

BOOL TChildView::CalcMaxLineChars()
{
	static char *test_data = [&](){
		char *p = new char[MAX_LINECHARS + 1];
		memset(p, ' ', MAX_LINECHARS);
		p[MAX_LINECHARS] = 0;
		return p;
	}();

	SIZE	sz;
	maxLineChars = 0;
	::GetTextExtentExPoint(hMemDc, test_data, MAX_LINECHARS, vrc.cx(), &maxLineChars, 0, &sz);
	Debug("max chars=%d\n", maxLineChars);

	return	maxLineChars ? TRUE : FALSE;
}

