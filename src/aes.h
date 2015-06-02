/* append by H.Shirouzu */

#ifndef AES_H
#define AES_H

#define AES_BLOCK_SIZE	16
#define AES_MAXNR		14

typedef unsigned char	u8;	
typedef unsigned short	u16;	
typedef unsigned int	u32;

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
	enum CypherMode { ECB, CBC };
	enum PaddingMode { NONE, PKCS5 };

private:
	aes_state	as;
	CypherMode	cm;
	PaddingMode	pm;
	u8			iv[AES_BLOCK_SIZE];

public:
	AES(void) {}
	AES(u8 *key, int key_len, PaddingMode _pm=PKCS5, CypherMode _cm=CBC);
	void Init(u8 *key, int key_len, PaddingMode _pm=PKCS5, CypherMode _cm=CBC);
	int Encrypt(u8 *in, u8 *out, int size, u8 *_iv=NULL);
	int Decrypt(u8 *in, u8 *out, int size, u8 *_iv=NULL);
	int GetLength(int len);
};

#endif

