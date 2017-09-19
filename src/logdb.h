/*	@(#)Copyright (C) H.Shirouzu 2015-2017   logdb.h	Ver4.50 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogDB 
	Create					: 2015-04-10(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGDB_H
#define LOGDB_H

#include "../external/sqlite3/sqlite3.h"
#include <vector>
#include <list>

#define LOGDB_FORMAT_NONE	0
#define LOGDB_FORMAT_V1		1
#define LOGDB_FORMAT_V2		2
#define LOGDB_FORMAT_V3		3
#define LOGDB_FORMAT_V4		4
#define LOGDB_FORMAT_V5		5
#define LOGDB_FORMAT_V6		6
#define LOGDB_FORMAT_V7		7
#define LOGDB_FORMAT_V8		8
#define LOGDB_FORMAT_CUR	LOGDB_FORMAT_V8

#define MEMO_HOST_ID		0
#define MEMO_HOST_UID		L"ipmsg-memo-<0000000000000000>"
#define MEMO_HOST_NICK		L" [ Memo ] "
#define MEMO_HOST_GROUP		L""
#define MEMO_HOST_HOST		L"localhost"
#define MEMO_HOST_ADDR		L""

/*
log_tbl(
	importid	integer primary key,
	logname		text,
	date		integer
	size		integer

msg_tbl(
	msg_id integer primary key,
	importid integer,
	cmd integer,
	flags integer,
	body text,
	lines integer

msg_fts_tbl using fts4(
	msg_id integer primary key,
	body text

host_tbl (
	msg_id integer,
	uid text,
	nick text,
	host text,
	addr text,
	gname text

host_fts_tbl using fts4(
	msg_id integer,
	uid  text,
	nick text

file_tbl (
	msg_id integer,
	fname text

clip_tbl (
	msg_id integer,
	fname text
*/

/*
  thumbnail_db / synonim_db / 
*/


/* ============================================
               convert routines
  ============================================== */
#define DB_FLAG_TO			0x00000000
#define DB_FLAG_FROM		0x00000001
#define DB_FLAG_FILE		0x00000002
#define DB_FLAG_CLIP		0x00000004
#define DB_FLAG_FAV			0x00000008
#define DB_FLAG_MARK		0x00000010
#define DB_FLAG_MEMO		0x00000020
#define DB_FLAG_CMNT		0x00000040
#define DB_FLAG_UNOPENR		0x00000080
#define DB_FLAG_UNOPENRTMP	0x00000100
#define DB_FLAG_RSA			0x00001000
#define DB_FLAG_RSA2		0x00002000
#define DB_FLAG_SIGNED		0x00004000
#define DB_FLAG_SIGNED2		0x00200000
#define DB_FLAG_SIGNERR		0x00008000
#define DB_FLAG_UNAUTH		0x00010000
#define DB_FLAG_AUTOREP		0x00020000
#define DB_FLAG_SEAL		0x00040000
#define DB_FLAG_LOCK		0x00080000
#define DB_FLAG_MULTI		0x00100000

#define DB_FLAGMH_UNOPEN	0x00000001	// for msghost_tbl.flag

inline time_t msgid_to_time(int64 msg_id) {
	return	(time_t)(msg_id >> 26);
}
//inline time_t msgid_to_Time(int64 msg_id) {
//	return	(time_t)(msg_id >> 26);
//}
inline int64 time_to_msgid(time_t t) {
	return	(int64)t << 26;
}
inline int64 msgid_to_msgtime(int64 msg_id) {	// (time_t << 10) + 10bit(mili seconds)
	return	msg_id >> 16;
}
inline int64 msgtime_to_msgid(int64 msg_time) {
	return	msg_time << 16;
}

inline int mids(int64 msg_id) {
	return	(int)(msg_id >> 26);
}

struct LogHost {
	Wstr	uid;
	Wstr	nick;
	Wstr	gname;
	Wstr	host;
	Wstr	addr;
	DWORD	flags;	// for open status

	LogHost() {
		flags = 0;
	}
	bool operator==(const LogHost &lh) {
		return (uid   == lh.uid   &&
				nick  == lh.nick  &&
				gname == lh.gname &&
				host  == lh.host  &&
				addr  == lh.addr);
	}
	bool operator!=(const LogHost &lh) {
		return	!(*this == lh);
	}
	bool IsSameUidHost(const LogHost &lh) {
		return	uid == lh.uid && host == lh.host;
	}
};

struct LogMark {
	int		kind;
	int		pos;
	int		len;
};

struct LogClip {
	Wstr	fname;
	TSize	sz;

	LogClip() {
	}
	LogClip(const LogClip &org) {
		*this = org;
	}
	LogClip &operator=(const LogClip &clip) {
		fname = clip.fname;
		sz = clip.sz;
		return	*this;
	}
};

struct LogMsg {
	ULONG	packet_no;
	int64	msg_id;		// time_t(38) msec(10) genericno(16)
	int		importid;
	int		line_no;
	time_t	date;		// generate by msg_id
	time_t	alter_date;
	int		cmd;
	int		flags;		// recv: 0x0: send 0x1:
	int		lines;

	std::vector<LogHost>	host;
	std::vector<LogClip>	clip;
	std::vector<Wstr>		files;
	std::vector<LogMark>	mark;
	Wstr					body;
	Wstr					comment;

	LogMsg & operator=(const LogMsg &msg) {
		packet_no	= msg.packet_no;
		msg_id		= msg.msg_id;
		importid	= msg.importid;

		line_no		= msg.line_no;
		lines		= msg.lines;
		date		= msg.date;
		alter_date	= msg.alter_date;
		cmd			= msg.cmd;
		flags		= msg.flags;
		host		= msg.host;
		clip		= msg.clip;
		body		= msg.body;
		comment		= msg.comment;
		return	*this;
	}
	LogMsg() {
		Init();
	}
	LogMsg(const LogMsg &msg) {
		*this = msg;
	}
	void Init() {
		packet_no = 0;
		msg_id = 0;
		importid = 0;
		line_no = 0;
		date = 0;
		alter_date = 0;
		cmd = 0;
		flags = 0;
		lines = 0;
		host.clear();
		clip.clear();
		files.clear();
		mark.clear();
		body = NULL;
		comment = NULL;
	}
	bool IsRecv() { return (flags & DB_FLAG_FROM) ? true : false; }
};

struct LogVec {			// 16byte
	int		importid;
	time_t	date;
	Wstr	logname;
	time_t	logdate;
	int64	logsize;
};

struct MsgVec {			// 16byte
	int64	msg_id;		// DB
	int		totalln;	// VBVec<MsgVec>における現在msg迄の行数積算値
//	u_short	disp_num;	// 0で未算出
//	u_int	flags;		// clip選択フラグ
};

// thumb_cache_db
// line_cache_db

struct AddTblData {
	BOOL			status;
	char			*err;
	bool			prevent_begin;
	sqlite3_stmt	*ins_msg;
	sqlite3_stmt	*ins_msgfts;
	sqlite3_stmt	*ins_host;
	sqlite3_stmt	*ins_hostfts;
	sqlite3_stmt	*ins_msghost;
	sqlite3_stmt	*ins_clip;
	sqlite3_stmt	*ins_file;
	sqlite3_stmt	*ins_mark;
	sqlite3_stmt	*sel_hostid;
	sqlite3_stmt	*sel_hostidmax;
	int				importid;
	Wstr			wbuf;
	Wstr			body_fts;
	Wstr			uid_fts;
	Wstr			nick_fts;
	Wstr			host_fts;
	Wstr			addr_fts;
	Wstr			gname_fts;

	AddTblData() {
		Init();
	}
	void Init() {
		status = TRUE;
		prevent_begin = false;
		err = NULL;
		ins_msg = NULL;
		ins_msgfts = NULL;
		ins_host = NULL;
		ins_hostfts = NULL;
		ins_msghost = NULL;
		ins_clip = NULL;
		ins_file = NULL;
		ins_mark = NULL;
		sel_hostid = NULL;
		sel_hostidmax = NULL;

		importid = 1;
		wbuf.Init(8192);
		body_fts.Init(MAX_MSGBODY);
		uid_fts.Init(1024);
		nick_fts.Init(1024);
		host_fts.Init(1024);
		addr_fts.Init(1024);
		gname_fts.Init(1024);
	}
};


class LogDb {
protected:
	Cfg		*cfg;
	sqlite3	*sqlDb;
	char	*sqlErr;
	Wstr	dbName;
	BOOL	CreateTbl(int from_ver=LOGDB_FORMAT_NONE);

	BOOL	userPhrase;
	Wstr	user;
	Wstr	body;
	int		condFlags;
	int		AddTblDataAddHost(AddTblData *atd, const std::vector<LogHost> &host, int64 msg_id);
	int		AddMemoHost();
	BOOL	SelectMsgIdRange(VBVec<int64> *msg_ids, int64 from_id, int64 to_id, BOOL is_asc);
	int64	CoordinateMsgId(int64 ref_msgid, BOOL is_next);
	int		FlipFromTo();
	int		CommentFlags();
	int		ConvertFtsBody();
	int		SetMsgHostIdx();
	BOOL	isBegin;
	BOOL	useHostFts;
	BOOL	isPostInit;

public:
	LogDb(Cfg *_cfg, const WCHAR *db_name=NULL) {
		cfg = _cfg;
		sqlDb = NULL;
		sqlErr = NULL;
		condFlags = 0;
		useHostFts = FALSE;
		isPostInit = FALSE;
		if (db_name) Init(db_name);
	}
	~LogDb() { UnInit(); }

	BOOL	Init(const WCHAR *db_name);
	BOOL	PostInit();
	void	UnInit();
	BOOL	IsInit() { return sqlDb ? TRUE : FALSE; }

//	void	AddMsg();

	// Viewer用
//	int		GetMsgCount(const Cond &cond);
//	int		GetMsg(const Cond &cond);
	void	SetCondUser(const WCHAR *_user, BOOL phrase) {
		user = _user;
		userPhrase = phrase;
	}
	void	SetCondBody(const WCHAR *_body=NULL) { body = _body; }
	void	SetCondFlags(int _flags) { condFlags = _flags; }
	void	SetUseHostFts(BOOL use_host_fts) { useHostFts = use_host_fts; }

	BOOL	SelectMsgIdList(VBVec<MsgVec> *msg_ids, BOOL is_rev, int line_lim);
	BOOL	SelectHostList(VBVec<LogHost> *log_host);
	BOOL	SelectHost(int host_id, LogHost *host);
	BOOL	SelectUnOpenHosts(int64 begin_time, VBVec<LogHost> *_log_host);

	BOOL	SelectMsgIdRange(VBVec<int64> *msg_ids, int64 from_id, int64 to_id);
	int64	SelectMsgIdByIdent(MsgIdent *mi, BOOL is_recv);
	int64	SelectMsgIdByMsg(LogMsg *msg, int importid);

	int		GetMaxNum();
	int		GetMaxNum(int importid);
	int		GetMaxNum(const WCHAR *uid);
	int		GetFlagsNum(DWORD inc_flags, DWORD exc_flags=0);

	int		GetMaxHostId();
	int		GetHostIdNum();
	BOOL	ClearFlags(DWORD flags);

	BOOL	SelectOneData(int64 msg_id, LogMsg *msg);
	BOOL	UpdateOneData(int64 msg_id, int flags);
	BOOL	UpdateOneMsgHost(int64 msg_id, LogHost *host);
	BOOL	UpdateHostInfo(LogHost *in, LogHost *out);
	BOOL	DeleteOneData(int64 msg_id);
	BOOL	InsertOneData(LogMsg *m);
	BOOL	UpdateClip(int64 msg_id, LogClip *clip);

	BOOL	Begin();
	BOOL	Commit();

	BOOL	AddTblDataBegin(AddTblData *atd, const WCHAR *logname, int64 size, time_t date);
	BOOL	AddTblDataBegin(AddTblData *atd) { return AddTblDataBegin(atd, NULL, 0, 0); }
	BOOL	AddTblDataAdd(AddTblData *atd, LogMsg *m, int64 explicit_msgid=0);
	enum EndMode { GOAHEAD, ROLLBACK };
	BOOL	AddTblDataEnd(AddTblData *atd, EndMode end_mode=GOAHEAD);

	BOOL	SelectImportId(VBVec<LogVec> *log_ids);
	int		GetMaxImportId();
	int		GetImportIdByMsg(int64 msg_id);
	BOOL	DeleteImportData(int importid);

	BOOL	GetDbVer(int *ver);
	BOOL	Vacuum();
	void	PrefetchCache(HWND hWnd=NULL, UINT doneMsg=0);
};

inline BOOL IsDbOperatorToken(const WCHAR *tok) {
	return	wcscmp(tok, L"OR") == 0 || wcscmp(tok, L"NOT") == 0;
}


#endif


