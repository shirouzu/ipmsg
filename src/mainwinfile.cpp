static char *mainwinfile_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2018   mainwinfile.cpp	Ver4.90";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window File Transfer
	Create					: 1996-06-01(Sat)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "install/instcmn.h"
#include <process.h>

using namespace std;

/*
	TCP Packet 受信処理 (File Transfer & Dir Message)
*/
BOOL TMainWin::RecvTcpMsg(SOCKET sd)
{
	ConnectInfo 	*conInfo = NULL;
	AcceptFileInfo	fileInfo = {};

	for (conInfo=connList.TopObj(); conInfo; conInfo=connList.NextObj(conInfo)) {
		if (conInfo->sd == sd) break;
	}

	if (conInfo == NULL) {
		::closesocket(sd);
		return	FALSE;
	}

	DynBuf	&buf = conInfo->buf;
	int	size = ::recv(conInfo->sd, (char *)buf + buf.UsedSize(), (int)buf.RemainSize() - 1, 0);
	if (size > 0) {
		buf.AddUsedSize(size);
		buf.Buf()[buf.UsedSize()] = 0;

		size_t	dsize = (int)ipdict_size_fetch(buf.Buf(), buf.UsedSize());
		if (dsize > buf.UsedSize()) {
			return	TRUE;	// 後続の受信待ち
		}
	}

	MsgBuf	msg;
	msg.hostSub.addr = conInfo->addr;
	msg.hostSub.portNo = portNo; // TCP用portは使わない

	if (size <= 0 || !msgMng->ResolveMsg(buf, (int)buf.UsedSize(), &msg)) {
		goto END;
	}

	if (msg.ipdict.key_num() == 0) {
		if (msg.command & IPMSG_ENCRYPTOPT) {
			UINT	cryptCapa = 0;
			if (!msgMng->DecryptMsg(&msg, &cryptCapa, &fileInfo.logOpt)) {
				goto END;
			}
		}
	}
	else if (msg.signMode != MsgBuf::SIGN_OK) {
		goto END;
	}
	msgMng->ConnectDone(hWnd, conInfo);	// 非同期メッセージの抑制

	int	cmd = GET_MODE(msg.command);

	switch (cmd) {
	case IPMSG_GETFILEDATA:
	case IPMSG_GETDIRFILES:
		if (!shareMng->GetAcceptableFileInfo(conInfo, &msg, &fileInfo)) {
			goto END;
		}
		StartSendFile(sd, conInfo, &fileInfo);
		break;

	default:
		conInfo->fin = TRUE;
		UdpEventCore(&msg);
		goto END;
	}

	return	TRUE;

END:
	connList.DelObj(conInfo);
	::closesocket(sd);
	delete conInfo;
	return	FALSE;
}


SendFileObj *TMainWin::FindSendFileObj(SOCKET sd)
{
	for (auto obj = sendFileList.TopObj(); obj; obj = sendFileList.NextObj(obj)) {
		if (obj->conInfo->sd == sd) {
			return	obj;
		}
	}
	return	NULL;
}

BOOL TMainWin::CheckConnectInfo(ConnectInfo *conInfo)
{
	return	shareMng->TopObj() ? TRUE : FALSE;	// for authorized connection
}

/*
	ファイル送受信開始処理
*/
BOOL TMainWin::StartSendFile(SOCKET sd, ConnectInfo *conInfo, AcceptFileInfo *fileInfo)
{
	SendFileObj	*obj = new SendFileObj;
	obj->conInfo = conInfo;
	obj->hFile = INVALID_HANDLE_VALUE;
	obj->fileInfo = fileInfo->fileInfo;
	obj->offset = fileInfo->offset;
	obj->packetNo = fileInfo->packetNo;
	obj->host = fileInfo->host;
	obj->command = fileInfo->command;
	obj->conInfo->startTick = obj->conInfo->lastTick = ::GetTick();
	obj->fileCapa = fileInfo->fileCapa;
	obj->attachTime = fileInfo->attachTime;

	if ((fileInfo->logOpt & (LOG_SIGN_OK|LOG_SIGN2_OK))
	 && (GET_OPT(fileInfo->command) & IPMSG_ENCFILEOPT)) {
		if (fileInfo->fileCapa == (IPMSG_AES_256|IPMSG_PACKETNO_IV)) {
			BYTE	nonce[AES_BLOCK_SIZE] = {};
			snprintfz((char *)nonce, sizeof(nonce), "%d", fileInfo->ivPacketNo);
			obj->aes.Init(fileInfo->aesKey, sizeof(fileInfo->aesKey), nonce);
		}
		else if (fileInfo->fileCapa == IPMSG_NOENC_FILEBODY) {
			//
		}
		else {
			return	FALSE;
		}
	}
	if (!obj->vbuf.AllocBuf(cfg->IoBufMax)) return FALSE;

	connList.DelObj(conInfo);
	sendFileList.AddObj(obj);

	BOOL	ret = FALSE;

	if (obj->fileInfo->Attr() == IPMSG_FILE_CLIPBOARD && obj->fileInfo->MemData()) {
		obj->isDir	= FALSE;
		obj->status	= FS_MEMFILESTART;
		ret = TRUE;
	}
	else {
		if (!GetFileInfomationU8(obj->fileInfo->Fname(), &obj->fdata)) {
			return	EndSendFile(obj), FALSE;
		}
		obj->isDir = (obj->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
		obj->status = (obj->isDir || GET_MODE(obj->command) == IPMSG_GETDIRFILES) ?
						FS_DIRFILESTART : FS_TRANSFILE;

		if (*obj->fdata.cFileName == 0) {
			ForcePathToFname(obj->fileInfo->Fname(), obj->fdata.cFileName);
		}

		if (obj->isDir) {
			ret = GET_MODE(obj->command) == IPMSG_GETDIRFILES ? TRUE : FALSE;
			obj->hDir = (HANDLE *)malloc((MAX_PATH_U8/2) * sizeof(HANDLE));
		}
		else {
			if ((cfg->fileTransOpt & FT_STRICTDATE) &&
				FileTime2UnixTime(&obj->fdata.ftLastWriteTime) > obj->attachTime) {
				ret = FALSE, obj->status = FS_COMPLETE;		// 共有情報から消去
			}
			else if (GET_MODE(obj->command) == IPMSG_GETDIRFILES) {
				ret = TRUE;
			}
			else {
				ret = OpenSendFile(obj->fileInfo->Fname(), obj);
			}
		}
	}
	if (!ret) {
		EndSendFile(obj);
		return FALSE;
	}

	UINT	id;	// 使わず（95系で error になるのを防ぐだけ）
	obj->hThread = (HANDLE)~0;	// 微妙な領域を避ける
	if (!(obj->hThread = (HANDLE)_beginthreadex(NULL, 0, SendFileThread, obj, 0, &id))) {
		return	EndSendFile(obj), FALSE;
	}

	return	TRUE;
}

BOOL TMainWin::OpenSendFile(const char *fname, SendFileObj *obj)
{
#define USE_OSCACHE_LIMIT	(4 * 1024 * 1024)
	DWORD	flags = (obj->fdata.nFileSizeHigh > 0 || obj->fdata.nFileSizeLow > USE_OSCACHE_LIMIT) ?
						FILE_FLAG_NO_BUFFERING : 0;

	obj->hFile = CreateFileU8(fname, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|flags, 0);
	if (obj->hFile == INVALID_HANDLE_VALUE) return FALSE;

	DWORD	lowSize, highSize;
	lowSize = ::GetFileSize(obj->hFile, &highSize);
	obj->fileSize = (int64)highSize << 32 | lowSize;

	return	TRUE;
}

BOOL TMainWin::CloseSendFile(SendFileObj *obj)
{
	if (obj == NULL) return	FALSE;

	::CloseHandle(obj->hFile);
	obj->hFile	= INVALID_HANDLE_VALUE;
	obj->offset = 0;

	return	TRUE;
}

UINT WINAPI TMainWin::SendFileThread(void *_sendFileObj)
{
	SendFileObj	*obj = (SendFileObj *)_sendFileObj;
	fd_set		fds;
	fd_set		*rfds = NULL;
	fd_set		*wfds = &fds;
	timeval		tv;
	BOOL		ret = FALSE;
	BOOL		complete_wait = FALSE;
	BOOL		(TMainWin::*SendFileFunc)(SendFileObj*) =
					obj->status == FS_MEMFILESTART ?				&TMainWin::SendMemFile :
					GET_MODE(obj->command) == IPMSG_GETDIRFILES ?	&TMainWin::SendDirFile :
																	&TMainWin::SendFile;
	FD_ZERO(&fds);
	FD_SET(obj->conInfo->sd, &fds);

	for (int waitCnt=0; waitCnt < 180 && obj->hThread; waitCnt++) {
		if (obj->needStop) {
			Debug("SendFileThread: stopRequest %d\n", obj->packetNo);
			break;
		}

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		int		sock_ret;

		if ((sock_ret = ::select((int)obj->conInfo->sd + 1, rfds, wfds, NULL, &tv)) > 0) {
			waitCnt = 0;

			if (complete_wait) {
				// dummy read により、相手側の socket クローズによる EOF を検出
				if (::recv(obj->conInfo->sd, (char *)&ret, sizeof(ret), 0) >= 0) {
					ret = TRUE;
				}
				break;
			}
			else if (!(GetMain()->*SendFileFunc)(obj)) {
				break;
			}
			else if (obj->status == FS_COMPLETE) {
				complete_wait = TRUE;
				rfds = &fds;
				wfds = NULL;
				// 過去β7以前の互換性のため
				if (obj->fileSize == 0) {
					ret = TRUE;
					break;
				}
			}
		}
		else if (sock_ret == 0) {
			FD_ZERO(&fds);
			FD_SET(obj->conInfo->sd, &fds);
		}
		else if (sock_ret == SOCKET_ERROR) {
			break;
		}
	}

	if (obj->isDir) {
		GetMain()->CloseSendFile(obj);
		while (--obj->dirCnt >= 0)
			::FindClose(obj->hDir[obj->dirCnt]);
	}

	obj->status = ret ? FS_COMPLETE : FS_ERROR;
	GetMain()->PostMessage(WM_TCPEVENT, obj->conInfo->sd, FD_CLOSE);

	_endthreadex(0);
	return	0;
}

int MakeDirHeader(SendFileObj *obj, BOOL find)
{
	int					len;
	WIN32_FIND_DATA_U8	*dat = &obj->fdata;
	DWORD				attr = dat->dwFileAttributes, ipmsg_attr;
	char				cFileName[MAX_PATH_U8];
	WCHAR				wbuf[MAX_PATH];

	if (obj->command & IPMSG_UTF8OPT) {
		strncpyz(cFileName, dat->cFileName, sizeof(cFileName));
	}
	else {
		U8toW(dat->cFileName, wbuf, wsizeof(wbuf));
		WtoA(wbuf, cFileName, sizeof(cFileName));
	}

	ipmsg_attr = 
		(!find ? IPMSG_FILE_RETPARENT :
			((attr & FILE_ATTRIBUTE_DIRECTORY) ? IPMSG_FILE_DIR : IPMSG_FILE_REGULAR)) |
		((attr & FILE_ATTRIBUTE_READONLY) ? IPMSG_FILE_RONLYOPT : 0) |
		((attr & FILE_ATTRIBUTE_HIDDEN) ? IPMSG_FILE_HIDDENOPT : 0) |
		((attr & FILE_ATTRIBUTE_SYSTEM) ? IPMSG_FILE_SYSTEMOPT : 0);

	if (find) {
		len = sprintf(obj->header, "0000:%s:%x%08x:%x:%x=%llx:%x=%llx:", cFileName,
				dat->nFileSizeHigh, dat->nFileSizeLow, ipmsg_attr,
				IPMSG_FILE_MTIME, FileTime2UnixTime(&dat->ftLastWriteTime),
				IPMSG_FILE_CREATETIME, FileTime2UnixTime(&dat->ftCreationTime));
	}
	else if (*(int64 *)&dat->ftLastWriteTime) {
		len = sprintf(obj->header, "0000:.:0:%x:%x=%llx:%x=%llx:", ipmsg_attr,
				IPMSG_FILE_MTIME, FileTime2UnixTime(&dat->ftLastWriteTime),
				IPMSG_FILE_CREATETIME, FileTime2UnixTime(&dat->ftCreationTime));
	}
	else {
		len = sprintf(obj->header, "0000:.:0:%x:", ipmsg_attr);
	}
	sprintf(obj->header, "%04x", len);
	obj->header[4] = ':';

	return	len;
}

/*
	ファイル送受信
*/
BOOL TMainWin::SendDirFile(SendFileObj *obj)
{
	BOOL	find = FALSE;

	if (obj->status == FS_OPENINFO) {
		char	buf[MAX_PATH_U8];
		if (obj->dirCnt == 0) {
			strncpyz(buf, obj->fileInfo->Fname(), sizeof(buf));
		}
		else if (MakePathU8(buf, obj->path, *obj->fdata.cAlternateFileName ?
					obj->fdata.cAlternateFileName : obj->fdata.cFileName) >= sizeof(buf)) {
			return	FALSE;
		}
		strncpyz(obj->path, buf, sizeof(obj->path));
		obj->dirCnt++;
		obj->status = FS_FIRSTINFO;
	}

	if (obj->status == FS_FIRSTINFO || obj->status == FS_NEXTINFO) {
		if (obj->status == FS_FIRSTINFO) {
			char	buf[MAX_BUF];
			MakePathU8(buf, obj->path, "*");
			find = (obj->hDir[obj->dirCnt -1] = 
					FindFirstFileU8(buf, &obj->fdata)) == INVALID_HANDLE_VALUE ? FALSE : TRUE;
		}
		else {
			find = FindNextFileU8(obj->hDir[obj->dirCnt -1], &obj->fdata);
		}

		while (find && (strcmp(obj->fdata.cFileName, ".") == 0 ||
						strcmp(obj->fdata.cFileName, "..") == 0)) {
			find = FindNextFileU8(obj->hDir[obj->dirCnt -1], &obj->fdata);
		}
		obj->status = FS_MAKEINFO;
	}

	if (obj->status == FS_DIRFILESTART || obj->status == FS_MAKEINFO) {
		if (obj->status == FS_DIRFILESTART) {
			find = TRUE;
		}
		if (find && (obj->dirCnt > 0 || !obj->isDir) &&
			(obj->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			char	buf[MAX_BUF];
			int		len = obj->isDir ?
							MakePathU8(buf, obj->path, *obj->fdata.cAlternateFileName ?
										obj->fdata.cAlternateFileName : obj->fdata.cFileName) :
							snprintfz(buf, sizeof(buf), "%s", obj->fileInfo->Fname());
			BOOL	is_modify = (cfg->fileTransOpt & FT_STRICTDATE) &&
						FileTime2UnixTime(&obj->fdata.ftLastWriteTime) > obj->attachTime;

			if (len >= MAX_PATH_U8 || is_modify || !OpenSendFile(buf, obj)) {
				len = (int)strlen(obj->fdata.cFileName);
				strncpyz(obj->fdata.cFileName + len, " (Can't open)", MAX_PATH_U8 - len);
				obj->fdata.nFileSizeHigh = obj->fdata.nFileSizeLow = 0;
			}
		}
		if (!find && obj->isDir) {
			GetFileInfomationU8(obj->path, &obj->fdata);
		}
		obj->headerOffset = 0;
		obj->headerLen = MakeDirHeader(obj, find);
		if (!find) {
			if (--obj->dirCnt >= 0 && obj->isDir) {
				::FindClose(obj->hDir[obj->dirCnt]);
				if (!GetParentDirU8(obj->path, obj->path) && obj->dirCnt > 0) {
					return	FALSE;
				}
			}
			if (obj->dirCnt <= 0) {
				obj->dirCnt--;	// 終了
			}
		}
		if (obj->aes.IsKeySet()) {
			obj->aes.EncryptCTR((BYTE *)obj->header, (BYTE *)obj->header, obj->headerLen);
		}
		obj->status = FS_TRANSINFO;
	}

	if (obj->status == FS_TRANSINFO) {
		int	size = ::send(obj->conInfo->sd, obj->header + obj->headerOffset,
							obj->headerLen - obj->headerOffset, 0);
		if (size < 0) {
			return	FALSE;
		}
		else {
			if ((obj->headerOffset += size) < obj->headerLen) {
				return	TRUE;
			}
			obj->status = (obj->dirCnt < 0) ? FS_COMPLETE : !find ? FS_NEXTINFO :
							(obj->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
							FS_OPENINFO : FS_TRANSFILE;
		}
	}

	if (obj->status == FS_TRANSFILE) {
		if (!SendFile(obj)) {
			CloseSendFile(obj);
			return	FALSE;
		}
		else if (obj->status == FS_ENDFILE) {
			CloseSendFile(obj);
			obj->status = obj->isDir ? FS_NEXTINFO : FS_MAKEINFO;
		}
	}
	return	TRUE;
}

/*
	ファイル送受信
*/
BOOL TMainWin::SendFile(SendFileObj *obj)
{
	if (obj == NULL || obj->hFile == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}

	int		size = 0;
	int64	remain = obj->fileSize - obj->offset;
	int64	offset = obj->offset % cfg->IoBufMax;
	int		trans  = cfg->IoBufMax - (int)offset;

	if (remain > 0 && offset == 0) {
		DWORD	rsize=0;
		if (!::ReadFile(obj->hFile, obj->vbuf, trans, &rsize, 0)) {
			return FALSE;
		}
		if (obj->aes.IsKeySet()) {
			obj->aes.EncryptCTR(obj->vbuf, obj->vbuf, rsize);
		}
	}

	if (remain > 0) {
		if ((size = ::send(obj->conInfo->sd, (char *)obj->vbuf + offset,
							(int)(remain > trans ? trans : remain), 0)) <= 0) {
			return	FALSE;
		}
	}

	obj->offset += size;

	if (obj->offset == obj->fileSize) {
		obj->status = (GET_MODE(obj->command) == IPMSG_GETDIRFILES && obj->isDir)
						? FS_ENDFILE : FS_COMPLETE;
	}

	obj->conInfo->lastTick = ::GetTick();

	return	TRUE;
}

/*
	ファイル送受信
*/
BOOL TMainWin::SendMemFile(SendFileObj *obj)
{
	if (!obj || !obj->fileInfo || !obj->fileInfo->MemData()) {
		return	FALSE;
	}

	int			size = 0;
	int64		remain64 = obj->fileInfo->Size() - obj->offset;
	int			remain   = (remain64 > cfg->IoBufMax) ? cfg->IoBufMax : (int)remain64;
	const BYTE	*data    = obj->fileInfo->MemData() + obj->offset;

	if (remain > 0) {
		if (obj->aes.IsKeySet()) {
			if ((obj->offset % cfg->IoBufMax) == 0) {
				obj->aes.EncryptCTR(data, obj->vbuf, remain);
			}
			data = obj->vbuf;
		}
		if ((size = ::send(obj->conInfo->sd, (char *)data, remain, 0)) < 0) {
			return	FALSE;
		}
	}

	obj->offset += size;

	if (obj->offset == obj->fileInfo->Size()) {
		obj->status = FS_COMPLETE;
	}

	obj->conInfo->lastTick = ::GetTick();

	return	TRUE;
}

BOOL TMainWin::EndSendFile(SendFileObj *obj)
{
	if (obj == NULL) return	FALSE;

	if (obj->hThread) {
		HANDLE	hThread = obj->hThread;
		obj->hThread = 0;	// 中断の合図
		::WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hThread);
	}
	if (::closesocket(obj->conInfo->sd) != 0) {
		obj->status = FS_ERROR;	// error 扱いにする
	}

	CloseSendFile(obj);

	if (obj->isDir) {
		free(obj->hDir);
	}

	shareMng->EndHostShare(obj->packetNo, &obj->host->hostSub,
							obj->fileInfo, obj->status == FS_COMPLETE ? TRUE : FALSE);
	sendFileList.DelObj(obj);
	delete obj->conInfo;
	delete obj;
	return	TRUE;
}


void TMainWin::StopSendFile(int packet_no)
{
	for (auto obj = sendFileList.TopObj(); obj; obj = sendFileList.NextObj(obj)) {
		if (obj->packetNo == packet_no) {
			obj->needStop = TRUE;
			Debug("StopSendFile %d\n", packet_no);
		}
	}
}

