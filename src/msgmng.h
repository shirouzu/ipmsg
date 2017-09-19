/*	@(#)Copyright (C) H.Shirouzu 2013-2017   msgmng.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Mssenger base defines
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MSGMNG_H
#define MSGMNG_H

#include <vector>

class FileInfo : public TListObj {
	U8str	fname;
	U8str	fnameOrg;
	DynBuf	memData;

	int		id;
	UINT	attr;
	int		pos;
	int64	size;
	time_t	mtime;
	time_t	atime;
	time_t	crtime;
	BOOL	isSelected;		// for recvdlg

public:
	FileInfo(int _id=0) {
		id = _id;
		attr = 0;
		pos = -1;
		size = 0;
		mtime = 0;
		atime = 0;
		crtime = 0;
		isSelected = FALSE;
	}
	FileInfo(const FileInfo& org) {
		*this = org;	// operaotr=()
	}
	FileInfo& operator =(const FileInfo& org) {
		fname = org.fname;
		fnameOrg = org.fnameOrg;
		memData = org.memData;

		memcpy(&id, &org.id, ((BYTE *)&isSelected - (BYTE *)&id) + sizeof(isSelected));

		return *this;
	}

	int Id() { return id; }
	void SetId(int _id) { id = _id; }
	const char *Fname() { return fname.s(); }
	const char *FnameOrg() { return fnameOrg.s(); }
	void SetFname(const char *_fname, BOOL replace_org=FALSE) {
		fname = _fname;
		if (replace_org || fnameOrg.Len() == 0) {
			SetFnameOrg(_fname);
		}
	}
	void SetFnameOrg(const char *_fnameOrg) {
		fnameOrg = _fnameOrg;
	}
	const BYTE *MemData() { return memData; }
	void SetMemData(const BYTE *_memData, int64 _size) {
		memData.Alloc((int)_size);
		if (memData.Buf()) {
			memcpy(memData.Buf(), _memData, (int)_size);
			size = _size;
		}
	}
	int64 Size() { return size; }
	void SetSize(int64 _size) { size = _size; }
	time_t Mtime() { return mtime; }
	void SetMtime(time_t _mtime) { mtime = _mtime; }
	time_t Atime() { return atime; }
	void SetAtime(time_t _atime) { atime = _atime; }
	time_t Crtime() { return crtime; }
	void SetCrtime(time_t _crtime) { crtime = _crtime; }
	UINT Attr() { return attr; }
	void SetAttr(UINT _attr) { attr = _attr; }
	int  Pos() { return pos; }
	void SetPos(UINT _pos) { pos = _pos; }
	BOOL IsSelected() { return isSelected; }
	void SetSelected(BOOL _isSelected) { isSelected = _isSelected; }
};

struct MsgBuf {
/* SavePacket でそのまま格納 */
	HostSub	hostSub;
	VerInfo	verInfo;	// client version
	int		version;	// protocol version
	int		portNo;
	int		pollReq;
	BOOL	viaAgent;
	ULONG	packetNo;
	ULONG	command;
	time_t	timestamp;
	int64	msgId;		// generate logdb (available after logging)

	ULONG	flags;
	enum { SIGN_INIT, SIGN_NOKEY, SIGN_OK, SIGN_NG } signMode;
	enum { DEC_INIT, DEC_OK, DEC_NG } decMode;

/* ここまで */
	char	msgBuf[MAX_UDPBUF];
	char	exBuf[MAX_UDPBUF];
	char	uList[MAX_ULISTBUF];
	char	packetNoStr[AES_BLOCK_SIZE];// for IV (256bit)
	char	verInfoRaw[MAX_VERBUF];

	IPDict	ipdict;
	IPDict	wrdict;
	IPDict	orgdict;
	U8str	body;
	U8str	group;	// only entry/notify
	U8str	nick;	// only entry/notify

	MsgBuf() {
		Init();
	}
	MsgBuf(const MsgBuf &org) {
		*this = org;
	}

	// メモリコピー節約のため。
	void	Init(const MsgBuf *org=NULL) {
		if (org == NULL) {
			version   = 0;
			portNo    = 0;
			pollReq   = 0;
			viaAgent  = 0;
			packetNo  = 0;
			command   = 0;
			flags     = 0;
			timestamp = 0;
			msgId     = 0;
			*msgBuf   = 0;
			*exBuf    = 0;
			*uList    = 0;
			signMode  = SIGN_INIT;
			decMode   = DEC_INIT;
			ipdict.clear();
			wrdict.clear();
			*packetNoStr = 0;
			*verInfoRaw = 0;
			nick.Init();
			group.Init();
			return;
		}
		memcpy(this, org, (char *)&this->msgBuf - (char *)this);
		strcpy(this->msgBuf, org->msgBuf);
		strcpy(this->exBuf, org->exBuf);
		strcpy(this->uList, org->uList);
		strcpy(this->packetNoStr, org->packetNoStr);
		ipdict = org->ipdict;
		wrdict = org->wrdict;
		orgdict = org->orgdict;
		group = org->group;
		nick = org->nick;
	}
	MsgBuf& operator =(const MsgBuf& d) {
		Init(&d);
		return	*this;
	}
};

struct DynMsgBuf {
	MsgBuf	*msg;
	DynMsgBuf() {
		msg = new MsgBuf;
	}
	~DynMsgBuf() {
		delete msg;
	}
};


struct RecvBuf {
	Addr	addr;
	int		port;
	int		size;
	char	*msgBuf;

	RecvBuf() {
		msgBuf = new char [MAX_UDPBUF];
		port = 0;
		size = 0;
	}
	~RecvBuf() {
		delete [] msgBuf;
	}
};

struct ConnectInfo : public TListObj {
	ConnectInfo() {
		Init();
	}
	void Init() {
		sd = 0;
		port = 0;
		server = TRUE;
		complete = FALSE;
		fin = FALSE;
		startTick = 0;
		lastTick = 0;
		uMsg = WM_TCPEVENT;
		growSbuf = TRUE;
		addEvFlags = 0;
	}
	SOCKET	sd;
	Addr	addr;
	int		port;
	BOOL	server;
	BOOL	complete;
	BOOL	fin;
	DWORD	startTick;
	DWORD	lastTick;
	UINT	uMsg;
	DynBuf	buf;
	BOOL	growSbuf;
	DWORD	addEvFlags;
	ConnectInfo& operator =(const ConnectInfo &);
};

struct AddrInfo {
	enum Type { NONE=0, UNICAST=0, BROADCAST=1, MULTICAST=2, ANYCAST=3 };
	Type			type;
	Addr			addr;
	std::list<Addr>	gw;	// default gateway
	AddrInfo() { type = UNICAST; }
};

class MsgMng {
protected:
	SOCKET		udp_sd;
	SOCKET		tcp_sd;

	BOOL		status;
	static ULONG packetNo;
	Cfg			*cfg;
	THosts		*hosts;
	BOOL		isV6;

	HWND		hAsyncWnd;
	UINT		uAsyncMsg;
	UINT		lAsyncMode;
	static HostSub local;
	static HostSub localA;
	HostSub		orgLocal;
	DynBuf		lastPacket;

	BOOL	WSockInit();
	void	WSockTerm(void);
	BOOL	WSockReset(void);
	BOOL	RecvCore(RecvBuf *buf, MsgBuf *msg);

public:
	MsgMng(Addr nicAddr, int portNo, Cfg *cfg=NULL, THosts *hosts=NULL);
	~MsgMng();

	static BOOL	WSockStartup();
	static void	WSockCleanup();

	BOOL	MulticastJoin();
	BOOL	MulticastLeave();

	BOOL	GetStatus(void)	{ return	status; }
	static HostSub *GetLocalHost(void) { return &local; }
	static HostSub *GetLocalHostA(void) { return &localA; }
	HostSub	*GetOrgLocalHost(void) { return &orgLocal; }
	void	CloseSocket(void);
	BOOL	IsAvailableTCP() { return tcp_sd != INVALID_SOCKET ? TRUE : FALSE; }

	BOOL	Send(HostSub *hostSub, ULONG command, int val);
	BOOL	Send(HostSub *hostSub, ULONG command, const char *message=NULL,
				const char *exMsg=NULL, const IPDict *ipdict=NULL);
	BOOL	Send(Addr host_addr, int port_no, ULONG command, const char *message=NULL,
				const char *exMsg=NULL, const IPDict *ipdict=NULL);
	BOOL	Send(HostSub *hostSub, IPDict *ipdict);
	BOOL	Send(Addr host_addr, int port_no, IPDict *ipdict);
	BOOL	AsyncSelectRegister(HWND hWnd);

	BOOL	Recv(MsgBuf *msg);
	BOOL	PseudoRecv(RecvBuf *buf, MsgBuf *msg);
	BOOL	ResolveDictMsg(MsgBuf *msg);
	BOOL	GetWrappedMsg(MsgBuf *msg, MsgBuf *wmsg);
	BOOL	CheckVerify(MsgBuf *msg, const IPDict *ipdict);

	BOOL	ResolveMsg(char *buf, int size, MsgBuf *msg);
	BOOL	ResolveMsg(RecvBuf *buf, MsgBuf *msg);
	BOOL	GetPubKey(HostSub *hostSub, BOOL isAuto=TRUE);
	BOOL	DecryptMsg(MsgBuf *msg, UINT *cryptCapa, UINT *logOpt);

	ULONG	InitIPDict(IPDict *ipdict, ULONG command=0, ULONG flags=0, ULONG pktno=0);
	BOOL	SignIPDict(IPDict *ipdict);

	BOOL	EncIPDict(IPDict *ipdict, PubKey *pub, DynBuf *dbuf);
	BOOL	EncIPDict(IPDict *ipdict, PubKey *pub, IPDict *out_dict);
	size_t	DecIPDict(BYTE *data, size_t size, IPDict *dict);
	BOOL	DecIPDict(IPDict *wdict, IPDict *dict);

	ULONG	MakeMsg(char *udp_msg, ULONG packetNo, ULONG command, const char *msg,
			const char *exMsg=NULL, const IPDict *ipdict=NULL, int *packet_len=NULL);
	ULONG	MakeMsg(char *udp_msg, ULONG command, const char *msg, const char *exMsg=NULL,
					const IPDict *ipdict=NULL, int *packet_len=NULL) {
				return	MakeMsg(udp_msg, MakePacketNo(), command, msg, exMsg, ipdict, packet_len);
			}
	BOOL	MakeMsg(IPDict *ipdict, DynBuf *buf);
	BOOL	MakeEncryptMsg(Host *host, ULONG packet_no, char *msg_str, bool is_extenc,
							char *share_str, char *ulist_str, char *buf);
	BOOL	UdpSend(Addr host_addr, int port_no, const char *udp_msg);
	BOOL	UdpSend(Addr host_addr, int port_no, const char *udp_msg, int len);
	BOOL	UdpSendCore(Addr host_addr, int port_no, const char *udp_msg, int len);
	BOOL	UdpRecv(RecvBuf *buf);

	BOOL	Accept(HWND hWnd, ConnectInfo *info);
	BOOL	Connect(HWND hWnd, ConnectInfo *info);
	BOOL	AsyncSelectConnect(HWND hWnd, ConnectInfo *info);
	BOOL	ConnectDone(HWND hWnd, ConnectInfo *info);

	void	UListToHostSub(MsgBuf *msg, std::vector<HostSub> *hosts);
	void	UListToHostSubDict(MsgBuf *msg, std::vector<HostSub> *hosts);
	void	UserToUList(const std::vector<User> &uvec, char *ulist);
	void	UserToUListDict(const std::vector<User> &uvec, IPDict *dict);

	void	AddFileShareDict(IPDict *dict, const IPDictList& share_dlist);
	void	AddBodyDict(IPDict *dict, const char *msg);

	int GetEncryptMaxMsgLen();

	static ULONG MakePacketNo(void);
};

size_t	EncryptDataSize(Cfg *cfg, const std::vector<PubKey *> &masterPub, size_t in_len);
size_t	EncryptData(Cfg *cfg, const std::vector<PubKey *> &masterPub, const BYTE *in, size_t in_len,
	BYTE *out, size_t max_len);
size_t	DecryptData(Cfg *cfg, const BYTE *in, size_t in_len, BYTE *out, size_t max_len);

enum GetIPAddrsMode { GIA_NORMAL, GIA_STRICT, GIA_NOBROADCAST };
AddrInfo *GetIPAddrs(GetIPAddrsMode mode, int *num, int ipv6_mode=0);

BOOL CheckVerifyIPDict(HCRYPTPROV csp, const IPDict *ipdict, PubKey *pub);

#endif

