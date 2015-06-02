// blowfish.h     interface file for blowfish.cpp
// _THE BLOWFISH ENCRYPTION ALGORITHM_
// by Bruce Schneier
// Revised code--3/20/94
// Converted to C++ class 5/96, Jim Conger
//
// modify H.Shirouzu 07/2002 (add change_order(), CBC mode)
// modify H.Shirouzu 01/2004 (add PKCS#5 mode)

#define MAXKEYBYTES		56		// 448 bits max
#define NPASS			16		// SBox passes

#define DWORD	unsigned long
#define WORD	unsigned short
#define BYTE	unsigned char

#define BF_ECB		0x0001
#define BF_CBC		0x0002
//#define BF_OFB		0x0004
//#define BF_CFB		0x0008
//#define BF_PKCS2	0x0010
#define BF_PKCS5	0x0020
#define BF_BLKSIZE	8

#ifndef ALIGN_SIZE
#define ALIGN_SIZE(size, align_size) ((((size) + (align_size) -1) / (align_size)) * (align_size))
#endif

class CBlowFish
{
private:
	DWORD	*PArray;
	DWORD	(*SBoxes)[256];
	void	Blowfish_encipher(DWORD *xl, DWORD *xr);
	void	Blowfish_decipher(DWORD *xl, DWORD *xr);

public:
			CBlowFish(const BYTE *key=NULL, int keybytes=0);
			~CBlowFish();
	void	Initialize(const BYTE *key, int keybytes);
	DWORD	GetOutputLength(DWORD lInputLong, int mode);
	DWORD	Encrypt(const BYTE *pInput, BYTE *pOutput, DWORD lSize, int mode=BF_CBC|BF_PKCS5, BYTE * IV=0);
	DWORD	Decrypt(const BYTE *pInput, BYTE *pOutput, DWORD lSize, int mode=BF_CBC|BF_PKCS5, BYTE *IV=0);
};

// choose a byte order for your hardware
#define ORDER_DCBA	// chosing Intel in this case

union aword {
	DWORD	dword;
	BYTE	byte[4];
	struct {
#ifdef ORDER_ABCD  	// ABCD - big endian - motorola
		BYTE byte0;
		BYTE byte1;
		BYTE byte2;
		BYTE byte3;
#endif
#ifdef ORDER_DCBA  	// DCBA - little endian - intel
		BYTE byte3;
		BYTE byte2;
		BYTE byte1;
		BYTE byte0;
#endif
#ifdef ORDER_BADC  	// BADC - vax
		BYTE byte1;
		BYTE byte0;
		BYTE byte3;
		BYTE byte2;
#endif
	} w;
};


