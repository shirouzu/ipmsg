static char *sendmng_id = 
	"@(#)Copyright (C) H.Shirouzu 2018   sendmng.cpp	Ver4.90";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Manager
	Create					: 2018-09-01(Sat)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */
#include "resource.h"
#include "ipmsg.h"

using namespace std;

/*
	SendManager の初期化
*/
SendMng::SendMng(Cfg *_cfg, MsgMng *_msgmng, ShareMng *_shareMng)
{
	cfg			= _cfg;
	msgMng		= _msgmng;
	shareMng	= _shareMng;
}

/*
	SendMng の終了
*/
SendMng::~SendMng()
{
}

void SendMng::LoadSendPkt()
{
	IPDict			dict;
	vector<ULONG>	del_item;

	for (int i=0; cfg->LoadSendPkt(i, &dict); i++) {
		auto	msg = make_shared<SendMsg>(cfg, msgMng);
		if (msg->Deserial(dict)) {
			msgDis.push_back(msg);
		}
		else if (msg->PacketNo()) {
			del_item.push_back(msg->PacketNo());
		}
	}
	for (auto &packetNo: del_item) {
		cfg->DelSendPkt(packetNo);
	}
}

void SendMng::AddHostEvent(Host *host)
{
	for (auto i=msgDis.begin(); i != msgDis.end(); ) {
		auto&	ent = *i;

		if (ent->NeedActive(host)) {
			ent->Reset();
			msgAct.push_back(ent);
			i = msgDis.erase(i);
		} else {
			i++;
		}
	}
}

void SendMng::AddMsg(std::shared_ptr<SendMsg> msg)
{
	IPDict	d;
	msg->Serial(&d);
	msg->SaveHost();
	cfg->SaveSendPkt(msg->PacketNo(), d);

	msgAct.push_back(msg);
}

void SendMng::SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa)
{
	for (auto &msg: msgAct) {
		msg->SendPubKeyNotify(hostSub, pubkey, len, e, capa);
	}
}

void SendMng::SendFinishNotify(HostSub *hostSub, ULONG packet_no)
{
	for (auto i=msgAct.begin(); i != msgAct.end(); i++) {
		auto&	ent = *i;

		if (ent->SendFinishNotify(hostSub, packet_no) && ent->IsSendFinish()) {
			cfg->DelSendPkt(ent->PacketNo());
			msgAct.erase(i);
			Debug("notify done\n");
			return;
		}
	}
}

bool SendMng::Del(HostSub *hostSub, ULONG packet_no)
{
	SendMsgList	*msgVecList[] = { &msgAct, &msgDis };

	for (auto msgVec: msgVecList) {
		for (auto i=msgVec->begin(); i != msgVec->end(); i++) {
			auto&	ent = *i;
			if (ent->Del(hostSub, packet_no)) {
				shareMng->EndHostShare(packet_no, hostSub);
				if (ent->IsSendFinish()) {
					cfg->DelSendPkt(ent->PacketNo());
					msgVec->erase(i);
				}
				else {
					IPDict	d;
					ent->Serial(&d);
					cfg->SaveSendPkt(ent->PacketNo(), d);
				}
				Debug("del done\n");
				return	true;
			}
		}
	}
	return	false;
}

void SendMng::TimerProc()
{
	for (auto i=msgAct.begin(); i != msgAct.end(); ) {
		auto&	ent = *i;
		if (ent->TimerProc()) {
			if (ent->IsSendFinish() || cfg->DelaySend == 0 || !ent->IsSendMsg()) {
				cfg->DelSendPkt(ent->PacketNo());
			}
			else {
				IPDict	d;
				ent->Serial(&d);
				cfg->SaveSendPkt(ent->PacketNo(), d);
				msgDis.push_back(ent);
			}
			i = msgAct.erase(i);
			Debug("timer done\n");
		} else {
			i++;
		}
	}
}

