static char *cfg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   cfg.cpp	Ver4.60";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Configuration
	Create					: 1996-09-27(Sat)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "resource.h"
#include "blowfish.h"
#include <vector>
using namespace std;

#ifdef IPMSG_PRO
#include "master.dat"
#endif

#ifdef DISALBE_HOOK
volatile int64 gEnableHook = 0;
#else
volatile int64 gEnableHook = ENABLE_HOOK;
#endif

// Official RSA key for update
static int  official_e   = 65537;
static BYTE official_n[] = {	// little endian for MS CryptoAPI
	0x1f,0xbf,0xc2,0xfc,0xca,0x43,0x2f,0x71,0x3f,0xae,0x96,0x72,0x66,0xdb,0x9d,0x3c,
	0xe0,0x54,0x36,0x8a,0x26,0xaf,0x0d,0xec,0x5c,0x7e,0x30,0x8e,0x3d,0x92,0x6c,0x24,
	0x19,0x0d,0x70,0xc2,0xab,0x50,0x6f,0xb1,0x11,0xf5,0x53,0x17,0xaa,0x82,0x46,0x9d,
	0xde,0x87,0x46,0xad,0x86,0x7d,0x2b,0x03,0x6c,0xe9,0xbd,0x68,0x3e,0xa5,0xbf,0x38,
	0xc9,0x94,0xc1,0xcb,0x27,0x99,0xcb,0xa5,0x7c,0x76,0x9e,0xe0,0xa3,0x6d,0x9b,0x90,
	0x12,0xda,0x64,0x2d,0x9a,0xc0,0xcf,0xe6,0xe6,0x6a,0x48,0xe7,0xfd,0x00,0xe3,0x0f,
	0xe7,0x07,0x29,0x54,0xcf,0x44,0xfe,0xb8,0xcf,0xa7,0x8c,0x67,0x7b,0x78,0x86,0x09,
	0xc9,0xf1,0xf5,0x49,0xc8,0x4c,0x2c,0xad,0x40,0xca,0xde,0xb9,0x33,0x78,0xdc,0xf6,
	0xc2,0xa1,0xb2,0xfb,0x15,0x67,0xbb,0x67,0xff,0xba,0xcf,0x80,0x72,0x2e,0xa7,0x06,
	0xd1,0xca,0x8b,0x9a,0x6d,0x82,0x89,0x5a,0x06,0x47,0x01,0xb4,0xfe,0x2d,0xc0,0x02,
	0xeb,0xe8,0x14,0x98,0x38,0xf9,0xab,0xd1,0x32,0x34,0x37,0x93,0x75,0x11,0x7b,0x6b,
	0x61,0xed,0x0a,0xcb,0x49,0xc4,0x27,0xcf,0x86,0x78,0xff,0x09,0x61,0xca,0x3f,0xa8,
	0x1a,0xbb,0x24,0xb7,0xda,0x2d,0xc9,0xd2,0x6f,0xad,0x91,0xba,0x06,0xd8,0xdf,0xa1,
	0xde,0x1a,0xf0,0x2d,0xf4,0x4e,0xda,0xa2,0x05,0x45,0xb8,0xf9,0xa3,0x3b,0x36,0xe9,
	0xa0,0x44,0x9d,0x0e,0x61,0x24,0x37,0xd3,0x18,0x63,0x53,0x10,0x3e,0xf8,0xf3,0xf6,
	0x22,0xee,0x52,0x7a,0xb6,0x4a,0x1a,0x71,0xe6,0xb9,0xb8,0x27,0xf0,0xa6,0xa9,0xa7,
};

#define IPMSG_DEFAULT_LISTGETMSEC	3000
#define IPMSG_DEFAULT_RETRYMSEC		750
#define IPMSG_DEFAULT_RETRYMAX		6
#define IPMSG_DEFAULT_RECVMAX		300
#define IPMSG_DEFAULT_ABSENCEMAX	8
#define IPMSG_DEFAULT_FINDMAX		12
#define IPMSG_DEFAULT_DELAY			250
#define IPMSG_DEFAULT_UPDATETIME	10
#define IPMSG_DEFAULT_KEEPHOSTTIME	(3600 * 24 * 180)	// 180日間
#define IPMSG_DEFAULT_DISPHOSTTIME	(3600 * 24 * 14)	// 14日間
#define IPMSG_DEFAULT_QUOTE			">"
#define IPMSG_DEFAULT_CLIPMODE		(CLIP_ENABLE|CLIP_SAVE)
#define IPMSG_DEFAULT_CLIPMAX		10
#define IPMSG_DEFAULT_LRUUSERMAX	10
#define IPMSG_DEFAULT_OPENMSGTIME	3000
#define IPMSG_DEFAULT_RECVMSGTIME	10000
#define IPMSG_DEFAULT_MARKER_THICK	3
#define IPMSG_DEFAULT_REMOTEGRACE	15 // 15sec

#ifdef _WIN64
#define IPMSG_DEFAULT_VIEWMAX		(16 * 1024 * 1024)	// 16MB
#define IPMSG_DEFAULT_IOBUFMAX		(8 * 1024 * 1024)	// 8MB
#else
#define IPMSG_DEFAULT_VIEWMAX		(8 * 1024 * 1024)	// 1MB
#define IPMSG_DEFAULT_IOBUFMAX		(4 * 1024 * 1024)	// 1MB
#endif
#define IPMSG_DEFAULT_TRANSMAX		(128 * 1024)		// 128KB
#define IPMSG_DEFAULT_TCPBUFMAX		(256 * 1024)		// 256KB

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
#define IPMSG_DEFAULT_HWMSG_WIDTH	100
#define IPMSG_DEFAULT_HWID_WIDTH	 10

#define IPMSG_DEFAULT_AUTOSAVE_FLAGS (AUTOSAVE_ENABLED | AUTOSAVE_INCDIR)
#define IPMSG_DEFAULT_AUTOSAVE_TOUT	120
#define IPMSG_DEFAULT_AUTOSAVE_MAX	1000
#define IPMSG_DEFAULT_LVFINDMAX		10

#define IPMSG_DEFAULT_MASTERPOLL	60000
#define IPMSG_DEFAULT_DIRSPAN		180
#define IPMSG_DEFAULT_DIRAGENTNUM	3

#define LCID_KEY			"lcid"
#define NOBEEP_STR			"NoBeep"
#define NOTCP_STR			"NoTcp"
#define NOFILETRANS_STR		"NoFileTrans"
#define LISTGET_STR			"ListGet"
#define LISTGETMSEC_STR		"ListGetMSec"
#define RETRYMSEC_STR		"RetryMSec3"
#define RETRYMAX_STR		"RetryMax2"
#define RECVMAX_STR			"RecvMax"
#define LASTRECV_STR		"LastRecv"
#define NOERASE_STR			"NoErase"
#define REPROMSG_STR		"ReproMsg"
#define DEBUG_STR			"Debug"
//#define FWCHECKMODE_STR		"FwCheckMode" // define in ipmsgdef.h
#define FW3RDPARTY_STR		"Fw3rdParty"
#define RECVICONMODE_STR	"RecvIconMode"
#define BROADCAST_STR		"BroadCast"
#define CLICKABLEURL_STR	"ClickableUrl"
#define NOPOPUPCHECK_STR	"NoPopup"
#define OPENCHECK_STR		"OpenCheck"
#define REPLYFILTER_STR		"ReplyFilter"
#define ABSENCESAVE_STR		"AbsenceSave"
#define ABSENCECHECK_STR	"AbsenceCheck"
#define ABSENCECHOICE_STR	"AbsenceChoice"
#define ABSENCEMAX_STR		"AbsenceMax"
#define ABSENCESTR_STR		"AbsenceStr"
#define ABSENCEHEAD_STR		"AbsenceHead"
#define USELOCKNAME_STR		"UseLockName"
#define LOCKNAME_STR		"LockName"
#define FINDMAX_STR			"FindMax2"
#define FINDALL_STR			"FindAll"
#define FINDSTR_STR			"FindStr"
#define PASSWORDUSE_STR		"PasswordUse"
#define PASSWORD_STR		"PasswordStr"
#define PASSWDLOGCHECK_STR	"PasswdLogCheck"
#define DELAYTIME_STR		"DelayTime2"
#define QUOTECHECK_STR		"QuoteCheck"
#define SECRETCHECK_STR		"SecretCheck"
#define IPADDRCHECK_STR		"IPAddrCheck2"
#define RECVIPADDRCHECK_STR	"RecvIPAddrCheck"
#define ONECLICKPOPUP_STR	"OneClickPopup2"
#define BALLOONNOTIFY_STR	"BalloonNotify"
#define TRAYICON_STR		"TrayIcon"
#define ABNORMALBTN_STR		"AbnormalButton"
#define DIALUPCHECK_STR		"DialUpCheck"
#define LOGONLOG_STR		"LogonLog2"
#define RECVLOGONDISP_STR	"RecvLogonDisp"
#define NICKNAMESTR_STR		"NickNameStr"
#define GROUPNAMESTR_STR	"GroupNameStr"
#define ABSENCENONPOPUP_STR	"AbsenceNonPopup"
#define LOGVIEWATRECV_STR	"LogViewAtRecv"
#define SORT_STR			"Sort"
#define UPDATETIME_STR		"UpdateTime"
#define KEEPHOSTTIME_STR	"KeepHostTime"
#define DISPHOSTTIME_STR	"DispHostTime2"
#define MSGMINIMIZE_STR		"MsgMinimize"
#define DEFAULTURL_STR		"DefaultUrl"
#define SHELLEXEC_STR		"ShellExec"
#define EXTENDENTRY_STR		"ExtendEntry"
#define EXTENDBROADCAST_STR	"ExtendBroadcast"
#define MULTICASTMODE_STR	"MulticastMode"
#define QUOTESTR_STR		"QuoteStr"
#define CONTROLIME_STR		"ControlIME2"
#define HOTKEY_STR			"HotKey"
#define HOTKEYCHECK_STR		"HotKeyCheck"
#define HOTKEYMODIFY_STR	"HotKeyModify"
#define HOTKEYSEND_STR		"HotKeySend"
#define HOTKEYRECV_STR		"HotKeyRecv"
#define HOTKEYMISC_STR		"HotKeyMisc"
#define HOTKEYLOGVIEW_STR	"HotKeyLogView"
#define ALLOWSENDLIST_STR	"AllowSendList"
#define FILETRANSOPT_STR	"FileTransOpt"
#define RESOLVEOPT_STR		"ResolveOpt"
#define LETTERKEY_STR		"LetterKey"
#define LVFINDSTR_STR		"LvFindStr"
#define LVFINDMAX_STR		"LvFindMax"
#define UPLOADPKT_STR		"UploadPkt"
#define LISTCONFIRMKEY_STR	"ListConfirm"
#define DELAYSEND_STR		"DelaySend"

#define LINKACT_STR			"LinkAct"
#define DIRECTOPENEXT_STR	"DirectOpenExt"
#define LINKDETECTMODE_STR	"LinkDetectMode"
#define CLICKOPENMODE_STR	"ClipOpenMode"

#define REMOTEGRACE_STR		"RemoteGraceSec"
#define REBOOT_STR			"RemoteReboot"
#define REBOOTMODE_STR		"RemoteRebootMode"
#define EXIT_STR			"RemoteExit"
#define EXITMODE_STR		"RemoteExitMode"
#define IPMSGEXIT_STR		"RemoteIPMsgExit"
#define IPMSGEXITMODE_STR	"RemoteIPMsgExitMode"

#define CLIPMODE_STR		"ClipMode"
#define CLIPMAX_STR			"ClipMax"
#define CAPTUREDELAY_STR	"CaptureDelay"
#define CAPTUREDELAYEX_STR	"CaptureDelayEx"
#define CAPTUREMINIMIZE_STR	"CaptureMinimize"
#define CAPTURECLIP_STR		"CaptureClip"
#define CAPTURESAVE_STR		"CaptureSave"
#define OPENMSGTIME_STR		"OpenMsgTime"
#define RECVMSGTIME_STR		"RecvMsgTime"
#define BALLOONNOINFO_STR	"BalloonNoInfo"
#define TASKBARUI_STR		"TaskbarUI"
#define MARKERTHICK_STR		"MarkerThick"
#define LASTWND_STR			"LastWnd"

#define IPV6MODE_STR		"IPv6Mode"
#define IPV6MULITCAST		"IPv6MultiCast"

#define VIEWMAX_STR			"ViewMax2"
#define TRANSMAX_STR		"TransMax"
#define TCPBUFMAX_STR		"TcpbufMax"
#define IOBUFMAX_STR		"IoBufMax"
#define LUMPCHECK_STR		"LumpCheck"
#define ENCTRANSCHECK_STR	"EncTransCheck2"

#define LOGCHECK_STR		"LogCheck"
#define LOGUTF8_STR			"LogUTF8Ex"
#define LOGFILE_STR			"LogFile"
#define SOUNDFILE_STR		"SoundFile"
#define ICON_STR			"Icon"
#define REVICON_STR			"RevIcon"
#define LASTOPEN_STR		"lastOpen"
#define LASTSAVE_STR		"lastSave"
#define LASTVACUUM_STR		"lastVacuum"

#define AUTOSAVEDIR_STR		"autoSaveDir"
#define AUTOSAVETOUT_STR	"autoSaveTout"
#define AUTOSAVEMAX_STR		"autoSaveMax"
#define AUTOSAVELEVEL_STR	"autoSaveLevel"
#define AUTOSAVEFLAGS_STR	"autoSaveFlags2"
#define DOWNLOADLINK_STR	"DownloadLink"

#define WAITUPDATE_STR		"WaitUpdate"
#define AUTOUPDATEMODE_STR	"autoUpdateMode"
#define VIEWEPOCHSAVE_STR	"viewEpochSave"

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

#define HISTXDIFF_STR		"HistXdiff2"
#define HISTYDIFF_STR		"HistYdiff"
#define HISTUSER_STR		"HistUser"
#define HISTODATE_STR		"HistODate"
//#define HISTID_STR			"HistId2"
#define HISTSDATE_STR		"HistSDate2"
#define HISTMSG_STR			"HistMsg"

#define LVX_STR				"LvX"
#define LVY_STR				"LvY"
#define LVCX_STR			"LvCx"
#define LVCY_STR			"LvCy"

#define FONT_STR			"Fonts"
#define SENDEDITFONT_STR	"SendEdit"
#define SENDLISTFONT_STR	"SendListView"
#define RECVHEADFONT_STR	"RecvHead"
#define RECVEDITFONT_STR	"RecvEdit"
#define LOGVIEWFONT_STR		"LogView2"

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

#define HOSTINFO_STR		"HostInfo2"
#define HOSTINFOOLD_STR		"HostInfo"
#define USERNAME_STR		"UserName"
#define HOSTNAME_STR		"HostName"
#define IPADDROLD_STR		"IPAddr"
#define IPADDR_STR			"IPAddr2"
#define PORTNOOLD_STR		"PortNo"
#define PORTNO_STR			"PortNo2"
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
#define REGOWNER_STR		"RegOwner"

#define MASTERSVR_STR		"MasterSvr"
#define MASTERPOLL_STR		"MasterPoll"
#define MASTERSTRICT_STR	"MasterStrict"
#define DIRMODE_STR			"DirMode"
#define DIRSPAN_STR			"DirSpan"
#define DIRAGENTNUM_STR		"DirAgentNum"

#define LOGVIEWDISPMODE_STR	"LogViewDispMode"

#define HOOK_STR			"hook"
#define HOOKMODE_STR		"hook_mode"
#define HOOKKIND_STR		"kind"
#define HOOKMODEOLD_STR		"mode"
#define HOOKURL_STR			"hook_url"
#define BODY_STR			"body"

#define SLACK_STR			"Slack"
#define HOST_STR			"host"
#define KEY_STR				"key"
#define CHAN_STR			"chan"
//#define ICON_STR			"icon"

#define UPDATE_STR			"Update"
#define UPDATEFLAG_STR		"flag2"
#define UPDATEFLAGOLD_STR	"flag"
#define UPDATESPAN_STR		"span"
#define UPDATELAST_STR		"last"

#define WINE_STR			"Wine"	// for Wine environment

char	*DefaultUrlProtocol[] = { "HTTP", "HTTPS", "FTP", "FILE", "TELNET", NULL };
char	*DefaultAbsence[IPMSG_DEFAULT_ABSENCEMAX];
char	*DefaultAbsenceHead[IPMSG_DEFAULT_ABSENCEMAX];

char	*DefaultDirectOpenExt = 
	"txt log ini pdf hlp chm "				"\r\n" \
	"doc docx ppt pptx xls xlsx csv "		"\r\n" \
	"html htm mht xml css "					"\r\n" \
	"jpg jpeg png gif bmp tiff "			"\r\n" \
	"mp3 mp4 avi mpeg mov wma wmv "			"\r\n" \
	"c cpp h hpp cs java class clj d dart"	"\r\n" \
	"erl go hs lua lisp r rs scala scm";

char *GetIdsIPMSG()
{
	static char *ids_ipmsg = strdup(LoadStr(IDS_IPMSG)); // for TSetThreadLocale

	return	ids_ipmsg;
}


Cfg::Cfg(const Addr &_nicAddr, int _portNo) :
	nicAddr(_nicAddr),
	portNo(_portNo)
{
	AbsenceHead = NULL;
	AbsenceStr = NULL;
	FindStr = NULL;

	int	abs_ids[]  = { IDS_DEFABSENCE1, IDS_DEFABSENCE2, IDS_DEFABSENCE3, IDS_DEFABSENCE4, IDS_DEFABSENCE5, IDS_DEFABSENCE6, IDS_DEFABSENCE7, IDS_DEFABSENCE8, 0 };
	for (int i=0; i < sizeof(DefaultAbsence) / sizeof(char *) && abs_ids[i]; i++) {
		DefaultAbsence[i] = LoadStrU8(abs_ids[i]);
	}

	int	absh_ids[] = { IDS_DEFABSENCEHEAD1, IDS_DEFABSENCEHEAD2, IDS_DEFABSENCEHEAD3, IDS_DEFABSENCEHEAD4, IDS_DEFABSENCEHEAD5, IDS_DEFABSENCEHEAD6, IDS_DEFABSENCEHEAD7, IDS_DEFABSENCEHEAD8, 0 };
	for (int i=0; i < sizeof(DefaultAbsenceHead) / sizeof(char *) && absh_ids[i]; i++) {
		DefaultAbsenceHead[i] = LoadStrU8(absh_ids[i]);
	}

	char	buf[MAX_PATH_U8];
	char	path[MAX_PATH_U8];
	char	*fname = NULL;

	GetModuleFileNameU8(NULL, buf, MAX_PATH_U8);
	GetFullPathNameU8(buf, MAX_PATH_U8, path, &fname);
	fname[-1] = 0; // remove '\\'
	execDir = strdup(path);
}

Cfg::~Cfg()
{
	delete [] FindStr;
	delete [] AbsenceHead;
	delete [] AbsenceStr;
	free(execDir);
}

#ifdef IPMSG_PRO
#define CFG_FUNCS
#include "miscext.dat"
#undef  CFG_FUNCS
#endif

#define PRIORITY_KEY	"PR"
#define HOSTSTAT_KEY	"HS"
#define UNAME_KEY		"UN"
#define HNAME_KEY		"HN"
#define ADDR_KEY		"AD"
#define PORT_KEY		"PT"
#define NICK_KEY		"NK"
#define GROUP_KEY		"GP"
#define ALTER_KEY		"AL"
#define UPTIME_KEY		"UT"
#define PUBKEY_KEY		"PB"

bool Cfg::ReadRegistry(void)
{
	char	buf[MAX_BUF];

	GetRegName(buf, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, buf);
	DWORD		saveRegFlg = 0;
	bool		isChangeApp = false;

	if (!reg.GetInt(NOPOPUPCHECK_STR, &NoPopupCheck))
	{
		GetRegName(buf, 0, portNo);
		reg.ChangeApp(HS_TOOLS, buf);
		if (!reg.GetInt(NOPOPUPCHECK_STR, &NoPopupCheck)) {
			reg.ChangeApp(HS_TOOLS, GetIdsIPMSG());
			isChangeApp = true;
		}
	}

	lcid = -1;
	NoBeep = FALSE;
	NoTcp = FALSE;
#if defined(PROHIBIT_TRANSMODE)
	NoFileTrans = 1; // ファイル/フォルダ転送の完全禁止（画像貼付含む）
#elif defined(INTERMID_TRANSMODE)
	NoFileTrans = 2; // ネットワークボリュームのみ禁止
#elif defined(IMGONLY_TRANSMODE)
	NoFileTrans = 3; // 画像添付以外のファイル/フォルダを禁止
#else
	NoFileTrans = 0; // 許可
#endif

	ListGet = FALSE;
	HotKeyCheck   = TRUE;
	HotKeyModify  = MOD_ALT|MOD_CONTROL;
	HotKeySend    = 'S';
	HotKeyRecv    = 'R';
	HotKeyLogView = 'E';
	HotKeyMisc    = 'D';

	ListGetMSec = IPMSG_DEFAULT_LISTGETMSEC;
	RetryMSec = IPMSG_DEFAULT_RETRYMSEC;
	RetryMax = IPMSG_DEFAULT_RETRYMAX;
	RecvMax = IPMSG_DEFAULT_RECVMAX;
	LastRecv = 0;
	NoErase = FALSE;
	ReproMsg = TRUE;
	Debug = 0;
	FwCheckMode = 0;
	Fw3rdParty = -1;
	RecvIconMode = 0;
	NoPopupCheck = 1;
	OpenCheck = 1;
	ReplyFilter = 1;
	AbsenceSave = 0;
	AbsenceCheck = FALSE;
	AbsenceMax = IPMSG_DEFAULT_ABSENCEMAX;
	AbsenceChoice = 0;
	FindMax = IPMSG_DEFAULT_FINDMAX;
	FindAll = TRUE;
	DelayTime = IPMSG_DEFAULT_DELAY;
	QuoteCheck = 0x1;
	SecretCheck = TRUE;
	lastWnd = 0;

	LogCheck = TRUE;
	LogonLog = TRUE;
	LogUTF8 = TRUE;
	PasswdLogCheck = FALSE;
	IPAddrCheck = TRUE;

	RecvLogonDisp = FALSE;
	RecvIPAddr = TRUE;
	OneClickPopup = FALSE;
	BalloonNotify = TRUE;
	TrayIcon = TRUE;
	AbnormalButton = FALSE;
	DialUpCheck = FALSE;
	*NickNameStr = 0;
	*GroupNameStr = 0;
	AbsenceNonPopup = TRUE;
	logViewAtRecv = FALSE;
	Sort = IPMSG_NAMESORT;
	UpdateTime = IPMSG_DEFAULT_UPDATETIME;
	KeepHostTime = IPMSG_DEFAULT_KEEPHOSTTIME;
	DispHostTime = IPMSG_DEFAULT_DISPHOSTTIME;
	DefaultUrl = TRUE;
	ShellExec = FALSE;
	ExtendEntry = TRUE;
	ExtendBroadcast = 1;
	MulticastMode = 0;
	LvFindMax = IPMSG_DEFAULT_LVFINDMAX;

	ControlIME = 1;
	GridLineCheck = TRUE;
	PriorityMax = DEFAULT_PRIORITYMAX;
	PriorityReject = 0;
	AllowSendList = TRUE;
	fileTransOpt = 0;
	ResolveOpt = 0;
	LetterKey = IsLang(LANG_JAPANESE) ? FALSE : TRUE;
	ListConfirm = FALSE;
	DelaySend = 1;

	RemoteGraceSec = IPMSG_DEFAULT_REMOTEGRACE;
	RemoteRebootMode = 0;
	*RemoteReboot = 0;
	RemoteExitMode = 0;
	*RemoteExit = 0;
	RemoteIPMsgExitMode = 0;
	*RemoteIPMsgExit = 0;

	ClipMode = IPMSG_DEFAULT_CLIPMODE;
	ClipMax = IPMSG_DEFAULT_CLIPMAX;
	CaptureDelay   = 300;
	CaptureDelayEx = 500;
	CaptureMinimize = TRUE;
	CaptureClip = TRUE;
	CaptureSave = FALSE;
	OpenMsgTime = IPMSG_DEFAULT_OPENMSGTIME;
	RecvMsgTime = IPMSG_DEFAULT_RECVMSGTIME;
	BalloonNoInfo = FALSE;
	TaskbarUI = /*IsWin7() ? TRUE :*/ FALSE;
	MarkerThick = IPMSG_DEFAULT_MARKER_THICK;
	IPv6Mode = IPv6ModeNext = 0;
	strcpy(IPv6Multicast, IPMSG_DEFAULT_MULTICAST_ADDR6); // in ipmsg.h

	ViewMax = IPMSG_DEFAULT_VIEWMAX;
	TransMax = IPMSG_DEFAULT_TRANSMAX;
	TcpbufMax = IPMSG_DEFAULT_TCPBUFMAX;
	IoBufMax = IPMSG_DEFAULT_IOBUFMAX;
	LumpCheck = FALSE;
	EncTransCheck = FALSE;
	lruUserMax = IPMSG_DEFAULT_LRUUSERMAX;

	Wine = -1;

	logViewDispMode = 0;

	// CryptProtectData is available only Win2K/XP
	for (int i=0; i < MAX_KEY; i++) {
//		priv[i].encryptType = IsWin2K() ? PRIV_BLOB_DPAPI : PRIV_BLOB_RAW;
		priv[i].encryptType = PRIV_BLOB_RAW;
	}

	strncpyz(QuoteStr, IPMSG_DEFAULT_QUOTE, sizeof(QuoteStr));
	U8toW(QuoteStr, QuoteStrW, wsizeof(QuoteStrW), -1);

	SendWidth[SW_NICKNAME] = IPMSG_DEFAULT_NICKNAMEWIDTH;
	SendWidth[SW_ABSENCE] = IPMSG_DEFAULT_ABSENCEWIDTH;
	SendWidth[SW_GROUP] = IPMSG_DEFAULT_GROUPWIDTH;
	SendWidth[SW_HOST] = IPMSG_DEFAULT_HOSTWIDTH;
	SendWidth[SW_IPADDR] = IPMSG_DEFAULT_IPADDRWIDTH;
	SendWidth[SW_USER] = IPMSG_DEFAULT_USERWIDTH;
	SendWidth[SW_PRIORITY] = IPMSG_DEFAULT_PRIORITYWIDTH;

	ColumnItems = 0;
	SetItem(&ColumnItems, SW_NICKNAME, true);
	SetItem(&ColumnItems, SW_GROUP, true);
	SetItem(&ColumnItems, SW_HOST, true);

	for (int i=0; i < MAX_SENDWIDTH; i++) {
		SendOrder[i] = i;
	}

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
	HistWidth[HW_MSG]   = IPMSG_DEFAULT_HWMSG_WIDTH;
//	HistWidth[HW_ID]    = IPMSG_DEFAULT_HWID_WIDTH;
	HistXdiff		= 0;
	HistYdiff		= 0;

	LvX				= 0;
	LvY				= 0;
	LvCx			= 0;
	LvCy			= 0;

	*SoundFile = 0;
	*IconFile = 0;
	*RevIconFile = 0;
	*lastSaveDir = 0;
	*lastOpenDir = 0;

	*autoSaveDir  = 0;
	autoSaveTout  = IPMSG_DEFAULT_AUTOSAVE_TOUT;
	autoSaveLevel = 0;
	autoSaveFlags = IPMSG_DEFAULT_AUTOSAVE_FLAGS;
	autoSaveMax   = IPMSG_DEFAULT_AUTOSAVE_MAX;
	autoSavedSize = 0;
	downloadLink = 0;	// 0x1 : prevent to create downloadlink

	waitUpdate = FALSE;
	autoUpdateMode = 1;
	viewEpoch = time(NULL);
	viewEpochSave = 0;

	memset(&SendEditFont, 0, sizeof(SendEditFont));
	memset(&SendListFont, 0, sizeof(SendListFont));
	memset(&RecvHeadFont, 0, sizeof(RecvHeadFont));
	memset(&RecvEditFont, 0, sizeof(RecvEditFont));
	memset(&LogViewFont, 0, sizeof(LogViewFont));

	reg.GetInt(LCID_KEY, &lcid);
	reg.GetInt(NOBEEP_STR, &NoBeep);
	reg.GetInt(NOTCP_STR, &NoTcp);
	reg.GetInt(NOFILETRANS_STR, &NoFileTrans);
	reg.GetInt(LISTGET_STR, &ListGet);
	reg.GetInt(LISTGETMSEC_STR, (int *)&ListGetMSec);
	reg.GetInt(RETRYMSEC_STR, (int *)&RetryMSec);
	reg.GetInt(RETRYMAX_STR, (int *)&RetryMax);
	reg.GetInt(RECVMAX_STR, (int *)&RecvMax);
	reg.GetInt64(LASTRECV_STR, (int64 *)&LastRecv);
	reg.GetInt(NOERASE_STR, &NoErase);
	reg.GetInt(REPROMSG_STR, &ReproMsg);
	reg.GetInt(DEBUG_STR, &Debug);
	reg.GetInt(FWCHECKMODE_STR, &FwCheckMode);
	reg.GetInt(FW3RDPARTY_STR, &Fw3rdParty);
	reg.GetInt(RECVICONMODE_STR, &RecvIconMode);

	reg.GetInt(NOPOPUPCHECK_STR, &NoPopupCheck);
	reg.GetInt(OPENCHECK_STR, &OpenCheck);
	reg.GetInt(REPLYFILTER_STR, &ReplyFilter);
	reg.GetInt(ABSENCESAVE_STR, &AbsenceSave);
	reg.GetInt(ABSENCEMAX_STR, &AbsenceMax);
	if (AbsenceSave) {
		if (AbsenceSave & ABSENCESAVE_ONESHOT) {
			AbsenceSave &= ~ABSENCESAVE_ONESHOT;
			saveRegFlg |= CFG_ABSENCE;
		}
		reg.GetInt(ABSENCECHECK_STR, &AbsenceCheck);
		reg.GetInt(ABSENCECHOICE_STR, &AbsenceChoice);
		if (AbsenceChoice >= AbsenceMax) {
			AbsenceChoice = 0;
		}
	}
	reg.GetInt(ALLOWSENDLIST_STR, &AllowSendList);
	reg.GetInt(FILETRANSOPT_STR, &fileTransOpt);
	reg.GetInt(RESOLVEOPT_STR, &ResolveOpt);
	reg.GetInt(LETTERKEY_STR, &LetterKey);
	reg.GetInt(LISTCONFIRMKEY_STR, &ListConfirm);
	reg.GetInt(DELAYSEND_STR, &DelaySend);

	reg.GetInt(REMOTEGRACE_STR, &RemoteGraceSec);
	reg.GetStr(REBOOT_STR, RemoteReboot, sizeof(RemoteReboot));
	reg.GetInt(REBOOTMODE_STR, &RemoteRebootMode);
	reg.GetStr(EXIT_STR, RemoteExit, sizeof(RemoteExit));
	reg.GetInt(EXITMODE_STR, &RemoteExitMode);
	reg.GetStr(IPMSGEXIT_STR, RemoteIPMsgExit, sizeof(RemoteIPMsgExit));
	reg.GetInt(IPMSGEXITMODE_STR, &RemoteIPMsgExitMode);

	if (!isChangeApp) {
		reg.GetInt64(LASTWND_STR, &lastWnd);
	}

	if (NoTcp) {
		ClipMode = 0;
		NoFileTrans = 1;
	}
	else {
		reg.GetInt(CLIPMODE_STR, &ClipMode);
		ClipMode |= CLIP_ENABLE;
	}
	if (NoFileTrans == 1) {
		ClipMode &= ~CLIP_ENABLE;
	}

	reg.GetInt(CLIPMAX_STR, &ClipMax);
	reg.GetInt(CAPTUREDELAY_STR, &CaptureDelay);
	reg.GetInt(CAPTUREDELAYEX_STR, &CaptureDelayEx);
	reg.GetInt(CAPTUREMINIMIZE_STR, &CaptureMinimize);
	reg.GetInt(CAPTURECLIP_STR, &CaptureClip);
	reg.GetInt(CAPTURESAVE_STR, &CaptureSave);
	reg.GetInt(OPENMSGTIME_STR, &OpenMsgTime);
	reg.GetInt(RECVMSGTIME_STR, &RecvMsgTime);
	reg.GetInt(BALLOONNOINFO_STR, &BalloonNoInfo);
	reg.GetInt(TASKBARUI_STR, &TaskbarUI);
	reg.GetInt(MARKERTHICK_STR, &MarkerThick);
	reg.GetInt(WINE_STR, &Wine);
	if (Wine == -1) {
		Wine = IsWineEnvironment();
	}

	reg.GetInt(LOGVIEWDISPMODE_STR, &logViewDispMode);

	reg.GetStr(IPV6MULITCAST, IPv6Multicast, sizeof(IPv6Multicast));
	if (IsWinVista()) {
		reg.GetInt(IPV6MODE_STR, &IPv6Mode);
		if (nicAddr.size == 16 && !IPv6Mode) {
			IPv6Mode = 2;
		}
		reg.GetStr(IPV6MULITCAST, IPv6Multicast, sizeof(IPv6Multicast));
		if (!*IPv6Multicast) {
			strcpy(IPv6Multicast, IPMSG_DEFAULT_MULTICAST_ADDR6); // in ipmsg.h
		}
	}
	IPv6ModeNext = IPv6Mode;

// for File Transfer
	reg.GetInt(VIEWMAX_STR, &ViewMax);
	if (ViewMax < 1024 * 1024) {	// 1MB 以下の MapFileOfView は認めない
		ViewMax = IPMSG_DEFAULT_VIEWMAX;
	}
	reg.GetInt(TRANSMAX_STR, &TransMax);
	if (TransMax < 8 * 1024) {	// 8KB 以下の file transfer buf は認めない
		TransMax = IPMSG_DEFAULT_TRANSMAX;
	}
	reg.GetInt(TCPBUFMAX_STR, &TcpbufMax);
	if (TcpbufMax < 8 * 1024) {	// 8KB 以下の sockbuf は認めない
		TcpbufMax = IPMSG_DEFAULT_TCPBUFMAX;
	}
	reg.GetInt(IOBUFMAX_STR, &IoBufMax);
	if (IoBufMax < TransMax) {	// transmax 以下の IO/Buf は認めない
		IoBufMax = TransMax;
	}

	reg.GetInt(LUMPCHECK_STR, &LumpCheck);
	reg.GetInt(ENCTRANSCHECK_STR, &EncTransCheck);

// for Absence Message
	typedef char MaxBuf[MAX_PATH_U8];
	AbsenceStr = new MaxBuf[AbsenceMax];
	memset(AbsenceStr, 0, MAX_PATH_U8 * AbsenceMax);
	reg.GetStr(ABSENCESTR_STR, AbsenceStr[0], sizeof(AbsenceStr[0]));

	typedef char MaxHead[MAX_NAMEBUF];
	AbsenceHead = new MaxHead[AbsenceMax];
	memset(AbsenceHead, 0, MAX_NAMEBUF * AbsenceMax);

	if (reg.CreateKey(ABSENCESTR_STR)) {
		for (int i=0; i < AbsenceMax; i++) {
			char	key[MAX_PATH_U8];

			sprintf(key, "%s%d", ABSENCESTR_STR, i);
			if (!reg.GetStr(key, AbsenceStr[i], sizeof(AbsenceStr[i])))
				strncpyz(AbsenceStr[i], DefaultAbsence[i < IPMSG_DEFAULT_ABSENCEMAX ? i : 0], MAX_PATH_U8);

			sprintf(key, "%s%d", ABSENCEHEAD_STR, i);
			if (!reg.GetStr(key, AbsenceHead[i], sizeof(AbsenceHead[i])))
				strcpy(AbsenceHead[i], DefaultAbsenceHead[i < IPMSG_DEFAULT_ABSENCEMAX ? i : 0]);
		}
		reg.CloseKey();
	}

// Find User
	typedef char MaxFind[MAX_NAMEBUF];
	if (reg.CreateKey(FINDSTR_STR)) {
		reg.GetInt(FINDMAX_STR, &FindMax);
		reg.GetInt(FINDALL_STR, &FindAll);
		FindStr = new MaxFind[FindMax];

		for (int i=0; i < FindMax; i++) {
			char	key[MAX_PATH_U8];
			sprintf(key, "%d", i);
			if (!reg.GetStr(key, FindStr[i], sizeof(FindStr[i]))) {
				FindStr[i][0] = '\0';
			}
		}
		reg.CloseKey();
	}
	else {
		FindStr = new MaxFind[FindMax];
		memset(FindStr, 0, MAX_NAMEBUF * FindMax);
	}

	reg.GetInt(LRUUSERMAX_STR, &lruUserMax);
	if (reg.OpenKey(LRUUSERKEY_STR)) {
		for (int i=0; ; i++) {
			sprintf(buf, "%d", i);
			if (!reg.OpenKey(buf)) {
				break;
			}
			UsersObj *obj = new UsersObj;
			for (int j=0; ; j++) {
				sprintf(buf, "%d", j);
				if (!reg.OpenKey(buf)) {
					break;
				}
				UserObj *u = new UserObj;
				if (reg.GetStr(USERNAME_STR, u->hostSub.u.userName, sizeof(u->hostSub.u.userName))
				 && reg.GetStr(HOSTNAME_STR, u->hostSub.u.hostName, sizeof(u->hostSub.u.hostName)))
				{
					obj->users.AddObj(u);
				}
				reg.CloseKey();
			}
			reg.CloseKey();
			lruUserList.AddObj(obj);
		}
		reg.CloseKey();
	}

	PasswordUse = 0;
	*PasswordStr = 0;
	reg.GetInt(PASSWORDUSE_STR, &PasswordUse);
	PasswordUseNext = PasswordUse;
	if (PasswordUse) {
		reg.GetStr(PASSWORD_STR, PasswordStr, sizeof(PasswordStr));
		reg.GetInt(PASSWDLOGCHECK_STR, &PasswdLogCheck);
	}
	reg.GetInt(DELAYTIME_STR, &DelayTime);
	reg.GetInt(QUOTECHECK_STR, &QuoteCheck);
	reg.GetInt(SECRETCHECK_STR, &SecretCheck);

	reg.GetInt(RECVLOGONDISP_STR, &RecvLogonDisp);
	reg.GetInt(RECVIPADDRCHECK_STR, &RecvIPAddr);
	reg.GetInt(ONECLICKPOPUP_STR, &OneClickPopup);
	reg.GetInt(BALLOONNOTIFY_STR, &BalloonNotify);
	reg.GetInt(TRAYICON_STR, &TrayIcon);
	reg.GetInt(ABNORMALBTN_STR, &AbnormalButton);
	reg.GetInt(DIALUPCHECK_STR, &DialUpCheck);
	reg.GetInt(ABSENCENONPOPUP_STR, &AbsenceNonPopup);
	reg.GetInt(LOGVIEWATRECV_STR, &logViewAtRecv);

	reg.GetStr(NICKNAMESTR_STR, NickNameStr, sizeof(NickNameStr));
	StrictUTF8(NickNameStr);
	reg.GetStr(GROUPNAMESTR_STR, GroupNameStr, sizeof(GroupNameStr));
	StrictUTF8(GroupNameStr);
	reg.GetLong(SORT_STR, (long *)&Sort);
//	reg.GetInt(UPDATETIME_STR, &UpdateTime);
	reg.GetInt(KEEPHOSTTIME_STR, &KeepHostTime);
	reg.GetInt(DISPHOSTTIME_STR, &DispHostTime);
	reg.GetInt(DEFAULTURL_STR, &DefaultUrl);
	reg.GetInt(SHELLEXEC_STR, &ShellExec);
	reg.GetInt(EXTENDENTRY_STR, &ExtendEntry);
	reg.GetInt(EXTENDBROADCAST_STR, &ExtendBroadcast);
	if (ExtendBroadcast == 0) {
		ExtendBroadcast = 2;
	}

	reg.GetInt(MULTICASTMODE_STR, &MulticastMode);

	reg.GetInt(CONTROLIME_STR, &ControlIME);
	reg.GetInt(GRIDLINE_STR, &GridLineCheck);
	reg.GetInt(COLUMNITEMS_STR, (int *)&ColumnItems);
	reg.GetStr(QUOTESTR_STR, QuoteStr, sizeof(QuoteStr));

	if (reg.CreateKey(HOTKEY_STR)) {
		reg.GetInt(HOTKEYCHECK_STR, &HotKeyCheck);
		reg.GetInt(HOTKEYMODIFY_STR, &HotKeyModify);
		reg.GetInt(HOTKEYSEND_STR, &HotKeySend);
		reg.GetInt(HOTKEYRECV_STR, &HotKeyRecv);
		reg.GetInt(HOTKEYMISC_STR, &HotKeyMisc);
		reg.GetInt(HOTKEYLOGVIEW_STR, &HotKeyLogView);
		reg.CloseKey();
	}
	reg.GetInt(LOGCHECK_STR, &LogCheck);
	reg.GetInt(LOGUTF8_STR, &LogUTF8);
	reg.GetInt(LOGONLOG_STR, &LogonLog);
	reg.GetInt(IPADDRCHECK_STR, &IPAddrCheck);

	if (!reg.GetStr(LOGFILE_STR, LogFile, sizeof(LogFile)) || !*LogFile) {
		strncpyz(LogFile, IPMSG_LOGNAME, sizeof(LogFile));
	}
	LogMng::StrictLogFile(LogFile);

	reg.GetStr(SOUNDFILE_STR, SoundFile, sizeof(SoundFile));
	reg.GetStr(ICON_STR, IconFile, sizeof(IconFile));
	reg.GetStr(REVICON_STR, RevIconFile, sizeof(RevIconFile));
	reg.GetStr(LASTOPEN_STR, lastOpenDir, sizeof(lastOpenDir));
	reg.GetStr(LASTSAVE_STR, lastSaveDir, sizeof(lastSaveDir));

	if (!*lastOpenDir) {
		SHGetSpecialFolderPathU8(0, lastOpenDir, CSIDL_DESKTOPDIRECTORY, FALSE);
	}
	if (!*lastSaveDir) {
		SHGetSpecialFolderPathU8(0, lastSaveDir, CSIDL_DESKTOPDIRECTORY, FALSE);
	}

	reg.GetStr(AUTOSAVEDIR_STR,   autoSaveDir, sizeof(autoSaveDir));
	reg.GetInt(AUTOSAVETOUT_STR,  &autoSaveTout);
	reg.GetInt(AUTOSAVEMAX_STR,   &autoSaveMax);
	reg.GetInt(AUTOSAVEFLAGS_STR, &autoSaveFlags);
	reg.GetInt(AUTOSAVELEVEL_STR, &autoSaveLevel);
	reg.GetInt(DOWNLOADLINK_STR, &downloadLink);

	reg.GetInt(WAITUPDATE_STR, &waitUpdate);
	//reg.GetInt(AUTOUPDATEMODE_STR, &autoUpdateMode);
	reg.GetInt64(VIEWEPOCHSAVE_STR, &viewEpochSave);
	if (viewEpochSave && viewEpochSave > time(NULL)) {
		viewEpochSave = time(NULL);
	}
	if (viewEpochSave && viewEpoch > viewEpochSave) {
		viewEpoch = viewEpochSave;
	}

	useLockName = 0;
	reg.GetInt(USELOCKNAME_STR, &useLockName);
	strncpyz(LockName, LoadStrU8(IDS_LOCKNAME), sizeof(LockName));
	reg.GetStr(LOCKNAME_STR, LockName, sizeof(LockName));

	lastVacuum = 0;
	reg.GetInt64(LASTVACUUM_STR, &lastVacuum);

	if (reg.CreateKey(LINKACT_STR)) {
		strncpyz(directOpenExt, DefaultDirectOpenExt, sizeof(directOpenExt));
		reg.GetStr(DIRECTOPENEXT_STR, directOpenExt, sizeof(directOpenExt));
		StrToExtVec(directOpenExt, &directExtVec);

		linkDetectMode = 0;
		clickOpenMode  = 0;
		reg.GetInt(LINKDETECTMODE_STR, &linkDetectMode);
		reg.GetInt(CLICKOPENMODE_STR, &clickOpenMode);

		reg.CloseKey();
	}

// Send/Recv Window Location
	if (reg.CreateKey(WINSIZE_STR)) {
		reg.GetInt(SENDNICKNAME_STR, &SendWidth[SW_NICKNAME]);
		reg.GetInt(SENDUSERNAME_STR, &SendWidth[SW_USER]);
		reg.GetInt(SENDABSENCE_STR, &SendWidth[SW_ABSENCE]);
		reg.GetInt(SENDPRIORITY_STR, &SendWidth[SW_PRIORITY]);
		reg.GetInt(SENDGROUPNAME_STR, &SendWidth[SW_GROUP]);
		reg.GetInt(SENDHOSTNAME_STR, &SendWidth[SW_HOST]);
		reg.GetInt(SENDIPADDR_STR, &SendWidth[SW_IPADDR]);

		if (reg.CreateKey(SENDORDER_STR)) {
			for (int i=0; i < MAX_SENDWIDTH; i++) {
				sprintf(buf, "%d", i);
				reg.GetInt(buf, &SendOrder[i]);
			}
			reg.CloseKey();
		}
		int idx = 0;
		for ( ; idx < MAX_SENDWIDTH; idx++) {
			if (SendOrder[idx] >= MAX_SENDWIDTH || SendOrder[idx] < 0) {
				break;
			}
			int i2;
			for (i2=idx+1; i2 < MAX_SENDWIDTH; i2++) {
				if (SendOrder[idx] == SendOrder[i2]) {
					break;
				}
			}
			if (i2 != MAX_SENDWIDTH)
				break;
		}
		if (idx != MAX_SENDWIDTH) { // reset
			for (int i=0; i < MAX_SENDWIDTH; i++) {
				SendOrder[i] = i;
			}
		}

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
		reg.GetInt(HISTMSG_STR, &HistWidth[HW_MSG]);
	//	reg.GetInt(HISTID_STR, &HistWidth[HW_ID]);

		reg.GetInt(LVX_STR, &LvX);
		reg.GetInt(LVY_STR, &LvY);
		reg.GetInt(LVCX_STR, &LvCx);
		reg.GetInt(LVCY_STR, &LvCy);

		reg.CloseKey();
	}

// Send/Recv Window Font
	if (reg.CreateKey(FONT_STR)) {
		ReadFontRegistry(&reg, SENDEDITFONT_STR, &SendEditFont);
		ReadFontRegistry(&reg, SENDLISTFONT_STR, &SendListFont);
		ReadFontRegistry(&reg, RECVHEADFONT_STR, &RecvHeadFont);
		ReadFontRegistry(&reg, RECVEDITFONT_STR, &RecvEditFont);
		ReadFontRegistry(&reg, LOGVIEWFONT_STR,  &LogViewFont);
		reg.CloseKey();
	}

	DirMode = DIRMODE_NONE;

#ifdef IPMSG_PRO
#define CFG_READREG
#include "miscext.dat"
#undef CFG_READREG
#else
	*masterSvr = 0;
	if (IPv6Mode != 1) {
		reg.GetInt(DIRMODE_STR, (int *)&DirMode);
		reg.GetStr(MASTERSVR_STR, masterSvr, sizeof(masterSvr));
		reg.GetInt(DIRMODE_STR, (int *)&DirMode);
		if (DirMode == DIRMODE_USER) {
			if (*masterSvr) {
				if (!masterAddr.Set(masterSvr)) {
					masterAddr.Resolve(masterSvr);
				}
			}
		}
	}

	pollTick = IPMSG_DEFAULT_MASTERPOLL;
	reg.GetInt(MASTERPOLL_STR, (int *)&pollTick);
	if (pollTick < 5000) {
		pollTick = IPMSG_DEFAULT_MASTERPOLL;
	}
	dirSpan = IPMSG_DEFAULT_DIRSPAN;
	reg.GetInt(DIRSPAN_STR, &dirSpan);
	if (dirSpan <= 0) {
		dirSpan = IPMSG_DEFAULT_DIRSPAN;
	}
	DirAgentNum = IPMSG_DEFAULT_DIRAGENTNUM;
	reg.GetInt(DIRAGENTNUM_STR, &DirAgentNum);
	if (DirAgentNum <= 1 || DirAgentNum > 4) {
		DirAgentNum = IPMSG_DEFAULT_DIRAGENTNUM;
	}

#endif
	masterStrict = 0;
	reg.GetInt(MASTERSTRICT_STR, &masterStrict);

// Broadcast Data
	if (reg.CreateKey(BROADCAST_STR)) {
		for (int i=0; ; i++) {
			char	val[MAX_BUF];
			sprintf(buf, "%d", i);
			if (!reg.GetStr(buf, val, sizeof(val))) {
				break;
			}
			brList.SetHostRaw(val, (ResolveOpt & RS_REALTIME) ? 0 : ResolveAddr(val));
			Addr addr(val);
		}
		reg.CloseKey();
	}

// Clickable URL
	for (int i=0; DefaultUrlProtocol[i]; i++) {
		UrlObj *obj = new UrlObj;
		strncpyz(obj->protocol, DefaultUrlProtocol[i], sizeof(obj->protocol));
		*obj->program = 0;
		urlList.AddObj(obj);
	}

	InitLinkRe();

//	if (reg.CreateKey(CLICKABLEURL_STR))
//	{
//		for (i = 0; reg.EnumValue(i, buf, sizeof(buf)) || reg.EnumKey(i, buf, sizeof(buf)); i++)
//		{
//			UrlObj *obj;
//			for (obj = urlList.TopObj(); obj; obj = urlList.NextObj(obj)) {
//				if (stricmp(obj->protocol, buf) == 0)
//					break;
//			}
//
//			if (obj == NULL)
//			{
//				obj = new UrlObj;
//				urlList.AddObj(obj);
//			}
//			strncpyz(obj->protocol, buf, sizeof(obj->protocol));
//			reg.GetStr(buf, obj->program, sizeof(obj->program));
//		}
//		reg.CloseKey();
//	}

// User Sort Priority
	if (reg.CreateKey(PRIORITY_STR)) {
		reg.GetInt(PRIORITYMAX_STR, &PriorityMax);
		PriorityMax = max(PriorityMax, DEFAULT_PRIORITYMAX);
		reg.GetInt(PRIORITYREJECT_STR, &PriorityReject);
#if 0
		PrioritySound = new char *[PriorityMax];
		for (int i=0; i < PriorityMax; i++) {
			sprintf(buf, "%d", i);

			if (reg.CreateKey(buf)) {
				if (reg.GetStr(PRIORITYSOUND_STR, buf, sizeof(buf)) && *buf != '\0')
					PrioritySound[i] = strdup(buf);
				reg.CloseKey();
			}
		}
#endif
		reg.CloseKey();
	}

	priorityHosts.Enable(THosts::NAME, true);
	fileHosts.Enable(THosts::NAME_ADDR, true);

	if (reg.OpenKey(HOSTINFO_STR)) {
		char		key[MAX_BUF];
		time_t		default_time = time(NULL) - KeepHostTime / 2;	// 90 日
		DynBuf		data(MAX_UDPBUF);
		list<U8str>	del_item;

		for (int i=0; reg.EnumValue(i, key, sizeof(key)); i++) {
			IPDict	d;
			int		len = MAX_UDPBUF;
			auto	host = new Host;

			do {
				if (!reg.GetByte(key, data, &len) || !len || d.unpack(data, len) != len) break;
				int64	val;
				U8str	s;

				if (!d.get_int(PRIORITY_KEY, &val)) break;
				host->priority = (int)val;
				if (!d.get_int(HOSTSTAT_KEY, &val)) break;
				host->hostStatus = (ULONG)val & ~IPMSG_ABSENCEOPT;

				if (!d.get_str(UNAME_KEY, &s)) break;
				strncpyz(host->hostSub.u.userName, s.s(), MAX_NAMEBUF);
				if (!d.get_str(HNAME_KEY, &s)) break;
				strncpyz(host->hostSub.u.hostName, s.s(), MAX_NAMEBUF);

				if (!d.get_str(ADDR_KEY, &s)) break;
				host->hostSub.addr.Set(s.s());
				if (!d.get_int(PORT_KEY, &val)) break;
				host->hostSub.portNo = (int)val;

				if (!d.get_str(NICK_KEY, &s)) break;
				strncpyz(host->nickName, s.s(), MAX_NAMEBUF);
				if (!d.get_str(GROUP_KEY, &s)) break;
				strncpyz(host->groupName, s.s(), MAX_NAMEBUF);
				if (d.get_str(ALTER_KEY, &s)) {
					strncpyz(host->alterName, s.s(), MAX_NAMEBUF);
				}
				if (!d.get_int(UPTIME_KEY, &val)) break;
				host->updateTime = (int)val;
				if (host->updateTime < default_time) break;

				DynBuf	pub;
				if (d.get_bytes(PUBKEY_KEY, &pub)) {
					host->pubKey.DeSerialize(pub.Buf(), (int)pub.UsedSize());
				}
				priorityHosts.AddHost(host);
				host = NULL;
			} while (0);

			if (host) {
				delete host;
				del_item.push_back(key);
			}
		}
		for (auto &k: del_item) {
			reg.DeleteValue(k.s());
		}
		reg.CloseKey();
	}
	else {
		ConvertHostInfo(&reg);
		saveRegFlg |= CFG_HOSTINFO;
	}

// Self private/public key (1024/2048)
	for (int i=KEY_1024; i <= KEY_2048; i++) {
		const char *key = (i == KEY_1024) ? CRYPT_STR : CRYPT2_STR;
		int			need_blob_size = ((i == KEY_1024) ? 1024 : 2048) / 8 * 4;

		if (reg.CreateKey(key)) {
			BYTE	key[MAX_BUF_EX];
			int		blen = sizeof(key);
			int		slen = sizeof(key);

			if (reg.GetByte(PRIVBLOB_STR, key, &blen) && blen > need_blob_size) {
				priv[i].SetBlob(key, blen);
			}
			if (reg.GetByte(PRIVSEED_STR, key, &slen)) {
				priv[i].SetSeed(key, slen);
			}
			reg.GetInt(PRIVTYPE_STR, &priv[i].encryptType);
			reg.CloseKey();
		}
	}

	officialPub.Set(official_n, (int)sizeof(official_n), official_e,
		IPMSG_RSA_2048|IPMSG_SIGN_SHA256|IPMSG_IPDICT_CTR);

// Imported Registry Check
	*regOwner = 0;
	reg.GetStr(REGOWNER_STR, regOwner, sizeof(regOwner));

	WCHAR	wbuf[MAX_NAMEBUF] = L"";
	DWORD	wLen = wsizeof(wbuf);

	if (::GetUserNameW(wbuf, &wLen)) {
		U8str	user(wbuf);
		bool	is_set   = *regOwner ? true : false;
		bool	is_owner = !is_set || strcmpi(user.s(), regOwner) == 0;

		if (!is_set || !is_owner) {
			strncpyz(regOwner, user.s(), sizeof(regOwner));
			saveRegFlg |= CFG_GENERAL;

			if (!is_owner) {
				if (TMessageBoxU8(Fmt(LoadStrU8(IDS_RECONFIG_FMT), regOwner, user.s()),
					IP_MSG, MB_OKCANCEL) == IDOK) {
					for (int i=0; i < MAX_KEY; i++) {
						priv[i].UnSet();
					}
					priv[KEY_1024].encryptType = PRIV_BLOB_RESET;
					strncpyz(LogFile, "ipmsg.log", sizeof(LogFile));
					LogMng::StrictLogFile(LogFile);
					saveRegFlg |= CFG_GENERAL | CFG_CRYPT;
				}
			}
		}
	}

// Find String for LogViewer
	if (reg.CreateKey(LVFINDSTR_STR)) {
		reg.GetInt(LVFINDMAX_STR, &LvFindMax);

		for (int i=0; i < LvFindMax; i++) {
			WCHAR	key[MAX_PATH_U8];
			WCHAR	val[MAX_PATH_U8];

			swprintf(key, L"%d", i);
			if (reg.GetStrW(key, val, sizeof(val))) {
				TWstrObj *obj = new TWstrObj;
				obj->w = val;
				LvFindList.AddObj(obj);
			}
		}
		reg.CloseKey();
	}

#ifdef IPMSG_PRO
#define CFG_LOADUPLOAD
#include "miscext.dat"
#undef CFG_LOADUPLOAD
#endif

	if (gEnableHook) {
		if (reg.CreateKey(HOOK_STR)) {
			hookMode = -1;
			hookKind = 1;
			hookUrl =
				"https://hooks.slack.com/services/Txxxxxxxx/Bxxxxxxxx/xxxxxxxxxxxxxxxxxxxxxxxx";
			hookBody =
				"{ \"channel\":\"@user\", \"username\":\"$(sender)\", \"text\":\"$(msg)\" }";

			reg.GetInt(HOOKMODE_STR, &hookMode);
			reg.GetInt(HOOKKIND_STR, &hookKind);

			if (reg.GetStr(HOOKURL_STR, buf, sizeof(buf))) {
				hookUrl = buf;
			}
			if (reg.GetStr(BODY_STR, buf, sizeof(buf))) {
				hookBody = buf;
			}
			reg.CloseKey();
		}

		if (reg.CreateKey(SLACK_STR)) {
			if (hookMode == -1) {
				hookMode = 0;
				reg.GetInt(HOOKMODEOLD_STR, &hookMode);
			}
			slackHost = "hooks.slack.com";
			slackKey = "/Txxxxxxxx/Bxxxxxxxx/xxxxxxxxxxxxxxxxxxxxxxxx";
			slackChan = "@username";
			slackIcon = "https://ipmsg.org/ipmsg-slack.png";

			if (reg.GetStr(HOST_STR, buf, sizeof(buf))) {
				slackHost = buf;
			}
			if (reg.GetStr(KEY_STR, buf, sizeof(buf))) {
				slackKey = buf;
			}
			if (reg.GetStr(CHAN_STR, buf, sizeof(buf))) {
				slackChan = buf;
			}
			if (reg.GetStr(ICON_STR, buf, sizeof(buf))) {
				slackIcon = buf;
			}
			reg.CloseKey();
		}
	}

#ifndef IPMSG_PRO
	if (reg.CreateKey(UPDATE_STR)) {
		updateFlag = UPDATE_ON;
		updateSpan = 3600 * 24;
		updateLast = 0;
		if (!reg.GetInt(UPDATEFLAG_STR, (int *)&updateFlag)) {
			if (reg.GetInt(UPDATEFLAGOLD_STR, (int *)&updateFlag) && updateFlag == 0) {
				updateFlag = UPDATE_ON;
			}
		}
		reg.GetInt(UPDATESPAN_STR, (int *)&updateSpan);
		reg.GetInt64(UPDATELAST_STR, &updateLast);
		reg.CloseKey();
	}
#endif

	if (!reg.GetStr(NULL, buf, sizeof(buf)) || strcmp(buf, GetVersionStr())) {
		saveRegFlg |= CFG_ALL;
	}

	if (saveRegFlg && !isChangeApp) {
		reg.CloseKey();		// close top appreg
		WriteRegistry(saveRegFlg);
	}

	return	true;
}

void Cfg::ConvertHostInfo(TRegistry *reg)
{
	if (!reg->OpenKey(HOSTINFOOLD_STR)) return;

	char	key[MAX_BUF];
	for (int i=0; reg->EnumKey(i, key, sizeof(key)); i++) {
		if (!reg->OpenKey(key)) continue;

		auto	host = new Host;
		do {
			int		priority = DEFAULT_PRIORITY;
			reg->GetInt(PRIORITY_STR, &priority);
			host->priority = priority;
			host->hostStatus = IPMSG_FULLSTAT;
			if (!reg->GetStr(USERNAME_STR, host->hostSub.u.userName, MAX_NAMEBUF)) break;
			if (!reg->GetStr(HOSTNAME_STR, host->hostSub.u.hostName, MAX_NAMEBUF)) break;

			char	buf[MAX_BUF];
			if (reg->GetStr(IPADDR_STR, buf, sizeof(buf))) {
				host->hostSub.addr.Set(buf);
			}
			reg->GetInt(PORTNO_STR, &host->hostSub.portNo);
			reg->GetStr(NICKNAME_STR, host->nickName, MAX_NAMEBUF);
			reg->GetStr(GROUPNAME_STR, host->groupName, MAX_NAMEBUF);
			reg->GetStr(ALTERNAME_STR, host->alterName, MAX_NAMEBUF);
			reg->GetInt64(UPDATETIME_STR, &host->updateTime);
			BYTE	pubkey[512];
			int		pubkeySize = sizeof(pubkey);
			if (!reg->GetByte(PUBLICKEY_STR, pubkey, &pubkeySize)) pubkeySize = 0;
			if (pubkeySize) {
				host->pubKey.DeSerialize(pubkey, pubkeySize);
			}
			priorityHosts.AddHost(host);
			host = NULL;
		} while (0);

		if (host) {
			delete host;
		}
		reg->CloseKey();
	}
	reg->CloseKey();
}

bool Cfg::ReadFontRegistry(TRegistry *reg, char *key, LOGFONT *font)
{
	if (reg->CreateKey(key)) {
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
	return	true;
}


bool Cfg::WriteRegistry(int ctl_flg)
{
	char	buf[MAX_LISTBUF];

	GetRegName(buf, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, buf);

	if (ctl_flg & CFG_GENERAL) {
		reg.SetStr(NULL, GetVersionStr());
		reg.SetInt(LCID_KEY, lcid);
		reg.SetInt(NOBEEP_STR, NoBeep);
//		reg.SetInt(NOTCP_STR, NoTcp);
//		reg.SetInt(NOFILETRANS_STR, NoFileTrans);
		reg.SetStr(REGOWNER_STR, regOwner);

		reg.SetInt(LISTGET_STR, ListGet);
		reg.SetInt(LISTGETMSEC_STR, (int)ListGetMSec);
		reg.SetInt(RETRYMSEC_STR, (int)RetryMSec);
		reg.SetInt(RETRYMAX_STR, (int)RetryMax);
//		reg.SetInt(RECVMAX_STR, RecvMax);
		reg.SetInt64(LASTRECV_STR, LastRecv);
		reg.SetInt(NOERASE_STR, NoErase);
		reg.SetInt(REPROMSG_STR, ReproMsg);
//		reg.SetInt(DEBUG_STR, Debug);
		reg.SetInt(FWCHECKMODE_STR, FwCheckMode);
		reg.SetInt(FW3RDPARTY_STR, Fw3rdParty);
		reg.SetInt(RECVICONMODE_STR, RecvIconMode);

		reg.SetInt(NOPOPUPCHECK_STR, NoPopupCheck);
		reg.SetInt(OPENCHECK_STR, OpenCheck);
		reg.SetInt(REPLYFILTER_STR, ReplyFilter);
		reg.SetInt(ALLOWSENDLIST_STR, AllowSendList);
		reg.SetInt(FILETRANSOPT_STR, fileTransOpt);
		reg.SetInt(RESOLVEOPT_STR, ResolveOpt);
//		reg.SetInt(LETTERKEY_STR, LetterKey);
		reg.SetInt(LISTCONFIRMKEY_STR, ListConfirm);
		reg.SetInt(DELAYSEND_STR, DelaySend);

//		reg.SetInt(REMOTEGRACE_STR, RemoteGraceSec);
		reg.SetStr(REBOOT_STR, RemoteReboot);
		reg.SetInt(REBOOTMODE_STR, RemoteRebootMode);
		reg.SetStr(EXIT_STR, RemoteExit);
		reg.SetInt(EXITMODE_STR, RemoteExitMode);
		reg.SetStr(IPMSGEXIT_STR, RemoteIPMsgExit);
		reg.SetInt(IPMSGEXITMODE_STR, RemoteIPMsgExitMode);

		reg.SetInt(CLIPMODE_STR, ClipMode);
//		reg.SetInt(CLIPMAX_STR, ClipMax);
//		reg.SetInt(CAPTUREDELAY_STR, CaptureDelay);
//		reg.SetInt(CAPTUREDELAYEX_STR, CaptureDelayEx);
		reg.SetInt(CAPTUREMINIMIZE_STR, CaptureMinimize);
		reg.SetInt(CAPTURECLIP_STR, CaptureClip);
		reg.SetInt(CAPTURESAVE_STR, CaptureSave);
		reg.SetInt(OPENMSGTIME_STR, OpenMsgTime);
		reg.SetInt(RECVMSGTIME_STR, RecvMsgTime);
		reg.SetInt(BALLOONNOINFO_STR, BalloonNoInfo);
//		reg.SetInt(TASKBARUI_STR, TaskbarUI);
//		reg.SetInt(MARKERTHICK_STR, MarkerThick);

		reg.SetInt(LOGVIEWDISPMODE_STR, logViewDispMode);

		if (IsWinVista()) reg.SetInt(IPV6MODE_STR, IPv6ModeNext);
//		if (IsWinVista()) reg.SetStr(IPV6MULITCAST, IPv6Multicast);

//		reg.SetInt(VIEWMAX_STR, ViewMax);
//		reg.SetInt(TRANSMAX_STR, TransMax);
//		reg.SetInt(TCPBUFMAX_STR, TcpbufMax);
//		reg.SetInt(IOBUFMAX_STR, IoBufMax);
		reg.SetInt(LUMPCHECK_STR, LumpCheck);
		reg.SetInt(ENCTRANSCHECK_STR, EncTransCheck);
	}

	if (ctl_flg & CFG_LASTWND) {
		reg.SetInt64(LASTWND_STR, lastWnd);
	}

	if (ctl_flg & CFG_ABSENCE) {
		reg.SetInt(ABSENCESAVE_STR, AbsenceSave);
		reg.SetInt(ABSENCECHECK_STR, AbsenceCheck);
		if (AbsenceSave) {
			reg.SetInt(ABSENCECHOICE_STR, AbsenceChoice);
		}
		reg.SetInt(ABSENCEMAX_STR, AbsenceMax);

		if (reg.CreateKey(ABSENCESTR_STR)) {
			for (int i=0; i < AbsenceMax; i++) {
				char	key[MAX_PATH_U8];
				sprintf(key, "%s%d", ABSENCESTR_STR, i);
				reg.SetStr(key, AbsenceStr[i]);
				sprintf(key, "%s%d", ABSENCEHEAD_STR, i);
				reg.SetStr(key, AbsenceHead[i]);
			}
			reg.CloseKey();
		}
	}

	if (ctl_flg & CFG_GENERAL) {
		reg.SetInt(PASSWORDUSE_STR, PasswordUseNext);
		if (PasswordUse) {
			reg.SetStr(PASSWORD_STR, PasswordStr);
			reg.SetInt(PASSWDLOGCHECK_STR, PasswdLogCheck);
		}

		//reg.SetInt(DELAYTIME_STR, DelayTime);
		reg.SetInt(QUOTECHECK_STR, QuoteCheck);
		reg.SetInt(SECRETCHECK_STR, SecretCheck);

		reg.SetInt(LOGCHECK_STR, LogCheck);
		//reg.SetInt(LOGUTF8_STR, LogUTF8);
		reg.SetStr(LOGFILE_STR, LogFile);
		//reg.SetInt(LOGONLOG_STR, LogonLog);
		//reg.SetInt(IPADDRCHECK_STR, IPAddrCheck);

		reg.SetInt(RECVLOGONDISP_STR, RecvLogonDisp);
		reg.SetInt(RECVIPADDRCHECK_STR, RecvIPAddr);
		reg.SetInt(ONECLICKPOPUP_STR, OneClickPopup);
		reg.SetInt(BALLOONNOTIFY_STR, BalloonNotify);		
		reg.SetInt(TRAYICON_STR, TrayIcon);
		reg.SetInt(ABNORMALBTN_STR, AbnormalButton);
		reg.SetInt(DIALUPCHECK_STR, DialUpCheck);
		//reg.SetInt(ABSENCENONPOPUP_STR, AbsenceNonPopup);
		reg.SetInt(LOGVIEWATRECV_STR, logViewAtRecv);

		reg.SetStr(NICKNAMESTR_STR, NickNameStr);
		reg.SetStr(GROUPNAMESTR_STR, GroupNameStr);
		reg.SetLong(SORT_STR, Sort);
		reg.SetInt(UPDATETIME_STR, UpdateTime);
		reg.SetInt(KEEPHOSTTIME_STR, KeepHostTime);
		reg.SetInt(DISPHOSTTIME_STR, DispHostTime);
		reg.SetInt(EXTENDENTRY_STR, ExtendEntry);
		reg.SetInt(EXTENDBROADCAST_STR, ExtendBroadcast);
		reg.SetInt(MULTICASTMODE_STR, MulticastMode);
		reg.SetInt(CONTROLIME_STR, ControlIME);
		reg.SetInt(GRIDLINE_STR, GridLineCheck);
		reg.SetInt(COLUMNITEMS_STR, ColumnItems);
		reg.SetStr(QUOTESTR_STR, QuoteStr);

		if (reg.CreateKey(HOTKEY_STR)) {
			reg.SetInt(HOTKEYCHECK_STR, HotKeyCheck);
			reg.SetInt(HOTKEYMODIFY_STR, HotKeyModify);
			reg.SetInt(HOTKEYSEND_STR, HotKeySend);
			reg.SetInt(HOTKEYRECV_STR, HotKeyRecv);
			reg.SetInt(HOTKEYMISC_STR, HotKeyMisc);
			reg.SetInt(HOTKEYLOGVIEW_STR, HotKeyLogView);
			reg.CloseKey();
		}
		reg.SetStr(SOUNDFILE_STR, SoundFile);
		reg.SetStr(ICON_STR, IconFile);
		reg.SetStr(REVICON_STR, RevIconFile);
		reg.SetStr(LASTOPEN_STR, lastOpenDir);
		reg.SetStr(LASTSAVE_STR, lastSaveDir);
		reg.SetInt(LRUUSERMAX_STR, lruUserMax);

		reg.SetStr(AUTOSAVEDIR_STR,   autoSaveDir);
		reg.SetInt(AUTOSAVETOUT_STR,  autoSaveTout);
		reg.SetInt(AUTOSAVEMAX_STR,   autoSaveMax);
		reg.SetInt(AUTOSAVELEVEL_STR, autoSaveLevel);
		reg.SetInt(AUTOSAVEFLAGS_STR, autoSaveFlags);
		//reg.SetInt(DOWNLOADLINK_STR, downloadLink);

		reg.SetInt(WAITUPDATE_STR, waitUpdate);
		//reg.SetInt(AUTOUPDATEMODE_STR, autoUpdateMode);
		reg.SetInt64(VIEWEPOCHSAVE_STR, viewEpochSave);

		reg.SetInt(USELOCKNAME_STR, useLockName);
//		reg.SetStr(LOCKNAME_STR, LockName);

		reg.SetInt64(LASTVACUUM_STR, lastVacuum);
	}

	if ((ctl_flg & CFG_LINKACT) && reg.CreateKey(LINKACT_STR)) {
		if (strcmpi(directOpenExt, DefaultDirectOpenExt)) {
			reg.SetStr(DIRECTOPENEXT_STR, directOpenExt);
		} else {
			reg.DeleteValue(DIRECTOPENEXT_STR);
		}
		StrToExtVec(directOpenExt, &directExtVec);
		if (linkDetectMode) {
			reg.SetInt(LINKDETECTMODE_STR, linkDetectMode);
		} else {
			reg.DeleteValue(LINKDETECTMODE_STR);
		}
		if (clickOpenMode) {
			reg.SetInt(CLICKOPENMODE_STR, clickOpenMode);
		} else {
			reg.DeleteValue(CLICKOPENMODE_STR);
		}
		reg.CloseKey();
	}

	if ((ctl_flg & CFG_WINSIZE) && reg.CreateKey(WINSIZE_STR)) {
		reg.SetInt(SENDNICKNAME_STR, SendWidth[SW_NICKNAME]);
		reg.SetInt(SENDUSERNAME_STR, SendWidth[SW_USER]);
		reg.SetInt(SENDABSENCE_STR, SendWidth[SW_ABSENCE]);
		reg.SetInt(SENDPRIORITY_STR, SendWidth[SW_PRIORITY]);
		reg.SetInt(SENDGROUPNAME_STR, SendWidth[SW_GROUP]);
		reg.SetInt(SENDHOSTNAME_STR, SendWidth[SW_HOST]);
		reg.SetInt(SENDIPADDR_STR, SendWidth[SW_IPADDR]);

		if (reg.CreateKey(SENDORDER_STR)) {
			for (int i=0; i < MAX_SENDWIDTH; i++) {
				sprintf(buf, "%d", i);
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
		reg.SetInt(HISTMSG_STR, HistWidth[HW_MSG]);
	//	reg.SetInt(HISTID_STR, HistWidth[HW_ID]);

		reg.SetInt(LVX_STR, LvX);
		reg.SetInt(LVY_STR, LvY);
		reg.SetInt(LVCX_STR, LvCx);
		reg.SetInt(LVCY_STR, LvCy);

		reg.CloseKey();
	}

	if ((ctl_flg & CFG_FONT) && reg.CreateKey(FONT_STR)) {
		WriteFontRegistry(&reg, SENDEDITFONT_STR, &SendEditFont);
		WriteFontRegistry(&reg, SENDLISTFONT_STR, &SendListFont);
		WriteFontRegistry(&reg, RECVHEADFONT_STR, &RecvHeadFont);
		WriteFontRegistry(&reg, RECVEDITFONT_STR, &RecvEditFont);
		WriteFontRegistry(&reg, LOGVIEWFONT_STR, &LogViewFont);
		reg.CloseKey();
	}

#ifdef IPMSG_PRO
#else
	reg.SetStr(MASTERSVR_STR, masterSvr);
//	reg.SetInt(MASTERPOLL_STR, pollTick);
//	reg.SetInt(MASTERSTRICT_STR, masterStrict);
	if (dirSpan == IPMSG_DEFAULT_DIRSPAN) {
		reg.DeleteValue(DIRSPAN_STR);
	}
	else {
		reg.SetInt(DIRSPAN_STR, dirSpan);
	}
	reg.SetInt(DIRMODE_STR, DirMode);

#endif


	if ((ctl_flg & CFG_BROADCAST) && reg.CreateKey(BROADCAST_STR)) {
		int i = 0;
		for (auto obj=brList.TopObj(); obj; obj=brList.NextObj(obj)) {
			sprintf(buf, "%d", i++);
			reg.SetStr(buf, obj->Host());
		}
		while (1) {
			char	val[MAX_LISTBUF];
			sprintf(buf, "%d", i++);
			if (!reg.GetStr(buf, val, sizeof(val))) {
				break;
			}
			if (!reg.DeleteValue(buf)) {
				break;
			}
		}
		reg.CloseKey();
	}

//	if (ctl_flg & CFG_CLICKURL)
//	{
//		reg.SetInt(DEFAULTURL_STR, DefaultUrl);
//		reg.SetInt(SHELLEXEC_STR, ShellExec);
//		if (reg.CreateKey(CLICKABLEURL_STR))
//		{
//			for (auto obj = urlList.TopObj(); obj; obj = urlList.NextObj(obj)) {
//				reg.SetStr(obj->protocol, obj->program);
//			}
//			reg.CloseKey();
//		}
//	}

	if ((ctl_flg & CFG_PRIORITY) && reg.CreateKey(PRIORITY_STR)) {
		reg.SetInt(PRIORITYMAX_STR, PriorityMax);
		reg.SetInt(PRIORITYREJECT_STR, PriorityReject);
#if 0
		for (i=0; i < PriorityMax; i++) {
			sprintf(buf, "%d", i);

			if (reg.CreateKey(buf)) {
				reg.SetStr(PRIORITYSOUND_STR, PrioritySound[i] ? PrioritySound[i] : "");
				reg.CloseKey();
			}
		}
#endif
		reg.CloseKey();
	}

	if ((ctl_flg & CFG_HOSTINFO) && reg.CreateKey(HOSTINFO_STR)) {
		time_t	now_time = time(NULL);

		if (ctl_flg & CFG_DELHOST) {
			reg.DeleteChildTree();
		}
		for (int i=0; i < priorityHosts.HostCnt(); i++) {
			Host	*host = priorityHosts.GetHost(i);

			if (host->updateTime + KeepHostTime < now_time) {
				DelHost(host, &reg);
			}
			else {
				WriteHost(host, &reg);
			}
		}
		reg.CloseKey();
	}

	if ((ctl_flg & CFG_FINDHIST) && reg.CreateKey(FINDSTR_STR)) {
		reg.SetInt(FINDMAX_STR, FindMax);
		reg.SetInt(FINDALL_STR, FindAll);

		for (int i=0; i < FindMax; i++) {
			char	key[MAX_PATH_U8];
			sprintf(key, "%d", i);
			reg.SetStr(key, FindStr[i]);
		}
		reg.CloseKey();
	}

	if (ctl_flg & CFG_LRUUSER) {
		reg.DeleteChildTree(LRUUSERKEY_STR);

		if (reg.CreateKey(LRUUSERKEY_STR)) {
			UsersObj *obj=lruUserList.TopObj();
			for (int i=0; obj; i++) {
				sprintf(buf, "%d", i);
				if (!reg.CreateKey(buf)) {
					break;
				}
				UserObj *u = obj->users.TopObj();
				for (int j=0; u; j++) {
					sprintf(buf, "%d", j);
					if (!reg.CreateKey(buf)) {
						break;
					}
					reg.SetStr(USERNAME_STR, u->hostSub.u.userName);
					reg.SetStr(HOSTNAME_STR, u->hostSub.u.hostName);
					reg.CloseKey();
					u = obj->users.NextObj(u);
				}
				reg.CloseKey();
				obj = lruUserList.NextObj(obj);
			}
			reg.CloseKey();
		}
	}

// private/public key
	if (ctl_flg & CFG_CRYPT) {
		for (int i=KEY_1024; i <= KEY_2048; i++) {
			const char *key = (i == KEY_1024) ? CRYPT_STR : CRYPT2_STR;
			if (priv[i].blob) {
				if (reg.CreateKey(key)) {
					reg.SetByte(PRIVBLOB_STR, priv[i].blob, priv[i].blobLen);
					if (priv[i].encryptSeed) {
						reg.SetByte(PRIVSEED_STR, priv[i].encryptSeed, priv[i].encryptSeedLen);
					}
					reg.SetInt(PRIVTYPE_STR, priv[i].encryptType);
					reg.CloseKey();
				}
			}
			else if (reg.OpenKey(key)) {
				reg.CloseKey();
				reg.DeleteKey(key);
			}
		}
	}

	if ((ctl_flg & CFG_LVFINDHIST) && reg.CreateKey(LVFINDSTR_STR)) {
		reg.SetInt(LVFINDMAX_STR, LvFindMax);
		int		cnt=0;
		for (auto obj=LvFindList.TopObj(); obj && cnt < LvFindMax; obj=LvFindList.NextObj(obj)) {
			WCHAR	key[MAX_BUF];
			swprintf(key, L"%d", cnt++);
			reg.SetStrW(key, obj->w.s());
		}
		for (int i=LvFindList.Num(); ; i++) {
			sprintf(buf, "%d", i);
			if (!reg.DeleteValue(buf)) {
				break;
			}
		}
		reg.CloseKey();
	}

#ifdef IPMSG_PRO
#define CFG_SAVEUPLOAD
#include "miscext.dat"
#undef CFG_SAVEUPLOAD
#endif

	if (gEnableHook) {
		if ((ctl_flg & CFG_HOOKOPT) && reg.CreateKey(HOOK_STR)) {
			reg.SetInt(HOOKMODE_STR, hookMode);
			reg.SetInt(HOOKKIND_STR, hookKind);
			reg.SetStr(HOOKURL_STR, hookUrl.s());
			reg.SetStr(BODY_STR, hookBody.s());
			reg.CloseKey();
		}

		if ((ctl_flg & CFG_SLACKOPT) && reg.CreateKey(SLACK_STR)) {
			reg.SetStr(HOST_STR, slackHost.s());
			reg.SetStr(KEY_STR, slackKey.s());
			reg.SetStr(CHAN_STR, slackChan.s());
			reg.SetStr(ICON_STR, slackIcon.s());
			reg.CloseKey();
		}
	}

#ifndef IPMSG_PRO
	if ((ctl_flg & CFG_UPDATEOPT) && reg.CreateKey(UPDATE_STR)) {
		reg.SetInt(UPDATEFLAG_STR, updateFlag);
		//reg.SetInt(UPDATESPAN_STR, updateSpan);
		reg.SetInt64(UPDATELAST_STR, updateLast);
		reg.CloseKey();
	}
#endif

	return	true;
}


bool Cfg::WriteFontRegistry(TRegistry *reg, char *key, LOGFONT *font)
{
	if (reg->CreateKey(key)) {
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
	return	true;
}

void Cfg::GetRegName(char *buf, Addr nic_addr, int port_no)
{

	buf += sprintf(buf, "%s", GetIdsIPMSG());
	if (port_no != IPMSG_DEFAULT_PORT) {
		buf += sprintf(buf, "%d", port_no);
	}
	if (nic_addr.IsEnabled()) {
		*buf++ = '_';
		nic_addr.S(buf);
	}
}

void Cfg::GetSelfRegName(char *buf)
{
	GetRegName(buf, nicAddr, portNo);
}

#define SAVEPACKETKEY_STR	"SavePackets"
#define MSGHEADKEY_STR		"msg_head"
#define MSGDICTKEY_STR		"msg_dict"
#define MSGBUFKEY_STR		"msg_buf"
#define MSGEXBUFKEY_STR		"msg_exbuf"
#define MSGPKTNOKEY_STR		"msg_pktno"
#define HEADKEY_STR			"head"
#define IMGBASEKEY_STR		"img_base"
#define AUTOSAVED_STR		"auto_saved"
#define MSGID_STR			"msg_id"

static void MakeSavedPacketKey(const MsgBuf *msg, char *key)
{
	sprintf(key, msg->msgId ? "%x:%s:%llx" : "%x:%s",
		msg->packetNo, msg->hostSub.u.userName, msg->msgId);
}

bool Cfg::SavePacket(const MsgBuf *msg, const char *head, ULONG img_base)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.CreateKey(SAVEPACKETKEY_STR)) {
		return false;
	}

	MakeSavedPacketKey(msg, key);
	if (!reg.CreateKey(key)) {
		return false;
	}

	reg.SetByte(MSGHEADKEY_STR, (BYTE *)msg, (int)offsetof(MsgBuf, msgBuf));
	if (msg->orgdict.key_num() > 0) {
		DynBuf	buf(msg->orgdict.pack_size());
		size_t	size = msg->orgdict.pack(buf, buf.Size());
		reg.SetByte(MSGDICTKEY_STR, buf, (int)size);
	}
	else {
		reg.SetByte(MSGBUFKEY_STR, (const BYTE *)msg->msgBuf.s(), (int)strlen(msg->msgBuf.s())+1);
		reg.SetByte(MSGEXBUFKEY_STR, (const BYTE *)msg->exBuf, (int)strlen(msg->exBuf.s())+1);
	}
	reg.SetByte(MSGPKTNOKEY_STR, (BYTE *)msg->packetNoStr, (int)strlen(msg->packetNoStr)+1);
	reg.SetByte(HEADKEY_STR, (BYTE *)head, (int)strlen(head)+1);
	reg.SetInt(IMGBASEKEY_STR, (int)img_base);
	if (msg->msgId) {
		reg.SetInt64(MSGID_STR, msg->msgId);
	}

	return true;
}

bool Cfg::UpdatePacket(const MsgBuf *msg, const char *auto_saved)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.OpenKey(SAVEPACKETKEY_STR)) return false;

	MakeSavedPacketKey(msg, key);

	if (!reg.OpenKey(key)) return false;
	return	reg.SetStr(AUTOSAVED_STR, auto_saved);
}

bool Cfg::LoadPacket(MsgMng *msgMng, int idx, MsgBuf *msg, char *head,
	ULONG *img_base, char *auto_saved)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);
	time_t		timestamp = 0;

	if (!reg.CreateKey(SAVEPACKETKEY_STR)) return false;

	while (1) {
		bool	broken_data = false;
		if (!reg.EnumKey(idx, key, sizeof(key))) return false;
		if (!reg.OpenKey(key)) return false;

		int size = (int)offsetof(MsgBuf, msgBuf);
		reg.GetByte(MSGHEADKEY_STR, (BYTE *)msg, &size);

		if (size < 252) {	// too old version header
			broken_data = true;
		}
		else if (size == 252) {
			// MsgBuf の Time_t -> time_t timestamp 移行では、
			// 後続の int64 により4byteギャップが存在
			// 結果 252byte。ギャップ部分だけ 0 クリア
			memset((char *)&msg->timestamp + 4, 0, 4);
			timestamp = msg->timestamp;

			if (!IsSavedPacket(msg) || msg->timestamp == 0) {
				broken_data = true;
			}
		}
		if (broken_data) {
			reg.CloseKey();
			if (!reg.DeleteKey(key)) {
				return false;
			}
			continue;
		}
		break;
	}

	DynBuf	dbuf(MAX_UDPBUF);
	int size = MAX_UDPBUF;
	if (reg.GetByte(MSGDICTKEY_STR, dbuf.Buf(), &size)) {
		msg->msgBuf.SetByStr(dbuf.s(), size);
		if (msg->ipdict.unpack(dbuf, size) <= 0 || !msgMng->ResolveDictMsg(msg)) {
			return	false;
		}
		msg->timestamp = timestamp; // ResolveDictMsg で現在になったのを戻す
	}
	else {
		reg.GetByte(MSGBUFKEY_STR, dbuf.Buf(), &size);
		msg->msgBuf.SetByStr(dbuf.s(), size);

		size = MAX_UDPBUF;
		reg.GetByte(MSGEXBUFKEY_STR, dbuf.Buf(), &size);
		msg->exBuf.SetByStr(dbuf.s(), size);
	}
	size = (int)sizeof(msg->packetNoStr);
	reg.GetByte(MSGPKTNOKEY_STR, (BYTE *)msg->packetNoStr, &size);

	size = MAX_LISTBUF;
	reg.GetByte(HEADKEY_STR, (BYTE *)head, &size);
	reg.GetInt(IMGBASEKEY_STR, (int *)img_base);

	*auto_saved = 0;
	reg.GetStr(AUTOSAVED_STR, auto_saved, MAX_PATH);
	msg->msgId = 0;
	reg.GetInt64(MSGID_STR, &msg->msgId);

	return true;
}

bool Cfg::DeletePacket(const MsgBuf *msg)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.CreateKey(SAVEPACKETKEY_STR)) return false;

	MakeSavedPacketKey(msg, key);

	return	reg.DeleteKey(key);
}

bool Cfg::IsSavedPacket(const MsgBuf *msg)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.CreateKey(SAVEPACKETKEY_STR)) return false;

	MakeSavedPacketKey(msg, key);
	return	reg.OpenKey(key);
}

bool Cfg::CleanupPackets()
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	return	reg.DeleteChildTree(SAVEPACKETKEY_STR);
}

bool Cfg::InitLinkRe()
{
#define URL_RE		L"([a-z]+)://[a-z.0-9!%%#$&'*+,/:;=?@\\-_~\\[\\]]+"
#define FILEURL_RE	L"file://[a-z.0-9!%%#$&'*+,/:;=?@\\-_~\\[\\]]+"

#define PATH_ENT_CORE	L"\"<>\\|:?\\*\\r\\n"
#define PATH_ENT_CORE_S	PATH_ENT_CORE L" "

#define PATH_ENT	L"[^" PATH_ENT_CORE_S   L"]"
#define PATH_ENT_S	L"[^" PATH_ENT_CORE     L"]"
#define PATH_ITEM	L"[^\\\\/" PATH_ENT_CORE_S  L"]"
#define PATH_ITEM_S	L"[^\\\\/" PATH_ENT_CORE    L"]"

// STL regex では事前参照が出来ないため、行頭 or スペース/タブでマッチさせることに。
//  スペース/タブでマッチした場合は、match側（AnalyzeUrlMsgCore）で取り除く
#define UNC_RE		L"(^|[ \t])\\\\\\\\"   PATH_ITEM   L"+" PATH_ENT   L"+"
#define UNC_S_RE	L"\"\\\\\\\\"          PATH_ITEM_S L"+" PATH_ENT_S L"+\""

#define FILE_RE		L"(^|[ \t])[a-z]:\\\\" PATH_ENT    L"+"
#define FILE_S_RE	L"\"[a-z]:\\\\"        PATH_ENT_S  L"+\""

// 実行ポリシー
//  URL系は file://のみ、ファイルと同じ扱い or file://は使えないようにする
//  ファイルは explorer で上位フォルダを開き、選択状態に
//  フォルダは explorer で開く
//  PDF/DOC等は、直接 shell execute したいところだが。
// 拡張子がない＆フォルダでない場合、or PATHEXT に含まれる拡張子、だけ警告

	urlex_re = new wregex(L"(" URL_RE L"|" UNC_S_RE L"|" UNC_RE L"|" FILE_S_RE  L"|" FILE_RE L")",
		regex_constants::icase);

	url_re = new wregex(URL_RE, regex_constants::icase);

	file_re = new wregex(L"(" UNC_S_RE L"|" UNC_RE L"|" FILE_S_RE  L"|" FILE_RE L")",
		regex_constants::icase);

	fileurl_re = new wregex(FILEURL_RE, regex_constants::icase);

	return true;
}

bool Cfg::StrToExtVec(const char *_s, vector<Wstr> *extVec)
{
	U8str	u(_s);
	extVec->clear();

#define EXT_SEP		" .\t\r\n"
	for (auto s=strtok(u.Buf(), EXT_SEP); s; s=strtok(NULL, EXT_SEP)) {
		extVec->push_back(s);
	}
	return	true;
}

#define SAVESHARE_STR	"SaveShare"
#define SHAREDICT_STR	"ShareDict"

bool Cfg::SaveShare(UINT packet_no, const IPDict& dict)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.CreateKey(SAVESHARE_STR)) {
		return false;
	}

	snprintfz(key, sizeof(key), "%x", packet_no);
	if (!reg.CreateKey(key)) {
		return false;
	}

	DynBuf	dbuf;
	if (dict.pack(&dbuf) == 0) {
		reg.CloseKey();
		reg.DeleteChildTree(key);
		return	false;
	}

	reg.SetByte(SHAREDICT_STR, dbuf.Buf(), (int)dbuf.UsedSize());

	return true;
}

bool Cfg::LoadShare(int idx, IPDict *dict)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.OpenKey(SAVESHARE_STR)) {
		return false;
	}

	if (!reg.EnumKey(idx, key, sizeof(key))) {
		return false;
	}
	if (!reg.OpenKey(key)) {
		return false;
	}
	int	size = 0;
	if (!reg.GetByte(SHAREDICT_STR, 0, &size)) {
		return false;
	}

	DynBuf	dbuf(size);
	if (!reg.GetByte(SHAREDICT_STR, dbuf.Buf(), &size)) {
		return false;
	}

	return	dict->unpack(dbuf.Buf(), size) > 0;
}

bool Cfg::DeleteShare(UINT packet_no)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.OpenKey(SAVESHARE_STR)) {
		return false;
	}

	snprintfz(key, sizeof(key), "%x", packet_no);
	return	reg.DeleteChildTree(key);
}


#define SENDPKT_STR		"SendPkt"
#define SENDDICT_STR	"SendDict"

bool Cfg::SaveSendPkt(ULONG packet_no, const IPDict& dict)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.CreateKey(SENDPKT_STR)) {
		return false;
	}

	DynBuf	dbuf;
	dict.pack(&dbuf);
	snprintfz(key, sizeof(key), "%x", packet_no);
	reg.SetByte(key, dbuf.Buf(), (int)dbuf.UsedSize());

	return true;
}

bool Cfg::LoadSendPkt(int idx, IPDict *dict)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.OpenKey(SENDPKT_STR)) {
		return false;
	}

	if (!reg.EnumValue(idx, key, sizeof(key))) {
		return false;
	}

	int	size = 0;
	if (!reg.GetByte(key, 0, &size)) {
		return false;
	}

	DynBuf	dbuf(size);
	if (!reg.GetByte(key, dbuf.Buf(), &size)) {
		return false;
	}

	return	dict->unpack(dbuf.Buf(), size) > 0;
}

bool Cfg::DelSendPkt(ULONG packet_no)
{
	char	key[MAX_LISTBUF];
	GetRegName(key, nicAddr, portNo);
	TRegistry	reg(HS_TOOLS, key);

	if (!reg.OpenKey(SENDPKT_STR)) {
		return false;
	}

	snprintfz(key, sizeof(key), "%x", packet_no);
	return	reg.DeleteValue(key);
}

void Cfg::WriteHost(Host *host, TRegistry *reg)
{
	TRegistry	*reg_sv = reg;

	if (!reg) {
		char	key[MAX_LISTBUF];
		GetRegName(key, nicAddr, portNo);
		reg = new TRegistry(HS_TOOLS, key);

		reg->CreateKey(HOSTINFO_STR);
	}

	char	key[MAX_BUF];
	sprintf(key, "%s:%s", host->hostSub.u.userName, host->hostSub.u.hostName);

	IPDict	d;

	d.put_int(PRIORITY_KEY, host->priority);
	d.put_int(HOSTSTAT_KEY, host->hostStatus);

	d.put_str(UNAME_KEY, host->hostSub.u.userName);
	d.put_str(HNAME_KEY, host->hostSub.u.hostName);

	d.put_str(ADDR_KEY, host->hostSub.addr.S());
	d.put_int(PORT_KEY, host->hostSub.portNo);

	d.put_str(NICK_KEY, host->nickName);
	d.put_str(GROUP_KEY, host->groupName);
	if (*host->alterName) {
		d.put_str(ALTER_KEY, host->alterName);
	}
	d.put_int(UPTIME_KEY, host->updateTime);

	if (host->pubKey.KeyLen()) {
		BYTE	pub[MAX_BUF];
		int		plen = host->pubKey.Serialize(pub, sizeof(pub));
		if (plen > 0) {
			d.put_bytes(PUBKEY_KEY, pub, plen);
		}
	}
	DynBuf	buf;
	d.pack(&buf);
	reg->SetByte(key, buf, (int)buf.UsedSize());

	if (!reg_sv) {
		delete reg;
	}
}

void Cfg::DelHost(Host *host, TRegistry *reg)
{
	TRegistry	*reg_sv = reg;

	if (!reg) {
		char	key[MAX_LISTBUF];
		GetRegName(key, nicAddr, portNo);
		reg = new TRegistry(HS_TOOLS, key);
	}

	if (reg_sv || reg->OpenKey(HOSTINFO_STR)) {
		char	key[MAX_BUF];
		sprintf(key, "%s:%s", host->hostSub.u.userName, host->hostSub.u.hostName);
		reg->DeleteValue(key);
	}

	if (!reg_sv) {
		delete reg;
	}
}

