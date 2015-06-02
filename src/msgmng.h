/*	@(#)Copyright (C) H.Shirouzu 2013-2015   msgmng.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Mssenger base defines
	Create					: 2013-03-03(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MSGMNG_H
#define MSGMNG_H

struct MsgBuf {
	HostSub	hostSub;
	int		version;
	int		portNo;
	ULONG	packetNo;
	ULONG	command;
	char	msgBuf[MAX_UDPBUF];
	char	exBuf[MAX_UDPBUF];
	char	packetNoStr[256/8]; // for IV (256bit)

	MsgBuf() {
		*msgBuf = 0;
		*exBuf = 0;
		*packetNoStr = 0;
	}

	// メモリコピー節約のため。
	void	Init(MsgBuf *org) {
		if (org == NULL) {
			memset(this, 0, (char *)&this->msgBuf - (char *)this);
			*msgBuf = 0;
			*exBuf = 0;
			*packetNoStr = 0;
			return;
		}
		memcpy(this, org, (char *)&this->msgBuf - (char *)this);
		strcpy(this->msgBuf, org->msgBuf);
		strcpy(this->exBuf, org->exBuf);
		strcpy(this->packetNoStr, org->packetNoStr);
	}
};

struct RecvBuf {
	Addr	addr;
	int		port;
	int		size;
	char	msgBuf[MAX_UDPBUF];
};

class ConnectInfo : public TListObj {
public:
	ConnectInfo() { Init(); }
	void Init() { sd=0; port=0; server=complete=FALSE; startTick=lastTick=0; }
	SOCKET	sd;
	Addr	addr;
	int		port;
	BOOL	server;
	BOOL	complete;
	DWORD	startTick;
	DWORD	lastTick;
};

struct AddrInfo {
	enum Type { NONE=0, UNICAST=0, BROADCAST=1, MULTICAST=2, ANYCAST=3 };
	Type	type;
	Addr	addr;
	DWORD	mask;
	AddrInfo() { type = UNICAST; mask = NONE; }
};

class MsgMng {
protected:
	SOCKET		udp_sd;
	SOCKET		tcp_sd;

	BOOL		status;
	ULONG		packetNo;
	Cfg			*cfg;
	THosts		*hosts;
	BOOL		isV6;

	HWND		hAsyncWnd;
	UINT		uAsyncMsg;
	UINT		lAsyncMode;
	HostSub		local;
	HostSub		localA;
	HostSub		orgLocal;

	BOOL		WSockInit();
	void		WSockTerm(void);
	BOOL		WSockReset(void);

public:
	MsgMng(Addr nicAddr, int portNo, Cfg *cfg=NULL, THosts *hosts=NULL);
	~MsgMng();

	static BOOL	WSockStartup();
	static void	WSockCleanup();

	BOOL	MulticastJoin();
	BOOL	MulticastLeave();

	BOOL	GetStatus(void)	{ return	status; }
	HostSub	*GetLocalHost(void) { return	&local; }
	HostSub	*GetLocalHostA(void) { return	&localA; }
	HostSub	*GetOrgLocalHost(void) { return	&orgLocal; }
	void	CloseSocket(void);
	BOOL	IsAvailableTCP() { return tcp_sd != INVALID_SOCKET ? TRUE : FALSE; }

	BOOL	Send(HostSub *hostSub, ULONG command, int val);
	BOOL	Send(HostSub *hostSub, ULONG command, const char *message=NULL, const char *exMsg=NULL);
	BOOL	Send(Addr host_addr, int port_no, ULONG command, const char *message=NULL,
			const char *exMsg=NULL);
	BOOL	AsyncSelectRegister(HWND hWnd);
	BOOL	Recv(MsgBuf *msg);
	BOOL	ResolveMsg(char *buf, int size, MsgBuf *msg);
	BOOL	ResolveMsg(RecvBuf *buf, MsgBuf *msg);
	BOOL	DecryptMsg(MsgBuf *msg, UINT *cryptCapa, UINT *logOpt);
	ULONG	MakePacketNo(void);
	ULONG	MakeMsg(char *udp_msg, int packetNo, ULONG command, const char *msg,
			const char *exMsg=NULL, int *packet_len=NULL);
	ULONG	MakeMsg(char *udp_msg, ULONG command, const char *msg, const char *exMsg=NULL,
					int *packet_len=NULL) {
				return	MakeMsg(udp_msg, MakePacketNo(), command, msg, exMsg, packet_len);
			}
	BOOL	MakeEncryptMsg(Host *host, ULONG packet_no, char *msg_str, bool is_extenc,
							char *share_str, char *buf);
	BOOL	UdpSend(Addr host_addr, int port_no, const char *udp_msg);
	BOOL	UdpSend(Addr host_addr, int port_no, const char *udp_msg, int len);
	BOOL	UdpRecv(RecvBuf *buf);

	BOOL	Accept(HWND hWnd, ConnectInfo *info);
	BOOL	Connect(HWND hWnd, ConnectInfo *info);
	BOOL	AsyncSelectConnect(HWND hWnd, ConnectInfo *info);
	BOOL	ConnectDone(HWND hWnd, ConnectInfo *info);
};

enum GetIPAddrsMode { GIA_NORMAL, GIA_STRICT, GIA_NOBROADCAST };
AddrInfo *GetIPAddrs(GetIPAddrsMode mode, int *num, int ipv6_mode=0);

#endif

