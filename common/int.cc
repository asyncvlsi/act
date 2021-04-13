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
#include "int.h"

#define UNIT_SZ 

/*------------------------------------------------------------------------
 *
 *  BigInt::BigInt --
 *
 *   Constructor. No specified width makes it dynamic. Specified width
 *   makes it static.
 *
 *------------------------------------------------------------------------
 */
BigInt::BigInt()
{
  len = 1;
  width = 1;
  MALLOC (v, unsigned long, len);
  v[0] = 0;
  isdynamic = 1;
  issigned = 1;
}

BigInt::BigInt(int w, int s)
{
  len = 0;
  width = w;
  do {
    len++;
    w = w - BIGINT_BITS_ONE;
  } while (w > 0);
  Assert (len > 0, "What?");
  MALLOC (v, unsigned long, len);
  for (int i=0; i < len; i++) {
    v[i] = 0;
  }
  isdynamic = 0;
  issigned = s;
}

BigInt::~BigInt()
{
  if (len > 0) {
    FREE (v);
  }
  v = NULL;
  len = 0;
}

BigInt BigInt::dynInt (int x)
{
  BigInt b;

  b.v[0] = x;
  b.width = 32;

  return b;
}

/*-- copy constructor --*/
BigInt::BigInt (BigInt &b)
{
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
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

/*-- move constructor --*/
BigInt::BigInt (BigInt &&b)
{
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  v = b.v;
  width = b.width;

  b.v = NULL;
  b.len = 0;
}


BigInt& BigInt::operator=(BigInt &b)
{
  FREE (v);
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  MALLOC (v, unsigned long, len);
  for (int i=0; i < len; i++) {
    v[i] = b.v[i];
  }
  return *this;
}

BigInt& BigInt::operator=(BigInt &&b)
{
  FREE (v);
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  v = b.v;

  b.v = NULL;
  b.len = 0;

  return *this;
}

/*------------------------------------------------------------------------
 *
 *  BigInt::expandSpace --
 *
 *   Expand space for bits by the specified # of bits. Does not change
 *   bitwidth specifier, but changes array and length fields.
 *
 *------------------------------------------------------------------------
 */
void BigInt::expandSpace (int amt)
{
  if (amt + width <= len*BIGINT_BITS_ONE) {
    return;
  }
  int x = (amt + width + BIGINT_BITS_ONE-1)/BIGINT_BITS_ONE;
  Assert (x > len, "What?");
  
  REALLOC (v, unsigned long, x);
  for (; len < x; len++) {
    v[len] = 0;
  }
}

BigInt BigInt::operator-()
{
  int i;
  BigInt b(*this);
  int c = 0;
  int sa = isNegative();

  for (int i=0; i < len; i++) {
    b.v[i] = ~b.v[i];
  }

  c = 1;
  for (int i=0; c == 1 && (i < len); i++) {
    int msb = (b.v[i] >> (BIGINT_BITS_ONE-1)) & 0x1;
    b.v[i]++;
    if (((b.v[i] >> (BIGINT_BITS_ONE-1)) & 0x1) != msb) {
      c = 1;
    }
    else {
      c = 0;
    }
  }
  if (c && isdynamic) {
    b.expandSpace (1);
    b.v[len-1] = 0;
    if (sa) {
      b.v[len-1] = ~b.v[len-1];
    }
    b.width++;
  }
  return b;
}

static int _allzeros (unsigned long *x, int len)
{
  for (int i=0; i < len; i++) {
    if (x[i] != 0) return 0;
  }
  return 1;
}

/*------------------------------------------------------------------------
 *
 *  BigInt::isNegative --
 *
 *   Returns 1 if the number is negative
 *
 *------------------------------------------------------------------------
 */
int BigInt::isNegative ()
{
  /* residual width */
  if (!issigned) {
    return 0;
  }
 
  int res = width - (len-1)*BIGINT_BITS_ONE;
  
  return (v[len-1] >> (res-1)) & 0x1;
}

/*------------------------------------------------------------------------
 *
 *  BigInt::signExtend --
 *
 *   Sign extend it
 *
 *------------------------------------------------------------------------
 */
void BigInt::signExtend ()
{
  int res = width - (len-1)*BIGINT_BITS_ONE;
  int sa = (v[len-1] >> (res-1)) & 0x1;

  if (!issigned) {
    sa = 0;
  }

  unsigned long x = 0;
  x = ~x;
  x = x << res;
  if (sa) {
    /* negative */
    v[len-1] |= x;
  }
  else {
    v[len-1] &= ~x;
  }
}

/*------------------------------------------------------------------------
 *
 *  Comparison operators
 *
 *------------------------------------------------------------------------
 */
int BigInt::operator==(BigInt &b)
{
 int za, zb;
 int sa, sb;
 int i;
 
 za = _allzeros (v, len);
 zb = _allzeros (b.v, len);
 if (za && zb) return 1;
 if (za || zb) return 0;

 sa = isNegative();
 sb = b.isNegative();

 if (len > b.len) {
   for (i=len-1; i >= b.len; i--) {
     if (sa == 0) {
       /* positive */
       if (v[i]) {
	 return 0;
       }
     }
     else {
       /* negative */
       if (~v[i]) {
	 return 0;
       }
     }
   }
 }
 else {
   for (i=b.len-1; i >= len; i--) {
     if (sa) {
       if (b.v[i]) {
	 return 0;
       }
     }
     else {
       if (~b.v[i]) {
	 return 0;
       }
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
  int sa, sb;

  za = _allzeros (v, len);
  zb = _allzeros (b.v, b.len);

  /* zero cases */
  if (za && zb) {
    return 0;
  }

  sa = isNegative();
  sb = b.isNegative();

  if (za) {
    return sb ? 0 : 1;
  }
  if (zb) {
    return sa ? 1 : 0;
  }

  /* different signs */
  if (sa != sb) {
    return sa;
  }

  res = -1;
    
  /* now actual unsigned compare */
  if (len > b.len) {
    for (i=len-1; i >= b.len; i--) {
      if (sa == 0) {
	if (v[i]) {
	  res = 1;  // I am larger
	  break;
	}
      }
      else {
	if (~v[i]) {
	  res = 0;
	  break;
	}
      }
    }
    /* either res is set, or i = b.len - 1 */
  }
  else {
    for (i=b.len-1; i >= len; i--) {
      if (sa == 0) {
	if (b.v[i]) {
	  res = 0;
	  break;
	}
      }
      else {
	if (~b.v[i]) {
	  res = 1;
	  break;
	}
      }
    }
    /* either res is set, or i = len - 1 */
  }
  if (res == -1) {
    for (; i >= 0; i--) {
      if (sa == 0) {
	if (v[i] > b.v[i]) {
	  res = 1;
	  break;
	}
	else if (v[i] < b.v[i]) {
	  res = 0;
	  break;
	}
      }
      else {
	if (v[i] > b.v[i]) {
	  res = 0;
	  break;
	}
	else if (v[i] < b.v[i]) {
	  res = 1;
	  break;
	}
      }
    }
  }
  return res == 0 ? 1 : 0;
}

int BigInt::operator>(BigInt &b)
{
 return (b < *this);
}

int BigInt::operator<=(BigInt &b)
{
 return ((*this) < b) || ((*this) == b);
}

int BigInt::operator>=(BigInt &b)
{
 return ((*this) > b) || ((*this) == b);
}


/*------------------------------------------------------------------------
 *
 *   Arithmetic
 *
 *------------------------------------------------------------------------
 */
BigInt &BigInt::operator+(BigInt &b)
{
  int i;
  int c;
  int sa, sb;
  unsigned long x;

  sb = b.isNegative();
  sa = isNegative();

  if (len < b.len && isdynamic) {
    /* allocate more space! */
    REALLOC (v, unsigned long, b.len);
    for (i=len; i < b.len; i++) {
      v[i] = 0;
      if (sa == 1) {
	v[i] = ~v[i];
      }
    }
    len = b.len;
    width = b.width;
  }

  c = 0;
  for (i=0; i < len; i++) {
    int msb = (v[i] >> (BIGINT_BITS_ONE-1)) & 0x1;
    int xmsb, vmsb;
    if (i < b.len) {
      x = b.v[i];
    }
    else {
      x = 0;
      if (sb) {
	x = ~x;
      }
    }
    xmsb = (x >> (BIGINT_BITS_ONE-1)) & 0x1;
    v[i] = v[i] + x + c;
    if (xmsb == 1 && msb == 1) {
      c = 1;
    }
    else if (xmsb == 0 && msb == 0) {
      c = 0;
    }
    else {
      vmsb = (v[i] >> (BIGINT_BITS_ONE-1)) & 0x1;
      if (vmsb == 0) {
	c = 1;
      }
      else {
	c = 0;
      }
    }
  }

  if (isdynamic) {
    if (width == len*BIGINT_BITS_ONE) {
      expandSpace (len*BIGINT_BITS_ONE+1 - width);
      width = len*BIGINT_BITS_ONE+1;
      v[len-1] = c;
      signExtend ();
    }
    else {
      width++;
      signExtend ();
    }
    width = (len-1)*BIGINT_BITS_ONE;
    for (int i=BIGINT_BITS_ONE-1; i >= 0; i--) {
      if (((v[len-1] >> i) & 0x1) != sa) {
	Assert (i != BIGINT_BITS_ONE-1, "What?");
	width = width + i + 2;
	break;
      }
    }
  }
  else {
    signExtend ();
  }
  return *this;
}

BigInt &BigInt::operator-(BigInt &b)
{
 BigInt nb = (-b);
 return (*this) + nb;
}


BigInt &BigInt::operator*(BigInt &b)
{
  warning ("Need mult!");
  return (*this);
}

BigInt &BigInt::operator/(BigInt &b)
{
  warning ("Need div!");
  return (*this);
}

BigInt &BigInt::operator%(BigInt &b)
{
  warning ("Need mod!");
  return (*this);
}



/*------------------------------------------------------------------------
 *
 *  BigInt::zeroExtend --
 *
 *   Extend to specified width, if necessary
 *
 *------------------------------------------------------------------------
 */
void BigInt::zeroClear ()
{
  int res = width - (len-1)*BIGINT_BITS_ONE;
  unsigned long x;
  x = 0;
  x = ~x;
  x = x << res;
  v[len-1] = v[len-1] & ~x;
}

void BigInt::zeroExtend (int w)
{
  zeroClear ();
  if (w > width) {
    expandSpace (w - width);
  }
}

/*------------------------------------------------------------------------
 *
 *  Logical operations: widths are zero-extended
 *
 *------------------------------------------------------------------------
 */
#define LOGICAL_SETUP				\
  BigInt x = b;					\
  zeroExtend (b.width);				\
  x.zeroExtend (width)
  
BigInt &BigInt::operator&(BigInt &b)
{
  LOGICAL_SETUP;

  for (int i=0; i < len; i++) {
    v[i] = v[i] & x.v[i];
  }

  issigned = 0;

  return (*this);
}

BigInt &BigInt::operator|(BigInt &b)
{
  LOGICAL_SETUP;
  
  for (int i=0; i < len; i++) {
    v[i] = v[i] | x.v[i];
  }

  issigned = 0;

  return (*this);
}

BigInt &BigInt::operator^(BigInt &b)
{
  LOGICAL_SETUP;
  
  for (int i=0; i < len; i++) {
    v[i] = v[i] ^ x.v[i];
  }

  issigned = 0;
  
  return (*this);
}

BigInt &BigInt::operator~()
{
  for (int i=0; i < len; i++) {
    v[i] = ~v[i];
  }
  zeroClear ();

  issigned = 0;
  
  return (*this);
}

BigInt &BigInt::operator<<(unsigned long x)
{
  int stride = x / BIGINT_BITS_ONE;

  if (x == 0) return *this;

  x = x % BIGINT_BITS_ONE;

  if (isdynamic) {
    expandSpace (x);
    width += x;
  }

  for (int i=len-1-stride; i >= 0; i--) {
    v[i+stride] = (v[i] << x);
    if (i > 0) {
      v[i+stride] |= (v[i-1] >> (BIGINT_BITS_ONE - x));
    }
  }
  for (int i=0; i < stride; i++) {
    v[i] = 0;
  }

  signExtend ();
  
  return (*this);
}

BigInt &BigInt::operator>>(unsigned long x)
{
  if (x == 0) return *this;

  if (x >= width) {
    int sa = isNegative ();
    if (isdynamic) {
      FREE (v);
      len = 1;
      width = 1;
      MALLOC (v, unsigned long, 1);
      v[0] = 0;
      if (sa) {
	v[0] = ~v[0];
      }
    }
    else {
      for (int i=0; i < len; i++) {
	v[i] = 0;
	if (sa) {
	  v[i] = ~v[i];
	}
      }
    }
    return *this;
  }

  int stride = x / BIGINT_BITS_ONE;
  unsigned long mask = 0;

  if (isdynamic) {
    width -= x;
  }

  x = x % BIGINT_BITS_ONE;

  mask = ~mask;
  mask = mask >> (BIGINT_BITS_ONE-x);

  for (int i=0; i < len-stride; i++) {
    v[i] = (v[i+stride] >> x);
    if (i < len) {
      v[i] |= (v[i+1] & mask);
    }
  }

  if (stride > 0) {
    REALLOC (v, unsigned long, len-stride);
    len -= stride;
  }

  signExtend ();

  return (*this);
}

BigInt &BigInt::operator<<(BigInt &b)
{
  Assert (b.isOneInt(), "Shift amounts have to be small enough to fit into a single intger");
  Assert (!b.isNegative(), "Non-negative shift amounts only");
  
  return (*this) << b.v[0];
}

BigInt &BigInt::operator>>(BigInt &b)
{
  Assert (b.isOneInt(), "Shift amounts have to be small enough to fit into a single intger");
  Assert (!b.isNegative(), "Non-negative shift amounts only");

  return (*this) >> b.v[0];
}


void BigInt::Print (FILE *fp)
{
 fprintf (fp, "{w=%d,dyn=%d,sgn=%d}0x", width, isdynamic, issigned);
 for (int i=len-1; i >= 0; i--) {
   fprintf (fp, "%lx", v[i]);
 }
}

