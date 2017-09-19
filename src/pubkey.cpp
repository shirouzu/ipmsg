static char *pubkey_id = 
	"@(#)Copyright (C) H.Shirouzu 2011-2017   pubkey.cpp	Ver4.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Public Key Encryption
	Create					: 2011-05-03(Tue)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "blowfish.h"

/* VC4 で CryptoAPI を使用可能にする */
BOOL SetupCryptAPI(Cfg *cfg)
{
	DWORD	flag = 0;

	if (cfg->priv[KEY_1024].encryptType == PRIV_BLOB_RESET) {
		flag |= KEY_REBUILD;
	}
	SetupCryptAPICore(cfg, flag);

// 起動直後に、まれにコケる環境があるらしいので、わずかにリトライ...
#define MAX_RETRY	3
	int	cnt = 0;

	while (1) {
		BOOL	need_retry = FALSE;

		for (int i=0; i < MAX_KEY; i++) {
			if (cfg->priv[i].hCsp && cfg->pub[i].Key() == 0) {
				need_retry = TRUE;
				break;
			}
		}

		if (++cnt > MAX_RETRY || !need_retry) {
			break;
		}

		for (int i=0; i < MAX_KEY; i++) {
			if (cfg->priv[i].hCsp) {
				::CryptReleaseContext(cfg->priv[i].hCsp, 0);
				cfg->priv[i].hCsp = NULL;
			}
			if (cfg->priv[i].hPubCsp) {
				::CryptReleaseContext(cfg->priv[i].hPubCsp, 0);
				cfg->priv[i].hPubCsp = NULL;
			}
		}
		::Sleep(1000);
		SetupCryptAPICore(cfg, (cnt == MAX_RETRY ? KEY_DIAG : 0) | flag);
	}

	BOOL	ret = FALSE;
	for (int i=0; i < MAX_KEY; i++) {
		if (cfg->pub[i].Key()) {
			ret = TRUE;
			break;
		}
	}

	if (cnt > MAX_RETRY || !ret) {
		if (MessageBox(GetMainWnd(), "RSA keyset can't access. Create New RSA key?",
				IP_MSG, MB_OKCANCEL) == IDOK) {
			SetupCryptAPICore(cfg, KEY_REBUILD|KEY_DIAG|flag);
		}
	}

	if (cfg->pub[KEY_2048].Capa() & IPMSG_SIGN_SHA1) {
		HostSub *host;
		host = MsgMng::GetLocalHost();
		GenUserNameDigest(host->u.userName, cfg->pub[KEY_2048].Key(), host->u.userName);
		host = MsgMng::GetLocalHostA();
		GenUserNameDigest(host->u.userName, cfg->pub[KEY_2048].Key(), host->u.userName);
	}

	return	ret;
}

BOOL SetupCryptAPICore(Cfg *cfg, int ctl_flg)
{
	BYTE	data[MAX_BUF_EX];
	int		len = sizeof(data);
	BOOL	ret = FALSE;

// RSA 鍵の生成
	for (int i=0; i < MAX_KEY; i++) {
		SetupRSAKey(cfg, (KeyType)i, ctl_flg);
	}

	for (int i=0; i < MAX_KEY; i++) {
		if (!cfg->pub[i].Key()) {
			continue;
		}

		BOOL		ret = FALSE;
		HCRYPTKEY	hKey = 0, hExKey = 0;
		DWORD		tmplen = MAX_BUF / 2;

		cfg->pub[i].KeyBlob(data, sizeof(data), &len);
		// Self Check 1024/2048 bits
		if (::CryptImportKey(cfg->priv[i].hPubCsp, data, len, 0, 0, &hExKey)) {
			len = 128/8;
			if (::CryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF)) {
				ret = TRUE;
			}
			else if (ctl_flg & KEY_DIAG) {
				GetLastErrorMsg("CryptEncrypt test1024/2048");
			}
			::CryptDestroyKey(hExKey);
		}
		else if (ctl_flg & KEY_DIAG) {
			GetLastErrorMsg("CryptImportKey test1024/2048");
		}

		if (i == KEY_2048) {
			DWORD	calg_list[] = {CALG_SHA_256, CALG_SHA};
			DWORD	ok_calg = 0;

			for (auto &calg: calg_list) {
				HCRYPTHASH	hHash;
				if (!::CryptCreateHash(cfg->priv[i].hCsp, calg, 0, 0, &hHash)) {
					Debug("CryptCreateHash %d (%x)\n", calg==CALG_SHA, GetLastError());
					continue;
				}
				tmplen = 512;
				BYTE	tmp[MAX_BUF] = {};
				if (::CryptHashData(hHash, tmp, 1, 0)) {
					if (::CryptSignHashW(hHash, AT_KEYEXCHANGE, 0, 0, 0, &tmplen)) {
						if (::CryptSignHashW(hHash, AT_KEYEXCHANGE, 0, 0, tmp+512, &tmplen)) {
							ok_calg = calg;
							break;
						}
						else Debug("CryptSignHash %d (%x)\n", calg==CALG_SHA, GetLastError());
					}
					else Debug("CryptSignHash2 %d (%x)\n", calg==CALG_SHA, GetLastError());
				}
				else Debug("CryptHashData %d (%x)\n", calg==CALG_SHA, GetLastError());

				::CryptDestroyHash(hHash);
			}
			if (ok_calg != CALG_SHA_256) {
				cfg->pub[i].SetCapa(
					cfg->pub[i].Capa() & ~(IPMSG_SIGN_SHA256|(ok_calg ? 0 : IPMSG_SIGN_SHA1)));
			}
		}
		if (ret) {
			ret = FALSE;
			if (::CryptDecrypt(cfg->priv[i].hKey, 0, TRUE, 0, (BYTE *)data, (DWORD *)&len)) {
				ret = TRUE;
			}
			else if (ctl_flg & KEY_DIAG) {
				GetLastErrorMsg("CryptDecrypt test1024/2048");
			}
		}
		if (!ret) {
			cfg->pub[i].UnSet();
		}
	}

	for (int i=0; i < MAX_KEY; i++) {
		if (cfg->pub[i].Key()) {
			ret = TRUE;
		}
		else if (cfg->priv[i].hKey) {
			::CryptDestroyKey(cfg->priv[i].hKey);
			cfg->priv[i].hKey = NULL;
		}
	}

	return	ret;
}

const char *GetCSP(DWORD *prov_type)
{
	if (IsWin2003Only()) {
		*prov_type = PROV_RSA_FULL;
		return	MS_ENHANCED_PROV;
	}

	*prov_type = PROV_RSA_AES;
	return	IsWinVista() ? MS_ENH_RSA_AES_PROV : MS_ENH_RSA_AES_PROV_XP;
}

BOOL MakeDefaultRSAKey()
{
	int			flags[] = { CRYPT_NEWKEYSET|CRYPT_MACHINE_KEYSET, CRYPT_NEWKEYSET, 0 };
	DWORD		prov_type = 0;
	const char	*csp_name = GetCSP(&prov_type);
	BOOL		ret = FALSE;

	for (int i=0; flags[i]; i++) {
		HCRYPTPROV	hCsp = NULL;
		if (::CryptAcquireContext(&hCsp, NULL, csp_name, prov_type, flags[i])) {
			::CryptReleaseContext(hCsp, 0);
			ret = TRUE;
			break;
		}
	}
	return	ret;
}

BOOL SetupRSAKey(Cfg *cfg, KeyType kt, int ctl_flg)
{
	BYTE		data[MAX_BUF_EX];
	char		contName[MAX_BUF];
	int			len = sizeof(data);
	HCRYPTPROV&	hCsp     = cfg->priv[kt].hCsp;
	HCRYPTPROV&	hPubCsp  = cfg->priv[kt].hPubCsp;
	HCRYPTKEY&	hPrivKey = cfg->priv[kt].hKey;
	PubKey&		pubKey   = cfg->pub[kt];
	PrivKey&	privKey  = cfg->priv[kt];
	DWORD		prov_type = 0;
	const char	*csp_name = GetCSP(&prov_type);
	int			key_bits  = (kt == KEY_1024) ? 1024 : 2048;

	if (kt == KEY_2048 && !IsWinXP()) {
		return FALSE; // Win2000 can't be trusted for RSA2048...
	}

	int	cap = kt == KEY_1024 ? (IPMSG_RSA_1024|IPMSG_BLOWFISH_128|IPMSG_PACKETNO_IV) :
			  kt == KEY_2048 ? (IPMSG_RSA_2048|IPMSG_AES_256|
			  					IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256|
			  					IPMSG_PACKETNO_IV|IPMSG_NOENC_FILEBODY)
			  				 : 0;

	if (IsWinXP()) {
		cap |= IPMSG_ENCODE_BASE64;
	}

	int	AcqFlgs[] = { CRYPT_MACHINE_KEYSET, 0, CRYPT_NEWKEYSET|CRYPT_MACHINE_KEYSET,
		CRYPT_NEWKEYSET, -1 };

	sprintf(contName, "ipmsg.rsa%d.%s", key_bits, MsgMng::GetLocalHostA()->u.userName);

	if (hCsp) {
		::CryptReleaseContext(hCsp, 0);
		hCsp = NULL;
	}
	if (hPubCsp) {
		::CryptReleaseContext(hPubCsp, 0);
		hPubCsp = NULL;
	}

// デフォルトキーセットを作成しておく
	if (TIsEnableUAC() && ::IsUserAnAdmin()) {
		MakeDefaultRSAKey();
	}

// rebuld 時には、事前に公開鍵を消去
	if ((ctl_flg & KEY_REBUILD) /* && pubKey.Key() == NULL */ ) {
		if (hPrivKey) {
			::CryptDestroyKey(hPrivKey);
			hPrivKey = NULL;
		}
		if (!::CryptAcquireContext(&hCsp, contName, csp_name, prov_type,
				CRYPT_DELETEKEYSET|CRYPT_MACHINE_KEYSET)) {
			if (ctl_flg & KEY_DIAG) {
				GetLastErrorMsg("CryptAcquireContext(destroy)");
			}
		}
		::CryptAcquireContext(&hCsp, contName, csp_name, prov_type, CRYPT_DELETEKEYSET);
		pubKey.UnSet();
		privKey.UnSet();
	}

// open key cotainer
	for (int i=0; AcqFlgs[i] != -1; i++) {
		hCsp = NULL;
		hPubCsp = NULL;
		if (::CryptAcquireContext(&hCsp, contName, csp_name, prov_type, AcqFlgs[i])) {
			::CryptAcquireContext(&hPubCsp, NULL, csp_name, prov_type, CRYPT_VERIFYCONTEXT);
			break;
		}
	}
	if (hCsp == NULL) {
		if (kt == KEY_1024 && (ctl_flg & KEY_DIAG)) {
			GetLastErrorMsg("CryptAcquireContext");
		}
		return	FALSE;
	}

// プライベート鍵をimport (1024/2048bit)
	if (cfg->priv[kt].blob) {
		if (LoadPrivBlob(&cfg->priv[kt], data, &len)
			&& !::CryptImportKey(hCsp, data, len, 0, CRYPT_EXPORTABLE, &hPrivKey)) {
			// import is fail...
			if (ctl_flg & KEY_DIAG) {
				GetLastErrorMsg("CryptImportKey(blob)");
			}
			// コケた場合、再Acquireしないと副作用が残ることがある...
			::CryptReleaseContext(hCsp, 0);
			::CryptReleaseContext(hPubCsp, 0);
			hCsp = NULL;
			hPubCsp = NULL;

			::CryptAcquireContext(&hCsp, contName, csp_name, prov_type, CRYPT_MACHINE_KEYSET);
			::CryptAcquireContext(&hPubCsp, NULL, csp_name, prov_type, CRYPT_VERIFYCONTEXT);
		}
		if (hPrivKey == NULL) {
			if (cfg->priv[kt].encryptType == PRIV_BLOB_USER && cfg->priv[kt].encryptSeed) {
				::SendMessage(GetMainWnd(), WM_FORCE_TERMINATE, 0, 0); // 強制終了
				return	FALSE;
			}
			cfg->priv[kt].ClearBlob();
		}
	}

// 初回 or 512bit 鍵の場合は、hCsp からプライベート鍵ハンドルを取得
	if (hPrivKey == NULL) {
		if (!::CryptGetUserKey(hCsp, AT_KEYEXCHANGE, &hPrivKey)
		&& !::CryptGenKey(hCsp, CALG_RSA_KEYX, (key_bits << 16) | CRYPT_EXPORTABLE, &hPrivKey)) {
			if (ctl_flg & KEY_DIAG) {
				GetLastErrorMsg("CryptGenKey");
			}
		}
	}

// 公開鍵を export
	if (::CryptExportKey(hPrivKey, 0, PUBLICKEYBLOB, 0, data, (DWORD *)&len)) {
		pubKey.SetByBlob(data, cap);
	}
	else if (ctl_flg & KEY_DIAG) GetLastErrorMsg("CryptExportKey");

// プライベート鍵を保存
	if (hPrivKey &&
		(cfg->priv[kt].blob == NULL || cfg->priv[kt].encryptType == PRIV_BLOB_DPAPI)) {
		len = sizeof(data);
		if (::CryptExportKey(hPrivKey, 0, PRIVATEKEYBLOB, 0, data, (DWORD *)&len)) {
			if (cfg->priv[kt].encryptType == PRIV_BLOB_DPAPI) {
				cfg->priv[kt].encryptType = PRIV_BLOB_RAW;	// 生データ形式に変更
			}
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
	else if (priv->encryptType == PRIV_BLOB_USER) {
		if (priv->encryptSeed == NULL) {
			return FALSE;
		}
		while (1) {
			TPasswordDlg	dlg((char *)key);

			if (dlg.Exec() != IDOK) {
				return	FALSE;
			}
			CBlowFish	bl(key, (int)strlen((char *)key));

			if (bl.Decrypt(priv->encryptSeed, key, priv->encryptSeedLen) == PRIV_SEED_LEN &&
				memcmp(key, PRIV_SEED_HEADER, PRIV_SEED_HEADER_LEN) == 0) {
				break;
			}
		}
	}
	else if (priv->encryptType == PRIV_BLOB_DPAPI) {
		if (priv->encryptSeed == NULL) {
			return FALSE;
		}

		DATA_BLOB	in = { (DWORD)priv->encryptSeedLen, priv->encryptSeed }, out;
		if (!::CryptUnprotectData(&in, 0, 0, 0, 0,
				 CRYPTPROTECT_LOCAL_MACHINE|CRYPTPROTECT_UI_FORBIDDEN, &out)) {
			return	FALSE;
		}
		memcpy(key, out.pbData, out.cbData);
		::LocalFree(out.pbData);
		if (out.cbData != PRIV_SEED_LEN) {
			return	FALSE;
		}
	}
	else {
		return	FALSE;
	}

	CBlowFish	bl(key + PRIV_SEED_HEADER_LEN, 128/8);
	return (*rawBlobLen = bl.Decrypt(priv->blob, rawBlob, priv->blobLen)) != 0;
}

BOOL StorePrivBlob(PrivKey *priv, BYTE *rawBlob, int rawBlobLen)
{
	int		blobLen = 0;
	BYTE	data[MAX_BUF_EX];
	BYTE	*encodeBlob = data;

	if (priv->encryptType == PRIV_BLOB_RAW) {
		encodeBlob = rawBlob;
		blobLen    = rawBlobLen;
	}
	else {
		BYTE	seed[PRIV_SEED_LEN];
		BYTE	*seedCore = seed + PRIV_SEED_HEADER_LEN;
		// seed の作成
		memcpy(seed, PRIV_SEED_HEADER, PRIV_SEED_HEADER_LEN);
		::CryptGenRandom(priv->hCsp, 128/8, seedCore);

		if (priv->encryptType == PRIV_BLOB_USER) {
			TPasswordDlg	dlg((char *)data);
			if (dlg.Exec() != IDOK) {
				return	FALSE;
			}
			// seed の暗号化
			CBlowFish	bl(data, (int)strlen((char *)data));
			int			len = bl.Encrypt(seed, data, PRIV_SEED_LEN);
			priv->SetSeed(data, len);
		}
		else if (priv->encryptType == PRIV_BLOB_DPAPI) {
			// seed の暗号化
			DATA_BLOB in = { PRIV_SEED_LEN, seed }, out;
			if (!::CryptProtectData(&in, L"ipmsg", 0, 0, 0,
					CRYPTPROTECT_LOCAL_MACHINE|CRYPTPROTECT_UI_FORBIDDEN, &out)) {
				return	FALSE;
			}
			priv->SetSeed(out.pbData, out.cbData);
			::LocalFree(out.pbData);
		}
		else {
			return	FALSE;
		}
		// seed による、暗号化blob の作成
		CBlowFish	bl(seedCore, 128/8);
		blobLen = bl.Encrypt(rawBlob, encodeBlob, rawBlobLen);
	}

	priv->SetBlob(encodeBlob, blobLen);
	return	TRUE;
}

