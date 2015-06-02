#include "tlib.h"
#include "twinsock.h"

int (WINAPI *TWSAStartup)(WORD wVersionRequested, WSADATA *lpWSAData);
int (WINAPI *TWSACleanup)(void);
int (WINAPI *TWSAGetLastError)(void);
int (WINAPI *TWSAAsyncSelect)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);

SOCKET (WINAPI *Tsocket)(int af, int type, int protocol);
int (WINAPI *Tbind)(SOCKET s, const struct sockaddr *name, int namelen);
int (WINAPI *Tclosesocket)(SOCKET s);
int (WINAPI *Tsendto)(SOCKET s, const char * buf, int len, int flags, const struct sockaddr * to, int tolen);
int (WINAPI *Trecvfrom)(SOCKET s, char * buf, int len, int flags, struct sockaddr * from, int * fromlen);
int (WINAPI *Trecv)(SOCKET s, char *buf, int len, int flags);
int (WINAPI *Tsend)(SOCKET s, const char *buf, int len, int flags);
SOCKET (WINAPI *Taccept)(SOCKET s, struct sockaddr *addr, int *addrlen);
int (WINAPI *Tconnect)(SOCKET s, const struct sockaddr *name, int namelen);
int (WINAPI *Tlisten)(SOCKET s, int backlog);
int (WINAPI *Tselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
struct hostent *(WINAPI *Tgethostbyname)(const char *name);
int (WINAPI *Tgethostname)(char *name, int namelen);
int (WINAPI *Tsetsockopt)(SOCKET s, int level, int optname, const char *optval, int optlen);
int (WINAPI *Tioctlsocket)(SOCKET s, long cmd, u_long *argp);

unsigned long (WINAPI *Tinet_addr)(const char * cp);
char *(WINAPI *Tinet_ntoa)(struct in_addr in);
u_long (WINAPI *Tntohl)(u_long netlong);
u_short (WINAPI *Thtons)(u_short hostshort);


BOOL TLibInit_WinSock()
{
	HMODULE	hSock = ::LoadLibrary("Ws2_32.dll");

	if (!hSock) hSock = ::LoadLibrary("wsock32.dll");
	if (!hSock) {
		MessageBox(0, "winsock library can't found (ws_32.dll or wsock32.dll)", "", MB_OK);
	}

	TWSAStartup = (int (WINAPI *)(WORD wVersionRequested, WSADATA *lpWSAData))::GetProcAddress(hSock, "WSAStartup");
	TWSACleanup = (int (WINAPI *)(void))::GetProcAddress(hSock, "WSACleanup");
	TWSAGetLastError = (int (WINAPI *)(void))::GetProcAddress(hSock, "WSAGetLastError");
	TWSAAsyncSelect = (int (WINAPI *)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent))::GetProcAddress(hSock, "WSAAsyncSelect");

	Tsocket = (SOCKET (WINAPI *)(int af, int type, int protocol))::GetProcAddress(hSock, "socket");
	Tbind = (int (WINAPI *)(SOCKET s, const struct sockaddr *name, int namelen))::GetProcAddress(hSock, "bind");
	Tclosesocket = (int (WINAPI *)(SOCKET s))::GetProcAddress(hSock, "closesocket");
	Tsendto = (int (WINAPI *)(SOCKET s, const char * buf, int len, int flags, const struct sockaddr * to, int tolen))::GetProcAddress(hSock, "sendto");
	Trecvfrom = (int (WINAPI *)(SOCKET s, char * buf, int len, int flags, struct sockaddr * from, int * fromlen))::GetProcAddress(hSock, "recvfrom");
	Trecv = (int (WINAPI *)(SOCKET s, char *buf, int len, int flags))::GetProcAddress(hSock, "recv");
	Tsend = (int (WINAPI *)(SOCKET s, const char *buf, int len, int flags))::GetProcAddress(hSock, "send");
	Taccept = (SOCKET (WINAPI *)(SOCKET s, struct sockaddr *addr, int *addrlen))::GetProcAddress(hSock, "accept");
	Tconnect = (int (WINAPI *)(SOCKET s, const struct sockaddr *name, int namelen))::GetProcAddress(hSock, "connect");
	Tlisten = (int (WINAPI *)(SOCKET s, int backlog))::GetProcAddress(hSock, "listen");
	Tselect = (int (WINAPI *)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout))::GetProcAddress(hSock, "select");
	Tgethostbyname = (struct hostent *(WINAPI *)(const char *name))::GetProcAddress(hSock, "gethostbyname");
	Tgethostname = (int (WINAPI *)(char *name, int namelen))::GetProcAddress(hSock, "gethostname");
	Tsetsockopt = (int (WINAPI *)(SOCKET s, int level, int optname, const char *optval, int optlen))::GetProcAddress(hSock, "setsockopt");
	Tioctlsocket = (int (WINAPI *)(SOCKET s, long cmd, u_long *argp))::GetProcAddress(hSock, "ioctlsocket");

	Tinet_addr = (unsigned long (WINAPI *)(const char *cp))::GetProcAddress(hSock, "inet_addr");
	Tinet_ntoa = (char *(WINAPI *)(struct in_addr in))::GetProcAddress(hSock, "inet_ntoa");
	Tntohl = (u_long (WINAPI *)(u_long netlong))::GetProcAddress(hSock, "ntohl");
	Thtons = (u_short (WINAPI *)(u_short hostshort))::GetProcAddress(hSock, "htons");

	return	TRUE;
}

