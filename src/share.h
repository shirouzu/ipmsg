/*	@(#)Copyright (C) H.Shirouzu 2013-2015   share.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: File Sharing
	Create					: 2013-03-03(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SHARE_H
#define SHARE_H

class FileInfo : public TListObj {
	int			id;
	char		*fname;
	BYTE		*memData;
	UINT		attr;
	int			pos;
	_int64		size;
	Time_t		mtime;
	Time_t		atime;
	Time_t		crtime;
	BOOL		isSelected;		// for recvdlg

public:
	FileInfo(int _id=0) {
		id = _id;
		fname = NULL;
		memData = NULL;
		attr = 0;
		pos = -1;
		size = 0;
		mtime = 
		atime = 0;
		crtime = 0;
		isSelected = FALSE;
	}
	FileInfo(const FileInfo& org) {
		fname	= NULL;
		memData	= NULL;
		*this = org;
	}
	~FileInfo() {
		free(fname);
		free(memData);
	}

	int Id() { return id; }
	void SetId(int _id) { id = _id; }
	const char *Fname() { return fname; }
	void SetFname(const char *_fname) {
		free(fname);
		fname = (char *)strdup(_fname);
	}
	const BYTE *MemData() { return memData; }
	void SetMemData(const BYTE *_memData, _int64 _size) {
		free(memData);
		if ((memData = (BYTE *)malloc((int)_size))) {
			memcpy(memData, _memData, (int)_size);
			size = _size;
		}
	}
	_int64 Size() { return size; }
	void SetSize(_int64 _size) { size = _size; }
	Time_t Mtime() { return mtime; }
	void SetMtime(Time_t _mtime) { mtime = _mtime; }
	Time_t Atime() { return atime; }
	void SetAtime(Time_t _atime) { atime = _atime; }
	Time_t Crtime() { return crtime; }
	void SetCrtime(Time_t _crtime) { crtime = _crtime; }
	UINT Attr() { return attr; }
	void SetAttr(UINT _attr) { attr = _attr; }
	int  Pos() { return pos; }
	void SetPos(UINT _pos) { pos = _pos; }
	BOOL IsSelected() { return isSelected; }
	void SetSelected(BOOL _isSelected) { isSelected = _isSelected; }
	FileInfo& operator =(const FileInfo& org) {
		id = org.id;
		if (org.fname) SetFname(org.fname);
		if (org.memData) SetMemData(org.memData, org.size);
		attr = org.attr;
		pos = org.pos;
		size = org.size;
		mtime = org.mtime;
		atime = org.atime;
		crtime = org.crtime;
		isSelected = org.isSelected;
		return *this;
	}
};

class ShareInfo : public TListObj {
public:
	int			packetNo;		// not use recvdlg
	Host		**host;			// allow host list, not use recvdlg
	int			hostCnt;		// not use recvdlg
	char		*transStat;		// not use recvdlg
	FileInfo	**fileInfo;		// allow file list
	int			fileCnt;
	FILETIME	attachTime;

	ShareInfo(int packetNo=0) {
		Init(packetNo);
	}
	ShareInfo(char *msg, BOOL enable_clip);

	~ShareInfo() {
		while (fileCnt-- > 0) {
			delete fileInfo[fileCnt];
		}
		free(fileInfo);
	}
	void Init(int _packetNo=0) {
		packetNo = _packetNo;
		host = NULL;
		transStat = NULL;
		fileInfo = NULL;
		hostCnt = fileCnt = 0;
		memset(&attachTime, 0, sizeof(attachTime));
	}
	BOOL RemoveFileInfo(int idx) {
		if (idx >= fileCnt) return FALSE;
		delete fileInfo[idx];
		--fileCnt;
		memmove(fileInfo + idx, fileInfo + idx +1, sizeof(FileInfo *) * (fileCnt - idx));
		return	TRUE;
	}
	BOOL EncodeMsg(char *buf, int bufsize, BOOL incMem=FALSE);
};

struct AcceptFileInfo {
	FileInfo	*fileInfo;
	Host		*host;
	_int64		offset;
	int			packetNo;
	UINT		command;
	UINT		logOpt; // decode status
	int			ivPacketNo;
	BYTE		aesKey[256/8];
	FILETIME	attachTime;
};

struct ShareCntInfo {
	int		hostCnt;
	int		fileCnt;
	int		dirCnt;
	int		transferCnt;
	int		doneCnt;
	int		packetCnt;
	_int64	totalSize;
};

class TShareStatDlg;
class SendEntry;

class ShareMng : public TListEx<ShareInfo> {
public:
	enum			{ TRANS_INIT, TRANS_BUSY, TRANS_DONE };
protected:
	TShareStatDlg	*statDlg;
	Cfg				*cfg;
	MsgMng			*msgMng;
	FileInfo		*SetFileInfo(char *fname);
	BOOL			AddShareCore(ShareInfo *info, FileInfo	*fileInfo);

public:
	ShareMng(Cfg *_cfg, MsgMng *_msgMng);
	~ShareMng();
	ShareInfo *CreateShare(int packetNo);
	void	DestroyShare(ShareInfo *info);
	BOOL	AddFileShare(ShareInfo *info, char *fname);
	BOOL	AddMemShare(ShareInfo *info, char *dummy_name, BYTE *data, int size, int pos);
	BOOL	DelFileShare(ShareInfo *info, int fileNo);

	BOOL	EndHostShare(int packetNo, HostSub *hostSub, FileInfo *fileInfo=NULL, BOOL done=TRUE);
	BOOL	AddHostShare(ShareInfo *info, SendEntry *entry, int entryNum);
	ShareInfo	*Search(int packetNo);
	BOOL	GetShareCntInfo(ShareCntInfo *info, ShareInfo *shareInfo=NULL);
	BOOL	GetAcceptableFileInfo(ConnectInfo *info, char *buf, int size, AcceptFileInfo *fileInfo);
	void	RegisterShareStatDlg(TShareStatDlg *_dlg) { statDlg = _dlg; }
	void	Cleanup();
	static int GetFileInfoNo(ShareInfo *info, FileInfo *fileInfo);
};

class TShareDlg : public TDlg {
	ShareMng	*shareMng;
	ShareInfo	*shareInfo;
	Cfg			*cfg;
	BOOL		AddList(int idx);
	BOOL		DelList(int idx);
	TListViewEx	shareListView;

public:
	TShareDlg(ShareMng *_shareMng, ShareInfo *_shareInfo, Cfg *_cfg, TWin *_parent = NULL);
	~TShareDlg();
//	virtual int		Exec(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDropFiles(HDROP hDrop);
static BOOL FileAddDlg(TDlg *dlg, ShareMng *sharMng, ShareInfo *shareInfo, Cfg *cfg);
};

class TSaveCommonDlg : public TDlg {
protected:
	TWin		*parentWin;
	ShareInfo	*shareInfo;
	Cfg			*cfg;
	ULONG		hostStatus;
	int			offset;
	BOOL		isLinkFile;
	BOOL		SetInfo(void);
	BOOL		LumpCheck();
	BOOL		EncTransCheck();

public:
	TSaveCommonDlg(ShareInfo *_shareInfo, Cfg *_cfg, ULONG _hostStatus, TWin *_parentWin);
	virtual int		Exec(void);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void	EndDialog(int) {}	// 無視させる
};

class TShareStatDlg : public TDlg {
	ShareMng	*shareMng;
	Cfg			*cfg;
	BOOL		SetAllList(void);
	BOOL		DelList(int cnt);
	TListViewEx	shareListView;

public:
	TShareStatDlg(ShareMng *_shareMng, Cfg *_cfg, TWin *_parent = NULL);
	~TShareStatDlg();
//	virtual int		Exec(void);
	virtual BOOL	Refresh(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

#endif

