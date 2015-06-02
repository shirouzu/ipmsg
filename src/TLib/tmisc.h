/* @(#)Copyright (C) 1996-2014 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 1996-06-01(Sat)
	Update					: 2014-04-14(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TLIBMISC_H
#define TLIBMISC_H

class THashTbl;

class THashObj {
public:
	THashObj	*prevHash;
	THashObj	*nextHash;
	u_int		hashId;

public:
	THashObj() { prevHash = nextHash = NULL; hashId = 0; }
	virtual ~THashObj() { if (prevHash && prevHash != this) UnlinkHash(); }

	virtual BOOL LinkHash(THashObj *top);
	virtual BOOL UnlinkHash();
	friend THashTbl;
};

class THashTbl {
protected:
	THashObj	*hashTbl;
	int			hashNum;
	int			registerNum;
	BOOL		isDeleteObj;

	virtual BOOL	IsSameVal(THashObj *, const void *val) = 0;

public:
	THashTbl(int _hashNum=0, BOOL _isDeleteObj=TRUE);
	virtual ~THashTbl();
	virtual BOOL	Init(int _hashNum);
	virtual void	UnInit();
	virtual void	Register(THashObj *obj, u_int hash_id);
	virtual void	UnRegister(THashObj *obj);
	virtual THashObj *Search(const void *data, u_int hash_id);
	virtual int		GetRegisterNum() { return registerNum; }
//	virtual u_int	MakeHashId(const void *data) = 0;
};

/* for internal use start */
struct TResHashObj : THashObj {
	void	*val;
	TResHashObj(UINT _resId, void *_val) { hashId = _resId; val = _val; }
	~TResHashObj() { free(val); }
	
};

class TResHash : public THashTbl {
protected:
	virtual BOOL IsSameVal(THashObj *obj, const void *val) {
		return obj->hashId == *(u_int *)val;
	}

public:
	TResHash(int _hashNum) : THashTbl(_hashNum) {}
	TResHashObj	*Search(UINT resId) { return (TResHashObj *)THashTbl::Search(&resId, resId); }
	void		Register(TResHashObj *obj) { THashTbl::Register(obj, obj->hashId); }
};

class Condition {
protected:
	enum WaitEvent { CLEAR_EVENT=0, DONE_EVENT, WAIT_EVENT };
	CRITICAL_SECTION	cs;
	HANDLE				*hEvents;
	WaitEvent			*waitEvents;
	int					max_threads;
	int					waitCnt;

public:
	Condition(void);
	~Condition();

	BOOL Initialize(int _max_threads);
	void UnInitialize(void);

	void Lock(void)		{ ::EnterCriticalSection(&cs); }
	void UnLock(void)	{ ::LeaveCriticalSection(&cs); }

	// ロックを取得してから利用すること
	int  WaitThreads()	{ return waitCnt; }
	int  IsWait()		{ return waitCnt ? TRUE : FALSE; }
	void DetachThread() { max_threads--; }
	int  MaxThreads()   { return max_threads; }

	BOOL Wait(DWORD timeout=INFINITE);
	void Notify(void);
};

#define PAGE_SIZE	(4 * 1024)

class VBuf {
protected:
	BYTE	*buf;
	VBuf	*borrowBuf;
	size_t	size;
	size_t	usedSize;
	size_t	maxSize;
	void	Init();

public:
	VBuf(size_t _size=0, size_t _max_size=0, VBuf *_borrowBuf=NULL);
	~VBuf();
	BOOL	AllocBuf(size_t _size, size_t _max_size=0, VBuf *_borrowBuf=NULL);
	BOOL	LockBuf();
	void	FreeBuf();
	BOOL	Grow(size_t grow_size);
	operator bool() { return buf ? true : false; }
	operator void *() { return buf; }
	operator BYTE *() { return buf; }
	operator char *() { return (char *)buf; }
	BYTE	*Buf() { return	buf; }
	WCHAR	*WBuf() { return (WCHAR *)buf; }
	size_t	Size() { return size; }
	size_t	MaxSize() { return maxSize; }
	size_t	UsedSize() { return usedSize; }
	void	SetUsedSize(size_t _used_size) { usedSize = _used_size; }
	size_t	AddUsedSize(size_t _used_size) { return usedSize += _used_size; }
	size_t	RemainSize(void) { return	size - usedSize; }
};

class GBuf {
protected:
	HGLOBAL	hGlobal;
	BYTE	*buf;
	int		size;
	UINT	flags;

public:
	GBuf(int _size=0, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		Init(_size, with_lock, _flags);
	}
	GBuf(VBuf *vbuf, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		Init((int)vbuf->Size(), with_lock, _flags);
		if (buf) memcpy(buf, vbuf->Buf(), (int)vbuf->Size());
	}
	~GBuf() {
		UnInit();
	}
	BOOL Init(int _size=0, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		hGlobal	= NULL;
		buf		= NULL;
		flags	= _flags;
		if ((size = _size) == 0) return TRUE;
		if (!(hGlobal = ::GlobalAlloc(flags, size))) return FALSE;
		if (!with_lock) return	TRUE;
		return	Lock() ? TRUE : FALSE;
	}
	void UnInit() {
		if (buf && (flags & GMEM_FIXED)) ::GlobalUnlock(buf);
		if (hGlobal) ::GlobalFree(hGlobal);
		buf		= NULL;
		hGlobal	= NULL;
	}
	HGLOBAL	Handle() { return hGlobal; }
	BYTE *Buf() {
		return	buf;
	}
	BYTE *Lock() {
		if ((flags & GMEM_FIXED))	buf = (BYTE *)hGlobal;
		else						buf = (BYTE *)::GlobalLock(hGlobal);
		return	buf;
	}
	void Unlock() {
		if (!(flags & GMEM_FIXED)) {
			::GlobalUnlock(buf);
			buf = NULL;
		}
		
	}
	int	Size() { return size; }
};

class DynBuf {
protected:
	char	*buf;
	int		size;

public:
	DynBuf(int _size=0)	{
		buf = NULL;
		if ((size = _size) > 0) Alloc(size);
	}
	~DynBuf() {
		free(buf);
	}
	char *Alloc(int _size) {
		if (buf) free(buf);
		buf = NULL;
		if ((size = _size) <= 0) return NULL;
		return	(buf = (char *)malloc(size));
	}
	operator char*()	{ return (char *)buf; }
	operator BYTE*()	{ return (BYTE *)buf; }
	operator WCHAR*()	{ return (WCHAR *)buf; }
	operator void*()	{ return (void *)buf; }
	int	Size()			{ return size; }
};

void InitInstanceForLoadStr(HINSTANCE hI);
LPSTR GetLoadStrA(UINT resId, HINSTANCE hI=NULL);
LPSTR GetLoadStrU8(UINT resId, HINSTANCE hI=NULL);
LPWSTR GetLoadStrW(UINT resId, HINSTANCE hI=NULL);
void TSetDefaultLCID(LCID id=0);
HMODULE TLoadLibrary(LPSTR dllname);
HMODULE TLoadLibraryW(WCHAR *dllname);
int MakePath(char *dest, const char *dir, const char *file);
int MakePathW(WCHAR *dest, const WCHAR *dir, const WCHAR *file);

int64 hex2ll(char *buf);
int bin2hexstr(const BYTE *bindata, int len, char *buf);
int bin2hexstr_revendian(const BYTE *bin, int len, char *buf);
int bin2hexstrW(const BYTE *bindata, int len, WCHAR *buf);
BOOL hexstr2bin(const char *buf, BYTE *bindata, int maxlen, int *len);
BOOL hexstr2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len);

int bin2b64str(const BYTE *bindata, int len, char *buf);
int bin2b64str_revendian(const BYTE *bin, int len, char *buf);
BOOL b64str2bin(const char *buf, BYTE *bindata, int maxlen, int *len);
BOOL b64str2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len);

int bin2urlstr(const BYTE *bindata, int len, char *str);
BOOL urlstr2bin(const char *str, BYTE *bindata, int maxlen, int *len);

void rev_order(BYTE *data, int size);
void rev_order(const BYTE *src, BYTE *dst, int size);

char *strdupNew(const char *_s, int max_len=-1);
WCHAR *wcsdupNew(const WCHAR *_s, int max_len=-1);

int strncmpi(const char *str1, const char *str2, size_t num);
char *strncpyz(char *dest, const char *src, size_t num);

int LocalNewLineToUnix(const char *src, char *dest, int maxlen);
int UnixNewLineToLocal(const char *src, char *dest, int maxlen);

BOOL TIsWow64();
BOOL TRegEnableReflectionKey(HKEY hBase);
BOOL TRegDisableReflectionKey(HKEY hBase);
BOOL TWow64DisableWow64FsRedirection(void *oldval);
BOOL TWow64RevertWow64FsRedirection(void *oldval);
BOOL TIsEnableUAC();
BOOL TIsVirtualizedDirW(WCHAR *path);
BOOL TMakeVirtualStorePathW(WCHAR *org_path, WCHAR *buf);
BOOL TSetPrivilege(LPSTR pszPrivilege, BOOL bEnable);
BOOL TSetThreadLocale(int lcid);
BOOL TChangeWindowMessageFilter(UINT msg, DWORD flg);
void TSwitchToThisWindow(HWND hWnd, BOOL flg);

BOOL InstallExceptionFilter(const char *title, const char *info, const char *fname=NULL);
void Debug(char *fmt,...);
void DebugW(WCHAR *fmt,...);
void DebugU8(char *fmt,...);
const char *Fmt(char *fmt,...);
const WCHAR *FmtW(WCHAR *fmt,...);

BOOL SymLinkW(WCHAR *src, WCHAR *dest, WCHAR *arg=L"");
BOOL ReadLinkW(WCHAR *src, WCHAR *dest, WCHAR *arg=NULL);
BOOL DeleteLinkW(WCHAR *path);
BOOL GetParentDirW(const WCHAR *srcfile, WCHAR *dir);
HWND ShowHelpW(HWND hOwner, WCHAR *help_dir, WCHAR *help_file, WCHAR *section=NULL);
HWND ShowHelpU8(HWND hOwner, const char *help_dir, const char *help_file, const char *section=NULL);

#endif

