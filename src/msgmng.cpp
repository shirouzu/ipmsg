static char *msgmng_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2015   msgmng.cpp	Ver3.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Message Manager
	Create					: 1996-06-01(Sat)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "aes.h"
#include "blowfish.h"
#include <iphlpapi.h>

MsgMng::MsgMng(Addr nicAddr, int portNo, Cfg *_cfg, THosts *_hosts)
{
	status = FALSE;
	packetNo = (ULONG)Time();
	udp_sd = tcp_sd = INVALID_SOCKET;
	hAsyncWnd = 0;
	local.addr = nicAddr;
	local.portNo = ::htons(portNo);
	cfg = _cfg;
	hosts = _hosts;
	isV6 = !cfg || cfg->IPv6Mode;

	if (!WSockInit()) return;

	WCHAR	wbuf[128];
	DWORD	size = sizeof(wbuf);
	if (!::GetComputerNameW(wbuf, &size)) {
		GetLastErrorMsg("GetComputerName()");
		return;
	}
	WtoU8(wbuf, local.hostName, sizeof(local.hostName));
	WtoA(wbuf, localA.hostName, sizeof(localA.hostName));

	if (nicAddr.IsEnabled()) {
		char	host[MAX_BUF];
		if (::gethostname(host, sizeof(host)) == -1)
			strcpy(host, local.hostName);

//		hostent	*ent = ::gethostbyname(host);
//		if (ent) {
//			local.addr.Set(ent->h_addr_list[0]);
//		}
	}

	size = sizeof(wbuf);
	if (!::GetUserNameW(wbuf, &size)) {
		GetLastErrorMsg("GetUserName()");
		return;
	}
	WtoU8(wbuf, local.userName, sizeof(local.userName));
	WtoA(wbuf, localA.userName, sizeof(localA.userName));
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
		addr6.sin6_port		= local.portNo;
		if (local.addr.IsEnabled() && local.addr.IsIPv6()) {
			local.addr.Put(&addr6.sin6_addr, 16);
		}
	}
	else {
		addr4.sin_family	= AF_INET;
		addr4.sin_port		= local.portNo;
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

	wsprintf(buf, "%d", val);
	return	Send(hostSub, command, buf);
}

BOOL MsgMng::Send(HostSub *hostSub, ULONG command, const char *message, const char *exMsg)
{
	return	Send(hostSub->addr, hostSub->portNo, command, message, exMsg);
}

BOOL MsgMng::Send(Addr host, int port_no, ULONG command, const char *message, const char *exMsg)
{
	char	buf[MAX_UDPBUF];
	int		trans_len;

	MakeMsg(buf, command, message, exMsg, &trans_len);
	return	UdpSend(host, port_no, buf, trans_len);
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

BOOL MsgMng::Recv(MsgBuf *msg)
{
	RecvBuf		buf;

	if (!UdpRecv(&buf) || buf.size == 0)
		return	FALSE;

	return	ResolveMsg(&buf, msg);
}

ULONG MsgMng::MakeMsg(char *buf, int _packetNo, ULONG command, const char *msg, const char *exMsg, int *packet_len)
{
	int			cmd = GET_MODE(command);
	BOOL		is_br_cmd =	cmd == IPMSG_BR_ENTRY ||
							cmd == IPMSG_BR_EXIT  ||
							cmd == IPMSG_BR_ABSENCE ||
							cmd == IPMSG_NOOPERATION ? TRUE : FALSE;
	BOOL		is_utf8		= (command & IPMSG_UTF8OPT);
	HostSub		*host		= is_utf8 ? &local : &localA;
	char		*out_buf	= NULL;
	char		*out_exbuf	= NULL;
	const char	*msg_org	= msg;
	const char	*exMsg_org	= exMsg;
	int			len			= 0;
	int			ex_len		= 0;
	int			&pkt_len	= packet_len ? *packet_len : len;
	int			max_len		= MAX_UDPBUF;
	BOOL		is_ascii;

	pkt_len = sprintf(buf, "%d:%u:%s:%s:%u:", IPMSG_VERSION, _packetNo,
							host->userName, host->hostName, command);

	if (msg) {
		if (!is_utf8 && IsUTF8(msg, &is_ascii) && !is_ascii) {
			msg = out_buf = U8toA(msg);
		}
	}

	if (exMsg) {
		if (!is_utf8 && IsUTF8(exMsg, &is_ascii) && !is_ascii) {
			exMsg = out_exbuf = U8toA(exMsg);
		}
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
		pkt_len += sprintf(buf + pkt_len, "\nUN:%s\nHN:%s\nNN:%s\nGN:%s",
						local.userName, local.hostName, msg_org, exMsg_org);
	}

	delete [] out_exbuf;
	delete [] out_buf;

	return	_packetNo;
}

BOOL MsgMng::MakeEncryptMsg(Host *host, ULONG packet_no, char *msgstr, bool is_extenc, char *share,
							char *buf)
{
	HCRYPTKEY	hExKey = 0, hKey = 0;
	BYTE		skey[MAX_BUF];
	DynBuf		data(MAX_UDPBUF);
	DynBuf		msg_data(MAX_UDPBUF);
	BYTE		iv[256/8];
	int			len = 0, sharelen = 0;
	int			capa = host->pubKey.Capa() & GetLocalCapa(cfg);
	int			kt = (capa & IPMSG_RSA_2048) ? KEY_2048 :
					 (capa & IPMSG_RSA_1024) ? KEY_1024 : KEY_512;
	HCRYPTPROV	target_csp = cfg->priv[kt].hCsp;
	DWORD 		msgLen, encMsgLen;
	int			max_body_size = MAX_UDPBUF - MAX_BUF;
	double		stringify_coef = 2.0; // hex encoding (default)
	int			(*bin2str_revendian)(const BYTE *, int, char *) = bin2hexstr_revendian;
	int			(*bin2str)(const BYTE *, int, char *)           = bin2hexstr;

	// すべてのマトリクスはサポートしない（特定の組み合わせのみサポート）
	if ((capa & IPMSG_RSA_2048) && (capa & IPMSG_AES_256)) {
		capa &= IPMSG_RSA_2048 | IPMSG_AES_256 | IPMSG_SIGN_SHA1 | IPMSG_PACKETNO_IV |
				IPMSG_ENCODE_BASE64;
	}
	else if ((capa & IPMSG_RSA_1024) && (capa & IPMSG_BLOWFISH_128)) {
		capa &= IPMSG_RSA_1024 | IPMSG_BLOWFISH_128 | IPMSG_PACKETNO_IV | IPMSG_ENCODE_BASE64;
	}
	else if ((capa & IPMSG_RSA_512) && (capa & IPMSG_RC2_40)) {
		capa &= IPMSG_RSA_512 | IPMSG_RC2_40 | IPMSG_ENCODE_BASE64;
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
	max_body_size -= (int)((host->pubKey.KeyLen() * stringify_coef + 3) *
							((capa & IPMSG_SIGN_SHA1) ? 2 : 1));
	max_body_size = (int)(max_body_size / stringify_coef);

	// KeyBlob 作成
	host->pubKey.KeyBlob(data, data.Size(), &len);

	// 送信先公開鍵の import
	if (!::CryptImportKey(target_csp, data, len, 0, 0, &hExKey)) {
		host->pubKey.UnSet();
		return GetLastErrorMsg("CryptImportKey"), FALSE;
	}

	// IV の初期化
	memset(iv, 0, sizeof(iv));
	if (capa & IPMSG_PACKETNO_IV) sprintf((char *)iv, "%u", packet_no);

	// UNIX 形式の改行に変換
	msgLen = LocalNewLineToUnix(msgstr, msg_data, max_body_size) + 1;
	if (share && is_extenc) {
		memcpy((char *)msg_data + msgLen, share, sharelen);
		msgLen += sharelen;
	}

	if (capa & IPMSG_AES_256) {	// AES
		// AES 用ランダム鍵作成
		if (!::CryptGenRandom(target_csp, len = 256/8, data))
			return	GetLastErrorMsg("CryptGenRandom"), FALSE;

		// AES 用共通鍵のセット
		AES		aes(data, len, iv);

		//共通鍵の暗号化
		if (!::CryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF))
			return GetLastErrorMsg("CryptEncrypt"), FALSE;
		bin2str_revendian(data, len, (char *)skey);	// 鍵をhex文字列に

		// メッセージ暗号化
		encMsgLen = aes.EncryptCBC(msg_data, data, msgLen);
	}
	else if (capa & IPMSG_BLOWFISH_128) {	// blowfish
		// blowfish 用ランダム鍵作成
		if (!::CryptGenRandom(target_csp, len = 128/8, data))
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
	else {	// RC2
		if (!::CryptGenKey(target_csp, CALG_RC2, CRYPT_EXPORTABLE, &hKey))
			return	GetLastErrorMsg("CryptGenKey"), FALSE;

		::CryptExportKey(hKey, hExKey, SIMPLEBLOB, 0, NULL, (DWORD *)&len);
		if (!::CryptExportKey(hKey, hExKey, SIMPLEBLOB, 0, data, (DWORD *)&len))
			return GetLastErrorMsg("CryptExportKey"), FALSE;

		len -= SKEY_HEADER_SIZE;
		bin2str_revendian((BYTE*)data + SKEY_HEADER_SIZE, len, (char *)skey);
		memcpy(data, msg_data, msgLen);

		// メッセージの暗号化
		encMsgLen = msgLen;
		if (!::CryptEncrypt(hKey, 0, TRUE, 0, data, &encMsgLen, MAX_UDPBUF))
			return GetLastErrorMsg("CryptEncrypt RC2"), FALSE;
		::CryptDestroyKey(hKey);
	}
	len =  wsprintf(buf, "%X:%s:", capa, skey);
	len += bin2str(data, (int)encMsgLen, buf + len);

	::CryptDestroyKey(hExKey);

	// 電子署名の追加
	if (capa & IPMSG_SIGN_SHA1) {
		HCRYPTHASH	hHash;
		if (::CryptCreateHash(target_csp, CALG_SHA, 0, 0, &hHash)) {
			if (::CryptHashData(hHash, msg_data, msgLen, 0)) {
				DWORD		sigLen = 0;
				::CryptSignHash(hHash, AT_KEYEXCHANGE, 0, 0, 0, &sigLen);
				if (::CryptSignHash(hHash, AT_KEYEXCHANGE, 0, 0, data, &sigLen)) {
					buf[len++] = ':';
					len += bin2str_revendian(data, (int)sigLen, buf + len);
				}
			}
			::CryptDestroyHash(hHash);
		}
	}

	return TRUE;
}

BOOL MsgMng::ResolveMsg(RecvBuf *buf, MsgBuf *msg)
{
	if (!ResolveMsg(buf->msgBuf, buf->size, msg)) return	FALSE;

	msg->hostSub.addr   = buf->addr;
	msg->hostSub.portNo = buf->port;
	return	TRUE;
}

BOOL MsgMng::ResolveMsg(char *buf, int size, MsgBuf *msg)
{
	char	*exStr  = NULL, *tok, *p;
	char	*exStr2 = NULL;
	char	*userName, *hostName;
	int		len, exLen = 0;

	len = (int)strlen(buf); // main message

	if (size > len + 1) { // ex message (group name or attached file)
		exStr = buf + len + 1;
		exLen = (int)strlen(exStr);
		if (size > len + 1 + exLen + 1) { // ex2 message (utf8 entry)
			exStr2 = exStr + exLen + 1;
		}
	}

	if ((tok = separate_token(buf, ':', &p)) == NULL)
		return	FALSE;
	if ((msg->version = atoi(tok)) != IPMSG_VERSION)
		return	FALSE;

	if ((tok = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;
	msg->packetNo = strtoul(tok, 0, 10);
	strncpyz(msg->packetNoStr, tok, sizeof(msg->packetNoStr)); // for IV

	if ((userName = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;

	if ((hostName = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;

	if ((tok = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;
	msg->command = atol(tok);
	BOOL	is_utf8 = (msg->command & IPMSG_UTF8OPT);

	strncpyz(msg->hostSub.userName, is_utf8 ? userName : AtoU8s(userName),
			 sizeof(msg->hostSub.userName));
	strncpyz(msg->hostSub.hostName, is_utf8 ? hostName : AtoU8s(hostName),
			 sizeof(msg->hostSub.hostName));

	msg->timestamp = Time();

	int		cnt = 0;
	*msg->msgBuf = 0;
	if ((tok = separate_token(NULL, 0, &p))) { // 改行をUNIX形式からDOS形式に変換
		if (!is_utf8) {
			tok = AtoU8s(tok);
		}
		UnixNewLineToLocal(tok, msg->msgBuf, MAX_UDPBUF);
	}

	if (exStr) {
		if (exStr[0] != '\n') {
			if ((msg->command & IPMSG_UTF8OPT) == 0) {
				exStr = AtoU8s(exStr);
			}
			strncpyz(msg->exBuf, exStr, sizeof(msg->exBuf));
		}
		else if (exStr2 == NULL) {
			exStr2 = exStr;
		}

		if (exStr2 && exStr2[0] == '\n' && (msg->command & IPMSG_CAPUTF8OPT)) {
			for (tok=separate_token(exStr2, '\n', &p); tok; tok=separate_token(NULL, '\n', &p)) {
				if      (strncmp(tok, "UN:", 3) == 0) {
					strncpyz(msg->hostSub.userName, tok+3, sizeof(msg->hostSub.userName));
				}
				else if (strncmp(tok, "HN:", 3) == 0) {
					strncpyz(msg->hostSub.hostName, tok+3, sizeof(msg->hostSub.hostName));
				}
				else if (strncmp(tok, "NN:", 3) == 0) {
					switch (GET_MODE(msg->command)) {
					case IPMSG_BR_ENTRY: case IPMSG_BR_ABSENCE:
						strncpyz(msg->msgBuf, tok+3, sizeof(msg->msgBuf));
						break;
					}
				}
				else if (strncmp(tok, "GN:", 3) == 0) {
					switch (GET_MODE(msg->command)) {
					case IPMSG_BR_ENTRY: case IPMSG_BR_ABSENCE:
						strncpyz(msg->exBuf, tok+3, sizeof(msg->exBuf));
						break;
					}
				}
			}
		}
	}

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
	char		*capa_hex, *skey_hex, *msg_hex, *hash_hex, *p;
	BYTE		skey[MAX_BUF], sig_data[MAX_BUF], data[MAX_BUF], iv[256/8];
	int			len, msgLen, encMsgLen;
	HCRYPTPROV	target_csp;
	// hex encoding
	BOOL		(*str2bin_revendian)(const char *, BYTE *, int, int *) = hexstr2bin_revendian;
	BOOL		(*str2bin)(const char *, BYTE *, int, int *)           = hexstr2bin;
	DynBuf		tmpBuf(MAX_UDPBUF);

	if ((capa_hex = separate_token(msg.msgBuf, ':', &p)) == NULL) {
		return	FALSE;
	}
	cryptCapa = strtoul(capa_hex, 0, 16);

	int	kt = (cryptCapa & IPMSG_RSA_2048) ? KEY_2048 :
			 (cryptCapa & IPMSG_RSA_1024) ? KEY_1024 :
			 (cryptCapa & IPMSG_RSA_512)  ? KEY_512  : -1;
	if (kt == -1) return FALSE;

	target_csp	= cfg->priv[kt].hCsp;
	hExKey		= cfg->priv[kt].hKey;

	if (cryptCapa & IPMSG_ENCODE_BASE64) { // change to base64 encoding
		str2bin_revendian = b64str2bin_revendian;
		str2bin           = b64str2bin;
	}

	if ((skey_hex = separate_token(NULL, ':', &p)) == NULL) {
		return	FALSE;
	}
	if ((msg_hex = separate_token(NULL, ':', &p)) == NULL) {
		return	FALSE;
	}

	if (cryptCapa & IPMSG_SIGN_SHA1) {
		if ((hash_hex = separate_token(NULL, ':', &p)) == NULL)
			return	FALSE;
	}

	// IV の初期化
	memset(iv, 0, sizeof(iv));
	if (cryptCapa & IPMSG_PACKETNO_IV) strncpyz((char *)iv, msg.packetNoStr, sizeof(iv));

	if (cryptCapa & IPMSG_AES_256) {	// AES
		str2bin_revendian(skey_hex, skey, sizeof(skey), &len);
		// 公開鍵取得
		if (!::CryptDecrypt(hExKey, 0, TRUE, 0, (BYTE *)skey, (DWORD *)&len))
			return	wsprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;

		AES		aes(skey, len, iv);
		str2bin(msg_hex, tmpBuf, tmpBuf.Size(), &encMsgLen);
		msgLen = aes.DecryptCBC(tmpBuf, tmpBuf, encMsgLen);
	}
	else if (cryptCapa & IPMSG_BLOWFISH_128) {	// blowfish
		str2bin_revendian(skey_hex, skey, sizeof(skey), &len);
		// 公開鍵取得
		if (!::CryptDecrypt(hExKey, 0, TRUE, 0, (BYTE *)skey, (DWORD *)&len))
			return	wsprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;

		CBlowFish	bl(skey, len);
		str2bin(msg_hex, tmpBuf, tmpBuf.Size(), &encMsgLen);
		msgLen = bl.Decrypt(tmpBuf, tmpBuf, encMsgLen, BF_CBC|BF_PKCS5, iv);
	}
	else {	// RC2
		// Skey Blob を作る
		skey[0] = SIMPLEBLOB;
		skey[1] = CUR_BLOB_VERSION;
		*(WORD *)(skey + 2) = 0;
		*(ALG_ID *)(skey + 4) = CALG_RC2;
		*(ALG_ID *)(skey + 8) = CALG_RSA_KEYX;
		str2bin_revendian(skey_hex, skey + SKEY_HEADER_SIZE, sizeof(skey)-SKEY_HEADER_SIZE, &len);

		// セッションキーの import
		if (!::CryptImportKey(target_csp, skey, len + SKEY_HEADER_SIZE, hExKey, 0, &hKey))
			return	wsprintf(msg.msgBuf, "CryptImportKey Err(%X)", GetLastError()), FALSE;

		// メッセージの Decrypt
		str2bin(msg_hex, tmpBuf, tmpBuf.Size(), &encMsgLen);
		msgLen = encMsgLen;
		if (!::CryptDecrypt(hKey, 0, TRUE, 0, tmpBuf, (DWORD *)&msgLen))
			return	wsprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;
		::CryptDestroyKey(hKey);
	}

	// 電子署名の検証
	if (cryptCapa & IPMSG_SIGN_SHA1) {
		Host		*host = hosts ? hosts->GetHostByName(&msg.hostSub) : NULL;

		if (!host) host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
		if (host && IsSameHost(&msg.hostSub, &host->hostSub) && host->pubKey.KeyLen() == 256) {
			HCRYPTHASH	hHash = NULL;
			HCRYPTKEY	hExKey = 0;
			int			capa = host->pubKey.Capa();
			int			kt = (capa & IPMSG_RSA_2048) ? KEY_2048 :
							 (capa & IPMSG_RSA_1024) ? KEY_1024 : KEY_512;

			logOpt |= LOG_SIGN_ERR;
			target_csp = cfg->priv[kt].hCsp;
			host->pubKey.KeyBlob(data, sizeof(data), &len);	// KeyBlob 作成/import
			if (::CryptImportKey(target_csp, data, len, 0, 0, &hExKey)) {
				if (::CryptCreateHash(target_csp, CALG_SHA, 0, 0, &hHash)) {
					if (::CryptHashData(hHash, tmpBuf, msgLen, 0)) {
						int		sigLen = 0;
						str2bin_revendian(hash_hex, sig_data, sizeof(sig_data), &sigLen);
						if (::CryptVerifySignature(hHash, sig_data, sigLen, hExKey, 0, 0)) {
							logOpt &= ~LOG_SIGN_ERR;
							logOpt |= LOG_SIGN_OK;
						}
					}
					::CryptDestroyHash(hHash);
				}
				::CryptDestroyKey(hExKey);
			}
		}
		else logOpt |= LOG_SIGN_NOKEY;
	}
	logOpt |= (cryptCapa & IPMSG_RSA_2048) ? LOG_ENC2 :
			  (cryptCapa & IPMSG_RSA_1024) ? LOG_ENC1 : LOG_ENC0;


	// 暗号化添付メッセージ
	if ((msg.command & IPMSG_ENCEXTMSGOPT)) {
		if ((len = (int)strlen(tmpBuf) + 1) < msgLen) {
			strncpyz(msg.exBuf, (char *)tmpBuf + len, MAX_UDPBUF);
		}
	}

	// UNIX 形式の改行を変換
	UnixNewLineToLocal(tmpBuf, msg.msgBuf, sizeof(msg.msgBuf));
	if ((msg.command & IPMSG_UTF8OPT) == 0) {
		strncpyz(msg.msgBuf, AtoU8s(msg.msgBuf), sizeof(msg.msgBuf));
	}

	return	TRUE;
}


ULONG MsgMng::MakePacketNo(void)
{
	ULONG now = (ULONG)Time();

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
	if (isV6 && host_addr.IsIPv4()) host_addr.ChangeMappedIPv6();

	sockaddr_in6	addr6;
	sockaddr_in		addr4;
	sockaddr		*addr = host_addr.size == 16 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
	int				size  = host_addr.size == 16 ? sizeof(addr6)      : sizeof(addr4);

	if (host_addr.size != 16 && host_addr.size != 4) return FALSE;

	if (host_addr.size == 16) {
		memset(&addr6, 0, sizeof(addr6));
		addr6.sin6_family	= AF_INET6;
		addr6.sin6_port		= port_no;
		host_addr.Put(&addr6.sin6_addr, 16);
	}
	else {
		memset(&addr4, 0, sizeof(addr4));
		addr4.sin_family	= AF_INET;
		addr4.sin_port		= port_no;
		host_addr.Put(&addr4.sin_addr, 4);
	}

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
//				MessageBox(0, GetLoadStr(IDS_HOSTUNREACH), ::inet_ntoa(*(LPIN_ADDR)&host_addr), MB_OK);
			}
			return	FALSE;
		default:
			Debug("sendto(%s) error=%d\n", host_addr.ToStr(), ret);
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

	buf->size = ::recvfrom(udp_sd, buf->msgBuf, sizeof(buf->msgBuf) -1, 0, addr, &size);
	if (buf->size == SOCKET_ERROR) return	FALSE;
	buf->msgBuf[buf->size] = 0;

	if (size == sizeof(addr6)) {
		buf->addr.Set(&addr6.sin6_addr, 16);
		buf->port = addr6.sin6_port;
	}
	else if (size == sizeof(addr4)) {
		buf->addr.Set(&addr4.sin_addr, 4);
		buf->port = addr4.sin_port;
	}
	else return FALSE;

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
		info->port = addr6.sin6_port;
	}
	else {
		info->addr.Set(&addr4.sin_addr, 4);
		info->port = addr4.sin_port;
	}
	info->server = info->complete = TRUE;

	for (int buf_size=cfg->TcpbufMax; buf_size > 0; buf_size /= 2) {
		if (::setsockopt(info->sd, SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size)) == 0)
			break;
	}

	if (AsyncSelectConnect(hWnd, info)) {
		info->startTick = info->lastTick = ::GetTickCount();
		return	TRUE;
	}

	::closesocket(info->sd);
	return	FALSE;
}

BOOL MsgMng::Connect(HWND hWnd, ConnectInfo *info)
{
	info->server = FALSE;
	if ((info->sd = ::socket(isV6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return FALSE;
	}
	BOOL flg = FALSE;
	if (isV6 && ::setsockopt(info->sd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&flg, sizeof(flg))) {
		return	GetSockErrorMsg("setsockopt3(IPV6_V6ONLY)"), FALSE;
	}

	flg = TRUE;	// Non Block
	if (::ioctlsocket(info->sd, FIONBIO, (unsigned long *)&flg)) return FALSE;

	::setsockopt(info->sd, SOL_SOCKET, TCP_NODELAY, (char *)&flg, sizeof(flg));

	for (int buf_size=cfg->TcpbufMax; buf_size > 0; buf_size /= 2) {
		if (::setsockopt(info->sd, SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(buf_size)) == 0)
			break;
	}

	if (AsyncSelectConnect(hWnd, info)) {
		sockaddr_in6	addr6;
		sockaddr_in		addr4;
		sockaddr		*addr = isV6 ? (sockaddr *)&addr6 : (sockaddr *)&addr4;
		int				size =  isV6 ? sizeof(addr6) : sizeof(addr4);
		memset(addr, 0, size);

		if (isV6) {
			addr6.sin6_family	= AF_INET6;
			addr6.sin6_port		= info->port;
			info->addr.Put(&addr6.sin6_addr, 16);
		}
		else {
			addr4.sin_family	= AF_INET;
			addr4.sin_port		= info->port;
			info->addr.Put(&addr4.sin_addr, 4);
		}

		info->complete = ::connect(info->sd, (LPSOCKADDR)addr, size) != -1;
		if (info->complete || ::WSAGetLastError() == WSAEWOULDBLOCK) {
			info->startTick = info->lastTick = ::GetTickCount();
			return	TRUE;
		}
	}
	::closesocket(info->sd);
	return	FALSE;
}

BOOL MsgMng::AsyncSelectConnect(HWND hWnd, ConnectInfo *info)
{
	if (::WSAAsyncSelect(info->sd, hWnd, WM_TCPEVENT,
						 (info->server ? FD_READ : FD_CONNECT)|FD_CLOSE) == SOCKET_ERROR) {
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

AddrInfo *GetIPAddrs(GetIPAddrsMode mode, int *num, int ipv6mode)
{
	DWORD		size;
	AddrInfo	*ret = NULL;
	int			&di = *num;
	ULONG		family = ipv6mode == 0 ? AF_INET : ipv6mode == 1 ? AF_INET6 : AF_UNSPEC;
	DWORD		flags = GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_SKIP_FRIENDLY_NAME;
	IP_ADAPTER_ADDRESSES *adr;

#define MAX_ADDRS	100

	di = 0;

	if (::GetAdaptersAddresses(family, 0, 0, 0, &size) != ERROR_BUFFER_OVERFLOW) return NULL;

	DynBuf	buf(size);
	adr = (IP_ADAPTER_ADDRESSES *)(void *)buf;
	if (::GetAdaptersAddresses(family, 0, 0, adr, &size) != ERROR_SUCCESS) return NULL;

	ret = new AddrInfo[MAX_ADDRS];

	for ( ; adr && di < MAX_ADDRS; adr = adr->Next) {
		if (adr->OperStatus != IfOperStatusUp ||
			adr->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
		//	adr->IfType == IF_TYPE_TUNNEL ||
			adr->PhysicalAddressLength <= 0) continue;

		for (IP_ADAPTER_UNICAST_ADDRESS	*uni = adr->FirstUnicastAddress; uni; uni = uni->Next) {
			if (ipv6mode == 0 && !(uni->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE)) continue;
			if (uni->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) continue;

			ret[di].type = AddrInfo::UNICAST;
			ret[di].mask = uni->OnLinkPrefixLength;

			if (uni->Address.lpSockaddr->sa_family == AF_INET6 && ipv6mode > 0) {
				sockaddr_in6	*sin6 = (sockaddr_in6  *)uni->Address.lpSockaddr;
				ret[di].addr.Set(&sin6->sin6_addr, 16);
				Debug("addr6(uni)=%s\n", ret[di].addr.ToStr());
			}
			else if (uni->Address.lpSockaddr->sa_family == AF_INET && ipv6mode != 1) {
				sockaddr_in		*sin4 = (sockaddr_in  *)uni->Address.lpSockaddr;
				ret[di].addr.Set(&sin4->sin_addr, 4);
				Debug("addr4(uni)=%s\n", ret[di].addr.ToStr());

				if (mode != GIA_NOBROADCAST) {
					ULONG	br_addr = ret[di].addr.V4Addr() | (0xffffffffUL >> ret[di].mask);
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
						ret[di].mask = uni->OnLinkPrefixLength;
						ret[di].addr.SetV4Addr(br_addr);
						Debug("addr4(br)=%s\n", ret[di].addr.ToStr());
					}
				}
			}
			di++;
		}

		if (mode == GIA_NOBROADCAST) continue;

		for (IP_ADAPTER_MULTICAST_ADDRESS *mul=adr->FirstMulticastAddress; mul; mul=mul->Next) {
			if (ipv6mode == 0 && !(mul->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE)) continue;
			if (mul->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) continue;

			ret[di].mask = 128;
			ret[di].type = AddrInfo::MULTICAST;

			if (mul->Address.lpSockaddr->sa_family == AF_INET6 && ipv6mode > 0) {
				sockaddr_in6	*sin6 = (sockaddr_in6  *)mul->Address.lpSockaddr;
				ret[di].addr.Set(&sin6->sin6_addr, 16);
				Debug("addr6(mul)=%s\n", ret[di].addr.ToStr());
			}
			if (mul->Address.lpSockaddr->sa_family == AF_INET && ipv6mode != 1) {
				sockaddr_in		*sin4 = (sockaddr_in  *)mul->Address.lpSockaddr;
				ret[di].addr.Set(&sin4->sin_addr, 4);
				Debug("addr4(mul)=%s\n", ret[di].addr.ToStr());
			}
			di++;
		}
	}

	return	ret;
}

