static char *tmisc_id = 
	"@(#)Copyright (C) 1996-2015 H.Shirouzu		tmisc.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2015-04-06(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

DWORD TWinVersion = ::GetVersion();

HINSTANCE defaultStrInstance;


BOOL THashObj::LinkHash(THashObj *top)
{
	if (prevHash)
		return FALSE;
	this->nextHash = top->nextHash;
	this->prevHash = top;
	top->nextHash->prevHash = this;
	top->nextHash = this;
	return TRUE;
}

BOOL THashObj::UnlinkHash()
{
	if (!prevHash)
		return FALSE;
	prevHash->nextHash = nextHash;
	nextHash->prevHash = prevHash;
	prevHash = nextHash = NULL;
	return TRUE;
}


THashTbl::THashTbl(int _hashNum, BOOL _isDeleteObj)
{
	hashTbl = NULL;
	registerNum = 0;
	isDeleteObj = _isDeleteObj;

	if ((hashNum = _hashNum) > 0) {
		Init(hashNum);
	}
}

THashTbl::~THashTbl()
{
	UnInit();
}

BOOL THashTbl::Init(int _hashNum)
{
	if ((hashTbl = new THashObj [hashNum = _hashNum]) == NULL) {
		return	FALSE;	// VC4's new don't occur exception
	}

	for (int i=0; i < hashNum; i++) {
		THashObj	*obj = hashTbl + i;
		obj->prevHash = obj->nextHash = obj;
	}
	registerNum = 0;
	return	TRUE;
}

void THashTbl::UnInit()
{
	if (hashTbl) {
		if (isDeleteObj) {
			for (int i=0; i < hashNum && registerNum > 0; i++) {
				THashObj	*start = hashTbl + i;
				for (THashObj *obj=start->nextHash; obj != start; ) {
					THashObj *next = obj->nextHash;
					delete obj;
					obj = next;
					registerNum--;
				}
			}
		}
		delete [] hashTbl;
		hashTbl = NULL;
		registerNum = 0;
	}
}

void THashTbl::Register(THashObj *obj, u_int hash_id)
{
	obj->hashId = hash_id;

	if (obj->LinkHash(hashTbl + (hash_id % hashNum))) {
		registerNum++;
	}
}

void THashTbl::UnRegister(THashObj *obj)
{
	if (obj->UnlinkHash()) {
		registerNum--;
	}
}

THashObj *THashTbl::Search(const void *data, u_int hash_id)
{
	THashObj *top = hashTbl + (hash_id % hashNum);

	for (THashObj *obj=top->nextHash; obj != top; obj=obj->nextHash) {
		if (obj->hashId == hash_id && IsSameVal(obj, data)) {
			return obj;
		}
	}
	return	NULL;
}


/*=========================================================================
  クラス ： Condition
  概  要 ： 条件変数クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
Condition::Condition(void)
{
	hEvents = NULL;
}

Condition::~Condition(void)
{
	UnInitialize();
}

BOOL Condition::Initialize(int _max_threads)
{
	UnInitialize();

	max_threads = _max_threads;
	waitEvents = new WaitEvent [max_threads];
	hEvents = new HANDLE [max_threads];
	for (int wait_id=0; wait_id < max_threads; wait_id++) {
		if (!(hEvents[wait_id] = ::CreateEvent(0, FALSE, FALSE, NULL)))
			return	FALSE;
		waitEvents[wait_id] = CLEAR_EVENT;
	}
	::InitializeCriticalSection(&cs);
	waitCnt = 0;
	return	TRUE;
}

void Condition::UnInitialize(void)
{
	if (hEvents) {
		while (--max_threads >= 0)
			::CloseHandle(hEvents[max_threads]);
		delete [] hEvents;
		delete [] waitEvents;
		hEvents = NULL;
		waitEvents = NULL;
		::DeleteCriticalSection(&cs);
	}
}

BOOL Condition::Wait(DWORD timeout)
{
	int		wait_id = 0;

	for (wait_id=0; wait_id < max_threads && waitEvents[wait_id] != CLEAR_EVENT; wait_id++)
		;
	if (wait_id == max_threads) {	// 通常はありえない
		MessageBox(0, "Detect too many wait threads", "TLib", MB_OK);
		return	FALSE;
	}
	waitEvents[wait_id] = WAIT_EVENT;
	waitCnt++;
	UnLock();

	DWORD	status = ::WaitForSingleObject(hEvents[wait_id], timeout);

	Lock();
	--waitCnt;
	waitEvents[wait_id] = CLEAR_EVENT;

	return	status == WAIT_TIMEOUT ? FALSE : TRUE;
}

void Condition::Notify(void)	// 現状では、眠っているスレッド全員を起こす
{
	if (waitCnt > 0) {
		for (int wait_id=0, done_cnt=0; wait_id < max_threads; wait_id++) {
			if (waitEvents[wait_id] == WAIT_EVENT) {
				::SetEvent(hEvents[wait_id]);
				waitEvents[wait_id] = DONE_EVENT;
				if (++done_cnt >= waitCnt)
					break;
			}
		}
	}
}

/*=========================================================================
  クラス ： VBuf
  概  要 ： 仮想メモリ管理クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
VBuf::VBuf(size_t _size, size_t _max_size, VBuf *_borrowBuf)
{
	Init();

	if (_size || _max_size) AllocBuf(_size, _max_size, _borrowBuf);
}

VBuf::~VBuf()
{
	if (buf)
		FreeBuf();
}

void VBuf::Init(void)
{
	buf = NULL;
	borrowBuf = NULL;
	size = usedSize = maxSize = 0;
}

BOOL VBuf::AllocBuf(size_t _size, size_t _max_size, VBuf *_borrowBuf)
{
	if (_max_size == 0)
		_max_size = _size;
	maxSize = _max_size;
	borrowBuf = _borrowBuf;

	if (borrowBuf) {
		if (!borrowBuf->Buf() || borrowBuf->MaxSize() < borrowBuf->UsedSize() + maxSize)
			return	FALSE;
		buf = borrowBuf->Buf() + borrowBuf->UsedSize();
		borrowBuf->AddUsedSize(maxSize + PAGE_SIZE);
	}
	else {
	// 1page 分だけ余計に確保（buffer over flow 検出用）
		if (!(buf = (BYTE *)::VirtualAlloc(NULL, maxSize + PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE))) {
			Init();
			return	FALSE;
		}
	}
	return	Grow(_size);
}

BOOL VBuf::LockBuf(void)
{
	return	::VirtualLock(buf, size);
}

void VBuf::FreeBuf(void)
{
	if (buf) {
		if (borrowBuf) {
			::VirtualFree(buf, maxSize + PAGE_SIZE, MEM_DECOMMIT);
		}
		else {
			::VirtualFree(buf, 0, MEM_RELEASE);
		}
	}
	Init();
}

BOOL VBuf::Grow(size_t grow_size)
{
	if (size + grow_size > maxSize)
		return	FALSE;

	if (grow_size && !::VirtualAlloc(buf + size, grow_size, MEM_COMMIT, PAGE_READWRITE))
		return	FALSE;

	size += grow_size;
	return	TRUE;
}


void InitInstanceForLoadStr(HINSTANCE hI)
{
	defaultStrInstance = hI;
}

LPSTR GetLoadStrA(UINT resId, HINSTANCE hI)
{
	static TResHash	*hash;

	if (hash == NULL) {
		hash = new TResHash(100);
	}

	char		buf[1024];
	TResHashObj	*obj;

	if ((obj = hash->Search(resId)) == NULL) {
		if (::LoadStringA(hI ? hI : defaultStrInstance, resId, buf, sizeof(buf)) >= 0) {
			obj = new TResHashObj(resId, strdup(buf));
			hash->Register(obj);
		}
	}
	return	obj ? (char *)obj->val : NULL;
}

LPWSTR GetLoadStrW(UINT resId, HINSTANCE hI)
{
	static TResHash	*hash;

	if (hash == NULL) {
		hash = new TResHash(100);
	}

	WCHAR		buf[1024];
	TResHashObj	*obj;

	if ((obj = hash->Search(resId)) == NULL) {
		if (::LoadStringW(hI ? hI : defaultStrInstance, resId, buf,
				sizeof(buf) / sizeof(WCHAR)) >= 0) {
			obj = new TResHashObj(resId, wcsdup(buf));
			hash->Register(obj);
		}
	}
	return	obj ? (LPWSTR)obj->val : NULL;
}

static LCID defaultLCID;

void TSetDefaultLCID(LCID lcid)
{
	defaultLCID = lcid ? lcid : ::GetSystemDefaultLCID();

	TSetThreadLocale(defaultLCID);
}

HMODULE TLoadLibrary(LPSTR dllname)
{
	HMODULE	hModule = ::LoadLibrary(dllname);

	if (defaultLCID) {
		TSetThreadLocale(defaultLCID);
	}

	return	hModule;
}

HMODULE TLoadLibraryW(WCHAR *dllname)
{
	HMODULE	hModule = LoadLibraryW(dllname);

	if (defaultLCID) {
		TSetThreadLocale(defaultLCID);
	}

	return	hModule;
}

/*=========================================================================
	パス合成（ANSI 版）
=========================================================================*/
int MakePath(char *dest, const char *dir, const char *file)
{
	BOOL	separetor = TRUE;
	size_t	len;

	if ((len = strlen(dir)) == 0)
		return	wsprintf(dest, "%s", file);

	if (dir[len -1] == '\\')	// 表など、2byte目が'\\'で終る文字列対策
	{
		if (len >= 2 && !IsDBCSLeadByte(dir[len -2]))
			separetor = FALSE;
		else {
			u_char *p=(u_char *)dir;
			for (; *p && p[1]; IsDBCSLeadByte(*p) ? p+=2 : p++)
				;
			if (*p == '\\')
				separetor = FALSE;
		}
	}
	return	wsprintf(dest, "%s%s%s", dir, separetor ? "\\" : "", file);
}

/*=========================================================================
	パス合成（UNICODE 版）
=========================================================================*/
int MakePathW(WCHAR *dest, const WCHAR *dir, const WCHAR *file)
{
	size_t	len;

	if ((len = wcslen(dir)) == 0)
		return	wsprintfW(dest, L"%s", file);

	return	wsprintfW(dest, L"%s%s%s", dir, dir[len -1] == '\\' ? L"" : L"\\" , file);
}


/*=========================================================================
	bin <-> hex
=========================================================================*/
static char  *hexstr   =  "0123456789abcdef";
static WCHAR *hexstr_w = L"0123456789abcdef";

inline u_char hexchar2char(u_char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 10;
	return 0xff;
}

BOOL hexstr2bin(const char *buf, BYTE *bindata, int maxlen, int *len)
{
	for (*len=0; buf[0] && buf[1] && *len < maxlen; buf+=2, (*len)++)
	{
		u_char c1 = hexchar2char(buf[0]);
		u_char c2 = hexchar2char(buf[1]);
		if (c1 == 0xff || c2 == 0xff) break;
		bindata[*len] = (c1 << 4) | c2;
	}
	return	TRUE;
}

int bin2hexstr(const BYTE *bindata, int len, char *buf)
{
	for (const BYTE *end=bindata+len; bindata < end; bindata++)
	{
		*buf++ = hexstr[*bindata >> 4];
		*buf++ = hexstr[*bindata & 0x0f];
	}
	*buf = 0;
	return	len * 2;
}

int bin2hexstrW(const BYTE *bindata, int len, WCHAR *buf)
{
	for (const BYTE *end=bindata+len; bindata < end; bindata++)
	{
		*buf++ = hexstr_w[*bindata >> 4];
		*buf++ = hexstr_w[*bindata & 0x0f];
	}
	*buf = 0;
	return	len * 2;
}

/* little-endian binary to hexstr */
int bin2hexstr_revendian(const BYTE *bindata, int len, char *buf)
{
	int		sv_len = len;
	while (len-- > 0)
	{
		*buf++ = hexstr[bindata[len] >> 4];
		*buf++ = hexstr[bindata[len] & 0x0f];
	}
	*buf = 0;
	return	sv_len * 2;
}

BOOL hexstr2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len)
{
	*len = 0;
	for (int buflen = (int)strlen(buf); buflen >= 2 && *len < maxlen; buflen-=2, (*len)++)
	{
		u_char c1 = hexchar2char(buf[buflen-1]);
		u_char c2 = hexchar2char(buf[buflen-2]);
		if (c1 == 0xff || c2 == 0xff) break;
		bindata[*len] = c1 | (c2 << 4);
	}
	return	TRUE;
}

int strip_crlf(const char *s, char *d)
{
	char	*sv = d;

	while (*s) {
		char	c = *s++;
		if (c != '\r' && c != '\n') *d++ = c;
	}
	*d = 0;
	return	(int)(d - sv);
}

/* base64 convert routine */
BOOL b64str2bin(const char *buf, BYTE *bindata, int maxlen, int *len)
{
	*len = maxlen;
	return	::CryptStringToBinary(buf, 0, CRYPT_STRING_BASE64, bindata, (DWORD *)len, 0, 0);
}

int bin2b64str(const BYTE *bindata, int len, char *str)
{
	int		size = len * 2 + 5;
	char	*b64 = new char [size];

	if (!::CryptBinaryToString(bindata, len, CRYPT_STRING_BASE64, b64, (DWORD *)&size)) {
		return 0;
	}
	size = strip_crlf(b64, str);

	delete [] b64;
	return	size;
}

BOOL b64str2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len)
{
	if (!b64str2bin(buf, bindata, maxlen, len)) return FALSE;
	rev_order(bindata, *len);
	return	TRUE;
}

int bin2b64str_revendian(const BYTE *bindata, int len, char *buf)
{
	BYTE *rev = new BYTE [len];

	if (!rev) return -1;

	rev_order(bindata, rev, len);
	int	ret = bin2b64str(rev, len, buf);
	delete [] rev;

	return	ret;
}

int bin2urlstr(const BYTE *bindata, int len, char *str)
{
	int ret = bin2b64str(bindata, len, str);

	for (char *s=str; *s; s++) {
		switch (*s) {
		case '+': *s = '-'; break;
		case '/': *s = '_'; break;

		case '\r':
		case '\n':
		case '=': *s = 0;   break;
		}
	}
	return	ret;
}

/*
0: 0
1: 2+2
2: 3+1
3: 4
4  6+2
*/

BOOL urlstr2bin(const char *str, BYTE *bindata, int maxlen, int *len)
{
	size_t	size = strlen(str);
	char	*b64 = new char [size + 4];

	strcpy(b64, str);
	for (char *s=b64; *s; s++) {
		switch (*s) {
		case '-': *s = '+'; break;
		case '_': *s = '/'; break;
		}
	}
	if (b64[size-1] != '\n' && (size % 4) && b64[size-1] != '=') {
		sprintf(b64 + size -1, "%.*s", 4 - (size % 4), "===");
	}

	b64str2bin(b64, bindata, maxlen, len);

	free(b64);
	return	TRUE;
}

/*
	16進 -> long long
*/
int64 hex2ll(char *buf)
{
	int64	ret = 0;

	for ( ; *buf; buf++)
	{
		if (*buf >= '0' && *buf <= '9')
			ret = (ret << 4) | (*buf - '0');
		else if (toupper(*buf) >= 'A' && toupper(*buf) <= 'F')
			ret = (ret << 4) | (toupper(*buf) - 'A' + 10);
		else continue;
	}
	return	ret;
}

void rev_order(BYTE *data, int size)
{
	BYTE	*d1 = data;
	BYTE	*d2 = data + size - 1;

	for (BYTE *end = d1 + (size/2); d1 < end; ) {
		BYTE	sv = *d1;
		*d1++ = *d2;
		*d2-- = sv;
	}
}

void rev_order(const BYTE *src, BYTE *dst, int size)
{
	dst = dst + size - 1;

	for (const BYTE *end = src + size; src < end; ) {
		*dst-- = *src++;
	}
}

/*=========================================================================
	Debug
=========================================================================*/
void Debug(char *fmt,...)
{
	char buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	::OutputDebugString(buf);
}

void DebugW(WCHAR *fmt,...)
{
	WCHAR buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	_vsnwprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	::OutputDebugStringW(buf);
}

void DebugU8(char *fmt,...)
{
	char buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	WCHAR *wbuf = U8toWs(buf);
	::OutputDebugStringW(wbuf);
	delete [] wbuf;
}

const char *Fmt(char *fmt,...)
{
	static char buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return	buf;
}

const WCHAR *FmtW(WCHAR *fmt,...)
{
	static WCHAR buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	_vsnwprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return	buf;
}


/*=========================================================================
	例外情報取得
=========================================================================*/
static char *ExceptionTitle;
static char *ExceptionLogFile;
static char *ExceptionLogInfo;
#define STACKDUMP_SIZE		256
#define MAX_STACKDUMP_SIZE	8192

LONG WINAPI Local_UnhandledExceptionFilter(struct _EXCEPTION_POINTERS *info)
{
	static char			buf[MAX_STACKDUMP_SIZE];
	static HANDLE		hFile;
	static SYSTEMTIME	tm;
	static CONTEXT		*context;
	static DWORD		len, i, j;
	static char			*stack, *esp;

	hFile = ::CreateFile(ExceptionLogFile, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
	::SetFilePointer(hFile, 0, 0, FILE_END);
	::GetLocalTime(&tm);
	context = info->ContextRecord;

	len = sprintf(buf,
#ifdef _WIN64
		"------ %s -----\r\n"
		" Date        : %d/%02d/%02d %02d:%02d:%02d\r\n"
		" Code/Addr   : %x / %p\r\n"
		" AX/BX/CX/DX : %p / %p / %p / %p\r\n"
		" SI/DI/BP/SP : %p / %p / %p / %p\r\n"
		" 08/09/10/11 : %p / %p / %p / %p\r\n"
		" 12/13/14/15 : %p / %p / %p / %p\r\n"
		"------- stack info -----\r\n"
		, ExceptionTitle
		, tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond
		, info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress
		, context->Rax, context->Rbx, context->Rcx, context->Rdx
		, context->Rsi, context->Rdi, context->Rbp, context->Rsp
		, context->R8,  context->R9,  context->R10, context->R11
		, context->R12, context->R13, context->R14, context->R15
#else
		"------ %s -----\r\n"
		" Date        : %d/%02d/%02d %02d:%02d:%02d\r\n"
		" Code/Addr   : %X / %p\r\n"
		" AX/BX/CX/DX : %08x / %08x / %08x / %08x\r\n"
		" SI/DI/BP/SP : %08x / %08x / %08x / %08x\r\n"
		"------- stack info -----\r\n"
		, ExceptionTitle
		, tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond
		, info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress
		, context->Eax, context->Ebx, context->Ecx, context->Edx
		, context->Esi, context->Edi, context->Ebp, context->Esp
#endif
		);
	::WriteFile(hFile, buf, len, &len, 0);

#ifdef _WIN64
		esp = (char *)context->Rsp;
#else
		esp = (char *)context->Esp;
#endif

	for (i=0; i < MAX_STACKDUMP_SIZE / STACKDUMP_SIZE; i++) {
		stack = esp + (i * STACKDUMP_SIZE);
		if (::IsBadReadPtr(stack, STACKDUMP_SIZE))
			break;
		len = 0;
		for (j=0; j < STACKDUMP_SIZE / sizeof(DWORD_PTR); j++)
			len += sprintf(buf + len, "%p%s", ((DWORD_PTR *)stack)[j],
							((j+1)%(32/sizeof(DWORD_PTR))) ? " " : "\r\n");
		::WriteFile(hFile, buf, len, &len, 0);
	}

	len = sprintf(buf, "------------------------\r\n\r\n");
	::WriteFile(hFile, buf, len, &len, 0);
	::CloseHandle(hFile);

	sprintf(buf, ExceptionLogInfo, ExceptionLogFile);
	::MessageBox(0, buf, ExceptionTitle, MB_OK);

	return	EXCEPTION_EXECUTE_HANDLER;
}

BOOL InstallExceptionFilter(const char *title, const char *info, const char *fname)
{
	char	buf[MAX_PATH];

	if (fname && *fname) {
		strcpy(buf, fname);
	} else {
		::GetModuleFileName(NULL, buf, sizeof(buf));
		strcpy(strrchr(buf, '.'), "_exception.log");
	}
	ExceptionLogFile = strdup(buf);
	ExceptionTitle = strdup(title);
	ExceptionLogInfo = strdup(info);

	::SetUnhandledExceptionFilter(&Local_UnhandledExceptionFilter);
	return	TRUE;
}


/*
	nul文字を必ず付与する strncpy
*/
char *strncpyz(char *dest, const char *src, size_t num)
{
	char	*sv = dest;

	while (num-- > 0)
		if ((*dest++ = *src++) == '\0')
			return	sv;

	if (sv != dest)		// num > 0
		*(dest -1) = 0;
	return	sv;
}

/*
	大文字小文字を無視する strncmp
*/
int strncmpi(const char *str1, const char *str2, size_t num)
{
	for (size_t cnt=0; cnt < num; cnt++)
	{
		char	c1 = toupper(str1[cnt]), c2 = toupper(str2[cnt]);

		if (c1 == c2)
		{
			if (c1)
				continue;
			else
				return	0;
		}
		if (c1 > c2)
			return	1;
		else
			return	-1;
	}
	return	0;
}

char *strdupNew(const char *_s, int max_len)
{
	int		len = int((max_len == -1) ? strlen(_s) : strnlen(_s, max_len));
	char	*s = new char [len + 1];
	memcpy(s, _s, len);
	s[len] = 0;
	return	s;
}

WCHAR *wcsdupNew(const WCHAR *_s, int max_len)
{
	int		len = int((max_len == -1) ? wcslen(_s) : wcsnlen(_s, max_len));
	WCHAR	*s = new WCHAR [len + 1];
	memcpy(s, _s, len * sizeof(WCHAR));
	s[len] = 0;
	return	s;
}


/* UNIX - Windows 文字コード変換 */
int LocalNewLineToUnix(const char *src, char *dest, int maxlen)
{
	char	*sv_dest = dest;
	char	*max_dest = dest + maxlen - 1;
	int		len = 0;

	while (*src && dest < max_dest) {
		if ((*dest = *src++) != '\r') dest++;
	}
	*dest = 0;

	return	int(dest - sv_dest);
}

int UnixNewLineToLocal(const char *src, char *dest, int maxlen)
{
	char	*sv_dest = dest;
	char	*max_dest = dest + maxlen - 1;

	while (*src && dest < max_dest) {
		if ((*dest = *src++) == '\n' && dest + 1 < max_dest) {
			*dest++ = '\r';
			*dest++ = '\n';
		}
		else dest++;
	}
	*dest = 0;

	return	int(dest - sv_dest);
}


/* Win64検出 */
BOOL TIsWow64()
{
	static BOOL	once = FALSE;
	static BOOL	ret = FALSE;

	if (!once) {
		BOOL (WINAPI *pIsWow64Process)(HANDLE, BOOL *) = (BOOL (WINAPI *)(HANDLE, BOOL *))
				GetProcAddress(::GetModuleHandle("kernel32"), "IsWow64Process");
		if (pIsWow64Process) {
			pIsWow64Process(::GetCurrentProcess(), &ret);
		}
		once = TRUE;
	}
    return ret;
}

BOOL TRegDisableReflectionKey(HKEY hBase)
{
	static BOOL	once = FALSE;
	static LONG (WINAPI *pRegDisableReflectionKey)(HKEY);

	if (!once) {
		pRegDisableReflectionKey = (LONG (WINAPI *)(HKEY))
			GetProcAddress(::GetModuleHandle("advapi32"), "RegDisableReflectionKey");
		once = TRUE;
	}
	if (pRegDisableReflectionKey && pRegDisableReflectionKey(hBase) == ERROR_SUCCESS)
		return	TRUE;
	return	FALSE;
}

BOOL TRegEnableReflectionKey(HKEY hBase)
{
	static BOOL	once = FALSE;
	static LONG (WINAPI *pRegEnableReflectionKey)(HKEY);

	if (!once) {
		pRegEnableReflectionKey = (LONG (WINAPI *)(HKEY))
			GetProcAddress(::GetModuleHandle("advapi32"), "RegEnableReflectionKey");
		once = TRUE;
	}
	if (pRegEnableReflectionKey && pRegEnableReflectionKey(hBase) == ERROR_SUCCESS)
		return	TRUE;
	return	FALSE;
}

BOOL TWow64DisableWow64FsRedirection(void *oldval)
{
	static BOOL	once = FALSE;
	static BOOL (WINAPI *pWow64DisableWow64FsRedirection)(void *);

	if (!once) {
		pWow64DisableWow64FsRedirection = (BOOL (WINAPI *)(void *))
			GetProcAddress(::GetModuleHandle("kernel32"), "Wow64DisableWow64FsRedirection");
		once = TRUE;
	}
	return	pWow64DisableWow64FsRedirection ? pWow64DisableWow64FsRedirection(oldval) : FALSE;
}

BOOL TWow64RevertWow64FsRedirection(void *oldval)
{
	static BOOL	once = FALSE;
	static BOOL (WINAPI *pWow64RevertWow64FsRedirection)(void *);

	if (!once) {
		pWow64RevertWow64FsRedirection = (BOOL (WINAPI *)(void *))
			GetProcAddress(::GetModuleHandle("kernel32"), "Wow64RevertWow64FsRedirection");
		once = TRUE;
	}
	return	pWow64RevertWow64FsRedirection ? pWow64RevertWow64FsRedirection(oldval) : FALSE;
}

BOOL TIsEnableUAC()
{
	static BOOL once = FALSE;
	static BOOL ret = FALSE;

	if (!once) {
		if (IsWinVista()) {
			TRegistry reg(HKEY_LOCAL_MACHINE);
			ret = TRUE;
			if (reg.OpenKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System")) {
				int	val = 1;
				if (reg.GetInt("EnableLUA", &val) && val == 0) {
					ret = FALSE;
				}
			}
		}
		once = TRUE;
	}
	return	ret;
}

BOOL TIsVirtualizedDirW(WCHAR *path)
{
	if (!IsWinVista()) return FALSE;

	WCHAR	buf[MAX_PATH];
	DWORD	csidl[] = { CSIDL_WINDOWS, CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86,
						CSIDL_COMMON_APPDATA, 0xffffffff };

	for (int i=0; csidl[i] != 0xffffffff; i++) {
		if (SHGetSpecialFolderPathW(NULL, buf, csidl[i], FALSE)) {
			size_t	len = wcslen(buf);
			if (wcsnicmp(buf, path, len) == 0) {
				WCHAR	ch = path[len];
				if (ch == 0 || ch == '\\' || ch == '/') {
					return	TRUE;
				}
			}
		}
	}

	return	FALSE;
}

BOOL TMakeVirtualStorePathW(WCHAR *org_path, WCHAR *buf)
{
	if (!IsWinVista()) return FALSE;

	if (!TIsVirtualizedDirW(org_path)
	|| !SHGetSpecialFolderPathW(NULL, buf, CSIDL_LOCAL_APPDATA, FALSE)
	||	org_path[1] != ':' || org_path[2] != '\\') {
		wcscpy(buf, org_path);
		return	FALSE;
	}

	swprintf(buf + wcslen(buf), L"\\VirtualStore%s", org_path + 2);
	return	TRUE;
}

BOOL TSetPrivilege(LPSTR pszPrivilege, BOOL bEnable)
{
    HANDLE           hToken;
    TOKEN_PRIVILEGES tp;

    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken))
        return FALSE;

    if (!::LookupPrivilegeValue(NULL, pszPrivilege, &tp.Privileges[0].Luid))
        return FALSE;

    tp.PrivilegeCount = 1;

    if (bEnable)
         tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
         tp.Privileges[0].Attributes = 0;

    if (!::AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0))
         return FALSE;

    if (!::CloseHandle(hToken))
         return FALSE;

    return TRUE;
}

BOOL TSetThreadLocale(int lcid)
{
	static BOOL	once = FALSE;
	static LANGID (WINAPI *pSetThreadUILanguage)(LANGID LangId);

	if (!once) {
		if (IsWinVista()) {	// ignore if XP
			pSetThreadUILanguage = (LANGID (WINAPI *)(LANGID LangId))
				GetProcAddress(::GetModuleHandle("kernel32"), "SetThreadUILanguage");
		}
		once = TRUE;
	}

	if (pSetThreadUILanguage) {
		pSetThreadUILanguage(LANGIDFROMLCID(lcid));
	}
	return ::SetThreadLocale(lcid);
}

BOOL TChangeWindowMessageFilter(UINT msg, DWORD flg)
{
	static BOOL	once = FALSE;
	static BOOL	(WINAPI *pChangeWindowMessageFilter)(UINT, DWORD);
	static BOOL	ret = FALSE;

	if (!once) {
		pChangeWindowMessageFilter = (BOOL (WINAPI *)(UINT, DWORD))
			GetProcAddress(::GetModuleHandle("user32"), "ChangeWindowMessageFilter");
		once = TRUE;
	}

	if (pChangeWindowMessageFilter) {
		ret = pChangeWindowMessageFilter(msg, flg);
	}

	return	ret;
}

void TSwitchToThisWindow(HWND hWnd, BOOL flg)
{
	static BOOL	once = FALSE;
	static void	(WINAPI *pSwitchToThisWindow)(HWND, BOOL);

	if (!once) {
		pSwitchToThisWindow = (void (WINAPI *)(HWND, BOOL))
			GetProcAddress(::GetModuleHandle("user32"), "SwitchToThisWindow");
		once = TRUE;
	}

	if (pSwitchToThisWindow) {
		pSwitchToThisWindow(hWnd, flg);
	}
}

/*
	リンク
	あらかじめ、CoInitialize(NULL); を実行しておくこと
	src  ... old_path
	dest ... new_path
*/
BOOL SymLinkW(WCHAR *src, WCHAR *dest, WCHAR *arg)
{
	IShellLinkW		*shellLink;
	IPersistFile	*persistFile;
	WCHAR			*ps_dest = dest;
	BOOL			ret = FALSE;
	WCHAR			buf[MAX_PATH];

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
			(void **)&shellLink))) {
		shellLink->SetPath(src);
		shellLink->SetArguments(arg);
		GetParentDirW(src, buf);
		shellLink->SetWorkingDirectory(buf);
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Save(ps_dest, TRUE))) {
				ret = TRUE;
				GetParentDirW(dest, buf);
				::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW|SHCNF_FLUSH, buf, NULL);
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	ret;
}

BOOL ReadLinkW(WCHAR *src, WCHAR *dest, WCHAR *arg)
{
	IShellLinkW		*shellLink;		// 実際は IShellLinkA or IShellLinkW
	IPersistFile	*persistFile;
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
			(void **)&shellLink))) {
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Load((WCHAR *)src, STGM_READ))) {
				if (SUCCEEDED(shellLink->GetPath(dest, MAX_PATH, NULL, 0))) {
					if (arg) {
						shellLink->GetArguments(arg, MAX_PATH);
					}
					ret = TRUE;
				}
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	ret;
}

/*
	リンクファイル削除
*/
BOOL DeleteLinkW(WCHAR *path)
{
	WCHAR	dir[MAX_PATH];

	if (!DeleteFileW(path))
		return	FALSE;

	GetParentDirW(path, dir);
	::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW|SHCNF_FLUSH, dir, NULL);

	return	TRUE;
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL GetParentDirW(const WCHAR *srcfile, WCHAR *dir)
{
	WCHAR	path[MAX_PATH], *fname=NULL;

	if (GetFullPathNameW(srcfile, MAX_PATH, path, &fname) == 0 || fname == NULL)
		return	wcscpy(dir, srcfile), FALSE;

	if ((fname - path) > 3 || path[1] != ':')
		fname[-1] = 0;
	else
		fname[0] = 0;		// C:\ の場合

	wcscpy(dir, path);
	return	TRUE;
}



// HtmlHelp WorkShop をインストールして、htmlhelp.h を include path に
// 入れること。
#define ENABLE_HTML_HELP
#if defined(ENABLE_HTML_HELP)
#include <htmlhelp.h>
#endif

HWND ShowHelpW(HWND hOwner, WCHAR *help_dir, WCHAR *help_file, WCHAR *section)
{
#if defined(ENABLE_HTML_HELP)
	static HWND (WINAPI *pHtmlHelpW)(HWND, WCHAR *, UINT, DWORD_PTR) = NULL;

	if (pHtmlHelpW == NULL) {
		DWORD		cookie=0;
		HMODULE		hHtmlHelp = TLoadLibrary("hhctrl.ocx");
		if (hHtmlHelp)
			pHtmlHelpW = (HWND (WINAPI *)(HWND, WCHAR *, UINT, DWORD_PTR))
						::GetProcAddress(hHtmlHelp, "HtmlHelpW");
		if (pHtmlHelpW)
			pHtmlHelpW(NULL, NULL, HH_INITIALIZE, (DWORD)&cookie);
	}
	if (pHtmlHelpW) {
		WCHAR	path[MAX_PATH];

		MakePathW(path, help_dir, help_file);
		if (section)
			wcscpy(path + wcslen(path), section);
		return	pHtmlHelpW(hOwner, path, HH_DISPLAY_TOC, 0);
	}
#endif
	return	NULL;
}

HWND ShowHelpU8(HWND hOwner, const char *help_dir, const char *help_file, const char *section)
{
	Wstr	dir(help_dir);
	Wstr	file(help_file);
	Wstr	sec(section);

	return	ShowHelpW(hOwner, dir.Buf(), file.Buf(), sec.Buf());
}

#ifdef REPLACE_DEBUG_ALLOCATOR

#define VALLOC_SIG 0x12345678
#define ALLOC_ALIGN 4
//#define NON_FREE
#undef malloc
#undef realloc
#undef free

extern "C" {
void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);
}

inline size_t align_size(size_t size, size_t grain) {
	return (size + grain -1) / grain * grain;
}

inline size_t alloc_size(size_t size) {
	return	align_size((align_size(size, ALLOC_ALIGN) + 16 + PAGE_SIZE), PAGE_SIZE);
}
inline void *valloc_base(void *d)
{
	DWORD	org  = (DWORD)d;
	DWORD	base = org & 0xfffff000;

	if (org - base < 16) base -= PAGE_SIZE;

	return	(void *)base;
}
inline size_t valloc_size(void *d)
{
	d = valloc_base(d);

	if (((DWORD *)d)[0] != VALLOC_SIG) {
		return	(size_t)-1;
	}
	return	((size_t *)d)[1];
}


void *valloc(size_t size)
{
	size_t	s = alloc_size(size);
	void	*d = VirtualAlloc(0, s, MEM_RESERVE, PAGE_NOACCESS);

	if (!d || !VirtualAlloc(d, s - PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE)) {
		Debug("valloc error(%x %d %d)\n", d, s, size);
		return NULL;
	}

	((DWORD *)d)[0]  = VALLOC_SIG;
	((size_t *)d)[1] = size;

	Debug("valloc (%x %d %d)\n", d, s, size);

	return (void *)((u_char *)d + s - PAGE_SIZE - align_size(size, ALLOC_ALIGN));
}

void *vcalloc(size_t num, size_t ele)
{
	size_t	size = num * ele;
	void	*d = valloc(size);

	if (d) {
		memset(d, 0, size);
	}
	return	d;
}

void *vrealloc(void *d, size_t size)
{
	size_t	old_size = 0;

	if (d) {
		if ((old_size = valloc_size(d)) == -1) {
			Debug("non vrealloc (%x %d %d)\n", d, old_size, size);
			return realloc(d, size);
		}
		if (size == 0) {
			vfree(d);
			return NULL;
		}
	}

	void	*new_d = valloc(size);

	if (new_d && d) {
		memcpy(new_d, d, min(size, old_size));
	}
	return new_d;
}

void vfree(void *d)
{
	if (!d) return;

	size_t	size = valloc_size(d);

	if (size == -1) {
		Debug("vfree non vfree (%x)\n", d);
		free(d);
		return;
	}
	Debug(" vfree %x %d %d\n", valloc_base(d), alloc_size(size), size);

#ifdef NON_FREE
	VirtualFree(valloc_base(d), alloc_size(size), MEM_DECOMMIT);
#else
	VirtualFree(valloc_base(d), 0, MEM_RELEASE);
#endif
}

char *vstrdup(const char *s)
{
	size_t	size = strlen(s) + 1;
	void	*d = valloc(size);
	if (d) {
		memcpy(d, s, size);
	}
	return	(char *)d;
}

WCHAR *vwcsdup(const WCHAR *s)
{
	size_t	size = (wcslen(s) + 1) * sizeof(WCHAR);
	void	*d = valloc(size);
	if (d) {
		memcpy(d, s, size);
	}
	return	(WCHAR *)d;
}

void *operator new(size_t size)
{
	return	valloc(size);
}

void operator delete(void *d)
{
	vfree(d);
}

#if _MSC_VER >= 1200
void *operator new [](size_t size)
{
	return	valloc(size);
}

void operator delete [](void *d)
{
	vfree(d);
}
#endif

#endif


