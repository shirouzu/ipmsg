static char *mainwindir_id = 
	"@(#)Copyright (C) H.Shirouzu 2016-2017   mainwindir.cpp	Ver4.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window Directory
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "instdata/instcmn.h"
#include <process.h>

using namespace std;

#ifdef IPMSG_PRO
#define MAIN_FUNCS
#include "mainext.dat"
#undef  MAIN_FUNCS
#else

//////////////// Dir for Common /////////////////////

void TMainWin::SendHostListDict(HostSub *hostSub)
{
	int	start_no = 0;
	int	pkt_num = 0;

	while (start_no < allHosts.HostCnt()) {
		size_t		total_len = 100;
		IPDictList	dlist;

		for (int idx=start_no; idx < allHosts.HostCnt(); idx++) {
			Host	*host = allHosts.GetHost(idx);

			auto hdict = make_shared<IPDict>();
			total_len += MakeHostDict(host, hdict);
			dlist.push_back(hdict);

			if (total_len + 1024 >= MAX_UDPBUF) {
				break;
			}
		}

		if (dlist.size() > 0) {
			IPDict	dict;
			msgMng->InitIPDict(&dict, IPMSG_ANSLIST_DICT);
			dict.put_dict_list(IPMSG_HOSTLIST_KEY, dlist);

			dict.put_int(IPMSG_START_KEY, start_no);
			dict.put_int(IPMSG_NUM_KEY,   dlist.size());
			dict.put_int(IPMSG_TOTAL_KEY, allHosts.HostCnt());
			msgMng->SignIPDict(&dict);

			msgMng->Send(hostSub, &dict);
			pkt_num++;

			start_no += (int)dlist.size();
		}
		else {
			break;
		}
	}

	Debug("SendHostListDict done(%d/%d) %d pkts %s\n",
		start_no, allHosts.HostCnt(), pkt_num, hostSub->addr.S());

	brDirAgentTick = 0;
}

BOOL TMainWin::MasterPubKey(Host *host)
{
	int	cap = IPMSG_ENCRYPTOPT|IPMSG_CAPUTF8OPT|IPMSG_DIR_MASTER;

	if ((host->hostStatus & cap) == cap) {	// サーバであれば pubkeyを要求
		if (!host->pubKey.Key() && GetUserNameDigestField(host->hostSub.u.userName)) {
			msgMng->GetPubKey(&host->hostSub);
		}
	}
	return	TRUE;
}

/*
BOOL TMainWin::MasterDetect(const char *host)
{
	Addr	addr(host);
	if (!addr.IsEnabled() && !addr.Resolve(host) {
		return	FALSE;
	}

	msgMng->
}*/

BOOL TMainWin::CheckDirPacket(MsgBuf *msg, BOOL addr_check)
{
	if ((cfg->DirMode == DIRMODE_MASTER && !IsSameHost(&msg->hostSub, msgMng->GetLocalHost())) ||
		(cfg->DirMode != DIRMODE_MASTER && !cfg->masterAddr.IsEnabled() && cfg->masterStrict)) {
		return	FALSE;
	}

	if (addr_check && cfg->DirMode == DIRMODE_USER && cfg->masterAddr != msg->hostSub.addr) {
		return	FALSE;
	}

	if (msg->signMode != MsgBuf::SIGN_OK) {
		Debug("signMode is not OK in CheckDirPacket\n");
		return	FALSE;
	}

	return	TRUE;
}

BOOL TMainWin::AddHostListDict(IPDict *dict)
{
	int64	start = 0;
	int64	num = 0;
	int64	total = 0;

	dict->get_int(IPMSG_START_KEY, &start);
	dict->get_int(IPMSG_NUM_KEY, &num);
	dict->get_int(IPMSG_TOTAL_KEY, &total);

	IPDictList	dlist;
	if (!dict->get_dict_list(IPMSG_HOSTLIST_KEY, &dlist)) {
		return	FALSE;
	}
	if (dlist.size() != num || num == 0) {
		return	FALSE;
	}
	for (auto &d: dlist) {
		Host	host;
		if (!DictToHost(d, &host)) {
			return	FALSE;
		}

		if (host.active) {
			if (GetUserNameDigestField(host.hostSub.u.userName) &&
				(host.hostStatus & IPMSG_ENCRYPTOPT) == 0) {
				continue; // 成り済まし？
			}
			AddHost(&host.hostSub, IPMSG_BR_ENTRY|host.hostStatus, host.nickName,
				host.groupName, host.verInfoRaw, TRUE);
			Debug("addhost by anslistdict(%s) %x\n", host.hostSub.addr.S(), host.hostStatus);
		} else {
			Debug("delhost by anslistdict(%s) %x\n", host.hostSub.addr.S(), host.hostStatus);
			DelHost(&host.hostSub);
			if (IsSameHost(&host.hostSub, msgMng->GetLocalHost())) {
				if (lastExitTick && (GetTick() - lastExitTick) > 5000) {
					BroadcastEntry(IPMSG_BR_ENTRY);
					Debug("Try comeback self\n");
				}
			}
		}
	}
	return	TRUE;
}


//////////////// Dir for Agent /////////////////////

BOOL TMainWin::SetBrDirAddr(MsgBuf *msg)
{
	U8str	targ;
	if (!msg->ipdict.get_str(IPMSG_TARGADDR_KEY, &targ)) {
		return	FALSE;
	}

	Addr	in_addr(targ.s());
	if (!in_addr.IsIPv4()) {
		return	FALSE;
	}

	for (auto obj = brListOrg.TopObj() ; obj; obj=brListOrg.NextObj(obj)) {
		if (in_addr.IsInNet(obj->addr)) {
			Debug("AddrToBrAddr %s is in %s/%d\n", in_addr.S(), obj->addr.S(), obj->addr.mask);
			brDirAddr = obj->addr;
			return	TRUE;
		}
		else Debug("AddrToBrAddr %s isn't in %s/%d\n", in_addr.S(), obj->addr.S(), obj->addr.mask);
	}

	return	TRUE;
}

BOOL TMainWin::GetWrappedMsg(MsgBuf *msg, MsgBuf *wmsg, BOOL force_master)
{
	if (msg->wrdict.key_num() == 0) {
		Debug("wrdict is empty in GetWrappedMsg\n");
		return	FALSE;
	}

	wmsg->hostSub = msg->hostSub;

//	if (force_master) {
//		Host *host = hosts.GetHostByAddr(cfg->masterAddr, portNo);
//		if (!host) {
//			Debug("masterHost is null(%s) in GetWrappedMsg\n", cfg->masterAddr.S());
//			return	FALSE;
//		}
//		wmsg->hostSub = host->hostSub;
//	}

	if (!msgMng->GetWrappedMsg(msg, wmsg)) {
		Debug("msg->GetWrappedMsg err in GetWrappedMsg\n");
		return	FALSE;
	}

	if (!CheckDirPacket(wmsg)) {
		return	FALSE;
	}

	return	TRUE;
}

BOOL TMainWin::SendAgentReject(HostSub *hostSub)
{
	IPDict	dict;

	msgMng->InitIPDict(&dict, IPMSG_DIR_AGENTREJECT);
	msgMng->SignIPDict(&dict);
	msgMng->Send(hostSub, &dict);

	return	TRUE;
}

BOOL TMainWin::MsgDirBroadcast(MsgBuf *msg)
{
	if (cfg->DirMode != DIRMODE_USER) {
		SendAgentReject(&msg->hostSub);
		return FALSE;
	}

	if (IsLastPacket(msg)) {
		return	FALSE;
	}

	if (!CheckDirPacket(msg, TRUE)) {
		return	FALSE;
	}

	Debug("MsgDirBroadcast\n");

	if (DirPollCore(msg)) {
		DirBroadcastCore(msg);
	}
	return	TRUE;
}

BOOL TMainWin::MsgDirPollAgent(MsgBuf *msg)
{
	if (cfg->DirMode != DIRMODE_USER) {
		SendAgentReject(&msg->hostSub);
		return FALSE;
	}

	if (IsLastPacket(msg)) {
		return	FALSE;
	}

	if (!CheckDirPacket(msg, TRUE)) {
		return	FALSE;
	}

	Debug("MsgDirBroadcast\n");

	return	DirPollCore(msg);
}

BOOL TMainWin::MsgDirAgentReject(MsgBuf *msg)
{
	if (cfg->DirMode != DIRMODE_MASTER) {
		return FALSE;
	}

	if (msg->signMode != MsgBuf::SIGN_OK) {
		return FALSE;
	}
	Debug("MsgDirAgentReject %s\n", msg->hostSub.addr.S());

	auto host = allHosts.GetHostByName(&msg->hostSub);

	if (!host) {
		return	FALSE;
	}

	DelSegHost(host);
	return	TRUE;
}

BOOL TMainWin::DirPollCore(MsgBuf *msg)
{
	if (!SetBrDirAddr(msg)) {
		return	FALSE;
	}

	if (msg->hostSub.addr.IsInNet(brDirAddr)) {
		Debug("Server addr is same network(s:%s b:%s)\n",
			msg->hostSub.addr.S(), brDirAddr.S());
		return FALSE;
	}

	int64	val = 0;
	if (msg->ipdict.get_int(IPMSG_AGENTSEC_KEY, &val)) {
		brDirAgentLimit = (UINT)val * 1000; // sec to tick
		brDirAgentTick = ::GetTick();
		brDirAgentLast = brDirAgentTick;
		Debug("Set agent timer %zd sec\n", val);
	}

	return	TRUE;
}

void TMainWin::DirBroadcastCore(MsgBuf *msg)
{
	BroadcastEntrySub(brDirAddr, portNo, IPMSG_NOOPERATION);
	this->Sleep(cfg->DelayTime);

	IPDict	ipdict;
	ipdict.put_int(IPMSG_DIRBROAD_KEY, 1);
	BroadcastEntrySub(brDirAddr, portNo, IPMSG_BR_ENTRY, &ipdict);

	Debug("DirBroadcastCore\n");

	DWORD	rand_val;
	TGenRandom(&rand_val, sizeof(rand_val));
	SetTimer(IPMSG_BRDIR_TIMER, 4000 + (2047 & rand_val));
}

void TMainWin::ReplyDirBroadcast()
{
	KillTimer(IPMSG_BRDIR_TIMER);

	DynBuf	buf(MAX_UDPBUF);
	int		start_no = 0;
	int		offset = 0;
	time_t	now_time = time(NULL);

	while (start_no + offset < hosts.HostCnt()) {
		size_t		total_len = 100;
		IPDictList	dlist;

		for (int idx=start_no + offset; idx < hosts.HostCnt(); idx++) {
			Host	*host = hosts.GetHost(idx);

			if (!host->hostSub.addr.IsInNet(brDirAddr)
				|| (host->updateTimeDirect + cfg->UpdateTime) < now_time) {
				offset++;
				Debug("in net=%d addr=%s %s/%d, %lld+%d %lld\n", host->hostSub.addr.IsInNet(brDirAddr), host->hostSub.addr.S(), brDirAddr.S(), brDirAddr.mask, host->updateTimeDirect, cfg->UpdateTime, now_time);
				continue;
			}
			auto hdict = make_shared<IPDict>();
			total_len += MakeHostDict(host, hdict);
			dlist.push_back(hdict);

			if (total_len + 1024 >= MAX_UDPBUF) {
				break;
			}
		}

		IPDict	dict;
		msgMng->InitIPDict(&dict, IPMSG_DIR_ANSBROAD);
		dict.put_dict_list(IPMSG_HOSTLIST_KEY, dlist);

		msgMng->SignIPDict(&dict);
		msgMng->Send(cfg->masterAddr, portNo, &dict);

		start_no += (int)dlist.size();
	}

	Debug("ReplyDirBroadcast done(%d/%d)\n", start_no, offset);

	brDirAgentTick = 0;
}

// local で発生した BR_ENTRY/ABS/EXIT を Master に伝達
BOOL TMainWin::AgentDirHost(Host *host, BOOL is_direct)
{
	if (cfg->DirMode != DIRMODE_USER) return FALSE;

	if (!cfg->masterAddr.IsEnabled() || host->hostSub.addr == cfg->masterAddr) {
		return FALSE;
	}
	if (IsSameHost(&host->hostSub, msgMng->GetLocalHost())) {
		return FALSE;
	}
	if (::GetTick() - brDirAgentLast > brDirAgentLimit) {
		return FALSE;
	}
	if (!host->hostSub.addr.IsInNet(brDirAddr)) {
		return FALSE;
	}
	if (cfg->masterAddr.IsInNet(brDirAddr)) {
		Debug("Server addr is same network2(s:%s b:%s)\n",
			host->hostSub.addr.S(), brDirAddr.S());
		return FALSE;
	}

	IPDictList	dlist;

	auto hdict = make_shared<IPDict>();
	MakeHostDict(host, hdict);
	dlist.push_back(hdict);

	IPDict	dict;
	msgMng->InitIPDict(&dict, IPMSG_DIR_EVBROAD);
	dict.put_dict_list(IPMSG_HOSTLIST_KEY, dlist);
	if (is_direct) {
		dict.put_int(IPMSG_DIRECT_KEY, 1);	// br_entryのリダイレクト
	}

	msgMng->SignIPDict(&dict);
	msgMng->Send(cfg->masterAddr, portNo, &dict);
	return	TRUE;
}

BOOL TMainWin::PollSend()
{
	KillTimer(IPMSG_POLL_TIMER);

	if (cfg->DirMode != DIRMODE_USER) return FALSE;

	if (!cfg->masterAddr.IsEnabled() || cfg->pollTick < 5000) {
		return	FALSE;
	}

	HostSub	hostSub;
	hostSub.addr   = cfg->masterAddr;
	hostSub.portNo = portNo;

	IPDict	dict;
	msgMng->InitIPDict(&dict, IPMSG_DIR_POLL);

	if (cfg->Wine) {
		dict.put_int(IPMSG_AGENT_KEY, 0);
	}

	IPDictList	bl;
	for (auto obj=brListOrg.TopObj(); obj; obj=brListOrg.NextObj(obj)) {
		auto	dict = make_shared<IPDict>();
		Addr	net_addr = obj->addr.ToNet();
		dict->put_str(IPMSG_ADDR_KEY, net_addr.S());
		dict->put_int(IPMSG_MASK_KEY, net_addr.mask);
		bl.push_back(dict);
	}
	dict.put_dict_list(IPMSG_NADDRS_KEY, bl);

	msgMng->SignIPDict(&dict);
	msgMng->Send(&hostSub, &dict);

	SetTimer(IPMSG_POLL_TIMER, cfg->pollTick);

	Debug("Poll send %s/%d\n", hostSub.addr.S(), portNo);

	return	TRUE;
}

BOOL TMainWin::MsgDirRequest(MsgBuf *msg)
{
	if (cfg->DirMode != DIRMODE_USER) return FALSE;

	if (IsLastPacket(msg)) {
		return	FALSE;
	}

	Debug("MsgDirRequest start\n");

	DynMsgBuf	wmsg;

	if (!SetBrDirAddr(msg)) {
		Debug("MsgDirRequest SetBrDirAddr err\n");
		return	FALSE;
	}

	if (!GetWrappedMsg(msg, wmsg.msg)) {
		Debug("MsgDirRequest GetWrappedMsg err\n");
		return	FALSE;
	}

	int64	cmd = 0;
	if (!wmsg.msg->ipdict.get_int(IPMSG_CMD_KEY, &cmd)) {
		Debug("cmd not found in dir request=%x\n", cmd);
		return	FALSE;
	}

	switch (GET_MODE(cmd)) {
	case IPMSG_DIR_PACKET:
		break;

	default:
		Debug("unknwon dir request=%x\n", cmd);
		return	FALSE;
	}

	IPDict	dict;
	msgMng->InitIPDict(&dict, IPMSG_DIR_AGENTPACKET);
	dict.put_dict(IPMSG_WRAPPED_KEY, wmsg.msg->ipdict);

	Debug("fire IPMSG_DIR_AGENTPACKET (%s)\n", brDirAddr.S());

	return	msgMng->Send(brDirAddr, portNo, &dict);
}

BOOL TMainWin::MsgDirAgentPacket(MsgBuf *msg)
{
	if (cfg->DirMode == DIRMODE_MASTER) return FALSE;

	if (!cfg->masterAddr.IsEnabled() && cfg->masterStrict) {
		return	FALSE;
	}

	if (IsLastPacket(msg)) {
		return	FALSE;
	}

	Debug("MsgDirAgentPacket start\n");

	DynMsgBuf	wmsg;
	if (!GetWrappedMsg(msg, wmsg.msg, TRUE)) {
		return	FALSE;
	}

	Debug("MsgDirAgentPacket (%s)\n", msg->hostSub.addr.S());
	return	MsgDirPacket(wmsg.msg, TRUE);
}

BOOL TMainWin::MsgDirPacket(MsgBuf *msg, BOOL is_agent)
{
	// サーバも自分自身からの署名パケットは受信
	if ((cfg->DirMode == DIRMODE_MASTER && !IsSameHost(&msg->hostSub, msgMng->GetLocalHost())) ||
		(cfg->DirMode != DIRMODE_MASTER && !cfg->masterAddr.IsEnabled() && cfg->masterStrict)) {
		return	FALSE;
	}

	if (IsLastPacket(msg)) {
		return	FALSE;
	}

	TrcW(L"MsgDirPacket start\n");

	DynMsgBuf	wmsg;
	if (!GetWrappedMsg(msg, wmsg.msg)) {
		return	FALSE;
	}

	switch (GET_MODE(wmsg.msg->command)) {
	case IPMSG_ANSLIST_DICT:
	//	Debug("MsgDirPacket to AddHostListDict\n");
		AddHostListDict(&wmsg.msg->ipdict);
		break;

	default:
		Debug("Illegal MsgDirPacket(%x)\n", wmsg.msg->command);
		return	FALSE;
	}

	return	TRUE;
}

//////////////// DirAgent 関連 /////////////////////

void TMainWin::DirTimerMain()
{
	if (cfg->DirMode != DIRMODE_MASTER || dirPhase == DIRPH_NONE) {
		KillTimer(IPMSG_DIR_TIMER);
		dirPhase = DIRPH_NONE;
		ClearDirAll();
		return;
	}

	DWORD	loop_tick = cfg->dirSpan * 1000;
	DWORD	cur = ::GetTick();

	DirClean();
	DirRefreshAgent();
	DirSendAgentBroad();

	if (cur - dirTick >= loop_tick) {
		dirTick = cur;
		dirEvFlags = 0;
	}

	if ((dirEvFlags & DIRF_NOOP) == 0) {
		dirEvFlags |= DIRF_NOOP;
		DirSendNoOpe();
		return;
	}

	if ((dirEvFlags & DIRF_BR) == 0) {
		dirEvFlags |= DIRF_BR;
		DirSendBroadcast();
		return;
	}
}

void TMainWin::ClearDirAll()
{
	for (auto &itr : segMap) {
		auto	&seg_h = itr.second;
		while (seg_h->HostCnt() > 0) {
			seg_h->DelHost(seg_h->GetHost(0));
		}
	}
	segMap.clear();

	for (int num=allHosts.HostCnt(); num > 0; ) {
		Host	*host = allHosts.GetHost(--num);
		allHosts.DelHost(host);
		host->SafeRelease();
	}
}

void TMainWin::DirSendNoOpe()
{
	for (auto brobj=brListEx.TopObj(); brobj; brobj=brListEx.NextObj(brobj)) {
		BroadcastEntrySub(brobj->Addr(), portNo, IPMSG_NOOPERATION);	// 旧パケット送信
	}
}

void TMainWin::DirSendBroadcast()
{
//	IPDict	dict;

	for (auto brobj=brListEx.TopObj(); brobj; brobj=brListEx.NextObj(brobj)) {
		BroadcastEntrySub(brobj->Addr(), portNo, IPMSG_BR_ENTRY);	// 旧パケット送信
	}
}

void DirAgentValAdd(vector<DirAgent> *agents, vector<DirAgent>::iterator &itr, int val)
{
	auto	host = itr->host;

	host->agentVal += val;
	host->agentVal = min(host->agentVal,  1);
	host->agentVal = max(host->agentVal,  -1);

	if (host->agentVal < 0) {
		host->lastPoll = 0;
		agents->erase(itr);
	}
}

// Aggent通知 & Broadcast Agent通知
// target がある場合 target にのみ Broadcast通知
void TMainWin::DirSendAgentBroad(BOOL force_agent)
{
	TrcW(L"DirSendAgentBroad\n");

	int64	agent_sec = cfg->dirSpan + 10;
	IPDict	br_dict;
	IPDict	agent_dict;

	// 同一セグメントのエージェントに対し、先頭エージェントにブロードキャスト依頼
	// 後続エージェントには、変更差分発生時の通知のみ依頼
	DWORD	loop_tick = cfg->dirSpan * 1000;
	DWORD	cur = ::GetTick();

	for (auto &s: segMap) {
		auto	&seg_h = s.second;

		if (seg_h->isSameSeg) {
			continue;
		}

		DWORD	br_diff = seg_h->brResTick ? seg_h->brResTick - seg_h->brSetTick : 0;
		BOOL	need_br = (cur - seg_h->brSetTick > 8000
						&& cur - seg_h->brResTick > loop_tick - min(br_diff, 6000));
		BOOL	br_err = (need_br && seg_h->brSetTick && seg_h->brSetTick > seg_h->brResTick);

		//Debug("br_diff=%d res=%d set=%d need_br=%d\n", br_diff, seg_h->brResTick, seg_h->brSetTick, need_br);

		// BR要求に応えられなかった場合、カウンタを落とす
		if (br_err) {
			if (seg_h->agents.size() > 0 && seg_h->brSetAddr.IsEnabled()
			 && seg_h->brSetAddr == seg_h->agents[0].host->hostSub.addr) {
				Debug("AgentBroad failed %s\n", seg_h->agents[0].host->hostSub.addr.S());
				DirAgentValAdd(&seg_h->agents, seg_h->agents.begin(), -1);
			}
			seg_h->brSetAddr.Init();
		}

		auto	&agents = seg_h->agents;
		for (auto &ag: agents) {
			if (ag.isReq || (!need_br && (cur - ag.resTick < loop_tick && !force_agent))) {
				continue;
			}
			Host	*host = ag.host;
			IPDict	&dict = need_br ? br_dict : agent_dict;

			if (dict.key_num() == 0) {
				msgMng->InitIPDict(&dict, need_br ? IPMSG_DIR_BROADCAST : IPMSG_DIR_POLLAGENT);
				dict.put_int(IPMSG_AGENTSEC_KEY, agent_sec);
			}
			Debug("SendAgentBroad as %s (%s)\n",
				need_br ? "broad" : "agent", host->hostSub.addr.S());
			dict.put_str(IPMSG_TARGADDR_KEY, host->hostSub.addr.S());
			msgMng->SignIPDict(&dict);

			ag.isReq = TRUE;
			SendDirTcp(host, &dict);
			if (need_br) {
				seg_h->brSetTick = cur;
				seg_h->brSetAddr = host->hostSub.addr;
				need_br = FALSE;
			}
		}
	}
}

void TMainWin::DirClean()
{
	return;

	time_t	now = time(NULL);
	auto	&lru = allHosts.lruList;
	list<Host *>	hlist;

	for (auto i=lru.rbegin(); i != lru.rend(); i++) {
		Host	*host = *i;
		time_t	diff = now - host->updateTime;

		if (diff < cfg->dirSpan + DIR_BASE_SEC * 5) {
			break;
		}
		if (diff < cfg->dirSpan + DIR_BASE_SEC * 8) {
			BOOL	is_confirm = FALSE;

			for (auto obj=brListOrg.TopObj(); obj; obj=brListOrg.NextObj(obj)) {
				if (host->hostSub.addr.IsInNet(obj->addr)) {
					is_confirm = TRUE;
					break;
				}
			}
			if (!is_confirm) {
				for (auto &s: segMap) {
					auto	&net_addr = s.first;
					auto	&seg_h    = s.second;

					if (seg_h->isSameSeg || !host->hostSub.addr.IsInNet(net_addr)) {
						continue;
					}
					// 対象セグメントに Agent が居ない場合は、確認パケットを出さない
					if (seg_h->agents.size() == 0) {
						continue;
					}
					is_confirm = TRUE;
					break;
				}
			}
			if (is_confirm) {
				BroadcastEntrySub(&host->hostSub, IPMSG_BR_ENTRY); // 確認
				Debug("confirm delhost %s\n", host->hostSub.addr.S());
			}
		}
		else {
			hlist.push_back(host);
			host->active = FALSE; // DirSendAnsHosts で、削除モードにする
		}
	}
	if (hlist.size() > 0) {
		DirSendAnsHosts(hlist);
		for (auto &h: hlist) {
			DirDelHost(h, FALSE);
		}
	}
}

//////////////// DirAgent 関連 Sub /////////////////////

const DirAgent *GetDirAgent(const vector<DirAgent> &agents, Host *host)
{
	auto	itr = find_if(agents.begin(), agents.end(), [&](auto &ag) {
		return	ag.host == host;
	});
	if (itr == agents.end()) {
		return	NULL;
	}
	return	&(*itr);
}

BOOL TMainWin::DirRefreshAgent()
{
	TrcW(L"DirRefreshAgent segs=%zd ...\n", segMap.size());
	DWORD	cur = ::GetTick();

	for (auto &s: segMap) {
		auto	&seg_h  = s.second;
		auto	&agents = seg_h->agents;
		BOOL	seg_update = FALSE;

		if (seg_h->isSameSeg) {
			continue;
		}
		// Agentが不足している場合、最も agentValが高い中で、LRU最新のものをセット
		for (int i=(int)agents.size(); i < cfg->DirAgentNum; i++) {
			auto	&sl = seg_h->lruList;

			Host	*host = NULL;
			for (auto &h: sl) {
				if (cur - h->lastPoll < cfg->pollTick + 5000
				 && (!host || h->agentVal > host->agentVal)
				 && !GetDirAgent(agents, h)) {
					host = h;
				}
			}
			if (host) {
				agents.push_back(DirAgent(host));
				seg_update = TRUE;
				Debug("Set new agent(%d) %s\n", i, host->hostSub.addr.S());
			}
		}
		// 更新があった場合、Agentの中で最も AgentValが高いものを先頭（Broadcast用）に入れ替え
		if (seg_update && agents.size() >= 2) {
			auto	&ag1 = seg_h->agents[0];
			for (int i=1; i < agents.size(); i++) {
				auto	&ag2 = seg_h->agents[i];
				if (ag1.host->agentVal < ag2.host->agentVal) {
					swap(ag1, ag2);
					Debug("swap 0 <-> %d\n", i);
				}
			}
		}
	}

	TrcW(L"DirRefreshAgent done\n");
	return	TRUE;
}

void TMainWin::DirSendAgent(IPDict *in_dict)
{
	TrcW(L"DirSendAgent\n");

	DirRefreshAgent();

	IPDict	dict;
	msgMng->InitIPDict(&dict, IPMSG_DIR_REQUEST);

	for (auto &s: segMap) {
		auto	&seg_h  = s.second;
		auto	&agents = seg_h->agents;

		if (seg_h->isSameSeg) {
			continue;
		}
		for (auto &ai: agents) {
			Host	*host = ai.host;
			if (!host) {
				continue;
			}
			if (in_dict) {
				in_dict->put_str(IPMSG_TARGADDR_KEY, host->hostSub.addr.S());
				msgMng->SignIPDict(in_dict);
				dict.put_dict(IPMSG_WRAPPED_KEY, *in_dict);
			}
			dict.put_str(IPMSG_TARGADDR_KEY, host->hostSub.addr.S());

			TrcW(L"DirSendAgent sign\n");
			msgMng->SignIPDict(&dict);

			TrcU8("DirSendAgent send to %s\n", host->hostSub.addr.S());
			msgMng->Send(host->hostSub.addr, portNo, &dict);
		}
	}
}

void TMainWin::DirSendDirect(IPDict *dict)
{
	for (auto brobj=brListEx.TopObj(); brobj; brobj=brListEx.NextObj(brobj)) {
		msgMng->Send(brobj->Addr(), portNo, dict);
	}
}

void TMainWin::DirSendAll(IPDict *in_dict)
{
	TrcW(L"DirSendAll\n");

	IPDict	dict;
	msgMng->InitIPDict(&dict, IPMSG_DIR_PACKET);

	if (in_dict) {
		dict.put_dict(IPMSG_WRAPPED_KEY, *in_dict);
	}

	DirSendAgent(&dict);

	msgMng->SignIPDict(&dict);
	DirSendDirect(&dict);
}


void TMainWin::DirSendAnsList(const list<Host *> &host_list)
{
	// すぐには不要
}

void TMainWin::DirSendAnsHosts(const list<Host *> &hlist)
{
	TrcW(L"DirSendAnsHosts\n");

	IPDict	dict;
	msgMng->InitIPDict(&dict, IPMSG_ANSLIST_DICT);

	IPDictList	dlist;
	size_t		total_size = 0;
	size_t		offset = 0;

	for (auto itr=hlist.begin(); itr != hlist.end(); ) {
		auto	hdict = make_shared<IPDict>();
		total_size += MakeHostDict(*itr, hdict);
		dlist.push_back(hdict);

		itr++;
		if (itr == hlist.end() || total_size + 1024 >= MAX_UDPBUF) {
			dict.put_dict_list(IPMSG_HOSTLIST_KEY, dlist);
			dict.put_int(IPMSG_START_KEY, offset);
			dict.put_int(IPMSG_NUM_KEY,   dlist.size());
			dict.put_int(IPMSG_TOTAL_KEY, hlist.size());
			offset += dlist.size();
			msgMng->SignIPDict(&dict);

			DirSendAll(&dict);
		}
	}
}

void TMainWin::DirSendAnsOne(Host *host)
{
	list<Host *>	hlist;

	hlist.push_back(host);
	DirSendAnsHosts(hlist);
}


//////////////// MsgDirPoll関連 /////////////////////

//  Poll msg から target自身のnet_addrを検出
BOOL DetectPollNet(MsgBuf *msg, Addr *addr)
{
	IPDictList	bl;

	if (!msg->ipdict.get_dict_list(IPMSG_NADDRS_KEY, &bl)) {
		return FALSE;
	}

	for (auto &d: bl) {
		U8str	s;
		int64	mask;
		if (!d->get_str(IPMSG_ADDR_KEY, &s)) continue;
		if (!d->get_int(IPMSG_MASK_KEY, &mask) || mask < 0 || mask > 32) continue;

		addr->Set(s.s(), (int)mask);
		if (addr->IsEnabled() && msg->hostSub.addr.IsInNet(*addr)) {
			return	TRUE;
		}
		addr->Init();
	}
	return	FALSE;
}

BOOL TMainWin::UpdateSegs(Host *host)
{
	if (host->hostSub.addr.mask == 0 || host->hostSub.addr.mask == 32) {
		MessageBox(Fmt("Illegal netmask(%s/%d) in UpdateSegs\n",
			host->hostSub.addr.S(), host->hostSub.addr.mask));
		return	FALSE;
	}
	host->lastPoll = ::GetTick();

	Addr	seg_addr = host->hostSub.addr.ToNet();
	auto	itr = segMap.find(seg_addr);

	if (host->parent_seg.IsEnabled() && itr != segMap.end()) {
		if (host->parent_seg == seg_addr) {
			itr->second->UpdateLru(host);
			return	TRUE;
		}
		DelSegHost(host);
	}

	if (itr == segMap.end()) {
		auto seg_h = make_shared<THosts>();
		seg_h->Enable(THosts::NAME, TRUE);
		seg_h->Enable(THosts::ADDR, TRUE);
		seg_h->SetLruIdx(SEG_LRU);

		segMap[seg_addr] = seg_h;
		seg_h->AddHost(host);
		host->parent_seg = seg_addr;
		seg_h->agents.reserve(cfg->DirAgentNum);

		for (auto brobj=brListOrg.TopObj(); brobj; brobj=brListOrg.NextObj(brobj)) {
			if (seg_addr == brobj->addr.ToNet()) {
				seg_h->isSameSeg = TRUE;	// 自セグメントはマークを付けておく
				break;
			}
		}
		if (!seg_h->isSameSeg) {
			DirSendAgentBroad();
		}
		Debug("UpdateSegs: addseg %s/%d same=%d\n", seg_addr.S(), seg_addr.mask, seg_h->isSameSeg);
		return	TRUE;
	}

	auto &seg_h = itr->second;
	Host *ah = seg_h->GetHostByAddr(&host->hostSub);
	if (ah) {
		Debug("UpdateSegs : del %s\n", ah->hostSub.addr.S());
		allHosts.DelHost(ah);
		DelSegHost(ah, FALSE);
		ah->SafeRelease();
	}

	seg_h->AddHost(host);
	host->parent_seg = seg_addr;

	return	TRUE;
}

void TMainWin::DelSegHost(Host *host, BOOL allow_segdel)
{
	Addr	&seg_addr = host->parent_seg;

	if (!seg_addr.IsEnabled()) {
		Debug(" DelSegHost err, not seg_addr enabled(%s)\n", host->hostSub.addr.S());
		return;
	}
	if (host->hostSub.addr.mask == 0 || host->hostSub.addr.mask == 32) {
		Debug("Illegal netmask(%s/%d) in DelSegHost\n",
			host->hostSub.addr.S(), host->hostSub.addr.mask);
		return;
	}

	auto itr = segMap.find(seg_addr);
	if (itr == segMap.end()) {
		Debug(" DelSegHost err, seg not found(%s/%s)\n",
			seg_addr.S(), host->hostSub.addr.S());
		return;
	}

	auto	&seg_h  = itr->second;
	auto	&agents = seg_h->agents;
	auto	ai = find_if(agents.begin(), agents.end(), [&](auto &ag) {
		return	ag.host == host;
	});
	if (ai != agents.end()) {
		bool is_begin = ai == agents.begin();
		Debug("agent(%zd) is deleted %s\n", ai - agents.begin(), host->hostSub.addr.S());
		agents.erase(ai);
		if (is_begin && agents.size() > 0) {
			Debug("New agent(0) is %s\n", agents[0].host->hostSub.addr.S());
		}
	}

	seg_h->DelHost(host);

	if (allow_segdel && seg_h->HostCnt() == 0) {
		Debug(" DelSegHost seg del(%s)\n", seg_addr.S());
		segMap.erase(seg_addr);
	}
	seg_addr.Init();
}

BOOL TMainWin::DirUpdateHost(const Host &s, Host *d)
{
	strncpyz(d->nickName, s.nickName, sizeof(d->nickName));
	strncpyz(d->groupName, s.groupName, sizeof(d->groupName));
	strncpyz(d->alterName, s.alterName, sizeof(d->alterName));
	strncpyz(d->verInfoRaw, s.verInfoRaw, sizeof(d->verInfoRaw));
	d->hostStatus = s.hostStatus;
//	d->updateTime = s.updateTime;
//	d->updateTimeDirect = s.updateTimeDirect;
//	d->priority = s.priority;
//	if (!d->pubKey.Capa()) {
//		d->pubKey = s.pubKey;
//	}
//	d->active = s.active;
//	d->agent = s.agent;

	return	TRUE;
}

// allHost追加、pollならsegにも追加、変更があればnotify発行
Host *TMainWin::DirAddHost(Host *host, BOOL is_poll, BOOL is_notify, BOOL *_need_notify)
{
	BOOL	tmp = FALSE;
	BOOL	&need_notify = _need_notify ? *_need_notify : tmp;

	if (CheckDosHost(&host->hostSub, false)) {
		return NULL;
	}

	need_notify = FALSE;

	if (Host *nh = allHosts.GetHostByName(&host->hostSub)) {
	//	Debug("DirAddHost %s %p %p\n", nh->S(), nh, host);

		if (IsSameAddrPort(&nh->hostSub, &host->hostSub)) {
			allHosts.UpdateLru(nh);
			nh->updateTime = nh->updateTimeDirect = time(NULL);
			if (!NeedUpdateHost(host, nh)) {
				if (is_poll) {
					if (!nh->parent_seg.IsEnabled()) {
						nh->hostSub.addr.mask = host->hostSub.addr.mask;
					}
					UpdateSegs(nh);
				}
//				Debug("DirAddHost no update %s/%d poll=%d\n", nh->hostSub.addr.S(), nh->hostSub.addr.mask, is_poll);
				return	nh;
			}
			DirUpdateHost(*host, nh);
			if (is_notify) {
				DirSendAnsOne(nh);
			} else {
				need_notify = TRUE;
			}

			Debug("DirAddHost need update %s poll=%d\n", nh->S(), is_poll);
			return nh;
		}
		else {	// アドレスが従来と異なる
			if (nh->parent_seg.IsEnabled()) {
				DelSegHost(nh);
			}
			Debug("DirAddHost del %s poll=%d %p %p\n", nh->S(), is_poll, nh, host);
			allHosts.DelHost(nh);
			nh->SafeRelease();

			// 同一アドレスの別ホストが存在？
			if (Host *ah = allHosts.GetHostByAddr(&host->hostSub)) {
				if (ah->parent_seg.IsEnabled()) {
					DelSegHost(ah);
				}
				Debug("DirAddHost del2 %s poll=%d\n", ah->S(), is_poll);

				allHosts.DelHost(ah);
				ah->SafeRelease();
			}
		}
	}
	else {
		Debug("DirAddHost nohost %s poll=%d\n", host->S(), is_poll);
	}
	Host *dh = new Host;
	*dh = *host;
	dh->updateTime = dh->updateTimeDirect = time(NULL);

	allHosts.AddHost(dh);

	if (is_poll) {
		UpdateSegs(dh);
	}
	if (is_notify) {
		DirSendAnsOne(dh);
	} else {
		need_notify = TRUE;
	}

	Debug("DirAddHost %s poll=%d %p %p\n", dh->S(), is_poll, dh, host);

	return	dh;
}

BOOL TMainWin::DirDelHost(Host *_host, BOOL is_notify)
{
	Host	*host = allHosts.GetHostByName(&_host->hostSub);
	if (!host) {
		return FALSE;
	}

	if (CheckDosHost(&host->hostSub, true)) {
		return FALSE;
	}

	host->active = FALSE;

// 通知
	if (is_notify) {
		DirSendAnsOne(host);
	}

// segMap & allhost から削除
	if (host->parent_seg.IsEnabled()) {	// seg登録されている
		DelSegHost(host);
	}

	Debug("DelHostDir host(%s) %p\n", host->S(), host);

	allHosts.DelHost(host);
	host->SafeRelease();

	return	TRUE;
}


void TMainWin::MsgDirPoll(MsgBuf *msg)
{
	if (cfg->DirMode != DIRMODE_MASTER) return;

	if (msg->signMode != MsgBuf::SIGN_OK) {
		return;
	}

	BOOL	update = FALSE;
	Host	host;

	Addr	seg_addr;
	BOOL	is_poll = DetectPollNet(msg, &seg_addr); // 自ホスト情報がない場合agent扱いにしない

	if (is_poll) {
		msg->hostSub.addr.mask = seg_addr.mask;
	}

	MsgToHost(msg, &host);
	host.agent = TRUE;

	DirAddHost(&host, is_poll);
}

void TMainWin::DirSendHostList(MsgBuf *msg, Host *host)
{
	if (IsSameHost(&host->hostSub, msgMng->GetLocalHost())) return;

	DWORD	cur = ::GetTick();

	if ((cur - host->lastList) < 7000) {
		return;
	}

	host->lastList = cur;

	if (host->hostStatus & IPMSG_CAPIPDICTOPT) {
		SendHostListDict(&host->hostSub);
	}
	else {
		msg->hostSub = host->hostSub;
		if ((host->hostStatus & (IPMSG_CAPUTF8OPT|IPMSG_UTF8OPT)) == 0) {
			msg->command &= ~(IPMSG_CAPUTF8OPT|IPMSG_UTF8OPT);
		}
		*msg->msgBuf = 0;
		SendHostList(msg);
	}
}


int TMainWin::GetAgent(MsgBuf *msg, shared_ptr<THosts> *seg_h)
{
	Host *host = allHosts.GetHostByAddr(&msg->hostSub);

	if (!host || !IsSameHost(&host->hostSub, &msg->hostSub) || !host->parent_seg.IsEnabled()) {
		return	-1;
	}

	auto itr = segMap.find(host->parent_seg);
	if (itr == segMap.end()) {
		return	-1;
	}

	auto	&agents = itr->second->agents;
	auto	ai = find_if(agents.begin(), agents.end(), [&](auto &ag) {
		return	ag.host == host;
	});
	if (ai == agents.end()) {
		return	-1;
	}
	if (seg_h) {
		*seg_h = itr->second;
	}
	return	(int)(ai - agents.begin());
}

void TMainWin::MsgDirAnsBroad(MsgBuf *msg, BOOL is_ansbroad)
{
	if (cfg->DirMode != DIRMODE_MASTER) return;

	if (IsLastPacket(msg)) {
		return;
	}
	if (msg->signMode != MsgBuf::SIGN_OK) {
		return;
	}

	IPDictList	dlist;

	if (!msg->ipdict.get_dict_list(IPMSG_HOSTLIST_KEY, &dlist)) {
		return;
	}
	int64	is_direct = 0;
	msg->ipdict.get_int(IPMSG_DIRECT_KEY, &is_direct);

	shared_ptr <THosts> seg_h;
	if (GetAgent(msg, &seg_h) == -1) {
		return;
	}
	if (is_ansbroad) {
		seg_h->brResTick = ::GetTick();
		seg_h->brSetAddr.Init();
	}

	list<Host *>	ans_list;

	for (auto &d: dlist) {
		Host	host;

		if (!DictToHost(d, &host)) {
			return;
		}
		if (host.active) {
			if (GetUserNameDigestField(host.hostSub.u.userName) &&
				(host.hostStatus & IPMSG_ENCRYPTOPT) == 0) {
				continue; // 成り済まし？
			}
			BOOL	need_notify = FALSE;
			if (Host *dh = DirAddHost(&host, FALSE, FALSE, &need_notify)) {
				if (need_notify) {
					ans_list.push_back(dh);
				}
				if (is_direct) {
					DirSendHostList(msg, dh);
				}
			}
		} else {
			DirDelHost(&host);
		}
	}

	if (ans_list.size() > 0) {
		DirSendAnsHosts(ans_list);
	}

//	hostlist の update
//	IPMSG_DIRECT_KEY がある場合は、HostList を AddHost追加先に送信
}

BOOL TMainWin::SendDirTcp(Host *host, IPDict *dict)
{
	auto pkt = make_shared<PktData>();

	pkt->info.growSbuf   = FALSE;
	pkt->info.uMsg       = WM_TCPDIREVENT;
	pkt->info.addEvFlags = FD_READ|FD_WRITE|FD_CLOSE|FD_OOB;
	pkt->info.addr       = host->hostSub.addr;
	pkt->info.port       = host->hostSub.portNo;
	pkt->hostSub         = host->hostSub;
	dict->get_int(IPMSG_CMD_KEY, &pkt->cmd);

	pkt->buf.Alloc(dict->pack_size());
	pkt->buf.SetUsedSize(dict->pack(pkt->buf.Buf(), pkt->buf.Size()));

	if (!msgMng->Connect(hWnd, &pkt->info)) {
		return FALSE;
	}

	tcpMap[pkt->info.sd] = pkt;

	return	TRUE;
}

BOOL TMainWin::PollReqAccept(HostSub *hostSub, BOOL result)
{
	auto si = segMap.find(hostSub->addr.ToNet());
	if (si == segMap.end()) {
		Debug("PollReqAccept unkown %s res=%d\n", hostSub->addr.S(), result);
		return	FALSE;
	}

	auto	&seg_h = si->second;
	auto	&agents = seg_h->agents;
	Host	*host = NULL;

	for (auto ai=agents.begin(); ai != agents.end(); ai++) {
		host = ai->host;
		if (IsSameHost(&host->hostSub, hostSub)) {
			if (!result) {
				Debug("PollReqAccept(%zd) %s NG\n", ai-agents.begin(), hostSub->addr.S());
			}
			ai->isReq = FALSE;
			if (result) {
				ai->resTick = ::GetTick();
				host->lastPoll = ai->resTick;
				DirAgentValAdd(&agents, ai, 1);
			}
			else {
				DirAgentValAdd(&agents, ai, -1);
				if (seg_h->brSetAddr == hostSub->addr) {
					seg_h->brSetAddr.Init();
				}
			}
			break;
		}
		host = NULL;
	}
	if (!host) {
		Debug("PollReqAccept not match %s res=%d\n", hostSub->addr.S(), result);
	}

	return	TRUE;
}

BOOL TMainWin::TcpDirEvent(SOCKET sd, LPARAM lParam)
{
	if (WSAGETSELECTERROR(lParam))
		return	FALSE;

	auto	itr = tcpMap.find(sd);
	if (itr == tcpMap.end()) {
		Debug("unknown sd=%d\n", sd);
		::closesocket(sd);
		return	TRUE;
	}
	shared_ptr<PktData>	pkt = itr->second;

	pkt->info.lastTick = ::GetTick();

	switch (LOWORD(lParam)) {
	case FD_CONNECT:
		//Debug("fd_connect done %s\n", pkt->hostSub.addr.S());
		return	TRUE;

	case FD_WRITE:
		{
			int	size = ::send(sd, pkt->buf.s() + pkt->offset,
				int(pkt->buf.UsedSize() - pkt->offset), 0);
			if (size <= 0) {
				PollReqAccept(&pkt->hostSub, FALSE);
				goto CLOSE_END;
			}
			pkt->offset += size;
			if (pkt->offset == pkt->buf.UsedSize()) {
			//	::shutdown(sd, SD_SEND);
			}
		}
		return	TRUE;

	case FD_OOB:
	case FD_READ:
	case FD_CLOSE:
		{
			char	c;
			int		size = ::recv(sd, &c, 1, 0); // EOF
			PollReqAccept(&pkt->hostSub, size == 0);
		}
		goto CLOSE_END;
	}
	return	TRUE;

CLOSE_END:
	::closesocket(sd);
	tcpMap.erase(sd);
	return	FALSE;
}

BOOL TMainWin::CleanupDirTcp()
{
	DWORD			tick = 0;
	vector<SOCKET>	sdvec;

	for (auto itr=tcpMap.begin(); itr != tcpMap.end(); itr++) {
		if (!tick) {
			tick = ::GetTick();
		}
		auto	&pkt = itr->second;
		if (tick - pkt->info.lastTick >= 3000) {
			sdvec.push_back(itr->first);
			Debug("CleanupDirTcp(%s)\n", pkt->hostSub.addr.S());
		}
	}
	for (auto sd: sdvec) {
		TcpDirEvent(sd, FD_CLOSE);
	}

	return	TRUE;
}

DosHost *TMainWin::SearchDosHost(HostSub *hostSub, BOOL need_alloc)
{
	for (auto obj=dosHost->TopObj(USED_LIST); obj; obj = dosHost->NextObj(USED_LIST, obj)) {
		if (hostSub->addr == obj->addr && hostSub->portNo == obj->portNo) {
			dosHost->UpdObj(USED_LIST, obj);
			return	obj;
		}
	}
	if (!need_alloc) {
		return	NULL;
	}

	auto obj = dosHost->GetObj(FREE_LIST);
	if (!obj) {
		obj = dosHost->GetObj(USED_LIST);
	}
	obj->Set(hostSub, 0, true);
	dosHost->PutObj(USED_LIST, obj);

	return	obj;
}

BOOL TMainWin::CheckDosHost(HostSub *hostSub, bool is_exit)
{
	auto	dh = SearchDosHost(hostSub, is_exit);

	if (!dh) {
	//	Debug("CheckDosHost no ent(%s)\n", hostSub->addr.S());
		return	FALSE;
	}

	DWORD	tick = GetTick();
	if (!dh->tick || tick - dh->tick > 2000) {
		if (is_exit) {
			dh->tick = tick;
			dh->exit = true;
			dh->cnt  = 0;
		//	Debug("CheckDosHost set ent(%s)\n", hostSub->addr.S());
		}
		else {
			dosHost->DelObj(USED_LIST, dh);
			dosHost->PutObj(FREE_LIST, dh);
		//	Debug("CheckDosHost del ent(%s)\n", hostSub->addr.S());
		}
		return	FALSE;
	}

	dh->tick = tick;
	if (is_exit == dh->exit) {
		//Debug("CheckDosHost same ent(%s) %d\n", hostSub->addr.S(), dh->cnt);
		return	FALSE;
	}
	dh->exit = is_exit;
	dh->cnt++;

	Debug("CheckDosHost inc ent(%s) %d\n", hostSub->addr.S(), dh->cnt);

	return	dh->cnt >= 4 ? TRUE : FALSE;
}

#endif

