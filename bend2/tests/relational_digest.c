#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static const uint32_t relational_md5_k[64] = {0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x2441453u,0xd8a1e681u,0xe7d3fbc8u,0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x4881d05u,0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u};
static const unsigned relational_md5_s[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
static void relational_md5_block(uint32_t h[4], const unsigned char bytes[64]) {
  uint32_t m[16];
  for (unsigned i=0; i<16; ++i) m[i]=(uint32_t)bytes[4*i] | ((uint32_t)bytes[4*i+1]<<8) | ((uint32_t)bytes[4*i+2]<<16) | ((uint32_t)bytes[4*i+3]<<24);
  uint32_t a=h[0], b=h[1], c=h[2], d=h[3];
  for (unsigned i=0; i<64; ++i) {
    uint32_t f; unsigned g;
    if (i<16) { f=(b&c)|(~b&d); g=i; }
    else if (i<32) { f=(d&b)|(~d&c); g=(5*i+1)%16; }
    else if (i<48) { f=b^c^d; g=(3*i+5)%16; }
    else { f=c^(b|~d); g=(7*i)%16; }
    uint32_t v=a+f+relational_md5_k[i]+m[g]; unsigned s=relational_md5_s[i];
    a=d; d=c; c=b; b+=(v<<s)|(v>>(32-s));
  }
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;
}
Term relational_digest_run(Env e, Term *f, IoWork *w) {
  w->code=0;
  uint64_t length=0; unsigned char *text=(unsigned char *)io_cstr(e,f[0],&length);
  uint32_t limit=(uint32_t)f[1];
  if (limit && length>limit) length=limit;
  uint32_t h[4]={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u};
  uint64_t used=0;
  while (length-used>=64) { relational_md5_block(h,text+used);used+=64; }
  unsigned char tail[128]={0};size_t rem=(size_t)(length-used);
  memcpy(tail,text+used,rem);tail[rem]=0x80;
  size_t total=rem<56?64:128;uint64_t bits=length*8;
  for (unsigned i=0;i<8;++i) tail[total-8+i]=(unsigned char)(bits>>(8*i));
  relational_md5_block(h,tail);if(total==128)relational_md5_block(h,tail+64);
  char hex[32];const char *digits="0123456789abcdef";
  for(unsigned i=0;i<16;++i){unsigned char byte=(unsigned char)(h[i/4]>>(8*(i%4)));hex[2*i]=digits[byte>>4];hex[2*i+1]=digits[byte&15];}
  free(text);return io_str(e,hex,32);
}
static void __attribute__((constructor)) relational_digest_use(void) { io_eff(CID_RELATIONAL_DIGEST,relational_digest_run,0); }
