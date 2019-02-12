/* @(#)Copyright (C) H.Shirouzu 2016-2017   instcore.h	Ver4.61 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#define INSTALL_STR				"Install"

enum ExtractErr {
	NO_EXTRACT_ERR  = 0,
	CREATE_DIR_ERR  = 1,
	CREATE_FILE_ERR = 2,
	DATA_BROKEN     = 3,
	USER_CANCEL     = 4,
};

BOOL DeflateData(BYTE *buf, DWORD size, VBuf *vbuf);
BOOL CreateFileByData(const char *fname, const char *dir, BYTE *data, DWORD size, int retry_num=0);
ExtractErr GetExtractErr(const char **msg=NULL);
void SetExtractErr(ExtractErr err, const char *msg="");

BOOL IPDictCopy(IPDict *dict, const char *fname, const char *dst, BOOL *is_rotate);
BOOL GetIPDictBySelf(IPDict *dict);
BOOL IsSameFileDict(IPDict *dict, const char *fname, const char *dst);

