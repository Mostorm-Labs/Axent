#ifndef	__INCmd5h
#define	__INCmd5h

#ifdef __cplusplus
extern "C"{
#endif
#if defined(__APPLE__)
    typedef unsigned long size_t;
#endif
#define A_UINT32 	unsigned int
	struct MD5Context {
        A_UINT32 buf[4];
        A_UINT32 bits[2];
        unsigned char in[64];
	};

    void MD5Init(struct MD5Context *ctx);
    void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned int len);
    void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
    void MD5Transform(A_UINT32 buf[4], A_UINT32 in[16]);


	void md5_calc(unsigned char *output, unsigned char *input, unsigned int inlen);
	void md52str(unsigned char*digest, unsigned char* string);
	void str2md5(const char* message, int len, char output[16]);


	typedef struct MD5Context MD5_CTX;

#ifdef __cplusplus
}
#endif

#endif /* __INCmd5h */
