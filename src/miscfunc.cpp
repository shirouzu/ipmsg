static char *miscfunc_id = 
	"@(#)Copyright (C) H.Shirouzu 2011-2018   miscfunc.cpp	Ver4.90";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc functions
	Create					: 2011-05-03(Tue)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include "blowfish.h"
#include <time.h>
#include <vector>
using namespace std;

/*
	URL検索ルーチン
*/
UrlObj *SearchUrlObj(TListEx<UrlObj> *list, char *protocol)
{
	for (auto obj = list->TopObj(); obj; obj = list->NextObj(obj))
		if (stricmp(obj->protocol, protocol) == 0)
			return	obj;

	return	NULL;
}

#if !defined(USE_CMD) && !defined(USE_ADMIN)
/*
	ダイアログ用アイコン設定
*/
void SetDlgIcon(HWND hWnd)
{
	static HICON	oldHIcon = NULL;
	static HICON	oldHBigIcon = NULL;

	HICON	hBigIcon;
	HICON	hIcon = GetIPMsgIcon(&hBigIcon);

	if (oldHIcon != hIcon || oldHBigIcon != hBigIcon) {
		::SetClassLong(hWnd, GCL_HICONSM, LONG_RDC(hIcon));
		::SetClassLong(hWnd, GCL_HICON,   LONG_RDC(hBigIcon));

		oldHIcon = hIcon;
		oldHBigIcon = hBigIcon;
	}
}

void ChangeWindowTitle(TWin *wnd, Cfg *cfg)
{
	WCHAR		wbuf[MAX_BUF];
	vector<Wstr> wvec;

	if (cfg && (cfg->NoTcp || cfg->NoFileTrans)) {
		wvec.push_back(cfg->NoFileTrans == 2 ? L" (No Share Transfer)" : L" (No File Transfer)");
	}

	if (!gEnableHook) {
		wvec.push_back(L" (Non Hook)");
	}

	if (wvec.size() > 0) {
		wnd->GetWindowTextW(wbuf, wsizeof(wbuf));
		for (auto &w: wvec) {
			wcsncatz(wbuf, w.s(), wsizeof(wbuf));
		}
		wnd->SetWindowTextW(wbuf);
	}
}

#endif

/*
	ログ記録/ウィンドウ表示用の HostEntry表示文字列
*/
int MakeListString(Cfg *cfg, HostSub *hostSub, THosts *hosts, char *buf, BOOL is_log)
{
	Host	*host = hostSub->addr.IsEnabled() ?
		hosts->GetHostByAddr(hostSub) : hosts->GetHostByName(hostSub);

	if (host && IsSameHost(hostSub, &host->hostSub) ||
		GetUserNameDigestField(hostSub->u.userName) &&
		 	(host = cfg->priorityHosts.GetHostByName(hostSub))) {
		return	MakeListString(cfg, host, buf, is_log);
	}
	else {
		Host	host;

		memset(&host, 0, sizeof(host));
		host.hostSub = *hostSub;
		return	MakeListString(cfg, &host, buf, is_log);
	}
}

int MakeNick(Cfg *cfg, HostSub *hostSub, THosts *hosts, char *buf)
{
	Host	*host = hostSub->addr.IsEnabled() ?
		hosts->GetHostByAddr(hostSub) : hosts->GetHostByName(hostSub);
	const char *name = hostSub->u.userName;

	if (host && IsSameHost(hostSub, &host->hostSub) ||
		GetUserNameDigestField(hostSub->u.userName) &&
		 	(host = cfg->priorityHosts.GetHostByName(hostSub))) {
		if (*host->nickName) {
			name = host->nickName;
		}
	}
	return	strcpyz(buf, name);
}

void EscapeForJson(const char *s, U8str *d)
{
	int esc_num = 0;

	for (const char *p=s; *p; p++) {
		const char &c = *p;
		switch (c) {
		case '\\':
		case '"':
		case '/':
		case '\t':
		case '\n':
			esc_num++;
			break;
		}
	}

	d->Init((int)strlen(s) + esc_num + 1);
	char	*buf = d->Buf();

	for (const char *p=s; *p; p++) {
		const char &c = *p;
		switch (c) {
		case '\\':
		case '"':
		case '/':
			*buf++ = '\\';
			*buf++ = c;
			break;

		case '\t':
			*buf++ = '\\';
			*buf++ = 't';
			break;

		case '\n':
			*buf++ = '\\';
			*buf++ = 'n';
			break;

		case '\r':	// eat
			break;

		default:
			*buf++ = c;
			break;
		}
		s++;
	}
	*buf = 0;
}

struct ReplaceInfo {
	const char	*s;
	int			len;
	const U8str	*d;
};

int ReplaseKeyword(const char *s, U8str *out, map<U8str, U8str> *dict)
{
	static auto re = regex("\\$\\([a-zA-Z_]+\\)", regex_constants::icase);

	cmatch				cm;
	vector<ReplaceInfo> vri;
	ReplaceInfo			ri;

	vri.reserve(10);

	for (const char *p=s; *p; p = ri.s+ri.len) {
		if (!regex_search(p, cm, re)) {
			break;
		}
		ri.s   = p + cm.position(0);
		ri.len = (int)cm.length(0);

		auto itr = dict->find(U8str(ri.s, BY_UTF8, ri.len));
		if (itr != dict->end()) {
			ri.d   = &itr->second;
			vri.push_back(ri);
		}
	}

	int		dlen = (int)strlen(s);
	for (auto &i: vri) {
		dlen += i.d->Len() - i.len;
	}
	out->Init(dlen + 1);
	const char	*p = s;
	char		*d = out->Buf();

	for (auto &i: vri) {
		int slen = int(i.s - p);
		d += strncpyz(d, p, slen + 1);
		p += i.len + slen;
		d += strcpyz(d, i.d->s());
	}
	strcpyz(d, p);

	Debug("dlen=%d out=%d\n", dlen, out->Len());

	return	out->Len();
}

/*
	ログ記録/ウィンドウ表示用の HostEntry表示文字列
*/
int MakeListString(Cfg *cfg, Host *host, char *buf, BOOL is_log)
{
	BOOL	logon  = is_log ? cfg->LogonLog    : cfg->RecvLogonDisp;
	BOOL	ipaddr = is_log ? cfg->IPAddrCheck : cfg->RecvIPAddr;
	char	*sv_buf = buf;

	buf += sprintf(buf, "%s ", *host->nickName ? host->nickName : host->hostSub.u.userName);

	if (host->hostStatus & IPMSG_ABSENCEOPT) *buf++ = '*';

	buf += sprintf(buf, "(%s%s%s", host->groupName,
					*host->groupName ? "/" : "", host->hostSub.u.hostName);

	if (ipaddr) {
		buf += sprintf(buf, "/%s", host->hostSub.addr.S());
	}
	if (logon)  buf += sprintf(buf, "/%s", host->hostSub.u.userName);

	buf += strcpyz(buf, ")");

	return	int(buf - sv_buf);
}


/* 
	リンク関連
*/

LinkKind GetLinkKind(Cfg *cfg, const WCHAR *s)
{
	wcmatch	wm;

	if (regex_search(s, wm, *cfg->file_re)) {
		return	LK_FILE;
	}

	if (regex_search(s, wm, *cfg->fileurl_re)) {
		return	LK_FILE_URL;
	}

	if (regex_search(s, wm, *cfg->url_re)) {
		return	LK_URL;
	}

	return	LK_NONE;
}

LinkActKind GetLinkActKind(Cfg *cfg, LinkKind lk, BOOL is_view)
{
	if (lk == LK_NONE) {
		return LAK_NONE;
	}

	if (lk == LK_URL) {
		return	((cfg->clickOpenMode & 1) && (is_view || (cfg->clickOpenMode & 4) == 0))
			? LAK_CLICK : LAK_DBLCLICK;
	}

	// LK_FILE or LK_FILE_URL
	return	((cfg->clickOpenMode & 2) && (is_view || (cfg->clickOpenMode & 4) == 0))
		? LAK_CLICK : LAK_DBLCLICK;
}

BOOL CheckExtW(Cfg *cfg, const WCHAR *s, BOOL *is_dir)
{
	WCHAR	ext[MAX_PATH];

	if (is_dir) {
		*is_dir = FALSE;
	}

	*ext = 0;
	if (const WCHAR *p = wcsrchr(s, '.')) {
		p++;
		int len = wcsncpyz(ext, p, wsizeof(ext));
		if (len > 0 && ext[len-1] == '"') {
			ext[--len] = 0;
		}
	}

	// 末尾が \ or / なら常に許可、そうでないなら拡張子がない場合、不許可
	const WCHAR *sep1 = wcsrchr(*ext ? ext : s, '/');
	const WCHAR *sep2 = wcsrchr(*ext ? ext : s, '\\');
	if (sep1 || sep2) {
		const WCHAR *targ = sep1 > sep2 ? sep1 : sep2;
		if (targ[1] == 0) {
			if (is_dir) {
				*is_dir = TRUE;
			}
			return TRUE;
		}
		return FALSE;
	}

	if (*ext == 0) {
		return	FALSE;
	}

	for (auto &w: cfg->directExtVec) {
		if (wcsicmp(w.s(), ext) == 0) {
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL CheckExtU8(Cfg *cfg, const char *s, BOOL *is_dir)
{
	Wstr	w(s);
	return	CheckExtW(cfg, w.s(), is_dir);
}

#if !defined(USE_ADMIN) && !defined(USE_CMD)
JumpLinkRes JumpLinkWithCheck(TWin *parent, Cfg *cfg, const WCHAR *s, DWORD flg)
{
	LinkKind	lk = GetLinkKind(cfg, s);
	LinkActKind	ak = GetLinkActKind(cfg, lk, (flg & JLF_VIEWER) ? TRUE : FALSE);

	if (ak == LAK_CLICK    &&  (flg & JLF_DBLCLICK) ||
		ak == LAK_DBLCLICK && !(flg & JLF_DBLCLICK) ||
		ak == LAK_NONE) {
		return	JLR_SKIP;
	}

	if (lk == LK_FILE || lk == LK_FILE_URL) {
		if (cfg->linkDetectMode & 1) {	// ファイル系は実行禁止
			TMsgBox(parent).Exec( Fmt("%s", LoadStrU8(IDS_DISABLEFILELINK)));
			return	JLR_SKIP;
		}
		if (!CheckExtW(cfg, s)) {
			if (TMsgBox(parent).Exec(
				Fmt("%s\r\n%s", LoadStrU8(IDS_ALERTFILELINK), WtoU8s(s))) != IDOK) {
				return	JLR_SKIP;
			}
		}
	}
	if (INT_RDC(ShellExecuteW(NULL, NULL, s, 0, 0, SW_SHOW)) <= 32) {
		return	JLR_FAIL;
	}

	return	JLR_OK;
}
#endif

/*
	IME 制御
*/
BOOL SetImeOpenStatus(HWND hWnd, BOOL flg)
{
	BOOL ret = FALSE;

	HIMC hImc;

	if ((hImc = ::ImmGetContext(hWnd)))
	{
		if (::ImmGetOpenStatus(hImc) == flg) {
			ret = TRUE;
		}
		else {
			ret = ::ImmSetOpenStatus(hImc, flg);
		}
		::ImmReleaseContext(hWnd, hImc);
	}
	return	ret;
}

BOOL GetImeOpenStatus(HWND hWnd)
{
	BOOL ret = FALSE;

	HIMC hImc;

	if ((hImc = ::ImmGetContext(hWnd)))
	{
		ret = ::ImmGetOpenStatus(hImc);
		::ImmReleaseContext(hWnd, hImc);
	}
	return	ret;
}

#if !defined(USE_ADMIN) && !defined(USE_CMD)
/*
	ホットキー設定
*/
BOOL SetHotKey(Cfg *cfg)
{
	::UnregisterHotKey(GetMainWnd(), WM_SENDDLG_OPEN);
	::UnregisterHotKey(GetMainWnd(), WM_RECVDLG_OPEN);
	::UnregisterHotKey(GetMainWnd(), WM_LOGVIEW_OPEN);
	::UnregisterHotKey(GetMainWnd(), WM_DELMISCDLG);

	if (cfg->HotKeyCheck)
	{
		if (cfg->HotKeySend) {
			::RegisterHotKey(GetMainWnd(), WM_SENDDLG_OPEN, cfg->HotKeyModify, cfg->HotKeySend);
		}
		if (cfg->HotKeyRecv) {
			::RegisterHotKey(GetMainWnd(), WM_RECVDLG_OPEN, cfg->HotKeyModify, cfg->HotKeyRecv);
		}
		if (cfg->HotKeyLogView) {
			::RegisterHotKey(GetMainWnd(), WM_LOGVIEW_OPEN, cfg->HotKeyModify, cfg->HotKeyLogView);
		}
		if ((cfg->OpenCheck >= 2 || cfg->HotKeyCheck >= 2) && cfg->HotKeyMisc) {
			::RegisterHotKey(GetMainWnd(), WM_DELMISCDLG, cfg->HotKeyModify, cfg->HotKeyMisc);
		}
	}
	return	TRUE;
}
#endif

BOOL CheckPassword(const char *cfgPasswd, const char *inputPasswd)
{
	char	buf[MAX_NAMEBUF];

	MakePassword(inputPasswd, buf);

	return	strcmp(buf, cfgPasswd) == 0 ? TRUE : FALSE;
}

void MakePassword(const char *inputPasswd, char *outputPasswd)
{
	while (*inputPasswd)
		*outputPasswd++ = ((~*inputPasswd++) & 0x7f);
	*outputPasswd = 0;
}

/*
	Host1 と Host2 が同一かどうかを比較
*/
BOOL IsSameHost(HostSub *hostSub1, HostSub *hostSub2)
{
	if (stricmp(hostSub1->u.hostName, hostSub2->u.hostName))
		return	FALSE;

	return	stricmp(hostSub1->u.userName, hostSub2->u.userName) ? FALSE : TRUE;
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
	for (int kind=0; kind < MAX_KIND; kind++) {
		enable[kind] = FALSE;
	}
	lruIdx = -1;
	isSameSeg = FALSE;

	brSetTick = 0;
	brResTick = 0;
}

THosts::~THosts()
{
	ClearHosts();

	for (int kind=0; kind < MAX_KIND; kind++) {
		if (array[kind]) {
			free(array[kind]);
		}
	}
}

void THosts::ClearHosts()
{
	lruList.clear();

	int	kind = 0;
	for ( ; kind < MAX_KIND; kind++) {
		if (enable[kind]) break;
	}
	if (kind == MAX_KIND) return;

	for (int i=0; i < hostCnt; i++) {
		if (array[kind][i]) {
			array[kind][i]->RefCnt(-1);
			array[kind][i]->SafeRelease();
		}
	}
	hostCnt = 0;
}

int THosts::Cmp(HostSub *hostSub1, HostSub *hostSub2, Kind kind)
{
	switch (kind) {
	case NAME: case NAME_ADDR:
		{
			int	cmp;
			if (cmp = stricmp(hostSub1->u.userName, hostSub2->u.userName))
				return	cmp;
			if ((cmp = stricmp(hostSub1->u.hostName, hostSub2->u.hostName)) || kind == NAME)
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
	int	min = 0;
	int	max = hostCnt -1;
	int	tmpIndex;

	if (insertIndex == NULL) {
		insertIndex = &tmpIndex;
	}

	while (min <= max)
	{
		*insertIndex = (min + max) / 2;

		int	cmp = Cmp(hostSub, &array[kind][*insertIndex]->hostSub, kind);

		if (cmp == 0) {
			if (insertIndex == &tmpIndex) {
				auto &targ = array[kind][*insertIndex];
			//	Debug("Search(%p) %s %p\n", this, targ->S(), targ);
			}
			return	array[kind][*insertIndex];
		}
		else if (cmp > 0) {
			min = *insertIndex +1;
		}
		else {
			max = *insertIndex -1;
		}
	}

	*insertIndex = min;
	return	NULL;
}

BOOL THosts::AddHost(Host *host)
{
	int		insertIndex[MAX_KIND];
	int		kind;

// すべてのインデックス種類での確認を先に行う
	for (kind=0; kind < MAX_KIND; kind++) {
		if (!enable[kind])
			continue;
		if (Host *tmp = Search((Kind)kind, &host->hostSub, &insertIndex[kind])) {
			if (lruIdx >= 0) {
				lruList.splice(lruList.begin(), lruList, tmp->itr[lruIdx]);
			}
			Debug(" *** AddHost error kind=%d %s\n", kind, host->S());
			return	FALSE;
		}
	}

#define BIG_ALLOC	1000
	for (kind=0; kind < MAX_KIND; kind++) {
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

//	Debug("TAddHost(%p) %s %p\n", this, host->S(), host);

	if (lruIdx >= 0) {
		lruList.push_front(host);
		host->itr[lruIdx] = lruList.begin();
	}

	return	TRUE;
}

BOOL THosts::DelHost(Host *host)
{
	int		insertIndex[MAX_KIND];
	int		kind;

// すべてのインデックス種類での確認を先に行う
	for (kind=0; kind < MAX_KIND; kind++) {
		if (!enable[kind]) {
			continue;
		}
		if (Search((Kind)kind, &host->hostSub, &insertIndex[kind]) == NULL) {
			Debug(" *** DelHost error kind=%d %s\n", kind, host->S());
			return	FALSE;
		}
	}

	hostCnt--;

	for (kind=0; kind < MAX_KIND; kind++) {
		if (!enable[kind]) {
			continue;
		}
		memmove(array[kind] + insertIndex[kind], array[kind] + insertIndex[kind] + 1, (hostCnt - insertIndex[kind]) * sizeof(Host *));
	}

	if (lruIdx >= 0) {
		if (lruList.end() == host->itr[lruIdx]) {
			Debug(" *** DelHost already erased\n");
		}
		else {
			lruList.erase(host->itr[lruIdx]);
			host->itr[lruIdx] = lruList.end();
		}
	}

	host->RefCnt(-1);

	auto itr = find_if(agents.begin(), agents.end(), [&](auto &ag) {
		return	(ag.host == host);
	});

	if (itr != agents.end()) {
		Debug(" agent is deleted\n", host->hostSub.addr.S());
		agents.erase(itr);
	}
//	Debug("TDelHost(%p) %s %p\n", this, host->S(), host);

	return	TRUE;
}

BOOL THosts::PriorityHostCnt(int priority, int range)
{
	int		member = 0;

	for (int i=0; i < hostCnt; i++)
		if (array[NAME][i]->priority >= priority && array[NAME][i]->priority < priority + range)
			member++;
	return	member;
}

BOOL GetLastErrorMsg(const char *msg, TWin *win)
{
	char	buf[MAX_BUF];
	snprintfz(buf, sizeof(buf), "%s error = %x", msg ? msg : "", GetLastError());
	return	MessageBox(win ? win->hWnd : NULL, buf, IP_MSG, MB_OK);
}

BOOL GetSockErrorMsg(const char *msg, TWin *win)
{
	char	buf[MAX_BUF];
	snprintfz(buf, sizeof(buf), "%s error = %d", msg ? msg : "", ::WSAGetLastError());
	return	MessageBox(win ? win->hWnd : NULL, buf, IP_MSG, MB_OK);
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
		sprintf(target_fname, "(%c-drive)", *org_path);
	else if (org_path[0] == '\\' && org_path[1] == '\\') {
		if (!PathToFname(org_path + 2, target_fname))
			sprintf(target_fname, "(root)");
	}
	else sprintf(target_fname, "(unknown)");
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
//int MakePath(char *dest, const char *dir, const char *file)
//{
//	BOOL	separetor = TRUE;
//	int		len;
//
//	if ((len = strlen(dir)) == 0)
//		return	sprintf(dest, "%s", file);
//
//	if (dir[len -1] == '\\')	// 表など、2byte目が'\\'で終る文字列対策
//	{
//		if (len >= 2 && !IsDBCSLeadByte(dir[len -2]))
//			separetor = FALSE;
//		else {
//			for (u_char *p=(u_char *)dir; *p && p[1]; IsDBCSLeadByte(*p) ? p+=2 : p++)
//				;
//			if (*p == '\\')
//				separetor = FALSE;
//		}
//	}
//	return	sprintf(dest, "%s%s%s", dir, separetor ? "\\" : "", file);
//}
#endif

BOOL IsValidFileName(char *fname)
{
	return !strpbrk(fname, "\\/<>|:?*");
}



/*
	time() の代わり
*/
//time_t Time(void)
//{
//	SYSTEMTIME	st;
//	int64		ft;
//// 1601年1月1日から1970年1月1日までの通算100ナノ秒
//#define UNIXTIME_BASE	((int64)0x019db1ded53e8000)
//
//	::GetSystemTime(&st);
//	::SystemTimeToFileTime(&st, (FILETIME *)&ft);
//	return	(time_t)((ft - UNIXTIME_BASE) / 10000000);
//}

/*
	ctime() の代わり
	ただし、改行なし
*/
const char *Ctime(SYSTEMTIME *st)
{
	static char	buf[] = "Mon Jan 01 00:00:00 2999";
	static char *wday = "SunMonTueWedThuFriSat";
	static char *mon  = "JanFebMarAprMayJunJulAugSepOctNovDec";
	SYSTEMTIME	_st;

	if (st == NULL) {
		st = &_st;
		::GetLocalTime(st);
	}
	sprintf(buf, "%.3s %.3s %02d %02d:%02d:%02d %04d", &wday[st->wDayOfWeek * 3],
		&mon[(st->wMonth - 1) * 3], st->wDay, st->wHour, st->wMinute, st->wSecond, st->wYear);
	return	buf;
}

/*
	ctime() の代わり
	ただし、改行なし
*/
const char *Ctime(time_t *t)
{
	static char	buf[] = "Mon Jan 01 00:00:00 2999";
	time_t	tt = t ? *t : time(NULL);

	auto	p = ctime(&tt);
	strcpy(buf, p ? p : "Thu Jan 01 09:00:00 1970");

	if (buf[8] == ' ') {
		buf[8] = '0';
	}
	buf[24] = 0;
	return	buf;
}

/*
	ctime() の代わり
	ただし、改行なし
*/
const WCHAR *CtimeW(time_t t)
{
	static WCHAR	buf[] = L"Mon Jan 01 00:00:00 2999";
	if (t == -1) {
		t = time(NULL);
	}

	auto	p = _wctime(&t);
	wcscpy(buf, p ? p : L"Thu Jan 01 09:00:00 1970");

	if (buf[8] == ' ') {
		buf[8] = '0';
	}
	buf[24] = 0;
	return	buf;
}

/*
	サイズを文字列に
*/
int MakeSizeString(char *buf, int64 size, int flg)
{
	if (size >= 1024 * 1024)
	{
		if (flg & MSS_NOPOINT) {
			return	sprintf(buf, "%d%sMB",
				(int)(size / 1024 / 1024), (flg & MSS_SPACE) ? " " : "");
		}
		return	sprintf(buf, "%d.%d%sMB",
			(int)(size / 1024 / 1024),
			(int)((size * 10 / 1024 / 1024) % 10),
			(flg & MSS_SPACE) ? " " : "");
	}
	return	sprintf(buf, "%d%sKB",
		(int)(ALIGN_BLOCK(size, 1024)),
		(flg & MSS_SPACE) ? " " : "");
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

/*
	"" を意識した wcstok_r
*/
WCHAR *quote_tok(WCHAR *buf, WCHAR separetor, WCHAR **handle)
{
	WCHAR	*_handle;

	if (handle == NULL) {
		handle = &_handle;
	}
	if (buf) {
		*handle = buf;
	}
	if (*handle == NULL || **handle == 0) {
		return	NULL;
	}

	while (**handle == separetor) {	// 頭出し
		(*handle)++;
	}
	buf = *handle;

	if (**handle == '"') {
		separetor = '"';
		buf = ++(*handle);
	}

	if (**handle == 0) {
		return	NULL;
	}

	while (**handle && **handle != separetor) {
		(*handle)++;
	}

	if (**handle == separetor) {
		*(*handle)++ = 0;
	}

	return	buf;
}

Addr ResolveAddr(const char *_host)
{
	Addr	addr(_host);

	if (addr.size <= 0) {
		addr.Resolve(_host);
	}

	return	addr;
}

void TBrList::Reset()
{
	TBrObj	*obj;

	while ((obj = TopObj()))
	{
		DelObj(obj);
		delete obj;
	}
}

#if 0
BOOL TBrList::SetHost(const char *host)
{
	Addr	addr = ResolveAddr(host);

	if (addr == 0 || IsExistHost(host))
		return	FALSE;

	SetHostRaw(host, addr);
	return	TRUE;
}

BOOL TBrList::IsExistHost(const char *host)
{
	for (auto obj=Top(); obj; obj=Next(obj))
		if (stricmp(obj->Host(), host) == 0)
			return	TRUE;

	return	FALSE;
}

#endif

char *LoadStrAsFilterU8(UINT id)
{
	char	*ret = LoadStrU8(id);
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

	POINT	pt;
	::GetCursorPos(&pt);

	HMONITOR	hMon = hRefWnd ?
		::MonitorFromWindow(hRefWnd, MONITOR_DEFAULTTONEAREST) :
		::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	if (hMon) {
		MONITORINFO	info = { sizeof(MONITORINFO) };

		if (::GetMonitorInfo(hMon, &info))
			*rect = info.rcMonitor;
	}

	return	TRUE;
}

#define IPMSG_CLIPBOARD_PSEUDOFILE	"ipmsgclip_%s_%d_%d.png"
#define IPMSG_CLIPBOARD_STOREDIR	"ipmsg_img"

void MakeClipFileName(int id, int pos, int prefix, char *buf)
{
	sprintf(buf, IPMSG_CLIPBOARD_PSEUDOFILE,
		prefix == 0 ? "r" :
		prefix == 1 ? "s" : "c", id, pos);
}

BOOL MakeImageFolderName(Cfg *cfg, char *dir)
{
	cfg->GetBaseDir(dir);
	AddPathU8(dir, IPMSG_CLIPBOARD_STOREDIR);
	return	TRUE;
}

BOOL MakeImageTmpFileName(const char *dir, char *fname)
{
	SYSTEMTIME	st;
	::GetLocalTime(&st);

	sprintf(fname, "img_%04d%02d%02d.png", st.wYear, st.wMonth, st.wDay);

	return	MakeNonExistFileName(dir, fname);
}

BOOL MakeNonExistFileName(const char *dir, char *fname)
{
	char	buf[MAX_PATH_U8];
	char	ext[MAX_PATH_U8];

	char	*body_end = strrchr(fname, '.');
	if (body_end) {
		strncpyz(ext, body_end, sizeof(ext));
	} else {
		*ext = 0;
		body_end = fname + strlen(fname);
	}

	for (int i=1; i < 10000; i++) {
		MakePathU8(buf, dir, fname);
		if (GetFileAttributesU8(buf) == 0xffffffff) {
			return TRUE;
		}
		sprintf(body_end, "(%d)%s", i, ext);
	}
	return	FALSE;
}

BOOL MakeAutoSaveDir(Cfg *cfg, char *dir)
{
	if (*cfg->autoSaveDir) {
		strcpy(dir, cfg->autoSaveDir);
	} else {
		cfg->GetBaseDir(dir);
		AddPathU8(dir, "AutoSave");
	}
	return	TRUE;
}

int MakeDownloadLinkRootDirW(Cfg *cfg, WCHAR *dir)
{
	cfg->GetBaseDirW(dir);

	return	AddPathW(dir, L"ipmsg_dllinks");
}

BOOL MakeDownloadLinkDirW(Cfg *cfg, time_t t, WCHAR *dir)
{
	int	len = MakeDownloadLinkRootDirW(cfg, dir);

	struct tm *tm = localtime(&t);

	swprintf(dir + len, L"\\%04d\\%02d", tm->tm_year+1900, tm->tm_mon+1);

	return	TRUE;
}

BOOL MakeDownloadLinkW(Cfg *cfg, const WCHAR *fname, time_t t, BOOL is_recv, WCHAR *link)
{
#define DLLINK_MAX	(MAX_PATH - 80)
	MakeDownloadLinkDirW(cfg, t, link);
	int len = AddPathW(link, fname, DLLINK_MAX);

	swprintf(link + len, L"_%llx_%c.lnk", t, is_recv ? 'r' : 's');
	return	TRUE;
}

BOOL CreateDownloadLinkW(Cfg *cfg, const WCHAR *link, const WCHAR *target, time_t t, BOOL is_recv)
{
	WCHAR	link_path[MAX_PATH];

	MakeDownloadLinkDirW(cfg, t, link_path);
	MakeDirAllW(link_path);

	MakeDownloadLinkW(cfg, link, t, is_recv, link_path);

	BOOL ret = ShellLinkW(target, link_path, NULL, target);

	if (ret) {
		size_t len = wcslen(target);
		if (len > 4 && wcsicmp(target + len - 4, L".lnk") == 0) {
			UpdateLinkW(link_path, NULL, target);
		}
	}
	return	ret;
}

BOOL CreateDownloadLinkU8(Cfg *cfg, const char *link, const char *target, time_t t, BOOL is_recv)
{
	Wstr	wlink(link);
	Wstr	wtarg(target);

	return	CreateDownloadLinkW(cfg, wlink.s(), wtarg.s(), t, is_recv);
}

BOOL ConfirmDownloadLinkW(Cfg *cfg, const WCHAR *link_path, BOOL is_update, WCHAR *targ,
	WCHAR *linked_targ)
{
	UpdateLinkW(link_path, NULL, NULL, SLR_NO_UI|SLR_NOSEARCH|(is_update ? SLR_UPDATE : 0));

	WCHAR	wbuf[MAX_PATH];
	WCHAR	desc[INFOTIPSIZE];

	if (!targ) {
		targ = wbuf;
	}

	if (!ReadLinkW(link_path, targ, NULL, desc)) {
		return	FALSE;
	}
	if (linked_targ) {
		size_t	len = wcslen(desc);
		if (len > 4 && wcsicmp(desc + len - 4, L".lnk") == 0 && wcschr(desc, '\\')) {
			if (::GetFileAttributesW(desc) != 0xffffffff) {
				wcsncpyz(linked_targ, desc, MAX_PATH);
			}
		}
	}

	return	::GetFileAttributesW(targ) != 0xffffffff;
}

BOOL MakeImagePath(Cfg *cfg, const char *fname, char *path)
{
	if (!MakeImageFolderName(cfg, path)) {
		return FALSE;
	}
	AddPathU8(path, fname, MAX_PATH_U8);
	return	TRUE;
}

BOOL SaveImageFile(Cfg *cfg, const char *fname, VBuf *buf)
{
	char	path[MAX_PATH_U8];
	DWORD	size;
	HANDLE	hFile;

	if (!MakeImagePath(cfg, fname, path)) {
		return FALSE;
	}

	if ((hFile = CreateFileWithDirU8(path, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) return FALSE;

	WriteFile(hFile, buf->Buf(), (DWORD)buf->Size(), &size, 0);
	CloseHandle(hFile);

	return	TRUE;
}

VBuf *LoadImageFile(Cfg *cfg, const char *fname)
{
	VBuf *vbuf = new VBuf();

	if (!LoadImageFile(cfg, fname, vbuf)) {
		delete vbuf;
		vbuf = NULL;
	}
	return vbuf;
}

BOOL LoadImageFile(Cfg *cfg, const char *fname, VBuf *vbuf)
{
	char	path[MAX_PATH_U8] = "";
	HANDLE	hFile = INVALID_HANDLE_VALUE;
	DWORD	size, high;
	BOOL	ret = FALSE;

	if (cfg) {
		if (!MakeImageFolderName(cfg, path)) {
			return FALSE;
		}
	}
	AddPathU8(path, fname, MAX_PATH_U8);

	if ((hFile = CreateFileU8(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		/*FILE_FLAG_NO_BUFFERING|*/FILE_FLAG_SEQUENTIAL_SCAN, 0)) == INVALID_HANDLE_VALUE)
		return ret;

	if ((size = ::GetFileSize(hFile, &high)) <= 0 || high > 0) goto END; // I don't know over 2GB.

	vbuf->AllocBuf(size);
	ret = vbuf && vbuf->Buf() && ReadFile(hFile, vbuf->Buf(), size, &size, 0);

END:
	CloseHandle(hFile);
	return	ret;
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


BOOL GenUserNameDigestVal(const BYTE *key, BYTE *digest)
{
	TDigest	d;
	BYTE	data[SHA1_SIZE + 8];

	uint64	*in1 = (uint64 *)(data +  0);
	uint64	*in2 = (uint64 *)(data +  8);
	uint64	*in3 = (uint64 *)(data + 16);
	uint64	*out = (uint64 *)(digest);

	*in3 = 0; // 160bitを超える領域は初期化が必要

	if (!d.Init() || !d.Update((void *)key, 2048/8) || !d.GetRevVal((void *)data)) return FALSE;

	*out = *in1 ^ *in2 ^ *in3;
	return	TRUE;
}

BOOL GenUserNameDigest(char *org_name, const BYTE *key, char *new_name)
{
	if (org_name != new_name) {
		strncpyz(new_name, org_name, MAX_NAMEBUF);
	}

	BYTE	val[8];
	int		len = (int)strlen(new_name);

	if (!GenUserNameDigestVal(key, val)) {
		return	FALSE;
	}

	if (len + 20 > MAX_NAMEBUF) {
		len = MAX_NAMEBUF - 20;
	}

	new_name += len;
	*new_name++ = '-';
	*new_name++ = '<';
	new_name += bin2hexstr(val, 8, new_name);
	*new_name++ = '>';
	*new_name   = 0;

	return	TRUE;
}

const char *GetUserNameDigestField(const char *user)
{
	const char	*p = NULL;
	char		c;
	int			len = 0;
	int			state = 0;

	while ((c = *user)) {
		switch (state) {
		case 0:
			if (c == '-') {
				len = 0;
				state = 1;
				p = user;
			}
			break;

		case 1:
			if (c == '<')	state = 2;
			else			state = 0;
			break;

		case 2:
			if (c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z') {
				len++;
			}
			else if (len == 16 && c == '>') {
				state = 3;
			}
			else {
				state = 0;
			}
			break;
		}
		user++;
	}

	return	state == 3 ? p : NULL;
}

/*
  1: success
  0: verify_error
 -1: no_digest_user
*/
int VerifyUserNameDigest(const char *user, const BYTE *key)
{
	BYTE		val1[8];
	BYTE		val2[8];
	size_t		len = 0;
	const char	*p;

	if (!(p = GetUserNameDigestField(user)) || (len=hexstr2bin(p+2, val1, 8)) != 8) {
		return -1;
	}

	if (!GenUserNameDigestVal(key, val2)) {
		return -1;
	}

	return	memcmp(val1, val2, 8) == 0 ? 1 : 0;
}

/*
*/
BOOL VerifyUserNameExtension(Cfg *cfg, MsgBuf *msg)
{
	return	!IsUserNameExt(cfg) ||
			!GetUserNameDigestField(msg->hostSub.u.userName) || (msg->command & IPMSG_ENCRYPTOPT)
			? TRUE : FALSE;
}

BOOL IsUserNameExt(Cfg *cfg)
{
	return	cfg->pub[KEY_2048].Capa() ? TRUE : FALSE;
}

void MsgToHost(const MsgBuf *msg, Host *host, time_t t)
{
	host->hostSub = msg->hostSub;

	strncpyz(host->nickName, msg->nick.s(), sizeof(host->nickName));
	strncpyz(host->groupName, msg->group.s(), sizeof(host->groupName));
	strncpyz(host->verInfoRaw, msg->verInfoRaw, sizeof(host->verInfoRaw));
	host->hostStatus = GET_OPT(msg->command) & IPMSG_ALLSTAT;
	host->updateTime = t ? t : time(NULL);
	host->priority = DEFAULT_PRIORITY;

	host->active = TRUE;

//	host->updateTime = 0;
//	host->updateTimeDirect = 0;
//	h.updateTimeDirect;
//	priority = h.priority;
//	//refCnt = h.refCnt;
//	pubKey = h.pubKey;
//	agent = h.agent;
}

BOOL DictPubToHost(IPDict *dict, Host *host)
{
	int64	capa;
	int64	e;
	DynBuf	key;

	if (!dict->get_int(IPMSG_ENCCAPA_KEY, &capa)
	 || !dict->get_int(IPMSG_PUB_E_KEY, &e)
	 || !dict->get_bytes(IPMSG_PUB_N_KEY, &key)) {
		return	FALSE;
	}
	swap_s(key.Buf(), key.UsedSize());

	if (!VerifyUserNameDigest(host->hostSub.u.userName, key.Buf())) {
		return	FALSE;
	}

	host->pubKey.Set(key, (int)key.UsedSize(), (int)e, (int)capa);
	return	TRUE;
}

BOOL NeedUpdateHost(Host *h1, Host *h2)
{
	if (h1->hostStatus != h2->hostStatus
	 || !IsSameHostEx(&h1->hostSub, &h2->hostSub)
	 || strcmp(h1->nickName, h2->nickName)
	 || strcmp(h1->groupName, h2->groupName)
	 || strcmp(h1->verInfoRaw, h2->verInfoRaw)) return TRUE;

	return	FALSE;
}

size_t MakeHostDict(Host *host, std::shared_ptr<IPDict> dict)
{
	char	tmp_host[MAX_BUF];

	host->hostSub.addr.S(tmp_host);

	dict->put_str(IPMSG_UID_KEY, host->hostSub.u.userName);
	dict->put_str(IPMSG_HOST_KEY, host->hostSub.u.hostName);
	dict->put_int(IPMSG_STAT_KEY, host->hostStatus);
	dict->put_int(IPMSG_ACTIVE_KEY, host->active);
	dict->put_str(IPMSG_IPADDR_KEY, tmp_host);
	dict->put_int(IPMSG_PORT_KEY, host->hostSub.portNo);
	dict->put_str(IPMSG_NICK_KEY, host->nickName);
	dict->put_str(IPMSG_GROUP_KEY, host->groupName);
	dict->put_str(IPMSG_CLIVER_KEY, host->verInfoRaw);

	return	dict->pack_size();
}

BOOL DictToHost(std::shared_ptr<IPDict> dict, Host *host)
{
	U8str	u8;
	int64	val;

	host->Init();

	if (!dict->get_str(IPMSG_UID_KEY, &u8)) return FALSE;
	strncpyz(host->hostSub.u.userName, u8.s(), MAX_NAMEBUF);

	if (!dict->get_str(IPMSG_HOST_KEY, &u8)) return FALSE;
	strncpyz(host->hostSub.u.hostName, u8.s(), MAX_NAMEBUF);

	if (!dict->get_int(IPMSG_STAT_KEY, &val)) return FALSE;
	host->hostStatus = (ULONG)val;

	if (dict->get_int(IPMSG_ACTIVE_KEY, &val)) {
		host->active = val ? true : false;
	}

	if (!dict->get_str(IPMSG_IPADDR_KEY, &u8)) return FALSE;
	if (!host->hostSub.addr.Set(u8.s())) return FALSE;

	if (!dict->get_int(IPMSG_PORT_KEY, &val)) return FALSE;
	host->hostSub.portNo = (int)val;

	if (dict->get_str(IPMSG_NICK_KEY, &u8)) {
		strncpyz(host->nickName, u8.s(), MAX_NAMEBUF);
	}
	if (dict->get_str(IPMSG_GROUP_KEY, &u8)) {
		strncpyz(host->groupName, u8.s(), MAX_NAMEBUF);
	}

	if (dict->get_str(IPMSG_CLIVER_KEY, &u8)) {
		strncpyz(host->verInfoRaw, u8.s(), MAX_VERBUF);
	}

	return	TRUE;
}

int MakeHostListStr(char *buf, Host *host)
{
	char	tmp_host[MAX_BUF];

	host->hostSub.addr.S(tmp_host);

	return	sprintf(buf, "%s%c%s%c%d%c%s%c%d%c%s%c%s%s%s%c",
		host->hostSub.u.userName, HOSTLIST_SEPARATOR,
		host->hostSub.u.hostName, HOSTLIST_SEPARATOR,
		host->hostStatus, HOSTLIST_SEPARATOR,
		tmp_host, HOSTLIST_SEPARATOR,
		::htons(host->hostSub.portNo), HOSTLIST_SEPARATOR,
		*host->nickName ? host->nickName : HOSTLIST_DUMMY, HOSTLIST_SEPARATOR,
		*host->groupName ? host->groupName : HOSTLIST_DUMMY,
		"", "", HOSTLIST_SEPARATOR);
}


/*
	logview
*/
BOOL MakeClipPathW(WCHAR *clip_path, Cfg *cfg, const WCHAR *fname)
{
	char	path[MAX_PATH_U8];

	if (MakeImageFolderName(cfg, path)) {
		WCHAR	clip_dir[MAX_PATH];

		U8toW(path, clip_dir, wsizeof(clip_dir));
		MakePathW(clip_path, clip_dir, fname);
		return	TRUE;
	}
	return	FALSE;
}


BOOL PrepareBmp(int cx, int cy, int *_aligned_line_size, VBuf *vbuf)
{
	int		&aligned_line_size = *_aligned_line_size;

	aligned_line_size = ALIGN_SIZE(cx * 3, 4);
	if (!vbuf->AllocBuf(aligned_line_size * cy + sizeof(BITMAPINFOHEADER))) return NULL;

	BITMAPINFO	*bmi = (BITMAPINFO *)vbuf->Buf();

	bmi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biSizeImage   = aligned_line_size * cy;
	bmi->bmiHeader.biWidth       = cx;
	bmi->bmiHeader.biHeight      = cy;
	bmi->bmiHeader.biPlanes      = 1;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biBitCount    = 24;

	vbuf->SetUsedSize(bmi->bmiHeader.biSize);

	return	TRUE;
}

HBITMAP FinishBmp(VBuf *vbuf)
{
	HBITMAP	hBmp  = NULL;
	HWND	hWnd  = GetDesktopWindow();
	HDC		hDc   = NULL;

	if ((hDc = GetDC(hWnd))) {
		hBmp = CreateDIBitmap(hDc, (BITMAPINFOHEADER *)vbuf->Buf(), CBM_INIT,
				  vbuf->UsedEnd(), (BITMAPINFO *)vbuf->Buf(), DIB_RGB_COLORS);
		ReleaseDC(hWnd, hDc);
	}
	return	hBmp;
}

BOOL SetFileButton(TDlg *dlg, int buttonID, ShareInfo *info, const char *auto_saved)
{
	char	buf[MAX_BUF] = "";
	char	fname[MAX_PATH_U8] = "";
	int		offset = 0;
	BOOL	is_autosaved = (auto_saved && *auto_saved) ? TRUE : FALSE;

	for (int i=0; i < info->fileCnt; i++) {
		if (dlg->ResId() == SEND_DIALOG)
			ForcePathToFname(info->fileInfo[i]->Fname(), fname);
		else
			strncpyz(fname, info->fileInfo[i]->Fname(), MAX_PATH_U8);
		offset += sprintf(buf + offset, "%s ", fname);
		if (offset + MAX_PATH_U8 >= sizeof(buf))
			break;
	}
	if (is_autosaved) {
		char	disp[MAX_BUF] = "";
		char	tmp[MAX_BUF];

		// 内部表現→表示表現変換 (ID=fname:...)
		strncpyz(tmp, auto_saved, sizeof(tmp));
		for (char *p=strtok(tmp, ":"); p; p=strtok(NULL, ":")) {
			char *equal = strchr(p, '=');
			if (equal) {
				if (*disp) strncatz(disp, " ", sizeof(disp));
				strncatz(disp, equal+1, sizeof(disp));
			}
		}
		if (info->fileCnt > 0) {
			strncatz(buf, LoadStrU8(IDS_AUTOSAVEPARTIAL), sizeof(buf));
		} else {
			strcpyz(buf, LoadStrU8(IDS_AUTOSAVEDONE));
		}
		strncatz(buf, disp, sizeof(buf));
	}

	dlg->SetDlgItemTextU8(buttonID, buf);
	::ShowWindow(dlg->GetDlgItem(buttonID), info->fileCnt || is_autosaved ? SW_SHOW : SW_HIDE);
	::EnableWindow(dlg->GetDlgItem(buttonID), info->fileCnt || is_autosaved ? TRUE : FALSE);
	return	TRUE;
}

BOOL IsImageInClipboard(HWND hWnd, UINT *cf_type)
{
	if (!OpenClipboard(hWnd)) return FALSE;

	BOOL ret = FALSE;
	UINT fmt = 0;

	while ((fmt = EnumClipboardFormats(fmt))) {
		if (fmt == CF_BITMAP || fmt == CF_DIB || fmt == CF_DIBV5) {
			if (cf_type) {
				*cf_type = fmt;
			}
			ret = TRUE;
			break;
		}
	}

	CloseClipboard();
	return	ret;
}

BOOL IsTextInClipboard(HWND hWnd)
{
	if (!OpenClipboard(hWnd)) return FALSE;

	BOOL ret = FALSE;
	UINT fmt = 0;

	while ((fmt = EnumClipboardFormats(fmt))) {
		if (fmt == CF_TEXT || fmt == CF_UNICODETEXT) {
			ret = TRUE;
			break;
		}
	}

	CloseClipboard();
	return	ret;
}

int CALLBACK EditNoWordBreakProc(LPTSTR str, int cur, int len, int action)
{
	switch (action) {
	case WB_ISDELIMITER:
		return	0;
	}
	return	cur;
}

void GenRemoteKey(char *key)
{
	BYTE	buf[REMOTE_KEYLEN];

	TGenRandom(buf, REMOTE_KEYLEN);
	bin2urlstr(buf, REMOTE_KEYLEN, key);
}

static WCHAR *DW[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat", 0 };
//static WCHAR *MN[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug",
//	L"Sep", L"Oct", L"Nov", L"Dec", 0 };

int MakeDateStr(time_t t, WCHAR *buf, DWORD flags)
{
	struct tm *tm = localtime(&t);
	if (!tm) {
		return -1;
	}

	int len = swprintf(buf, L"%04d/%02d/%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);

	if (flags & MDS_WITH_DAYWEEK) {
		len += swprintf(buf + len, L" (%s)", DW[tm->tm_wday]);
	}

	return	len + swprintf(buf + len,
		(flags & MDS_WITH_SEC) ? L" %02d:%02d:%02d" : L" %02d:%02d",
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// 可変長日付
// 2015/01/02 12:34  昨年以前
// 01/02 12:34       昨日以前
// 12:34             今日
int MakeDateStrEx(time_t t, WCHAR *buf, SYSTEMTIME *lt)
{
	struct tm *tm = localtime(&t);
	if (!tm) {
		return -1;
	}

	const WCHAR *fmt = L"%04d/%02d/%02d %02d:%02d";
//	const WCHAR	*dw = DW[tm->tm_wday];
//	const WCHAR	*mn = MN[tm->tm_mon];

	if (lt) {
//		if (tm->tm_year+1900 >= lt->wYear -2 && (tm->tm_mon <= 5 || tm->tm_mon >= 10)) {
		if (tm->tm_year+1900 >= lt->wYear) {
			if (tm->tm_mon+1 == lt->wMonth && tm->tm_mday == lt->wDay) {
				fmt = L"%02d:%02d";
				return	swprintf(buf, fmt,
					tm->tm_hour, tm->tm_min, tm->tm_sec);
			}
			fmt = L"%02d/%02d %02d:%02d";
			return	swprintf(buf, fmt,
				tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
		}
	}

	return	swprintf(buf, fmt,
		tm->tm_year+1900, tm->tm_mon+1,  tm->tm_mday, tm->tm_hour, tm->tm_min);
}

int MakeDateStrEx(time_t t, WCHAR *buf, time_t lt)
{
	SYSTEMTIME	st;
	time_to_SYSTEMTIME(lt, &st);

	return	MakeDateStrEx(t, buf, &st);
}

int get_linenum(const WCHAR *s)
{
	int	lines = 0;

	while (const WCHAR *p = wcschr(s, '\n')) {
		lines++;
		s = p + 1;
	}
	if (*s) {
		lines++;
	}

	return	lines;
}

const WCHAR *wcschr_n(const WCHAR *s, WCHAR ch, int max_len)
{
	const WCHAR	*end = s + max_len;

	for ( ; s < end && *s; s++) {
		if (*s == ch) {
			return s;
		}
	}
	return	NULL;
}

int get_linenum_n(const WCHAR *s, int max_len)
{
	int	lines = 0;

	while (const WCHAR *p = wcschr_n(s, '\n', max_len)) {
		lines++;
		s = p + 1;
	}
	if (*s) {
		lines++;
	}

	return	lines;
}

ssize_t comma_int64(WCHAR *s, int64 val)
{
	WCHAR	tmp[40], *sv_s = s;
	ssize_t	len = swprintf(tmp, L"%lld", val);

	for (WCHAR *p=tmp; *s++ = *p++; ) {
		if (len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

ssize_t comma_double(WCHAR *s, double val, int precision)
{
	WCHAR	tmp[40], *sv_s = s;
	ssize_t	len = swprintf(tmp, L"%.*f", precision, val);
	WCHAR	*pos = precision ? wcschr(tmp, '.') : NULL;

	if (pos) len = pos - tmp;

	for (WCHAR *p=tmp; *s++ = *p++; ) {
		if ((!pos || p < pos) && len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

ssize_t comma_int64(char *s, int64 val)
{
	char	tmp[40], *sv_s = s;
	ssize_t	len = sprintf(tmp, "%lld", val);

	for (char *p=tmp; *s++ = *p++; ) {
		if (len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}

ssize_t comma_double(char *s, double val, int precision)
{
	char	tmp[40], *sv_s = s;
	ssize_t	len = sprintf(tmp, "%.*f", precision, val);
	char	*pos = precision ? strchr(tmp, '.') : NULL;

	if (pos) len = pos - tmp;

	for (char *p=tmp; *s++ = *p++; ) {
		if ((!pos || p < pos) && len > 2 && (--len % 3) == 0) *s++ = ',';
	}
	return	s - sv_s - 1;
}



BOOL GetPngDimension(WCHAR *fname, TSize *sz)
{
	HANDLE	fh = ::CreateFileW(fname, GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

	if (fh == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}
	BOOL	ret = FALSE;
	BYTE	data[24];
	DWORD	size = 0;

	if (::ReadFile(fh, data, sizeof(data), &size, 0) && size == sizeof(data)) {
		BYTE	png_sig[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
		if (memcmp(data, png_sig, sizeof(png_sig)) == 0) {
			sz->cx = ntohl(*(long *)(data + 16));
			sz->cy = ntohl(*(long *)(data + 20));
			ret = TRUE;
		}
	}

	::CloseHandle(fh);
	return	ret;
}

BOOL IsNetVolume(const char *path)
{
	char	root[4];

	if (strncpyz(root, path, 4) < 3) {
		return	FALSE;
	}
	if (root[1] != ':' || root[2] != '\\' || ::GetDriveTypeU8(root) == DRIVE_REMOTE) {
		return	TRUE;
	}
	return	FALSE;
}

void TruncateMsg(char *msg, BOOL is_mbcs, int max_len)
{
	if (is_mbcs) {
		BOOL	is_lead = FALSE;
		char	*end = msg + max_len;

		for ( ; *msg && msg < end; msg++) {
			if (is_lead) {
				is_lead = FALSE;
			} else {
				is_lead = IsDBCSLeadByte(*msg);
			}
		}
		if (is_lead && msg == end) {
			msg[-1] = 0;
		}
	}
	else {
		char	*invalid_ptr = NULL;
		int		len = (int)strlen(msg);

		if (len >= max_len) {
			msg[max_len-1] = 0;
		}
		if (!IsUTF8(msg, 0, &invalid_ptr) && invalid_ptr) {
			*invalid_ptr = 0;
		}
	}
}

//BOOL InitQuoteCombo(TDlg *dlg, int combo_id, int init_idx)
//{
//	SendDlgItemMessage(combo_id, CB_ADDSTRING, 0, (LPARAM)"Normal Quote");
//	SendDlgItemMessage(combo_id, CB_ADDSTRING, 0, (LPARAM)"Forward Quote");
//
//	SendDlgItemMessage(QUOTE_COMBO, CB_SETCURSEL, , init_idx);
//}

inline bool is_quote_str(const WCHAR *s, const WCHAR *special_quote, int special_quote_len)
{
	return	(*s == '>' || (special_quote && wcsncmp(s, special_quote, special_quote_len) == 0));
}

int FindTailQuoteIdx(Cfg *cfg, const Wstr& wstr, int *_top_pos)
{
	const WCHAR	*s = wstr.s();
	const WCHAR	*e = s + wstr.Len();
	int			pos = 0;
	int			top_pos = 0;
	bool		first = true;
	const WCHAR	*special_quote = wcscmp(cfg->QuoteStrW, L">") ? cfg->QuoteStrW : NULL;
	int			quote_len = special_quote ? (int)wcslen(cfg->QuoteStrW) : 0;

	for (const WCHAR *p=s; p && *p; ) {
		const WCHAR	*n = p < e ? wcschr(p, '\n') : NULL;
		if (n) n++;

		if (first) {
			bool is_quote = is_quote_str(p, special_quote, quote_len);

			if (is_quote) {
				if (n) {
					top_pos = (int)(n - s);
					if (s[top_pos] == '\r' && s[top_pos+1] == '\n') {
						top_pos += 2;
					}
				}
			}
			else if (*p != '\r' && *p != '\n') { // 先頭改行を過ぎたら、本文と見なす
				first = false;
			}
		}
		else {
			if (pos) {
				if (*p != '\n' && *p != '\r' && !is_quote_str(p, special_quote, quote_len)) {
					pos = 0;
				}
			}
			else {
				if (is_quote_str(p, special_quote, quote_len)) {
					pos = int(p - s);
				}
			}
		}
		p = n;
	}
	if (_top_pos) {
		*_top_pos = top_pos;
	}

	return	pos;
}

BOOL TruncTailQuote(Cfg *cfg, const char *src, char *dst, int max_dst)
{
	Wstr	wstr(src);

	int	top_pos = 0;
	int	pos = FindTailQuoteIdx(cfg, wstr, &top_pos);
	int	end = pos ? pos : wstr.Len();

	while (end >= 4 && wcsncmp(&wstr[end-4], L"\r\n\r\n", 4) == 0) {
		end -= 2;	// 末尾改行を削除
	}
	while (top_pos < end && wcsncmp(&wstr[top_pos], L"\r\n", 2) == 0) {
		top_pos += 2;	// 先頭改行を削除
	}

	if (top_pos > 0) {
		memmove(wstr.Buf(), &wstr[top_pos], (wstr.Len() - top_pos + 1) * sizeof(WCHAR));
		if (end >= top_pos) {
			end -= top_pos;
		}
		Debug("top_pos=%d pos=%d\n", top_pos, end);
	}

	WtoU8(wstr.s(), dst, max_dst, end > 0 ? end : -1);

	return	TRUE;
}


BOOL IsWritableDirW(const WCHAR *dir)
{
	WCHAR	path[MAX_PATH];
	int64	dat;
	TGenRandomMT(&dat, sizeof(dat));

	MakePathW(path, dir, FmtW(L"ipmsg-temp-%llx", dat));

	HANDLE	hFile = CreateFileW(path, GENERIC_WRITE,
								 FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
								 NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (hFile == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}
	::CloseHandle(hFile);
	for (int i=0; i < 10; i++) {
		if (::DeleteFileW(path)) {
			break;
		}
		Sleep(200);
	}
	return	TRUE;
}


void SlackMakeJson(LPCSTR chan, LPCSTR _user, LPCSTR _body, LPCSTR _icon, U8str *json)
{
	U8str	body;
	U8str	user;
	U8str	icon;

	EscapeForJson(_body, &body);
	EscapeForJson(_user, &user);
	EscapeForJson(_icon, &icon);

	int		json_size = (int)strlen(chan) + body.Len() + user.Len() + icon.Len() + MAX_BUF;
	json->Init(json_size);

	snprintfz(json->Buf(), json_size,
			"{ "
				"\"channel\" : \"%s\", "
				"\"username\" : \"%s\", "
				"\"text\" : \"%s\", "
//				"\"icon_emoji\" : \":%s:\" "
				"\"icon_url\" : \"%s\" "
			" }",
		chan, user.s(), body.s(), icon.s());
}

BOOL SlackRequest(LPCSTR host, LPCSTR _path, LPCSTR json, DynBuf *reply, U8str *errMsg)
{
	U8str	path = "/services/";
	path += _path;

	DWORD	code = TInetRequest(host, path.s(), (BYTE *)json, (int)strlen(json), 
		reply, errMsg, INETREQ_SECURE);

	return	 code >= 200 && code < 300 ? TRUE : FALSE;
}

void SlackRequestAsync(LPCSTR host, LPCSTR _path, LPCSTR json, HWND hWnd, UINT uMsg, int64 id)
{
	U8str	path = "/services/";
	path += _path;

	TInetRequestAsync(host, path.s(), (BYTE *)json, (int)strlen(json),
		hWnd, uMsg, id, INETREQ_SECURE);
}

