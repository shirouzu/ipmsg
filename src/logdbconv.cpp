static char *logdb_id = 
	"@(#)Copyright (C) H.Shirouzu 2015-2019   logdbcponv.cpp	Ver4.99";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Log Converter
	Create					: 2015-04-10(Sat)
	Update					: 2019-01-12(Sat)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
using namespace std;

#include "logdbconv.h"
#include "../external/sqlite3/sqlite3.h"

BOOL GetFileInfoForLog(HANDLE fh, int64 *size, time_t *t)
{
	if (t) {
		FILETIME	ft;
		if (!GetFileTime(fh, 0, 0, &ft)) {
			return FALSE;
		}
		*t = FileTime2UnixTime(&ft);
	}

	if (size) {
		LARGE_INTEGER	li;
		if (!GetFileSizeEx(fh, &li)) {
			return	FALSE;
		}
		*size = *(int64 *)&li;
	}
	return TRUE;
}


static int AddTextLogToDbMsg(LogDb *db, AddTblData *atd, LogMsg *msg,
	const VBVec<LineInfo> &vlines, int next_idx)
{
	ParseMsgBody(msg, vlines, next_idx);

	if (db->SelectMsgIdByMsg(msg, atd->importid) > 0) {
		return	0;	// already registerd
	}
	if (!db->AddTblDataAdd(atd, msg)) {
		return	-1;
	}
	return	1;
}

BOOL AddTextLogToDb(Cfg *cfg, LogDb *db, const char *logname)
{
	TTick	tick;

	HANDLE	hFile = CreateFileU8(logname, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
									NULL, OPEN_EXISTING, 0, 0);
	time_t	ftime;
	int64	size;

	if (!GetFileInfoForLog(hFile, &size, &ftime) || size >= 0xffffffffULL) {
		::CloseHandle(hFile);
		return	FALSE;
	}

// ファイル読み取り
	HANDLE	hMap = ::CreateFileMapping(hFile, 0, PAGE_READONLY, 0, 0, NULL);
	char	*top = (char *)::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	char	*end = top + size;

	Wstr	wbuf((int)size + 1);
	VBVec<LineInfo>	vlines;
	vlines.Init(1024, max((int)size, 1024));

	const char		msg_sep[] = "=====================================";
	const size_t	msg_sep_len = sizeof(msg_sep) - 1;

	if (top) {
		char	*line_top = top;
		WCHAR	*wp = wbuf.Buf();

		int	(*conv)(const char *, WCHAR *, int, int) = 
			(int (*)(const char *, WCHAR *, int, int))U8toW;

		while (line_top < end) {
			char *line_end = (char *)memchr(line_top, '\n', end - line_top);
			if (!line_end) break;
			int	len = int(line_end - line_top);

			if (len > 0 && line_end[-1] == '\r') len--;

			if (len == msg_sep_len && memcmp(line_top, msg_sep, len) == 0) {
				char *t = line_end + 1;
				char *e = NULL;
				while (t < end) {
					e = (char *)memchr(t, '\n', end - t);
					if (!e) break;
					int l = int(e - t);
					if (l > 0 && e[-1] == '\r') l--;
					if (l == msg_sep_len && memcmp(t, msg_sep, l) == 0) {
						break;
					}
					t = e + 1;
				}
				if (!e) {
					e = end;
				}
				conv = IsUTF8(line_top, 0, 0, int(e - line_top)) ?
					(int (*)(const char *, WCHAR *, int, int))U8toW :
					(int (*)(const char *, WCHAR *, int, int))AtoW;
			}

			int	wlen = conv(line_top, wp, int(size - (wp - wbuf.s())), len);
			LineInfo	li(wp, wlen);
			vlines.Push(li);

			wp += wlen + 1;		// wlenに\0分は入っていないため
			line_top = line_end + 1;
		}
	}

	Debug("file-in=%d\n", tick.elaps());

// DBへのデータ追加
	AddTblData	atd;
	if (!db->AddTblDataBegin(&atd, U8toWs(logname), size, ftime)) {
		return	FALSE;
	}

	int		msg_idx = 0;
	LogMsg	msg_buf[2];
	LogMsg	*msg  = NULL;
	LogMsg	*next = &msg_buf[0];
	BOOL	ret   = TRUE;
	int		data_count = 0;
	int		skip_count = 0;

	for (int line_idx=0; line_idx < vlines.UsedNumInt(); ) {
		if (!ParseMsg(cfg, vlines, line_idx, next)) {
			line_idx++;
			continue;
		}
		if (msg) {
			// 次メッセージヘッダ確定＝現メッセージbody確定
			if (next->date >= msg->date) {
				int	count = AddTextLogToDbMsg(db, &atd, msg, vlines, line_idx);
				if (count == 1) {
					data_count++;
				}
				else if (count == 0) {
					skip_count++;
				}
				else {
					ret = FALSE;
					break;
				}
				msg  = next;
				next = (msg == msg_buf) ? msg_buf + 1 : msg_buf;
				line_idx = msg->line_no;
			}
			else {	// 引用されたメッセージ等と見做す
				line_idx = next->line_no;
			}
		}
		else {	// 初回
			line_idx = next->line_no;
			msg  = next;
			next = (msg == msg_buf) ? msg_buf + 1 : msg_buf;
		}
	}
	if (msg && msg->date) {
		int	count = AddTextLogToDbMsg(db, &atd, msg, vlines, vlines.UsedNumInt());
		if (count == 1) {
			data_count++;
		}
		else if (count == 0) {
			skip_count++;
		}
		else {
			ret = FALSE;
		}
	}

	if (!db->AddTblDataEnd(&atd, data_count ? LogDb::GOAHEAD : LogDb::ROLLBACK)) {
		ret = FALSE;
	}
	if (data_count == 0) {
		ret = FALSE;
	}

	Debug("db-in=%d add=%d skip=%d\n", tick.elaps(), data_count, skip_count);

	if (top) ::UnmapViewOfFile(top);
	if (hMap) ::CloseHandle(hMap);
	if (hFile != INVALID_HANDLE_VALUE) ::CloseHandle(hFile);
	return	ret;
}

static BOOL is_ipaddr(const WCHAR *_s)	// 簡易検査
{
	const WCHAR	*s = _s;
	const WCHAR *e = _s + 16;
	WCHAR		ch = 0;
	BOOL		v6_check = FALSE;
	int			sep_cnt = 0;

	// v4 check
	for ( ; (ch = *s) && s < e; s++) {
		if (ch >= '0' && ch <= '9') continue;
		if (ch == '.') {
			sep_cnt++;
			continue;
		}
		if (ch == ':' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'f') {
			v6_check = TRUE;
			break;
		}
		return FALSE;
	}
	if (!v6_check) {
		return (sep_cnt == 3 && !ch) ? TRUE : FALSE;
	}

	// v6 check
	s = _s;
	e = _s + 41;
	sep_cnt = 0;

	for ( ; (ch = *s) && s < e; s++) {
		if (ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f' || ch >= 'A' && ch <= 'f') continue;
		if (ch == ':') {
			sep_cnt++;
			continue;
		}
		return FALSE;
	}
	return (sep_cnt >= 2 && !ch) ? TRUE : FALSE;
}

static BOOL is_uid(const WCHAR *_s)	// 簡易検査
{
	const WCHAR	*s = _s;
	WCHAR		ch = 0;
	int			len = (int)wcslen(_s);

	// v3 official uid
	if (len >= 20 && s[len-19] == '-' && s[len-18] == '<' && s[len-1] == '>') return TRUE;

	for ( ; (ch = *s); s++) {
		if (ch >= 'a' && ch <= 'z') return TRUE;
	}
	return FALSE;
}

static void ParseSenderSub(LogHost *_host, WCHAR items[4][MAX_BUF], int item_cnt)
{
	LogHost		&host  = *_host;

	// items は逆順に格納されている
	if (item_cnt == 4) {	// nick (group/host/addr/uid)
		host.gname = items[3];
		host.host  = items[2];
		host.addr  = items[1];
		host.uid   = items[0];
		return;
	}
	else if (item_cnt == 1) {	// nick (host)
		host.gname = L"";
		host.host  = items[0];
		host.addr  = L"";
		host.uid   = host.nick;
		return;
	}
	if (item_cnt == 3) {
		// group/host/uid
		// group/host/addr
		// host/addr/uid
		if (is_ipaddr(items[1])) {
			host.gname = L"";
			host.host  = items[2];
			host.addr  = items[1];
			host.uid   = items[0];
		} else if (is_ipaddr(items[0])) {
			host.gname = items[2];
			host.host  = items[1];
			host.addr  = items[0];
			host.uid   = host.nick;
		} else {
			host.gname = items[2];
			host.host  = items[1];
			host.addr  = L"";
			host.uid   = items[0];
		}
	}
	if (item_cnt == 2) {
		// group/host
		// host/addr
		// host/uid
		if (is_ipaddr(items[0])) {
			host.gname = L"";
			host.host  = items[1];
			host.addr  = items[0];
			host.uid   = host.nick;
		} else if (is_uid(items[0])) {	// 誤判定やむなし
			host.gname = L"";
			host.host  = items[1];
			host.addr  = L"";
			host.uid   = items[0];
		} else {
			host.gname = items[1];
			host.host  = items[0];
			host.addr  = L"";
			host.uid   = host.nick;
		}
	}
}

BOOL ParseSender(Cfg *cfg, const LineInfo *l, LogMsg *m)
{
	LogHost		host;
	BOOL		is_cc = FALSE;

	if (l->s[1] == 'F') {		// From:
		m->flags = DB_FLAG_FROM;
	}
	else if (l->s[1] == 'T') {	// To:
		m->flags = DB_FLAG_TO;	// dummy flag
	}
	else if (m->host.size() >= 1 && (m->flags & DB_FLAG_FROM)
		&& l->s[2] == ' ' && (l->s[3] == 'T' || l->s[3] == 'C')) {
		is_cc = TRUE;
	}
	else return	FALSE;

	if (l->s[l->len-1] != ')') return false;

	int			pl_num = 0;
	const WCHAR	*p     = l->s + l->len -1;
	const WCHAR	*end   = wcsstr(l->s, L" (");
	const WCHAR *item_end = p;
	WCHAR		items[4][MAX_BUF];
	int			item_cnt = 0;
	int			item_mid = 3;

	if (!end) return FALSE;

	BOOL	is_paren = wcsstr(end+2, L" (") ? TRUE : FALSE; // " (" がさらに存在

	for ( ; p != end && (!is_paren || pl_num > 0 || *p != ' '); p--) {
		if (is_paren) {		// パーレーンの対応が不完全な場合はパースできない
			if (*p == ')') {
				pl_num++;
				continue;
			}
			if (*p == '(') {
				pl_num--;
				continue;
			}
		}
		if (*p == '/' && item_cnt < item_mid) {
			wcsncpyz(items[item_cnt], p+1, int(item_end - p));
			// IPアドレスが最終要素（＝uidは存在しない）
			if (item_cnt == 0 && is_ipaddr(items[item_cnt])) {
				item_mid--;
			}
			item_end = p;
			item_cnt++;
		}
	}

	wcsncpyz(items[item_cnt++], p+2, int(item_end-(p+1)));

	// 宛先 Cc: は From: と同じ長さ(" From: " == "   Cc: ")
	const WCHAR *nick_p = l->s + ((m->flags & DB_FLAG_FROM) ? 7 : 5);
	host.nick.Init(nick_p, int(p - nick_p));

	ParseSenderSub(&host, items, item_cnt);

	// 自分自身と同じCc:はDBに登録しない
	if (!is_cc || cfg->selfHost.uid != host.uid || cfg->selfHost.host != host.host) {
		m->host.push_back(host);
	}
	return	TRUE;
}

BOOL ParseDate(const LineInfo *l, LogMsg *m)
{
	static WCHAR *mon_dict[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug",
		L"Sep", L"Oct", L"Nov", L"Dec", 0 };

	if (memcmp(l->s, L"  at ", sizeof(WCHAR)*5)) return FALSE;

	struct tm tm= {};
	tm.tm_mon = -1;

	for (int i=0; mon_dict[i]; i++) {
		if (!memcmp(l->s + 9, mon_dict[i], 3 * sizeof(WCHAR))) {
			tm.tm_mon = i;
			break;
		}
	}
	if (tm.tm_mon < 0) return FALSE;

//at Mon Jul 02 12:50:56 2012
	tm.tm_mday	= _wtoi(l->s+13);
	tm.tm_hour	= _wtoi(l->s+16);
	tm.tm_min	= _wtoi(l->s+19);
	tm.tm_sec	= _wtoi(l->s+22);
	tm.tm_year	= _wtoi(l->s+25) - 1900;
	tm.tm_isdst	= -1;

	if ((m->date = mktime(&tm)) == -1) {
		return FALSE;
	}
	return	TRUE;
}

BOOL ParseOption(const LineInfo *l, LogMsg *m)
{
	static struct {
		DWORD		flags;
		const WCHAR	*key;
		int			len;
	} Opt[] = { // SetThreadLocale + LoadStr という手もあるが…
		{ DB_FLAG_RSA,					L"(RSA)" },
		{ DB_FLAG_RSA2,					L"(RSA2)" },
		{ DB_FLAG_RSA2|DB_FLAG_SIGNED2,	L"(RSA2/認証+)" },
		{ DB_FLAG_RSA2|DB_FLAG_SIGNED2,	L"(RSA2/Signed+)" },
		{ DB_FLAG_RSA2|DB_FLAG_SIGNED,	L"(RSA2/認証)" },
		{ DB_FLAG_RSA2|DB_FLAG_SIGNED,	L"(RSA2/Signed)" },
		{ DB_FLAG_RSA2|DB_FLAG_SIGNERR,	L"(RSA2/偽装？)" },
		{ DB_FLAG_RSA2|DB_FLAG_SIGNERR,	L"(RSA2/Impersonate?)" },
		{ DB_FLAG_UNAUTH,				L"(未認証)" },
		{ DB_FLAG_UNAUTH,				L"(Unauthorized)" },
		{ DB_FLAG_AUTOREP,				L"(自)" },
		{ DB_FLAG_AUTOREP,				L"(auto)" },
		{ DB_FLAG_MULTI,				L"(多)" },
		{ DB_FLAG_MULTI,				L"(multi)" },
		{ DB_FLAG_SEAL,					L"(封)" },
		{ DB_FLAG_SEAL,					L"(sealed)" },
		{ DB_FLAG_LOCK,					L"(錠)" },
		{ DB_FLAG_LOCK,					L"(locked)" },
		{ DB_FLAG_DELAY,				L"(遅延)" },
		{ DB_FLAG_DELAY,				L"(Delayed)" },
		{ 0, 0, 0 }
	};
	if (Opt[0].len == 0) {	// first init
		for (int i=0; Opt[i].flags; i++) {
			Opt[i].len = int(wcslen(Opt[i].key));
		}
	}

	if (l->len <= 30 || l->s[30] != '(') {
		return FALSE;
	}
	const WCHAR	*top = l->s + 30;
	const WCHAR *end = l->s + l->len;

	while (top < end) {
		int	opt_idx = 0;

		while (Opt[opt_idx].flags) {
			if (wcsncmp(top, Opt[opt_idx].key, Opt[opt_idx].len) == 0) {
				break;
			}
			opt_idx++;
		}
		if (Opt[opt_idx].flags == 0) {
			break;
		}
		m->flags |= Opt[opt_idx].flags;
		top += Opt[opt_idx].len;
	}

	return	TRUE;
}

BOOL ParseAttach(Cfg *cfg, const LineInfo *l, LogMsg *m)
{
	static struct {
		DWORD		flags;
		const WCHAR	*key;
		int			len;
	} Opt[] = {
		{ DB_FLAG_FILE,					L"(添付)" },
		{ DB_FLAG_FILE,					L"(files)" },
		{ DB_FLAG_CLIP,					L"(画像)" },
		{ DB_FLAG_CLIP,					L"(image)" },
		{ DB_FLAG_FILE|DB_FLAG_CLIP,	L"(添付/画像)" },
		{ DB_FLAG_FILE|DB_FLAG_CLIP,	L"(files/images)" },
		{ 0, 0, 0 }
	};
	if (Opt[0].len == 0) {	// first init
		for (int i=0; Opt[i].flags; i++) {
			Opt[i].len = int(wcslen(Opt[i].key));
		}
	}

	if (l->len <= 2 || l->s[2] != '(') {
		return FALSE;
	}
	const WCHAR	*top = l->s + 2;

	int	opt_idx = 0;
	while (Opt[opt_idx].flags) {
		if (wcsncmp(top, Opt[opt_idx].key, Opt[opt_idx].len) == 0) {
			break;
		}
		opt_idx++;
	}
	if (Opt[opt_idx].flags == 0) {
		return TRUE;
	}
	m->flags |= Opt[opt_idx].flags;
	top      += Opt[opt_idx].len + 1;

	WCHAR		fname[MAX_PATH];
	size_t		off = 0;

	while (1) {
		const WCHAR	*end = wcsstr(top+off, L", ");
		int	len = wcsncpyz(fname, top, end ? int(end-top+1) : wsizeof(fname));

		BOOL	cur_clip = (m->flags & DB_FLAG_CLIP);
		if (cur_clip && (m->flags & DB_FLAG_FILE)) {	// clip か確認要
			cur_clip = !wcsncmp(fname, L"ipmsgclip_", 10) && !wcscmp(fname+len - 4, L".png");
		}
		if (cur_clip) {
			LogClip	clip;
			clip.fname = fname;
			SetClipDimension(cfg, &clip);
			m->clip.push_back(clip);
		} else {
			m->files.push_back(fname);
		}
		if (!end) {
			break;
		}
		top = end + 2;
		off = 0;
	}

	if (m->clip.size() == 0)  m->flags &= ~DB_FLAG_CLIP;
	if (m->files.size() == 0) m->flags &= ~DB_FLAG_FILE;

	return	TRUE;
}

#define LOG_HEAD1 L"====================================="
#define LOG_HEAD2 L"-------------------------------------"

BOOL ParseMsg(Cfg *cfg, const VBVec<LineInfo> &vlines, int idx, LogMsg *m)
{
	bool	is_attach=false;
	int		i=0;

	if (m->host.size() > 0)		m->host.clear();
	if (m->clip.size() > 0)		m->clip.clear();
	if (m->files.size() > 0)	m->files.clear();

	const LineInfo	*li = &vlines[idx];
	if (li->len != 37 || memcmp(li->s, LOG_HEAD1, sizeof(WCHAR)*37)) {
		return FALSE;
	}

	for (i=idx+1; i < vlines.UsedNumInt(); i++) {
		li = &vlines[i];
		if (li->s[0] != ' ' || (li->s[1] != 'T' && li->s[1] != 'F' &&
			(m->host.size() == 0 || (m->flags & DB_FLAG_FROM) == 0 ||
			 li->s[2] != ' ' || (li->s[3] != 'T' && li->s[3] != 'C')))) {
			break;
		}
		if (!ParseSender(cfg, li, m)) {
			return FALSE;
		}
	}
	if (i == idx+1) {
		return FALSE; // no user
	}

	li = &vlines[i];

	if (!ParseDate(li, m)) {
		return FALSE;
	}
	ParseOption(li, m);
	li++, i++;

	if (ParseAttach(cfg, li, m)) {
		li++, i++;
	}
	if (li->len != 37 || memcmp(li->s, LOG_HEAD2, 37*sizeof(WCHAR))) {
		return FALSE;
	}

	m->line_no = i+1;

	return	TRUE;
}

BOOL ParseMsgBody(LogMsg *m, const VBVec<LineInfo> &vlines, int next_idx)
{
	Wstr	wstr(MAX_UDPBUF);
	WCHAR	*p = wstr.Buf();

	m->lines = 0;
	for (int i=m->line_no; i < next_idx; i++) {
		const LineInfo	&li = vlines[i];

		if (i > m->line_no) {
			*p++ = '\n';
		}
		if ((p - wstr.s()) + li.len >= MAX_UDPBUF) {
			break;
		}
		li.Get(p);
		p += li.len;
		m->lines++;
	}

	if ((p - wstr.s() > 1) && p[-1] == '\n') {
		p[-1] = 0;
		m->lines--;
	}
	else {
		*p = 0;
	}

	m->body = wstr;
	return	TRUE;
}

BOOL SetClipDimension(Cfg *cfg, LogClip *clip)
{
	WCHAR	clip_path[MAX_PATH];
	MakeClipPathW(clip_path, cfg, clip->fname.s());

	return	GetPngDimension(clip_path, &clip->sz);
}


