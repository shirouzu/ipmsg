/*	@(#)Copyright (C) H.Shirouzu 2018   sendmng.h	Ver4.90 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Manager
	Create					: 2018-09-01(Sat)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SENDMNG_H
#define SENDMNG_H

typedef std::list<std::shared_ptr<SendMsg>> SendMsgList;

class SendMng {
	Cfg				*cfg = NULL;
	MsgMng			*msgMng = NULL;
	ShareMng		*shareMng = NULL;
	SendMsgList		msgAct;
	SendMsgList		msgDis;

public:
	SendMng(Cfg *, MsgMng *, ShareMng *);
	~SendMng();

	void AddHostEvent(Host *host);
	void AddMsg(std::shared_ptr<SendMsg> msg);
	void SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa);
	void SendFinishNotify(HostSub *hostSub, ULONG packet_no);
	bool Del(HostSub *hostSub, ULONG packet_no);
	void LoadSendPkt();

	void TimerProc();
};


#endif

