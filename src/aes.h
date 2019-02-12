/* append by H.Shirouzu */

#ifndef AES_H
#define AES_H

#define AES_BLOCK_SIZE	16
#define AES_MAXNR		14

typedef unsigned char	u8;	
typedef unsigned short	u16;	
typedef unsigned int	u32;
typedef unsigned _int64 u64;

#ifndef ALIGN_SIZE
#define ALIGN_SIZE(size, align_size) ((((size) + (align_size) -1) / (align_size)) * (align_size))
#endif

typedef struct {
	u32 ek[ 4*(AES_MAXNR+1) ]; 
	u32 dk[ 4*(AES_MAXNR+1) ];
	int rounds;
} aes_state;

class AES {
public:
	enum PaddingMode { NOPAD, PKCS5 };

private:
	aes_state	as;
	u8			iv[AES_BLOCK_SIZE];		// nonce in CTR mode
	u8			xor[AES_BLOCK_SIZE];	// used CTR mode only
	u64			offset;					// used CTR mode only

public:
	AES(void) { as.rounds = 0; }
	AES(const u8 *key, int key_len, const u8 *_iv=NULL);
	void Init(const u8 *key, int key_len, const u8 *_iv=NULL);
	void InitIv(const u8 *_iv=NULL, size_t len=AES_BLOCK_SIZE);
	size_t EncryptCBC(const u8 *in, u8 *out, size_t size, PaddingMode _pm=PKCS5);
	size_t EncryptCTR(const u8 *in, u8 *out, size_t size);
	size_t DecryptCBC(const u8 *in, u8 *out, size_t size, PaddingMode _pm=PKCS5);
	size_t DecryptCTR(const u8 *in, u8 *out, size_t size);
	size_t GetCBCLength(size_t len, PaddingMode pm);
	bool IsKeySet() { return as.rounds != 0; }
};

#endif

