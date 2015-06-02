static char *cfg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   cfg.cpp	Ver3.20";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Configuration
	Create					: 1996-09-27(Sat)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include "resource.h"
#include "ipmsg.h"
#include "blowfish.h"

#define IPMSG_DEFAULT_LISTGETMSEC	3000
#define IPMSG_DEFAULT_RETRYMSEC		1500
#define IPMSG_DEFAULT_RETRYMAX		3
#define IPMSG_DEFAULT_RECVMAX_NT	100
#define IPMSG_DEFAULT_RECVMAX_95	30
#define IPMSG_DEFAULT_ABSENCEMAX	8
#define IPMSG_DEFAULT_FINDMAX		12
#define IPMSG_DEFAULT_DELAY			500
#define IPMSG_DEFAULT_UPDATETIME	10
#define IPMSG_DEFAULT_KEEPHOSTTIME	(3600 * 24 * 180)	// 180日間
#define IPMSG_DEFAULT_QUOTE			">"
#define IPMSG_DEFAULT_ENCRYPTNUM	50
#define IPMSG_DEFAULT_CLIPMODE		7
#define IPMSG_DEFAULT_CLIPMAX		10
#define IPMSG_DEFAULT_LRUUSERMAX	10

#define IPMSG_DEFAULT_VIEWMAX		(1 * 1024 * 1024)	// 1MB
#define IPMSG_DEFAULT_TRANSMAX		(128 * 1024)		// 128KB
#define IPMSG_DEFAULT_TCPBUFMAX		(256 * 1024)		// 256KB
#define IPMSG_DEFAULT_IOBUFMAX		(1024 * 1024)		// 1MB

#define IPMSG_DEFAULT_NICKNAMEWIDTH	97
#define IPMSG_DEFAULT_ABSENCEWIDTH	16
#define IPMSG_DEFAULT_GROUPWIDTH	88
#define IPMSG_DEFAULT_HOSTWIDTH		58
#define IPMSG_DEFAULT_IPADDRWIDTH	110
#define IPMSG_DEFAULT_USERWIDTH		90
#define IPMSG_DEFAULT_PRIORITYWIDTH	21

#define IPMSG_DEFAULT_HWUSER_WIDTH	100
#define IPMSG_DEFAULT_HWODATE_WIDTH	 90
#define IPMSG_DEFAULT_HWSDATE_WIDTH	 10
#define IPMSG_DEFAULT_HWID_WIDTH	 10

#define LCID_KEY			"lcid"
#define NOBEEP_STR			"NoBeep"
#define LISTGET_STR			"ListGet"
#define LISTGETMSEC_STR		"ListGetMSec"
#define RETRYMSEC_STR		"RetryMSec2"
#define RETRYMAX_STR		"RetryMax"
#define RECVMAXNT_STR		"RecvMaxNT"
#define RECVMAX95_STR		"RecvMax95"
#define NOERASE_STR			"NoErase"
#define DEBUG_STR			"Debug"
#define BROADCAST_STR		"BroadCast"
#define CLICKABLEURL_STR	"ClickableUrl"
#define NOPOPUPCHECK_STR	"NoPopup"
#define OPENCHECK_STR		"OpenCheck"
#define ABSENCESAVE_STR		"AbsenceSave"
#define ABSENCECHECK_STR	"AbsenceCheck"
#define ABSENCEMAX_STR		"AbsenceMax"
#define ABSENCESTR_STR		"AbsenceStr"
#define ABSENCEHEAD_STR		"AbsenceHead"
#define FINDMAX_STR			"FindMax2"
#define FINDALL_STR			"FindAll"
#define FINDSTR_STR			"FindStr"
#define PASSWORD_STR		"PasswordStr"
#define PASSWDLOGCHECK_STR	"PasswdLogCheck"
#define DELAYTIME_STR		"DelayTime"
#define QUOTECHECK_STR		"QuoteCheck"
#define SECRETCHECK_STR		"SecretCheck"
#define IPADDRCHECK_STR		"IPAddrCheck2"
#define RECVIPADDRCHECK_STR	"RecvIPAddrCheck"
#define ONECLICKPOPUP_STR	"OneClickPopup2"
#define BALLOONNOTIFY_STR	"BalloonNotify"
#define ABNORMALBTN_STR		"AbnormalButton"
#define DIALUPCHECK_STR		"DialUpCheck"
#define LOGONLOG_STR		"LogonLog"
#define RECVLOGONDISP_STR	"RecvLogonDisp"
#define NICKNAMESTR_STR		"NickNameStr"
#define GROUPNAMESTR_STR	"GroupNameStr"
#define ABSENCENONPOPUP_STR	"AbsenceNonPopup"
#define SORT_STR			"Sort"
#define UPDATETIME_STR		"UpdateTime"
#define KEEPHOSTTIME_STR	"KeepHostTime"
#define MSGMINIMIZE_STR		"MsgMinimize"
#define DEFAULTURL_STR		"DefaultUrl"
#define SHELLEXEC_STR		"ShellExec"
#define EXTENDENTRY_STR		"ExtendEntry"
#define EXTENDBROADCAST_STR	"ExtendBroadcast"
#define QUOTESTR_STR		"QuoteStr"
#define CONTROLIME_STR		"ControlIME2"
#define HOTKEY_STR			"HotKey"
#define HOTKEYCHECK_STR		"HotKeyCheck"
#define HOTKEYMODIFY_STR	"HotKeyModify"
#define HOTKEYSEND_STR		"HotKeySend"
#define HOTKEYRECV_STR		"HotKeyRecv"
#define HOTKEYMISC_STR		"HotKeyMisc"
#define ALLOWSENDLIST_STR	"AllowSendList"
#define FILETRANSOPT_STR	"FileTransOpt"
#define RESOLVEOPT_STR		"ResolveOpt"
#define ENCRYPTNUM_STR		"EncryptNum2"
#define CLIPMODE_STR		"ClipMode"
#define CLIPMAX_STR			"ClipMax"

#define VIEWMAX_STR			"ViewMax2"
#define TRANSMAX_STR		"TransMax"
#define TCPBUFMAX_STR		"TcpbufMax"
#define IOBUFMAX_STR		"IoBufMax"
#define LUMPCHECK_STR		"LumpCheck"

#define LOGCHECK_STR		"LogCheck"
#define LOGUTF8_STR			"LogUTF8"
#define LOGFILE_STR			"LogFile"
#define SOUNDFILE_STR		"SoundFile"
#define ICON_STR			"Icon"
#define REVICON_STR			"RevIcon"
#define LASTOPEN_STR		"lastOpen"
#define LASTSAVE_STR		"lastSave"
#define LRUUSERMAX_STR		"lruUserMax"
#define LRUUSERKEY_STR		"lruUser"

#define WINSIZE_STR			"WindowSize"

#define SENDPRIORITY_STR	"SendPriority"
#define SENDNICKNAME_STR	"SendNickName"
#define SENDABSENCE_STR		"SendAbsence"
#define SENDUSERNAME_STR	"SendUserName"
#define SENDGROUPNAME_STR	"SendGroupName"
#define SENDHOSTNAME_STR	"SendHostName"
#define SENDIPADDR_STR		"SendIPAddr"
#define SENDORDER_STR		"SendOrder"

#define SENDXDIFF_STR		"SendXdiff"
#define SENDYDIFF_STR		"SendYdiff"
#define SENDMIDYDIFF_STR	"SendMidYdiff"
#define SENDSAVEPOS_STR		"SendSavePos"
#define SENDXPOS_STR		"SendXpos"
#define SENDYPOS_STR		"SendYpos"

#define RECVXDIFF_STR		"RecvXdiff"
#define RECVYDIFF_STR		"RecvYdiff"
#define RECVSAVEPOS_STR		"RecvSavePos"
#define RECVXPOS_STR		"RecvXpos"
#define RECVYPOS_STR		"RecvYpos"

#define HISTXDIFF_STR		"HistXdiff"
#define HISTYDIFF_STR		"HistYdiff"
#define HISTUSER_STR		"HistUser"
#define HISTODATE_STR		"HistODate"
#define HISTID_STR			"HistId2"
#define HISTSDATE_STR		"HistSDate2"


#define FONT_STR			"Fonts"
#define SENDEDITFONT_STR	"SendEdit"
#define SENDLISTFONT_STR	"SendListView"
#define RECVHEADFONT_STR	"RecvHead"
#define RECVEDITFONT_STR	"RecvEdit"

#define HEIGHT_STR			"Height"
#define WIDTH_STR			"Width"
#define ESCAPEMENT_STR		"Escapement"
#define ORIENTATION_STR		"Orientation"
#define WEIGHT_STR			"Weight"
#define ITALIC_STR			"Italic"
#define UNDERLINE_STR		"UnderLine"
#define STRIKEOUT_STR		"StrikeOut"
#define CHARSET_STR			"CharSet"
#define OUTPRECISION_STR	"OutPrecision"
#define CLIPPRECISION_STR	"ClipPrecision"
#define QUALITY_STR			"Quality"
#define PITCHANDFAMILY_STR	"PitchAndFamily"
#define FACENAME_STR		"FaceName"

#define HOSTINFO_STR		"HostInfo"
#define USERNAME_STR		"UserName"
#define HOSTNAME_STR		"HostName"
#define IPADDR_STR			"IPAddr"
#define PORTNO_STR			"PortNo"
#define NICKNAME_STR		"NickName"
#define GROUPNAME_STR		"GroupName"
#define ALTERNAME_STR		"AlterName"
#define PRIORITY_STR		"Priority"
#define PRIORITYMAX_STR		"PriorityMax"
#define PRIORITYSOUND_STR	"PrioritySound"
#define PRIORITYREJECT_STR	"PriorityReject"
#define PUBLICKEY_STR		"PublicKey"

#define GRIDLINE_STR		"GlidLine" // typo...
#define COLUMNITEMS_STR		"ColumnItems"

#define CRYPT_STR			"Crypt"
#define CRYPT2_STR			"Crypt2"
#define PRIVBLOB_STR		"PrivBlob"
#define PRIVTYPE_STR		"PrivEncryptType"
#define PRIVSEED_STR		"PrivEncryptSeed"
#define PRIVSEEDLEN_STR		"PrivEncryptSeedLen"

char	*DefaultUrlProtocol[] = { "HTTP", "HTTPS", "FTP", "FILE", "TELNET", NULL };
char	*DefaultAbsence[IPMSG_DEFAULT_ABSENCEMAX];
char	*DefaultAbsenceHead[IPMSG_DEFAULT_ABSENCEMAX];

Cfg::Cfg(ULONG _nicAddr, int _portNo)
{
	nicAddr = _nicAddr;
	portNo = _portNo;
	AbsenceHead = NULL;
	AbsenceStr = NULL;
	FindStr = NULL;

	int	i;

	int	abs_ids[]  = { IDS_DEFABSENCE1, IDS_DEFABSENCE2, IDS_DEFABSENCE3, IDS_DEFABSENCE4, IDS_DEFABSENCE5, IDS_DEFABSENCE6, IDS_DEFABSENCE7, IDS_DEFABSENCE8, 0 };
	for (i=0; abs_ids[i] && i < sizeof(DefaultAbsence) / sizeof(char *); i++) {
		DefaultAbsence[i] = GetLoadStrU8(abs_ids[i]);
	}

	int	absh_ids[] = { IDS_DEFABSENCEHEAD1, IDS_DEFABSENCEHEAD2, IDS_DEFABSENCEHEAD3, IDS_DEFABSENCEHEAD4, IDS_DEFABSENCEHEAD5, IDS_DEFABSENCEHEAD6, IDS_DEFABSENCEHEAD7, IDS_DEFABSENCEHEAD8, 0 };
	for (i=0; absh_ids[i] && i < sizeof(DefaultAbsenceHead) / sizeof(char *); i++) {
		DefaultAbsenceHead[i] = GetLoadStrU8(absh_ids[i]);
	}

	char	buf[MAX_PATH_U8], path[MAX_PATH_U8], *fname = NULL;
	GetModuleFileNameU8(NULL, buf, MAX_PATH_U8);
	GetFullPathNameU8(buf, MAX_PATH, path, &fname);
	fname[-1] = 0; // remove '\\'
	execDir = strdup(path);

	memset(pub, 0, sizeof(pub));
	memset(priv, 0, sizeof(priv));
}

Cfg::~Cfg()
{
	for (int i=KEY_1024; i < MAX_KEY; i++) {
		delete [] priv[i].blob;
	}
	delete [] FindStr;
	delete [] AbsenceHead;
	delete [] AbsenceStr;
	free(execDir);
}

BOOL Cfg::ReadRegistry(void)
{
	char	buf[MAX_BUF], val[MAX_BUF];
	int		i;

	GetRegName(buf, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, buf);

	if (!reg.GetInt(NOPOPUPCHECK_STR, &NoPopupCheck))
	{
		GetRegName(buf, 0, portNo);
		reg.ChangeApp(HS_TOOLS, buf);
		if (!reg.GetInt(NOPOPUPCHECK_STR, &NoPopupCheck))
			reg.ChangeApp(HS_TOOLS, GetLoadStr(IDS_IPMSG));
	}

	lcid = -1;
	NoBeep = FALSE;
	ListGet = FALSE;
	HotKeyCheck = FALSE;
	HotKeyModify = MOD_ALT|MOD_CONTROL;
	HotKeySend = 'S';
	HotKeyRecv = 'R';
	HotKeyMisc = 'D';
	ListGetMSec = IPMSG_DEFAULT_LISTGETMSEC;
	RetryMSec = IPMSG_DEFAULT_RETRYMSEC;
	RetryMax = IPMSG_DEFAULT_RETRYMAX;
	RecvMax = IsWinNT() ? IPMSG_DEFAULT_RECVMAX_NT : IPMSG_DEFAULT_RECVMAX_95;
	NoErase = FALSE;
	Debug = FALSE;
	NoPopupCheck = TRUE;
	OpenCheck = 1;
	AbsenceSave = FALSE;
	AbsenceCheck = FALSE;
	AbsenceMax = IPMSG_DEFAULT_ABSENCEMAX;
	AbsenceChoice = 0;
	FindMax = IPMSG_DEFAULT_FINDMAX;
	FindAll = TRUE;
	DelayTime = IPMSG_DEFAULT_DELAY;
	QuoteCheck = TRUE;
	SecretCheck = TRUE;
	LogonLog = TRUE;
	RecvLogonDisp = FALSE;
	IPAddrCheck = TRUE;
	RecvIPAddr = TRUE;
	OneClickPopup = TRUE;
	BalloonNotify = TRUE;
	AbnormalButton = FALSE;
	DialUpCheck = FALSE;
	*NickNameStr = 0;
	*GroupNameStr = 0;
	AbsenceNonPopup = TRUE;
	Sort = IPMSG_NAMESORT;
	UpdateTime = IPMSG_DEFAULT_UPDATETIME;
	KeepHostTime = IPMSG_DEFAULT_KEEPHOSTTIME;
	DefaultUrl = TRUE;
	ShellExec = FALSE;
	ExtendEntry = TRUE;
	ExtendBroadcast = 1;
	ControlIME = 1;
	GridLineCheck = TRUE;
	PriorityMax = DEFAULT_PRIORITYMAX;
	PriorityReject = 0;
	AllowSendList = TRUE;
	fileTransOpt = 0;
	ResolveOpt = 0;
	EncryptNum = IPMSG_DEFAULT_ENCRYPTNUM;
	ClipMode = IPMSG_DEFAULT_CLIPMODE;
	ClipMax = IPMSG_DEFAULT_CLIPMAX;
	ViewMax = IPMSG_DEFAULT_VIEWMAX;
	TransMax = IPMSG_DEFAULT_TRANSMAX;
	TcpbufMax = IPMSG_DEFAULT_TCPBUFMAX;
	IoBufMax = IPMSG_DEFAULT_IOBUFMAX;
	LumpCheck = FALSE;
	lruUserMax = IPMSG_DEFAULT_LRUUSERMAX;

	// CryptProtectData is available only Win2K/XP
	for (i=0; i < MAX_KEY; i++) {
		priv[i].encryptType = IsWin2K() ? PRIV_BLOB_DPAPI : PRIV_BLOB_RAW;
	}

	strncpyz(QuoteStr, IPMSG_DEFAULT_QUOTE, sizeof(QuoteStr));

	SendWidth[SW_NICKNAME] = IPMSG_DEFAULT_NICKNAMEWIDTH;
	SendWidth[SW_ABSENCE] = IPMSG_DEFAULT_ABSENCEWIDTH;
	SendWidth[SW_GROUP] = IPMSG_DEFAULT_GROUPWIDTH;
	SendWidth[SW_HOST] = IPMSG_DEFAULT_HOSTWIDTH;
	SendWidth[SW_IPADDR] = IPMSG_DEFAULT_IPADDRWIDTH;
	SendWidth[SW_USER] = IPMSG_DEFAULT_USERWIDTH;
	SendWidth[SW_PRIORITY] = IPMSG_DEFAULT_PRIORITYWIDTH;

	SetItem(&ColumnItems, SW_NICKNAME, TRUE);
	SetItem(&ColumnItems, SW_GROUP, TRUE);
	SetItem(&ColumnItems, SW_HOST, TRUE);

	for (i=0; i < MAX_SENDWIDTH; i++)
		SendOrder[i] = i;

	SendXdiff		= 0;
	SendYdiff		= 0;
	SendMidYdiff	= 0;
	SendSavePos		= 0;
	SendXpos		= 0;
	SendYpos		= 0;

	RecvXdiff		= 0;
	RecvYdiff		= 0;
	RecvSavePos		= 0;
	RecvXpos		= 0;
	RecvYpos		= 0;

	HistWidth[HW_USER]  = IPMSG_DEFAULT_HWUSER_WIDTH;
	HistWidth[HW_ODATE] = IPMSG_DEFAULT_HWODATE_WIDTH;
	HistWidth[HW_SDATE] = IPMSG_DEFAULT_HWSDATE_WIDTH;
	HistWidth[HW_ID]    = IPMSG_DEFAULT_HWID_WIDTH;
	HistXdiff		= 0;
	HistYdiff		= 0;

	LogCheck = FALSE;
	LogUTF8 = TRUE;
	PasswdLogCheck = FALSE;
	*SoundFile = 0;
	*IconFile = 0;
	*RevIconFile = 0;
	*lastSaveDir = *lastOpenDir = 0;
	memset(&SendEditFont, 0, sizeof(SendEditFont));
	memset(&SendListFont, 0, sizeof(SendListFont));
	memset(&RecvHeadFont, 0, sizeof(RecvHeadFont));
	memset(&RecvEditFont, 0, sizeof(RecvEditFont));

	reg.GetInt(LCID_KEY, &lcid);
	reg.GetInt(NOBEEP_STR, &NoBeep);
	reg.GetInt(LISTGET_STR, &ListGet);
	reg.GetInt(LISTGETMSEC_STR, (int *)&ListGetMSec);
	reg.GetInt(RETRYMSEC_STR, (int *)&RetryMSec);
	reg.GetInt(RETRYMAX_STR, (int *)&RetryMax);
	reg.GetInt(IsWinNT() ? RECVMAXNT_STR : RECVMAX95_STR, (int *)&RecvMax);
	reg.GetInt(NOERASE_STR, &NoErase);
	reg.GetInt(DEBUG_STR, &Debug);
	reg.GetInt(NOPOPUPCHECK_STR, &NoPopupCheck);
	reg.GetInt(OPENCHECK_STR, &OpenCheck);
	reg.GetInt(ABSENCESAVE_STR, &AbsenceSave);
	if (AbsenceSave)
		reg.GetInt(ABSENCECHECK_STR, &AbsenceCheck);
	reg.GetInt(ABSENCEMAX_STR, &AbsenceMax);
	reg.GetInt(ALLOWSENDLIST_STR, &AllowSendList);
	reg.GetInt(FILETRANSOPT_STR, &fileTransOpt);
	reg.GetInt(RESOLVEOPT_STR, &ResolveOpt);
	reg.GetInt(ENCRYPTNUM_STR, &EncryptNum);
	reg.GetInt(CLIPMODE_STR, &ClipMode);
	reg.GetInt(CLIPMAX_STR, &ClipMax);

// for File Transfer
	reg.GetInt(VIEWMAX_STR, &ViewMax);
	if (ViewMax < 1024 * 1024)	// 1MB 以下の MapFileOfView は認めない
		ViewMax = IPMSG_DEFAULT_VIEWMAX;
	reg.GetInt(TRANSMAX_STR, &TransMax);
	if (TransMax < 8 * 1024)	// 8KB 以下の file transfer buf は認めない
		TransMax = IPMSG_DEFAULT_TRANSMAX;
	reg.GetInt(TCPBUFMAX_STR, &TcpbufMax);
	if (TcpbufMax < 8 * 1024)	// 8KB 以下の sockbuf は認めない
		TcpbufMax = IPMSG_DEFAULT_TCPBUFMAX;
	reg.GetInt(IOBUFMAX_STR, &IoBufMax);
	if (IoBufMax < TransMax)	// transmax 以下の IO/Buf は認めない
		IoBufMax = TransMax;

	reg.GetInt(LUMPCHECK_STR, &LumpCheck);

// for Absence Message
	typedef char MaxBuf[MAX_PATH_U8];
	AbsenceStr = new MaxBuf[AbsenceMax];
	memset(AbsenceStr, 0, MAX_PATH_U8 * AbsenceMax);
	reg.GetStr(ABSENCESTR_STR, AbsenceStr[0], sizeof(AbsenceStr[0]));

	typedef char MaxHead[MAX_NAMEBUF];
	AbsenceHead = new MaxHead[AbsenceMax];
	memset(AbsenceHead, 0, MAX_NAMEBUF * AbsenceMax);

	if (reg.CreateKey(ABSENCESTR_STR))
	{
		for (i=0; i < AbsenceMax; i++)
		{
			char	key[MAX_PATH_U8];

			wsprintf(key, "%s%d", ABSENCESTR_STR, i);
			if (!reg.GetStr(key, AbsenceStr[i], sizeof(AbsenceStr[i])))
				strncpyz(AbsenceStr[i], DefaultAbsence[i < IPMSG_DEFAULT_ABSENCEMAX ? i : 0], MAX_PATH_U8);

			wsprintf(key, "%s%d", ABSENCEHEAD_STR, i);
			if (!reg.GetStr(key, AbsenceHead[i], sizeof(AbsenceHead[i])))
				strcpy(AbsenceHead[i], DefaultAbsenceHead[i < IPMSG_DEFAULT_ABSENCEMAX ? i : 0]);
		}
		reg.CloseKey();
	}

// Find User
	typedef char MaxFind[MAX_NAMEBUF];
	if (reg.CreateKey(FINDSTR_STR))
	{
		reg.GetInt(FINDMAX_STR, &FindMax);
		reg.GetInt(FINDALL_STR, &FindAll);
		FindStr = new MaxFind[FindMax];
		for (i=0; i < FindMax; i++)
		{
			char	key[MAX_PATH_U8];
			wsprintf(key, "%d", i);
			if (!reg.GetStr(key, FindStr[i], sizeof(FindStr[i])))
				FindStr[i][0] = '\0';
		}
		reg.CloseKey();
	}
	else {
		FindStr = new MaxFind[FindMax];
		memset(FindStr, 0, MAX_NAMEBUF * FindMax);
	}

	reg.GetInt(LRUUSERMAX_STR, &lruUserMax);
	if (reg.OpenKey(LRUUSERKEY_STR)) {
		for (i=0; ; i++) {
			wsprintf(buf, "%d", i);
			if (!reg.OpenKey(buf)) break;
			UsersObj *obj = new UsersObj;
			for (int j=0; ; j++) {
				wsprintf(buf, "%d", j);
				if (!reg.OpenKey(buf)) break;
				UserObj *u = new UserObj;
				if (reg.GetStr(USERNAME_STR, u->hostSub.userName, sizeof(u->hostSub.userName)) &&
					reg.GetStr(HOSTNAME_STR, u->hostSub.hostName, sizeof(u->hostSub.userName))) {
					obj->users.AddObj(u);
				}
				reg.CloseKey();
			}
			reg.CloseKey();
			lruUserList.AddObj(obj);
		}
		reg.CloseKey();
	}

	*PasswordStr = 0;
	reg.GetStr(PASSWORD_STR, PasswordStr, sizeof(PasswordStr));
	reg.GetInt(PASSWDLOGCHECK_STR, &PasswdLogCheck);
	reg.GetInt(DELAYTIME_STR, &DelayTime);
	reg.GetInt(QUOTECHECK_STR, &QuoteCheck);
	reg.GetInt(SECRETCHECK_STR, &SecretCheck);
	reg.GetInt(LOGONLOG_STR, &LogonLog);
	reg.GetInt(RECVLOGONDISP_STR, &RecvLogonDisp);
	reg.GetInt(IPADDRCHECK_STR, &IPAddrCheck);
	reg.GetInt(RECVIPADDRCHECK_STR, &RecvIPAddr);
	reg.GetInt(ONECLICKPOPUP_STR, &OneClickPopup);
	reg.GetInt(BALLOONNOTIFY_STR, &BalloonNotify);
	reg.GetInt(ABNORMALBTN_STR, &AbnormalButton);
	reg.GetInt(DIALUPCHECK_STR, &DialUpCheck);
	reg.GetInt(ABSENCENONPOPUP_STR, &AbsenceNonPopup);
	reg.GetStr(NICKNAMESTR_STR, NickNameStr, sizeof(NickNameStr));
	reg.GetStr(GROUPNAMESTR_STR, GroupNameStr, sizeof(GroupNameStr));
	reg.GetLong(SORT_STR, (long *)&Sort);
//	reg.GetInt(UPDATETIME_STR, &UpdateTime);
	reg.GetInt(KEEPHOSTTIME_STR, &KeepHostTime);
	reg.GetInt(DEFAULTURL_STR, &DefaultUrl);
	reg.GetInt(SHELLEXEC_STR, &ShellExec);
	reg.GetInt(EXTENDENTRY_STR, &ExtendEntry);
	reg.GetInt(EXTENDBROADCAST_STR, &ExtendBroadcast);
	reg.GetInt(CONTROLIME_STR, &ControlIME);
	reg.GetInt(GRIDLINE_STR, &GridLineCheck);
	reg.GetInt(COLUMNITEMS_STR, (int *)&ColumnItems);
	reg.GetStr(QUOTESTR_STR, QuoteStr, sizeof(QuoteStr));
	if (reg.CreateKey(HOTKEY_STR))
	{
		reg.GetInt(HOTKEYCHECK_STR, &HotKeyCheck);
		reg.GetInt(HOTKEYMODIFY_STR, &HotKeyModify);
		reg.GetInt(HOTKEYSEND_STR, &HotKeySend);
		reg.GetInt(HOTKEYRECV_STR, &HotKeyRecv);
		reg.GetInt(HOTKEYMISC_STR, &HotKeyMisc);
		reg.CloseKey();
	}
	reg.GetInt(LOGCHECK_STR, &LogCheck);
	reg.GetInt(LOGUTF8_STR, &LogUTF8);
	if (!reg.GetStr(LOGFILE_STR, LogFile, sizeof(LogFile)) || !*LogFile)
		strncpyz(LogFile, "ipmsg.log", sizeof(LogFile));
	LogMng::StrictLogFile(LogFile);

	reg.GetStr(SOUNDFILE_STR, SoundFile, sizeof(SoundFile));
	reg.GetStr(ICON_STR, IconFile, sizeof(IconFile));
	reg.GetStr(REVICON_STR, RevIconFile, sizeof(RevIconFile));
	reg.GetStr(LASTOPEN_STR, lastOpenDir, sizeof(lastOpenDir));
	reg.GetStr(LASTSAVE_STR, lastSaveDir, sizeof(lastSaveDir));

// Send/Recv Window Location
	if (reg.CreateKey(WINSIZE_STR))
	{
		reg.GetInt(SENDNICKNAME_STR, &SendWidth[SW_NICKNAME]);
		reg.GetInt(SENDUSERNAME_STR, &SendWidth[SW_USER]);
		reg.GetInt(SENDABSENCE_STR, &SendWidth[SW_ABSENCE]);
		reg.GetInt(SENDPRIORITY_STR, &SendWidth[SW_PRIORITY]);
		reg.GetInt(SENDGROUPNAME_STR, &SendWidth[SW_GROUP]);
		reg.GetInt(SENDHOSTNAME_STR, &SendWidth[SW_HOST]);
		reg.GetInt(SENDIPADDR_STR, &SendWidth[SW_IPADDR]);
		if (reg.CreateKey(SENDORDER_STR))
		{
			for (i=0; i < MAX_SENDWIDTH; i++)
			{
				wsprintf(buf, "%d", i);
				reg.GetInt(buf, &SendOrder[i]);
			}
			reg.CloseKey();
		}
		for (i=0; i < MAX_SENDWIDTH; i++)
		{
			if (SendOrder[i] >= MAX_SENDWIDTH || SendOrder[i] < 0)
				break;
			int i2;
			for (i2=i+1; i2 < MAX_SENDWIDTH; i2++)
				if (SendOrder[i] == SendOrder[i2])
					break;
			if (i2 != MAX_SENDWIDTH)
				break;
		}
		if (i != MAX_SENDWIDTH)
			for (i=0; i < MAX_SENDWIDTH; i++)
				SendOrder[i] = i;

		reg.GetInt(SENDXDIFF_STR, &SendXdiff);
		reg.GetInt(SENDYDIFF_STR, &SendYdiff);
		reg.GetInt(SENDMIDYDIFF_STR, &SendMidYdiff);
		reg.GetInt(SENDSAVEPOS_STR, &SendSavePos);
		reg.GetInt(SENDXPOS_STR, &SendXpos);
		reg.GetInt(SENDYPOS_STR, &SendYpos);

		reg.GetInt(RECVXDIFF_STR, &RecvXdiff);
		reg.GetInt(RECVYDIFF_STR, &RecvYdiff);
		reg.GetInt(RECVSAVEPOS_STR, &RecvSavePos);
		reg.GetInt(RECVXPOS_STR, &RecvXpos);
		reg.GetInt(RECVYPOS_STR, &RecvYpos);

		reg.GetInt(HISTXDIFF_STR, &HistXdiff);
		reg.GetInt(HISTYDIFF_STR, &HistYdiff);
		reg.GetInt(HISTUSER_STR, &HistWidth[HW_USER]);
		reg.GetInt(HISTODATE_STR, &HistWidth[HW_ODATE]);
		reg.GetInt(HISTSDATE_STR, &HistWidth[HW_SDATE]);
		reg.GetInt(HISTID_STR, &HistWidth[HW_ID]);

		reg.CloseKey();
	}

// Send/Recv Window Font
	if (reg.CreateKey(FONT_STR))
	{
		ReadFontRegistry(&reg, SENDEDITFONT_STR, &SendEditFont);
		ReadFontRegistry(&reg, SENDLISTFONT_STR, &SendListFont);
		ReadFontRegistry(&reg, RECVHEADFONT_STR, &RecvHeadFont);
		ReadFontRegistry(&reg, RECVEDITFONT_STR, &RecvEditFont);
		reg.CloseKey();
	}

// Broadcast Data
	if (reg.CreateKey(BROADCAST_STR))
	{
		i = 0;
		while (1)
		{
			wsprintf(buf, "%d", i++);
			if (!reg.GetStr(buf, val, sizeof(val)))
				break;
			brList.SetHostRaw(val, (ResolveOpt & RS_REALTIME) ? 0 : ResolveAddr(val));
		}
		reg.CloseKey();
	}

// Clickable URL
	for (i=0; DefaultUrlProtocol[i]; i++)
	{
		UrlObj *obj = new UrlObj;
		strncpyz(obj->protocol, DefaultUrlProtocol[i], sizeof(obj->protocol));
		*obj->program = 0;
		urlList.AddObj(obj);
	}

	if (reg.CreateKey(CLICKABLEURL_STR))
	{
		for (i = 0; reg.EnumValue(i, buf, sizeof(buf)) || reg.EnumKey(i, buf, sizeof(buf)); i++)
		{
			UrlObj *obj;
			for (obj = (UrlObj *)urlList.TopObj(); obj; obj = (UrlObj *)urlList.NextObj(obj))
				if (stricmp(obj->protocol, buf) == 0)
					break;

			if (obj == NULL)
			{
				obj = new UrlObj;
				urlList.AddObj(obj);
			}
			strncpyz(obj->protocol, buf, sizeof(obj->protocol));
			reg.GetStr(buf, obj->program, sizeof(obj->program));
		}
		reg.CloseKey();
	}

// User Sort Priority
	if (reg.CreateKey(PRIORITY_STR))
	{
		reg.GetInt(PRIORITYMAX_STR, &PriorityMax);
		reg.GetInt(PRIORITYREJECT_STR, &PriorityReject);
#if 0
		PrioritySound = new char *[PriorityMax];
		for (int i=0; i < PriorityMax; i++)
		{
			wsprintf(buf, "%d", i);

			if (reg.CreateKey(buf))
			{
				if (reg.GetStr(PRIORITYSOUND_STR, buf, sizeof(buf)) && *buf != '\0')
					PrioritySound[i] = strdup(buf);
				reg.CloseKey();
			}
		}
#endif
		reg.CloseKey();
	}

	if (reg.CreateKey(HOSTINFO_STR))
	{
		int		i = 0, priority = DEFAULT_PRIORITY;
		Host	*host;
		Time_t	default_time = Time() - KeepHostTime / 2;	// 90 日
		BOOL	rewriteFlg = FALSE;

		priorityHosts.Enable(THosts::NAME, TRUE);
		fileHosts.Enable(THosts::NAME_ADDR, TRUE);
		for (i = 0; reg.EnumValue(i, buf, sizeof(buf)) || reg.EnumKey(i, buf, sizeof(buf)); i++)
		{
			if (!reg.OpenKey(buf))
				break;

			BYTE	pubkey[512];
			int		pubkeySize = sizeof(pubkey);

			if (!reg.GetByte(PUBLICKEY_STR, pubkey, &pubkeySize)) pubkeySize = 0;

			if ((!reg.GetInt(PRIORITY_STR, &priority) || priority == DEFAULT_PRIORITY)
					&& pubkeySize == 0) {
				rewriteFlg = TRUE;
			}
			else {
				host = new Host;
				host->priority = priority;
				reg.GetStr(USERNAME_STR, host->hostSub.userName, MAX_NAMEBUF);
				reg.GetStr(HOSTNAME_STR, host->hostSub.hostName, MAX_NAMEBUF);
				reg.GetLong(IPADDR_STR, (long *)&host->hostSub.addr);
				reg.GetInt(PORTNO_STR, &host->hostSub.portNo);
				reg.GetStr(NICKNAME_STR, host->nickName, MAX_NAMEBUF);
				reg.GetStr(GROUPNAME_STR, host->groupName, MAX_NAMEBUF);
				reg.GetStr(ALTERNAME_STR, host->alterName, MAX_NAMEBUF);
				if (!reg.GetLong(UPDATETIME_STR, (long *)&host->updateTime)) {
					host->updateTime = default_time;
				}
				if (pubkeySize) {
					host->pubKey.UnSerialize(pubkey, pubkeySize);
// 古いエントリを削除にする処理は、一旦、様子見
//					char	*p = (char *)GetUserNameDigestField(host->hostSub.userName);
//					if (p) {
//						*p = 0;
//						Host *tmpHost = priorityHosts.GetHostByName(&host->hostSub);
//						if (tmpHost) {
//							priorityHosts.DelHost(tmpHost);
//							if (tmpHost->RefCnt() == 0) delete tmpHost;
//						}
//						*p = '-';
//					}
				}
				priorityHosts.AddHost(host);
			}
			reg.CloseKey();
		}
		reg.CloseKey();
		if (rewriteFlg) WriteRegistry(CFG_HOSTINFO|CFG_DELHOST);
	}

// Self private/public key (1024/2048)
	for (i=KEY_1024; i <= KEY_2048; i++) {
		const char *key = (i == KEY_1024) ? CRYPT_STR : CRYPT2_STR;
		int			need_blob_size = ((i == KEY_1024) ? 1024 : 2048) / 8 * 4;

		if (reg.CreateKey(key))
		{
			BYTE	key[MAX_BUF_EX];
			priv[i].encryptSeedLen = priv[i].blobLen = sizeof(key);

			if (reg.GetByte(PRIVBLOB_STR, key, &priv[i].blobLen)
				&& priv[i].blobLen > need_blob_size)
			{
				priv[i].blob = new BYTE [priv[i].blobLen];
				memcpy(priv[i].blob, key, priv[i].blobLen);
			}
			if (reg.GetByte(PRIVSEED_STR, key, &priv[i].encryptSeedLen))
			{
				priv[i].encryptSeed = new BYTE [priv[i].encryptSeedLen];
				memcpy(priv[i].encryptSeed, key, priv[i].encryptSeedLen);
			}
			reg.GetInt(PRIVTYPE_STR, &priv[i].encryptType);
			reg.CloseKey();
		}
	}

	if (reg.OpenKey(DEBUG_STR))	// 旧バージョンレジストリ削除
	{
		reg.CloseKey();
		reg.DeleteChildTree();
	}
	if (!reg.GetStr(NULL, buf, sizeof(buf)) || strcmp(buf, GetVersionStr()))
	{
		reg.CloseKey();		// close top appreg
		WriteRegistry();
	}

	return	TRUE;
}

BOOL Cfg::ReadFontRegistry(TRegistry *reg, char *key, LOGFONT *font)
{
	if (reg->CreateKey(key))
	{
		int		tmp = 0;

		reg->GetInt(HEIGHT_STR, &tmp);			font->lfHeight = tmp;
		reg->GetInt(WIDTH_STR, &tmp);			font->lfWidth = tmp;
		reg->GetInt(ESCAPEMENT_STR, &tmp);		font->lfEscapement = tmp;
		reg->GetInt(ORIENTATION_STR, &tmp);		font->lfOrientation = tmp;
		reg->GetInt(WEIGHT_STR, &tmp);			font->lfWeight = tmp;
		reg->GetInt(ITALIC_STR, &tmp);			font->lfItalic = (BYTE)tmp;
		reg->GetInt(UNDERLINE_STR, &tmp);		font->lfUnderline = (BYTE)tmp;
		reg->GetInt(STRIKEOUT_STR, &tmp);		font->lfStrikeOut = (BYTE)tmp;
		reg->GetInt(CHARSET_STR, &tmp);			font->lfCharSet = (BYTE)tmp;
		reg->GetInt(OUTPRECISION_STR, &tmp);	font->lfOutPrecision = (BYTE)tmp;
		reg->GetInt(CLIPPRECISION_STR, &tmp);	font->lfClipPrecision = (BYTE)tmp;
		reg->GetInt(QUALITY_STR, &tmp);			font->lfQuality = (BYTE)tmp;
		reg->GetInt(PITCHANDFAMILY_STR, &tmp);	font->lfPitchAndFamily = (BYTE)tmp;
		reg->GetStrA(FACENAME_STR, font->lfFaceName, sizeof(font->lfFaceName));
		reg->CloseKey();
	}
	return	TRUE;
}


BOOL Cfg::WriteRegistry(int ctl_flg)
{
	char	buf[MAX_LISTBUF], val[MAX_LISTBUF];
	int		i;

	GetRegName(buf, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, buf);

	if (ctl_flg & CFG_GENERAL)
	{
		reg.SetStr(NULL, GetVersionStr());
		reg.SetInt(LCID_KEY, lcid);
		reg.SetInt(NOBEEP_STR, NoBeep);
		reg.SetInt(LISTGET_STR, ListGet);
		reg.SetInt(LISTGETMSEC_STR, (int)ListGetMSec);
		reg.SetInt(RETRYMSEC_STR, (int)RetryMSec);
		reg.SetInt(RETRYMAX_STR, (int)RetryMax);
		reg.SetInt(IsWinNT() ? RECVMAXNT_STR : RECVMAX95_STR, (int)RecvMax);
		reg.SetInt(NOERASE_STR, NoErase);
//		reg.SetInt(DEBUG_STR, Debug);
		reg.SetInt(NOPOPUPCHECK_STR, NoPopupCheck);
		reg.SetInt(OPENCHECK_STR, OpenCheck);
		reg.SetInt(ALLOWSENDLIST_STR, AllowSendList);
		reg.SetInt(FILETRANSOPT_STR, fileTransOpt);
		reg.SetInt(RESOLVEOPT_STR, ResolveOpt);
//		reg.SetInt(ENCRYPTNUM_STR, EncryptNum);
		reg.SetInt(CLIPMODE_STR, ClipMode);
//		reg.SetInt(CLIPMAX_STR, ClipMax);
//		reg.SetInt(VIEWMAX_STR, ViewMax);
//		reg.SetInt(TRANSMAX_STR, TransMax);
//		reg.SetInt(TCPBUFMAX_STR, TcpbufMax);
//		reg.SetInt(IOBUFMAX_STR, IoBufMax);
		reg.SetInt(LUMPCHECK_STR, LumpCheck);
	}

	if (ctl_flg & CFG_ABSENCE)
	{
		reg.SetInt(ABSENCESAVE_STR, AbsenceSave);
		reg.SetInt(ABSENCECHECK_STR, AbsenceCheck);
		reg.SetInt(ABSENCEMAX_STR, AbsenceMax);
		if (reg.CreateKey(ABSENCESTR_STR))
		{
			for (i=0; i < AbsenceMax; i++)
			{
				char	key[MAX_PATH_U8];
				wsprintf(key, "%s%d", ABSENCESTR_STR, i);
				reg.SetStr(key, AbsenceStr[i]);
				wsprintf(key, "%s%d", ABSENCEHEAD_STR, i);
				reg.SetStr(key, AbsenceHead[i]);
			}
		}
		reg.CloseKey();
	}

	if (ctl_flg & CFG_GENERAL)
	{
		reg.SetStr(PASSWORD_STR, PasswordStr);
		reg.SetInt(PASSWDLOGCHECK_STR, PasswdLogCheck);
		reg.SetInt(DELAYTIME_STR, DelayTime);
		reg.SetInt(QUOTECHECK_STR, QuoteCheck);
		reg.SetInt(SECRETCHECK_STR, SecretCheck);
		reg.SetInt(LOGONLOG_STR, LogonLog);
		reg.SetInt(RECVLOGONDISP_STR, RecvLogonDisp);
		reg.SetInt(IPADDRCHECK_STR, IPAddrCheck);
		reg.SetInt(RECVIPADDRCHECK_STR, RecvIPAddr);
		reg.SetInt(ONECLICKPOPUP_STR, OneClickPopup);
		reg.SetInt(BALLOONNOTIFY_STR, BalloonNotify);
		reg.SetInt(ABNORMALBTN_STR, AbnormalButton);
		reg.SetInt(DIALUPCHECK_STR, DialUpCheck);
		reg.SetInt(ABSENCENONPOPUP_STR, AbsenceNonPopup);
		reg.SetStr(NICKNAMESTR_STR, NickNameStr);
		reg.SetStr(GROUPNAMESTR_STR, GroupNameStr);
		reg.SetLong(SORT_STR, Sort);
		reg.SetInt(UPDATETIME_STR, UpdateTime);
		reg.SetInt(KEEPHOSTTIME_STR, KeepHostTime);
		reg.SetInt(EXTENDENTRY_STR, ExtendEntry);
		reg.SetInt(EXTENDBROADCAST_STR, ExtendBroadcast);
		reg.SetInt(CONTROLIME_STR, ControlIME);
		reg.SetInt(GRIDLINE_STR, GridLineCheck);
		reg.SetInt(COLUMNITEMS_STR, ColumnItems);
		reg.SetStr(QUOTESTR_STR, QuoteStr);
		if (reg.CreateKey(HOTKEY_STR))
		{
			reg.SetInt(HOTKEYCHECK_STR, HotKeyCheck);
			reg.SetInt(HOTKEYMODIFY_STR, HotKeyModify);
			reg.SetInt(HOTKEYSEND_STR, HotKeySend);
			reg.SetInt(HOTKEYRECV_STR, HotKeyRecv);
			reg.SetInt(HOTKEYMISC_STR, HotKeyMisc);
			reg.CloseKey();
		}
		reg.SetInt(LOGCHECK_STR, LogCheck);
		reg.SetInt(LOGUTF8_STR, LogUTF8);
		reg.SetStr(LOGFILE_STR, LogFile);
		reg.SetStr(SOUNDFILE_STR, SoundFile);
		reg.SetStr(ICON_STR, IconFile);
		reg.SetStr(REVICON_STR, RevIconFile);
		reg.SetStr(LASTOPEN_STR, lastOpenDir);
		reg.SetStr(LASTSAVE_STR, lastSaveDir);
		reg.SetInt(LRUUSERMAX_STR, lruUserMax);
	}

	if ((ctl_flg & CFG_WINSIZE) && reg.CreateKey(WINSIZE_STR))
	{
		reg.SetInt(SENDNICKNAME_STR, SendWidth[SW_NICKNAME]);
		reg.SetInt(SENDUSERNAME_STR, SendWidth[SW_USER]);
		reg.SetInt(SENDABSENCE_STR, SendWidth[SW_ABSENCE]);
		reg.SetInt(SENDPRIORITY_STR, SendWidth[SW_PRIORITY]);
		reg.SetInt(SENDGROUPNAME_STR, SendWidth[SW_GROUP]);
		reg.SetInt(SENDHOSTNAME_STR, SendWidth[SW_HOST]);
		reg.SetInt(SENDIPADDR_STR, SendWidth[SW_IPADDR]);

		if (reg.CreateKey(SENDORDER_STR))
		{
			for (i=0; i < MAX_SENDWIDTH; i++)
			{
				wsprintf(buf, "%d", i);
				reg.SetInt(buf, SendOrder[i]);
			}
			reg.CloseKey();
		}

		reg.SetInt(SENDXDIFF_STR, SendXdiff);
		reg.SetInt(SENDYDIFF_STR, SendYdiff);
		reg.SetInt(SENDMIDYDIFF_STR, SendMidYdiff);
		reg.SetInt(SENDSAVEPOS_STR, SendSavePos);
		reg.SetInt(SENDXPOS_STR, SendXpos);
		reg.SetInt(SENDYPOS_STR, SendYpos);

		reg.SetInt(RECVXDIFF_STR, RecvXdiff);
		reg.SetInt(RECVYDIFF_STR, RecvYdiff);
		reg.SetInt(RECVSAVEPOS_STR, RecvSavePos);
		reg.SetInt(RECVXPOS_STR, RecvXpos);
		reg.SetInt(RECVYPOS_STR, RecvYpos);

		reg.SetInt(HISTXDIFF_STR, HistXdiff);
		reg.SetInt(HISTYDIFF_STR, HistYdiff);
		reg.SetInt(HISTUSER_STR, HistWidth[HW_USER]);
		reg.SetInt(HISTODATE_STR, HistWidth[HW_ODATE]);
		reg.SetInt(HISTSDATE_STR, HistWidth[HW_SDATE]);
		reg.SetInt(HISTID_STR, HistWidth[HW_ID]);

		reg.CloseKey();
	}

	if ((ctl_flg & CFG_FONT) && reg.CreateKey(FONT_STR))
	{
		WriteFontRegistry(&reg, SENDEDITFONT_STR, &SendEditFont);
		WriteFontRegistry(&reg, SENDLISTFONT_STR, &SendListFont);
		WriteFontRegistry(&reg, RECVHEADFONT_STR, &RecvHeadFont);
		WriteFontRegistry(&reg, RECVEDITFONT_STR, &RecvEditFont);
		reg.CloseKey();
	}

	if ((ctl_flg & CFG_BROADCAST) && reg.CreateKey(BROADCAST_STR))
	{
		i = 0;
		for (TBrObj *obj=brList.Top(); obj; obj=brList.Next(obj))
		{
			wsprintf(buf, "%d", i++);
			reg.SetStr(buf, obj->Host());
		}
		while (1)
		{
			wsprintf(buf, "%d", i++);
			if (!reg.GetStr(buf, val, sizeof(val)))
				break;
			if (!reg.DeleteValue(buf))
				break;
		}
		reg.CloseKey();
	}

	if (ctl_flg & CFG_CLICKURL)
	{
		reg.SetInt(DEFAULTURL_STR, DefaultUrl);
		reg.SetInt(SHELLEXEC_STR, ShellExec);
		if (reg.CreateKey(CLICKABLEURL_STR))
		{

			for (UrlObj *obj = (UrlObj *)urlList.TopObj(); obj; obj = (UrlObj *)urlList.NextObj(obj))
				reg.SetStr(obj->protocol, obj->program);
			reg.CloseKey();
		}
	}

	if ((ctl_flg & CFG_PRIORITY) && reg.CreateKey(PRIORITY_STR))
	{
		reg.SetInt(PRIORITYMAX_STR, PriorityMax);
		reg.SetInt(PRIORITYREJECT_STR, PriorityReject);
#if 0
		for (i=0; i < PriorityMax; i++)
		{
			wsprintf(buf, "%d", i);

			if (reg.CreateKey(buf))
			{
				reg.SetStr(PRIORITYSOUND_STR, PrioritySound[i] ? PrioritySound[i] : "");
				reg.CloseKey();
			}
		}
#endif
		reg.CloseKey();
	}

	if ((ctl_flg & CFG_HOSTINFO) && reg.CreateKey(HOSTINFO_STR))
	{
		Time_t	now_time = Time();
		BYTE	pubkey[MAX_BUF];
		int		pubkeySize;

		if (ctl_flg & CFG_DELHOST)
			reg.DeleteChildTree();

		for (i=0; i < priorityHosts.HostCnt(); i++)
		{
			Host	*host = priorityHosts.GetHost(i);
			int		to_store_size = 2048 / 8; // 2048bits RSA key
			wsprintf(buf, "%s:%s", host->hostSub.userName, host->hostSub.hostName);

			if (host->updateTime + KeepHostTime < now_time ||
				(host->pubKey.KeyLen() < to_store_size && host->priority == DEFAULT_PRIORITY))
			{
				if (ctl_flg & CFG_DELCHLDHOST)
					reg.DeleteChildTree(buf);
			}
			else if (reg.CreateKey(buf))
			{
				reg.SetStr(USERNAME_STR, host->hostSub.userName);
				reg.SetStr(HOSTNAME_STR, host->hostSub.hostName);
				reg.SetLong(IPADDR_STR, host->hostSub.addr);
				reg.SetInt(PORTNO_STR, host->hostSub.portNo);
				reg.SetStr(NICKNAME_STR, host->nickName);
				reg.SetStr(GROUPNAME_STR, host->groupName);
				if (*host->alterName) reg.SetStr(ALTERNAME_STR, host->alterName);
				reg.SetInt(PRIORITY_STR, host->priority);
				reg.SetLong(UPDATETIME_STR, (long)host->updateTime);
				if (host->pubKey.KeyLen() >= to_store_size) {
					char	*p = (char *)GetUserNameDigestField(host->hostSub.userName);
					if (p && (pubkeySize = host->pubKey.Serialize(pubkey, sizeof(pubkey))) > 0) {
						reg.SetByte(PUBLICKEY_STR, pubkey, pubkeySize);
					}
				}
				reg.CloseKey();
			}
		}
		reg.CloseKey();
	}

	if ((ctl_flg & CFG_FINDHIST) && reg.CreateKey(FINDSTR_STR))
	{
		reg.SetInt(FINDMAX_STR, FindMax);
		reg.SetInt(FINDALL_STR, FindAll);
		for (i=0; i < FindMax; i++)
		{
			char	key[MAX_PATH_U8];
			wsprintf(key, "%d", i);
			reg.SetStr(key, FindStr[i]);
		}
		reg.CloseKey();
	}

	if (ctl_flg & CFG_LRUUSER) {
		reg.DeleteChildTree(LRUUSERKEY_STR);
		if (reg.CreateKey(LRUUSERKEY_STR)) {
			UsersObj *obj=(UsersObj *)lruUserList.TopObj();
			for (i=0; obj; i++) {
				wsprintf(buf, "%d", i);
				if (!reg.CreateKey(buf)) break;
				UserObj *u = (UserObj *)obj->users.TopObj();
				for (int j=0; u; j++) {
					wsprintf(buf, "%d", j);
					if (!reg.CreateKey(buf)) break;
					reg.SetStr(USERNAME_STR, u->hostSub.userName);
					reg.SetStr(HOSTNAME_STR, u->hostSub.hostName);
					reg.CloseKey();
					u = (UserObj *)obj->users.NextObj(u);
				}
				reg.CloseKey();
				obj = (UsersObj *)lruUserList.NextObj(obj);
			}
			reg.CloseKey();
		}
	}

// private/public key
	if (ctl_flg & CFG_CRYPT) {
		for (int i=KEY_1024; i <= KEY_2048; i++) {
			const char *key = (i == KEY_1024) ? CRYPT_STR : CRYPT2_STR;
			if (reg.CreateKey(key) && priv[i].blob)
			{
				reg.SetByte(PRIVBLOB_STR, priv[i].blob, priv[i].blobLen);
				if (priv[i].encryptSeed)
					reg.SetByte(PRIVSEED_STR, priv[i].encryptSeed, priv[i].encryptSeedLen);
				reg.SetInt(PRIVTYPE_STR, priv[i].encryptType);
				reg.CloseKey();
			}
		}
	}

	return	TRUE;
}


BOOL Cfg::WriteFontRegistry(TRegistry *reg, char *key, LOGFONT *font)
{
	if (reg->CreateKey(key))
	{
		reg->SetInt(HEIGHT_STR, (int)font->lfHeight);
		reg->SetInt(WIDTH_STR, (int)font->lfWidth);
		reg->SetInt(ESCAPEMENT_STR, (int)font->lfEscapement);
		reg->SetInt(ORIENTATION_STR, (int)font->lfOrientation);
		reg->SetInt(WEIGHT_STR, (int)font->lfWeight);
		reg->SetInt(ITALIC_STR, (int)font->lfItalic);
		reg->SetInt(UNDERLINE_STR, (int)font->lfUnderline);
		reg->SetInt(STRIKEOUT_STR, (int)font->lfStrikeOut);
		reg->SetInt(CHARSET_STR, (int)font->lfCharSet);
		reg->SetInt(OUTPRECISION_STR, (int)font->lfOutPrecision);
		reg->SetInt(CLIPPRECISION_STR, (int)font->lfClipPrecision);
		reg->SetInt(QUALITY_STR, (int)font->lfQuality);
		reg->SetInt(PITCHANDFAMILY_STR, (int)font->lfPitchAndFamily);
		reg->SetStrA(FACENAME_STR, font->lfFaceName);
		reg->CloseKey();
	}
	return	TRUE;
}

void Cfg::GetRegName(char *buf, ULONG nic_addr, int port_no)
{
	buf += wsprintf(buf, "%s", GetLoadStr(IDS_IPMSG));
	if (port_no != IPMSG_DEFAULT_PORT)
		buf += wsprintf(buf, "%d", port_no);
	if (nic_addr)
		buf += wsprintf(buf, "_%s", inet_ntoa(*(in_addr *)&nic_addr));
}

