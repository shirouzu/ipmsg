static char *pubkey_id = 
	"@(#)Copyright (C) H.Shirouzu 2011   pubkey.cpp	Ver3.01";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Public Key Encryption
	Create					: 2011-05-03(Tue)
	Update					: 2011-05-03(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include "resource.h"
#include "ipmsg.h"
#include "blowfish.h"


/* VC4 で CryptoAPI を使用可能にする */
BOOL SetupCryptAPI(Cfg *cfg, MsgMng *msgMng)
{
	if (pCryptProtectData == NULL) {
		for (int i=KEY_1024; i < MAX_KEY; i++) {
			if (cfg->priv[i].encryptType == PRIV_BLOB_DPAPI)
				cfg->priv[i].encryptType = PRIV_BLOB_RAW;
		}
	}

	if (pCryptAcquireContext == NULL)
		return	GetLastErrorMsg("CryptAcquireContext"), FALSE;

	SetupCryptAPICore(cfg, msgMng);

// 起動直後に、まれにコケる環境があるらしいので、わずかにリトライ...
#define MAX_RETRY	3
	int	cnt = 0;
	while (1)
	{
		BOOL	need_retry = FALSE;
		int		i;

		for (i=0; i < MAX_KEY; i++) {
			if (cfg->priv[i].hCsp && cfg->pub[i].Key() == 0) {
				need_retry = TRUE;
				break;
			}
		}

		if (++cnt > MAX_RETRY || !need_retry)
			break;

		for (i=0; i < MAX_KEY; i++) {
			if (cfg->priv[i].hCsp) {
				pCryptReleaseContext(cfg->priv[i].hCsp, 0);
				cfg->priv[i].hCsp = NULL;
			}
		}
		::Sleep(1000);
		SetupCryptAPICore(cfg, msgMng, cnt == MAX_RETRY ? KEY_DIAG : 0);
	}
	if (cnt > MAX_RETRY || !cfg->pub[0].Key() && !cfg->pub[1].Key() && !cfg->pub[2].Key())
	{
		if (MessageBox(GetMainWnd(), "RSA keyset can't access. Create New RSA key?",
				IP_MSG, MB_OKCANCEL) == IDOK) {
			SetupCryptAPICore(cfg, msgMng, KEY_REBUILD|KEY_DIAG);
		}
	}

	if (cfg->pub[KEY_2048].Key()) {
		HostSub *host;
		host = msgMng->GetLocalHost();
		GenUserNameDigest(host->userName, cfg->pub[KEY_2048].Key(), host->userName);
		host = msgMng->GetLocalHostA();
		GenUserNameDigest(host->userName, cfg->pub[KEY_2048].Key(), host->userName);
	}

	return	cfg->pub[0].Key() || cfg->pub[1].Key() || cfg->pub[2].Key();
}

BOOL SetupCryptAPICore(Cfg *cfg, MsgMng *msgMng, int ctl_flg)
{
	BYTE	data[MAX_BUF_EX];
	int		len = sizeof(data);
	BOOL	ret = FALSE;
	int		i;

// RSA 鍵の生成
	for (i=KEY_512; i < MAX_KEY; i++) {
		SetupRSAKey(cfg, msgMng, (KeyType)i, ctl_flg);
	}

	for (i=0; i <= KEY_2048; i++) {
		if (!cfg->pub[i].Key()) continue;

		BOOL		ret = FALSE;
		HCRYPTKEY	hKey = 0, hExKey = 0;
		BYTE		tmp[MAX_BUF];
		DWORD		tmplen = MAX_BUF / 2;

		cfg->pub[i].KeyBlob(data, sizeof(data), &len);
		if (i == KEY_512) {		// Self Check 512bit
			if (pCryptImportKey(cfg->priv[i].hCsp, data, len, 0, 0, &hExKey)) {
				if (pCryptGenKey(cfg->priv[i].hCsp, CALG_RC2, CRYPT_EXPORTABLE, &hKey)) {
					pCryptExportKey(hKey, hExKey, SIMPLEBLOB, 0, NULL, (DWORD *)&len);
					if (pCryptExportKey(hKey, hExKey, SIMPLEBLOB, 0, data, (DWORD *)&len)) {
						if (pCryptEncrypt(hKey, 0, TRUE, 0, tmp, &tmplen, MAX_BUF)) ret = TRUE;
						else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptEncrypt test512");
					}
					else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptExportKey test512");
					pCryptDestroyKey(hKey);
				}
				else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptGenKey test512");
				pCryptDestroyKey(hExKey);
			}
			else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptImportKey test512");

			if (ret) {
				ret = FALSE;
				if (pCryptImportKey(cfg->priv[i].hCsp, data, len, cfg->priv[i].hKey, 0, &hKey)) {
					if (pCryptDecrypt(hKey, 0, TRUE, 0, (BYTE *)tmp, (DWORD *)&tmplen)) ret = TRUE;
					else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptDecrypt test512");
					pCryptDestroyKey(hKey);
				}
				else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptImportKey test512");
			}
		}
		else {					// Self Check 1024/2048 bits
			if (pCryptImportKey(cfg->priv[i].hCsp, data, len, 0, 0, &hExKey)) {
				len = 128/8;
				if (pCryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF)) ret = TRUE;
				else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptEncrypt test1024/2048");
				pCryptDestroyKey(hExKey);
			}
			else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptImportKey test1024/2048");

			if (ret) {
				ret = FALSE;
				if (pCryptDecrypt(cfg->priv[i].hKey, 0, TRUE, 0, (BYTE *)data, (DWORD *)&len)) ret = TRUE;
				else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptDecrypt test1024/2048");
			}
		}
		if (!ret) cfg->pub[i].UnSet();
	}

	for (i=0; i < MAX_KEY; i++) {
		if (cfg->pub[i].Key()) {
			ret = TRUE;
		}
		else if (cfg->priv[i].hKey) {
			pCryptDestroyKey(cfg->priv[i].hKey);
			cfg->priv[i].hKey = NULL;
		}
	}

	return	ret;
}

BOOL MakeDefaultRSAKey()
{
	HCRYPTPROV	hCsp = NULL;
	int			flags[] = { CRYPT_NEWKEYSET|CRYPT_MACHINE_KEYSET, CRYPT_NEWKEYSET, -1 };
	const char	*csp_name[] = { MS_DEF_PROV, MS_ENHANCED_PROV, NULL };
	BOOL		ret = TRUE;
	int			i, j;

	for (i=0; csp_name[i]; i++) {
		for (j=0; csp_name[j]; j++) {
			if (pCryptAcquireContext(&hCsp, NULL, csp_name[j], PROV_RSA_FULL, flags[j])) {
				 pCryptReleaseContext(hCsp, 0);
				 hCsp = 0;
			}
		}
		if (csp_name[j] == NULL) ret = FALSE;
	}
	return	ret;
}

BOOL SetupRSAKey(Cfg *cfg, MsgMng *msgMng, KeyType kt, int ctl_flg)
{
	BYTE		data[MAX_BUF_EX];
	char		contName[MAX_BUF];
	int			len = sizeof(data), i;
	HCRYPTPROV&	hCsp     = cfg->priv[kt].hCsp;
	HCRYPTKEY&	hPrivKey = cfg->priv[kt].hKey;
	PubKey&		pubKey   = cfg->pub[kt];
	const char	*csp_name = kt == KEY_512 ? MS_DEF_PROV : MS_ENHANCED_PROV;
	int			key_bits  = kt == KEY_512 ? 512 : kt == KEY_1024 ? 1024 : 2048;

	if (kt == KEY_2048 && !IsWinXP()) return FALSE; // Win2000 can't be trusted for RSA2048...

	int	cap =	kt == KEY_512  ? IPMSG_RSA_512 |IPMSG_RC2_40 :
				kt == KEY_1024 ? IPMSG_RSA_1024|IPMSG_BLOWFISH_128|IPMSG_PACKETNO_IV :
				kt == KEY_2048 ? IPMSG_RSA_2048|IPMSG_AES_256|IPMSG_SIGN_SHA1|IPMSG_PACKETNO_IV : 0;
	int	AcqFlgs[] = { CRYPT_MACHINE_KEYSET, 0, CRYPT_NEWKEYSET|CRYPT_MACHINE_KEYSET, CRYPT_NEWKEYSET, -1 };

	if (pCryptStringToBinary && pCryptBinaryToString) {
		cap |= IPMSG_ENCODE_BASE64;
	}

	wsprintf(contName, "ipmsg.rsa%d.%s", key_bits, msgMng->GetLocalHostA()->userName);

	if (hCsp) {
		pCryptReleaseContext(hCsp, 0);
		hCsp = NULL;
	}

// デフォルトキーセットを作成しておく
	if (TIsEnableUAC() && TIsUserAnAdmin()) {
		MakeDefaultRSAKey();
	}

// rebuld 時には、事前に公開鍵を消去
	if ((ctl_flg & KEY_REBUILD) && pubKey.Key() == NULL) {
		if (!pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, CRYPT_DELETEKEYSET|CRYPT_MACHINE_KEYSET))
			if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptAcquireContext(destroy)");
		pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
	}

// open key cotainer
	for (i=0; AcqFlgs[i] != -1; i++) {
		hCsp = NULL;
		if (pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, AcqFlgs[i]))
			break;
	}
	if (hCsp == NULL) {
		if (kt == KEY_512 && (ctl_flg & KEY_DIAG))
			GetLastErrorMsg("CryptAcquireContext");
		return	FALSE;
	}

// プライベート鍵をimport (1024/2048bit)
	if (cfg->priv[kt].blob) {
		if (LoadPrivBlob(&cfg->priv[kt], data, &len)
			&& !pCryptImportKey(hCsp, data, len, 0, CRYPT_EXPORTABLE, &hPrivKey))
		{	// import is fail...
			if (ctl_flg & KEY_DIAG)
				GetLastErrorMsg("CryptImportKey(blob)");
			// コケた場合、再Acquireしないと副作用が残ることがある...
			pCryptReleaseContext(hCsp, 0), hCsp = NULL;
			if (!pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
				pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, 0);
		}
		if (hPrivKey == NULL) {
			if (cfg->priv[kt].encryptType == PRIV_BLOB_USER && cfg->priv[kt].encryptSeed) {
				::SendMessage(GetMainWnd(), WM_FORCE_TERMINATE, 0, 0); // 強制終了
				return	FALSE;
			}
			delete [] cfg->priv[kt].blob;
			cfg->priv[kt].blob = NULL;
		}
	}

// 初回 or 512bit 鍵の場合は、hCsp からプライベート鍵ハンドルを取得
	if (hPrivKey == NULL) {
		if (!pCryptGetUserKey(hCsp, AT_KEYEXCHANGE, &hPrivKey)
		&& !pCryptGenKey(hCsp, CALG_RSA_KEYX, (key_bits << 16) | CRYPT_EXPORTABLE, &hPrivKey))
			if (ctl_flg & KEY_DIAG)
				GetLastErrorMsg("CryptGenKey");
	}

// 公開鍵を export
	if (pCryptExportKey(hPrivKey, 0, PUBLICKEYBLOB, 0, data, (DWORD *)&len)) {
		if (len < key_bits / 8) {	// 鍵長の短いキーペア（v2.50b14 で発生する可能性）
			pCryptDestroyKey(hPrivKey);
			hPrivKey = NULL;
			pCryptReleaseContext(hCsp, 0);
			hCsp = NULL;
			pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, CRYPT_DELETEKEYSET|CRYPT_MACHINE_KEYSET);
			pCryptAcquireContext(&hCsp, contName, csp_name, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
			if (ctl_flg & KEY_INTERNAL)
				return FALSE;
			return	SetupRSAKey(cfg, msgMng, kt, ctl_flg | KEY_INTERNAL);
		}
		pubKey.SetByBlob(data, cap);
	}
	else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptExportKey");

// プライベート鍵を保存
	if (kt != KEY_512 && cfg->priv[kt].blob == NULL && hPrivKey) {
		len = sizeof(data);
		if (pCryptExportKey(hPrivKey, 0, PRIVATEKEYBLOB, 0, data, (DWORD *)&len)) {
			if (StorePrivBlob(&cfg->priv[kt], data, len)) {
				cfg->WriteRegistry(CFG_CRYPT);
			}
		}
	}
	return	TRUE;
}

BOOL LoadPrivBlob(PrivKey *priv, BYTE *rawBlob, int *rawBlobLen)
{
	if (priv->blob == NULL) return	FALSE;

	BYTE	key[MAX_BUF_EX];

	if (priv->encryptType == PRIV_BLOB_RAW) {
		memcpy(rawBlob, priv->blob, *rawBlobLen = priv->blobLen);
		return	TRUE;
	}
	else if (priv->encryptType == PRIV_BLOB_USER)
	{
		if (priv->encryptSeed == NULL)
			return	FALSE;
		while (1)
		{
			TPasswordDlg	dlg((char *)key);
			if (!dlg.Exec())
				return	FALSE;
			CBlowFish	bl(key, strlen((char *)key));
			if (bl.Decrypt(priv->encryptSeed, key, priv->encryptSeedLen) == PRIV_SEED_LEN && memcmp(key, PRIV_SEED_HEADER, PRIV_SEED_HEADER_LEN) == 0)
				break;
		}
	}
	else if (priv->encryptType == PRIV_BLOB_DPAPI)
	{
		if (priv->encryptSeed == NULL)
			return	FALSE;
		DATA_BLOB	in = { priv->encryptSeedLen, priv->encryptSeed }, out;
		if (!pCryptUnprotectData(&in, 0, 0, 0, 0,
				CRYPTPROTECT_LOCAL_MACHINE|CRYPTPROTECT_UI_FORBIDDEN, &out))
			return	FALSE;
		memcpy(key, out.pbData, out.cbData);
		::LocalFree(out.pbData);
		if (out.cbData != PRIV_SEED_LEN)
			return	FALSE;
	}
	else return	FALSE;

	CBlowFish	bl(key + PRIV_SEED_HEADER_LEN, 128/8);
	return (*rawBlobLen = bl.Decrypt(priv->blob, rawBlob, priv->blobLen)) != 0;
}

BOOL StorePrivBlob(PrivKey *priv, BYTE *rawBlob, int rawBlobLen)
{
	delete	priv->blob;
	priv->blob = NULL;
	delete	priv->encryptSeed;
	priv->encryptSeed = NULL;
	priv->blobLen = priv->encryptSeedLen = 0;

	BYTE	data[MAX_BUF_EX], *encodeBlob = data;

	if (priv->encryptType == PRIV_BLOB_RAW) {
		encodeBlob = rawBlob;
		priv->blobLen = rawBlobLen;
	}
	else {
		BYTE	seed[PRIV_SEED_LEN], *seedCore = seed + PRIV_SEED_HEADER_LEN;
		// seed の作成
		memcpy(seed, PRIV_SEED_HEADER, PRIV_SEED_HEADER_LEN);
		pCryptGenRandom(priv->hCsp, 128/8, seedCore);

		if (priv->encryptType == PRIV_BLOB_USER) {
			TPasswordDlg	dlg((char *)data);
			if (!dlg.Exec())
				return	FALSE;
			// seed の暗号化
			CBlowFish	bl(data, strlen((char *)data));
			priv->encryptSeedLen = bl.Encrypt(seed, data, PRIV_SEED_LEN);
			priv->encryptSeed = new BYTE [priv->encryptSeedLen];
			memcpy(priv->encryptSeed, data, priv->encryptSeedLen);
		}
		else if (priv->encryptType == PRIV_BLOB_DPAPI) {
			// seed の暗号化
			DATA_BLOB in = { PRIV_SEED_LEN, seed }, out;
			if (!pCryptProtectData(&in, L"ipmsg", 0, 0, 0, CRYPTPROTECT_LOCAL_MACHINE|CRYPTPROTECT_UI_FORBIDDEN, &out))
				return	FALSE;
			priv->encryptSeed = new BYTE [priv->encryptSeedLen = out.cbData];
			memcpy(priv->encryptSeed, out.pbData, out.cbData);
			::LocalFree(out.pbData);
		}
		else return	FALSE;
		// seed による、暗号化blob の作成
		CBlowFish	bl(seedCore, 128/8);
		priv->blobLen = bl.Encrypt(rawBlob, encodeBlob, rawBlobLen);
	}

	priv->blob = new BYTE [priv->blobLen];
	memcpy(priv->blob, encodeBlob, priv->blobLen);
	return	TRUE;
}

