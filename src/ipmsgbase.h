/*	@(#)Copyright (C) H.Shirouzu 2013-2015   ipmsgbase.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Mssenger base defines
	Create					: 2013-03-03(Sun)
	Update					: 2015-06-02(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPMSGBASE_H
#define IPMSGBASE_H

#define IPv6ZEROFILL		"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0"
#define IPv4MAPPED_PREFIX	"\x0\x0\x0\x0\x0\x0\x0\x0\x0\x0\xff\xff"

struct Addr {
	union {
		char		addr[16];
		in_addr		in4;
		in6_addr	in6;
		ULONG		v4addr;
	};
	DWORD	size;

	Addr(void *_addr, DWORD _size) { Set(_addr, _size); }
	Addr(void)			{ size = 0; }
	Addr(const char *s) { Set(s); }
	Addr(const Addr& o) { Set(o.addr, o.size); }

	bool	IsIPv6()	{ return size == 16; }
	bool	IsIPv4()	{ return size ==  4; }
	bool	IsEnabled()	{
				return size != 0 && memcmp(addr, IPv6ZEROFILL, size) != 0;
			}
	int		family()	{ return IsIPv6() ? AF_INET6 : AF_INET; }
	ULONG	V4Addr()	{ return IsIPv4() ? htonl(v4addr) : 0; }
	void	SetV4Addr(ULONG _v4addr) { v4addr = htonl(_v4addr); size = 4; }
	void	ChangeMappedIPv6()	{
				if (!IsIPv4()) return;
				memcpy(addr+12, addr, 4); memset(addr, 0, 10); memset(addr+10, 0xff, 2); size=16;
			}

	bool	operator ==(const Addr& o) {
				return	size && size == o.size && memcmp(addr, o.addr, size) == 0;
			}
	bool	operator !=(const Addr& o) {
				return	!size || size != o.size || memcmp(addr, o.addr, size);
			}
	bool	operator >(const Addr& o) {
				return	size > o.size || size == o.size && memcmp(addr, o.addr, size) > 0;
			}
	bool	operator <(const Addr& o) {
				return	size < o.size || size == o.size && memcmp(addr, o.addr, size) < 0;
			}
	Addr&	operator =(const Addr& o) {
				Set(o.addr, o.size);
				return	*this;
			}
	void	Set(const void *_addr, DWORD _size) {
				if (_size > sizeof(addr)) return;
				if (_size == 16 && !memcmp(_addr, IPv4MAPPED_PREFIX, 12)) {
					_addr = (char *)_addr + 12;
					_size = 4;
				}
				memcpy(addr, _addr, size=_size);
			}
	void	Set(const char *s) {
				sockaddr_in6	sin6;
				sockaddr_in		sin4;
				int				slen;
				if (!s || !*s) {
					size = 0;
					return;
				}
				slen = sizeof(sin6);
				if (WSAStringToAddress((char *)s, AF_INET6, 0, (sockaddr *)&sin6, &slen) == 0) {
					Set(&sin6.sin6_addr, sizeof(sin6.sin6_addr));
				}
				else {
					slen = sizeof(sin4);
					if (WSAStringToAddress((char *)s, AF_INET, 0, (sockaddr *)&sin4, &slen) == 0) {
						Set(&sin4.sin_addr, sizeof(sin4.sin_addr));
					}
					else {
						size = 0;
						Debug("WSAStringToAddress err=%d\n", WSAGetLastError());
					}
				}
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
	char	*ToStr(char *buf=NULL, DWORD bufsize=INET6_ADDRSTRLEN) {
				static char _buf[INET6_ADDRSTRLEN];
				if (!buf) buf = _buf;
				*buf = 0;
				if (IsIPv6()) {
					sockaddr_in6	sin6 = { AF_INET6, 0, 0, in6 };
					WSAAddressToString((sockaddr *)&sin6, sizeof(sin6), 0, buf, &bufsize);
				}
				else if (IsIPv4()) {
					sockaddr_in		sin4  = { AF_INET,  0, in4 };
					WSAAddressToString((sockaddr *)&sin4, sizeof(sin4), 0, buf, &bufsize);
				}
				else {
					strcpy(buf, "0");
				}
				return	buf;
			}
};

struct HostSub {
	HostSub() { Init(); }
	void Init() { *userName = 0; *hostName = 0; portNo = 0; }
	char	userName[MAX_NAMEBUF];
	char	hostName[MAX_NAMEBUF];
	Addr	addr;
	int		portNo;
};

struct Host {
	HostSub	hostSub;
	char	nickName[MAX_NAMEBUF];
	char	groupName[MAX_NAMEBUF];
	char	alterName[MAX_NAMEBUF];
	ULONG	hostStatus;
	Time_t	updateTime;
	int		priority;
	int		refCnt;
	PubKey	pubKey;
	BOOL	pubKeyUpdated;

	Host(void) { Init(); }
	~Host() { refCnt = 0; }
	void Init() {
		refCnt = 0; pubKeyUpdated = FALSE; *nickName = *groupName = *alterName = 0;
		hostStatus = 0; updateTime = 0; priority = 0;
	}
	int	RefCnt(int cnt=0) { return refCnt += cnt; }
};

class THosts {
public:
	enum Kind { NAME, ADDR, NAME_ADDR, MAX_ARRAY };
	BOOL enable[MAX_ARRAY];

protected:
	int		hostCnt;
	Host	**array[MAX_ARRAY];
	Host	*Search(Kind kind, HostSub *hostSub, int *insertIndex=NULL);
	int		Cmp(HostSub *hostSub1, HostSub *hostSub2, Kind kind);

public:
	THosts(void);
	~THosts();

	void	Enable(Kind kind, BOOL _enable) { enable[kind] = _enable; }
	BOOL	AddHost(Host *host);
	BOOL	DelHost(Host *host);
	int		HostCnt(void) { return hostCnt; }
	Host	*GetHost(int index, Kind kind=NAME) { return array[kind][index]; }
	Host	*GetHostByName(HostSub *hostSub) { return enable[NAME] ? Search(NAME, hostSub) : NULL; }
	Host	*GetHostByAddr(HostSub *hostSub) { return enable[ADDR] ? Search(ADDR, hostSub) : NULL; }
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
		while ((obj = users.TopObj())) { users.DelObj(obj); delete obj; }
	}
};

Addr ResolveAddr(const char *_host);

struct TBrObj : public TListObj {
	char	*host;
	Addr	addr;

	TBrObj(const char *_host=NULL, Addr _addr=0) {
		host = NULL;
		if (_host) host = strdup(_host);
		addr=_addr;
	}
	~TBrObj() { if (host) free(host); };
	const char *Host() { return	host; }
	Addr	Addr(BOOL realTime=FALSE) { return realTime ? (addr = ResolveAddr(host)) : addr; }
};

class TBrList : public TListEx<TBrObj> {
public:
	TBrList() {}
	~TBrList() { Reset(); }
	void Reset(void);
//	BOOL SetHost(const char *host);
	void SetHostRaw(const char *host, Addr addr) {
		TBrObj *obj = new TBrObj(host, addr); AddObj(obj);
	}
//	BOOL IsExistHost(const char *host);
};


#define rRGB(r,g,b)  ((DWORD)(((BYTE)(b)|((WORD)(g) << 8))|(((DWORD)(BYTE)(r)) << 16)))

// 1601年1月1日から1970年1月1日までの通算100ナノ秒
#define UNIXTIME_BASE	((_int64)0x019db1ded53e8000)

inline Time_t FileTime2UnixTime(FILETIME *ft) {
	return	(Time_t)((*(_int64 *)ft - UNIXTIME_BASE) / 10000000);
}
inline void UnixTime2FileTime(Time_t ut, FILETIME *ft) {
	*(_int64 *)ft = (_int64)ut * 10000000 + UNIXTIME_BASE;
}

#endif

