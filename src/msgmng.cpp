static char *msgmng_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   msgmng.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Message Manager
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "aes.h"
#include "blowfish.h"
#include <iphlpapi.h>

using namespace std;

ULONG MsgMng::packetNo;
HostSub MsgMng::local;
HostSub MsgMng::localA;

MsgMng::MsgMng(Addr nicAddr, int portNo, Cfg *_cfg, THosts *_hosts)
{
	status = FALSE;
	packetNo = (ULONG)time(NULL);
	udp_sd = tcp_sd = INVALID_SOCKET;
	hAsyncWnd = 0;
	local.addr = nicAddr;
	local.portNo = portNo;
	cfg = _cfg;
	hosts = _hosts;
	isV6 = cfg && cfg->IPv6Mode;
	lastPacket.Alloc(MAX_UDPBUF);

	if (!WSockInit()) return;

	WCHAR	wbuf[128];
	DWORD	size = wsizeof(wbuf);
	if (!::GetComputerNameW(wbuf, &size)) {
		GetLastErrorMsg("GetComputerName()");
		return;
	}
	WtoU8(wbuf, local.u.hostName, sizeof(local.u.hostName));
	WtoA(wbuf, localA.u.hostName, sizeof(localA.u.hostName));

	if (nicAddr.IsEnabled()) {
		char	host[MAX_BUF];
		if (::gethostname(host, sizeof(host)) == -1)
			strcpy(host, local.u.hostName);

//		hostent	*ent = ::gethostbyname(host);
//		if (ent) {
//			local.addr.Set(ent->h_addr_list[0]);
//		}
	}

	size = wsizeof(wbuf);
	if (!::GetUserNameW(wbuf, &size)) {
		GetLastErrorMsg("GetUserName()");
		return;
	}
	WtoU8(wbuf, local.u.userName, sizeof(local.u.userName));
	WtoA(wbuf, localA.u.userName, sizeof(localA.u.userName));
	orgLocal = local;

	status = TRUE;
}

MsgMng::~MsgMng()
{
	WSockTerm();
}

static bool is_wsock_startup = 0;

BOOL MsgMng::WSockStartup()
{
	if (!is_wsock_startup) {
		WSADATA	wsaData;
		if (::WSAStartup(MAKEWORD(2,2), &wsaData) &&
			::WSAStartup(MAKEWORD(1,1), &wsaData)) {
			return	GetSockErrorMsg("WSAStart()"), FALSE;
		}
		is_wsock_startup = true;
	}
	return	TRUE;
}

void MsgMng::WSockCleanup()
{
	if (is_wsock_startup) {
		::WSACleanup();
		is_wsock_startup = false;
	}
}

BOOL MsgMng::MulticastJoin()
{
	if (!isV6 || !cfg) return FALSE;

	if ((cfg->MulticastMode & 0x1) == 0) {
		int val = 32;
		if (::setsockopt(udp_sd, IPPROTO_IPV6, IP_MULTICAST_TTL, (char *)&val, sizeof(val))) {
			GetSockErrorMsg("setsockopt(IP_MULTICAST_TTL) error");
		}

		ipv6_mreq	mreq = {};
		Addr		maddr(cfg->IPv6Multicast);

		maddr.Put(&mreq.ipv6mr_multiaddr, 16);
		if (::setsockopt(udp_sd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq, sizeof(mreq))) {
			GetSockErrorMsg("setsockopt(IPV6_DROP_MEMBERSHIP) error");
		}
	}
	if ((cfg->MulticastMode & 0x2) == 0) {
		ipv6_mreq	mreq = {};
		Addr		laddr(LINK_MULTICAST_ADDR6);

		laddr.Put(&mreq.ipv6mr_multiaddr, 16);
		if (::setsockopt(udp_sd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq, sizeof(mreq))) {
			GetSockErrorMsg("setsockopt(IPV6_DROP_MEMBERSHIP2) error");
		}
	}
	return	TRUE;
}

BOOL MsgMng::MulticastLeave()
{
	if (!isV6 || !cfg) return FALSE;

	if ((cfg->MulticastMode & 0x1) == 0) {
		ipv6_mreq	mreq = {};
		Addr		maddr(cfg->IPv6Multicast);

		maddr.Put(&mreq.ipv6mr_multiaddr, 16);
		if (::setsockopt(udp_sd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (char *)&mreq, sizeof(mreq))){
			GetSockErrorMsg("setsockopt(IPV6_DROP_MEMBERSHIP) error");
		}
	}
	if ((cfg->MulticastMode & 0x2) == 0) {
		ipv6_mreq	mreq = {};
		Addr		laddr(LINK_MULTICAST_ADDR6);

		laddr.Put(&mreq.ipv6mr_multiaddr, 16);
		if (::setsockopt(udp_sd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (char *)&mreq, sizeof(mreq))){
			GetSockErrorMsg("setsockopt(IPV6_DROP_MEMBERSHIP2) error");
		}
	}
	return	TRUE;
}

BOOL MsgMng::WSockInit()
{
	BOOL		flg; // socket flag
	ULONG		family = isV6 ? AF_INET6 : AF_INET;

	if (!WSockStartup()) return FALSE;

	if ((udp_sd = ::socket(family, SOCK_DGRAM, 0)) == INVALID_SOCKET)
		return	GetSockErrorMsg("Please setup TCP/IP(controlpanel->network)\r\n"), FALSE;

	if (family == AF_INET6) {
		flg = cfg && cfg->IPv6Mode == 1 ? TRUE : FALSE;
		if (::setsockopt(udp_sd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&flg, sizeof(flg)))
			return	GetSockErrorMsg("setsockopt(IPV6_V6ONLY)"), FALSE;
	}

	if (!cfg) return TRUE;

	if (!cfg->NoTcp) {
		if ((tcp_sd = ::socket(family, SOCK_STREAM, 0)) == INVALID_SOCKET)
			return	GetSockErrorMsg("Please setup2 TCP/IP(controlpanel->network)\r\n"), FALSE;
	}

	if (IsAvailableTCP() && family == AF_INET6) {
		flg = cfg && cfg->IPv6Mode == 1 ? TRUE : FALSE;
		if (::setsockopt(tcp_sd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&flg, sizeof(flg)))
			return	GetSockErrorMsg("setsockopt2(IPV6_V6ONLY)"), FALSE;
	}

	sockaddr_in6	addr6 = {};
	sockaddr_in		addr4 = {};
	sockaddr		*addr = isV6 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
	int				size =  isV6 ? sizeof(addr6) : sizeof(addr4);

	if (isV6) {
		addr6.sin6_family	= AF_INET6;
		addr6.sin6_port		= ::htons(local.portNo);
		if (local.addr.IsEnabled() && local.addr.IsIPv6()) {
			local.addr.Put(&addr6.sin6_addr, 16);
		}
	}
	else {
		addr4.sin_family	= AF_INET;
		addr4.sin_port		= ::htons(local.portNo);
		if (local.addr.IsEnabled() && local.addr.IsIPv4()) {
			local.addr.Put(&addr4.sin_addr, 4);
		}
	}

	if (::bind(udp_sd, addr, size)) return GetSockErrorMsg("bind()"), FALSE;

	if (isV6 && cfg) {
		MulticastJoin();
	}

	if (IsAvailableTCP() && ::bind(tcp_sd, addr, size)) {
		::closesocket(tcp_sd);
		tcp_sd = INVALID_SOCKET;
		GetSockErrorMsg("bind(tcp) error. Can't support file attach");
	}

	flg = TRUE;	// Non Block
	if (::ioctlsocket(udp_sd, FIONBIO, (unsigned long *)&flg))
		return	GetSockErrorMsg("ioctlsocket(nonblock)"), FALSE;

	if (IsAvailableTCP() && ::ioctlsocket(tcp_sd, FIONBIO, (unsigned long *)&flg))
		return	GetSockErrorMsg("ioctlsocket tcp(nonblock)"), FALSE;

	flg = TRUE;			// allow broadcast
	if (::setsockopt(udp_sd, SOL_SOCKET, SO_BROADCAST, (char *)&flg, sizeof(flg)))
		return	GetSockErrorMsg("setsockopt(broadcast)"), FALSE;

	int	buf_size = MAX_SOCKBUF, buf_minsize = MAX_SOCKBUF / 2;		// UDP バッファ設定
	if (::setsockopt(udp_sd, SOL_SOCKET, SO_SNDBUF, (char *)&buf_size, sizeof(int))
	&&	::setsockopt(udp_sd, SOL_SOCKET, SO_SNDBUF, (char *)&buf_minsize, sizeof(int)))
		GetSockErrorMsg("setsockopt(sendbuf)");

	buf_size = MAX_SOCKBUF, buf_minsize = MAX_SOCKBUF / 2;
	if (::setsockopt(udp_sd, SOL_SOCKET, SO_RCVBUF, (char *)&buf_size, sizeof(int))
	&&	::setsockopt(udp_sd, SOL_SOCKET, SO_RCVBUF, (char *)&buf_minsize, sizeof(int)))
		GetSockErrorMsg("setsockopt(recvbuf)");

	flg = TRUE;	// REUSE ADDR
	if (IsAvailableTCP()
		&& ::setsockopt(tcp_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&flg, sizeof(flg))) {
		GetSockErrorMsg("setsockopt tcp(reuseaddr)");
	}

	if (IsAvailableTCP() && ::listen(tcp_sd, 100))
		return	FALSE;

	return	TRUE;
}

void MsgMng::WSockTerm(void)
{
	CloseSocket();
	WSockCleanup();
}

void MsgMng::CloseSocket(void)
{
	if (tcp_sd != INVALID_SOCKET) {
		::closesocket(tcp_sd);
		tcp_sd = INVALID_SOCKET;
	}
	if (udp_sd == INVALID_SOCKET) return;

	if (isV6 && cfg) {
		MulticastLeave();
	}
	::closesocket(udp_sd);
	udp_sd = INVALID_SOCKET;
}

BOOL MsgMng::WSockReset(void)
{
	WSockTerm();
	return	WSockInit();
}

BOOL MsgMng::Send(HostSub *hostSub, ULONG command, int val)
{
	char	buf[MAX_NAMEBUF];

	sprintf(buf, "%d", val);
	return	Send(hostSub, command, buf);
}

BOOL MsgMng::Send(HostSub *hostSub, ULONG command, const char *message, const char *exMsg,
	const IPDict *ipdict)
{
	return	Send(hostSub->addr, hostSub->portNo, command, message, exMsg, ipdict);
}

BOOL MsgMng::Send(Addr host, int port_no, ULONG command, const char *message, const char *exMsg,
	const IPDict *ipdict)
{
	DynBuf	buf(MAX_UDPBUF);
	int		trans_len;

	MakeMsg(buf, command, message, exMsg, ipdict, &trans_len);
	return	UdpSend(host, port_no, buf, trans_len);
}

BOOL MsgMng::Send(HostSub *hostSub, IPDict *ipdict)
{
	return	Send(hostSub->addr, hostSub->portNo, ipdict);
}

BOOL MsgMng::Send(Addr host, int port_no, IPDict *ipdict)
{
	DynBuf	buf(MAX_UDPBUF);

	if (!MakeMsg(ipdict, &buf)) {
		return	FALSE;
	}
	return	UdpSend(host, port_no, buf, (int)buf.UsedSize());
}

BOOL MsgMng::AsyncSelectRegister(HWND hWnd)
{
	if (hAsyncWnd == 0)
		hAsyncWnd = hWnd;

	if (::WSAAsyncSelect(udp_sd, hWnd, WM_UDPEVENT, FD_READ) == SOCKET_ERROR)
		return	FALSE;

	if (::WSAAsyncSelect(tcp_sd, hWnd, WM_TCPEVENT, FD_ACCEPT|FD_CLOSE) == SOCKET_ERROR)
		return	FALSE;

	return	TRUE;
}

BOOL MsgMng::RecvCore(RecvBuf *buf, MsgBuf *msg)
{
	memcpy(lastPacket.Buf(), buf->msgBuf, buf->size);
	lastPacket.Buf()[buf->size] = 0;
	lastPacket.SetUsedSize(buf->size);

	return	ResolveMsg(buf, msg);
}

BOOL MsgMng::Recv(MsgBuf *msg)
{
	RecvBuf		buf;

	if (!UdpRecv(&buf) || buf.size == 0) {
		return	FALSE;
	}

	return	RecvCore(&buf, msg);
}

BOOL MsgMng::PseudoRecv(RecvBuf *buf, MsgBuf *msg)
{
	return	RecvCore(buf, msg);
}

ULONG MsgMng::InitIPDict(IPDict *ipdict, ULONG cmd, ULONG flags, ULONG _pktno)
{
#ifndef USE_ADMIN
	TMainWin	*mainWin = GetMain();

	if (!mainWin) return 0;
#endif

	ULONG	pkt_no = _pktno ? _pktno : MakePacketNo();
	ipdict->clear();

	ipdict->put_int(IPMSG_VER_KEY, IPMSG_NEW_VERSION);
	ipdict->put_int(IPMSG_PKTNO_KEY, pkt_no);
	ipdict->put_int(IPMSG_DATE_KEY, time(NULL));

	ipdict->put_str(IPMSG_UID_KEY, local.u.userName);
	ipdict->put_str(IPMSG_HOST_KEY, local.u.hostName);

	ipdict->put_int(IPMSG_CMD_KEY, cmd);
	ipdict->put_int(IPMSG_FLAGS_KEY, flags);

	ipdict->put_str(IPMSG_CLIVER_KEY, GetVerHexInfo());

	ipdict->put_str(IPMSG_GROUP_KEY, cfg->GroupNameStr);

#ifndef USE_ADMIN
	if (mainWin) {
		ipdict->put_str(IPMSG_NICK_KEY, mainWin->GetNickNameEx());
		ipdict->put_int(IPMSG_STAT_KEY, mainWin->HostStatus());
	}
#endif

	return	pkt_no;
}

BOOL MsgMng::MakeMsg(IPDict *ipdict, DynBuf *buf)
{
	buf->Alloc(ipdict->pack_size());

	size_t	size = ipdict->pack(buf->Buf(), buf->Size());
	buf->SetUsedSize(size);

	return	size ? TRUE : FALSE;
}

BOOL MsgMng::SignIPDict(IPDict *ipdict)
{
	TrcW(L"SignIPDict\n");

	if (ipdict->has_key(IPMSG_SIGN_KEY)) {
		ipdict->del_key(IPMSG_SIGN_KEY);
	}

	PubKey&	pubKey = cfg->pub[KEY_2048];
	ipdict->put_int(IPMSG_PUB_E_KEY, pubKey.Exponent());

	BYTE	key[RSA2048_KEY_SIZE];
	memcpy(key, pubKey.Key(), RSA2048_KEY_SIZE);
	swap_s(key, RSA2048_KEY_SIZE);

	ipdict->put_bytes(IPMSG_PUB_N_KEY, key, RSA2048_KEY_SIZE);
	ipdict->put_int(IPMSG_ENCFLAG_KEY, IPMSG_SIGN_SHA256);
	ipdict->put_int(IPMSG_ENCCAPA_KEY, cfg->GetCapa());

	DynBuf	dbuf(MAX_UDPBUF);
	size_t	size = ipdict->pack(dbuf.Buf(), dbuf.Size());
	dbuf.SetUsedSize(size);

	HCRYPTHASH	hHash;
	HCRYPTPROV	csp	= cfg->priv[KEY_2048].hCsp;
	BOOL		ret = FALSE;
	DWORD		sigLen = 0;

	if (::CryptCreateHash(csp, CALG_SHA_256, 0, 0, &hHash)) {
		if (::CryptHashData(hHash, dbuf.Buf(), (DWORD)size, 0)) {
			sigLen = RSA2048_SIGN_SIZE;
			TrcW(L"CryptSignHash(%zd)\n", size);
			if (!::CryptSignHashW(hHash, AT_KEYEXCHANGE, 0, 0, dbuf.Buf(), &sigLen)) {
				sigLen = 0;
			}
		}
		::CryptDestroyHash(hHash);
	}
	if (!sigLen) return FALSE;

	swap_s(dbuf, sigLen);
	ipdict->put_bytes(IPMSG_SIGN_KEY, dbuf, sigLen);

	if (sigLen != RSA2048_SIGN_SIZE) {
		Debug("Illegal siglen=%d\n", sigLen);
	}

	return	TRUE;
}

BOOL MsgMng::EncIPDict(IPDict *ipdict, PubKey *pub, DynBuf *dbuf)
{
	IPDict	out_dict;

	if (!EncIPDict(ipdict, pub, &out_dict)) {
		return	FALSE;
	}
	if (!dbuf->Alloc(out_dict.pack_size())) {
		return	FALSE;
	}

	size_t	size = out_dict.pack(dbuf->Buf(), dbuf->Size());
	dbuf->SetUsedSize(size);

	return	size > 0 ? TRUE : FALSE;
}

BOOL MsgMng::EncIPDict(IPDict *ipdict, PubKey *pub, IPDict *out_dict)
{
	if (!SignIPDict(ipdict)) {
		return FALSE;
	}

	DynBuf	dbuf(MAX_UDPBUF);
	size_t	dsize = ipdict->pack(dbuf.Buf(), dbuf.Size());
	dbuf.SetUsedSize(dsize);

	if (dsize == 0) {
		return FALSE;
	}

	// データの共通鍵暗号化（AES256 / CTR）
	const DWORD	key_len = 256 / 8;
	BYTE	key[RSA2048_KEY_SIZE]; // enc後のサイズ
	BYTE	iv[AES_BLOCK_SIZE];

	TGenRandom(key, key_len);
	TGenRandom(iv, sizeof(iv));

	DynBuf	enc(dsize);
	AES		aes(key, key_len, iv);
	aes.EncryptCTR(dbuf.Buf(), enc.Buf(), dsize);

	// 送信先公開鍵の import
	int		blob_len = 0;
	BYTE	blob[MAX_BUF];
	pub->KeyBlob(blob, MAX_BUF, &blob_len);

	HCRYPTKEY	hExKey = 0;
	if (!::CryptImportKey(cfg->priv[KEY_2048].hPubCsp, blob, blob_len, 0, 0, &hExKey)) {
		return FALSE;
	}

	// 共通鍵の暗号化＆セット
	DWORD	enc_keylen = key_len;
	if (!::CryptEncrypt(hExKey, 0, TRUE, 0, key, &enc_keylen, sizeof(key))) {
		::CryptDestroyKey(hExKey);
		return FALSE;
	}
	swap_s(key, enc_keylen);

	// wdict へのセット
	int64	flags = IPMSG_RSA_2048|IPMSG_AES_256|IPMSG_IPDICT_CTR;
	out_dict->put_int(IPMSG_ENCFLAG_KEY, flags);
	out_dict->put_bytes(IPMSG_ENCIV_KEY, iv, sizeof(iv));
	out_dict->put_bytes(IPMSG_ENCKEY_KEY, key, enc_keylen);
	out_dict->put_bytes(IPMSG_ENCBODY_KEY, enc.Buf(), dsize);

	::CryptDestroyKey(hExKey);

	return	TRUE;
}

size_t MsgMng::DecIPDict(BYTE *data, size_t size, IPDict *dict)
{
	IPDict	wdict;

	size_t	dsize = wdict.unpack(data, size);

	if (dsize == 0) {
		return	0;
	}

	if (!DecIPDict(&wdict, dict)) {
		return	0;
	}

	return	dsize;
}

BOOL MsgMng::DecIPDict(IPDict *wdict, IPDict *dict)
{
	DynBuf	iv;
	DynBuf	key;
	DynBuf	enc_body;

	if (!wdict->get_bytes(IPMSG_ENCIV_KEY, &iv) || iv.UsedSize() != AES_BLOCK_SIZE) return FALSE;
	if (!wdict->get_bytes(IPMSG_ENCKEY_KEY, &key)) return FALSE;
	if (!wdict->get_bytes(IPMSG_ENCBODY_KEY, &enc_body)) return FALSE;

	swap_s(key.Buf(), key.UsedSize());

	// 共通鍵の暗号化＆セット
	DWORD	key_len = (DWORD)key.UsedSize();
	if (!::CryptDecrypt(cfg->priv[KEY_2048].hKey, 0, TRUE, 0, key.Buf(), &key_len)) {
		return FALSE;
	}

	DynBuf	body(enc_body.UsedSize());
	AES		aes(key.Buf(), key_len, iv.Buf());
	aes.DecryptCTR(enc_body.Buf(), body.Buf(), enc_body.UsedSize());

	if (dict->unpack(body.Buf(), enc_body.UsedSize()) != enc_body.UsedSize()) {
		return	FALSE;
	}
	return	TRUE;
}

ULONG MsgMng::MakeMsg(char *buf, ULONG _packetNo, ULONG command, const char *msg,
	const char *exMsg, const IPDict *ipdict, int *packet_len)
{
	int			cmd = GET_MODE(command);
	BOOL		is_br_cmd =	cmd == IPMSG_BR_ENTRY ||
							cmd == IPMSG_BR_EXIT  ||
							cmd == IPMSG_BR_NOTIFY ||
							cmd == IPMSG_NOOPERATION ? TRUE : FALSE;
	BOOL		is_utf8		= (command & IPMSG_UTF8OPT);
	HostSub		*host		= is_utf8 ? &local : &localA;
	MBCSstr		msg_a;
	MBCSstr		exmsg_a;
	const char	*msg_org	= msg;
	const char	*exMsg_org	= exMsg;
	int			len			= 0;
	int			ex_len		= 0;
	int			&pkt_len	= packet_len ? *packet_len : len;
	int			max_len		= MAX_UDPBUF;

	if (!is_utf8) {	// MBCS
		if (msg) {
			msg_a.Init(msg, BY_UTF8);
			msg = msg_a.s();
		}
		if (exMsg) {
			exmsg_a.Init(exMsg, BY_UTF8);
			exMsg = exmsg_a.s();
		}
	}

	pkt_len = sprintf(buf, "%d:%u:%s:%s:%u:", IPMSG_VERSION, _packetNo,
							host->u.userName, host->u.hostName, command);

	if (exMsg) {
		ex_len = (int)strlen(exMsg);
	}

	if (ex_len + pkt_len + 2 >= MAX_UDPBUF)
		ex_len = 0;
	max_len -= ex_len;

	if (msg) {
		pkt_len += LocalNewLineToUnix(msg, buf + pkt_len, max_len - pkt_len);
	}

	pkt_len++;

	if (ex_len) {
		memcpy(buf + pkt_len, exMsg, ex_len);
		pkt_len += ex_len;
	}

	if (is_br_cmd) {
		buf[pkt_len++] = 0;
		pkt_len += sprintf(buf + pkt_len, "\nUN:%s\nHN:%s\nNN:%s\nGN:%s\nVS:%s",
						local.u.userName, local.u.hostName, msg_org, exMsg_org, GetVerHexInfo());
		if (ipdict) {
			DynBuf	dbuf(MAX_UDPBUF);
			dbuf.SetUsedSize(ipdict->pack(dbuf.Buf(), dbuf.Size()));
			if (pkt_len + (dbuf.UsedSize() * 4 / 3) + 10 < MAX_UDPBUF) {
				pkt_len += strcpyz(buf + pkt_len, "\nIP:");
				pkt_len += bin2b64str(dbuf.Buf(), dbuf.UsedSize(), buf + pkt_len);
			}
			else {
				Debug("Too big ipdict size=%d + %zd\n", pkt_len, dbuf.UsedSize());
			}
		}
	}
	else if (cmd == IPMSG_ANSENTRY || cmd == IPMSG_DIR_POLL) {
		buf[pkt_len++] = 0;
		pkt_len += sprintf(buf + pkt_len, "\nVS:%s", GetVerHexInfo());
	}

	return	_packetNo;
}

BOOL MsgMng::GetPubKey(HostSub *hostSub, BOOL isAuto)
{
	char	buf[128];

	sprintf(buf, "%x", cfg->GetCapa());
	Debug("GetPubKey(%s)\n", hostSub->addr.S());
	return	Send(hostSub, IPMSG_GETPUBKEY|(isAuto ? IPMSG_AUTORETOPT :  0), buf);
}

int MsgMng::GetEncryptMaxMsgLen()
{
	int		len = MAX_UDPBUF - MAX_BUF - 3; // ulen/sharelen
	double	coef = 4.0/3.0;

	len -= (int)((256 * coef + 3) * 2);
	len = (int)(len / coef);

	return len;
}

BOOL MsgMng::MakeEncryptMsg(Host *host, ULONG packet_no, char *msgstr, bool is_extenc,
	char *share, char *ulist_str, char *buf)
{
	HCRYPTKEY	hExKey = 0, hKey = 0;
	BYTE		skey[MAX_BUF];
	BYTE		sign[MAX_BUF];
	DWORD		sigLen = 0;
	DynBuf		data(MAX_UDPBUF);
	DynBuf		msg_data(MAX_UDPBUF);
	BYTE		iv[256/8];
	int			len = 0;
	int			sharelen = 0;
	int			ulistlen = 0;
	int			capa = host->pubKey.Capa() & cfg->GetCapa();
	int			kt = (capa & IPMSG_RSA_2048) ? KEY_2048 : 
					 (capa & IPMSG_RSA_1024) ? KEY_1024 : -1;
	if (kt == -1) return FALSE;

	HCRYPTPROV	enc_csp  = cfg->priv[kt].hPubCsp;
	HCRYPTPROV	sign_csp = cfg->priv[kt].hCsp;
	DWORD 		msgLen, encMsgLen;
	int			max_body_size = MAX_UDPBUF - MAX_BUF;
	double		stringify_coef = 2.0; // hex encoding (default)
	int			(*bin2str_revendian)(const BYTE *, size_t, char *) = bin2hexstr_revendian;
	int			(*bin2str)(const BYTE *, size_t, char *)           = bin2hexstr;
	BOOL		is_mbcs = (host->hostStatus & IPMSG_CAPUTF8OPT) ? FALSE : TRUE;
	MBCSstr		msg_a;
	MBCSstr		share_a;

	if (is_mbcs) {
		msg_a.Init(msgstr, BY_UTF8);
		msgstr = msg_a.Buf();
		if (share) {
			share_a.Init(share, BY_UTF8);
			share = share_a.Buf();
		}
	}

	// すべてのマトリクスはサポートしない（特定の組み合わせのみサポート）
	if ((capa & IPMSG_RSA_2048) && (capa & IPMSG_AES_256) && (capa & IPMSG_SIGN_SHA256)) {
		capa &= IPMSG_RSA_2048 | IPMSG_AES_256 | IPMSG_SIGN_SHA256 | IPMSG_PACKETNO_IV |
				IPMSG_ENCODE_BASE64;
	}
	else if ((capa & IPMSG_RSA_2048) && (capa & IPMSG_AES_256)) {
		capa &= IPMSG_RSA_2048 | IPMSG_AES_256 | IPMSG_SIGN_SHA1 | IPMSG_PACKETNO_IV |
				IPMSG_ENCODE_BASE64;
	}
	else if ((capa & IPMSG_RSA_1024) && (capa & IPMSG_BLOWFISH_128)) {
		capa &= IPMSG_RSA_1024 | IPMSG_BLOWFISH_128 | IPMSG_PACKETNO_IV | IPMSG_ENCODE_BASE64;
	}
	else return	FALSE;

	if (capa & IPMSG_ENCODE_BASE64) {	// change to base64 encoding
#pragma warning ( disable : 4056 )
		stringify_coef = 4.0 / 3.0;
		bin2str_revendian = bin2b64str_revendian;
		bin2str = bin2b64str;
	}

	if (share) {
		sharelen = (int)strlen(share) + 1;
		max_body_size -= sharelen;
	}
	if (ulist_str) {
		ulistlen = (int)strlen(ulist_str) + 1;
		max_body_size -= ulistlen;
	}
	max_body_size -= (int)((host->pubKey.KeyLen() * stringify_coef + 3) *
							((capa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256)) ? 2 : 1));
	max_body_size = (int)(max_body_size / stringify_coef);

	TruncateMsg(msgstr, is_mbcs, max_body_size);

	// KeyBlob 作成
	host->pubKey.KeyBlob(data, (int)data.Size(), &len);

	// 送信先公開鍵の import
	if (!::CryptImportKey(enc_csp, data, len, 0, 0, &hExKey)) {

#ifdef IPMSG_PRO
#define MSG_MASTERPUB
#include "miscext.dat"
#undef  MSG_MASTERPUB
#else
		host->pubKey.UnSet();
#endif
		return GetLastErrorMsg("CryptImportKey"), FALSE;
	}

	// IV の初期化
	memset(iv, 0, sizeof(iv));
	if (capa & IPMSG_PACKETNO_IV) sprintf((char *)iv, "%u", packet_no);

	// UNIX 形式の改行に変換
	msgLen = LocalNewLineToUnix(msgstr, msg_data, max_body_size) + 1;
	if (is_extenc) {
		if (share) {
			memcpy((char *)msg_data + msgLen, share, sharelen);
			msgLen += sharelen;
		}
		if (ulist_str) {
			if (!share) {
				*(((char *)msg_data) + msgLen++) = 0;
			}
			memcpy((char *)msg_data + msgLen, ulist_str, ulistlen);
			msgLen += ulistlen;
		}
	}

	if (capa & IPMSG_AES_256) {	// AES
		// AES 用ランダム鍵作成
		if (!::CryptGenRandom(enc_csp, len = 256/8, data))
			return	GetLastErrorMsg("CryptGenRandom"), FALSE;

		// AES 用共通鍵のセット
		AES		aes(data, len, iv);

		//共通鍵の暗号化
		if (!::CryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF))
			return GetLastErrorMsg("CryptEncrypt"), FALSE;
		bin2str_revendian(data, len, (char *)skey);	// 鍵をhex文字列に

		// メッセージ暗号化
		encMsgLen = (DWORD)aes.EncryptCBC(msg_data, data, msgLen);
	}
	else if (capa & IPMSG_BLOWFISH_128) {	// blowfish
		// blowfish 用ランダム鍵作成
		if (!::CryptGenRandom(enc_csp, len = 128/8, data))
			return	GetLastErrorMsg("CryptGenRandom"), FALSE;

		// blowfish 用共通鍵のセット
		CBlowFish	bl(data, len);

		//共通鍵の暗号化
		if (!::CryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF))
			return GetLastErrorMsg("CryptEncrypt"), FALSE;
		bin2str_revendian(data, len, (char *)skey);	// 鍵をhex文字列に

		// メッセージ暗号化
		encMsgLen = bl.Encrypt(msg_data, data, msgLen, BF_CBC|BF_PKCS5, iv);
	}
	::CryptDestroyKey(hExKey);

	// 電子署名の作成
	if ((capa & IPMSG_SIGN_SHA1) || (capa & IPMSG_SIGN_SHA256)) {
		HCRYPTHASH	hHash;
		DWORD		calg = (capa & IPMSG_SIGN_SHA256) ? CALG_SHA_256 : CALG_SHA;
		if (::CryptCreateHash(sign_csp, calg, 0, 0, &hHash)) {
			if (::CryptHashData(hHash, msg_data, msgLen, 0)) {
				if (::CryptSignHashW(hHash, AT_KEYEXCHANGE, 0, 0, 0, &sigLen)) {
					if (!::CryptSignHashW(hHash, AT_KEYEXCHANGE, 0, 0, sign, &sigLen)) {
						sigLen = 0;
					}
				}
			}
			::CryptDestroyHash(hHash);
		}
		if (sigLen == 0) {
			capa &= ~(IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256);
		}
	}
	len =  sprintf(buf, "%X:%s:", capa, skey);
	len += bin2str(data, (int)encMsgLen, buf + len);

	if (capa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256)) {	// 電子署名の追加
		buf[len++] = ':';
		len += bin2str_revendian(sign, (int)sigLen, buf + len);
	}

	return TRUE;
}

BOOL MsgMng::GetWrappedMsg(MsgBuf *msg, MsgBuf *wmsg)
{
	if (msg->wrdict.key_num() <= 0) {
		return	FALSE;
	}
	wmsg->ipdict = msg->wrdict;
	if (!ResolveDictMsg(wmsg)) {
		return	FALSE;
	}

	return	TRUE;
}

BOOL MsgMng::ResolveMsg(RecvBuf *buf, MsgBuf *msg)
{
	msg->hostSub.addr   = buf->addr;
	msg->hostSub.portNo = buf->port;

	if (!ResolveMsg(buf->msgBuf, buf->size, msg)) return	FALSE;

	return	TRUE;
}

BOOL MsgMng::ResolveDictMsg(MsgBuf *msg)
{
	int64	val;
	U8str	u8;

	IPDict *ipdict = &msg->ipdict;
	msg->orgdict = msg->ipdict;

	if (ipdict->has_key(IPMSG_ENCBODY_KEY)) {
		IPDict	edict = *ipdict;
		ipdict->clear();
		if (!DecIPDict(&edict, ipdict)) {
			msg->decMode = MsgBuf::DEC_NG;
			return FALSE;
		}
		msg->decMode = MsgBuf::DEC_OK;
	}

	msg->timestamp = time(NULL);

	if (!ipdict->get_int(IPMSG_VER_KEY, &val) || val != IPMSG_NEW_VERSION) return FALSE;
	msg->version = (int)val;

	if (!ipdict->get_int(IPMSG_PKTNO_KEY, &val)) return	FALSE;
	msg->packetNo = (ULONG)val;
	snprintfz(msg->packetNoStr, sizeof(msg->packetNoStr), "%d", msg->packetNo); // for IV

	if (!ipdict->get_str(IPMSG_UID_KEY, &u8)) return FALSE;
	msg->hostSub.u.SetUserName(u8.s());

	if (!ipdict->get_str(IPMSG_HOST_KEY, &u8)) return FALSE;
	msg->hostSub.u.SetHostName(u8.s());

	if (!ipdict->get_int(IPMSG_CMD_KEY, &val)) return FALSE;
	msg->command = (UINT)val;

	if (!ipdict->get_int(IPMSG_FLAGS_KEY, &val)) return FALSE;
	msg->flags = (UINT)val;

	ULONG	hostStatus = 0;
	if (ipdict->get_int(IPMSG_STAT_KEY, &val)) {
		switch (msg->command) {
		case IPMSG_BR_ENTRY:
		case IPMSG_BR_EXIT:
		case IPMSG_ANSENTRY:
		case IPMSG_BR_NOTIFY:
		case IPMSG_DIR_POLL:
			msg->flags |= (UINT)val;
		}
		hostStatus = (ULONG)val;
	}

	msg->command |= msg->flags; // 互換用


	if (ipdict->get_str(IPMSG_BODY_KEY, &u8)) {
		UnixNewLineToLocal(u8.s(), msg->msgBuf, MAX_UDPBUF);
	}

	if (ipdict->get_str(IPMSG_NICK_KEY, &u8)) {
		msg->nick = u8;
	}

	if (ipdict->get_str(IPMSG_GROUP_KEY, &u8)) {
		msg->group = u8;
	}
	if (ipdict->get_str(IPMSG_CLIVER_KEY, &u8)) {
		strncpyz(msg->verInfoRaw, u8.s(), MAX_VERBUF);
	}
	if (ipdict->get_int(IPMSG_POLL_KEY, &val)) {
		msg->pollReq = (int)val;
	}
	if (ipdict->has_key(IPMSG_SIGN_KEY)) {
		if (CheckVerify(msg, ipdict)) {
			if (Host *host = hosts->GetHostByName(&msg->hostSub)) {
				msg->hostSub.addr = host->hostSub.addr;
//				if (msg->nick != host->nickName) {
//					strncpyz(host->nickName, msg->nick.s(), sizeof(host->nickName));
//				}
//				if (msg->group != host->groupName) {
//					strncpyz(host->groupName, msg->group.s(), sizeof(host->groupName));
//				}
//				if (hostStatus && hostStatus != host->hostStatus) {
//					host->hostStatus = hostStatus;
//				}
			}
		}
		else {
			return	FALSE;
		}
	}

	ipdict->get_dict(IPMSG_WRAPPED_KEY, &msg->wrdict);

	TrcU8("ResolveMsgDict: cmd=%x flags=%x\n", GET_MODE(msg->command), msg->flags);

	return	TRUE;
}

BOOL CheckVerifyIPDict(HCRYPTPROV csp, const IPDict *ipdict, PubKey *pub)
{
	DynBuf	dbuf;

	if (!ipdict->get_bytes(IPMSG_SIGN_KEY, &dbuf) || dbuf.UsedSize() != RSA2048_SIGN_SIZE) {
		return	FALSE;
	}

	BYTE	sign[RSA2048_SIGN_SIZE];
	memcpy(sign, dbuf, RSA2048_SIGN_SIZE);
	swap_s(sign, RSA2048_SIGN_SIZE);

	HCRYPTHASH	hHash = NULL;
	HCRYPTKEY	hExKey = 0;
	BYTE		blob[1024];
	int			bloblen;
	BOOL		ret = FALSE;

	size_t		max_packnum = ipdict->key_num() - 1; // 末尾SIGNを除外
	dbuf.Alloc(ipdict->pack_size(max_packnum));
	DWORD	pack_size = (DWORD)ipdict->pack(dbuf.Buf(), dbuf.Size(), max_packnum);
	dbuf.SetUsedSize(pack_size);

	pub->KeyBlob(blob, sizeof(blob), &bloblen);	// KeyBlob 作成/import

	if (!::CryptImportKey(csp, blob, bloblen, 0, 0, &hExKey)) {
		return	ret;
	}
	if (::CryptCreateHash(csp, CALG_SHA_256, 0, 0, &hHash)) {
		if (::CryptHashData(hHash, dbuf, pack_size, 0)) {
			if (::CryptVerifySignature(hHash, sign, RSA2048_SIGN_SIZE, hExKey, 0, 0)) {
				ret = TRUE;
		//		Debug("CryptVerifySignature OK!\n");
			}
			else {
				Debug("CryptVerifySignature err=%x\n", GetLastError());
			}
		}
		::CryptDestroyHash(hHash);
	}
	::CryptDestroyKey(hExKey);

	return	ret;
}


BOOL MsgMng::CheckVerify(MsgBuf *msg, const IPDict *ipdict)
{
	msg->signMode = MsgBuf::SIGN_INIT;

	Host	*p_host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
	Host	*host = p_host ? p_host : hosts ? hosts->GetHostByName(&msg->hostSub) : NULL;

	if (!host || !IsSameHost(&msg->hostSub, &host->hostSub) || host->pubKey.KeyLen() != 256) {
		if (!host) {
			host = new Host;
			MsgToHost(msg, host);
		}
		if (!p_host) {
			cfg->priorityHosts.AddHost(host);	// pubkey 保存用
		}
		if (!DictPubToHost(&msg->ipdict, host)) {
			return FALSE;
		}
	}

	if (CheckVerifyIPDict(cfg->priv[KEY_2048].hPubCsp, ipdict, &host->pubKey)) {
		msg->signMode = MsgBuf::SIGN_OK;
	}
	else {
		msg->signMode = MsgBuf::SIGN_NG;
	}

	return	msg->signMode == MsgBuf::SIGN_OK ? TRUE : FALSE;
}


BOOL MsgMng::ResolveMsg(char *buf, int size, MsgBuf *msg)
{
	if (size <= 0) return FALSE;

	size_t used = msg->ipdict.unpack((BYTE *)buf, size);
	if (used == size) {
		if (!cfg->IPDictEnabled()) {
			return FALSE;
		}
		return	 ResolveDictMsg(msg);
	}
	if (used > 0) {
		Debug("partial resolve(remain=%zd / org=%d)\n", used, size);
		return	FALSE;
	}

	char	*exStr  = NULL;
	char	*ulist = NULL;
	char	*uname = NULL;
	char	*hname = NULL;
	char	*tok = NULL;
	char	*p = NULL;
	int		len = 0;

	len = (int)strlen(buf); // main message

	if (size > len + 1) { // ex message (group name or attached file)
		exStr = buf + len + 1;
		int exLen = (int)strlen(exStr);
		if (size > len + 1 + exLen + 1) { // ulist message (utf8 entry)
			ulist = exStr + exLen + 1;
		}
	}

	if ((tok = sep_tok(buf, ':', &p)) == NULL)
		return	FALSE;
	if ((msg->version = atoi(tok)) != IPMSG_VERSION)
		return	FALSE;

	if ((tok = sep_tok(NULL, ':', &p)) == NULL)
		return	FALSE;
	msg->packetNo = strtoul(tok, 0, 10);
	strncpyz(msg->packetNoStr, tok, sizeof(msg->packetNoStr)); // for IV

	if ((uname = sep_tok(NULL, ':', &p)) == NULL)
		return	FALSE;

	if ((hname = sep_tok(NULL, ':', &p)) == NULL)
		return	FALSE;

	if ((tok = sep_tok(NULL, ':', &p)) == NULL)
		return	FALSE;
	msg->command = atol(tok);

	ULONG	cmd = GET_MODE(msg->command);
	BOOL	is_utf8 = (msg->command & IPMSG_UTF8OPT);

	msg->hostSub.u.SetUserName(is_utf8 ? uname : AtoU8s(uname));
	msg->hostSub.u.SetHostName(is_utf8 ? hname : AtoU8s(hname));

	msg->timestamp = time(NULL);

	int		cnt = 0;
	*msg->msgBuf = 0;
	if ((tok = sep_tok(NULL, 0, &p))) { // 改行をUNIX形式からDOS形式に変換
		if (!is_utf8) {
			tok = AtoU8s(tok);
		}
		UnixNewLineToLocal(tok, msg->msgBuf, MAX_UDPBUF);

		if (cmd == IPMSG_BR_ENTRY || cmd == IPMSG_BR_NOTIFY || cmd == IPMSG_ANSENTRY) {
			msg->nick = msg->msgBuf;
		}
	}

	if (exStr) {
		if (exStr[0] != '\n') {
			if ((msg->command & IPMSG_UTF8OPT) == 0) {
				exStr = AtoU8s(exStr);
			}
			strncpyz(msg->exBuf, exStr, sizeof(msg->exBuf));

			if (cmd == IPMSG_BR_ENTRY || cmd == IPMSG_BR_NOTIFY || cmd == IPMSG_ANSENTRY
			|| cmd == IPMSG_DIR_POLL) {
				msg->group = exStr;
			}
		}
		else if (ulist == NULL) {
			ulist = exStr;
		}

		if (ulist && ulist[0] == '\n' && (msg->command & IPMSG_CAPUTF8OPT)) {
			for (tok=sep_tok(ulist, '\n', &p); tok; tok=sep_tok(NULL, '\n', &p)) {
				if      (strncmp(tok, "UN:", 3) == 0) {
					msg->hostSub.u.SetUserName(tok + 3);
				}
				else if (strncmp(tok, "HN:", 3) == 0) {
					msg->hostSub.u.SetHostName(tok + 3);
				}
				else if (strncmp(tok, "NN:", 3) == 0) {
					if (cmd == IPMSG_BR_ENTRY || cmd == IPMSG_BR_NOTIFY || cmd == IPMSG_DIR_POLL) {
						msg->nick = tok+3;
					}
				}
				else if (strncmp(tok, "GN:", 3) == 0) {
					if (cmd == IPMSG_BR_ENTRY || cmd == IPMSG_BR_NOTIFY || cmd == IPMSG_DIR_POLL) {
						msg->group = tok+3;
					}
				}
				else if (strncmp(tok, "VS:", 3) == 0) {
					strncpyz(msg->verInfoRaw, tok+3, sizeof(msg->verInfoRaw));
					GetVerInfoByHex(tok+3, &msg->verInfo);
				}
				else if (strncmp(tok, "PL:", 3) == 0) {
					msg->pollReq = atoi(tok+3);
				}
				else if (strncmp(tok, "IP:",3) == 0) {
					DynBuf	dbuf(strlen(tok+3));
					size_t	dsize = b64str2bin(tok+3, dbuf.Buf(), dbuf.Size());
					if (dsize == 0) {
						return FALSE;
					}
					if (msg->ipdict.unpack(dbuf.Buf(), dsize) != dsize) {
						return FALSE;
					}
				}
			}
			if (msg->ipdict.key_num()) {
				ResolveDictMsg(msg);
			}
		}
	}
	TrcU8("ResolveMsg: cmd=%x flags=%x\n", GET_MODE(msg->command), GET_OPT(msg->command));

	return	TRUE;
}

/*
	メッセージの復号化
*/
BOOL MsgMng::DecryptMsg(MsgBuf *_msg, UINT *_cryptCapa, UINT *_logOpt)
{
	MsgBuf		&msg = *_msg;
	UINT		&cryptCapa = *_cryptCapa;
	UINT		&logOpt = *_logOpt;
	HCRYPTKEY	hKey=0, hExKey=0;
	char		*capa_hex = NULL;
	char		*skey_hex = NULL;
	char		*msg_hex = NULL;
	char		*hash_hex = NULL;
	char		*p;
	BYTE		skey[MAX_BUF];
	BYTE		iv[128/8];
	int			len, msgLen, encMsgLen;
	HCRYPTPROV	dec_csp;
	// hex encoding
	size_t		(*str2bin_revendian)(const char *, BYTE *, size_t) = hexstr2bin_revendian;
	size_t		(*str2bin)(const char *, BYTE *, size_t)           = hexstr2bin;
	DynBuf		tmpBuf(MAX_UDPBUF);

	if ((capa_hex = sep_tok(msg.msgBuf, ':', &p)) == NULL) {
		return	FALSE;
	}
	cryptCapa = strtoul(capa_hex, 0, 16);

	int	kt = (cryptCapa & IPMSG_RSA_2048) ? KEY_2048 :
			 (cryptCapa & IPMSG_RSA_1024) ? KEY_1024 : -1;
	if (kt == -1) return FALSE;

	dec_csp		= cfg->priv[kt].hCsp;
	hExKey		= cfg->priv[kt].hKey;

	if (cryptCapa & IPMSG_ENCODE_BASE64) { // change to base64 encoding
		str2bin_revendian = b64str2bin_revendian;
		str2bin           = b64str2bin;
	}

	if (cryptCapa & IPMSG_COMMON_KEYS) {
		if ((skey_hex = sep_tok(NULL, ':', &p)) == NULL) {
			return	FALSE;
		}
	}
	else {
		return	FALSE;
	}

	if ((msg_hex = sep_tok(NULL, ':', &p)) == NULL) {
		return	FALSE;
	}

	if (cryptCapa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256)) {
		if ((hash_hex = sep_tok(NULL, ':', &p)) == NULL)
			return	FALSE;
	}

	// IV の初期化
	memset(iv, 0, sizeof(iv));
	if (cryptCapa & IPMSG_PACKETNO_IV) strncpyz((char *)iv, msg.packetNoStr, sizeof(iv));

	if (cryptCapa & IPMSG_AES_256) {	// AES
		len = (int)str2bin_revendian(skey_hex, skey, sizeof(skey));
		// 公開鍵取得
		if (!::CryptDecrypt(hExKey, 0, TRUE, 0, (BYTE *)skey, (DWORD *)&len))
			return	sprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;

		AES		aes(skey, len, iv);
		encMsgLen = (int)str2bin(msg_hex, tmpBuf, tmpBuf.Size());
		msgLen = (DWORD)aes.DecryptCBC(tmpBuf, tmpBuf, encMsgLen);
	}
	else if (cryptCapa & IPMSG_BLOWFISH_128) {	// blowfish
		len = (int)str2bin_revendian(skey_hex, skey, sizeof(skey));
		// 公開鍵取得
		if (!::CryptDecrypt(hExKey, 0, TRUE, 0, (BYTE *)skey, (DWORD *)&len))
			return	sprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;

		CBlowFish	bl(skey, len);
		encMsgLen = (int)str2bin(msg_hex, tmpBuf, tmpBuf.Size());
		msgLen = bl.Decrypt(tmpBuf, tmpBuf, encMsgLen, BF_CBC|BF_PKCS5, iv);
	}
	else {
		return	FALSE;
	}

	// 電子署名の検証
	if (cryptCapa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256)) {
		Host	*host = hosts ? hosts->GetHostByName(&msg.hostSub) : NULL;

		if (!host) host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
		if (host && IsSameHost(&msg.hostSub, &host->hostSub) && host->pubKey.KeyLen() == 256) {
			HCRYPTHASH	hHash = NULL;
			HCRYPTKEY	hExKey = 0;
			BYTE		data[MAX_BUF];
			int			capa = host->pubKey.Capa();
			int			kt = (capa & IPMSG_RSA_2048) ? KEY_2048 : KEY_1024;

			logOpt |= LOG_SIGN_ERR;
			HCRYPTPROV	verify_csp	= cfg->priv[kt].hPubCsp;
			host->pubKey.KeyBlob(data, sizeof(data), &len);	// KeyBlob 作成/import
			if (::CryptImportKey(verify_csp, data, len, 0, 0, &hExKey)) {
				DWORD	calg = (cryptCapa & IPMSG_SIGN_SHA256) ? CALG_SHA_256 : CALG_SHA;
				if (::CryptCreateHash(verify_csp, calg, 0, 0, &hHash)) {
					if (::CryptHashData(hHash, tmpBuf, msgLen, 0)) {
						int		sigLen = 0;
						BYTE	sig_data[MAX_BUF];
						sigLen = (int)str2bin_revendian(hash_hex, sig_data, sizeof(sig_data));
						if (::CryptVerifySignature(hHash, sig_data, sigLen, hExKey, 0, 0)) {
							logOpt &= ~LOG_SIGN_ERR;
							logOpt |= (cryptCapa & IPMSG_SIGN_SHA256) ? LOG_SIGN2_OK : LOG_SIGN_OK;
						}
					}
					::CryptDestroyHash(hHash);
				}
				::CryptDestroyKey(hExKey);
			}
		}
		else {
			logOpt |= LOG_SIGN_NOKEY;
		}
	}
	logOpt |= (cryptCapa & IPMSG_RSA_2048) ? LOG_ENC2 :
			  (cryptCapa & IPMSG_RSA_1024) ? LOG_ENC1 : LOG_ENC0;

	// 暗号化添付メッセージ
	if ((msg.command & IPMSG_ENCEXTMSGOPT)) {
		if ((len = (int)strlen(tmpBuf) + 1) < msgLen) {
			len += strncpyz(msg.exBuf, (char *)tmpBuf + len, MAX_UDPBUF) + 1;
			if (len < msgLen) {
				strncpyz(msg.uList, (char *)tmpBuf + len, MAX_ULISTBUF);
			}
		}
	}

	TruncateMsg(tmpBuf, (msg.command & IPMSG_UTF8OPT) == 0, MAX_UDPBUF);

	// UNIX 形式の改行を変換
	UnixNewLineToLocal(tmpBuf, msg.msgBuf, sizeof(msg.msgBuf));

	if ((msg.command & IPMSG_UTF8OPT) == 0) {
		strncpyz(msg.msgBuf, AtoU8s(msg.msgBuf), sizeof(msg.msgBuf));
	}

	return	TRUE;
}

ULONG MsgMng::MakePacketNo(void)
{
	ULONG now = (ULONG)time(NULL);

	if (now > packetNo) {
		packetNo = now;
	}

	return packetNo++;
}


BOOL MsgMng::UdpSend(Addr host_addr, int port_no, const char *buf)
{
	return	UdpSend(host_addr, port_no, buf, (int)strlen(buf) +1);
}

BOOL MsgMng::UdpSend(Addr host_addr, int port_no, const char *buf, int len)
{
#ifdef IPMSG_PRO
#define MSG_UDPSEND
#include "miscext.dat"
#undef  MSG_UDPSEND
#endif

	return	UdpSendCore(host_addr, port_no, buf, len);
}

/* パケット大量送信等の異常動作時に強制停止 */
/* （将来、受信側も宛先毎のフィルタを入れてもよいかも） */
BOOL CheckWatchDog()
{
	static DWORD	watchDog;
	static DWORD	watchCnt;
	static BOOL		isBlocked = FALSE;

	if (isBlocked) {
		return	FALSE;
	}
	if ((watchCnt++ % 100)) {
		return	TRUE;
	}

	DWORD cur = ::GetTick();
	if (watchDog == 0 || (cur - watchDog) > 10000) {
		watchDog = cur;
		watchCnt = 0;
		return	TRUE;
	}
	if (watchCnt >= 10000) {
		isBlocked = TRUE;
		::MessageBox(0, Fmt("Too many sendto. Exit process...%u", cur - watchDog), IP_MSG, MB_OK);
		::ExitProcess(-1);
		return	FALSE;
	}
	return	TRUE;
}

BOOL MsgMng::UdpSendCore(Addr host_addr, int port_no, const char *buf, int len)
{
//	if (!CheckWatchDog()) {
//		return	FALSE;
//	}

	if (isV6 && host_addr.IsIPv4()) {
		host_addr.ChangeMappedIPv6();
	}

	sockaddr_in6	addr6;
	sockaddr_in		addr4;
	sockaddr		*addr = host_addr.size == 16 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
	int				size  = host_addr.size == 16 ? sizeof(addr6)      : sizeof(addr4);

	if (host_addr.size != 16 && host_addr.size != 4) {
		return FALSE;
	}

	if (host_addr.size == 16) {
		memset(&addr6, 0, sizeof(addr6));
		addr6.sin6_family	= AF_INET6;
		addr6.sin6_port		= ::htons(port_no);
		host_addr.Put(&addr6.sin6_addr, 16);
	}
	else {
		memset(&addr4, 0, sizeof(addr4));
		addr4.sin_family	= AF_INET;
		addr4.sin_port		= ::htons(port_no);
		host_addr.Put(&addr4.sin_addr, 4);
	}

	TrcU8("sendto: addr=%s size=%d\n", host_addr.S(), len);
	if (::sendto(udp_sd, buf, len, 0, addr, size) == SOCKET_ERROR)
	{
		int	ret = ::WSAGetLastError();
		switch (ret) {
		case WSAENETDOWN:
			break;
		case WSAEHOSTUNREACH:
			static	BOOL	done;
			if (!done) {
				done = TRUE;
//				MessageBox(0, LoadStr(IDS_HOSTUNREACH), ::inet_ntoa(*(LPIN_ADDR)&host_addr), MB_OK);
			}
			return	FALSE;
		default:
			Debug("sendto(%s) error=%d\n", host_addr.S(), ret);
			return	FALSE;
		}

		if (!WSockReset()) return FALSE;

		if (hAsyncWnd && !AsyncSelectRegister(hAsyncWnd)) return FALSE;

		if (::sendto(udp_sd, buf, len, 0, addr, size) == SOCKET_ERROR) return FALSE;
	}

	return	TRUE;
}


BOOL MsgMng::UdpRecv(RecvBuf *buf)
{
	sockaddr_in6	addr6;
	sockaddr_in		addr4;
	sockaddr		*addr = isV6 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
	int				size  = isV6 ? sizeof(addr6) : sizeof(addr4);

	buf->size = ::recvfrom(udp_sd, buf->msgBuf, MAX_UDPBUF -1, 0, addr, &size);
	if (buf->size == SOCKET_ERROR) return	FALSE;
	buf->msgBuf[buf->size] = 0;

	if (size == sizeof(addr6)) {
		buf->addr.Set(&addr6.sin6_addr, 16);
		buf->port = ::htons(addr6.sin6_port);
	}
	else if (size == sizeof(addr4)) {
		buf->addr.Set(&addr4.sin_addr, 4);
		buf->port = ::htons(addr4.sin_port);
	}
	else return FALSE;

	TrcU8("recvfrom: addr=%s size=%d\n", buf->addr.S(), buf->size);

	return	TRUE;
}

BOOL MsgMng::Accept(HWND hWnd, ConnectInfo *info)
{
	sockaddr_in6	addr6;
	sockaddr_in		addr4;
	sockaddr		*addr = isV6 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
	int				size  = isV6 ? sizeof(addr6) : sizeof(addr4);
	int				flg=TRUE;

	if ((info->sd = ::accept(tcp_sd, addr, &size)) == INVALID_SOCKET) return FALSE;

	::setsockopt(info->sd, SOL_SOCKET, TCP_NODELAY, (char *)&flg, sizeof(flg));

	if (isV6) {
		info->addr.Set(&addr6.sin6_addr, 16);
		info->port = ::htons(addr6.sin6_port);
	}
	else {
		info->addr.Set(&addr4.sin_addr, 4);
		info->port = ::htons(addr4.sin_port);
	}
	info->server = info->complete = TRUE;

	TrcU8("accept: addr=%s\n", info->addr.S());

	for (int buf_size=cfg->TcpbufMax; buf_size > 0; buf_size /= 2) {
		if (::setsockopt(info->sd, SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size)) == 0)
			break;
	}

	if (AsyncSelectConnect(hWnd, info)) {
		info->startTick = info->lastTick = ::GetTick();
		return	TRUE;
	}

	::closesocket(info->sd);
	info->sd = INVALID_SOCKET;
	return	FALSE;
}

BOOL MsgMng::Connect(HWND hWnd, ConnectInfo *info)
{
	BOOL flg = FALSE;

	info->server = FALSE;

	if ((info->sd = ::socket(isV6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		goto ERR;
	}

	flg = FALSE;
	if (isV6 && ::setsockopt(info->sd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&flg, sizeof(flg))) {
		GetSockErrorMsg("setsockopt3(IPV6_V6ONLY)");
		goto ERR;
	}

	flg = TRUE;	// Non Block
	if (hWnd) {
		if (::ioctlsocket(info->sd, FIONBIO, (unsigned long *)&flg)) {
			goto ERR;
		}
	}

	::setsockopt(info->sd, SOL_SOCKET, TCP_NODELAY, (char *)&flg, sizeof(flg));

	if (cfg && info->growSbuf) {
		for (int buf_size = cfg->TcpbufMax; buf_size > 0; buf_size /= 2) {
			if (::setsockopt(info->sd, SOL_SOCKET, SO_RCVBUF, (char*)&buf_size,
			 sizeof(buf_size)) == 0) {
				break;
			}
		}
	}

	if (!hWnd || AsyncSelectConnect(hWnd, info)) {
		sockaddr_in6	addr6;
		sockaddr_in		addr4;
		sockaddr		*addr = isV6 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
		int				size =  isV6 ? sizeof(addr6) : sizeof(addr4);
		memset(addr, 0, size);

		if (isV6) {
			addr6.sin6_family	= AF_INET6;
			addr6.sin6_port		= ::htons(info->port);
			info->addr.Put(&addr6.sin6_addr, 16);
		}
		else {
			addr4.sin_family	= AF_INET;
			addr4.sin_port		= ::htons(info->port);
			info->addr.Put(&addr4.sin_addr, 4);
		}

		info->complete = ::connect(info->sd, (LPSOCKADDR)addr, size) != -1;

		TrcU8("connect: addr=%s complete=%d\n", info->addr.S(), info->complete);

		if (info->complete || ::WSAGetLastError() == WSAEWOULDBLOCK) {
			info->startTick = info->lastTick = ::GetTick();
			return	TRUE;
		}
	}

ERR:
	::closesocket(info->sd);
	info->sd = INVALID_SOCKET;
	return	FALSE;
}

BOOL MsgMng::AsyncSelectConnect(HWND hWnd, ConnectInfo *info)
{
	if (::WSAAsyncSelect(info->sd, hWnd, info->uMsg,
			  (info->server ? FD_READ : FD_CONNECT)|FD_CLOSE|info->addEvFlags) == SOCKET_ERROR) {
		return	FALSE;
	}
	return	TRUE;
}

/*
	非同期系の抑制
*/
BOOL MsgMng::ConnectDone(HWND hWnd, ConnectInfo *info)
{
	::WSAAsyncSelect(info->sd, hWnd, 0, 0);	// 非同期メッセージの抑制
	BOOL	flg = FALSE;
	::ioctlsocket(info->sd, FIONBIO, (unsigned long *)&flg);
	return	TRUE;
}

void GetIPAddrsGw(IP_ADAPTER_GATEWAY_ADDRESS *first_gw, AddrInfo *ai)
{
	for (auto gw=first_gw; gw; gw = gw->Next) {
		Addr	addr;
//		if (gw->Address.lpSockaddr->sa_family == AF_INET6) {
//			sockaddr_in6	*sin6 = (sockaddr_in6  *)gw->Address.lpSockaddr;
//			addr.Set(&sin6->sin6_addr, 16);
//			Debug("addr6(gw)=%s/%d\n", addr.S(), addr.mask);
//		}
//		else
		if (gw->Address.lpSockaddr->sa_family == AF_INET) {
			sockaddr_in		*sin4 = (sockaddr_in  *)gw->Address.lpSockaddr;
			addr.Set(&sin4->sin_addr, 4);
			Debug("addr4(gw)=%s/%d\n", addr.S(), addr.mask);
		}
		if (addr.IsEnabled()) {
			ai->gw.push_back(addr);
		}
	}
}

AddrInfo *GetIPAddrs(GetIPAddrsMode mode, int *num, int ipv6mode)
{
	DWORD		size = 0;
	AddrInfo	*ret = NULL;
	int			&di = *num;
	ULONG		family = ipv6mode == 0 ? AF_INET : ipv6mode == 1 ? AF_INET6 : AF_UNSPEC;
	DWORD		flags = GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_SKIP_FRIENDLY_NAME
		|GAA_FLAG_INCLUDE_PREFIX|GAA_FLAG_INCLUDE_GATEWAYS;
	IP_ADAPTER_ADDRESSES *adr;

#if 0
	Addr	addr("2404:6800:4004:818::200e");
	SOCKADDR_INET	in = {};
	SOCKADDR_INET	out = {};
	MIB_IPFORWARD_ROW2 mib = {};
	in.Ipv6.sin6_family = AF_INET6;
	in.Ipv6.sin6_addr = addr.in6;
	NETIOAPI_API r = ::GetBestRoute2(NULL, 0, 0, &in, 0, &mib, &out);
	addr.Set((void *)&out.Ipv6.sin6_addr, 16);
	Debug("r=%d %s\n", ret, addr.S());
#endif

#define MAX_ADDRS	100

	di = 0;

	if (::GetAdaptersAddresses(family, flags, 0, 0, &size) != ERROR_BUFFER_OVERFLOW) return NULL;

	DynBuf	buf(size);
	adr = (IP_ADAPTER_ADDRESSES *)(void *)buf;
	if (::GetAdaptersAddresses(family, flags, 0, adr, &size) != ERROR_SUCCESS) return NULL;

	ret = new AddrInfo[MAX_ADDRS];

	for ( ; adr && di < MAX_ADDRS; adr = adr->Next) {
		if (adr->OperStatus != IfOperStatusUp ||
			adr->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
		//	adr->IfType == IF_TYPE_TUNNEL ||
			adr->PhysicalAddressLength <= 0) continue;

		int	uni_num = 0;	// 同じadptor上のIP数

		for (auto uni = adr->FirstUnicastAddress; uni; uni = uni->Next) {
			if (/* ipv6mode == 0 &&*/ !(uni->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE)) continue;
			if (uni->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) continue;

			ret[di].type = AddrInfo::UNICAST;
//			ret[di].mask = uni->OnLinkPrefixLength;

			if (uni->Address.lpSockaddr->sa_family == AF_INET6 && ipv6mode > 0) {
				sockaddr_in6	*sin6 = (sockaddr_in6  *)uni->Address.lpSockaddr;
				ret[di].addr.Set(&sin6->sin6_addr, 16, uni->OnLinkPrefixLength);
				Debug("addr6(uni)=%s/%d flg=%d\n", ret[di].addr.S(), ret[di].addr.mask, uni->Flags);
			}
			else if (uni->Address.lpSockaddr->sa_family == AF_INET && ipv6mode != 1) {
				sockaddr_in		*sin4 = (sockaddr_in  *)uni->Address.lpSockaddr;
				ret[di].addr.Set(&sin4->sin_addr, 4, uni->OnLinkPrefixLength);
				Debug("addr4(uni)=%s/%d flg=%d\n", ret[di].addr.S(), ret[di].addr.mask, uni->Flags);

				if (uni_num++ == 0 && IsWinVista()) { // XP has no FirstGatewayAddress member
					GetIPAddrsGw(adr->FirstGatewayAddress, &ret[di]);
				}

				if (mode != GIA_NOBROADCAST) {
					ULONG	br_addr = ret[di].addr.V4Addr() | (0xffffffffUL >> ret[di].addr.mask);
					if (mode == GIA_STRICT && br_addr) {
						for (int i=0; i < di; i++) {
							if (ret[i].addr.V4Addr() == br_addr) {
								br_addr = 0;
								break;
							}
						}
					}
					if (br_addr) {
						di++;
						ret[di].type = AddrInfo::BROADCAST;
						ret[di].addr.SetV4Addr(br_addr, uni->OnLinkPrefixLength);
						Debug("addr4(br)=%s/%d\n", ret[di].addr.S(), ret[di].addr.mask);
					}
				}
			}
			di++;
		}

		if (mode == GIA_NOBROADCAST) continue;

		for (auto mul=adr->FirstMulticastAddress; mul; mul=mul->Next) {
			if (ipv6mode == 0 && !(mul->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE)) continue;
			if (mul->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) continue;

		//	ret[di].mask = 128;
			ret[di].type = AddrInfo::MULTICAST;

			if (mul->Address.lpSockaddr->sa_family == AF_INET6 && ipv6mode > 0) {
				sockaddr_in6	*sin6 = (sockaddr_in6  *)mul->Address.lpSockaddr;
				ret[di].addr.Set(&sin6->sin6_addr, 16, 128);
				Debug("addr6(mul)=%s/%d\n", ret[di].addr.S(), ret[di].addr.mask);
			}
			if (mul->Address.lpSockaddr->sa_family == AF_INET && ipv6mode != 1) {
				sockaddr_in		*sin4 = (sockaddr_in  *)mul->Address.lpSockaddr;
				ret[di].addr.Set(&sin4->sin_addr, 4, 128);
				Debug("addr4(mul)=%s/%d\n", ret[di].addr.S(), ret[di].addr.mask);
			}
			di++;
		}
	}

	return	ret;
}

size_t EncryptDataSize(Cfg *cfg, const vector<PubKey *> &masterPub, size_t in_len)
{
	return	RSA2048_KEY_SIZE * (int)masterPub.size() + ALIGN_SIZE(in_len+1, AES_BLOCK_SIZE);
}

size_t EncryptData(Cfg *cfg, const vector<PubKey *> &masterPub, const BYTE *in, size_t in_len,
	BYTE *out, size_t max_len)
{
	HCRYPTPROV	csp = cfg->priv[KEY_2048].hPubCsp;
	HCRYPTKEY	hExKey = 0;
	size_t		len = 0;
	BYTE		key[MAX_BUF];

	// AES 用ランダム鍵作成
	key[0] = 0; // for code analyze
	if (!::CryptGenRandom(csp, AES256_SIZE, key)) {
		Debug("CryptGenRandom err(%x)\n", GetLastError());
		::CryptDestroyKey(hExKey);
		return	0;
	}

	// AES 用共通鍵のセット
	AES		aes(key, AES256_SIZE);

	for (auto &pub: masterPub) {
		int		blob_len;
		BYTE	blob[MAX_BUF];
		// KeyBlob 作成
		pub->KeyBlob(blob, sizeof(blob), &blob_len);

		// 送信先公開鍵の import
		if (!::CryptImportKey(csp, blob, blob_len, 0, 0, &hExKey)) {
			Debug("CryptImportKey err(%x)\n", GetLastError());
			return	0;
		}

		//共通鍵の暗号化
		memcpy(out + len, key, AES256_SIZE);
		DWORD	enckey_len = AES256_SIZE;

		if (!::CryptEncrypt(hExKey, 0, TRUE, 0, out + len, (DWORD *)&enckey_len, sizeof(key))) {
			Debug("CryptEncrypt err(%x)\n", GetLastError());
			::CryptDestroyKey(hExKey);
			return	0;
		}
		::CryptDestroyKey(hExKey);

		len += enckey_len;
	}

	//rev_order(key, key_len);

	if (in_len + len >= max_len) {
		return	0;
	}

//	Debug("keylen=%d\n", len, max_len);

	len += aes.EncryptCBC(in, out + len, in_len);

//	Debug("len=%d max=%d\n", len, max_len);

	return	len;
}

size_t DecryptData(Cfg *cfg, const BYTE *in, size_t in_len, BYTE *out, size_t max_len)
{
	HCRYPTPROV	csp		= cfg->priv[KEY_2048].hCsp;
	HCRYPTKEY	hExKey	= cfg->priv[KEY_2048].hKey;
	int			key_len = RSA2048_KEY_SIZE;
	BYTE		key[MAX_BUF] = {};

	if (!::CryptDecrypt(hExKey, 0, TRUE, 0, key, (DWORD *)&key_len)) {
		Debug("CryptDecrypt err(%x)\n", GetLastError());
		return	0;
	}

	// AES 用共通鍵のセット
	AES		aes(key, key_len);
	size_t	len = aes.DecryptCBC(in + RSA2048_KEY_SIZE, out, in_len);

	return	len;
}

#define TOLIST_KEY		"TO:"
#define TOLIST_KEYSIZE	(sizeof(TOLIST_KEY) - 1)

void MsgMng::UListToHostSub(MsgBuf *msg, vector<HostSub> *hosts)
{
	char	*ulist = msg->uList;

	hosts->clear();
	if (!*ulist || strncmp(ulist, TOLIST_KEY, TOLIST_KEYSIZE) != 0) {
		return;
	}
	ulist += TOLIST_KEYSIZE;

	if (char *buf = strdup(ulist)) {
		char *p;
		char *tok = sep_tok(buf, HOSTLIST_SEPARATOR, &p);

		for ( ; tok; tok=sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) {
			char *p2;
			if (char *uname = sep_tok(tok, ':', &p2)) {
				if (char *hname = sep_tok(NULL, ':', &p2)) {
					HostSub	hostSub;
					hostSub.SetUserName(uname);
					hostSub.SetHostName(hname);
					if (!IsSameHost(&hostSub, &local) && !IsSameHost(&hostSub, &msg->hostSub)) {
						hosts->push_back(hostSub);
					}
				}
			}
		}
		free(buf);
	}
}

void MsgMng::UListToHostSubDict(MsgBuf *msg, vector<HostSub> *hosts)
{
	hosts->clear();

	IPDictList	dlist;

	if (!msg->ipdict.get_dict_list(IPMSG_TOLIST_KEY, &dlist)) {
		return;
	}
	for (auto &h: dlist) {
		HostSub	hostSub;
		U8str	uid;
		U8str	hid;
		if (h->get_str(IPMSG_UID_KEY, &uid) && h->get_str(IPMSG_HOST_KEY, &hid)) {
			hostSub.SetUserName(uid.s());
			hostSub.SetHostName(hid.s());
			if (!IsSameHost(&hostSub, &local) && !IsSameHost(&hostSub, &msg->hostSub)) {
				hosts->push_back(hostSub);
			}
		}
	}
}

void MsgMng::UserToUList(const vector<User> &uvec, char *ulist)
{
	if (uvec.size() <= 0) {
		*ulist = 0;
		return;
	}
	ulist += strcpyz(ulist, TOLIST_KEY);
	int	len = 0;

	for (auto &u: uvec) {
		if (len + MAX_NAMEBUF * 2 + 3 >= MAX_ULISTBUF) {
			break;
		}
		len += sprintf(ulist+len, "%s:%s%c", u.userName, u.hostName, HOSTLIST_SEPARATOR);
	}
}

void MsgMng::UserToUListDict(const vector<User> &uvec, IPDict *dict)
{
	if (uvec.size() <= 0) {
		return;
	}

	IPDictList	dlist;

	for (auto &u: uvec) {
		auto	h = make_shared<IPDict>();
		h->put_str(IPMSG_UID_KEY,  u.userName);
		h->put_str(IPMSG_HOST_KEY, u.hostName);
		dlist.push_back(h);
	}
	dict->put_dict_list(IPMSG_TOLIST_KEY, dlist);
}

void MsgMng::AddFileShareDict(IPDict *dict, const IPDictList &share_dlist)
{
	if (share_dlist.size() <= 0) {
		return;
	}
	dict->put_dict_list(IPMSG_FILE_KEY, share_dlist);
}


void MsgMng::AddBodyDict(IPDict *dict, const char *msg)
{
	dict->put_str(IPMSG_BODY_KEY, msg);
}


