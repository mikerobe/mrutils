#ifndef _MR_SQL_CRYPT_H
#define _MR_SQL_CRYPT_H

#include "mysql_sha1.h"

namespace mrutils {
namespace mysql {


static const char _dig_vec_upper[] =
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const char _dig_vec_lower[] =
  "0123456789abcdefghijklmnopqrstuvwxyz";

static inline uint8_t char_val(uint8_t X)
{
  return (unsigned) (X >= '0' && X <= '9' ? X-'0' :
      X >= 'A' && X <= 'Z' ? X-'A'+10 : X-'a'+10);
}

/*
    Convert given octet sequence to asciiz string of hex characters;
    str..str+len and 'to' may not overlap.
  SYNOPSIS
    octet2hex()
    buf       OUT output buffer. Must be at least 2*len+1 bytes
    str, len  IN  the beginning and the length of the input string

  RETURN
    buf+len*2
*/

static inline char *octet2hex(char *to, const char *str, unsigned len)
{
  const char *str_end= str + len; 
  for (; str != str_end; ++str)
  {
    *to++= _dig_vec_upper[((unsigned char) *str) >> 4];
    *to++= _dig_vec_upper[((unsigned char) *str) & 0x0F];
  }
  *to= '\0';
  return to;
}

/*
    Convert given asciiz string of hex (0..9 a..f) characters to octet
    sequence.
  SYNOPSIS
    hex2octet()
    to        OUT buffer to place result; must be at least len/2 bytes
    str, len  IN  begin, length for character string; str and to may not
                  overlap; len % 2 == 0
*/ 

static inline void
hex2octet(uint8_t *to, const char *str, unsigned len)
{
  const char *str_end= str + len;
  while (str < str_end)
  {
    register char tmp= char_val(*str++);
    *to++= (tmp << 4) | char_val(*str++);
  }
}

/*
    Encrypt/Decrypt function used for password encryption in authentication.
    Simple XOR is used here but it is OK as we crypt random strings. Note,
    that XOR(s1, XOR(s1, s2)) == s2, XOR(s1, s2) == XOR(s2, s1)
  SYNOPSIS
    my_crypt()
    to      OUT buffer to hold crypted string; must be at least len bytes
                long; to and s1 (or s2) may be the same.
    s1, s2  IN  input strings (of equal length)
    len     IN  length of s1 and s2
*/

static inline void
my_crypt(char *to, const unsigned char *s1, const unsigned char *s2, unsigned len)
{
  const uint8_t *s1_end= s1 + len;
  while (s1 < s1_end)
    *to++= *s1++ ^ *s2++;
}


/*
    MySQL 4.1.1 password hashing: SHA conversion (see RFC 2289, 3174) twice
    applied to the password string, and then produced octet sequence is
    converted to hex string.
    The result of this function is used as return value from PASSWORD() and
    is stored in the database.
  SYNOPSIS
    my_make_scrambled_password()
    buf       OUT buffer of size 2*SHA1_HASH_SIZE + 2 to store hex string
    password  IN  password string
    pass_len  IN  length of password string
*/

inline void my_make_scrambled_password(char *to, const char *password,
                                size_t pass_len)
{
  SHA1_CONTEXT sha1_context;
  uint8_t hash_stage2[SHA1_HASH_SIZE];

  mysql_sha1_reset(&sha1_context);
  /* stage 1: hash password */
  mysql_sha1_input(&sha1_context, (uint8_t *) password, (unsigned) pass_len);
  mysql_sha1_result(&sha1_context, (uint8_t *) to);
  /* stage 2: hash stage1 output */
  mysql_sha1_reset(&sha1_context);
  mysql_sha1_input(&sha1_context, (uint8_t *) to, SHA1_HASH_SIZE);
  /* separate buffer is used to pass 'to' in octet2hex */
  mysql_sha1_result(&sha1_context, hash_stage2);
  /* convert hash_stage2 to hex string */
  *to++= PVERSION41_CHAR;
  octet2hex(to, (const char*) hash_stage2, SHA1_HASH_SIZE);
}

/*
    Produce an obscure octet sequence from password and random
    string, recieved from the server. This sequence corresponds to the
    password, but password can not be easily restored from it. The sequence
    is then sent to the server for validation. Trailing zero is not stored
    in the buf as it is not needed.
    This function is used by client to create authenticated reply to the
    server's greeting.
  SYNOPSIS
    scramble()
    buf       OUT store scrambled string here. The buf must be at least 
                  SHA1_HASH_SIZE bytes long. 
    message   IN  random message, must be exactly SCRAMBLE_LENGTH long and 
                  NULL-terminated.
    password  IN  users' password 
*/

inline void
scramble(char *to, const char *message, const char *password)
{
  SHA1_CONTEXT sha1_context;
  uint8_t hash_stage1[SHA1_HASH_SIZE];
  uint8_t hash_stage2[SHA1_HASH_SIZE];

  mysql_sha1_reset(&sha1_context);
  /* stage 1: hash password */
  mysql_sha1_input(&sha1_context, (uint8_t *) password, (unsigned) strlen(password));
  mysql_sha1_result(&sha1_context, hash_stage1);


  /* stage 2: hash stage 1; note that hash_stage2 is stored in the database */
  mysql_sha1_reset(&sha1_context);
  mysql_sha1_input(&sha1_context, hash_stage1, SHA1_HASH_SIZE);
  mysql_sha1_result(&sha1_context, hash_stage2);
  /* create crypt string as sha1(message, hash_stage2) */;
  mysql_sha1_reset(&sha1_context);
  mysql_sha1_input(&sha1_context, (const uint8_t *) message, SCRAMBLE_LENGTH);
  mysql_sha1_input(&sha1_context, hash_stage2, SHA1_HASH_SIZE);
  /* xor allows 'from' and 'to' overlap: lets take advantage of it */
  mysql_sha1_result(&sha1_context, (uint8_t *) to);
  my_crypt(to, (const unsigned char *) to, hash_stage1, SCRAMBLE_LENGTH);
}

}} // namespaces

#endif
