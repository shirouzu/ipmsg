#ifndef TWINSOCK_H
#define TWINSOCK_H

extern int (WINAPI *TWSAStartup)(WORD wVersionRequested, WSADATA *lpWSAData);
extern int (WINAPI *TWSACleanup)(void);
extern int (WINAPI *TWSAGetLastError)(void);
extern int (WINAPI *TWSAAsyncSelect)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);

extern SOCKET (WINAPI *Tsocket)(int af, int type, int protocol);
extern int (WINAPI *Tbind)(SOCKET s, const struct sockaddr *name, int namelen);
extern int (WINAPI *Tclosesocket)(SOCKET s);
extern int (WINAPI *Tsendto)(SOCKET s, const char * buf, int len, int flags, const struct sockaddr * to, int tolen);
extern int (WINAPI *Trecvfrom)(SOCKET s, char * buf, int len, int flags, struct sockaddr * from, int * fromlen);
extern int (WINAPI *Trecv)(SOCKET s, char *buf, int len, int flags);
extern int (WINAPI *Tsend)(SOCKET s, const char *buf, int len, int flags);
extern SOCKET (WINAPI *Taccept)(SOCKET s, struct sockaddr *addr, int *addrlen);
extern int (WINAPI *Tconnect)(SOCKET s, const struct sockaddr *name, int namelen);
extern int (WINAPI *Tlisten)(SOCKET s, int backlog);
extern int (WINAPI *Tselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
extern struct hostent *(WINAPI *Tgethostbyname)(const char *name);
extern int (WINAPI *Tgethostname)(char *name, int namelen);
extern int (WINAPI *Tsetsockopt)(SOCKET s, int level, int optname, const char *optval, int optlen);
extern int (WINAPI *Tioctlsocket)(SOCKET s, long cmd, u_long *argp);

extern unsigned long (WINAPI *Tinet_addr)(const char *cp);
extern char *(WINAPI *Tinet_ntoa)(struct in_addr in);
extern u_long (WINAPI *Tntohl)(u_long netlong);
extern u_short (WINAPI *Thtons)(u_short hostshort);

BOOL TLibInit_WinSock();

#endif

