static char *sendmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 2018   sendmsg.cpp	Ver4.90";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Message Core
	Create					: 2018-09-01(Sat)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */
#include "resource.h"
#include "ipmsg.h"

using namespace std;

/*
	SendMsg の初期化
*/
SendMsg::SendMsg(Cfg *_cfg, MsgMng *_msgmng, int unum, ULONG _packetNo, HWND _cmdHWnd,
				bool _onceNotify, time_t _timestamp, bool _isSendMsg)
{
	msgMng		= _msgmng;
	cfg			= _cfg;
	packetNo	= _packetNo;
	cmdHWnd		= _cmdHWnd;
	onceNotify	= _onceNotify;
	timestamp	= _timestamp;
	isSendMsg	= _isSendMsg;
	sendEntry.reserve(unum);
}

/*
	SendMsg の終了
*/
SendMsg::~SendMsg()
{
}


void SendMsg::RegisterMsg(const char *msg)
{
	msgBuf.SetByStr(msg);
}

void SendMsg::RegisterHost(Host *host)
{
	sendEntry.push_back(make_shared<SendEntry>(host));
}

int SendMsg::RegisterShare(ShareInfo *shareInfo)
{
	int		share_len = 0;
	DynBuf	buf(MAX_UDPBUF * 2 / 3);
	shareInfo->EncodeMsg(buf, (int)buf.Size(), &shareDictList, &share_len);
	shareStr = buf.s();
	return	share_len;
}


void SendMsg::MakeDelayFoot(char *head)
{
	time_t	now = time(NULL);

	if (now < timestamp + 120) {
		*head = 0;
		return;
	}

	WCHAR	dstr[50];
	MakeDateStrEx(timestamp, dstr, now);

	sprintf(head, LoadStrU8(IDS_DELAYSENDFMT), WtoU8(dstr));
}

/*
	メッセージの作成
*/
bool SendMsg::MakeMsgPacket(SendEntry *ent)
{
	DynBuf	buf(MAX_UDPBUF);
	DynBuf	pktBuf(MAX_UDPBUF);
	bool	ret = false;
	DynBuf	msgTarg(msgBuf.Size() + 100);
	char	foot[100];

	MakeDelayFoot(foot);
	sprintf(msgTarg.S(), "%s%s", msgBuf.s(), foot);

	if ((ent->Host()->hostStatus & IPMSG_CAPIPDICTOPT) && cfg->IPDictEnabled()) {
		IPDict	dict;

		msgMng->InitIPDict(&dict, GET_MODE(ent->Command()), GET_OPT(ent->Command()), packetNo);
		if (ent->UseUlist()) {
			MakeUlistDict(ent, &dict);
		}
		if (shareDictList.size() > 0) {
			msgMng->AddFileShareDict(&dict, shareDictList);
		}
		LocalNewLineToUnix(msgTarg, buf, MAX_UDPBUF);
		msgMng->AddBodyDict(&dict, buf);
		msgMng->EncIPDict(&dict, &ent->Host()->pubKey, &pktBuf);
	}
	else {
		if (ent->Status() == ST_MAKECRYPTMSG) {
			char *ulist = NULL;
			if (ent->UseUlist()) {
				ulist = pktBuf;
				MakeUlistStr(ent, ulist);
			}
			ret = msgMng->MakeEncryptMsg(ent->Host(), packetNo, msgTarg,
				shareStr || ulist ? true : false, shareStr.Buf(), ulist, buf);
		}
		if (!ret) {
			ent->SetCommand(ent->Command() & ~IPMSG_ENCRYPTOPT);
			LocalNewLineToUnix(msgTarg, buf, MAX_UDPBUF);
		}

		int		msg_len = 0;
		msgMng->MakeMsg(pktBuf, packetNo, ent->Command(), buf.s(),
						(ent->Command() & IPMSG_ENCEXTMSGOPT)  != 0 ||
						(ent->Command() & IPMSG_FILEATTACHOPT) == 0 ? NULL : shareStr.Buf(),
						NULL, &msg_len);
		pktBuf.SetUsedSize(msg_len);
	}

	ent->SetMsg(pktBuf, (int)pktBuf.UsedSize());
	ent->SetStatus(ST_SENDMSG);

	return	true;
}

/*
	通常送信Sub routine
*/
bool SendMsg::Send(void)
{
	bool ret = false;

	for (auto &ent: sendEntry) {
		SendMsgEntry(ent.get());

		if (ent->Host()->active && ent->Status() != ST_DONE) { // 未完了ユーザが存在
			ret = true;
		}
	}

	return	ret;
}


//void tcp_send(MsgMng *msgMng, Addr addr, int portNo, const char *data, int len)
//{
//	ConnectInfo	info;
//	info.addr = addr;
//	info.port = portNo;
//	Debug("addr=%s\n", info.addr.S());
//
//	if (!msgMng->Connect(0, &info)) {
//		return;
//	}
//
//	if (::send(info.sd, (char *)data, len, 0) < len) {
//		::closesocket(info.sd);
//		info.sd = -1;
//		return;
//	}
//
//	fd_set	fd;
//	timeval	tv = { 1, 0 };
//	FD_ZERO(&fd);
//	FD_SET(info.sd, &fd);
//
//	int	sock_ret = ::select((int)info.sd + 1, &fd, NULL, NULL, &tv);
//	if (sock_ret > 0) {
//		char d;
//		if (::recv(info.sd, &d, 1, 0) > 0) {
//			::closesocket(info.sd);
//			return;
//		}
//	}
//	::closesocket(info.sd);
//}

int RetryMax(Cfg *cfg) {
	return cfg->RetryMax / 2;
}
/*
	通常送信Sub routine
*/
bool SendMsg::SendMsgEntry(SendEntry *ent)
{
	if (!ent->Host()->active) {
		Debug("no active\n");
		return false;
	}
	if (ent->Status() == ST_INIT) {
		ent->SetStatus((ent->Command() & IPMSG_ENCRYPTOPT) == 0 ? ST_MAKEMSG :
						ent->Host()->pubKey.Key() ? ST_MAKECRYPTMSG : ST_GETCRYPT);
	}
	if (ent->Status() == ST_GETCRYPT) {
		msgMng->GetPubKey(&ent->Host()->hostSub, false);
	}
	if (ent->Status() == ST_MAKECRYPTMSG || ent->Status() == ST_MAKEMSG) {
		MakeMsgPacket(ent);		// ST_MAKECRYPTMSG/ST_MAKEMSG -> ST_SENDMSG
	}
	if (ent->Status() == ST_SENDMSG) {
		msgMng->UdpSend(ent->Host()->hostSub.addr, ent->Host()->hostSub.portNo,
						ent->Msg(), ent->MsgLen(retryCnt >= RetryMax(cfg) ? true : false));
//		tcp_send(msgMng, ent->Host()->hostSub.addr, ent->Host()->hostSub.portNo,
//						ent->Msg(), ent->MsgLen(retryCnt >= RetryMax(cfg) ? true : false));
	}
	return	true;
}

void SendMsg::MakeHistMsg(SendEntry *ent, U8str *str)
{
	if (ent->Status() != ST_DONE) {
		*str = LoadStrU8(IDS_WAITSEND);
	}
	if (ent->Command() & IPMSG_FILEATTACHOPT) {
		int	id = ent->UseClip() ? ent->UseFile() ? IDS_FILEWITHCLIP :
			IDS_WITHCLIP : IDS_FILEATTACH;
		*str += LoadStrU8(id);
		*str += " ";
	}
	*str += msgBuf.s();
}

/*
	送信終了通知
	packet_noが、この SendDialogの送った送信packetであれば、true
*/
bool SendMsg::SendFinishNotify(HostSub *hostSub, ULONG packet_no)
{
	if (packet_no && packet_no != packetNo) return false;

	for (auto &ent: sendEntry) {
		if (ent->Status() == ST_SENDMSG &&
			ent->Host()->hostSub.addr == hostSub->addr &&
			ent->Host()->hostSub.portNo == hostSub->portNo &&
		(packet_no == packetNo || (packet_no == 0 && !IsSendMsg())))
		{
			ent->SetStatus(ST_DONE);
			if (IsSendMsg()) {
				bool is_secret = (ent->Command() & IPMSG_SECRETOPT) ? true : false;
				RegisterHist(ent.get(), is_secret, false);
			}
			if (IsSendFinish()) {	//再送MessageBoxU8を消す
				if (retryDlg.hWnd) {
					Debug("done=%p", retryDlg.hWnd);
					retryDlg.Destroy();
				}
			}
			return	true;
		}
	}
	return	false;
}

/*
	送信終了通知
	packet_noが、この SendDialogの送った送信packetであれば、true
*/
bool SendMsg::SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa)
{
	for (auto &ent: sendEntry) {
		if (ent->Status() == ST_GETCRYPT && IsSameHostEx(&ent->Host()->hostSub, hostSub))
		{
			if (!ent->Host()->pubKey.Key()) {
				ent->Host()->pubKey.Set(pubkey, len, e, capa);
			}
			ent->SetStatus(ST_MAKECRYPTMSG);
			SendMsgEntry(ent.get());
			return	true;
		}
	}
	return	false;
}

bool SendMsg::Del(HostSub *hostSub, ULONG packet_no)
{
	if (packet_no != packetNo) return false;

	for (auto &ent: sendEntry) {
		if (IsSameHostEx(&ent->Host()->hostSub, hostSub)) {
			ent->SetStatus(ST_DONE);
			return	true;
		}
	}

	return	false;
}

/*
	送信終了したかどうか
*/
bool SendMsg::IsSendFinish(void)
{
	bool	finish = true;

	for (auto &ent: sendEntry) {
		if (ent->Status() != ST_DONE) {
			finish = false;
			break;
		}
	}

	if (finish) {
		Finished();
	}

	return	finish;
}

void SendMsg::Finished()
{
	if (cmdHWnd && cfg->IPDictEnabled()) {
		IPIpc			ipc;
		IPDict			out;
		IPDictStrList	tl;
		int				num = 0;

		for (auto &ent: sendEntry) {
			auto	u = make_shared<U8str>(
				Fmt("%s : %s", (ent->Status()==ST_DONE) ? "OK" : "NG", ent->Host()->S()));
			tl.push_back(u);
			if (ent->Status() == ST_DONE) {
				num++;
			}
		}
		out.put_str_list(IPMSG_TOLIST_KEY, tl);
		out.put_int(IPMSG_STAT_KEY, num);

		ipc.SaveDictToMap(cmdHWnd, false, out);
		::SendMessage(cmdHWnd, WM_IPMSG_CMDRES, 0, 0);
		cmdHWnd = NULL;
	}
}

bool SendMsg::NeedActive(Host *host)
{
	for (auto &ent: sendEntry) {
		if (ent->Host() == host && ent->Status() != ST_DONE) {
			return	true;
		}
	}
	return	false;
}

bool SendMsg::TimerProc()
{
	if (IsSendFinish()) {
		return true;
	}
	if (retryCnt++ <= RetryMax(cfg)) {
		if (Send()) {
			return false;
		}
		retryCnt = RetryMax(cfg) + 1;
	}
	if (retryDlg.hWnd) {
		return false;
	}

	char	unsend[MAX_BUF];
	char	*p = unsend;
	UINT	command = 0;

	for (auto& ent: sendEntry) {
		if (ent->Status() == ST_DONE) continue;

		command = ent->Command();
		p += MakeListString(cfg, ent->Host(), p);
		p += strcpyz(p, "\r\n");
		if (p - unsend > MAX_BUF - 200) break;
	}
	if (cfg->DelaySend == 0 || !IsSendMsg()) {
		strncatz(unsend, LoadStrU8(IDS_RETRYSEND), MAX_BUF);
		retryDlg.SetFlags(TMsgBox::RETRY | TMsgBox::BIGX | TMsgBox::CENTER);
		int ret = cmdHWnd ? IDCANCEL : retryDlg.Exec(unsend, IP_MSG);

		if (ret == IDOK && !IsSendFinish()) {
			retryCnt = 0;
			Send();
			return	false;
		}
		Finished();
		return	true;
	}
	if (onceNotify) {
		onceNotify = FALSE;

		for (auto& ent: sendEntry) {
			if (ent->Status() == ST_DONE) continue;
			RegisterHist(ent.get(), true, true);
		}

		::PostMessage(GetMainWnd(), WM_DELAYMSG_BALLOON, 0,
			(LPARAM)strdupNew(Fmt("%s\r\n%s", LoadStrU8(IDS_DELAYSENDMSG), unsend)));
	}
	Finished();

	return	true;
}


void SendMsg::MakeUlistCore(SendEntry *self, std::vector<User> *uvec)
{
	uvec->push_back(self->Host()->hostSub.u);

	for (auto &ent: sendEntry) {
		if (ent.get() != self) {
			uvec->push_back(ent->Host()->hostSub.u);
		}
	}
}

void SendMsg::MakeUlistStr(SendEntry *self, char *ulist)
{
	vector<User>	uvec;

	MakeUlistCore(self, &uvec);

	msgMng->UserToUList(uvec, ulist);
}

void SendMsg::MakeUlistDict(SendEntry *self, IPDict *dict)
{
	vector<User>	uvec;

	MakeUlistCore(self, &uvec);

	msgMng->UserToUListDict(uvec, dict);
}

int SendMsg::GetUlistStrLen()
{
	DynBuf dbuf(MAX_UDPBUF);
	MakeUlistStr(sendEntry[0].get(), dbuf);
	return	(int)strlen(dbuf);
}

#define PKTNO_KEY		"PKT"
#define TIMESTAMP_KEY	"TM"
#define MSG_KEY			"MSG"
#define SHARESTR_KEY	"SHS"
#define SHAREDICT_KEY	"SHD"
#define ENTRY_KEY		"ENT"
#define UID_KEY			"UID"
#define HOST_KEY		"HST"
#define CMD_KEY			"CMD"
#define STAT_KEY		"STA"
#define ULIST_KEY		"ULS"
#define FILE_KEY		"FIL"
#define CLIP_KEY		"CLP"

bool SendMsg::Serial(IPDict *d)
{
	d->put_int(PKTNO_KEY, packetNo);
	d->put_int(TIMESTAMP_KEY, timestamp);
	d->put_str(MSG_KEY, msgBuf.s());
	d->put_str(SHARESTR_KEY, shareStr.s());
	d->put_dict_list(SHAREDICT_KEY, shareDictList);

	IPDictList	ed_list;
	for (auto &ent: sendEntry) {
		auto ed = make_shared<IPDict>();
		ed->put_str(UID_KEY, ent->Host()->hostSub.u.userName);
		ed->put_str(HOST_KEY, ent->Host()->hostSub.u.hostName);
		ed->put_int(STAT_KEY, ent->Status());
		ed->put_int(CMD_KEY, ent->Command());
		ed->put_int(ULIST_KEY, ent->UseUlist());
		ed->put_int(FILE_KEY, ent->UseFile());
		ed->put_int(CLIP_KEY, ent->UseClip());
		ed_list.push_back(ed);
	}
	d->put_dict_list(ENTRY_KEY, ed_list);

	return	true;
}

void SendMsg::SaveHost()
{
	for (auto &ent: sendEntry) {
		cfg->WriteHost(ent->Host());
	}
}

bool SendMsg::Deserial(const IPDict &d)
{
	int64	val;
	U8str	vs;

	if (!d.get_int(PKTNO_KEY, &val)) return false;
	packetNo = (ULONG)val;

	if (!d.get_int(TIMESTAMP_KEY, &timestamp)) return false;

	if (!d.get_str(MSG_KEY, &vs)) return false;
	msgBuf.SetByStr(vs.s());

	if (!d.get_str(SHARESTR_KEY, &shareStr)) return false;
	if (!d.get_dict_list(SHAREDICT_KEY, &shareDictList)) return false;

	IPDictList	ed_list;
	if (!d.get_dict_list(ENTRY_KEY, &ed_list)) return false;

	sendEntry.reserve(ed_list.size());

	for (auto &ed: ed_list) {
		HostSub	hs;
		auto	ent = make_shared<SendEntry>();

		if (!ed->get_str(UID_KEY, &vs)) return false;
		strncpyz(hs.u.userName, vs.s(), MAX_NAMEBUF);

		if (!ed->get_str(HOST_KEY, &vs)) return false;
		strncpyz(hs.u.hostName, vs.s(), MAX_NAMEBUF);

		if (!ed->get_int(STAT_KEY, &val)) return false;
		ent->SetStatus((SendStatus)(val == ST_DONE ? ST_DONE : ST_INIT));

		if (!ed->get_int(CMD_KEY, &val)) return false;
		ent->SetCommand((int)val);

		if (!ed->get_int(ULIST_KEY, &val)) return false;
		ent->SetUseUlist((bool)val);

		if (!ed->get_int(FILE_KEY, &val)) return false;
		ent->SetUseFile((bool)val);

		if (!ed->get_int(CLIP_KEY, &val)) return false;
		ent->SetUseClip((bool)val);

		auto host = cfg->priorityHosts.GetHostByName(&hs);
		if (!host) {
			Debug("priorityHost not found(%s/%s)\n", hs.u.userName, hs.u.hostName);
			return false;
		}
		ent->SetHost(host);
		sendEntry.push_back(ent);

		if (ent->Status() != ST_DONE) {
			RegisterHist(ent.get(), true, true);
		}
	}
	return	true;
}

void SendMsg::RegisterHist(SendEntry *ent, bool use_msg, bool delayed)
{
	U8str	msg;

	if (use_msg) {
		MakeHistMsg(ent, &msg);
	}
	HistNotify	hn = {
		&ent->Host()->hostSub,
		packetNo,
		use_msg ? msg.s() : NULL,
		delayed
	};
	::SendMessage(GetMainWnd(), WM_HISTDLG_NOTIFY,  (WPARAM)&hn, 0);
}

