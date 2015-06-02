static char *miscfunc_id = 
	"@(#)Copyright (C) H.Shirouzu 2011   miscfunc.cpp	Ver3.20";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc functions
	Create					: 2011-05-03(Tue)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include "blowfish.h"

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
	ログ記録/ウィンドウ表示用の HostEntry表示文字列
*/
void MakeListString(Cfg *cfg, HostSub *hostSub, THosts *hosts, char *buf, BOOL is_log)
{
	Host	*host;

	if ((host = hosts->GetHostByAddr(hostSub)) && IsSameHost(hostSub, &host->hostSub)
		 || GetUserNameDigestField(hostSub->userName) &&
		 	(host = cfg->priorityHosts.GetHostByName(hostSub)))
		MakeListString(cfg, host, buf, is_log);
	else {
		Host	host;

		memset(&host, 0, sizeof(host));
		host.hostSub = *hostSub;
		MakeListString(cfg, &host, buf, is_log);
	}
}

/*
	ログ記録/ウィンドウ表示用の HostEntry表示文字列
*/
void MakeListString(Cfg *cfg, Host *host, char *buf, BOOL is_log)
{
	BOOL	logon  = is_log ? cfg->LogonLog    : cfg->RecvLogonDisp;
	BOOL	ipaddr = is_log ? cfg->IPAddrCheck : cfg->RecvIPAddr;

	buf += wsprintf(buf, "%s ", *host->nickName ? host->nickName : host->hostSub.userName);

	if (host->hostStatus & IPMSG_ABSENCEOPT) *buf++ = '*';

	buf += wsprintf(buf, "(%s%s%s", host->groupName,
					*host->groupName ? "/" : "", host->hostSub.hostName);

	if (ipaddr) buf += wsprintf(buf, "/%s", inet_ntoa(*(LPIN_ADDR)&host->hostSub.addr));
	if (logon)  buf += wsprintf(buf, "/%s", host->hostSub.userName);

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

BOOL GetLastErrorMsg(char *msg, TWin *win)
{
	char	buf[MAX_BUF];
	wsprintf(buf, "%s error = %x", msg ? msg : "", GetLastError());
	return	MessageBox(win ? win->hWnd : NULL, buf, IP_MSG, MB_OK);
}

BOOL GetSockErrorMsg(char *msg, TWin *win)
{
	char	buf[MAX_BUF];
	wsprintf(buf, "%s error = %d", msg ? msg : "", WSAGetLastError());
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
			MONITORINFO	info = { sizeof(MONITORINFO) };

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


BOOL GenUserNameDigestVal(const BYTE *key, BYTE *digest)
{
	TDigest	d;
	BYTE	data[SHA1_SIZE + 8];

	unsigned _int64	*in1 = (unsigned _int64 *)(data +  0);
	unsigned _int64	*in2 = (unsigned _int64 *)(data +  8);
	unsigned _int64	*in3 = (unsigned _int64 *)(data + 16);
	unsigned _int64	*out = (unsigned _int64 *)(digest);

	*in3 = 0; // 160bitを超える領域は初期化が必要

	if (!d.Init() || !d.Update((void *)key, 2048/8) || !d.GetRevVal((void *)data)) return FALSE;

	*out = *in1 ^ *in2 ^ *in3;
	return	TRUE;
}

BOOL GenUserNameDigest(char *org_name, const BYTE *key, char *new_name)
{
	if (org_name != new_name) strncpyz(new_name, org_name, MAX_NAMEBUF);

	BYTE	val[8];
	int		len = strlen(new_name);

	if (!GenUserNameDigestVal(key, val)) return	FALSE;

	if (len + 20 > MAX_NAMEBUF) len = MAX_NAMEBUF - 20;

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
	int			len = 0;
	const char	*p;

	if (!(p = GetUserNameDigestField(user)) || !hexstr2bin(p+2, val1, 8, &len) || len != 8) {
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
			!GetUserNameDigestField(msg->hostSub.userName) || (msg->command & IPMSG_ENCRYPTOPT)
			? TRUE : FALSE;
}

BOOL IsUserNameExt(Cfg *cfg)
{
	return	cfg->pub[KEY_2048].Capa() ? TRUE : FALSE;
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
				  vbuf->Buf() + vbuf->UsedSize(), (BITMAPINFO *)vbuf->Buf(), DIB_RGB_COLORS);
		ReleaseDC(hWnd, hDc);
	}
	return	hBmp;
}

BOOL SetFileButton(TDlg *dlg, int buttonID, ShareInfo *info)
{
	char	buf[MAX_BUF] = "", fname[MAX_PATH_U8];
	int		offset = 0;
	for (int cnt=0; cnt < info->fileCnt; cnt++)
	{
		if (dlg->ResId() == SEND_DIALOG)
			ForcePathToFname(info->fileInfo[cnt]->Fname(), fname);
		else
			strncpyz(fname, info->fileInfo[cnt]->Fname(), MAX_PATH_U8);
		offset += wsprintf(buf + offset, "%s ", fname);
		if (offset + MAX_PATH_U8 >= sizeof(buf))
			break;
	}
	dlg->SetDlgItemTextU8(buttonID, buf);
	::ShowWindow(dlg->GetDlgItem(buttonID), info->fileCnt ? SW_SHOW : SW_HIDE);
	::EnableWindow(dlg->GetDlgItem(buttonID), info->fileCnt ? TRUE : FALSE);
	return	TRUE;
}

BOOL IsImageInClipboard(HWND hWnd)
{
	if (!OpenClipboard(hWnd)) return FALSE;

	BOOL ret = FALSE;
	UINT fmt = 0;

	while ((fmt = EnumClipboardFormats(fmt))) {
		if (fmt == CF_BITMAP || fmt == CF_DIB || fmt == CF_DIBV5) {
			ret = TRUE;
			break;
		}
	}

	CloseClipboard();
	return	ret;
}

