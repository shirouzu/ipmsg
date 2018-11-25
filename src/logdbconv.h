/*	@(#)Copyright (C) H.Shirouzu 2015   logdbconv.h	Ver4.00 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogDB 
	Create					: 2015-04-10(Sat)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGDBCONV_H
#define LOGDBCONV_H

#include "../external/sqlite3/sqlite3.h"
#include <vector>
#include <list>

#include "logdb.h"

struct LineInfo {
	const WCHAR	*s;
	int			len;
	int			flags;

	LineInfo(const WCHAR *_s=L"", int _len=0) {
		s=_s;
		len=_len;
		flags = 0;
	}
	LineInfo(const LineInfo &ls) {
		*this = ls;
	}
	bool operator ==(const LineInfo& ls) const {
		return len == ls.len && !memcmp(s, ls.s, sizeof(WCHAR)*len);
	}
	bool operator <(const LineInfo& ls) const {
		if (len < ls.len) return true;
		if (len == ls.len && memcmp(s, ls.s, sizeof(WCHAR)*len) < 0) return true;
		return	false;
	}
	LineInfo& operator=(const LineInfo& ls) {
		s=ls.s; len=ls.len;
		flags=ls.flags;
		return *this;
	}
	void Get(WCHAR *buf) const {
		memcpy(buf, s, sizeof(WCHAR)*len);
		buf[len] = 0;
	}
	int GetU8(char *buf, int bufsize) {
		return WtoU8(s, buf, bufsize, len);
	}
	const char *GetU8s() {
		return WtoU8s(s, len);
	}
	const WCHAR *Gets() {
		return WtoWs(s, len);
	}
	bool IsBegin(const WCHAR *key, int klen=-1) const {
		if (klen == -1) klen = (int)wcslen(key);
		if (klen > len) return false;
		return	wcsnicmp(s, key, klen) == 0;
	}
	bool IsContain(const WCHAR *key, int klen=-1) const {
		if (klen == -1) klen = (int)wcslen(key);
		int max_len = len - klen;
		for (int i=0; i < max_len; i++) {
			if (wcsnicmp(s+i, key, klen) == 0) return true;
		}
		return false;
	}
};

BOOL	AddTextLogToDb(Cfg *cfg, LogDb *db, const char *logfile);
BOOL	DelLogFromDb(LogDb *db, const char *logfile);

BOOL	ParseSender(const LineInfo *l, LogMsg *lm);
BOOL	ParseDate(const LineInfo *l, LogMsg *lm);
BOOL	ParseOption(const LineInfo *l, LogMsg *lm);
BOOL	ParseAttach(Cfg *cfg, const LineInfo *l, LogMsg *lm);
BOOL	ParseMsg(Cfg *cfg, const VBVec<LineInfo>& vlines, int idx, LogMsg *lm);
BOOL	ParseMsgBody(LogMsg *lm, const VBVec<LineInfo>& vlines, int next_idx);

BOOL	SetClipDimension(Cfg *cfg, LogClip *clip);

#endif


