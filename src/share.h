/*	@(#)Copyright (C) H.Shirouzu 2013-2017   share.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: File Sharing
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SHARE_H
#define SHARE_H

class SendMsg;

class ShareInfo : public TListObj {
public:
	UINT		packetNo;		// not use recvdlg
	Host		**host;			// allow host list, not use recvdlg
	int			hostCnt;		// not use recvdlg
	char		*transStat;		// not use recvdlg
	UINT		*lastPkt;		// not use recvdlg
	FileInfo	**fileInfo;		// allow file list
	int			fileCnt;		// include clipCnt
	int			clipCnt;
	time_t		attachTime;
	BOOL		needSave;
	enum Flags { SI_NONE=0, SI_FILE=1, SI_CLIP=2, SI_ALL=3 };

	ShareInfo(int packetNo=0) {
		Init(packetNo);
	}
	ShareInfo(char *msg, DWORD flags=SI_ALL);
	ShareInfo(const IPDict &dict, DWORD flags=SI_ALL);

	~ShareInfo() {
		UnInit();
	}
	void Init(int _packetNo=0) {
		packetNo = _packetNo;
		host = NULL;
		transStat = NULL;
		fileInfo = NULL;
		lastPkt = NULL;
		hostCnt = 0;
		fileCnt = 0;
		clipCnt = 0;
		needSave = FALSE;
		attachTime = 0;
	}
	void UnInit() {
		while (fileCnt > 0) {
			delete fileInfo[--fileCnt];
		}
		free(fileInfo);
		fileInfo = NULL;

		delete [] host;
		delete [] lastPkt;
		delete [] transStat;
		host = NULL;
		lastPkt = NULL;
		transStat = NULL;
		needSave = FALSE;
	}

	BOOL RemoveFileInfo(int idx) {
		if (idx >= fileCnt || fileCnt <= 0) return FALSE;
		delete fileInfo[idx];
		--fileCnt;
		memmove(fileInfo + idx, fileInfo + idx +1, sizeof(FileInfo *) * (fileCnt - idx));
		return	TRUE;
	}
	BOOL EncodeMsg(char *buf, int bufsize, IPDictList *dlist, int *share_len);

	BOOL Pack(IPDict *dict);
	BOOL UnPack(Cfg *cfg, const IPDict &dict);
};

struct AcceptFileInfo {
	FileInfo	*fileInfo;
	Host		*host;
	int64		offset;
	int			packetNo;
	UINT		command;
	UINT		logOpt; // decode status
	UINT		fileCapa;
	int			ivPacketNo;
	BYTE		aesKey[256/8];
	time_t		attachTime;
};

struct ShareCntInfo {
	int		hostCnt;
	int		fileCnt;
	int		dirCnt;
	int		transferCnt;
	int		doneCnt;
	int		packetCnt;
	int64	totalSize;
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
	BOOL			AddShareCore(ShareInfo *info, FileInfo	*fileInfo, BOOL isClip=FALSE);
	TListEx<ShareInfo>	delayList;

public:
	ShareMng(Cfg *_cfg, MsgMng *_msgMng);
	~ShareMng();
	ShareInfo *CreateShare(int packetNo);
	void	DestroyShare(ShareInfo *info);
	BOOL	AddFileShare(ShareInfo *info, char *fname, int pos=-1);
	BOOL	AddMemShare(ShareInfo *info, char *dummy_name, BYTE *data, int size, int pos);
	BOOL	DelFileShare(ShareInfo *info, int fileNo);

	BOOL	AddHostShare(ShareInfo *info, std::shared_ptr<SendMsg> sendMsg);
	BOOL	EndHostShare(int packetNo, HostSub *hostSub, FileInfo *fileInfo=NULL, BOOL done=TRUE);

	ShareInfo	*Search(int packetNo);
	ShareInfo	*SearchDelayList(int packetNo);

	BOOL	GetShareCntInfo(ShareCntInfo *info, ShareInfo *shareInfo=NULL);
	BOOL	GetAcceptableFileInfo(ConnectInfo *info, MsgBuf *msg, AcceptFileInfo *fileInfo);
	void	RegisterShareStatDlg(TShareStatDlg *_dlg) { statDlg = _dlg; }
	void	Cleanup();

	void	LoadShare();
	void	SaveShare(ShareInfo *info);

	static int GetFileInfoNo(ShareInfo *info, FileInfo *fileInfo);
};

class TShareDlg : public TDlg {
	ShareMng	*shareMng;
	ShareInfo	*shareInfo;
	TSendDlg	*parentDlg;
	Cfg			*cfg;
	BOOL		AddList(int idx);
	BOOL		DelList(int idx);
	TListViewEx	shareListView;

public:
	TShareDlg(ShareMng *_shareMng, ShareInfo *_shareInfo, Cfg *_cfg, TSendDlg *_parent = NULL);
	~TShareDlg();
//	virtual int		Exec(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDropFiles(HDROP hDrop);
};

BOOL ShareFileAddDlg(TSendDlg *sendDlg, TDlg *parent, ShareMng *sharMng, ShareInfo *shareInfo,
	Cfg *cfg);

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

