#ifndef _MR_SQL_SHA1_H
#define _MR_SQL_SHA1_H

#include "mr_packets.h"

namespace mrutils {
namespace mysql {

    static const int SCRAMBLE_LENGTH = 20;
    static const int SCRAMBLE_LENGTH_323 = 8;
    static const int SCRAMBLED_PASSWORD_CHAR_LENGTH  = (SCRAMBLE_LENGTH*2+1);
    static const int SCRAMBLED_PASSWORD_CHAR_LENGTH_323  = (SCRAMBLE_LENGTH_323*2);
    static const char PVERSION41_CHAR = '*';

enum sha_result_codes
{
  SHA_SUCCESS = 0,
  SHA_NULL,		/* Null pointer parameter */
  SHA_INPUT_TOO_LONG,	/* input data too long */
  SHA_STATE_ERROR	/* called Input after Result */
};

#define SHA1_HASH_SIZE 20 /* Hash size in bytes */

/*
  This structure will hold context information for the SHA-1
  hashing operation
*/

typedef struct SHA1_CONTEXT
{
  uint64_t  Length;		/* Message length in bits      */
  uint32_t Intermediate_Hash[SHA1_HASH_SIZE/4]; /* Message Digest  */
  int Computed;			/* Is the digest computed?	   */
  int Corrupted;		/* Is the message digest corrupted? */
  int16_t Message_Block_Index;	/* Index into message block array   */
  uint8_t Message_Block[64];	/* 512-bit message blocks      */
} SHA1_CONTEXT;

/*
  Function Prototypes
*/

int mysql_sha1_reset(SHA1_CONTEXT*);
int mysql_sha1_input(SHA1_CONTEXT*, const uint8_t *, unsigned int);
int mysql_sha1_result(SHA1_CONTEXT* , uint8_t Message_Digest[SHA1_HASH_SIZE]);

}}

#endif
