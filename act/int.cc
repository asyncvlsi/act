/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include <act/int.h>

BigInt::BigInt()
{
  sign = 0;
  len = 1;
  MALLOC (v, unsigned long, len);
  v[0] = 0;
}

BigInt::~BigInt()
{
  if (len > 0) {
    FREE (v);
  }
}


BigInt::BigInt (BigInt &b)
{
  sign = b.sign;
  len = b.len;
  if (len > 0) {
    MALLOC (v, unsigned long, b.len);
    for (int i=0; i < len; i++) {
      v[i] = b.v[i];
    }
  }
  else {
    v = NULL;
  }
}

BigInt::BigInt (BigInt &&b)
{
  sign = b.sign;
  len = b.len;
  v = b.v;
}

BigInt BigInt::operator-()
{
  BigInt b(*this);
  b.sign = 1 - b.sign;
  return b;
}

static int _allzeros (unsigned long *x, int len)
{
  for (int i=0; i < len; i++) {
    if (x[i] != 0) return 0;
  }
  return 1;
}

int BigInt::operator==(BigInt &b)
{
 int za, zb;
 int i;
 
 za = _allzeros (v, len);
 zb = _allzeros (b.v, len);
 if (za && zb) return 1;
 if (za || zb) return 0;
 if (sign != b.sign) return 0;

 if (len > b.len) {
   for (i=len-1; i >= b.len; i--) {
     if (v[i]) {
       return 0;
     }
   }
 }
 else {
   for (i=b.len-1; i >= len; i--) {
     if (b.v[i]) {
       return 0;
     }
   }
 }
 for (; i >= 0; i--) {
   if (v[i] != b.v[i]) {
     return 0;
   }
 }
 return 1;
}


int BigInt::operator<(BigInt &b)
{
  int za, zb;
  int i;
  int res;

  za = _allzeros (v, len);
  zb = _allzeros (b.v, b.len);

  /* zero cases */
  if (za && zb) {
    return 0;
  }
  if (za) {
    return 1 - b.sign;
  }
  if (zb) {
    return sign;
  }

  /* different signs */
  if (sign != b.sign) {
    return sign;
  }

  res = -1;
  /* now actual unsigned compare */
  if (len > b.len) {
    for (i=len-1; i >= b.len; i--) {
      if (v[i]) {
	res = 1;  // I am larger
	break;
      }
    }
    /* either res is set, or i = b.len - 1 */
  }
  else {
    for (i=b.len-1; i >= len; i--) {
      if (b.v[i]) {
	res = 0;
	break;
      }
    }
    /* either res is set, or i = len - 1 */
  }
  if (res == -1) {
    for (; i >= 0; i--) {
      if (v[i] > b.v[i]) {
	res = 1;
	break;
      }
      else if (v[i] < b.v[i]) {
	res = 0;
	break;
      }
    }
  }
  if ((res == 0 && sign == 0) || (res == 1 && sign == 1)) {
    return 1;
  }
  else {
    return 0;
  }	  
}

int BigInt::operator>(BigInt &b)
{
 return (b < *this);
}

 
BigInt & BigInt::operator+(BigInt &b)
{
  int i;
  int c = 0;
  unsigned long x, y, res;
  /* 0 = msb */

  if (b.len > len) {
    REALLOC (v, unsigned long, b.len);
    for (i=len; i < b.len; i++) {
      v[i] = 0;
    }
    len = b.len;
  }
  
  for (i=0; i < len; i++) {
    x = v[i];
    if (i < b.len) {
      y = b.v[i];
    }
    else {
      y = 0;
    }
    res = x + y + c;
    if (res < x || res < y) {
      c = 1;
    }
    else {
      c = 0;
    }
    v[i] = res;
  }
  if (c) {
    len++;
    REALLOC (v, unsigned long, len);
    v[len-1] = c;
  }
  return *this;
}


