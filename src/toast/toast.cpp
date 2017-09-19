static char *toast_id = 
	"@(#)Copyright (C) H.Shirouzu 2016   toast.cpp	Ver4.10";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Win10 toast control dll
	Create					: 2016-06-02(Thu)
	Update					: 2016-11-01(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <SDKDDKVer.h>
#include <Windows.h>
#include <sal.h>
#include <Psapi.h>
#include <strsafe.h>
#include <ObjBase.h>
#include <ShObjIdl.h>
#include <propvarutil.h>
#include <functiondiscoverykeys.h>
#include <intsafe.h>
#include <guiddef.h>
#include <process.h>

#include <roapi.h>
#include <wrl\client.h>
#include <wrl\implements.h>
#include <windows.ui.notifications.h>

using namespace Microsoft::WRL;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;

#define TOAST_CORE
#include "toast.h"
#pragma comment (lib, "runtimeobject.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static const WCHAR AppId[] = L"IPMSG for Win";
#define wsizeof(x) (sizeof(x)/sizeof(WCHAR))

static ToastMgr *gToastMgr = NULL;

//#define LOG_DEBUG

#ifdef LOG_DEBUG
BOOL SetDebugInfo(WCHAR *logname, CRITICAL_SECTION *cs)
{
	*logname = 0;

	if (::GetModuleFileNameW(NULL, logname, wsizeof(logname))) {
		if (WCHAR *p = wcsrchr(logname, '\\')) {
			wcsncpy(p, L"iptoast.log", MAX_PATH - (p - logname) - 1);

			HANDLE	hFile = ::CreateFileW(logname, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
			DWORD	size = 2;
			::WriteFile(hFile, "\xff\xfe", size, &size, 0);
#define LOG_HEADER	L"*** Start iptoast log ***\n"
			size = sizeof(LOG_HEADER) - 2;
			WriteFile(hFile, LOG_HEADER, size, &size, 0);
			::CloseHandle(hFile);
			::InitializeCriticalSection(cs);
		} else {
			*logname = 0;
		}
	}

	return	*logname ? TRUE : FALSE;
}
#endif

void DebugW(const WCHAR *fmt,...)
{
#ifdef LOG_DEBUG
	static CRITICAL_SECTION	cs;
	static WCHAR	LogFile[MAX_PATH];
	static BOOL		firstInit = SetDebugInfo(LogFile, &cs);
#endif
	static DWORD	last;

	DWORD cur  = ::GetTickCount();
	DWORD diff = last ? (cur - last) : 0;
	last = cur;

	WCHAR t[100];
	DWORD tlen;

	tlen = swprintf(t, wsizeof(t), L"%5.2f TID:%x: ",
		(double)diff / 1000, GetCurrentThreadId());
	::OutputDebugStringW(t);

	WCHAR	buf[4096];
	DWORD	buflen;

	va_list	ap;
	va_start(ap, fmt);
	buflen = vswprintf(buf, wsizeof(buf), fmt, ap);
	va_end(ap);
	::OutputDebugStringW(buf);

#ifdef LOG_DEBUG
	if (*LogFile) {
		::EnterCriticalSection(&cs);
		HANDLE	hFile = ::CreateFileW(LogFile, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		::SetFilePointer(hFile, 0, 0, FILE_END);
		DWORD	wsize = 0;
		::WriteFile(hFile, t, tlen * sizeof(WCHAR), &wsize, 0);
		::WriteFile(hFile, buf, buflen * sizeof(WCHAR), &wsize, 0);
		::CloseHandle(hFile);
		::LeaveCriticalSection(&cs);
	}
#endif
}

BOOL SetNodeStr(HSTRING str, IXmlNode *node, IXmlDocument *xml)
{
	ComPtr<IXmlText> xmlTxt;

	xml->CreateTextNode(str, &xmlTxt);
	if (xmlTxt) {
		ComPtr<IXmlNode> xmlTxtNode;

		xmlTxt.As(&xmlTxtNode);
		if (xmlTxtNode) {
			ComPtr<IXmlNode> child;
			node->AppendChild(xmlTxtNode.Get(), &child);
			if (child) {
				return	TRUE;
			}
		}
	}
	return FALSE;
}


BOOL SetImageSrc(const WCHAR *img, IXmlDocument *xml)
{
	ComPtr<IXmlNodeList> nodeList;
	xml->GetElementsByTagName(HStr(L"image").Get(), &nodeList);
	if (!nodeList) {
		return	FALSE;
	}

	ComPtr<IXmlNode> imageNode;
	nodeList->Item(0, &imageNode);
	if (!imageNode) {
		return	FALSE;
	}

	ComPtr<IXmlNamedNodeMap> attr;
	imageNode->get_Attributes(&attr);
	if (!attr) {
		return	FALSE;
	}

	ComPtr<IXmlNode> srcAttr;
	attr->GetNamedItem(HStr(L"src").Get(), &srcAttr);
	if (!srcAttr) {
		return	FALSE;
	}

	return	SetNodeStr(HStr(img).Get(), srcAttr.Get(), xml);
}

BOOL SetNoSound(IXmlDocument *xml)
{
	ComPtr<IXmlNodeList> roots;
	xml->GetElementsByTagName(HStr(L"toast").Get(), &roots);
	if (!roots) {
		return	FALSE;
	}

	ComPtr<IXmlNode> root;
	roots->Item(0, &root);
	if (!root) {
		return FALSE;
	}

//	ComPtr<IXmlNodeList> audioList;	// audioノードが存在する場合は先に削除
//	xml->GetElementsByTagName(HStr(L"audio").Get(), &audioList);
//	if (audioList) {
//		ComPtr<IXmlNode> audioNode;
//		ComPtr<IXmlNode> audioNodeTmp;
//		audioList->Item(0, &audioNode);
//		if (!audioNode) {
//			root->RemoveChild(audioNode.Get(), &audioNodeTmp);
//		}
//	}

	ComPtr<IXmlElement> audioEle;
	xml->CreateElement(HStr(L"audio").Get(), &audioEle);
	if (!audioEle) {
		return	FALSE;
	}

	ComPtr<IXmlNode> nodeTmp;
	audioEle.As(&nodeTmp);
	if (!nodeTmp) {
		return	FALSE;
	}

	ComPtr<IXmlNode> audioNode;
	root->AppendChild(nodeTmp.Get(), &audioNode);
	if (!audioNode) {
		return	FALSE;
	}

	ComPtr<IXmlNamedNodeMap> audioAttr;
	audioNode->get_Attributes(&audioAttr);
	if (!audioAttr) {
		return	FALSE;
	}

	ComPtr<IXmlAttribute> silentAttr;
	xml->CreateAttribute(HStr(L"silent").Get(), &silentAttr);
	if (!silentAttr) {
		return	FALSE;
	}

	ComPtr<IXmlNode> silentNode;
	silentAttr.As(&silentNode);
	if (!silentNode) {
		return	FALSE;
	}

	ComPtr<IXmlNode> namedNode;
	audioAttr->SetNamedItem(silentNode.Get(), &namedNode);
//	if (!namedNode) { // 成功してもNULL?
//		return	FALSE;
//	}

	return	SetNodeStr(HStr(L"true").Get(), silentNode.Get(), xml);
}

BOOL SetTextVal(const WCHAR **strs, u_int num, IXmlDocument *xml)
{
	ComPtr<IXmlNodeList> nodeList;
	xml->GetElementsByTagName(HStr(L"text").Get(), &nodeList);
	if (!nodeList) {
		return FALSE;
	}

	for (u_int i=0; i < num; i++) {
		ComPtr<IXmlNode> node;
		nodeList->Item(i, &node);
		if (!node) {
			return FALSE;
		}
		if (!SetNodeStr(HStr(strs[i]).Get(), node.Get(), xml)) {
			return FALSE;
		}
	}
	return TRUE;
}

ToastObj::ToastObj()
{
	hWnd	= NULL;
	uMsg	= 0;
	toast	= NULL;
}

ToastObj::~ToastObj()
{
	DelToast();
}

BOOL ToastObj::Init(HWND _hWnd, UINT _uMsg)
{
	hWnd	= _hWnd;
	uMsg	= _uMsg;
	HRESULT hr = 0;

	HStr	tnm_name = RuntimeClass_Windows_UI_Notifications_ToastNotificationManager;
	hr = ::GetActivationFactory(tnm_name.Get(), &tnms);
    if (!tnms) {
		DebugW(L"GetActivationFactory err(%x)\n", hr);
		return FALSE;
	}

	hr = tnms->CreateToastNotifierWithId(HStr(AppId).Get(), &tnr);
	if (!tnr) {
		DebugW(L"CreateToastNotifierWithId err(%x)\n", hr);
		return FALSE;
	}

	HStr	tn_name = RuntimeClass_Windows_UI_Notifications_ToastNotification;
	hr = ::GetActivationFactory(tn_name.Get(), &tnf);
	if (!tnf) {
		DebugW(L"RuntimeClass_Windows_UI_Notifications_ToastNotification err(%x)\n", hr);
		return FALSE;
	}

	tEv = new ToastEvent(this);

	return	TRUE;
}

BOOL ToastObj::ToastXml(IXmlDocument** xml, const WCHAR *title, const WCHAR *msg,
	const WCHAR *img)
{
	*xml = NULL;
	tnms->GetTemplateContent(ToastTemplateType_ToastImageAndText04, xml);
	if (!*xml) {
		return	FALSE;
	}
	if (!SetImageSrc(img, *xml)) {
		return FALSE;
	}

	int		val_num = 2;
	const	WCHAR *val[] = { title, msg, NULL };
	WCHAR	*msg_tmp = NULL;

	if (const WCHAR *p = wcschr(msg, '\n')) {
		int idx = (int)(p - msg);

		msg_tmp = _wcsdup(msg);
		msg_tmp[idx] = 0;
		if (idx > 0 && msg_tmp[idx-1] == '\r') {
			msg_tmp[idx-1] = 0;
		}
		val[1] = msg_tmp;
		val[2] = msg_tmp + idx + 1;
		val_num = 3;
	}

	if (!SetTextVal(val, val_num, *xml)) {
		return FALSE;
	}
	if (msg_tmp) {
		free(msg_tmp);
	}

	if (!SetNoSound(*xml)) {
		return FALSE;
	}
	return	TRUE;
}

BOOL ToastObj::Show(const WCHAR *title, const WCHAR *msg, const WCHAR *img)
{
	//DebugW(L"PreShow(%p)\n", toast);

    ComPtr<IXmlDocument> xml;
	ToastXml(&xml, title, msg, img);

	tnf->CreateToastNotification(xml.Get(), &toast);

	//DebugW(L"Show(%p)\n", toast);

	if (toast) {
		toast->add_Activated(tEv.Get(), &evAct);
	}
	if (toast) {
		toast->add_Dismissed(tEv.Get(), &evDism);
	}
	if (toast) {
		toast->add_Failed(tEv.Get(), &evFail);
	}

	if (toast) {
		tnr->Show(toast);
	}
	//DebugW(L"End Show(%p)\n", toast);
	return	TRUE;
}

BOOL ToastObj::IsAlive()
{
	return	toast ? TRUE : FALSE;
}

BOOL ToastObj::Hide()
{
	//DebugW(L"Hide(%p)\n", toast);

	DelToast();

	//DebugW(L"end of Hide(%p)\n", toast);
	return	TRUE;
}

BOOL ToastObj::Event(ToastDismissalReason tdr)
{
	//DebugW(L"Dismiss(%p)\n", toast);

	LPARAM	lParam = 0;

	switch (tdr) {
	case ToastDismissalReason_ApplicationHidden:
		lParam = NIN_BALLOONHIDE;
		break;

	case ToastDismissalReason_UserCanceled:
		lParam = NIN_BALLOONHIDE;
		break;

	case ToastDismissalReason_TimedOut:
		lParam = NIN_BALLOONTIMEOUT;
		break;

	default:
		break;
	}
	::PostMessage(hWnd, uMsg, 0, lParam);

//	DelToast();

	//DebugW(L"end of Dismiss(%p)\n", toast);

	return	TRUE;
}

BOOL ToastObj::EventClick()
{
	//DebugW(L"Click(%p)\n", toast);

	::PostMessage(hWnd, uMsg, 0, NIN_BALLOONUSERCLICK);

//	DelToast();

	//DebugW(L"end of Click(%p)\n", toast);

	return	TRUE;
}

BOOL ToastObj::EventErr(int hint)
{
	//DebugW(L"EvErr(%p)\n", toast);
	::PostMessage(hWnd, uMsg, 0, NIN_BALLOONHIDE);

//	DelToast();

	//DebugW(L"end of EvErr(%p)\n", toast);
	return TRUE;
}

void ToastObj::DelToast()
{
	//DebugW(L"DelToast(%p)\n", toast);
	if (toast) {
		toast->remove_Activated(evAct);
	}
	if (toast) {
		toast->remove_Dismissed(evDism);
	}
	if (toast) {
		toast->remove_Failed(evFail);
	}

	if (toast) {
		//DebugW(L" call hide(%p)\n", toast);
		tnr->Hide(toast);
	}
	if (toast) {	// re-entrant?
		//DebugW(L" end call hide (%p)\n", toast);
		toast->Release();
		//DebugW(L" ** Release (%p)\n", toast);
		toast = NULL;
	}
	//DebugW(L"end of DelToast(%p)\n", toast);
}

ToastEvent::ToastEvent(ToastObj *_core) : ref(1), core(_core)
{
	//DebugW(L"ToastEvent()\n");
}

ToastEvent::~ToastEvent()
{
	//DebugW(L"~ToastEvent()\n");
}

// ActEvent
IFACEMETHODIMP ToastEvent::Invoke(IToastNotification*, IInspectable*)
{
	//DebugW(L"Invoke inspect\n");

	core->EventClick();

	//DebugW(L"end of Invoke inspect\n");

	return	S_OK;
}

// DismEvent
IFACEMETHODIMP ToastEvent::Invoke(IToastNotification*, IToastDismissedEventArgs* ev)
{
	//DebugW(L"Invoke dismiss\n");

	ToastDismissalReason	tdr;

	if (ev->get_Reason(&tdr) >= S_OK) {
		core->Event(tdr);
	}
	else {
		core->EventErr(1);
	}

	//DebugW(L"end of Invoke dismiss\n");

	return	S_OK;
}

// FailEvent
IFACEMETHODIMP ToastEvent::Invoke(IToastNotification*, IToastFailedEventArgs*)
{
	//DebugW(L"ToastEvent::Invoke The toast encountered an error.\n");

	core->EventErr(2);

	//DebugW(L"end of ToastEvent::Invoke The toast encountered an error.\n");

	return S_OK;
}

void ToastMgr::ToastProc(void *param)
{
	::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	gToastMgr->ToastProcCore((ToastParam *)param);

	::CoUninitialize();
}

void ToastMgr::ToastProcCore(ToastParam *param)
{
	//DebugW(L"ToastProcCore\n");

	ToastObj	*toastObj = new ToastObj();
	if (toastObj->Init(hWnd, uMsg)) {
		toastObj->Show(param->title, param->msg, param->img);
	}

	Lock();
	while (param->genId > ignId) {
		UnLock();
		Sleep(100);
		Lock();
	}
	UnLock();

	delete toastObj;

	Lock();
	count--;
	if (param->genId > ignId) {
		ignId = param->genId;
	}

	UnLock();

	delete param;

	//DebugW(L"end of ToastProc\n");
}

BOOL ToastMgr::Show(const WCHAR *title, const WCHAR *msg, const WCHAR *img)
{
	//DebugW(L"Show\n");

	Lock();

	ignId = genId;
	count++;
	_beginthread(ToastProc, 0, new ToastParam(title, msg, img, ++genId));
	UnLock();

	//DebugW(L"end of Show\n");
	return	TRUE;
}

BOOL ToastMgr::Hide()
{
	//DebugW(L"Hide\n");

	Lock();
	ignId = genId;
	UnLock();

	//DebugW(L"end of Hide\n");
	return	TRUE;
}

BOOL ToastMgr::IsAlive()
{
	//DebugW(L"IsAlive\n");

	Lock();
	BOOL ret = ignId < genId ? TRUE : FALSE;
	UnLock();

	//DebugW(L"end of IsAlive\n");
	return	ret;
}

BOOL ToastMgrInit(HWND hWnd, UINT uMsg)
{
	//DebugW(L"ToastMgrInit\n");

	gToastMgr = new ToastMgr(hWnd, uMsg);

	//DebugW(L"end of ToastMgrInit\n");
	return	TRUE;
}

BOOL ToastShow(const WCHAR *title, const WCHAR *msg, const WCHAR *img)
{
	//DebugW(L"ToastShow\n");

	if (!gToastMgr) return FALSE;
	BOOL ret = gToastMgr->Show(title, msg, img);

	//DebugW(L"end of ToastShow\n");
	return	ret;
}

BOOL ToastHide()
{
	//DebugW(L"ToastHide\n");

	if (!gToastMgr) return FALSE;
	BOOL ret = gToastMgr->Hide();

	//DebugW(L"end of ToastHide\n");
	return	ret;
}

BOOL ToastIsAlive()
{
	//DebugW(L"ToastIsAlive\n");

	if (!gToastMgr) return FALSE;
	BOOL ret = gToastMgr->IsAlive();

	//DebugW(L"end of ToastIsAlive\n");
	return	ret;
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD fdwReason, void *resv)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		break;
	}
	return	TRUE;
}

// toastapi で必要なもの
//  表示（文面/icon/秒数, HWND/eventMsg, shortcut-path）
//   - callback イベント
//  消去
//  表示状態

// キューイングの有無


