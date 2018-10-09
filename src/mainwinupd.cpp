static char *mainwinupd_id = 
	"@(#)Copyright (C) H.Shirouzu 2017-2018   mainwinupd.cpp	Ver4.90";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window Update routine
	Create					: 2017-08-27(Sun)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

using namespace std;

#ifndef IPMSG_PRO

//#define UPDATE_DBG
#ifdef UPDATE_DBG
#undef  IPMSG_UPDATEINFO
#define IPMSG_UPDATEINFO	"ipmsg-update-tmp.dat"
#endif

void TMainWin::UpdateCheckTimer()
{
	if (lastExitTick) return;

	time_t	now = time(NULL);

	if (now - cfg->updateLast > cfg->updateSpan) {
		UpdateCheck(UPD_BUSYCONFIRM |
			(((cfg->updateFlag & Cfg::UPDATE_MANUAL || !UpdateWritable())) ? UPD_CONFIRM : 0));
	}
}

BOOL TMainWin::UpdateWritable(BOOL force)
{
	static BOOL is_writable = [&]() {
		return	force ? FALSE : IsWritableDirW(U8toWs(cfg->execDir));
	}();

	if (force) {
		is_writable = IsWritableDirW(U8toWs(cfg->execDir));
	}

	return	is_writable;
}

BOOL TMainWin::UpdateCheck(DWORD flags, HWND targ_wnd)
{
	Debug("UpdateCheck flags=%x hwnd=%p\n", flags, targ_wnd);
	auto now = time(NULL);
	cfg->updateLast = now;
	recvIdx = [=]() {
		auto d = (now - cfg->LastRecv) / (3600 * 24);
		if (d <=   1) return 9;
		if (d <=   3) return 8;
		if (d <=   7) return 7;
		if (d <=  15) return 6;
		if (d <=  30) return 5;
		if (d <=  60) return 4;
		if (d <= 120) return 3;
		if (d <= 180) return 2;
		if (d <= 360) return 1;
		return 0;
	}();

	updData.flags = flags;
	updData.hWnd = targ_wnd;
	updRetry = FALSE;
	SetUserAgent();
	TInetAsync(IPMSG_SITE, IPMSG_UPDATEINFO, hWnd, WM_IPMSG_UPDATERES);

	return	TRUE;
}

BOOL TMainWin::UpdateCheckResCore(TInetReqReply *irr, BOOL *need_update)
{
	*need_update = FALSE;

	if (irr->reply.UsedSize() == 0 || irr->code != HTTP_STATUS_OK) {
		if (irr->errMsg.Len() == 0) {
			irr->errMsg = Fmt("Can't download update-info status=%d", irr->code);
		}
		return FALSE;
	}

	IPDict	dict(irr->reply.Buf(), irr->reply.UsedSize());

	if (!CheckVerifyIPDict(cfg->priv[KEY_2048].hPubCsp, &dict, &cfg->officialPub)) {
		irr->errMsg = Fmt("%s: verify error", IPMSG_UPDATEINFO); 
		return FALSE;
	}

	U8str	ts;
	if (dict.get_str("time", &ts)) {
		Debug("time=%s\n", ts.s());
	}

#ifdef _WIN64
	char key[10] = "x64";
#else
	char key[10] = "x86";
#endif
	if (!gEnableHook) {
		strcat(key, "-n");
	}

	IPDict	data;

	if (!dict.get_dict(key, &data)) {
		irr->errMsg = Fmt("%s: key not found(%s)", IPMSG_UPDATEINFO, key); 
		return FALSE;
	}

	updData.DataInit();
	if (!data.get_str("ver", &updData.ver)) {
		irr->errMsg = Fmt("%s: ver not found", IPMSG_UPDATEINFO);
		return FALSE;
	}

	if (!data.get_str("path", &updData.path)) {
		irr->errMsg = Fmt("%st: path not found", IPMSG_UPDATEINFO);
		return FALSE;
	}
	if (!data.get_bytes("hash", &updData.hash) || updData.hash.UsedSize() != SHA256_SIZE) {
		irr->errMsg = Fmt("%s: hash not found or size(%zd) err",
			IPMSG_UPDATEINFO, updData.hash.UsedSize());
		return FALSE;
	}
	if (!data.get_int("size", &updData.size)) {
		irr->errMsg = Fmt("%s: size not found err", IPMSG_UPDATEINFO);
		return FALSE;
	}

	double	self_ver = VerStrToDouble(GetVersionStr(TRUE));
	double	new_ver  = VerStrToDouble(updData.ver.s());

	Debug("ver=%s path=%s hash=%zd size=%lld ver=%.8f/%.8f\n",
		updData.ver.s(), updData.path.s(), updData.hash.UsedSize(), updData.size,
		self_ver, new_ver);

	if (self_ver >= new_ver) {
		irr->errMsg = Fmt("Not need update (%s -> %s)", GetVersionStr(TRUE), updData.ver.s());
		cfg->WriteRegistry(CFG_UPDATEOPT);
		return TRUE;
	}
	*need_update = TRUE;
	irr->errMsg = Fmt("Need update (%s -> %s)", GetVersionStr(TRUE), updData.ver.s());

	return	TRUE;
}

void TMainWin::UpdateCheckRes(TInetReqReply *irr)
{
	BOOL	need_update = FALSE;
	BOOL	ret = UpdateCheckResCore(irr, &need_update);

	if (ret && need_update) {
		auto idle_state = CheckUpdateIdle();

		if (updData.flags & UPD_STEP) {
			//
		}
		else if (updData.flags & UPD_CONFIRM) {
		//	BalloonWindow(TRAY_UPDATE, LoadStrU8(IDS_NEEDUPDATE), IP_MSG, ADAY_MSEC, TRUE);
			PostMessage(WM_IPMSG_UPDATEDLG, 0, 0);
		}
		else if (!UpdateWritable()) {
			UpdateExec();
		}
		else if (idle_state == UPDI_TRANS ||
			idle_state == UPDI_WIN && (updData.flags & UPD_BUSYCONFIRM)) {
			BalloonWindow(TRAY_UPDATE, LoadStrU8(IDS_UPDATEWAIT), IP_MSG, ADAY_MSEC, TRUE);
			updRetry = TRUE;
		}
		else {
			UpdateExec();
		}
	}

	if (updData.hWnd) {
		UpdateReply	ur = { updData.ver, irr->errMsg, need_update };
		::SendMessage(updData.hWnd, WM_IPMSG_UPDINFORESULT, ret, (LPARAM)&ur);
	}
	else if (!ret && (updData.flags & UPD_SHOWERR)) {
		BalloonWindow(TRAY_NORMAL, Fmt("%s\r\n%.300s", LoadStrU8(IDS_UPDATEFAIL),
			irr->errMsg.s()), IP_MSG, ERR_MSEC);
	}
	else if (ret && !need_update && (updData.flags & UPD_SKIPNOTIFY)) {
		BalloonWindow(TRAY_NORMAL, LoadStrU8(IDS_LATESTVER), IP_MSG, ERR_MSEC);
	}

	delete irr;
}

BOOL TMainWin::UpdateExec()
{
	if (updData.path.Len() <= 0) {
		return FALSE;
	}
	TInetAsync(IPMSG_SITE, updData.path.s(), hWnd, WM_IPMSG_UPDATEDLRES);
	return	TRUE;
}

void TMainWin::UpdateDlRes(TInetReqReply *irr)
{
	BYTE	hash[SHA256_SIZE] = {};
	TDigest	d;
	BOOL	ret = FALSE;

	if (irr->reply.UsedSize() <= 0) {
		irr->errMsg = Fmt("Update download err %zd", irr->reply.UsedSize());
		goto END;
	}

	if (!d.Init(TDigest::SHA256)) {
		irr->errMsg = Fmt("Update digest init err");
		goto END;
	}

	Debug("UpdateDlRes: reply size=%zd, code=%d\n", irr->reply.UsedSize(), irr->code);

	if (irr->code != HTTP_STATUS_OK) {
		irr->errMsg = Fmt("Download error status=%d len=%lld", irr->code, irr->reply.UsedSize());
		goto END;
	}

	if ((int64)irr->reply.UsedSize() != updData.size) {
		irr->errMsg = Fmt("Update size not correct %lld / %lld",
			irr->reply.UsedSize(), updData.size);
		goto END;
	}

	if ((int64)irr->reply.UsedSize() != updData.size) {
		irr->errMsg = Fmt("Update size not correct %lld / %lld",
			irr->reply.UsedSize(), updData.size);
		goto END;
	}

	if (!d.Update(irr->reply.Buf(), (int)irr->reply.UsedSize())) {
		irr->errMsg = Fmt("Update digest update err");
		goto END;
	}
	if (!d.GetVal(hash)) {
		irr->errMsg = Fmt("Update get digest err");
		goto END;
	}

	if (memcmp(hash, updData.hash.Buf(), SHA256_SIZE)) {
		irr->errMsg = Fmt("Update verify error");
		for (int i=0; i < SHA256_SIZE; i++) {
			Debug("%02d: %02x %02x\n", i, hash[i], updData.hash.Buf()[i]);
		}
		goto END;
	}
	updData.dlData = irr->reply;
	Debug("UpdateDlRes: OK %zd\n", irr->reply.UsedSize());

	ret = UpdateProc(&irr->errMsg);

END:
	if (updData.hWnd) {
		UpdateReply	ur = { updData.ver, irr->errMsg, TRUE };
		::SendMessage(updData.hWnd, WM_IPMSG_UPDDATARESULT, !irr->errMsg.Len(), (LPARAM)&ur);
	}
	else if (!ret && (updData.flags & UPD_SHOWERR)) {
		BalloonWindow(TRAY_NORMAL, Fmt("%s\r\n%.300s", LoadStrU8(IDS_UPDATEFAIL),
			irr->errMsg.s()), IP_MSG, ERR_MSEC);
	}

	delete irr;
}

BOOL TMainWin::UpdateProc(U8str *errMsg)
{
	char	path[MAX_PATH_U8];

	GenUpdateFileName(path, UpdateWritable() ? FALSE : TRUE);

	if (GetFileAttributesU8(path) != 0xffffffff) {
		if (!DeleteFileU8(path)) {
			char	bak[MAX_PATH_U8];
			snprintfz(bak, sizeof(bak), "%s.bak", path);
			MoveFileExU8(path, bak, MOVEFILE_REPLACE_EXISTING);
		}
	}

	HANDLE hFile = CreateFileU8(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, 0);

	if (hFile == INVALID_HANDLE_VALUE) {
		if (errMsg) {
			*errMsg = Fmt("Update CreateFile err(%s) %d", path, GetLastError());
		}
		return FALSE;
	}
	DWORD	size = 0;
	if (!::WriteFile(hFile, updData.dlData.Buf(), (DWORD)updData.dlData.UsedSize(),
		&size, 0)) {
		::CloseHandle(hFile);
		if (errMsg) {
			*errMsg = Fmt("Update WriteFile err(%s) %d", path, GetLastError());
		}
		return FALSE;
	}
	::CloseHandle(hFile);

	WCHAR	opt[MAX_PATH * 2];
	snwprintfz(opt, wsizeof(opt), L"%s%s", U8toWs(path),
		UpdateWritable() ? L" /SILENT /INTERNAL" : L"");

	STARTUPINFOW		sui = { sizeof(sui) };
	PROCESS_INFORMATION pi = {};

#if 1
	if (!::CreateProcessW(NULL, opt, 0, 0, 0, CREATE_NO_WINDOW, 0, TGetExeDirW(), &sui, &pi)) {
		if (errMsg) {
			*errMsg = Fmt("Update WriteFile err(%s) %d", path, GetLastError());
		}
		return FALSE;
	}
	::CloseHandle(pi.hThread);
	::CloseHandle(pi.hProcess);

	cfg->WriteRegistry(CFG_UPDATEOPT);
#else
	Debug("CreateProcess skipped for debug\n");
#endif
	updData.DataInit();

	DebugW(L"UpdateProc: done(%s)\n", opt);
	return	TRUE;
}

BOOL TUpdConfim::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);
	SetDlgItemTextU8(BODY_STATIC, body.s());

	SetForceForegroundWindow();
	MoveCenter(TRUE);

	return	TRUE;
}

BOOL TUpdConfim::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID) {
	case IDOK:
		if (auto mainWin = GetMain()) {
			mainWin->UpdateCheck(UPD_SHOWERR);
		}
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

void TMainWin::ShowUpdateDlg()
{
	if (updConfirm->hWnd) {
		updConfirm->Destroy();
	}
	if (UpdateWritable()) {
		updConfirm->Create(Fmt(LoadStrU8(IDS_UPDATE_FMT), updData.ver.s()));
	}
	else {
		UpdateCheck(UPD_SHOWERR|UPD_BUSYCONFIRM);
	}
}

#endif

UpdIdleState TMainWin::CheckUpdateIdle()
{
	if (sendFileList.Num() > 0) {
		Debug("CheckUpdateIdle trans\n");
		return	UPDI_TRANS;
	}

	if (sendList.Num() > 0 || recvList.Num() > 0 /* || setupDlg->hWnd */) {
		Debug("CheckUpdateIdle win\n");
		return UPDI_WIN;
	}

	for (auto view=logViewList.TopObj(); view; view=logViewList.NextObj(view)) {
		if (view->hWnd) {
		Debug("CheckUpdateIdle2 win\n");
			return	UPDI_WIN;
		}
	}

	return	UPDI_IDLE;
}

void GenUpdateFileName(char *buf, BOOL use_tmp)
{
	char	path[MAX_PATH_U8] = "";
	char	dir[MAX_PATH_U8] = "";

	if (use_tmp) {
		WCHAR	wdir[MAX_PATH] = L"";
		::GetTempPathW(wsizeof(wdir), wdir);
		WtoU8(wdir, dir, MAX_PATH_U8);
	}
	else {
		GetModuleFileNameU8(NULL, path, sizeof(path));
		GetParentDirU8(path, dir);
	}

	MakePathU8(path, dir, UPDATE_FILENAME);

	strcpy(buf, path);
}

BOOL TMainWin::UpdateFileCleanup()
{
#ifndef IPMSG_PRO
	updData.DataInit();
#endif

	if (param.isUpdated || param.isUpdateErr) {
		char	path[MAX_PATH_U8];

		GenUpdateFileName(path);

		if (GetFileAttributesU8(path) == 0xffffffff) {
			param.isUpdated = FALSE;
			param.isUpdateErr = FALSE;
		}
		if (DeleteFileU8(path)) {
			param.isUpdated = FALSE;
			param.isUpdateErr = FALSE;
		}
		else {
			static int count = 0;
			if (count++ > 10) {
				param.isUpdated = FALSE;
				param.isUpdateErr = FALSE;
			}
		}
	}
	return	!param.isUpdated && !param.isUpdateErr;
}

