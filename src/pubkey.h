/*	@(#)Copyright (C) H.Shirouzu 2013-2017   pubkey.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Public Key
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef PUBKEY_H
#define PUBKEY_H

#define PRIV_BLOB_USER			0x0001
#define PRIV_BLOB_DPAPI			0x0002
#define PRIV_BLOB_RAW			0x0003
#define PRIV_BLOB_RESET			0x0004
#define PRIV_SEED_HEADER		"ipmsg:"
#define PRIV_SEED_HEADER_LEN	6
#define PRIV_SEED_LEN			(PRIV_SEED_HEADER_LEN + (128/8))	// 128bit seed

#define RSA2048_KEY_SIZE		(2048 / 8)
#define RSA2048_SIGN_SIZE		RSA2048_KEY_SIZE
#define AES256_SIZE				(256 / 8)

struct PrivKey {
	BYTE		*blob;
	int			blobLen;
	int			encryptType;
	BYTE		*encryptSeed;
	int			encryptSeedLen;
	HCRYPTKEY	hKey;
	HCRYPTPROV	hCsp;
	HCRYPTPROV	hPubCsp;

	PrivKey()  {
		blob    = encryptSeed    = NULL;
		blobLen = encryptSeedLen = 0;
		encryptType = 0;
		hKey = NULL;
		hCsp = NULL;
		hPubCsp = NULL;
	}
	~PrivKey() {
		delete [] blob;
		delete [] encryptSeed;
	}

	void SetBlob(BYTE *_blob, int _len) {
		blob = new BYTE [blobLen = _len];
		memcpy(blob, _blob, blobLen);
	}
	void ClearBlob() {
		delete [] blob;
		blob    = NULL;
		blobLen = 0;
	}
	void SetSeed(BYTE *_encryptSeed, int _len) {
		encryptSeed = new BYTE [encryptSeedLen = _len];
		memcpy(encryptSeed, _encryptSeed, encryptSeedLen);
	}
	void ClearSeed() {
		delete [] encryptSeed;
		encryptSeed    = NULL;
		encryptSeedLen = 0;
	}
	void UnSet() {
		ClearBlob();
		ClearSeed();
	}
};

class PubKey {
protected:
	int		keyLen;
	int		e;
	int		capa;
	int		dummy;	// for 8byte align
	BYTE	*key;

public:
	PubKey(void) {
		FirstInit();
	}
	~PubKey() { UnSet(); }

	// 代入対応
	PubKey(const PubKey& o) {
		FirstInit();
		*this = o;
	}
	void FirstInit() {
		key = NULL;
		keyLen = 0;
		e = 0;
		capa = 0;
		dummy = 0;
	}

	PubKey &operator=(const PubKey &o) {
		Set(o.key, o.keyLen, o.e, o.capa);
		dummy = 0;
		return	*this;
	}

	void Set(BYTE *_key, int len, int _e, int _capa) {
		UnSet();
		e = _e;
		capa = _capa;
		key = new BYTE [keyLen=len];
		memcpy(key, _key, len);
	}
	void UnSet(void) {
		delete [] key;
		key = NULL;
		keyLen = 0;
		e = 0;
		capa = 0;
	}
	const BYTE *Key(void) {
		return key;
	}
	int KeyLen(void) {
		return keyLen;
	}
	int Exponent(void) {
		return e;
	}
	BOOL KeyBlob(BYTE *blob, int maxLen, int *len) {
		if ((*len = KeyBlobLen()) > maxLen) {
			return FALSE;
		}
		/* PUBLICSTRUC */
		blob[0] = PUBLICKEYBLOB;
		blob[1] = CUR_BLOB_VERSION;
		*(WORD *)(blob+2) = 0;
		*(ALG_ID *)(blob+4) = CALG_RSA_KEYX;

		/* RSAPUBKEY */
		memcpy(blob+8, "RSA1", 4);
		*(DWORD *)(blob+12) = keyLen * 8;
		*(int *)(blob+16) = e;

		/* PUBKEY_DATA */
		memcpy(blob+20, key, keyLen);

		return	TRUE;
	}
	int KeyBlobLen(void) {
		return keyLen + 8 + 12; /* PUBLICKEYSTRUC + RSAPUBKEY */ 
	}
	void SetByBlob(BYTE *blob, int _capa) {
		UnSet();
		keyLen = *(int *)(blob+12) / 8;
		key = new BYTE [keyLen];
		memcpy(key, blob+20, keyLen);
		e = *(int *)(blob+16);
		capa = _capa;
	}
	int Capa(void) {
		return capa;
	}
	void SetCapa(int _capa) {
		capa = _capa;
	}

	int Serialize(BYTE *buf, int size) {
		int	offset = offsetof(PubKey, key);
		if (offset + keyLen > size) {
			return -1;
		}
		memcpy(buf, this, offset);
		if (keyLen > 0) {
			memcpy(buf + offset, key, keyLen);
		}
		return	offset + keyLen;
	}
	BOOL DeSerialize(BYTE *buf, int size) {
		int	offset = offsetof(PubKey, key);
		if (offset >= size) {
			return FALSE;
		}
		PubKey	*k = (PubKey *)buf;
		BYTE	*n = (BYTE *)&k->key;

		if (k->keyLen + offset != size) {
			if (k->keyLen + offset - 4 == size) {	// old 32bit data
				n -= 4;
			}
			else {
				return FALSE;
			}
		}
		Set(n, k->keyLen, k->e, k->capa);
		return	TRUE;
	}
};

#define		KEY_REBUILD		0x0001
#define		KEY_DIAG		0x0002
#define		KEY_INTERNAL	0x0004

struct Cfg;

enum KeyType { KEY_1024, KEY_2048, MAX_KEY };

BOOL	SetupCryptAPI(Cfg *cfg);
BOOL	SetupCryptAPICore(Cfg *cfg, int ctl_flg = 0);
BOOL	SetupRSAKey(Cfg *cfg, KeyType key_type, int ctl_flg = 0);
BOOL	LoadPrivBlob(PrivKey *priv, BYTE *rawBlob, int *rawBlobLen);
BOOL	StorePrivBlob(PrivKey *priv, BYTE *rawBlob, int rawBlobLen);

#endif

