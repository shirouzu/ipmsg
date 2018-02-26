/* @(#)Copyright (C) H.Shirouzu 1998-2017   install.h	Ver4.61 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#define IPMSG_CLASS				"ipmsg_class"
#define IPMSG_NAME				"IPMSG for Win"
#define IP_MSG					"IPMsg"
#define HSTOOLS_STR				"HSTools"
#define IPMSG_DEFAULT_PORT		0x0979

#define IPMSG_EXENAME			"ipmsg.exe"
#define IPMSG_EXENAME_W			L"ipmsg.exe"
#define IPCMD_EXENAME			"ipcmd.exe"
#define UNINST_EXENAME			"uninst.exe"
#define OLD_UNINST_EXENAME		"setup.exe"
#define UNINST_BAT				"uninst.bat"
#define UNINST_BAT_W			L"uninst.bat"
#define IPMSGHELP_NAME			"ipmsg.chm"

#define IPTOAST_NAME			"iptoast.dll"
#define IPMSGPNG_NAME			"ipmsg.png"
#define IPEXCPNG_NAME			"ipexc.png"

#define IPMSG_SHORTCUT_NAME		IPMSG_NAME ".lnk"
#define UNINST_SHORTCUT_NAME	"Uninstall IPMSG.lnk"
#define UNC_PREFIX				"\\\\"
#define MAX_BUF					1024

#define REGSTR_SHELLFOLDERS		REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_STARTUP			"Startup"
#define REGSTR_DESKTOP			"Desktop"
#define REGSTR_PROGRAMS			"Programs"
#define REGSTR_PATH				"Path"
//#define REGSTR_PROGRAMFILES		"ProgramFilesDir"
//#define REGSTR_PROGRAMFILESX86	"ProgramFilesDir (x86)"

#ifdef MAIN_EXEC
//const char *SetupFiles [] = {
//	IPMSG_EXENAME, UNINST_EXENAME, IPMSGHELP_NAME, NULL
//};
const char *UnSetupFiles [] = {
	IPMSG_EXENAME, IPCMD_EXENAME, UNINST_EXENAME, IPMSGHELP_NAME, UNINST_BAT,
	IPTOAST_NAME, IPMSGPNG_NAME, IPEXCPNG_NAME,
	UPDATE32_FILENAME, UPDATE64_FILENAME, OLD_UNINST_EXENAME, NULL
};
#endif

// function prototype
int TerminateIPMsg(HWND hWnd, const char *title, const char *msg, BOOL silent=FALSE);
BOOL CALLBACK TerminateIPMsgProc(HWND hWnd, LPARAM lParam);
BOOL ShellLink(LPCSTR src, LPSTR dest, LPCSTR arg="", LPCSTR app_id=NULL);
BOOL DeleteLink(LPCSTR path);
BOOL DeleteLinkFolder(LPCSTR path);
void BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title);
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);
BOOL GetParentDirU8(const char *srcfile, char *dir);
BOOL RemoveSameLink(const char *dir, const char *check_name);
BOOL RunAsAdmin(HWND hWnd, BOOL imm=FALSE);
UINT GetDriveTypeEx(const char *file);
inline BOOL IsUncFile(const char *path) { return strnicmp(path, UNC_PREFIX, 2) == 0; }

enum GenStrMode { WITH_PERIOD, WITH_PAREN };
BOOL GenStrWithUser(const char *str, char *buf, GenStrMode mode=WITH_PERIOD);

