/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __COMMON_STRING_H__
#define __COMMON_STRING_H__

#ifdef __cplusplus
extern "C" {
#endif


/**
 *  Reference-counted string library
 *
 */


typedef struct _my_string_ {
  struct _my_string_ *next;
  short ref;
  char s[1];
} mstring_t;

/**
 * Create a string reference from a char * (reference-counted)
 * 
 * \param s is the string to be converted into a mstring_t *
 */
mstring_t *string_create (const char *s);


/**
 * Duplicate the string
 *
 * \param s is the string to be duplicated
 */
mstring_t *string_dup (mstring_t *s);

/**
 * Release a reference to a string
 *
 * \param s is the string to be free'd
 */
void string_free (mstring_t *s);

/**
 * Return the char * corresponding to the string
 *
 * \param s is the string reference
 */
const char *string_char (mstring_t *s);

/**
 * Standard interface that caches a traditional string
 * \param s is the original string
 */
#define string_cache(s) string_char(string_create (s))

#ifdef __cplusplus
}
#endif

#endif /* __COMMON_STRING_H__ */
