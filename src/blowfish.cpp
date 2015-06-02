// blowfish.cpp   C++ class implementation of the BLOWFISH encryption algorithm
// _THE BLOWFISH ENCRYPTION ALGORITHM_
// by Bruce Schneier
// Revised code--3/20/94
// Converted to C++ class 5/96, Jim Conger
//
// modify H.Shirouzu 07/2002 (add change_order(), CBC mode)
// modify H.Shirouzu 01/2004 (add PKCS#5 mode)

#include <string.h>
#include "blowfish.h"
#include "blowfish.h2"	// holds the random digit tables

#define S(x,i) (SBoxes[i][x.w.byte##i])
#define bf_F(x) (((S(x,0) + S(x,1)) ^ S(x,2)) + S(x,3))
#define ROUND(a,b,n) (a.dword ^= bf_F(b) ^ PArray[n])

CBlowFish::CBlowFish (const BYTE *key, int keybytes)
{
	PArray = new DWORD [NPASS + 2];
	SBoxes = new DWORD [4][256];

	if (key)
		Initialize(key, keybytes);
}

CBlowFish::~CBlowFish ()
{
	delete [] SBoxes;
	delete [] PArray;
}

	// the low level (private) encryption function
void CBlowFish::Blowfish_encipher(DWORD *xl, DWORD *xr)
{
	union aword  Xl, Xr;

	Xl.dword = *xl;
	Xr.dword = *xr;

	Xl.dword ^= PArray[0];
	ROUND(Xr, Xl, 1);  ROUND(Xl, Xr, 2);
	ROUND(Xr, Xl, 3);  ROUND(Xl, Xr, 4);
	ROUND(Xr, Xl, 5);  ROUND(Xl, Xr, 6);
	ROUND(Xr, Xl, 7);  ROUND(Xl, Xr, 8);
	ROUND(Xr, Xl, 9);  ROUND(Xl, Xr, 10);
	ROUND(Xr, Xl, 11); ROUND(Xl, Xr, 12);
	ROUND(Xr, Xl, 13); ROUND(Xl, Xr, 14);
	ROUND(Xr, Xl, 15); ROUND(Xl, Xr, 16);
	Xr.dword ^= PArray[17];

	*xr = Xl.dword;
	*xl = Xr.dword;
}

	// the low level (private) decryption function
void CBlowFish::Blowfish_decipher(DWORD *xl, DWORD *xr)
{
	union aword  Xl;
	union aword  Xr;

	Xl.dword = *xl;
	Xr.dword = *xr;

	Xl.dword ^= PArray[17];
	ROUND(Xr, Xl, 16);  ROUND(Xl, Xr, 15);
	ROUND(Xr, Xl, 14);  ROUND(Xl, Xr, 13);
	ROUND(Xr, Xl, 12);  ROUND(Xl, Xr, 11);
	ROUND(Xr, Xl, 10);  ROUND(Xl, Xr, 9);
	ROUND(Xr, Xl, 8);   ROUND(Xl, Xr, 7);
	ROUND(Xr, Xl, 6);   ROUND(Xl, Xr, 5);
	ROUND(Xr, Xl, 4);   ROUND(Xl, Xr, 3);
	ROUND(Xr, Xl, 2);   ROUND(Xl, Xr, 1);
	Xr.dword ^= PArray[0];

	*xl = Xr.dword;
	*xr = Xl.dword;
}


	// constructs the enctryption sieve
void CBlowFish::Initialize(const BYTE *key, int keybytes)
{
	int  		i, j;
	DWORD		datal, datar;
	union aword	temp;

	// first fill arrays from data tables
	for (i = 0; i < NPASS + 2; i++)
		PArray[i] = bf_P[i];

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 256; j++)
			SBoxes[i][j] = bf_S[i][j];
	}

	for (i=0, j=0; i < NPASS + 2; i++)
	{
		temp.w.byte0 = key[j++ % keybytes];
		temp.w.byte1 = key[j++ % keybytes];
		temp.w.byte2 = key[j++ % keybytes];
		temp.w.byte3 = key[j++ % keybytes];
		PArray[i] ^= temp.dword;
	}

	datal = datar = 0;

	for (i = 0; i < NPASS + 2; i += 2)
	{
		Blowfish_encipher(&datal, &datar);
		PArray[i] = datal;
		PArray[i + 1] = datar;
	}

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 256; j += 2)
		{
			Blowfish_encipher(&datal, &datar);
			SBoxes[i][j] = datal;
			SBoxes[i][j + 1] = datar;
		}
	}
}

	// get output length, which must be even MOD 8
DWORD CBlowFish::GetOutputLength(DWORD lInputLong, int mode)
{
	if (mode & BF_PKCS5)
		lInputLong++;
	return	ALIGN_SIZE(lInputLong, BF_BLKSIZE);
}

	// change byte order
#ifdef ORDER_ABCD
#define change_order
#else
inline void change_order(DWORD *val)
{
	aword	org_val;
	org_val.dword = *val;

	((aword *)val)->w.byte0 = org_val.byte[0];
	((aword *)val)->w.byte1 = org_val.byte[1];
	((aword *)val)->w.byte2 = org_val.byte[2];
	((aword *)val)->w.byte3 = org_val.byte[3];
}
#endif

	// Encrypt pIntput into pOutput.  Input length in lSize.  Returned value
	// is length of output which will be even MOD 8 bytes.  Input buffer and
	// output buffer can be the same, but be sure buffer length is even MOD 8.
DWORD CBlowFish::Encrypt(const BYTE *pInput, BYTE *pOutput, DWORD lSize, int mode, BYTE *_IV)
{
	DWORD 	lCount, lOutSize, *dwOutput = (DWORD *)pOutput;
	_int64	IV = 0;

	if (_IV) memcpy(&IV, _IV, sizeof(IV));

	if (pInput != pOutput)
		memcpy(pOutput, pInput, lSize);

	lOutSize = GetOutputLength(lSize, mode);
	memset(pOutput + lSize, (mode & BF_PKCS5) ? lOutSize - lSize : 0, lOutSize - lSize);

	for (lCount = 0; lCount < lOutSize; lCount += BF_BLKSIZE)
	{
		if (mode & BF_CBC)
			*(_int64 *)dwOutput ^= IV;

		change_order(dwOutput); change_order(dwOutput + 1);

		Blowfish_encipher(dwOutput, dwOutput + 1);

		change_order(dwOutput); change_order(dwOutput + 1);

		if (mode & BF_CBC)
			IV = *(_int64 *)dwOutput;
		dwOutput += 2;
	}

	return lCount;
}

	// Decrypt pIntput into pOutput.  Input length in lSize.  Input buffer and
	// output buffer can be the same, but be sure buffer length is even MOD 8.
DWORD CBlowFish::Decrypt(const BYTE *pInput, BYTE *pOutput, DWORD lSize, int mode, BYTE *_IV)
{
	DWORD	lCount, *dwOutput = (DWORD *)pOutput;
	_int64	prevIV = 0;
	_int64	IV = 0;

	if (_IV) memcpy(&IV, _IV, sizeof(IV));

	if (pInput != pOutput)
		memcpy(pOutput, pInput, lSize);

	for (lCount = 0; lCount < lSize; lCount += BF_BLKSIZE)
	{
		if (mode & BF_CBC)
		{
			prevIV = IV;
			IV = *(_int64 *)dwOutput;
		}

		change_order(dwOutput); change_order(dwOutput + 1);

		Blowfish_decipher(dwOutput, dwOutput + 1);

		change_order(dwOutput); change_order(dwOutput + 1);

		if (mode & BF_CBC)
			*(_int64 *)dwOutput ^= prevIV;
		dwOutput += 2;
	}

	if (mode & BF_PKCS5)
	{
		DWORD	paddingLen = pOutput[lCount - 1];
		if (paddingLen <= 0 || paddingLen > BF_BLKSIZE || pOutput[lCount - paddingLen] != paddingLen)
			return	0;	// error
		pOutput[lCount -= paddingLen] = 0;
	}
	return	lCount;
}

