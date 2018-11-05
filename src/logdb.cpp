static char *logdb_id = 
	"@(#)Copyright (C) H.Shirouzu 2015-2017   logdb.cpp	Ver4.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Log Database
	Create					: 2015-04-10(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include <assert.h>
#include <process.h>
using namespace std;
#include "logdb.h"
#include "../external/sqlite3/sqlite3.h"

using namespace std;

// ToDo:
//   UTF-8 DB
//   mark -> mark_tbl化 (msg_id / kind /  pos / len )
//   付箋, tag (msg_id / kind / text
//   self msg -> flag のみでOK? (host_id)

//#define GRN
#ifdef GRN
#include "logdbgrn.h"
#include "../external/grn/include/groonga/groonga.h"
#endif

static int gen_ngram_multi_str(const WCHAR *key, WCHAR *wbuf, int max_size);
static int gen_ngram(const WCHAR *src, WCHAR *dst, int max_buf, int src_len=-1);
static int gen_ngram_str(const WCHAR *key, WCHAR *wbuf, int max_size);
static int gen_like_str(const WCHAR *key, WCHAR *wbuf, int max_size);
static int gen_2gram(const WCHAR *src, WCHAR *dst, int max_buf);

BOOL LogDb::Init(const WCHAR *fname)
{
	BOOL new_db = false;

	dbName = fname;
	if (::GetFileAttributesW(dbName.s()) == 0xffffffff) {
		new_db = true;
	}
	BOOL ret = sqlite3_open16(dbName.s(), &sqlDb) == SQLITE_OK;

	if (new_db) {
		CreateTbl();
	}
	else {
		int	ver = LOGDB_FORMAT_NONE;
		if (!GetDbVer(&ver) || ver < LOGDB_FORMAT_CUR) {
			if (!CreateTbl(ver)) {
				UnInit();
				TMessageBoxW(
					FmtW(L"Sorry, ipmsg DB updating failed.\r\n"
						  " please contact to shirouzu@ipmsg.org or "
						  "rename %s to other fname", dbName.s()));
			}
		}
		else {
			if (ver > LOGDB_FORMAT_CUR) {
				UnInit();
				TMessageBoxW(
					FmtW(L"Sorry, ipmsg DB is too new.\r\n"
						  "If you want to use this version,\r\n"
						  " please rename %s to other fname", dbName.s()));
			}
		}
	}

	//PrefetchCache();
	isBegin = FALSE;
	userPhrase = FALSE;

#ifdef GRN
	GrnOpen();
#endif
	return	ret;
}

void LogDb::UnInit()
{
	if (sqlDb) sqlite3_close(sqlDb);
#ifdef GRN
	GrnEnd();
#endif
	sqlDb = NULL;
}

BOOL LogDb::PostInit()
{
	if (isPostInit) return	TRUE;
	isPostInit = TRUE;

	int	ret;
	// Clear UnOpenR cache
	if ((ret = sqlite3_exec(sqlDb, "begin;", 0, 0, &sqlErr)) != SQLITE_OK) {
		return	FALSE;
	}

	char sql[MAX_BUF];

	// 全ての UnOopenRTmp(0x100) フラグと古い UnOpenR(0x80) フラグをクリア
	sprintf(sql, "update msg_tbl set flags=(flags&~0x180) "
		" where (flags&0x100) or (flags&0x80 and msg_id<%lld);",
		time_to_msgid(cfg->viewEpoch));

	if ((ret = sqlite3_exec(sqlDb, sql, 0, 0, &sqlErr)) != SQLITE_OK) {
		DebugW(L"PostInit 1 sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		goto ERR;
	}

	// 新しいが UnOpenR が立っていない msg があれば ON
	sprintf(sql, 
		"update msg_tbl set flags=(flags|0x80) "
		" where flags&1 and (flags&0x80)=0 and msg_id>=%lld"
		" and msg_id in (select msg_id from msghost_tbl where flags&1);",
			time_to_msgid(cfg->viewEpoch));

	if ((ret = sqlite3_exec(sqlDb, sql, 0, 0, &sqlErr)) != SQLITE_OK) {
		DebugW(L"PostInit 2 sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		goto ERR;
	}

	if ((ret = sqlite3_exec(sqlDb, "commit;", 0, 0, &sqlErr)) != SQLITE_OK) {
		goto ERR;
	}
	DebugW(L"PostInit done\n");
	return	TRUE;

ERR:
	DebugW(L"PostInit sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_exec(sqlDb, "rollback;", 0, 0, &sqlErr);
	return	FALSE;
}

BOOL LogDb::ClearFlags(DWORD flags)
{
	char	sql[MAX_BUF];
	sprintf(sql, "update msg_tbl set flags=(flags&~0x%x) where (flags&0x%x);", flags, flags);

	int ret;
	if ((ret = sqlite3_exec(sqlDb, sql, 0, 0, &sqlErr)) != SQLITE_OK) {
		DebugW(L"ClearFlags sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		return	FALSE;
	}
//	DebugW(L"ClearFlags done\n");
	return	 TRUE;
}

BOOL LogDb::Vacuum()
{
	if (!sqlDb) {
		return	FALSE;
	}
	if (isBegin) {	// busy
		return	FALSE;
	}

	int ret = sqlite3_exec(sqlDb, "vacuum;", 0, 0, &sqlErr);

	if (ret != SQLITE_OK) {
		DebugW(L"vacuum sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		return	FALSE;
	}
	return	TRUE;
}

void LogDb::PrefetchCache(HWND hWnd, UINT doneMsg, BOOL with_vacuum)
{
	PrefetchFile(dbName.s(), hWnd, doneMsg, with_vacuum);
}

/*
	dbinfo_tbl {	// DB情報テーブル
		db_ver			// DBバージョン（1:初期tbl、2:mark追加）
	}
	log_tbl {		// ログ（インポート履歴）テーブル
		importid		// 過去ログインポートID (primary key)
		date			// インポート日時
		logname			// ログファイル名
		logdate			// ログのタイムスタンプ
		logsize			// ログのファイルサイズ
	}
	msg_tbl {		// メッセージテーブル
		msg_id			// メッセージID（上位40bit:time_t、後続10bit: msec、下位14bit: uniq化）
							// primary key
		importid		// -> log_tbl
		cmd				// msg.cmd（現状、未使用）
		flags			// db用flags
		body			// メッセージ本文
		lines			// 論理行数
		alter_date		// ソートに影響しない日付（メモ用）
		comment			// 追加コメント
		packet_no		// packet_no
	}
	host_tbl {		// ホスト（ユーザ）テーブル
		host_id			// ホストID（primary key）... （host_id==0 はメモ用の特別用途）
		uid				// ユーザID（ログオン名 + 公開鍵指紋）
		nick			// ニックネーム（ユーザ設定ユーザ名）
		host			// ホスト名（NetBIOS用コンピュータ名）
		addr			// IPアドレス（過去ログの場合、存在しない場合あり）
		gname			// グループ名（過去ログの場合、存在しない場合あり）
	}
	msghost_tbl {	// msg_tbl と host_tbl の紐づけ用
		msg_id			// -> msg_tbl
		host_id			// -> host_tbl
		flags			// open status
		idx				// message内での順番
	}
	file_tbl {		// ファイルテーブル
		msg_id			// -> msg_tbl
		fname			// 添付ファイル名
	}
	clip_tbl {		// クリップ（貼付画像）テーブル
		msg_id			// -> msg_tbl
		fname			// 貼付画像ファイル名（(log_dir)/img_img/ からのファイル名）
		cx				// cx
		cy				// cy
	}
	mark_tbl {		// マーカーテーブル
		msg_id			// -> msg_tbl
		kind			// 種類（現状0のみ。将来、色分け等に利用）
		pos				// msg_tbl.body での開始文字位置
		len				// pos からの文字長
	}
*/

/* ============================================
		Create Table
  ============================================== */
BOOL LogDb::CreateTbl(int from_ver)
{
	int		ret = 0;
	char	sql[MAX_BUF];

	if ((ret = sqlite3_exec(sqlDb, "begin;", 0, 0, &sqlErr)) != SQLITE_OK) {
		goto ERR;
	}

	if (from_ver < LOGDB_FORMAT_V1) {
		// log_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create table log_tbl( \n"
			"	importid	integer primary key, \n"
			"	date		integer, \n"
			"	logname		text, \n"
			"	logdate		integer, \n"
			"	logsize		integer \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index log_importid_idx on log_tbl(importid); \n"
			"create index log_logname_idx on log_tbl(logname);   \n"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// msg_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create table msg_tbl( \n"
			"	msg_id integer primary key, \n"
			"	importid integer, \n"
			"	cmd integer, \n"
			"	flags integer, \n"
			"	body text, \n"
			"	lines integer \n"
			//", alter_date integer \n"
			//", comment text\n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index msg_msgid_idx on msg_tbl(msg_id); \n"
			"create index msg_importid_idx on msg_tbl(importid); \n"
			"create index msg_cmd_idx  on msg_tbl(cmd); \n"
			"create index msg_flags_idx on msg_tbl(flags); \n"
			"create index msg_body_idx on msg_tbl(body); \n"
		//	"create index msg_lines_idx on msg_tbl(lines); \n"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// msg_fts_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create virtual table msg_fts_tbl using fts4( \n"
			"	msg_id integer primary key, \n"
			"	body text, \n"
			// OK: tokenize=simple " " " "
			// NG: tokenize=simple "tokenchars= " "separators= "
			//  (':'(or more chars?) becomes separotors?)
			"	tokenize=simple \" \" \" \" "
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// host_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create table host_tbl ( \n"
			"	host_id integer primary key, \n"
			"	uid text,  \n"
			"	nick text, \n"
			"	host text, \n"
			"	addr text, \n"
			"	gname text \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index host_id_idx   on host_tbl(host_id); \n"
			"create index host_uid_idx  on host_tbl(uid); \n"
			"create index host_nick_idx on host_tbl(nick); \n"
	//		"create index host_addr_idx on host_tbl(addr); \n"
	//		"create index host_gname_idx on host_tbl(gname); \n"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// host_fts_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create virtual table host_fts_tbl using fts4( \n"
			"	host_id integer, \n"
			"	uid  text, \n"
			"	nick text, \n"
	//		"	host text, \n"
	//		"	addr text, \n"
	//		"	gname text, \n"
			"	tokenize=simple \" \" \" \" "
			");"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// msghost_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create table msghost_tbl ( \n"
			"	msg_id  integer, \n"
			"	host_id integer  \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index msghost_idx on msghost_tbl(msg_id, host_id); \n"
			"create index msghost_host_id_idx on msghost_tbl(host_id); \n"
			"create index msghost_msg_id_idx  on msghost_tbl(msg_id);  \n"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// file_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create table file_tbl ( \n"
			"	msg_id integer, \n"
			"	fname text \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index file_msgid_idx on file_tbl(msg_id); \n"
	//		"create index file_fname_idx on file_tbl(fname);  \n"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// clip_tbl		将来的には cx/cy cache も
		if ((ret = sqlite3_exec(sqlDb,
			"create table clip_tbl ( \n"
			"	msg_id integer, \n"
			"	fname text \n"
	//		",	cx integer \n"
	//		",	cy integer \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index clip_msgid_idx on clip_tbl(msg_id); \n"
	//		"create index clip_fname_idx on clip_tbl(fname); \n"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}
	}

	if (from_ver < LOGDB_FORMAT_V2) {
		// table info
		if ((ret = sqlite3_exec(sqlDb,
			"create table dbinfo_tbl( \n"
			"	db_ver	integer primary key \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		sprintf(sql, "insert into dbinfo_tbl (db_ver) values (%d);", LOGDB_FORMAT_NONE);
		if ((ret = sqlite3_exec(sqlDb, sql, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		// mark_tbl
		if ((ret = sqlite3_exec(sqlDb,
			"create table mark_tbl ( \n"
			"	msg_id integer, \n"
			"	kind   integer, \n"
			"	pos    integer, \n"
			"	len    integer  \n"
			"); "
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}

		if ((ret = sqlite3_exec(sqlDb,
			"create index mark_msgid_idx on mark_tbl(msg_id);"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}
	}

	if (from_ver < LOGDB_FORMAT_V3) {
		if ((ret = sqlite3_exec(sqlDb,
			"	alter table msg_tbl add column alter_date integer;"
			"	alter table msg_tbl add column comment text;"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}
		// AddMemoHost は V4の後
	}

	if (from_ver < LOGDB_FORMAT_V4) {
		if ((ret = sqlite3_exec(sqlDb,
			"	alter table msg_tbl  add column packet_no integer;"
			"	alter table clip_tbl add column cx integer;"
			"	alter table clip_tbl add column cy integer;"
			"	alter table msghost_tbl add column flags integer;"
			"create index host_host_idx on host_tbl(host);"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}
	}

	if (from_ver < LOGDB_FORMAT_V5) {	// importデータは From/To flagが逆
		if (FlipFromTo() != SQLITE_DONE) {	
			goto ERR;
		}
	}

	if (from_ver < LOGDB_FORMAT_V6) {
		if (CommentFlags() != SQLITE_DONE) {	
			goto ERR;
		}
	}

	if (from_ver < LOGDB_FORMAT_V7) {
		if (ConvertFtsBody() != SQLITE_DONE) {	
			goto ERR;
		}
	}

	if (from_ver < LOGDB_FORMAT_V8) {
		if ((ret = sqlite3_exec(sqlDb,
			"	alter table msghost_tbl add column idx integer;"
			, 0, 0, &sqlErr)) != SQLITE_OK) {
			goto ERR;
		}
		if (SetMsgHostIdx() != SQLITE_DONE) {	
			goto ERR;
		}
	}

	// cx追加後(V4) and idx(V8) 等、各種DBupdate後に行う必要あり
	if (from_ver < LOGDB_FORMAT_V3) {
		if (AddMemoHost() != SQLITE_DONE) {
			goto ERR;
		}
	}

	sprintf(sql, "update dbinfo_tbl set db_ver=%d;", LOGDB_FORMAT_CUR);
	if ((ret = sqlite3_exec(sqlDb, sql, 0, 0, &sqlErr)) != SQLITE_OK) {
		goto ERR;
	}

	if ((ret = sqlite3_exec(sqlDb, "commit;", 0, 0, &sqlErr)) != SQLITE_OK) {
		goto ERR;
	}
	return	TRUE;

ERR:
	DebugW(L"CreateTbl sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_exec(sqlDb, "rollback;", 0, 0, &sqlErr);
	return	FALSE;
}

/* ============================================
		Select Data
  ============================================== */
// SQL文の汎用化/自動化は後日

#define SELDBVER_SQL	\
	L"select db_ver"	\
	L" from dbinfo_tbl"

#define SELLOGALL_SQL									\
	L"select importid, date, logname, logdate, logsize"	\
	L" from log_tbl order by date"

#define SELLOGIDMAX_SQL						\
	L"select max(importid)"					\
	L" from log_tbl"

#define SELALLIDX_SQL						\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" order by msg_id"

#define SELUIDIDX_FMTEX						\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" where msg_id in"						\
	L"  (select msg_id"						\
	L"   from msghost_tbl"					\
	L"   where host_id in"					\
	L"    (select host_id"					\
	L"     from host_fts_tbl"				\
	L"     where %%s%s?))"					\
	L" order by msg_id"

#define SELBODYIDX_SQL						\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" where msg_id in"						\
	L"  (select msg_id"						\
	L"   from msg_fts_tbl"					\
	L"   where body match ?)"				\
	L" order by msg_id"

#define SELUIDBODYIDX_FMTEX					\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" where msg_id in"						\
	L"  (select distinct msg_id"			\
	L"   from msg_fts_tbl"					\
	L"   where msg_id in"					\
	L"    (select msg_id"					\
	L"     from msghost_tbl"				\
	L"     where host_id in"				\
	L"      (select host_id"				\
	L"       from host_fts_tbl"				\
	L"        where %%s%s?))"				\
	L"     and msg_fts_tbl.body match ?)"	\
	L" order by msg_id"

#define SELFLAGIDX_SQL						\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" where ((flags & ?) and (flags & ?))"	\
	L" order by msg_id"

#define SELUIDFLAGIDX_FMTEX					\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" where ((flags & ?) and (flags & ?))"	\
	L" and msg_id in "						\
	L"  (select distinct msg_id"			\
	L"   from msghost_tbl"					\
	L"   where host_id in"					\
	L"    (select host_id"					\
	L"     from host_fts_tbl"				\
	L"     where %%s%s?))"					\
	L" order by msg_id"

#define SELBODYFLAGIDX_SQL					\
	L"select msg_id,lines"					\
	L" from msg_tbl"						\
	L" where ((flags & ?) and (flags & ?))"	\
	L" and msg_id in"						\
	L"  (select msg_id"						\
	L"   from msg_fts_tbl"					\
	L"   where body match ?)"				\
	L" order by msg_id"

#define SELUIDBODYFLAGIDX_FMTEX				\
	L"select msg_id,lines "					\
	L" from msg_tbl"						\
	L" where ((flags & ?) and (flags & ?))"	\
	L" and msg_id in"						\
	L"  (select distinct msg_id"			\
	L"   from msg_fts_tbl"					\
	L"   where msg_id in"					\
	L"     (select distinct msg_id"			\
	L"      from msghost_tbl"				\
	L"      where host_id in"				\
	L"       (select host_id"				\
	L"        from host_fts_tbl"			\
	L"        where %%s%s?))"				\
	L"   and body match ?)"					\
	L" order by msg_id"

#define SELHOSTPACKETIDX_SQL			\
	L"select msg_id "					\
	L" from msg_tbl"					\
	L" where (packet_no=?)"				\
	L" and ((flags & 1)=?)"				\
	L" and msg_id in"					\
	L"   (select distinct msg_id"		\
	L"    from msghost_tbl"				\
	L"    where host_id in"				\
	L"     (select host_id"				\
	L"      from host_tbl"				\
	L"      where uid=? and host=?))"

#define SELMSGIDBYMSG_SQL					\
	L"select msg_id "						\
	L" from msg_tbl"						\
	L" where (msg_id >= ? and msg_id <= ?)"	\
	L" and flags=?"							\
	L" and importid<>?"						
//	L" and msg_id in"						\
//	L"   (select distinct msg_id"			\
//	L"    from msghost_tbl"					\
//	L"    where host_id in"					\
//	L"     (select host_id"					\
//	L"      from host_tbl"					\
//	L"      where host=?))"					\
//	L" and (body=?)"

#define SELMSG_SQL																\
	L"select importid, cmd, flags, body, lines, alter_date, comment, packet_no"	\
	L" from msg_tbl where msg_id=?"

#define SELMSGHOST_SQL									\
	L"select uid, nick, host, addr, gname, flags, idx"	\
	L" from host_tbl"									\
	L" inner join msghost_tbl"							\
	L"  on host_tbl.host_id=msghost_tbl.host_id"		\
	L"  where msg_id=? order by idx"

#define SELMSGCLIP_SQL						\
	L"select fname, cx, cy"					\
	L" from clip_tbl"						\
	L" where msg_id=?"

#define SELMSGFILE_SQL						\
	L"select fname"							\
	L" from file_tbl"						\
	L" where msg_id=?"

#define SELMSGMARK_SQL						\
	L"select kind, pos, len"				\
	L" from mark_tbl"						\
	L" where msg_id=? order by pos"

#define SELMSGNUM_SQL						\
	L"select count(msg_id)"					\
	L" from msg_tbl"

#define SELMSGNUMBYIMPORTID_SQL				\
	L"select count(msg_id)"					\
	L" from msg_tbl"						\
	L" where importid=?"

#define SELHOSTID_SQL											\
	L"select host_id"											\
	L" from host_tbl"											\
	L" where uid=? and nick=? and host=? and addr=? and gname=?"

#define SELHOSTIDNUM_SQL					\
	L"select count(host_id)"				\
	L" from msghost_tbl"

#define SELHOSTIDMAX_SQL					\
	L"select max(host_id)"					\
	L" from host_tbl"

#define SELHOSTLIST_SQL																\
	L"select host_id, uid, nick, host, addr, gname"									\
	L" from"																		\
	L"  (select host_tbl.host_id, uid, max(mid) as msg_id, nick, host, addr, gname"	\
	L"    from host_tbl"															\
	L"    inner join"																\
	L"     (select host_id, max(msg_id) as mid"										\
	L"       from msghost_tbl"														\
	L"       group by host_id) as mht"												\
	L"     on host_tbl.host_id=mht.host_id"											\
	L"     group by uid)"															\
	L" order by msg_id desc"

#define SELHOST_SQL													\
	L"select host_id, uid, nick, host, addr, gname"					\
	L" from host_tbl"												\
	L" where host_id=?"

#define SELMSGIDRANGE_FMT									\
	"select msg_id"											\
	" from msg_tbl"											\
	" where msg_id >= ? and msg_id <= ?"					\
	" order by msg_id %s"

#define SELHOSTLISTUNOOPEN_SQL						\
	L"select t1.host_id, uid, host"					\
	L" from host_tbl as t1"							\
	L" where t1.host_id in"							\
	L"  (select distinct host_id"					\
	L"    from msghost_tbl as t2"					\
	L"    where (t2.flags & 1) and t2.msg_id in"	\
	L"     (select msg_id"							\
	L"       from msg_tbl"							\
	L"       where msg_id >= ? and (flags & 1)))"

BOOL LogDb::SelectImportId(VBVec<LogVec> *log_ids)
{
	sqlite3_stmt	*sel_idx = NULL;

	if (sqlite3_prepare16(sqlDb, SELLOGALL_SQL, -1, &sel_idx, NULL)) goto ERR;

	log_ids->SetUsedNum(0);

	while (sqlite3_step(sel_idx) == SQLITE_ROW) {
		int		sidx = 0;
		LogVec	vec;

		vec.importid	= sqlite3_column_int(sel_idx, sidx++);
		vec.date		= sqlite3_column_int(sel_idx, sidx++);
		vec.logname		= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		vec.logdate		= sqlite3_column_int(sel_idx, sidx++);
		vec.logsize		= sqlite3_column_int64(sel_idx, sidx++);
		log_ids->Push(vec);
	}
	sqlite3_finalize(sel_idx);
	return	TRUE;

ERR:
	sqlite3_finalize(sel_idx);
	return	FALSE;
}

void MakePlaceHolder(WCHAR *sql_place, const WCHAR *sql, BOOL is_rev, BOOL range=FALSE)
{
	int	len = 0;

	if (range) {
		len += wcscpyz(sql_place + len, L"select * from (");
	}
	len += wcscpyz(sql_place + len, sql);

	if (range) {
		if (!is_rev) {
			len += wcscpyz(sql_place + len, L" desc");
		}
		len += wcscpyz(sql_place + len, L" limit ?) order by msg_id");
	}
	if (is_rev) {
		len += wcscpyz(sql_place + len, L" desc");
	}
//	DebugW(L"sql=%s\n", sql_place);
}

void MakePlaceHolderHfts(WCHAR *sql_place, const WCHAR *fmt, BOOL is_host_fts, BOOL is_rev,
	BOOL range=FALSE)
{
	int	len = 0;

	if (range) {
		len += wcscpyz(sql_place + len, L"select * from (");
	}
	len += swprintf(sql_place + len, fmt, is_host_fts ? L"host_fts_tbl" : L"uid");

	if (range) {
		if (!is_rev) {
			len += wcscpyz(sql_place + len, L" desc");
		}
		len += wcscpyz(sql_place + len, L" limit ?) order by msg_id");
	}
	if (is_rev) {
		len += wcscpyz(sql_place + len, L" desc");
	}
//	DebugW(L"sqlft=%s\n", sql_place);
}

BOOL LogDb::SelectMsgIdList(VBVec<MsgVec> *msg_ids, BOOL is_rev, int line_lim)
{
	sqlite3_stmt	*sel_idx = NULL;
	WCHAR			user_ngram[MAX_BUF];
	WCHAR			body_ngram[MAX_BUF];
	WCHAR			sql_tmp[MAX_BUF];
	WCHAR			sql_place[MAX_BUF];
	int				ret = SQLITE_OK;
	int				cidx = 1;
	BOOL			is_lim = maxRange > 0 ? TRUE : FALSE;

	if (!isPostInit) PostInit();	// 通常は WM_LOGFETCH_DONE から呼び出し済み

	if (user) {
		if (userPhrase) {
			gen_ngram(user.s(), user_ngram, wsizeof(user_ngram));
		}
		else {
			gen_ngram_multi_str(user.s(), user_ngram, wsizeof(user_ngram));
		}
	}
	if (body) {
		gen_ngram_multi_str(body.s(), body_ngram, wsizeof(body_ngram));
	}

#ifdef GRN
	if (body) {
		TTick	t;
		GrnSearch(WtoU8s(body.s()), msg_ids);
		Debug("GrnSearch %d tickd\n", t.elaps());
		return	TRUE;
	}
#endif

	if (condFlags) {
		int	cond1 = condFlags;
		int	cond2 = ~0;

//		if ((condFlags & DB_FLAG_FAV) && (condFlags & ~DB_FLAG_FAV)) {
//			cond1 &= ~DB_FLAG_FAV;
//			cond2 = DB_FLAG_FAV;
//		}

		if (user) {
			swprintf(sql_tmp, body ? SELUIDBODYFLAGIDX_FMTEX : SELUIDFLAGIDX_FMTEX,
				userPhrase ? L"=" : L" match ");
			if (body) {
				MakePlaceHolderHfts(sql_place, sql_tmp, useHostFts, is_rev, is_lim);
				//DebugW(L"<%s> %s\n", user_ngram, sql_place);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond1)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond2)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, user_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, body_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			} else {
				MakePlaceHolderHfts(sql_place, sql_tmp, useHostFts, is_rev, is_lim);
				//DebugW(L"<%s> %s\n", user_ngram, sql_place);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond1)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond2)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, user_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			}
		} else {
			if (body) {
				MakePlaceHolder(sql_place, SELBODYFLAGIDX_SQL, is_rev, is_lim);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond1)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond2)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, body_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			} else {
				MakePlaceHolder(sql_place, SELFLAGIDX_SQL, is_rev, is_lim);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond1)) goto ERR;
				if (sqlite3_bind_int(sel_idx, cidx++, cond2)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			}
		}
	} else {
		if (user) {
			swprintf(sql_tmp, body ? SELUIDBODYIDX_FMTEX : SELUIDIDX_FMTEX,
				userPhrase ? L"=" : L" match ");
			if (body) {
				MakePlaceHolderHfts(sql_place, sql_tmp, useHostFts, is_rev, is_lim);
				//DebugW(L"<%s> %s\n", user_ngram, sql_place);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, user_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, body_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			} else {
				MakePlaceHolderHfts(sql_place, sql_tmp, useHostFts, is_rev, is_lim);
				//DebugW(L"<%s> %s\n", user_ngram, sql_place);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, user_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			}
		} else {
			if (body) {
				MakePlaceHolder(sql_place, SELBODYIDX_SQL, is_rev, is_lim);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (sqlite3_bind_text16(sel_idx, cidx++, body_ngram, -1, SQLITE_STATIC)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			} else {
				MakePlaceHolder(sql_place, SELALLIDX_SQL, is_rev, is_lim);
				if (sqlite3_prepare16(sqlDb, sql_place, -1, &sel_idx, NULL)) goto ERR;
				if (is_lim && sqlite3_bind_int(sel_idx, cidx++, maxRange)) goto ERR;
			}
		}
	}

	int	totalln = 0;
	int	count = 0;

//	{TTick tick;
	while (sqlite3_step(sel_idx) == SQLITE_ROW) {
		if (count++ == 0) {
//			Debug("first=%d\n", tick.elaps());
			msg_ids->SetUsedNum(0);
		}
		int	sidx = 0;
		MsgVec vec;
		vec.msg_id	= sqlite3_column_int64(sel_idx, sidx++);
		int lines = sqlite3_column_int(sel_idx, sidx++);
		if (line_lim > 0 && lines > line_lim) {
			lines = line_lim;
		}
		totalln += lines;
		vec.totalln = totalln;
		if (!msg_ids->Push(vec)) {
			break;
		}
	}
	sqlite3_finalize(sel_idx);

//	Debug("fin=%d\n", tick.elaps());}

	return	count ? TRUE : FALSE;

ERR:
	DebugW(L"SelectMsgIdList sqlerr=%s\n", sqlite3_errmsg16(sqlDb));
	sqlite3_finalize(sel_idx);
	return FALSE;
}

BOOL LogDb::SelectHostList(VBVec<LogHost> *_log_host)
{
	sqlite3_stmt	*sel_idx = NULL;
	VBVec<LogHost>	&log_host = *_log_host;

	if (sqlite3_prepare16(sqlDb, SELHOSTLIST_SQL, -1, &sel_idx, NULL)) goto ERR;

	log_host.SetUsedNum(0);

	while (sqlite3_step(sel_idx) == SQLITE_ROW) {
		int		sidx = 0;
		LogHost	vec;

		int host_id = sqlite3_column_int(sel_idx, sidx++);
		vec.uid		= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		vec.nick	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		vec.host	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		vec.addr	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		vec.gname	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);

		log_host.Push(vec);

		if (host_id == MEMO_HOST_ID && log_host.UsedNumInt() > 1) {
			BYTE	tmp[sizeof(LogHost)];		// dirty hack
			memcpy(tmp, &log_host[log_host.UsedNumInt()-1], sizeof(LogHost));
			memmove(&log_host[1], &log_host[0], (log_host.UsedNumInt()-1) * sizeof(LogHost));
			memcpy(&log_host[0], tmp, sizeof(LogHost));
		}
	}
	sqlite3_finalize(sel_idx);
	return	TRUE;

ERR:
	DebugW(L"SelectHostList sqlerr=%s\n", sqlite3_errmsg16(sqlDb));
	sqlite3_finalize(sel_idx);
	return	FALSE;
}

BOOL LogDb::SelectUnOpenHosts(int64 begin_time, VBVec<LogHost> *_log_host)
{
	sqlite3_stmt	*sel_idx = NULL;
	VBVec<LogHost>	&log_host = *_log_host;
	int				cidx = 1;

	if (sqlite3_prepare16(sqlDb, SELHOSTLISTUNOOPEN_SQL, -1, &sel_idx, NULL)) goto ERR;
	if (sqlite3_bind_int64(sel_idx, cidx++, begin_time)) goto ERR;

	log_host.SetUsedNum(0);

	while (sqlite3_step(sel_idx) == SQLITE_ROW) {
		int		sidx = 0;
		LogHost	vec;

		int host_id = sqlite3_column_int(sel_idx, sidx++);
		vec.uid		= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		vec.host	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);

		if (host_id == MEMO_HOST_ID) {
			continue;
		}
		log_host.Push(vec);
	}
	sqlite3_finalize(sel_idx);
	return	TRUE;

ERR:
	DebugW(L"SelectUnOpenHosts sqlerr=%s\n", sqlite3_errmsg16(sqlDb));
	sqlite3_finalize(sel_idx);
	return	FALSE;
}

BOOL LogDb::SelectHost(int host_id, LogHost *host)
{
	sqlite3_stmt	*sel_idx = NULL;

	if (sqlite3_prepare16(sqlDb, SELHOST_SQL, -1, &sel_idx, NULL)) goto ERR;

	sqlite3_bind_int(sel_idx, 1, host_id);

	if (sqlite3_step(sel_idx) == SQLITE_ROW) {
		int	sidx = 1;
		host->uid	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		host->nick	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		host->host	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		host->addr	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
		host->gname	= (WCHAR *)sqlite3_column_text16(sel_idx, sidx++);
	}
	sqlite3_finalize(sel_idx);
	return	TRUE;

ERR:
	DebugW(L"SelectHost sqlerr=%s\n", sqlite3_errmsg16(sqlDb));
	sqlite3_finalize(sel_idx);
	return	FALSE;
}


BOOL LogDb::GetDbVer(int *ver)
{
	sqlite3_stmt	*sel_idx = NULL;

	if (sqlite3_prepare16(sqlDb, SELDBVER_SQL, -1, &sel_idx, NULL)) goto ERR;

	int		sidx = 0;
	if (sqlite3_step(sel_idx) == SQLITE_ROW) {
		*ver = sqlite3_column_int(sel_idx, sidx++);
	}
	sqlite3_finalize(sel_idx);
	return	TRUE;

ERR:
	sqlite3_finalize(sel_idx);
	return	FALSE;
}

BOOL LogDb::SelectOneData(int64 msg_id, LogMsg *msg)
{
	sqlite3_stmt	*sel_msg   = NULL;
	sqlite3_stmt	*sel_mhost = NULL;
	sqlite3_stmt	*sel_mclip  = NULL;
	sqlite3_stmt	*sel_mfile = NULL;
	sqlite3_stmt	*sel_mmark = NULL;
	BOOL			ret = FALSE;

	sqlite3_prepare16(sqlDb, SELMSG_SQL,     -1, &sel_msg,   NULL);
	sqlite3_prepare16(sqlDb, SELMSGHOST_SQL, -1, &sel_mhost, NULL);
	sqlite3_prepare16(sqlDb, SELMSGCLIP_SQL, -1, &sel_mclip, NULL);
	sqlite3_prepare16(sqlDb, SELMSGFILE_SQL, -1, &sel_mfile, NULL);
	sqlite3_prepare16(sqlDb, SELMSGMARK_SQL, -1, &sel_mmark, NULL);

	sqlite3_bind_int64(sel_msg, 1, msg_id);

	if (sqlite3_step(sel_msg) == SQLITE_ROW) {
		int	midx = 0;
		msg->msg_id		= msg_id;
		msg->date		= msgid_to_time(msg->msg_id);
		msg->importid	= sqlite3_column_int(sel_msg, midx++);
		msg->cmd		= sqlite3_column_int(sel_msg, midx++);
		msg->flags		= sqlite3_column_int(sel_msg, midx++);
		msg->body		= (WCHAR *)sqlite3_column_text16(sel_msg, midx++);
		msg->lines		= sqlite3_column_int(sel_msg, midx++);
		if (msg->lines == 0 && msg->body) {
			msg->lines = get_linenum_n(msg->body.s(), msg->body.StripLen());
		}
		msg->alter_date	= sqlite3_column_int(sel_msg, midx++);
		msg->comment	= (WCHAR *)sqlite3_column_text16(sel_msg, midx++);
		msg->packet_no	= sqlite3_column_int(sel_msg, midx++);

		sqlite3_bind_int64(sel_mhost, 1, msg->msg_id);

		while (sqlite3_step(sel_mhost) == SQLITE_ROW) {
			int	hidx = 0;
			LogHost host;
			host.uid	= (WCHAR *)sqlite3_column_text16(sel_mhost, hidx++);
			host.nick	= (WCHAR *)sqlite3_column_text16(sel_mhost, hidx++);
			host.host	= (WCHAR *)sqlite3_column_text16(sel_mhost, hidx++);
			host.addr	= (WCHAR *)sqlite3_column_text16(sel_mhost, hidx++);
			host.gname	= (WCHAR *)sqlite3_column_text16(sel_mhost, hidx++);
			host.flags	= sqlite3_column_int64(sel_mhost, hidx++);
			msg->host.push_back(host);
			ret = TRUE;
		}
		sqlite3_reset(sel_mhost);

		sqlite3_bind_int64(sel_mclip, 1, msg->msg_id);
		while (sqlite3_step(sel_mclip) == SQLITE_ROW) {
			int	cidx = 0;
			LogClip	clip;
			clip.fname	= (WCHAR *)sqlite3_column_text16(sel_mclip, cidx++);
			clip.sz.cx	= sqlite3_column_int(sel_mclip, cidx++);
			clip.sz.cy	= sqlite3_column_int(sel_mclip, cidx++);
			msg->clip.push_back(clip);
		}
		sqlite3_reset(sel_mclip);

		sqlite3_bind_int64(sel_mfile, 1, msg->msg_id);
		while (sqlite3_step(sel_mfile) == SQLITE_ROW) {
			msg->files.push_back(Wstr((WCHAR *)sqlite3_column_text16(sel_mfile, 0)));
		}
		sqlite3_reset(sel_mfile);

		sqlite3_bind_int64(sel_mmark, 1, msg->msg_id);
		while (sqlite3_step(sel_mmark) == SQLITE_ROW) {
			int	midx = 0;
			LogMark	mark;
			mark.kind = sqlite3_column_int(sel_mmark, midx++);
			mark.pos  = sqlite3_column_int(sel_mmark, midx++);
			mark.len  = sqlite3_column_int(sel_mmark, midx++);
			msg->mark.push_back(mark);
		}
		sqlite3_reset(sel_mmark);
	}

	sqlite3_finalize(sel_msg);
	sqlite3_finalize(sel_mhost);
	sqlite3_finalize(sel_mclip);
	sqlite3_finalize(sel_mfile);
	sqlite3_finalize(sel_mmark);

	return	ret;
}

int64 LogDb::SelectMsgIdByIdent(MsgIdent *mi, BOOL is_recv)
{
	sqlite3_stmt	*sel_msghost = NULL;
	int64			msg_id = 0;
	int				ret;

	if ((ret = sqlite3_prepare16(sqlDb, SELHOSTPACKETIDX_SQL, -1, &sel_msghost, NULL))
		!= SQLITE_OK) {
		goto ERR;
	}

	int		sidx = 1;
	sqlite3_bind_int(sel_msghost,    sidx++, mi->packetNo);
	sqlite3_bind_int(sel_msghost,    sidx++, is_recv ? DB_FLAG_FROM : DB_FLAG_TO);
	sqlite3_bind_text16(sel_msghost, sidx++, mi->uid.s(), -1, SQLITE_STATIC);
	sqlite3_bind_text16(sel_msghost, sidx++, mi->host.s(), -1, SQLITE_STATIC);

	if (sqlite3_step(sel_msghost) != SQLITE_ROW) {
		goto ERR;
	}
	else {
		int		sidx = 0;
		msg_id = sqlite3_column_int64(sel_msghost, sidx++);
	}

	sqlite3_finalize(sel_msghost);
	return	 msg_id;

ERR:
	sqlite3_finalize(sel_msghost);
	DebugW(L"SelectMsgIdByIdent sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return msg_id;
}

int64 LogDb::SelectMsgIdByMsg(LogMsg *msg, int importid)
{
	sqlite3_stmt	*sel_msg = NULL;
	int64			msg_id = 0;
	int				ret;

	if ((ret = sqlite3_prepare16(sqlDb, SELMSGIDBYMSG_SQL, -1, &sel_msg, NULL))
		!= SQLITE_OK) {
		goto ERR;
	}

	int64	org_msg_id = time_to_msgid(msg->date);
	int		sidx = 1;
	sqlite3_bind_int64(sel_msg,  sidx++, org_msg_id);
	sqlite3_bind_int64(sel_msg,  sidx++, org_msg_id | 0x0000ffff);
	sqlite3_bind_int(sel_msg,    sidx++, msg->flags);
	sqlite3_bind_int(sel_msg,    sidx++, importid);
//	sqlite3_bind_text16(sel_msg, sidx++, msg->host[0].host.s(), -1, SQLITE_STATIC);
//	sqlite3_bind_text16(sel_msg, sidx++, msg->body.s(), -1, SQLITE_STATIC);

	ret = sqlite3_step(sel_msg);
	if (ret == SQLITE_ROW) {
		int		sidx = 0;
		msg_id = sqlite3_column_int64(sel_msg, sidx++);
	}
	else if (ret == SQLITE_DONE) {
		msg_id = 0;
	}
	else {
		msg_id = -1;
		goto ERR;
	}

	sqlite3_finalize(sel_msg);
	return	 msg_id;

ERR:
	sqlite3_finalize(sel_msg);
	DebugW(L"SelectMsgIdByMsg sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return msg_id;
}


int LogDb::GetMaxNum()
{
	int				ret;
	sqlite3_stmt	*sel_cnt = NULL;
	int				num = 0;

	if ((ret = sqlite3_prepare16(sqlDb, SELMSGNUM_SQL, -1, &sel_cnt, NULL)) != SQLITE_OK) {
		goto ERR;
	}
	if ((ret = sqlite3_step(sel_cnt)) == SQLITE_ROW) {
		num = sqlite3_column_int(sel_cnt, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		num = 0;
	}

	sqlite3_finalize(sel_cnt);
	return	 num;

ERR:
	if (sel_cnt) sqlite3_finalize(sel_cnt);
	DebugW(L"GetMaxNum sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}

int LogDb::GetMaxNum(int importid)
{
	int				ret;
	sqlite3_stmt	*sel_cnt = NULL;
	int				num = 0;

	if ((ret = sqlite3_prepare16(sqlDb, SELMSGNUMBYIMPORTID_SQL, -1, &sel_cnt, NULL))
		!= SQLITE_OK) {
		goto ERR;
	}
	int	sidx = 1;
	sqlite3_bind_int(sel_cnt, sidx++, importid);

	if ((ret = sqlite3_step(sel_cnt)) == SQLITE_ROW) {
		num = sqlite3_column_int(sel_cnt, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		num = 0;
	}

	sqlite3_finalize(sel_cnt);
	return	 num;

ERR:
	if (sel_cnt) sqlite3_finalize(sel_cnt);
	DebugW(L"GetMaxNum(%d) sqlerr=%s ret=%d\n", importid, sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}


#define SELMSGNUMBYUID_SQL					\
	L"select count(msg_id)"					\
	L" from msghost_tbl"					\
	L" where host_id in"					\
	L"  (select host_id"					\
	L"   from host_fts_tbl"					\
	L"   where uid=?)"

int LogDb::GetMaxNum(const WCHAR *uid)
{
	int				ret;
	sqlite3_stmt	*sel_cnt = NULL;
	int				num = 0;

	if ((ret = sqlite3_prepare16(sqlDb, SELMSGNUMBYUID_SQL, -1, &sel_cnt, NULL))
		!= SQLITE_OK) {
		goto ERR;
	}

	WCHAR	u_ngram[MAX_BUF];
	gen_ngram(uid, u_ngram, wsizeof(u_ngram));

	int		sidx = 1;
	sqlite3_bind_text16(sel_cnt, sidx++, u_ngram, -1, SQLITE_STATIC);

	if ((ret = sqlite3_step(sel_cnt)) == SQLITE_ROW) {
		num = sqlite3_column_int(sel_cnt, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		num = 0;
	}

	sqlite3_finalize(sel_cnt);
	DebugW(L"GetMaxNum(uid=%s) num=%d\n", uid, num);
	return	 num;

ERR:
	if (sel_cnt) sqlite3_finalize(sel_cnt);
	DebugW(L"GetMaxNum(uid=%s) sqlerr=%s ret=%d\n", uid, sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}

#define SELMSGNUMBYFLAGS_FMT				\
	L"select count(msg_id)"					\
	L" from msg_tbl "						\
	L" where (flags&?) %s;"

int LogDb::GetFlagsNum(DWORD inc_flags, DWORD exc_flags)
{
	int				ret;
	WCHAR			sql_place[MAX_BUF];
	sqlite3_stmt	*sel_cnt = NULL;
	int				num = 0;

	swprintf(sql_place, SELMSGNUMBYFLAGS_FMT, exc_flags ? L" and not (flags&?)" : L"");

	if ((ret = sqlite3_prepare16(sqlDb, sql_place, -1, &sel_cnt, NULL))
		!= SQLITE_OK) {
		goto ERR;
	}

	int		sidx = 1;
	sqlite3_bind_int(sel_cnt, sidx++, inc_flags);
	if (exc_flags) {
		sqlite3_bind_int(sel_cnt, sidx++, exc_flags);
	}

	if ((ret = sqlite3_step(sel_cnt)) == SQLITE_ROW) {
		num = sqlite3_column_int(sel_cnt, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		num = 0;
	}

	sqlite3_finalize(sel_cnt);
	DebugW(L"GetFlagsNum(flags=%x/%x) num=%d\n", inc_flags, exc_flags);
	return	 num;

ERR:
	if (sel_cnt) sqlite3_finalize(sel_cnt);
	DebugW(L"GetFlagsNum(flags=%x/%x) sqlerr=%s ret=%d\n",
		inc_flags, exc_flags, sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}


/* ============================================
		Insert Data
  ============================================== */
#define INSLOG_SQL										\
	L"insert into log_tbl"								\
	L" (importid, date, logname, logdate, logsize)"		\
	L" values (?, ?, ?, ?, ?);"

#define INSMSG_SQL																	\
	L"insert into msg_tbl"															\
	L" (msg_id, importid, cmd, flags, body, lines, alter_date, comment, packet_no)"	\
	L" values (?, ?, ?, ?, ?, ?, ?, ?, ?);"

#define INSMSGFTS_SQL									\
	L"insert into msg_fts_tbl"							\
	L" (msg_id, body)"									\
	L" values (?, ?);"

#define INSHOST_SQL										\
	L"insert into host_tbl"								\
	L" (host_id, uid, nick, host, addr, gname)"			\
	L" values (?, ?, ?, ?, ?, ?);"

#define INSHOSTFTS_SQL									\
	L"insert into host_fts_tbl"							\
	L" (host_id, uid, nick)"							\
	L" values (?, ?, ?);"

#define INSMSGHOST_SQL									\
	L"insert into msghost_tbl"							\
	L" (host_id, msg_id, flags, idx)"					\
	L" values (?, ?, ?, ?);"

#define INSCLIP_SQL										\
	L"insert into clip_tbl"								\
	L" (msg_id, fname, cx, cy)"							\
	L" values (?, ?, ?, ?);"

#define INSFILE_SQL										\
	L"insert into file_tbl"								\
	L" (msg_id, fname)"									\
	L" values (?, ?);"

#define INSMARK_SQL										\
	L"insert into mark_tbl"								\
	L" (msg_id, kind, pos, len)"						\
	L" values (?, ?, ?, ?);"

#define SELMAX_SQL										\
	L"select max(msg_id)"								\
	L" from msg_tbl"

int LogDb::GetMaxImportId()
{
	int				ret;
	sqlite3_stmt	*sel_log = NULL;
	int				importid = -1;

	if ((ret = sqlite3_prepare16(sqlDb, SELLOGIDMAX_SQL, -1, &sel_log, NULL)) != SQLITE_OK) {
		goto ERR;
	}
	if ((ret = sqlite3_step(sel_log)) == SQLITE_ROW) {
		importid = sqlite3_column_int(sel_log, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		importid = 0;
	}

	sqlite3_finalize(sel_log);
	return	 importid;

ERR:
	if (sel_log) sqlite3_finalize(sel_log);
	DebugW(L"GetMaxImportId sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}

BOOL LogDb::SelectMsgIdRange(VBVec<int64> *msg_ids, int64 from_id, int64 to_id, BOOL is_asc)
{
	sqlite3_stmt	*sel_idx = NULL;
	char			buf[MAX_BUF];

	snprintfz(buf, sizeof(buf), SELMSGIDRANGE_FMT, is_asc ? "asc" : "desc");
	if (sqlite3_prepare(sqlDb, buf, -1, &sel_idx, NULL)) goto ERR;

	int		sidx = 1;
	sqlite3_bind_int64(sel_idx, sidx++, from_id);
	sqlite3_bind_int64(sel_idx, sidx++, to_id);

	msg_ids->SetUsedNum(0);

	while (sqlite3_step(sel_idx) == SQLITE_ROW) {
		msg_ids->Push(sqlite3_column_int64(sel_idx, 0));
	}
	sqlite3_finalize(sel_idx);
	return	TRUE;

ERR:
	DebugW(L"SelectMsgIdRange sqlerr=%s\n", sqlite3_errmsg16(sqlDb));
	sqlite3_finalize(sel_idx);
	return	FALSE;
}

int64 LogDb::CoordinateMsgId(int64 ref_msgid, BOOL is_next)
{
	VBVec<int64>	ids;
	int64			from_id, to_id;

	ids.Init(0, 65536, MAX_BUF); // max 16bit message

	if (is_next) {
		from_id = ref_msgid + 1;
		to_id   = msgtime_to_msgid(msgid_to_msgtime(ref_msgid) + 1) -1;
	}
	else {
		to_id   = ref_msgid - 1;
		from_id = msgtime_to_msgid(msgid_to_msgtime(ref_msgid) - 1) +1;
	}

	if (to_id < from_id) {
		return	0;
	}

	if (!SelectMsgIdRange(&ids, from_id, to_id, is_next)) {
		return	0;
	}
	if (ids.UsedNum()) {
		if (is_next) {
			to_id = ids[0] - 1;
		}
		else {
			from_id = ids[0] + 1;
		}
	}
	if (to_id < from_id) {
		return	0;
	}

	return	(to_id + from_id) / 2;
}

BOOL LogDb::InsertOneData(LogMsg *m)
{
	AddTblData	atd;
	BOOL		ret = FALSE;
	int64		exciplit_msgid = 0;

	if ((ret = AddTblDataBegin(&atd))) {
//		if (ref_msgid) {
//			if ((exciplit_msgid = CoordinateMsgId(ref_msgid, is_next)) == 0) {
//				ret = FALSE;
//			}
//		}
		if (ret) {
			ret = AddTblDataAdd(&atd, m, exciplit_msgid);
		}
		if (!AddTblDataEnd(&atd)) {
			ret = FALSE;
		}
	}
	return	ret;
}

int LogDb::AddMemoHost()
{
	AddTblData	atd;
	int			ret = -1;

	atd.prevent_begin = true;

	if (AddTblDataBegin(&atd)) {
		vector<LogHost> hosts;
		LogHost			host;
		host.uid	= MEMO_HOST_UID;
		host.nick	= MEMO_HOST_NICK;
		host.gname	= MEMO_HOST_GROUP;
		host.host	= MEMO_HOST_HOST;
		host.addr	= MEMO_HOST_ADDR;

		hosts.push_back(host);
		ret = AddTblDataAddHost(&atd, hosts, MEMO_HOST_ID);

		if (!AddTblDataEnd(&atd)) {
			ret = -1;
		}
	}
	return	ret;
}

int LogDb::GetMaxHostId()
{
	int				ret;
	sqlite3_stmt	*sel_host = NULL;
	int				host_id = -1;

	if ((ret = sqlite3_prepare16(sqlDb, SELHOSTIDMAX_SQL, -1, &sel_host, NULL)) != SQLITE_OK) {
		goto ERR;
	}
	if ((ret = sqlite3_step(sel_host)) == SQLITE_ROW) {
		host_id = sqlite3_column_int(sel_host, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		host_id = MEMO_HOST_ID;	// 0
	}

	sqlite3_finalize(sel_host);
	return	 host_id;

ERR:
	if (sel_host) sqlite3_finalize(sel_host);
	DebugW(L"GetMaxHostId sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}

int LogDb::GetHostIdNum()
{
	int				ret;
	sqlite3_stmt	*sel_host = NULL;
	int				num = 0;

	if ((ret = sqlite3_prepare16(sqlDb, SELHOSTIDNUM_SQL, -1, &sel_host, NULL)) != SQLITE_OK) {
		goto ERR;
	}
	if ((ret = sqlite3_step(sel_host)) == SQLITE_ROW) {
		num = sqlite3_column_int(sel_host, 0);
	}
	else if (ret == SQLITE_DONE) {	// none
		num = 0;
	}

	sqlite3_finalize(sel_host);
	return	 num;

ERR:
	if (sel_host) sqlite3_finalize(sel_host);
	DebugW(L"GetMaxHostId sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return	-1;
}

BOOL LogDb::AddTblDataBegin(AddTblData *atd, const WCHAR *logname, int64 logsize, time_t logdate)
{
	int	ret = SQLITE_OK;

	if (!IsInit()) {
		return FALSE;
	}

	ret = sqlite3_prepare16(sqlDb, INSMSG_SQL,  -1, &atd->ins_msg, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSMSGFTS_SQL, -1, &atd->ins_msgfts, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSHOST_SQL, -1, &atd->ins_host, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSHOSTFTS_SQL, -1, &atd->ins_hostfts, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSMSGHOST_SQL, -1, &atd->ins_msghost, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSCLIP_SQL, -1, &atd->ins_clip,  NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSFILE_SQL, -1, &atd->ins_file, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, INSMARK_SQL, -1, &atd->ins_mark, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, SELHOSTID_SQL, -1, &atd->sel_hostid, NULL);
	if (ret != SQLITE_OK) goto ERR;

	ret = sqlite3_prepare16(sqlDb, SELHOSTIDMAX_SQL, -1, &atd->sel_hostidmax, NULL);
	if (ret != SQLITE_OK) goto ERR;

//	// msg_id 最大値
//	sqlite3_stmt	*sel_max;
//	if ((ret = sqlite3_prepare16(sqlDb, SELMAX_SQL,  -1, &sel_max,  NULL)) != SQLITE_OK) goto ERR;
//	if ((ret = sqlite3_step(sel_max)) == SQLITE_ROW) {
//		atd->msg_id = sqlite3_column_int64(sel_max, 0) + 1;
//	}
//	sqlite3_finalize(sel_max);

	if (!atd->prevent_begin) {
		if ((ret = sqlite3_exec(sqlDb, "begin;", 0, 0, &atd->err)) != SQLITE_OK) {
			goto ERR;
		}
	}
	if (logname) {
		atd->importid = GetMaxImportId() + 1;
		if (atd->importid <= 0) goto ERR;

		sqlite3_stmt	*ins_log = NULL;
		int				lidx = 1;
		if ((ret = sqlite3_prepare16(sqlDb, INSLOG_SQL, -1, &ins_log, NULL))) goto ERR;

		sqlite3_bind_int(ins_log,    lidx++, atd->importid);
		sqlite3_bind_int64(ins_log,  lidx++, (int64)time(NULL));
		sqlite3_bind_text16(ins_log, lidx++, logname, -1, SQLITE_STATIC);
		sqlite3_bind_int64(ins_log,  lidx++, logdate);
		sqlite3_bind_int64(ins_log,  lidx++, logsize);
		ret = sqlite3_step(ins_log);
		sqlite3_finalize(ins_log);
		if (ret != SQLITE_DONE) {
			AddTblDataEnd(atd);
			goto ERR;
		}
	}
	else {
		atd->importid = 0;
	}
	return	TRUE;

ERR:
	atd->status = FALSE;
	DebugW(L"AddTblDataBegin sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return FALSE;
}

int LogDb::AddTblDataAddHost(AddTblData *atd, const vector<LogHost> &host, int64 msg_id)
{
	int	ret = SQLITE_OK;

	for (auto itr=host.begin(); itr != host.end(); itr++) {
		int	uidx		= 1;
		int	host_id		= -1;

		sqlite3_bind_text16(atd->sel_hostid, uidx++, itr->uid.s(),   -1, SQLITE_STATIC);
		sqlite3_bind_text16(atd->sel_hostid, uidx++, itr->nick.s(),  -1, SQLITE_STATIC);
		sqlite3_bind_text16(atd->sel_hostid, uidx++, itr->host.s(),  -1, SQLITE_STATIC);
		sqlite3_bind_text16(atd->sel_hostid, uidx++, itr->addr.s(),  -1, SQLITE_STATIC);
		sqlite3_bind_text16(atd->sel_hostid, uidx++, itr->gname.s(), -1, SQLITE_STATIC);
		if (sqlite3_step(atd->sel_hostid) == SQLITE_ROW) {
			host_id = sqlite3_column_int(atd->sel_hostid, 0);
		}
		sqlite3_reset(atd->sel_hostid);

		if (host_id < 0) {
			if (msg_id) {
				if (sqlite3_step(atd->sel_hostidmax) == SQLITE_ROW) {
					host_id = sqlite3_column_int(atd->sel_hostidmax, 0) + 1;
				}
				sqlite3_reset(atd->sel_hostidmax);
			}
			else {
				host_id = MEMO_HOST_ID;	// memo用
			}

			uidx = 1;
			sqlite3_bind_int(atd->ins_host,  uidx++, host_id);
			sqlite3_bind_text16(atd->ins_host, uidx++, itr->uid.s(),   -1, SQLITE_STATIC);
			sqlite3_bind_text16(atd->ins_host, uidx++, itr->nick.s(),  -1, SQLITE_STATIC);
			sqlite3_bind_text16(atd->ins_host, uidx++, itr->host.s(),  -1, SQLITE_STATIC);
			sqlite3_bind_text16(atd->ins_host, uidx++, itr->addr.s(),  -1, SQLITE_STATIC);
			sqlite3_bind_text16(atd->ins_host, uidx++, itr->gname.s(), -1, SQLITE_STATIC);
			ret = sqlite3_step(atd->ins_host);
			sqlite3_reset(atd->ins_host);
			if (ret != SQLITE_DONE) return	ret;

			uidx = 1;
			sqlite3_bind_int(atd->ins_hostfts, uidx++, host_id);
			gen_ngram(itr->uid.s(), atd->uid_fts.Buf(), MAX_BUF);
			sqlite3_bind_text16(atd->ins_hostfts, uidx++, atd->uid_fts.s(),  -1, SQLITE_STATIC);
			gen_ngram(itr->nick.s(), atd->nick_fts.Buf(), MAX_BUF);
			sqlite3_bind_text16(atd->ins_hostfts, uidx++, atd->nick_fts.s(), -1, SQLITE_STATIC);
	//		gen_ngram(itr->host.s(), atd->host_fts.Buf(), MAX_BUF);
	//		sqlite3_bind_text16(atd->ins_hostfts, uidx++, atd->host_fts.s(), -1, SQLITE_STATIC);
	//		gen_ngram(itr->addr.s(), atd->addr_fts.Buf(), MAX_BUF);
	//		sqlite3_bind_text16(atd->ins_hostfts, uidx++, atd->addr_fts.s(), -1, SQLITE_STATIC);
	//		gen_ngram(itr->gname.s(), atd->gname_fts.Buf(), MAX_BUF);
	//		sqlite3_bind_text16(atd->ins_hostfts, uidx++, atd->gname_fts.s(), -1, SQLITE_STATIC);
			ret = sqlite3_step(atd->ins_hostfts);
			sqlite3_reset(atd->ins_hostfts);
			if (ret != SQLITE_DONE) return	ret;
		}
		if (msg_id) {
			uidx = 1;
			sqlite3_bind_int(atd->ins_msghost, uidx++, host_id);
			sqlite3_bind_int64(atd->ins_msghost, uidx++, msg_id);
			sqlite3_bind_int64(atd->ins_msghost, uidx++, itr->flags);
			sqlite3_bind_int(atd->ins_msghost, uidx++, int(itr - host.begin()));
			ret = sqlite3_step(atd->ins_msghost);
			sqlite3_reset(atd->ins_msghost);
			if (ret != SQLITE_DONE) return	ret;
		}
	}
	return	SQLITE_DONE;
}

void ConsistantMsgFlags(LogMsg *m)
{
	if (m->comment) {
		m->flags |= DB_FLAG_CMNT;
	} else {
		m->flags &= ~DB_FLAG_CMNT;
	}

	if (m->clip.size()) {
		m->flags |= DB_FLAG_CLIP;
	} else {
		m->flags &= ~DB_FLAG_CLIP;
	}

	if (m->files.size()) {
		m->flags |= DB_FLAG_FILE;
	} else {
		m->flags &= ~DB_FLAG_FILE;
	}

	if (m->mark.size()) {
		m->flags |= DB_FLAG_MARK;
	} else {
		m->flags &= ~DB_FLAG_MARK;
	}
}

BOOL LogDb::AddTblDataAdd(AddTblData *atd, LogMsg *m, int64 explicit_msgid)
{
	int64	msg_id = explicit_msgid ? explicit_msgid : time_to_msgid(m->date);
	int		ret = SQLITE_OK;

	while (1) {
		int	midx = 1;
		int	importid = atd->importid ? atd->importid : m->importid;

		ConsistantMsgFlags(m);
		sqlite3_bind_int64(atd->ins_msg, midx++, msg_id);		// msg_id
		sqlite3_bind_int(atd->ins_msg, midx++, importid);		// import id
		sqlite3_bind_int(atd->ins_msg, midx++, m->cmd);			// cmd
		sqlite3_bind_int(atd->ins_msg, midx++, m->flags);		// flags
		sqlite3_bind_text16(atd->ins_msg, midx++, m->body.s(), -1, SQLITE_STATIC); // body
		if (m->body && m->lines == 0) {
			m->lines = get_linenum_n(m->body.s(), m->body.StripLen());
		}
		sqlite3_bind_int(atd->ins_msg,   midx++, m->lines);			// lines
		sqlite3_bind_int64(atd->ins_msg, midx++, m->alter_date);	// alter_date
		sqlite3_bind_text16(atd->ins_msg, midx++, m->comment.s(), -1, SQLITE_STATIC);

		sqlite3_bind_int(atd->ins_msg, midx++, m->packet_no);

		ret = sqlite3_step(atd->ins_msg);
		sqlite3_reset(atd->ins_msg);
		if (ret == SQLITE_DONE) break;

		if ((ret = sqlite3_errcode(sqlDb)) == SQLITE_CONSTRAINT) {
			if ((++msg_id & 0x0000ffff)) {		// 同一時刻が存在する場合は
				continue;						// generic番号を増やしてリトライ
			}
		}
		goto ERR;
	}

	{	// for msg_fts
		int	midx = 1;
		sqlite3_bind_int64(atd->ins_msgfts, midx++, msg_id);	// msg_id
		gen_ngram(m->body.s(), atd->body_fts.Buf(), MAX_MSGBODY);
		sqlite3_bind_text16(atd->ins_msgfts, midx++, atd->body_fts.s(), -1, SQLITE_STATIC); // body

		ret = sqlite3_step(atd->ins_msgfts);
		sqlite3_reset(atd->ins_msgfts);
		if (ret != SQLITE_DONE) goto ERR;
	}

	if ((ret = AddTblDataAddHost(atd, m->host, msg_id)) != SQLITE_DONE) {
		goto ERR;
	}

	for (auto &c: m->clip) {
		int	cidx = 1;
		sqlite3_bind_int64(atd->ins_clip,  cidx++, msg_id);
		sqlite3_bind_text16(atd->ins_clip, cidx++, c.fname.s(), -1, SQLITE_STATIC);
		sqlite3_bind_int(atd->ins_clip,  cidx++, c.sz.cx);
		sqlite3_bind_int(atd->ins_clip,  cidx++, c.sz.cy);
		ret = sqlite3_step(atd->ins_clip);
		sqlite3_reset(atd->ins_clip);
		if (ret != SQLITE_DONE) goto ERR;
	}
	for (auto &f: m->files) {
		int	fidx = 1;
		sqlite3_bind_int64(atd->ins_file,  fidx++, msg_id);
		sqlite3_bind_text16(atd->ins_file, fidx++, f.s(), -1, SQLITE_STATIC);
		ret = sqlite3_step(atd->ins_file);
		sqlite3_reset(atd->ins_file);
		if (ret != SQLITE_DONE) goto ERR;
	}
	for (auto &m: m->mark) {
		int	midx = 1;
		sqlite3_bind_int64(atd->ins_mark, midx++, msg_id);
		sqlite3_bind_int(atd->ins_mark,   midx++, m.kind);
		sqlite3_bind_int(atd->ins_mark,   midx++, m.pos);
		sqlite3_bind_int(atd->ins_mark,   midx++, m.len);
		ret = sqlite3_step(atd->ins_mark);
		sqlite3_reset(atd->ins_mark);
		if (ret != SQLITE_DONE) goto ERR;
	}

#ifdef GRN
	GrnInsert(msg_id, WtoU8s(m->body.s()));
#endif
//	DebugW(L"AddTblData OK id=%llx\n", msg_id);
	m->msg_id = msg_id;
	return	TRUE;

ERR:
	atd->status = FALSE;
	DebugW(L"AddTblDataAdd sqlerr=%s ret=%d id=%llx\n", sqlite3_errmsg16(sqlDb), ret, msg_id);
	return FALSE;
}

BOOL LogDb::AddTblDataEnd(AddTblData *atd, LogDb::EndMode end_mode)
{
	int ret = SQLITE_OK;

	if (!atd->prevent_begin) {
		ret = sqlite3_exec(sqlDb,
			(atd->status && end_mode == GOAHEAD) ? "commit;" : "rollback;",
			0, 0, &atd->err);
	}

	sqlite3_finalize(atd->ins_msg);
	sqlite3_finalize(atd->ins_msgfts);
	sqlite3_finalize(atd->ins_host);
	sqlite3_finalize(atd->ins_hostfts);
	sqlite3_finalize(atd->ins_msghost);
	sqlite3_finalize(atd->ins_clip);
	sqlite3_finalize(atd->ins_file);
	sqlite3_finalize(atd->ins_mark);
	sqlite3_finalize(atd->sel_hostid);
	sqlite3_finalize(atd->sel_hostidmax);

	if (atd->status) {
		if (ret == SQLITE_OK) {
			return TRUE;
		}
		atd->status = FALSE;
		DebugW(L"AddTblDataEnd sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	}
	return	FALSE;
}

/* ============================================
		Update Data
  ============================================== */
#define UPDATEMSG_SQL	\
	L"update msg_tbl"	\
	L" set flags=?"		\
	L" where msg_id=?"

#define UPDATEMSGHOST_SQL				\
	L"update msghost_tbl"				\
	L" set flags=?"						\
	L" where msg_id=? and host_id in "	\
	L"  (select host_id"				\
	L"   from host_tbl"					\
	L"   where uid=? and host=?)"

#define UPDATEHOST_SQL											\
	L"update host_tbl"											\
	L" set uid=?, nick=?, host=?, addr=?, gname=? "				\
	L" where uid=? and nick=? and host=? and addr=? and gname=?"

#define UPDATEHOSTFTS_SQL										\
	L"update host_fts_tbl"										\
	L" set uid=?, nick=? where uid=? and nick=?"

#define UPDATECLIP_SQL				\
	L"update clip_tbl"				\
	L" set cx=?, cy=?"				\
	L" where msg_id=? and fname=?"

#define UPDATEFROMTOFLIP_SQL				\
	L"update msg_tbl"						\
	L" set flags=((~(flags&1))&(flags|1))"	\
	L" where importid <> 0"

#define UPDATECOMMENTFLAGS_SQL		\
	L"update msg_tbl"				\
	L" set flags=(flags|0x40)"		\
	L" where length(comment) > 0"

BOOL LogDb::UpdateOneData(int64 msg_id, int flags)
{
	int				ret = 0;
	sqlite3_stmt	*upd_msg = NULL;
	int				uidx = 1;

	if ((ret = sqlite3_prepare16(sqlDb, UPDATEMSG_SQL, -1, &upd_msg, NULL))) goto ERR;
	if ((ret = sqlite3_bind_int(upd_msg,   uidx++, flags))) goto ERR;
	if ((ret = sqlite3_bind_int64(upd_msg, uidx++, msg_id))) goto ERR;
	if ((ret = sqlite3_step(upd_msg)) != SQLITE_DONE) goto ERR;

	sqlite3_finalize(upd_msg);
	return	TRUE;

ERR:
	DebugW(L"UpdateOneData sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(upd_msg);
	return	FALSE;
}

BOOL LogDb::UpdateOneMsgHost(int64 msg_id, LogHost *host)
{
	int				ret = 0;
	sqlite3_stmt	*upd_msg = NULL;
	int				uidx = 1;

	if ((ret = sqlite3_prepare16(sqlDb, UPDATEMSGHOST_SQL, -1, &upd_msg, NULL))) {
		goto ERR;
	}
	//Debug("flags=%d\n", host->flags);
	sqlite3_bind_int64(upd_msg, uidx++, host->flags);
	sqlite3_bind_int64(upd_msg, uidx++, msg_id);
	sqlite3_bind_text16(upd_msg, uidx++, host->uid.s(),  -1, SQLITE_STATIC);
	sqlite3_bind_text16(upd_msg, uidx++, host->host.s(), -1, SQLITE_STATIC);
	if ((ret = sqlite3_step(upd_msg)) != SQLITE_DONE) {
		goto ERR;
	}

	sqlite3_finalize(upd_msg);
	return	TRUE;

ERR:
	DebugW(L"UpdateOneMsgHost sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(upd_msg);
	return	FALSE;
}

BOOL LogDb::UpdateHostInfo(LogHost *in, LogHost *out)
{
	int				ret = 0;
	sqlite3_stmt	*upd_host = NULL;
	int				uidx = 1;

	if ((ret = sqlite3_exec(sqlDb, "begin;", 0, 0, &sqlErr)) != SQLITE_OK) goto ERR;

	if ((ret = sqlite3_prepare16(sqlDb, UPDATEHOST_SQL, -1, &upd_host, NULL))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, out->uid.s(),  -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, out->nick.s(), -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, out->host.s(), -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, out->addr.s(), -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, out->gname.s(),-1, SQLITE_STATIC))) goto ERR;

	if ((ret = sqlite3_bind_text16(upd_host, uidx++, in->uid.s(),   -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, in->nick.s(),  -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, in->host.s(),  -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, in->addr.s(),  -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, in->gname.s(), -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_step(upd_host)) != SQLITE_DONE) goto ERR;
	sqlite3_finalize(upd_host);

	WCHAR	wbuf[MAX_BUF];
	uidx = 1;
	if ((ret = sqlite3_prepare16(sqlDb, UPDATEHOSTFTS_SQL, -1, &upd_host, NULL))) goto ERR;
	gen_ngram(out->uid.s(), wbuf, wsizeof(wbuf));
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, wbuf,  -1, SQLITE_TRANSIENT))) goto ERR;
	gen_ngram(out->nick.s(), wbuf, wsizeof(wbuf));
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, wbuf,  -1, SQLITE_TRANSIENT))) goto ERR;
	gen_ngram(in->uid.s(), wbuf, wsizeof(wbuf));
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, wbuf,  -1, SQLITE_TRANSIENT))) goto ERR;
	gen_ngram(in->nick.s(), wbuf, wsizeof(wbuf));
	if ((ret = sqlite3_bind_text16(upd_host, uidx++, wbuf,  -1, SQLITE_TRANSIENT))) goto ERR;

	if ((ret = sqlite3_step(upd_host)) != SQLITE_DONE) goto ERR;
	sqlite3_finalize(upd_host);

	if ((ret = sqlite3_exec(sqlDb, "commit;", 0, 0, &sqlErr)) != SQLITE_OK) goto ERR;
	return	TRUE;

ERR:
	DebugW(L"UpdateHostInfo sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(upd_host);
	sqlite3_exec(sqlDb, "rollback;", 0, 0, &sqlErr);
	return	FALSE;
}

BOOL LogDb::UpdateClip(int64 msg_id, LogClip *clip)
{
	int				ret = 0;
	sqlite3_stmt	*upd_clip = NULL;
	int				uidx = 1;

	if ((ret = sqlite3_prepare16(sqlDb, UPDATECLIP_SQL, -1, &upd_clip, NULL))) goto ERR;
	if ((ret = sqlite3_bind_int(upd_clip, uidx++, clip->sz.cx))) goto ERR;
	if ((ret = sqlite3_bind_int(upd_clip, uidx++, clip->sz.cy))) goto ERR;
	if ((ret = sqlite3_bind_int64(upd_clip, uidx++, msg_id))) goto ERR;
	if ((ret = sqlite3_bind_text16(upd_clip, uidx++, clip->fname.s(), -1, SQLITE_STATIC))) goto ERR;
	if ((ret = sqlite3_step(upd_clip)) != SQLITE_DONE) goto ERR;

	sqlite3_finalize(upd_clip);
	return	TRUE;

ERR:
	DebugW(L"UpdateClip sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(upd_clip);
	return	FALSE;
}

int LogDb::FlipFromTo()	// for old import data
{
	int				ret = 0;
	sqlite3_stmt	*upd_fromto = NULL;
	int				uidx = 1;

	if ((ret = sqlite3_prepare16(sqlDb, UPDATEFROMTOFLIP_SQL, -1, &upd_fromto, NULL))) {
		goto ERR;
	}
	if ((ret = sqlite3_step(upd_fromto)) != SQLITE_DONE) {
		goto ERR;
	}

	sqlite3_finalize(upd_fromto);
	return	ret;

ERR:
	DebugW(L"FlipFromTo sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(upd_fromto);
	return	ret;
}

int LogDb::CommentFlags()	// for old data
{
	int				ret = 0;
	sqlite3_stmt	*upd_cmntflags = NULL;
	int				uidx = 1;

	if ((ret = sqlite3_prepare16(sqlDb, UPDATECOMMENTFLAGS_SQL, -1, &upd_cmntflags, NULL))) {
		goto ERR;
	}
	if ((ret = sqlite3_step(upd_cmntflags)) != SQLITE_DONE) {
		goto ERR;
	}

	sqlite3_finalize(upd_cmntflags);
	return	ret;

ERR:
	DebugW(L"CommentFlags sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(upd_cmntflags);
	return	ret;
}

int LogDb::ConvertFtsBody()	// for old data
{
	int				ret = 0;
	sqlite3_stmt	*sel_msg = NULL;
	sqlite3_stmt	*ins_ftsbody = NULL;
	Wstr			body(MAX_MSGBODY);
	Wstr			ngram(MAX_MSGBODY);

	if ((ret = sqlite3_exec(sqlDb,
		"drop table msg_fts_tbl;"
		, 0, 0, &sqlErr)) != SQLITE_OK) {
		goto ERR;
	}
	if ((ret = sqlite3_exec(sqlDb,
		"create virtual table msg_fts_tbl using fts4( \n"
		"	msg_id integer primary key, \n"
		"	body text, \n"
		"	tokenize=simple \" \" \" \" "
		"); "
		, 0, 0, &sqlErr)) != SQLITE_OK) {
		goto ERR;
	}

#define SELIDX_SQL		L"select msg_id,body from msg_tbl"
#define INSBODYFTS_SQL	L"insert into msg_fts_tbl (msg_id, body) values(?, ?)"

	if ((ret = sqlite3_prepare16(sqlDb, SELIDX_SQL, -1, &sel_msg, NULL))) {
		goto ERR;
	}
	if ((ret = sqlite3_prepare16(sqlDb, INSBODYFTS_SQL, -1, &ins_ftsbody, NULL))) {
		goto ERR;
	}

	while ((ret = sqlite3_step(sel_msg)) == SQLITE_ROW) {
		int		sidx = 0;
		int64	msg_id = sqlite3_column_int64(sel_msg, sidx++);

		wcsncpyz(body.Buf(), (WCHAR *)sqlite3_column_text16(sel_msg, sidx++), MAX_MSGBODY);
		gen_ngram(body.s(), ngram.Buf(), MAX_MSGBODY);

		int		fidx = 1;
		if (sqlite3_bind_int64(ins_ftsbody, fidx++, msg_id)) goto ERR;
		if (sqlite3_bind_text16(ins_ftsbody, fidx++, ngram.s(), -1, SQLITE_STATIC)) goto ERR;
		ret = sqlite3_step(ins_ftsbody);

		sqlite3_reset(ins_ftsbody);
		if (ret != SQLITE_DONE) {
			goto ERR;
		}
	}

	sqlite3_finalize(ins_ftsbody);
	sqlite3_finalize(sel_msg);
	return	ret;

ERR:
	DebugW(L"ConvertFtsBody sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(ins_ftsbody);
	sqlite3_finalize(sel_msg);
	return	ret;
}

#define SELMHOST_SQL	L"select msg_id,host_id,rowid from msghost_tbl order by msg_id,rowid"
#define SELHMSG_SQL		L"select flags from msg_tbl where msg_id=?"
#define SELSELFHOST_SQL	L"select count(host_id) from host_tbl where host_id=? and uid=? and host=?"
#define DELMHOST_SQL	L"delete from msghost_tbl where rowid=?"
#define UPDMHOST_SQL	L"update msghost_tbl set idx=? where rowid=?"

int LogDb::SetMsgHostIdx()	// for old data
{
	int				ret = 0;
	sqlite3_stmt	*sel_mhost = NULL;
	sqlite3_stmt	*sel_hmsg  = NULL;
	sqlite3_stmt	*sel_shost = NULL;
	sqlite3_stmt	*upd_mhost = NULL;
	sqlite3_stmt	*del_mhost = NULL;

	if ((ret = sqlite3_prepare16(sqlDb, SELMHOST_SQL, -1, &sel_mhost, NULL))) goto ERR;
	if ((ret = sqlite3_prepare16(sqlDb, SELHMSG_SQL, -1, &sel_hmsg, NULL))) goto ERR;
	if ((ret = sqlite3_prepare16(sqlDb, SELSELFHOST_SQL, -1, &sel_shost, NULL))) goto ERR;
	if ((ret = sqlite3_prepare16(sqlDb, DELMHOST_SQL, -1, &del_mhost, NULL))) goto ERR;
	if ((ret = sqlite3_prepare16(sqlDb, UPDMHOST_SQL, -1, &upd_mhost, NULL))) goto ERR;

	int64	sv_msgid = 0;
	int		flags = 0;
	int		cnt = 0;
	const WCHAR	*uid  = cfg->selfHost.uid.s();
	const WCHAR	*host = cfg->selfHost.host.s();

	while ((ret = sqlite3_step(sel_mhost)) == SQLITE_ROW) {
		int64	msg_id	= sqlite3_column_int64(sel_mhost, 0);
		int		host_id	= sqlite3_column_int(sel_mhost,   1);
		int		row_id	= sqlite3_column_int(sel_mhost,   2);

		if (msg_id != sv_msgid) {
			sv_msgid = msg_id;
			cnt = 0;
			continue;
		}
		cnt++;
		if (cnt == 1) {
			if ((ret = sqlite3_bind_int64(sel_hmsg, 1, msg_id))) goto ERR;
			if ((ret = sqlite3_step(sel_hmsg)) != SQLITE_ROW) goto ERR;
			flags = sqlite3_column_int(sel_hmsg, 0);
			sqlite3_reset(sel_hmsg);
		}
		if (flags & DB_FLAG_FROM) {
			if ((ret = sqlite3_bind_int64(sel_shost, 1, host_id))) goto ERR;
			if ((ret = sqlite3_bind_text16(sel_shost, 2, uid, -1, SQLITE_STATIC))) goto ERR;
			if ((ret = sqlite3_bind_text16(sel_shost, 3, host, -1, SQLITE_STATIC))) goto ERR;
			if ((ret = sqlite3_step(sel_shost)) != SQLITE_ROW) goto ERR;
			int is_self = sqlite3_column_int(sel_shost, 0);
			sqlite3_reset(sel_shost);

			if (is_self) {
				if ((ret = sqlite3_bind_int64(del_mhost, 1, row_id))) goto ERR;
				if ((ret = sqlite3_step(del_mhost)) != SQLITE_DONE) goto ERR;
				sqlite3_reset(del_mhost);
				cnt--;
				Debug("del self msg_id=%lld\n", msg_id);
				continue;
			}
		}
		if ((ret = sqlite3_bind_int(upd_mhost, 1, cnt))) goto ERR;
		if ((ret = sqlite3_bind_int(upd_mhost, 2, row_id))) goto ERR;
		if ((ret = sqlite3_step(upd_mhost)) != SQLITE_DONE) goto ERR;
		Debug("upd msg_id=%lld %d\n", msg_id, cnt);
		sqlite3_reset(upd_mhost);
	}

	sqlite3_finalize(sel_mhost);
	sqlite3_finalize(sel_hmsg);
	sqlite3_finalize(sel_shost);
	sqlite3_finalize(upd_mhost);
	sqlite3_finalize(del_mhost);

	return	ret;

ERR:
	DebugW(L"SetMsgHostIdx sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	sqlite3_finalize(sel_mhost);
	sqlite3_finalize(sel_hmsg);
	sqlite3_finalize(sel_shost);
	sqlite3_finalize(upd_mhost);
	sqlite3_finalize(del_mhost);
	return	ret;
}

/* ============================================
		Delete Data
  ============================================== */
#define DELBYLOGID_FMT	/* import_id * 7 */			\
	"begin;"										\
	" delete"										\
	" from clip_tbl"								\
	" where msg_id in"								\
	"   (select msg_id"								\
	"     from msg_tbl"								\
	"     where importid=%d);"						\
	" delete"										\
	" from file_tbl"								\
	" where msg_id in"								\
	"   (select msg_id"								\
	"     from msg_tbl"								\
	"     where importid=%d);"						\
	" delete"										\
	" from mark_tbl"								\
	" where msg_id in"								\
	"   (select msg_id"								\
	"     from msg_tbl"								\
	"     where importid=%d);"						\
	" delete"										\
	" from msghost_tbl"								\
	" where msg_id in"								\
	"   (select msg_id"								\
	"     from msg_tbl"								\
	"     where importid=%d);"						\
	" delete"										\
	" from host_tbl"								\
	" where host_id <> 0"							\
	"  and host_id not in"							\
	"   (select host_id"							\
	"     from msghost_tbl);"						\
	" delete"										\
	" from host_fts_tbl"							\
	" where host_id <> 0"							\
	"  and host_id not in"							\
	"   (select host_id"							\
	"     from msghost_tbl);"						\
	" delete"										\
	" from msg_fts_tbl"								\
	" where msg_id in"								\
	"   (select msg_id"								\
	"     from msg_tbl"								\
	"     where importid=%d);"						\
	" delete"										\
	" from msg_tbl"									\
	" where importid=%d;"							\
	" delete"										\
	" from log_tbl"									\
	" where importid=%d;"							\
	"commit;"

#define DELBYMSGID_FMT	/* msg_id * 6 */			\
	" delete"										\
	"  from clip_tbl"								\
	"  where msg_id=%lld;"							\
	" delete"										\
	"  from file_tbl"								\
	"  where msg_id=%lld;"							\
	" delete"										\
	"  from mark_tbl"								\
	"  where msg_id=%lld;"							\
	" delete"										\
	"  from msghost_tbl"							\
	"  where msg_id=%lld;"							\
	" delete"										\
	"  from msg_fts_tbl"							\
	"  where msg_id=%lld;"							\
	" delete"										\
	"  from msg_tbl"								\
	"  where msg_id=%lld;"							\


BOOL LogDb::Begin()
{
	if (isBegin) {
		Debug("Begin already enabled\n");
		return	FALSE;
	}
	int ret = 0;
	if ((ret = sqlite3_exec(sqlDb, "begin;", 0, 0, &sqlErr)) != SQLITE_OK) {
		Debug("Begin sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		sqlite3_exec(sqlDb, "rollback;", 0, 0, &sqlErr);
		return	FALSE;
	}
	isBegin = TRUE;
	return	TRUE;
}

BOOL LogDb::Commit()
{
	if (!isBegin) {
		Debug("Commit, but not begin\n");
		return	FALSE;
	}
	isBegin = FALSE;

	int	ret = 0;
	if ((ret = sqlite3_exec(sqlDb, "commit;", 0, 0, &sqlErr)) != SQLITE_OK) {
		Debug("Commit sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		sqlite3_exec(sqlDb, "rollback;", 0, 0, &sqlErr);
		return	FALSE;
	}

	return	TRUE;
}

BOOL LogDb::DeleteOneData(int64 msg_id)
{
	char	buf[8192];
	int64	&id = msg_id;	// only alias
	BOOL	is_begin = isBegin;

	if (!is_begin) {
		Begin();
	}

	snprintfz(buf, sizeof(buf), DELBYMSGID_FMT, id, id, id, id, id, id);
	int ret = sqlite3_exec(sqlDb, buf, 0, 0, &sqlErr);

	if (!is_begin) {
		if (!Commit() && ret == SQLITE_OK) {
			ret = SQLITE_ERROR;
		}
	}

	if (ret != SQLITE_OK) {
		DebugW(L"DeleteOneData sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
		return FALSE;
	}

	return	TRUE;
}


BOOL LogDb::DeleteImportData(int importid)
{
	char	buf[8192];
	int		&id = importid;	// only alias

	snprintfz(buf, sizeof(buf), DELBYLOGID_FMT, id, id, id, id, id, id, id);
	int ret = sqlite3_exec(sqlDb, buf, 0, 0, &sqlErr);

	if (ret == SQLITE_OK) {
		return TRUE;
	}

	sqlite3_exec(sqlDb, "rollback;", 0, 0, &sqlErr);
	DebugW(L"DeleteImportData sqlerr=%s ret=%d\n", sqlite3_errmsg16(sqlDb), ret);
	return	FALSE;
}

/*
#define SELMSGALL_SQL	L"select msg_id, cmd, flags, body, lines from msg_tbl order by msg_id"
BOOL LogDb::SelectAllData(unordered_map<u_int, LogMsg *> *fullMsgMap)
{
	sqlite3_stmt	*sel_msg   = NULL;
	sqlite3_stmt	*sel_mhost = NULL;
	sqlite3_stmt	*sel_mclip  = NULL;
	sqlite3_stmt	*sel_mfile = NULL;

	sqlite3_prepare16(sqlDb, SELMSGALL_SQL,  -1, &sel_msg,   NULL);
	sqlite3_prepare16(sqlDb, SELMSGHOST_SQL, -1, &sel_mhost, NULL);
	sqlite3_prepare16(sqlDb, SELMSGCLIP_SQL, -1, &sel_mcilp, NULL);
	sqlite3_prepare16(sqlDb, SELMSGFILE_SQL, -1, &sel_mfile, NULL);

	while (sqlite3_step(sel_msg) == SQLITE_ROW) {
		LogMsg	*msg = new LogMsg;
		msg->msg_id = sqlite3_column_int(sel_msg, 0);
		msg->date   = sqlite3_column_int(sel_msg, 1);
//		msg->cmd    = sqlite3_column_int(sel_msg, 2);
		msg->flags  = sqlite3_column_int(sel_msg, 3);
		msg->body   = (WCHAR *)sqlite3_column_text16(sel_msg, 4);
//		msg->lines  = sqlite3_column_int(sel_msg, 5);

		sqlite3_bind_int(sel_mhost, 1, msg->msg_id);

		while (sqlite3_step(sel_mhost) == SQLITE_ROW) {
			LogHost host;
			host.uid	= (WCHAR *)sqlite3_column_text16(sel_mhost, 0);
			host.nick	= (WCHAR *)sqlite3_column_text16(sel_mhost, 1);
			host.host	= (WCHAR *)sqlite3_column_text16(sel_mhost, 2);
			host.addr	= (WCHAR *)sqlite3_column_text16(sel_mhost, 3);
			msg->host.push_back(host);
		}
		sqlite3_reset(sel_mhost);

		sqlite3_bind_int(sel_mclip, 1, msg->msg_id);
		while (sqlite3_step(sel_mclip) == SQLITE_ROW) {
			msg->clip.push_back(Wstr((WCHAR *)sqlite3_column_text16(sel_mclip, 0)));
		}
		sqlite3_reset(sel_mclip);

		sqlite3_bind_int(sel_mfile, 1, msg->msg_id);
		while (sqlite3_step(sel_mfile) == SQLITE_ROW) {
			msg->clip.push_back(Wstr((WCHAR *)sqlite3_column_text16(sel_mfile, 0)));
		}
		sqlite3_reset(sel_mfile);

		(*fullMsgMap)[msg->msg_id] = msg;
	}
	sqlite3_reset(sel_msg);

	sqlite3_finalize(sel_msg);
	sqlite3_finalize(sel_mhost);
	sqlite3_finalize(sel_mclip);
	sqlite3_finalize(sel_mfile);

	return TRUE;
}*/


static int gen_ngram(const WCHAR *src, WCHAR *dst, int max_buf, int src_len)
{
	const WCHAR	*src_sv = src;
	WCHAR		*p = dst;
	WCHAR		*end = p + max_buf - 2;

	while ((src_len == -1 || (src - src_sv) < src_len) && *src && p < end) {
		if (p != dst && (*src < 0xdc00 || *src >= 0xdf00)) {
			*p++ = ' ';
		}
		*p++ = *src++;
	}
	*p = 0;

	return	int(p - dst);
}

static int gen_ngram_str(const WCHAR *key, WCHAR *wbuf, int max_size)
{
	WCHAR	*p = wbuf;

	*p++ = '"';
	p += gen_ngram(key, p, max_size - 3);
	*p++ = '"';
	*p = 0;

	return	int(p - wbuf);
}

static const WCHAR *phrase_top(const WCHAR *s)
{
	if (!s) {
		return	NULL;
	}
	while (*s == ' ') {
		s++;
	}
	return	*s ? s : NULL;
}

static const WCHAR *phrase_next(const WCHAR *s)
{
	if (!s || !*s) {
		return	 NULL;
	}
	WCHAR		ch = (*s == '"') ? '"' : ' ';
	const WCHAR	*end = wcschr(s+1, ch);

	return	end ? end : (s + wcslen(s));
}

static int gen_ngram_multi_str(const WCHAR *_key, WCHAR *wbuf, int max_size)
{
	Wstr	key = _key;
	WCHAR	*sv;
	WCHAR	*tok = quote_tok(key.Buf(), ' ', &sv);
	WCHAR	*p = wbuf;
	BOOL	is_normal_tok = FALSE;

	for ( ; tok; tok=quote_tok(NULL, ' ', &sv)) {
		if (is_normal_tok && IsDbOperatorToken(tok)) {	// 2項演算子のみサポート
			*p++ = ' ';
			p += wcscpyz(p, tok);
			*p++ = ' ';
			is_normal_tok = FALSE;
		}
		else {
			*p++ = '"';
			p += gen_ngram(tok, p, max_size - int(p - wbuf) - 3);
			*p++ = '"';
			is_normal_tok = TRUE;
		}

		if (p - wbuf >= max_size - 4) {
			break;
		}
	}
	*p = 0;

	return	int(p - wbuf);
}

static int gen_like_str(const WCHAR *key, WCHAR *wbuf, int max_size)
{
	WCHAR	*p = wbuf;

	*p++ = '%';
	p += wcsncpyz(p, key, max_size-2);
	*p++ = '%';
	*p = 0;

	return	int(p - wbuf);
}

static int gen_2gram(const WCHAR *src, WCHAR *dst, int max_buf)
{
	WCHAR	*p = dst;
	WCHAR	*end = p + max_buf - 3;

	while (*src && *(src+1) && p < end) {
		if (p != dst) *p++ = ' ';
		*p++ = *src++;
		*p++ = *src;
	}
	*p = 0;
	return	int(p - dst);
}

struct FetchParam {
	LogDb		*self;
	const WCHAR	*path;
	HWND		hWnd;
	UINT		doneMsg;
	BOOL		withVacuum;
};

void LogDb::PrefetchFileProc(void *_param)
{
	FetchParam *param = (FetchParam *)_param;
	TTick	t;

	HANDLE	fh = ::CreateFileW(param->path, GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (fh != INVALID_HANDLE_VALUE) {
		DWORD	size = ::GetFileSize(fh, 0);
		if (HANDLE hMap = ::CreateFileMapping(fh, 0, PAGE_READONLY, 0, 0, NULL)) {
			if (char *top = (char *)::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) {
				DWORD	size = ::GetFileSize(fh, 0);
				char	*end = top + size - 1; // valid end point
				DWORD	dummy = 0;

				__try {
					for (char *p=top; p < end; p += PAGE_SIZE) {
						dummy += *p;
					}
					dummy += *end;
				}
				__except(EXCEPTION_EXECUTE_HANDLER) {	// avoid "In Page Error" (= disconnect network drive)
				}

				Debug("fetch fin = %d tick size=%dKB  (%x)\n", t.elaps(), size/1024, dummy);
				::UnmapViewOfFile(top);

				if (param->withVacuum) {
					param->self->Vacuum();
				}

				if (param->hWnd && param->doneMsg) {
					::PostMessage(param->hWnd, param->doneMsg, 0, 0);
				}
			}
			else DebugW(L" fetch view err(%s %d)\n", param->path, GetLastError());
			::CloseHandle(hMap);
		}
		else DebugW(L" fetch map err(%s %d)\n", param->path, GetLastError());
		::CloseHandle(fh);
	}
	else DebugW(L" fetch open err(%s %d)\n", param->path, GetLastError());

	delete param;
}

int LogDb::PrefetchFile(const WCHAR *path, HWND hWnd, UINT doneMsg, BOOL with_vacuum)
{
	FetchParam *param = new FetchParam;
	param->self    = this;
	param->path    = path;
	param->hWnd    = hWnd;
	param->doneMsg = doneMsg;
	param->withVacuum = with_vacuum;
	_beginthread(PrefetchFileProc, 0, (void *)param);
	return	0;
}


