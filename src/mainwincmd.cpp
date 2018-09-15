static char *mainwincmd_id = 
	"@(#)Copyright (C) H.Shirouzu 2017   mainwincmd.cpp	Ver4.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window
	Create					: 2017-05-27(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

using namespace std;

BOOL TMainWin::Cmd(HWND targ_wnd)
{
	IPIpc	ipc;
	IPDict	in;
	IPDict	out;

	if (!cfg->IPDictEnabled()) {
		return	FALSE;
	}

	if (!ipc.LoadDictFromMap(targ_wnd, TRUE, &in)) {
		return	FALSE;
	}

	BOOL	auto_res = TRUE;
	int64	cmd = 0;
	in.get_int(IPMSG_CMD_KEY, &cmd);
	out.put_int(IPMSG_STAT_KEY, -1);

	switch(cmd) {
	case IPMSG_CMD_GETABSENCE:
		CmdGetAbs(in, &out);
		break;

	case IPMSG_CMD_SETABSENCE:
		CmdSetAbs(in, &out);
		break;

	case IPMSG_CMD_GETHOSTLIST:
		CmdGetHostList(in, &out);
		break;

	case IPMSG_CMD_REFRESH:
		CmdRefresh(in, &out);
		break;

	case IPMSG_CMD_SENDMSG:
		if (CmdSendMsg(in, &out, targ_wnd)) {
			auto_res = FALSE;
		}
		break;

	case IPMSG_CMD_RECVMSG:
		if (CmdRecvMsg(in, &out, targ_wnd)) {
			auto_res = FALSE;
		}
		break;

	case IPMSG_CMD_GETSTATE:
		CmdGetState(in, &out);
		break;

	case IPMSG_CMD_TERMINATE:
		CmdTerminate(in, &out);
		break;

	default:
		out.put_str(IPMSG_ERRINFO_KEY, Fmt("cmd not found(%d)", cmd));
		break;
	}

	if (auto_res) {
		ipc.SaveDictToMap(targ_wnd, FALSE, out);
		::SendMessage(targ_wnd, WM_IPMSG_CMDRES, 0, 0);
	}

	return	TRUE;
}

BOOL TMainWin::CmdGetAbs(const IPDict &in, IPDict *out)
{
	if (cfg->AbsenceCheck) {
		out->put_str(IPMSG_ABSTITLE_KEY, cfg->AbsenceHead[cfg->AbsenceChoice]);
		out->put_int(IPMSG_STAT_KEY, cfg->AbsenceChoice + 1);
	}
	else {
		out->put_int(IPMSG_STAT_KEY, 0);
	}
	return	TRUE;
}

BOOL TMainWin::CmdSetAbs(const IPDict &in, IPDict *out)
{
	int64	val = 0;

	if (!in.get_int(IPMSG_ABSMODE_KEY, &val)) {
		out->put_str(IPMSG_ERRINFO_KEY, "setabs value not found");
		return	FALSE;
	}

	if (val < 0 || val > cfg->AbsenceMax) {
		out->put_str(IPMSG_ERRINFO_KEY, Fmt("setabs range err(%d)", val));
		return	FALSE;
	}

	if (val == 0) {
		cfg->AbsenceCheck = 0;
	}
	else if (val >= 1 && val <= cfg->AbsenceMax) {
		cfg->AbsenceCheck = 1;
		cfg->AbsenceChoice = INT_RDC(val) - 1;
	}
	BroadcastEntry(IPMSG_BR_NOTIFY);
	SetIcon(cfg->AbsenceCheck);

	out->put_int(IPMSG_STAT_KEY, 0);
	return	TRUE;
}

BOOL TMainWin::CmdGetHostList(const IPDict &in, IPDict *out)
{
	IPDictList	dlist;

#ifdef IPMSG_PRO
	THosts &targ_hosts = hosts;
#else
	THosts &targ_hosts = (cfg->DirMode == DIRMODE_MASTER) ? allHosts : hosts;
#endif

	for (int idx=0; idx < targ_hosts.HostCnt(); idx++) {
		Host	*host = targ_hosts.GetHost(idx);

		auto hdict = make_shared<IPDict>();
		MakeHostDict(host, hdict);
		dlist.push_back(hdict);
	}
	out->put_dict_list(IPMSG_HOSTLIST_KEY, dlist);

	out->put_int(IPMSG_STAT_KEY, targ_hosts.HostCnt());
	return	TRUE;
}

BOOL TMainWin::CmdRefresh(const IPDict &in, IPDict *out)
{
	int64	flags = 0;

	in.get_int(IPMSG_FLAGS_KEY, &flags);

	RefreshHost((flags & IPMSG_REMOVE_FLAG) ? TRUE : FALSE);
	out->put_int(IPMSG_STAT_KEY, 0);
	return	TRUE;
}

Host *TMainWin::FindHostForCmd(shared_ptr<U8str> u)
{
	auto s = u->s();

	for (int i=0; i < hosts.HostCnt(); i++) {
		auto	h = hosts.GetHost(i);
		if (!strcmp(h->hostSub.u.userName, s)
		 || !strcmp(h->hostSub.u.hostName, s)
		 || !strcmp(h->hostSub.addr.S(), s)) {
			return	h;
		}
	}
	time_t disp_time = time(NULL) - cfg->DispHostTime;
	for (int i=0; i < cfg->priorityHosts.HostCnt(); i++) {
		auto	h = cfg->priorityHosts.GetHost(i);
		if (h->updateTime > disp_time) {
			if (!strcmp(h->hostSub.u.userName, s)) {
				return	h;
			}
		}
	}
	return	NULL;
}

BOOL TMainWin::CmdSendMsg(const IPDict &in, IPDict *out, HWND targ_wnd)
{
	ReplyInfo	rInfo;
	int64		flags = 0;

	in.get_int(IPMSG_FLAGS_KEY, &flags);

	rInfo.cmdHWnd  = targ_wnd;
	rInfo.cmdFlags = flags;

	// 宛先
	IPDictStrList	ipt_list;
	if (!in.get_str_list(IPMSG_TOLIST_KEY, &ipt_list)) {
		out->put_str(IPMSG_ERRINFO_KEY, "to not found");
		return FALSE;
	}

	vector<HostSub>	to_vec;

	if (ipt_list.size() == 1 && **(ipt_list.begin()) == "ALL") {
		for (int i=0; i < hosts.HostCnt(); i++) {
			to_vec.push_back(hosts.GetHost(i)->hostSub);
		}
	}
	else {
		for (auto &u: ipt_list) {
			Host *host = FindHostForCmd(u);
			if (!host) {
				out->put_str(IPMSG_ERRINFO_KEY, Fmt("user not found(%s)", u->s()));
				return FALSE;
			}
			to_vec.push_back(host->hostSub);
		}
	}
	if (to_vec.size() == 0) {
		out->put_str(IPMSG_ERRINFO_KEY, "no user");
		return FALSE;
	}
	rInfo.replyList = &to_vec;

	// 添付ファイル
	IPDictStrList				ipf_list;
	vector<shared_ptr<U8str>>	f_vec;

	if (in.get_str_list(IPMSG_FILELIST_KEY, &ipf_list)) {
		for (auto &u: ipf_list) {
			if (GetFileAttributesU8(u->s()) == 0xffffffff) {
				out->put_str(IPMSG_ERRINFO_KEY, Fmt("file not found %s", u->s()));
				return FALSE;
			}
			f_vec.push_back(u);
		}
		rInfo.fileList = &f_vec;
	}

	// 本文
	U8str	body;
	if (in.get_str(IPMSG_BODY_KEY, &body)) {
		rInfo.body = &body;
	}
	else if (f_vec.size() == 0) {
		out->put_str(IPMSG_ERRINFO_KEY, "no body and files");
		return FALSE;
	}

	return	SendDlgOpen(0, &rInfo);
}


BOOL TMainWin::CmdRecvMsg(const IPDict &in, IPDict *out, HWND targ_wnd)
{
	RecvCmd		rcmd = {};

	in.get_int(IPMSG_FLAGS_KEY, &rcmd.flags);
	rcmd.hWnd = targ_wnd;

	recvCmdVec.push_back(rcmd);
	return	TRUE;
}

BOOL TMainWin::CmdGetState(const IPDict &in, IPDict *out)
{
	U8str	u(MAX_UDPBUF);
	char	*buf = u.Buf();

	snprintfz(buf, MAX_UDPBUF, IPMSG_FULLNAME " ver%s\n", GetVersionStr());

	out->put_str(IPMSG_BODY_KEY, u.s());
	out->put_int(IPMSG_STAT_KEY, 0);
	return	TRUE;
}

BOOL TMainWin::CmdTerminate(const IPDict &in, IPDict *out)
{
	PostMessage(WM_COMMAND, IDCANCEL, 0);
	out->put_int(IPMSG_STAT_KEY, 0);
	return	TRUE;
}

