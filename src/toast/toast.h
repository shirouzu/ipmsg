/*	@(#)Copyright (C) H.Shirouzu 2016   toast.h	Ver4.10 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Win10 toast control dll
	Create					: 2016-06-02(Thu)
	Update					: 2016-11-01(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TOAST_H
#define TOAST_H

extern "C" {
__declspec(dllexport) BOOL ToastMgrInit(HWND hWnd, UINT uMsg);
__declspec(dllexport) BOOL ToastShow(const WCHAR *title, const WCHAR *msg, const WCHAR *img);
__declspec(dllexport) BOOL ToastHide();
__declspec(dllexport) BOOL ToastIsAlive();
}

#ifdef TOAST_CORE
class HStr;
struct ToastParam;
typedef __int64 int64;

class ToastMgr {
	CRITICAL_SECTION cs;
	HANDLE	hEvent;
	HWND	hWnd;
	UINT	uMsg;
	UINT	count;
	int64	genId;
	int64	ignId;
	static void ToastProc(void *param);
	void ToastProcCore(ToastParam *param);

public:
	ToastMgr(HWND _hWnd, UINT _uMsg) {
		::InitializeCriticalSection(&cs);
		hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		hWnd	= _hWnd;
		uMsg	= _uMsg;
		count	= 0;
		genId	= 0;
		ignId	= 0;
	}
	~ToastMgr() {
		::DeleteCriticalSection(&cs);
		::CloseHandle(hEvent);
	}
	void Lock() {
		::EnterCriticalSection(&cs);
	}
	void UnLock() {
		::LeaveCriticalSection(&cs);
	}
	BOOL Show(const WCHAR *title, const WCHAR *msg, const WCHAR *img);
	BOOL Hide();
	BOOL IsAlive();
};

typedef ITypedEventHandler<ToastNotification *, IInspectable *>            ActEvent;
typedef ITypedEventHandler<ToastNotification *, ToastDismissedEventArgs *> DismEvent;
typedef ITypedEventHandler<ToastNotification *, ToastFailedEventArgs *>    FailEvent;

class ToastObj;

class ToastEvent : public Implements<ActEvent, DismEvent, FailEvent> {
	ULONG		ref;
	ToastObj	*core;

public:
	ToastEvent::ToastEvent(ToastObj *_core);
	~ToastEvent();

	IFACEMETHODIMP Invoke(IToastNotification *sender, IInspectable* args);
	IFACEMETHODIMP Invoke(IToastNotification *sender, IToastDismissedEventArgs *e);
	IFACEMETHODIMP Invoke(IToastNotification *sender, IToastFailedEventArgs *e);
	IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&ref); }

	IFACEMETHODIMP_(ULONG) Release() {
		ULONG l = InterlockedDecrement(&ref);
		if (l == 0) delete this;
		return l;
	}

	IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
		if (IsEqualIID(riid, IID_IUnknown)) {
			*ppv = static_cast<IUnknown*>(static_cast<ActEvent*>(this));
		}
		else if (IsEqualIID(riid, __uuidof(ActEvent))) {
			*ppv = static_cast<ActEvent*>(this);
		}
		else if (IsEqualIID(riid, __uuidof(DismEvent))) {
			*ppv = static_cast<DismEvent*>(this);
		}
		else if (IsEqualIID(riid, __uuidof(FailEvent))) {
			*ppv = static_cast<FailEvent*>(this);
		}
		else {
			*ppv = nullptr;
		}

		if (*ppv) {
			reinterpret_cast<IUnknown*>(*ppv)->AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}
};

class ToastObj {
	ComPtr<IToastNotificationManagerStatics>	tnms;
	ComPtr<IToastNotifier>						tnr;
	ComPtr<IToastNotificationFactory>			tnf;

	ComPtr<ToastEvent>		tEv;
	EventRegistrationToken	evAct;
	EventRegistrationToken	evDism;
	EventRegistrationToken	evFail;

	IToastNotification		*toast;

	HWND	hWnd;
	UINT	uMsg;

	void DelToast();
	BOOL ToastXml(IXmlDocument** xml, const WCHAR *title, const WCHAR *msg, const WCHAR *img);

public:
	ToastObj();
	~ToastObj();

	BOOL Init(HWND _hWnd, UINT _uMsg);
	BOOL Show(const WCHAR *title, const WCHAR *msg, const WCHAR *img);
	BOOL Hide();
	BOOL IsAlive();

	BOOL Event(ToastDismissalReason tdr);
	BOOL EventClick();
	BOOL EventErr(int hint);
};

class HStr {
public:
	HSTRING			hstr;
	HSTRING_HEADER	hhead;

	HStr(PCWSTR str, u_int len=0) throw() {
		if (len == 0) {
			len = (u_int)wcslen(str);
		}
		::WindowsCreateStringReference(str, len, &hhead, &hstr);
	}
	~HStr() {
		::WindowsDeleteString(hstr);
	}
	HSTRING Get() const throw() {
		return	hstr;
	}
};

struct ToastParam {
	WCHAR	*title;
	WCHAR	*msg;
	WCHAR	*img;
	int64	genId;

	ToastParam(const WCHAR *_title, const WCHAR *_msg, const WCHAR *_img, int64 _genId) {
		title = _title ? _wcsdup(_title) : NULL;
		msg   = _msg   ? _wcsdup(_msg)   : NULL;
		img   = _img   ? _wcsdup(_img)   : NULL;
		genId = _genId;
	}
	~ToastParam() {
		free(title);
		free(msg);
		free(img);
	}
};
#endif // TOAST_CORE

#endif  // TOAST_H

