static char *taskbarui_id = 
	"@(#)Copyright (C) H.Shirouzu 2012-2016   taskbarui.cpp	Ver4.00";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Windows7 Taskbar UI
	Create					: 2012-01-09(Mon)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

#if 0

#include <ObjBase.h>
#include <propvarutil.h>
#include <propkey.h>

#include "resource.h"

// Creates a CLSID_ShellLink to insert into the Tasks section of the Jump List.  This type of Jump
// List item allows the specification of an explicit command line to execute the task.
HRESULT _CreateShellLink(PCWSTR pszArguments, PCWSTR pszTitle, IShellLink **ppsl)
{
	IShellLinkW *psl;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
	if (!SUCCEEDED(hr)) return hr;

	// Determine our executable's file path so the task will execute this application
	WCHAR szAppPath[MAX_PATH];
	if (GetModuleFileNameW(NULL, szAppPath, ARRAYSIZE(szAppPath)))
	{
		hr = psl->SetPath(szAppPath);
		if (SUCCEEDED(hr))
		{
			hr = psl->SetArguments(pszArguments);
			if (SUCCEEDED(hr))
			{
				// The title property is required on Jump List items provided as an IShellLink
				// instance.  This value is used as the display name in the Jump List.
				IPropertyStore *pps;
				hr = psl->QueryInterface(IID_PPV_ARGS(&pps));
				if (SUCCEEDED(hr))
				{
					PROPVARIANT propvar;
					hr = InitPropVariantFromString(pszTitle, &propvar);
					if (SUCCEEDED(hr))
					{
						hr = pps->SetValue(PKEY_Title, propvar);
						if (SUCCEEDED(hr))
						{
							hr = pps->Commit();
							if (SUCCEEDED(hr))
							{
								hr = psl->QueryInterface(IID_PPV_ARGS(ppsl));
							}
						}
						PropVariantClear(&propvar);
					}
					pps->Release();
				}
			}
		}
	}
	else
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	psl->Release();
	return hr;
}

// The Tasks category of Jump Lists supports separator items.  These are simply IShellLink instances
// that have the PKEY_AppUserModel_IsDestListSeparator property set to TRUE.  All other values are
// ignored when this property is set.
HRESULT _CreateSeparatorLink(IShellLink **ppsl)
{
	IPropertyStore *pps;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pps));
	if (SUCCEEDED(hr))
	{
		PROPVARIANT propvar;
		hr = InitPropVariantFromBoolean(TRUE, &propvar);
		if (SUCCEEDED(hr))
		{
			hr = pps->SetValue(PKEY_AppUserModel_IsDestListSeparator, propvar);
			if (SUCCEEDED(hr))
			{
				hr = pps->Commit();
				if (SUCCEEDED(hr))
				{
					hr = pps->QueryInterface(IID_PPV_ARGS(ppsl));
				}
			}
			PropVariantClear(&propvar);
		}
		pps->Release();
	}
	return hr;
}

// Builds the collection of task items and adds them to the Task section of the Jump List.  All tasks
// should be added to the canonical "Tasks" category by calling ICustomDestinationList::AddUserTasks.
HRESULT _AddTasksToList(ICustomDestinationList *pcdl, const WCHAR *opt)
{
	IObjectCollection *poc;
	HRESULT hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&poc));
	if (SUCCEEDED(hr))
	{
		IShellLink * psl;
		hr = _CreateShellLink(opt, L"Main Menu", &psl);
		if (SUCCEEDED(hr))
		{
			hr = poc->AddObject(psl);
			psl->Release();
		}
		if (SUCCEEDED(hr))
		{
			IObjectArray * poa;
			hr = poc->QueryInterface(IID_PPV_ARGS(&poa));
			if (SUCCEEDED(hr))
			{
				// Add the tasks to the Jump List. Tasks always appear in the canonical "Tasks"
				// category that is displayed at the bottom of the Jump List, after all other
				// categories.
				hr = pcdl->AddUserTasks(poa);
				poa->Release();
			}
		}
		poc->Release();
	}
	return hr;
}

// Builds a new custom Jump List for this application.
void CreateJumpList(const char *option)
{
	WCHAR	wopt[MAX_BUF];

	swprintfz(wopt, L"/TASKBAR_MSG %s 1", AtoWs(option));

	// Create the custom Jump List object.
	ICustomDestinationList *pcdl;
	HRESULT hr = CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pcdl));
	if (SUCCEEDED(hr))
	{
		UINT cMinSlots;
		IObjectArray *poaRemoved;
		hr = pcdl->BeginList(&cMinSlots, IID_PPV_ARGS(&poaRemoved));
		if (SUCCEEDED(hr))
		{
			// Add content to the Jump List.
			hr = _AddTasksToList(pcdl, wopt);
			if (SUCCEEDED(hr))
			{
				// Commit the list-building transaction.
				hr = pcdl->CommitList();
			}
			else MessageBox(0, "err4", "", MB_OK);
			poaRemoved->Release();
		}
		else MessageBox(0, "err2", "", MB_OK);
		pcdl->Release();
	}
	else MessageBox(0, "err1", "", MB_OK);
}

// Removes that existing custom Jump List for this application.
void DeleteJumpList()
{
	ICustomDestinationList *pcdl;
	HRESULT hr = CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pcdl));
	if (SUCCEEDED(hr))
	{
		hr = pcdl->DeleteList(NULL);
		pcdl->Release();
	}
}

#endif

