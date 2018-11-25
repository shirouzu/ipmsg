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

	int		id = 0;
	UINT	attr = 0;
	int		pos = -1;
	int64	size = 0;
	time_t	mtime = 0;
	time_t	atime = 0;
	time_t	crtime = 0;
	bool	isSelected = false;		// for recvdlg

public:
	FileInfo(int _id=0) {
		id = _id;
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
	void SetFname(const char *_fname, bool replace_org=false) {
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
	bool IsSelected() { return isSelected; }
	void SetSelected(bool _isSelected) { isSelected = _isSelected; }
};

struct MsgBuf {
/* SavePacket でそのまま格納 */
	HostSub	hostSub = {};
	VerInfo	verInfo = {};	// client version
	int		version = 0;	// protocol version
	int		portNo = 0;
	int		pollReq = 0;
	bool	viaAgent = false;
	ULONG	packetNo = 0;
	ULONG	command = 0;
	time_t	timestamp = 0;
	int64	msgId = 0;		// generate logdb (available after logging)

	ULONG	flags = 0;
	enum { SIGN_INIT, SIGN_NOKEY, SIGN_OK, SIGN_NG } signMode = SIGN_INIT;
	enum { DEC_INIT, DEC_OK, DEC_NG } decMode = DEC_INIT;

/* ここまで */
	DynBuf	msgBuf;
	DynBuf	exBuf;
	DynBuf	uList;
	char	packetNoStr[AES_BLOCK_SIZE] = {};// for IV (256bit)
	char	verInfoRaw[MAX_VERBUF] = {};

	IPDict	ipdict;
	IPDict	wrdict;
	IPDict	orgdict;
	U8str	body;
	U8str	group;	// only entry/notify
	U8str	nick;	// only entry/notify
};


struct RecvBuf {
	Addr	addr;
	int		port = 0;
	int		size = 0;
	DynBuf	msgBuf;
};

struct ConnectInfo : public TListObj {
	SOCKET	sd = 0;
	Addr	addr;
	int		port = 0;
	bool	server = 0;
	bool	complete = false;
	bool	fin = false;
	DWORD	startTick = 0;
	DWORD	lastTick = 0;
	UINT	uMsg = WM_TCPEVENT;
	DynBuf	buf;
	bool	growSbuf = false;
	DWORD	addEvFlags = 0;
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

	bool		status;
	static ULONG packetNo;
	Cfg			*cfg;
	THosts		*hosts;
	bool		isV6;

	HWND		hAsyncWnd;
	UINT		uAsyncMsg;
	UINT		lAsyncMode;
	static HostSub local;
	static HostSub localA;
	HostSub		orgLocal;
	DynBuf		lastPacket;

	bool	WSockInit();
	void	WSockTerm(void);
	bool	WSockReset(void);
	bool	RecvCore(RecvBuf *buf, MsgBuf *msg);

public:
	MsgMng(Addr nicAddr, int portNo, Cfg *cfg=NULL, THosts *hosts=NULL);
	~MsgMng();

	static bool	WSockStartup();
	static void	WSockCleanup();

	bool	MulticastJoin();
	bool	MulticastLeave();

	bool	GetStatus(void)	{ return	status; }
	static HostSub *GetLocalHost(void) { return &local; }
	static HostSub *GetLocalHostA(void) { return &localA; }
	HostSub	*GetOrgLocalHost(void) { return &orgLocal; }
	void	CloseSocket(void);
	bool	IsAvailableTCP() { return tcp_sd != INVALID_SOCKET ? TRUE : FALSE; }

	bool	Send(HostSub *hostSub, ULONG command, int val);
	bool	Send(HostSub *hostSub, ULONG command, const char *message=NULL,
				const char *exMsg=NULL, const IPDict *ipdict=NULL);
	bool	Send(Addr host_addr, int port_no, ULONG command, const char *message=NULL,
				const char *exMsg=NULL, const IPDict *ipdict=NULL);
	bool	Send(HostSub *hostSub, IPDict *ipdict);
	bool	Send(Addr host_addr, int port_no, IPDict *ipdict);
	bool	AsyncSelectRegister(HWND hWnd);

	bool	Recv(MsgBuf *msg);
	bool	PseudoRecv(RecvBuf *buf, MsgBuf *msg);
	bool	ResolveDictMsg(MsgBuf *msg);
	bool	GetWrappedMsg(MsgBuf *msg, MsgBuf *wmsg);
	bool	CheckVerify(MsgBuf *msg, const IPDict *ipdict);

	bool	ResolveMsg(char *buf, int size, MsgBuf *msg);
	bool	ResolveMsg(RecvBuf *buf, MsgBuf *msg);
	bool	GetPubKey(HostSub *hostSub, bool isAuto=TRUE);
	bool	DecryptMsg(MsgBuf *msg, UINT *cryptCapa, UINT *logOpt);

	ULONG	InitIPDict(IPDict *ipdict, ULONG command=0, ULONG flags=0, ULONG pktno=0);
	bool	SignIPDict(IPDict *ipdict);

	bool	EncIPDict(IPDict *ipdict, PubKey *pub, DynBuf *dbuf);
	bool	EncIPDict(IPDict *ipdict, PubKey *pub, IPDict *out_dict);
	size_t	DecIPDict(BYTE *data, size_t size, IPDict *dict);
	bool	DecIPDict(IPDict *wdict, IPDict *dict);

	ULONG	MakeMsg(char *udp_msg, ULONG packetNo, ULONG command, const char *msg,
			const char *exMsg=NULL, const IPDict *ipdict=NULL, int *packet_len=NULL);
	ULONG	MakeMsg(char *udp_msg, ULONG command, const char *msg, const char *exMsg=NULL,
					const IPDict *ipdict=NULL, int *packet_len=NULL) {
				return	MakeMsg(udp_msg, MakePacketNo(), command, msg, exMsg, ipdict, packet_len);
			}
	bool	MakeMsg(IPDict *ipdict, DynBuf *buf);
	bool	MakeEncryptMsg(Host *host, ULONG packet_no, char *msg_str, bool is_extenc,
							char *share_str, char *ulist_str, char *buf);
	bool	UdpSend(Addr host_addr, int port_no, const char *udp_msg);
	bool	UdpSend(Addr host_addr, int port_no, const char *udp_msg, int len);
	bool	UdpSendCore(Addr host_addr, int port_no, const char *udp_msg, int len);
	bool	UdpRecv(RecvBuf *buf);

	bool	Accept(HWND hWnd, ConnectInfo *info);
	bool	Connect(HWND hWnd, ConnectInfo *info);
	bool	AsyncSelectConnect(HWND hWnd, ConnectInfo *info);
	bool	ConnectDone(HWND hWnd, ConnectInfo *info);

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

bool CheckVerifyIPDict(HCRYPTPROV csp, const IPDict *ipdict, PubKey *pub);

#endif

