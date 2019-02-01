/*************************************************************************
 *
 *  Managed string library
 *
 *  Copyright (c) 2011, 2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
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
