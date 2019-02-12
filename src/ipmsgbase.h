/*	@(#)Copyright (C) H.Shirouzu 2013-2017   ipmsgbase.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Mssenger base defines
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPMSGBASE_H
#define IPMSGBASE_H

#define IPv6ZEROFILL		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
#define IPv4MAPPED_PREFIX	"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\xff\xff"

struct Addr {
	union {
		char		addr[16];	// big endian
		in_addr		in4;
		in6_addr	in6;
		ULONG		v4addr;		// big endian
	};
	DWORD	size;
	DWORD	mask;

	Addr(void) {
		size = 0;
		mask = 0;
	}
	Addr(void *_addr, DWORD _size, DWORD _mask=0) {
		size = 0;
		Set(_addr, _size, _mask);
	}
	Addr(const char *s, DWORD _mask=0) {
		size = 0;
		Set(s, _mask);
	}
	Addr(const Addr& o) {
		size = 0;
		Set(o.addr, o.size, o.mask);
	}
	bool	IsIPv6() const	{ return size == 16; }
	bool	IsIPv4() const	{ return size ==  4; }
	bool	IsEnabled()	const {
				return size != 0 && memcmp(addr, IPv6ZEROFILL, size) != 0;
			}
	void	Init() 		{ size = 0; }
	int		family()	{ return IsIPv6() ? AF_INET6 : AF_INET; }
	ULONG	V4Addr() const { return IsIPv4() ? htonl(v4addr) : 0; }
	void	SetV4Addr(ULONG _v4addr, DWORD _mask=0) {
				v4addr = htonl(_v4addr);
				size = 4;
				mask = _mask;
			}
	bool	ChangeMappedIPv6()	{
				if (!IsIPv4()) return false;
				memcpy(addr+12, addr, 4);
				memset(addr, 0, 10);
				memset(addr+10, 0xff, 2);
				size = 16;
				return true;
			}
	bool	IsInNet(const Addr& netAddr, DWORD mask_len=0) const {
				if (mask_len == 0) {
					mask_len = netAddr.mask;
				}
				if (IsIPv4() || IsIPv6()) {
					return	ToNet(mask_len) == netAddr.ToNet(mask_len);
				}
				return	false;
			}
	Addr	ToNet(DWORD mask_len=0) const {
				Addr	net_addr;
				DWORD	bits = size * 8;
				if (mask_len == 0) {
					mask_len = mask > 0 ? mask : bits;
				}
				if (mask_len > 0 && mask_len <= bits) {
					int	clear_bytes = (bits - mask_len) / 8;
					int	clear_bits  = (bits - mask_len) % 8;

					net_addr = *this;
					memset(net_addr.addr + (size - clear_bytes), 0, clear_bytes);
					net_addr.addr[clear_bytes] &= (0xff << clear_bits);
					net_addr.mask = mask_len;
				}
				return	net_addr;
			}

	bool	operator ==(const Addr& o) const {
				return	size && size == o.size && memcmp(addr, o.addr, size) == 0;
			}
	bool	operator !=(const Addr& o) const {
				return	!(*this == o);
			}
	bool	operator >(const Addr& o) const {
				return	size > o.size || size == o.size && memcmp(addr, o.addr, size) > 0;
			}
	bool	operator <(const Addr& o) const {
				return	size < o.size || size == o.size && memcmp(addr, o.addr, size) < 0;
			}
	Addr&	operator =(const Addr& o) {
				Set(o.addr, o.size, o.mask);
				return	*this;
			}
	explicit operator bool() const {
				return size != 0;
			}
	bool	Set(const void *_addr, DWORD _size, DWORD _mask=0) {
				if (_size != 16 && _size != 4) return false;
				if (_size == 16 && !memcmp(_addr, IPv4MAPPED_PREFIX, 12)) {
					_addr = (char *)_addr + 12;
					_size = 4;
				}
				memcpy(addr, _addr, size=_size);
				mask = _mask;
				return	true;
			}
	bool	Set(const char *s, DWORD _mask=0) {
				sockaddr_in6	sin6;
				sockaddr_in		sin4;
				int				slen;

				size = 0;
				mask = _mask;
				if (!s || !*s) return false;

				slen = sizeof(sin6);
				if (WSAStringToAddress((char *)s, AF_INET6, 0, (sockaddr *)&sin6, &slen) == 0) {
					return Set(&sin6.sin6_addr, sizeof(sin6.sin6_addr), _mask);
				}
				else {
					slen = sizeof(sin4);
					if (WSAStringToAddress((char *)s, AF_INET, 0, (sockaddr *)&sin4, &slen) == 0) {
						return Set(&sin4.sin_addr, sizeof(sin4.sin_addr), _mask);
					}
				}
				Debug("WSAStringToAddress err=%d %s\n", WSAGetLastError(), s);
				return false;
			}
	bool	Resolve(const char *s, DWORD _mask=0) {
				addrinfo hints={ 0, AF_INET, SOCK_STREAM }, *res=NULL;
				size = 0;
				mask = _mask;
				if (!s || !*s) return false;

				if (getaddrinfo(s, NULL, &hints, &res) == 0) {
					Set(&((sockaddr_in *)res->ai_addr)->sin_addr, 4, _mask);
					freeaddrinfo(res);
					return	true;
				}
				hints.ai_family  = AF_INET6;
				if (getaddrinfo(s, NULL, &hints, &res) == 0) {
					Set(&((sockaddr_in6 *)res->ai_addr)->sin6_addr, 16, _mask);
					freeaddrinfo(res);
					return	true;
				}
				Debug("getaddrinfo err=%d\n", WSAGetLastError());
				return false;
			}
	int		Put(void *_addr, DWORD max_size) {
				if (size > max_size) return 0;
				if (size < max_size && size == 4 && max_size == 16) { // IPv4-mapped IPv6
					memset((char *)_addr+ 0,    0, 10);
					memset((char *)_addr+10, 0xff,  2);
					memcpy((char *)_addr+12, addr,  4);
					return	max_size;
				}
				memcpy(_addr, addr, size);
				return	size;
			}
	char	*S(char *buf=NULL, DWORD bufsize=INET6_ADDRSTRLEN, BOOL is_mask=FALSE) {
#define MAX_ADDRSTR_NUM	8
				static char	_buf[MAX_ADDRSTR_NUM][INET6_ADDRSTRLEN];
				static std::atomic<DWORD> idx = 0;
				if (!buf) {
					buf = _buf[idx++ % MAX_ADDRSTR_NUM];
				}
				*buf = 0;
				if (IsIPv6()) {
					sockaddr_in6	sin6 = { AF_INET6, 0, 0, in6 };
					if (!WSAAddressToString((sockaddr *)&sin6, sizeof(sin6), 0, buf, &bufsize)
						&& is_mask && bufsize > 0) {
						sprintf(buf + bufsize - 1, "/%d", mask);
					}
				}
				else if (IsIPv4()) {
					sockaddr_in		sin4  = { AF_INET,  0, in4 };
					if (WSAAddressToString((sockaddr *)&sin4, sizeof(sin4), 0, buf, &bufsize)
						&& is_mask && bufsize > 0) {
						sprintf(buf + bufsize - 1, "/%d", mask);
					}
				}
				else {
					strcpy(buf, "0");
				}
				return	buf;
			}
};

struct User {
	char	userName[MAX_NAMEBUF];
	char	hostName[MAX_NAMEBUF];

	User() {
		Init();
	}
	User(const User &org) {
		*this = org;
	}
	void Init() {
		*userName = 0;
		*hostName = 0;
	}
	void SetUserName(const char *user) {
		strncpyz(userName, user, sizeof(userName));
	}
	void SetHostName(const char *host) {
		strncpyz(hostName, host, sizeof(hostName));
	}
	User& operator=(const User& u) {
		SetUserName(u.userName);
		SetHostName(u.hostName);
		return	*this;
	}
	bool operator==(const User& u) {
		return	!strcmp(userName, u.userName) && !strcmp(hostName, u.hostName);
	}
};

struct HostSub {
	HostSub() {
		Init();
	}
	HostSub(const HostSub &org) {
		*this = org;
	}
	void Init() {
		u.Init();
		portNo = 0;
	}
	User	u;
	Addr	addr;
	int		portNo;

	void SetUserName(const char *user) { u.SetUserName(user); }
	void SetHostName(const char *host) { u.SetHostName(host); }

	HostSub& operator=(const HostSub& h) {
		u      = h.u;
		addr   = h.addr;
		portNo = h.portNo;
		return	*this;
	}
	const char *S() {
		return	Fmt("%s/%s (%s/%d)", u.userName, u.hostName,
			addr.S(NULL, INET6_ADDRSTRLEN, TRUE), portNo);
	}
};

enum HostLruKind { ALL_LRU, SEG_LRU, MAX_LRU };

struct Host {
	HostSub	hostSub;
	char	nickName[MAX_NAMEBUF];
	char	groupName[MAX_NAMEBUF];
	char	alterName[MAX_NAMEBUF];
	char	verInfoRaw[MAX_VERBUF];
	ULONG	hostStatus = 0;
	time_t	updateTime = 0;
	time_t	updateTimeDirect = 0;
	DWORD	lastList = 0;
	DWORD	lastPoll = 0;
	int		priority = 0;
	int		refCnt = 0;
	PubKey	pubKey;
	bool	active = false;
	bool	agent = true;
	Addr	parent_seg;
	int		agentVal = 0;

	std::list<Host *>::iterator itr[MAX_LRU];

	Host(void) {
		Init();
	}
	Host(const Host &org) {
		*this = org;
	}
	~Host() {
	}

	void Init() {
		*nickName  = 0;
		*groupName = 0;
		*alterName = 0;
		*verInfoRaw = 0;
		parent_seg.Init();
	}
	int	RefCnt(int cnt=0) {
		return refCnt += cnt;
	}
	BOOL SafeRelease() {
		if (refCnt <= 0) {
			delete this;
			return	TRUE;
		}
		return	FALSE;
	}
	Host& operator=(const Host& h) { // itr and refCnt is not copied
		hostSub = h.hostSub;
		// itr = h.itr
		strncpyz(nickName, h.nickName, sizeof(nickName));
		strncpyz(groupName, h.groupName, sizeof(groupName));
		strncpyz(alterName, h.alterName, sizeof(alterName));
		strncpyz(verInfoRaw, h.verInfoRaw, sizeof(verInfoRaw));
		hostStatus = h.hostStatus;
		updateTime = h.updateTime;
		updateTimeDirect = h.updateTimeDirect;
		lastList = h.lastList;
		lastPoll = h.lastPoll;
		priority = h.priority;
		//refCnt = h.refCnt;
		pubKey = h.pubKey;
		active = h.active;
		agent = h.agent;
		agentVal = h.agentVal;
		return	*this;
	}
	char	*S(char *buf=NULL, DWORD bufsize=MAX_LISTBUF) {
#define MAX_HOSTSTR_NUM	8
		static char	_buf[MAX_HOSTSTR_NUM][MAX_LISTBUF];
		static std::atomic<DWORD> idx = 0;
		if (!buf) {
			buf = _buf[idx++ % MAX_HOSTSTR_NUM];
		}
		int len = strncpyz(buf, *nickName ? nickName : hostSub.u.userName, bufsize);
		len += snprintfz(buf + len, bufsize-len, " (%s/%s", groupName, hostSub.u.hostName);
		len += snprintfz(buf + len, bufsize-len, "/%s", hostSub.addr.S());
		len += snprintfz(buf + len, bufsize-len, "/%s", hostSub.u.userName);

		strncpyz(buf + len, ")", bufsize-len);
		return	buf;
	}
};

struct DirAgent {
	Host	*host;
	BOOL	isReq;
	DWORD	resTick;

	DirAgent(Host *h=NULL) {
		host	= h;
		isReq	= FALSE;
		resTick	= 0;
	}
};

#define MAX_AGENT 3

class THosts {
public:
	enum Kind { NAME, ADDR, NAME_ADDR, MAX_KIND };
	BOOL	enable[MAX_KIND];

	std::list<Host *>	lruList;
	int					lruIdx;
	BOOL				isSameSeg;
	DWORD				brSetTick;
	DWORD				brResTick;
	Addr				brSetAddr;

	std::vector<DirAgent> agents;

protected:
	int		hostCnt;
	Host	**array[MAX_KIND];
	Host	*Search(Kind kind, HostSub *hostSub, int *insertIndex=NULL);
	int		Cmp(HostSub *hostSub1, HostSub *hostSub2, Kind kind);

public:
	THosts(void);
	~THosts();

	void	Enable(Kind kind, BOOL _enable) { enable[kind] = _enable; }
	void	SetLruIdx(int idx) { lruIdx = idx; }
	void	ClearHosts();
	BOOL	AddHost(Host *host);
	BOOL	DelHost(Host *host);
	int		HostCnt(void) { return hostCnt; }
	void	UpdateLru(Host *host) {
				if (lruIdx >= 0) {
					lruList.splice(lruList.begin(), lruList, host->itr[lruIdx]);
				}
			}
	Host	*GetHost(int index, Kind kind=NAME) {
				return array[kind][index];
			}
	Host	*GetHostByName(HostSub *hostSub) {
				return enable[NAME] ? Search(NAME, hostSub) : NULL;
			}
	Host	*GetHostByAddr(HostSub *hostSub) {
				return enable[ADDR] ? Search(ADDR, hostSub) : NULL;
			}
	Host	*GetHostByAddr(Addr addr, int portNo) {
				HostSub	hostSub;
				hostSub.addr = addr;
				hostSub.portNo = portNo;
				return enable[ADDR] ? Search(ADDR, &hostSub) : NULL;
			}
	Host	*GetHostByNameAddr(HostSub *hostSub) {
				return enable[NAME_ADDR] ? Search(NAME_ADDR, hostSub) : NULL;
			}
	int		PriorityHostCnt(int priority, int range=1);
};

struct AddrObj : public TListObj {
	Addr	addr;
	int		portNo;
};

struct UrlObj : public TListObj {
	char	protocol[MAX_NAMEBUF];
	char	program[MAX_PATH_U8];
};

struct UserObj : public TListObj {
	HostSub	hostSub;
};

struct UsersObj : public TListObj {
	TListEx<UserObj>	users;
	~UsersObj() {
		UserObj *obj;
		while ((obj = users.TopObj())) {
			users.DelObj(obj); delete obj;
		}
	}
};

struct MsgIdent {
	MsgIdent() {
		Init();
	}
	void Init() {
		msgId = 0;
		packetNo = 0;
	}
	void Init(const char *userName, const char *hostName, ULONG _packetNo=0, int64 _msgId=0) {
		uid.Init(userName);
		host.Init(hostName);
		packetNo = _packetNo;
		msgId = _msgId;
	}
	bool operator==(const MsgIdent& mi) const {
		if (msgId && msgId == mi.msgId) {
			return	true;
		}
		return	packetNo == mi.packetNo &&
				uid == mi.uid &&
				host == mi.host;
	}
	bool operator!=(const MsgIdent& mi) const {
		return	!(*this == mi);
	}
	ULONG	packetNo;
	int64	msgId;
	Wstr	uid;	// HostSub.UserName
	Wstr	host;
};

struct UpdateLogReq {
	int64	msg_id;		// 0 means all msgs
	void	*parent;
	UpdateLogReq(int64 _msg_id=0, void *_parent=NULL) {
		msg_id = _msg_id;
		parent = _parent;
	}
};

Addr ResolveAddr(const char *_host);

struct TBrObj : public TListObj {
	char	*host;
	Addr	addr;

	TBrObj(const char *_host=NULL, Addr _addr=0) {
		host = NULL;
		if (_host) {
			host = strdup(_host);
		}
		addr = _addr;
	}
	~TBrObj() {
		if (host) free(host);
	};
	const char *Host() { return	host; }
	Addr	Addr(BOOL realTime=FALSE) {
		return	realTime ? (addr = ResolveAddr(host)) : addr;
	}
};

class TBrList : public TListEx<TBrObj> {
public:
	TBrList() {}
	~TBrList() { Reset(); }
	void Reset(void);
//	BOOL SetHost(const char *host);
	BOOL SetHostRaw(const char *host, Addr addr, BOOL dup_check=FALSE) {
		if (dup_check) {
			for (auto obj=TopObj(); obj; obj=NextObj(obj)) {
				if (obj->Addr() == addr) {
					return	TRUE;
				}
			}
		}
		TBrObj *obj = new TBrObj(host, addr);
		AddObj(obj);
		return	TRUE;
	}
//	BOOL IsExistHost(const char *host);
};


#define rRGB(r,g,b)  ((DWORD)(((BYTE)(b)|((WORD)(g) << 8))|(((DWORD)(BYTE)(r)) << 16)))

inline int64 MakeMsgId(WPARAM high, LPARAM low) {
	return	(((int64)(DWORD)high) << 32) | (int64)(DWORD)low;
}

inline WPARAM MakeMsgIdHigh(int64 msg_id) {
	return	(WPARAM)(DWORD)(msg_id >> 32);
}

inline LPARAM MakeMsgIdLow(int64 msg_id) {
	return	(LPARAM)(DWORD)msg_id;
}

#endif

