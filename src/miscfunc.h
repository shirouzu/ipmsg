/*	@(#)Copyright (C) H.Shirouzu 2013-2014   miscfunc.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc functions
	Create					: 2013-03-03(Sun)
	Update					: 2014-04-14(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MISCFUNC_H
#define MISCFUNC_H

BOOL IsImageInClipboard(HWND hWnd);
BOOL SetFileButton(TDlg *dlg, int buttonID, ShareInfo *info, BOOL isAutoSave=FALSE);
BOOL PrepareBmp(int cx, int cy, int *_aligned_line_size, VBuf *vbuf);
HBITMAP FinishBmp(VBuf *vbuf);

#define MSS_SPACE	0x00000001
#define MSS_NOPOINT	0x00000002
int MakeSizeString(char *buf, _int64 size, int flg=0);

BOOL IsUserNameExt(Cfg *cfg);
BOOL IsValidFileName(char *fname);
const char *GetUserNameDigestField(const char *user);
BOOL VerifyUserNameExtension(Cfg *cfg, MsgBuf *msg);
BOOL GenUserNameDigestVal(const BYTE *key, BYTE *digest);
BOOL GenUserNameDigest(char *org_name, const BYTE *key, char *new_name);
int VerifyUserNameDigest(const char *user, const BYTE *key);

void MakeClipFileName(int id, int pos, BOOL is_send, char *buf);
BOOL MakeImageFolderName(Cfg *, char *dir);
BOOL MakeNonExistFileName(const char *dir, char *path);
BOOL MakeAutoSaveDir(Cfg *cfg, char *dir);
BOOL SaveImageFile(Cfg *cfg, const char *target_fname, VBuf *buf);
VBuf *LoadImageFile(Cfg *cfg, const char *fname);
BOOL LoadImageFile(Cfg *cfg, const char *fname, VBuf *vbuf);


UrlObj *SearchUrlObj(TListEx<UrlObj> *list, char *protocol);
void SetDlgIcon(HWND hWnd);
void MakeListString(Cfg *cfg, HostSub *hostSub, THosts *hosts, char *buf, BOOL is_log=FALSE);
void MakeListString(Cfg *cfg, Host *host, char *buf, BOOL is_log=FALSE);

BOOL CheckPassword(const char *cfgPasswd, const char *inputPasswd);
void MakePassword(const char *inputPasswd, char *outputPasswd);

BOOL SetImeOpenStatus(HWND hWnd, BOOL flg);
BOOL GetImeOpenStatus(HWND hWnd);

BOOL SetHotKey(Cfg *cfg);
BOOL IsSameHost(HostSub *host, HostSub *host2);
inline BOOL IsSameHostEx(HostSub *host, HostSub *host2) {
	return IsSameHost(host, host2) && host->addr == host2->addr &&
			host->portNo == host2->portNo ? TRUE : FALSE;
}

void RectToWinPos(const RECT *rect, WINPOS *wpos);
Time_t Time(void);
const char *Ctime(SYSTEMTIME *st=NULL);
const char *Ctime(Time_t *t);
BOOL BrowseDirDlg(TWin *parentWin, const char *title, const char *defaultDir, char *buf);
BOOL PathToFname(const char *org_path, char *target_fname);
void ForcePathToFname(const char *org_path, char *target_fname);
void ConvertShareMsgEscape(char *str);
BOOL IsSafePath(const char *fullpath, const char *fname);

BOOL GetLastErrorMsg(char *msg=NULL, TWin *win=NULL);
BOOL GetSockErrorMsg(char *msg=NULL, TWin *win=NULL);
int MakePath(char *dest, const char *dir, const char *file);


char *separate_token(char *buf, char separetor, char **handle=NULL);
void MakeHash(const BYTE *data, int len, char *hashStr);
BOOL VerifyHash(const BYTE *data, int len, const char *orgHash);

char *GetLoadStrAsFilterU8(UINT id);
BOOL GetCurrentScreenSize(RECT *rect, HWND hRefWnd = NULL);

void GenRemoteKey(char *key);
int CALLBACK EditNoWordBreakProc(LPTSTR str, int cur, int len, int action);

#endif

