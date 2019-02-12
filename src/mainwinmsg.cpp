static char *mainwinmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2019 mainwinmsg.cpp	Ver4.90";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window Message
	Create					: 1996-06-01(Sat)
	Update					: 2019-01-12(Sat)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "install/instcmn.h"
#include <process.h>

using namespace std;

/*
	Entry Packet受信処理
*/
void TMainWin::MsgBrEntry(MsgBuf *msg)
{
	if (IsLastPacket(msg)) {
		return;
	}
	BOOL	is_send = TRUE;

	CheckMaster(msg);
	if (CheckSelfAlias(msg)) return;

	if (cfg->masterAddr.IsEnabled() && cfg->masterAddr == msg->hostSub.addr) {
		int64	val = -1;
		if (msg->ipdict.get_int(IPMSG_UPTIME_KEY, &val) && val == 0) {
			Debug(" boot poll send\n");
			PollSend();	// server boot 直後はすぐに pollを返す
		}
	}

	for (auto obj = ansList->TopObj(USED_LIST); obj; obj = ansList->NextObj(USED_LIST, obj)) {
		if (IsSameHost(&msg->hostSub, &obj->hostSub)) {
			is_send = FALSE;	// 既に返信キューに入っている
			break;
		}
	}

	int	command = IPMSG_ANSENTRY|HostStatus();

	if (msg->command & IPMSG_CAPUTF8OPT) {
		command |= IPMSG_UTF8OPT;
	}

	if (!VerifyUserNameExtension(cfg, msg)) {
		return;
	}

	if (is_send) {
		auto obj = ansList->GetObj(FREE_LIST);
		if (obj) {
			obj->hostSub = msg->hostSub;
			obj->command = command;
		}
		if (obj && SetAnswerQueue(&msg->hostSub)) {
			ansList->PutObj(USED_LIST, obj);
		}
		else {
			msgMng->Send(&msg->hostSub, command, GetNickNameEx(), cfg->GroupNameStr);
			ansList->PutObj(FREE_LIST, obj);
		}
	}

	if (Host *host = AddHost(&msg->hostSub, msg->command, msg->nick.s(), msg->group.s(),
		msg->verInfoRaw)) {
#ifndef IPMSG_PRO
		int64	flg = 0;
		if (!msg->ipdict.get_int(IPMSG_DIRBROAD_KEY, &flg) || !flg) {
			AgentDirHost(host, TRUE);
			if (cfg->DirMode == DIRMODE_MASTER) {
				if (Host *ah = allHosts.GetHostByAddr(&host->hostSub)) {
					DirSendHostList(msg, ah);
				}
			}
		}
#endif
	}
}

BOOL TMainWin::SetAnswerQueue(HostSub *hostSub)
{
	if (ansTimerID) {
		return	TRUE;
	}

	int		hostCnt = hosts.HostCnt();
	DWORD	spawn;
	DWORD	rand_val;

	TGenRandom(&rand_val, sizeof(rand_val));

	if (hostCnt < 50 ||
		((msgMng->GetLocalHost()->addr.V4Addr() ^ hostSub->addr.V4Addr()) << 8) == 0) {
		spawn = 1023 & rand_val;
	}
	else if (hostCnt < 300) {
		spawn = 2047 & rand_val;
	}
	else {
		spawn = 4095 & rand_val;
	}

	if ((ansTimerID = SetTimer(IPMSG_ANS_TIMER, spawn+5)) == 0) {
		return	FALSE;
	}

	return	TRUE;
}

void TMainWin::ExecuteAnsQueue(void)
{
	while (auto obj = ansList->GetObj(USED_LIST)) {
		msgMng->Send(&obj->hostSub, obj->command, GetNickNameEx(), cfg->GroupNameStr);
		ansList->PutObj(FREE_LIST, obj);
	}
}

/*
	Exit Packet受信処理
*/
void TMainWin::MsgBrExit(MsgBuf *msg)
{
	if (CheckSelfAlias(msg)) return;

	if (auto host = cfg->priorityHosts.GetHostByName(&msg->hostSub)) {
		host->updateTime = time(NULL);
	}

	if (auto host = hosts.GetHostByAddr(&msg->hostSub)) {
		host->active = FALSE;
#ifndef IPMSG_PRO
		AgentDirHost(host);
#endif
	}

	DelHost(&msg->hostSub);

/*
	for (auto info=shareMng->TopObj(),*next; info; info = next)
	{
		next = shareMng->NextObj(info);
		shareMng->EndHostShare(info->packetNo, &msg->hostSub);
	} */
}

/*
	AnsEntry Packet受信処理
*/
void TMainWin::MsgAnsEntry(MsgBuf *msg)
{
	if (!VerifyUserNameExtension(cfg, msg)) return;

	CheckMaster(msg);
	if (CheckSelfAlias(msg)) return;

	AddHost(&msg->hostSub, msg->command, msg->nick.s(), msg->group.s(), msg->verInfoRaw);
}

/*
	Absence Packet受信処理
*/
void TMainWin::MsgBrAbsence(MsgBuf *msg)
{
	if (!VerifyUserNameExtension(cfg, msg)) return;

	CheckMaster(msg);
	if (CheckSelfAlias(msg)) return;

	if (auto host = AddHost(&msg->hostSub, msg->command, msg->nick.s(), msg->group.s(),
		msg->verInfoRaw)) {
#ifndef IPMSG_PRO
		AgentDirHost(host);
#endif
	}
}

/*
	Send Packet受信処理
*/
void TMainWin::MsgSendMsg(MsgBuf *msg)
{
	TRecvDlg	*recvDlg;

	if (!VerifyUserNameExtension(cfg, msg) &&
		((msg->command & IPMSG_AUTORETOPT) == 0 ||
		 (msg->command & (IPMSG_PASSWORDOPT|IPMSG_SENDCHECKOPT|IPMSG_SECRETEXOPT
		 				 |IPMSG_FILEATTACHOPT)))) {
		return;
	}

	if (TRecvDlg::GetCreateCnt() >= cfg->RecvMax) {
		BalloonWindow(TRAY_NORMAL, Fmt(LoadStrU8(IDS_RECVMAX_TRAY), cfg->RecvMax),
			IP_MSG, cfg->RecvMsgTime, TRUE);
		return;
	}

	for (recvDlg = recvList.TopObj(); recvDlg; recvDlg = recvList.NextObj(recvDlg)) {
		if (recvDlg->IsSamePacket(msg)) break;
	}

	if (recvDlg || IsLastPacket(msg)) {
		if ((msg->command & IPMSG_SENDCHECKOPT) &&
			(msg->command & (IPMSG_BROADCASTOPT | IPMSG_AUTORETOPT)) == 0) {
			msgMng->Send(&msg->hostSub, IPMSG_RECVMSG, msg->packetNo);
		}
		return;
	}
	cfg->LastRecv = msg->timestamp;
	if (!RecvDlgOpen(msg)) return;

	if ((msg->command & IPMSG_BROADCASTOPT) == 0 && (msg->command & IPMSG_AUTORETOPT) == 0) {
		if ((msg->command & IPMSG_SENDCHECKOPT)) {
			msgMng->Send(&msg->hostSub, IPMSG_RECVMSG, msg->packetNo);
		}
		if (cfg->AbsenceCheck && *cfg->AbsenceStr[cfg->AbsenceChoice]) {
			msgMng->Send(&msg->hostSub, IPMSG_SENDMSG|IPMSG_AUTORETOPT,
							cfg->AbsenceStr[cfg->AbsenceChoice]);
		}
		if ((msg->command & IPMSG_NOADDLISTOPT) == 0 && !hosts.GetHostByAddr(&msg->hostSub)) {
			BroadcastEntrySub(&msg->hostSub, IPMSG_BR_ENTRY);
		}
	}
}

/*
	Recv Packet受信処理
*/
void TMainWin::MsgRecvMsg(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	Read Packet受信処理
*/

void TMainWin::MsgReadMsg(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	HostList 送出可能問合せ Packet受信処理
*/
void TMainWin::MsgBrIsGetList(MsgBuf *msg)
{
	if (cfg->AllowSendList
		&& (entryStartTime + (cfg->ListGetMSec / 1000) < time(NULL))
			&& (!cfg->ListGet || (IPMSG_RETRYOPT & msg->command))) {
		msgMng->Send(&msg->hostSub, IPMSG_OKGETLIST);
	}
}

/*
	HostList 送出可能通知 Packet受信処理
*/
void TMainWin::MsgOkGetList(MsgBuf *msg)
{
	if (entryStartTime != IPMSG_GETLIST_FINISH) {
		msgMng->Send(&msg->hostSub, IPMSG_GETLIST);
	}
}

/*
	HostList 送出要求 Packet受信処理
*/
void TMainWin::MsgGetList(MsgBuf *msg)
{
	if (cfg->AllowSendList) {
		SendHostList(msg);
	}
}

/*
	HostList 送出 Packet受信処理
*/
void TMainWin::MsgAnsList(MsgBuf *msg)
{
#ifndef IPMSG_PRO
	if (cfg->DirMode == DIRMODE_MASTER) return;
#endif

	if (entryStartTime != IPMSG_GETLIST_FINISH) {
		AddHostList(msg);
	}
	else {
		Debug("AddHostList reject %s\n", msg->hostSub.addr.S());
	}
}

#ifndef IPMSG_PRO
void TMainWin::MsgAnsListDict(MsgBuf *msg)
{
	if (!CheckDirPacket(msg)) return;

	if (IsLastPacket(msg)) {
		return;
	}

	AddHostListDict(&msg->ipdict);
}
#endif

/*
	Version Information 要求 Packet受信処理
*/
void TMainWin::MsgGetInfo(MsgBuf *msg)
{
	char	buf[MAX_LISTBUF];

	sprintf(buf, "%sVer%s", LoadStrU8(IDS_WIN_EDITION), GetVersionStr());
	msgMng->Send(&msg->hostSub, IPMSG_SENDINFO, buf);
}

/*
	Version Information 通知 Packet受信処理
*/
void TMainWin::MsgSendInfo(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	Public Key 要求 Packet受信処理
*/
void TMainWin::MsgGetPubKey(MsgBuf *msg)
{
	int		capa = strtoul(msg->msgBuf.s(), 0, 16);
	int		local_capa = cfg->GetCapa();
	char	buf[MAX_BUF];

//	if (!VerifyUserNameExtension(cfg, msg)) return;

	if (GetUserNameDigestField(msg->hostSub.u.userName)) {
		if ((capa & IPMSG_RSA_2048) == 0 || (capa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256)) == 0) {
			return;	// 成り済まし？
		}
	}

	if ((capa &= local_capa) == 0)
		return;

	PubKey&	pubKey = (capa & IPMSG_RSA_2048) ? cfg->pub[KEY_2048] : cfg->pub[KEY_1024];

	// 署名検証用公開鍵を要求（IPMSG_AUTORETOPT でピンポン抑止）
	if ((capa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256)) && (msg->command & IPMSG_AUTORETOPT) == 0) {
		Host	*host = hosts.GetHostByName(&msg->hostSub);
		if (!host) host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
		if (!host || !host->pubKey.Key()) {
			msgMng->GetPubKey(&msg->hostSub);
		}
	}

	// 自分自身の鍵を送信
	sprintf(buf, "%X:%X-", local_capa, pubKey.Exponent());
	bin2hexstr_revendian(pubKey.Key(), pubKey.KeyLen(), buf + strlen(buf));
	msgMng->Send(&msg->hostSub, IPMSG_ANSPUBKEY, buf);
}

/*
	Public Key 送信 Packet受信処理
*/
void TMainWin::MsgAnsPubKey(MsgBuf *msg)
{
	BYTE	key[MAX_BUF];
	int		key_len, e, capa;
	char	*capa_hex, *e_hex, *key_hex, *p;

	if (cfg->GetCapa() == 0 || !msg->msgBuf) return;

	if ((capa_hex = sep_tok(msg->msgBuf, ':', &p)) == NULL)
		return;
	if ((e_hex = sep_tok(NULL, '-', &p)) == NULL)
		return;
	if ((key_hex = sep_tok(NULL, ':', &p)) == NULL)
		return;

	capa = strtoul(capa_hex, 0, 16);
	e = strtoul(e_hex, 0, 16);
	key_len = (int)hexstr2bin_revendian(key_hex, key, sizeof(key));

	if (IsUserNameExt(cfg) && !VerifyUserNameDigest(msg->hostSub.u.userName, key)) {
		return; // Illegal public key
	}

	Host	*host = hosts.GetHostByName(&msg->hostSub);
	if (!host) {
		host = cfg->priorityHosts.GetHostByName(&msg->hostSub);

		if (!host) {
			host = new Host();
			MsgToHost(msg, host);
			cfg->priorityHosts.AddHost(host);
		}
	}
	if (host && (!host->pubKey.Key() || host->pubKey.Capa() != capa)) {
		host->pubKey.Set(key, key_len, e, capa);

	}

	sendMng->SendPubKeyNotify(&msg->hostSub, key, key_len, e, capa);
}

/*
	Information 通知処理
*/
void TMainWin::MsgInfoSub(MsgBuf *msg)
{
	int	cmd = GET_MODE(msg->command);
	int	packet_no = (cmd == IPMSG_RECVMSG || cmd == IPMSG_ANSREADMSG || cmd == IPMSG_READMSG)
					? atol(msg->msgBuf.s()) : 0;

	int64	val = 0;
	if (msg->ipdict.get_int(IPMSG_REPLYPKT_KEY, &val)) {
		packet_no = (int)val;
	}

	if (cmd == IPMSG_READMSG) {
		if (GET_OPT(msg->command) & IPMSG_READCHECKOPT) {
			msgMng->Send(&msg->hostSub, IPMSG_ANSREADMSG, msg->packetNo);
		}
	}
	else if (cmd == IPMSG_ANSREADMSG || cmd == IPMSG_RECVMSG) {
		sendMng->SendFinishNotify(&msg->hostSub, packet_no);
		for (auto dlg = recvList.TopObj(); dlg; dlg = recvList.NextObj(dlg)) {
			dlg->SendFinishNotify(&msg->hostSub, packet_no);
		}
		return;
	}
	if (IsLastPacket(msg))		//重複チェック
		return;

	char	title[MAX_LISTBUF];
	int		show_mode = cfg->OpenCheck == 3 && cmd == IPMSG_READMSG ? SW_MINIMIZE : SW_SHOW;
	Host	*host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
	DynBuf	buf(MAX_BUF);

	strncpyz(buf.S(), msg->msgBuf.s(), MAX_BUF);
	if (host && *host->alterName) {
		strcpy(title, host->alterName);
	} else {
		strcpy(title, host && *host->nickName ? host->nickName : msg->hostSub.u.userName);
	}

	switch (cmd) {
	case IPMSG_READMSG:
		histDlg->OpenNotify(&msg->hostSub, packet_no);
		{
			MsgIdent	mi;
			mi.Init(msg->hostSub.u.userName, msg->hostSub.u.hostName, packet_no);
			logmng->ReadCheckStatus(&mi, FALSE, FALSE);
		}
		SetCaption();
		if (cfg->BalloonNoInfo) strcpy(title, " ");
		if (cfg->OpenCheck == 1) {
			BalloonWindow(TRAY_OPENMSG, title, LoadStrU8(IDS_OPENFIN), cfg->OpenMsgTime);
			return;
		}
		else if (cfg->OpenCheck == 0) {
			return;
		}
		else {
			const char *c =  strchr(Ctime(), ' ');
			if (c) c++;
			snprintfz(buf.S(), (int)buf.Size(), "%s\r\n%s", LoadStrU8(IDS_OPENFIN), c ? c : "");
			if (char *p = strrchr(buf.S(), ' ')) *p = 0;
		}
		break;

	case IPMSG_SENDINFO:
		sendMng->SendFinishNotify(&msg->hostSub, packet_no);
		histDlg->OpenNotify(&msg->hostSub, msg->packetNo, msg->msgBuf.s());
		return;

	case IPMSG_SENDABSENCEINFO:
		sendMng->SendFinishNotify(&msg->hostSub, packet_no);
		show_mode = SW_SHOW;
		break;

	default:
		return;
	}

	if (cmd == IPMSG_SENDABSENCEINFO) {	//将来的には TMsgDlgで処理
		static int msg_cnt = 0;	// TMsgDlg 化した後は TMsgDlg::createCnt に移行
		if (msg_cnt >= cfg->RecvMax)
			return;
		msg_cnt++;
		MessageBoxU8(buf.s(), title);
		msg_cnt--;
	}
	else {
		if (TMsgDlg::GetCreateCnt() >= cfg->RecvMax * 4)
			return;
		TMsgDlg	*msgDlg = new TMsgDlg(cfg->TaskbarUI ? 0 : this);
		msgDlg->Create(buf.s(), title, show_mode);
		if (cmd == IPMSG_SENDINFO || cmd == IPMSG_SENDABSENCEINFO)
			msgDlg->ActiveDlg();
		msgList.AddObj(msgDlg);
	}
}

/*
	不在通知 Information 要求 Packet受信処理
*/
void TMainWin::MsgGetAbsenceInfo(MsgBuf *msg)
{
	msgMng->Send(&msg->hostSub, IPMSG_SENDABSENCEINFO,
		cfg->AbsenceCheck ? cfg->AbsenceStr[cfg->AbsenceChoice] : LoadStrU8(IDS_NOTABSENCE));
}

/*
	不在通知 Information 通知 Packet受信処理
*/
void TMainWin::MsgSendAbsenceInfo(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	添付ファイル破棄通知 Packet受信処理
*/
void TMainWin::MsgReleaseFiles(MsgBuf *msg)
{
	int64	val = 0;
	int		packet_no = 0;

	if (msg->ipdict.get_int(IPMSG_FID_KEY, &val)) {
		packet_no = (int)val;
	}
	else {
		packet_no = atoi(msg->msgBuf.s());
	}

	shareMng->EndHostShare(packet_no, &msg->hostSub);
}

/*
	確認Dialogを破棄
*/
void TMainWin::MsgDlgExit(DWORD msgid)
{
	TMsgDlg	*msgDlg = msgList.Search(msgid);

	if (msgDlg && msgDlg->modalCount == 0) {
		msgList.DelObj(msgDlg);
		delete msgDlg;
	}
}

