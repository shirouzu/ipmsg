static char *tlist_id = 
	"@(#)Copyright (C) 1996-2009 H.Shirouzu		tlist.cpp	Ver0.97";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: List Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

/*
	TList class
*/
TList::TList(void)
{
	Init();
}

void TList::Init(void)
{
	top.prev = top.next = &top;
	num = 0;
}

void TList::AddObj(TListObj * obj)
{
	obj->prev = top.prev;
	obj->next = &top;
	top.prev->next = obj;
	top.prev = obj;
	num++;
}

void TList::DelObj(TListObj * obj)
{
//	if (!obj->next || !obj->prev || obj->next != &top && obj->prev != &top) {
//		Debug("DelObj(%p) (%p/%p)\n", obj, obj->next, obj->prev);
//	}
	if (obj->next) {
		obj->next->prev = obj->prev;
	}
	if (obj->prev) {
		obj->prev->next = obj->next;
	}
	obj->next = obj->prev = NULL;
	num--;
}

void TList::MoveList(TList *from_list)
{
	if (from_list->top.next != &from_list->top) {	// from_list is not empty
		if (top.next == &top) {	// empty
			top = from_list->top;
			top.next->prev = top.prev->next = &top;
		}
		else {
			top.prev->next = from_list->top.next;
			from_list->top.next->prev = top.prev;
			from_list->top.prev->next = &top;
			top.prev = from_list->top.prev;
		}
		num += from_list->num;
		from_list->Init();
	}
}

/*
	TRecycleList class
*/
TRecycleList::TRecycleList(int init_cnt, int size)
{
	data = new char [init_cnt * size];
	memset(data, 0, init_cnt * size);

	for (int cnt=0; cnt < init_cnt; cnt++) {
		TListObj *obj = (TListObj *)(data + cnt * size);
		list[FREE_LIST].AddObj(obj);
	}
}

TRecycleList::~TRecycleList()
{
	delete [] data;
}

TListObj *TRecycleList::GetObj(int list_type)
{
	TListObj	*obj = list[list_type].TopObj();

	if (obj)
		list[list_type].DelObj(obj);

	return	obj;
}

void TRecycleList::PutObj(int list_type, TListObj *obj)
{
	list[list_type].AddObj(obj);
}

