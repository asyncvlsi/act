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
 *   Constructor. Destructor. Assignment.
 *
 *------------------------------------------------------------------------
 */
BigInt::BigInt(int w, int s, int d)
{
  len = 0;
  width = w;
  do {
    len++;
    w = w - BIGINT_BITS_ONE;
  } while (w > 0);
  Assert (len > 0, "What?");
  if (len >= 2) {
    MALLOC (u.v, UNIT_TYPE, len);
    for (int i=0; i < len; i++) {
      u.v[i] = 0;
    }
  } else {
    u.value = 0;
  }
  isdynamic = d;
  issigned = s;
}

/*-- copy constructor --*/
BigInt::BigInt (BigInt &b)
{
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  if (len >= 2) {
    MALLOC (u.v, UNIT_TYPE, b.len);
    for (int i=0; i < len; i++) {
      u.v[i] = b.u.v[i];
    }
  } else {
    u.value = b.u.value;
  }
}

/*-- move constructor --*/
BigInt::BigInt (BigInt &&b)
{
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  if (len >= 2) {
    u.v = b.u.v;
  } else {
    u.value = b.u.value;
  }
  width = b.width;
  if (len >= 2) {
    b.u.v = NULL;
  } else {
    b.u.value = 0;
  }
  b.len = 0;
}


BigInt& BigInt::operator=(const BigInt &b)
{
  if (&b == this) { return *this; }
  if (len >= 2) {
    FREE (u.v);
  }
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  if (len >= 2) {
    MALLOC (u.v, UNIT_TYPE, len);
    for (int i=0; i < len; i++) {
      u.v[i] = b.u.v[i];
    }
  } else {
    u.value = b.u.value;
  }
  return *this;
}

BigInt& BigInt::operator=(BigInt &&b)
{
  if (&b == this) { return *this; }
  if (len >= 2) {
    FREE (u.v);
  }
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  if (len >= 2) {
    u.v = b.u.v;
  } else {
    u.value = b.u.value;
  }

  b.u.v = NULL;
  b.len = 0;

  return *this;
}

BigInt& BigInt::operator=(const UNIT_TYPE &b)
{
  if (len >= 2) {
    FREE (u.v);
  }
  isdynamic = 0;
  issigned = 0;
  len = 1;
  width = 8*sizeof(UNIT_TYPE);
  u.value = b;
  return *this;
}

BigInt& BigInt::operator=(const std::string &b)
{
  if (len >= 2) {
    FREE(u.v);
  }

  int word_cnt = 0;
  int word_len = BIGINT_BITS_ONE/4;
  int word_num = 0;
  std::string word_str = "";
  std::string filler = "";

  isdynamic = 0;
  issigned = 0;

  if ((4*(b.size()-2)) % BIGINT_BITS_ONE == 0) {
    len = 4*(b.size()-2)/BIGINT_BITS_ONE;
    if (len >= 2) {
      MALLOC(u.v, UNIT_TYPE, len);
    }
  } else {
    len = 1 + 4*(b.size()-2)/BIGINT_BITS_ONE;
    if (len >= 2) {
      MALLOC(u.v, UNIT_TYPE, len);
    }
  }

  std::string hex_test = b.substr(0,2);
  if (hex_test != "0x") {
    fatal_error("unknown input format");
  }

  width = (b.size()-2) * 4;

  for (auto it = b.rbegin(); it != b.rend()-2; ++it) {
    if (word_cnt == word_len-1){
      word_str = *it + word_str;
      _setVal (word_num, strtoul(word_str.c_str(),0,16));
      word_str = "";
      word_cnt = 0;
      word_num++;
    } else {
      word_str = *it + word_str;
      if (it == b.rend()-3) {
        filler = "0";
        for (int k = 0; k < ((b.size()-2) % (BIGINT_BITS_ONE/4)); k++) {
          word_str = filler + word_str;
        }
        _setVal (word_num, strtoul(word_str.c_str(),0,16));
        break;
      }
      word_cnt++;
    }
  }

  return *this;
}



#ifdef BIGINT_TEST
void BigInt::SetV (int n, UNIT_TYPE nv)
{
  if (n > len) {
    expandSpace(sizeof(UNIT_TYPE));
  }
  _setVal (n, nv);
  if (isSigned()) {
    signExtend();
  }
}
#endif
/*------------------------------------------------------------------------
 *
 *   Group of methods to change bit width of the BigInt variable.
 *   setWidth - changes width to the specified value, reallocated memory
 *   expandSpace - reallocated more memory and does sign extension if needed
 *   squeezSpace - reallocates less memory and does sign extension if needed
 *
 *------------------------------------------------------------------------
 */
void BigInt::setWidth (unsigned int b)
{
  unsigned int tmp;
  if (width < b) {
    tmp = b - width;
    expandSpace(tmp);
  } else if (width > b) {
    tmp = width - b;
    squeezeSpace(tmp);
  }
  width = b;
}

void BigInt::expandSpace (int amt)
{
  if (amt + width <= len*BIGINT_BITS_ONE) {
    return;
  }
  int x = (amt + width + BIGINT_BITS_ONE-1)/BIGINT_BITS_ONE;
  Assert (x > len, "What?");
 
  int sa = 0;
  sa = isSigned() && isNegative();

  if (sa) {
    signExtend();
  }

  _adjlen (x);
  if (x > 1) {
    for (; len < x; len++) {
      u.v[len] = 0;
      if (sa) {
        u.v[len] = ~u.v[len];
      }
    }
  } else {
    len = x;
  }
}

void BigInt::squeezeSpace (int amt)
{
  unsigned int tmp;
  tmp = width - (len-1)*BIGINT_BITS_ONE;
  unsigned int x = 0;
  if (tmp <= amt) {
    x = (width - amt + BIGINT_BITS_ONE-1)/BIGINT_BITS_ONE;
    if (x < len) {
      _adjlen (x);
      len = x;
    }
  }
  width = width - amt;

  tmp = len*BIGINT_BITS_ONE - width;
  UNIT_TYPE res = 0;
  res = ~res;
  res = res >> tmp;
  if (len >= 2) {
    u.v[len-1] = u.v[len-1] & res;
  } else {
    u.value = u.value & res;
  }

  int sa = 0;
  sa = isSigned() && isNegative();

  if (sa) {
    signExtend();
  }
}

/*------------------------------------------------------------------------
 *
 *    Unary minus. Returns two's complement value
 *
 *------------------------------------------------------------------------
 */
BigInt BigInt::operator-() const
{
  BigInt b;
  int c = 1;
  int sa = isSigned() && isNegative();

  b = (*this);
  
  if (len >= 2) {
    for (int i=0; i < len; i++) {
      b.u.v[i] = ~b.u.v[i];
    }
  } else {
    b.u.value = ~b.u.value;
  }

  for (int i = 0; c == 1 && (i<len); i++) {
    if (len >= 2) {
      b.u.v[i] = b.u.v[i] + c;
      if (b.u.v[i] == 0) {
        c = 1;
      } else {
        c = 0;
        break;
      }
    } else {
      b.u.value = b.u.value + c;
      if (b.u.value == 0) {
        c = 1;
      } else {
        c = 0;
        break;
      }
    }
  }
  if (c && isDynamic()) {
    b.expandSpace (1);
    if (b.len >= 2) {
      b.u.v[len-1] = 0;
      if (sa) {
        b.u.v[len-1] = ~b.u.v[len-1];
      }
    } else {
      b.u.value = 0;
      if (sa) {
        b.u.value = ~b.u.value;
      }
    }
    b.width++;
  }
  return b;
}

/*------------------------------------------------------------------------
 *
 *   Returns 1 if the number is negative and 0 if it is non-negative
 *
 *------------------------------------------------------------------------
 */
int BigInt::isNegative () const
{
  /* residual width */
  if (!issigned) {
    return 0;
  }
  return (getVal (len-1) >> (BIGINT_BITS_ONE-1)) & 0x1;
}

/*------------------------------------------------------------------------
 *
 *   Sign extend the number if the number width is not 
 *   a multiple of UNIT_TYPE
 *
 *------------------------------------------------------------------------
 */
void BigInt::signExtend ()
{
  int res;
  int sa;

  if (width == len*BIGINT_BITS_ONE)  {
    return;
  } else {
    res = width - (len-1)*BIGINT_BITS_ONE;
    sa = (getVal (len-1) >> (res-1)) & 0x1;
  }

  if (!issigned) {
    sa = 0;
  }

  UNIT_TYPE x = 0;
  x = ~x;
  x = x << res;

  if (sa) {
    /* negative */
    _setVal (len-1, getVal (len-1)| x);
  } else {
    _setVal (len-1, getVal (len-1) & ~x);
  }
}

/*------------------------------------------------------------------------
 *
 *   Clear sign extension if any
 *
 *------------------------------------------------------------------------
 */
void BigInt::zeroClear ()
{
  int need = width % BIGINT_BITS_ONE;
  if (need != 0) {
    int res = width - (len-1)*BIGINT_BITS_ONE;
    if (res != 0) {
      UNIT_TYPE x;
      x = 0;
      x = ~x;
      x = x << res;
      x = ~x;
      _setVal (len-1, getVal (len-1) & x);
    }
  }
}

/*------------------------------------------------------------------------
 *
 *   A group of methods to update variable properties.
 *   toSigned - set sign flag and sign extend the number
 *   toUnsigned - reset sign flag and zero extend the number
 *   isSigned - returns sign flag
 *
 *   toDynamic - set dynamic flag
 *   toStatic - reset dynamic flag
 *   isDynamic - returns dynamic flag
 *
 *------------------------------------------------------------------------
 */
void BigInt::toSigned() 
{
  issigned = 1;
  signExtend();
}

void BigInt::toUnsigned() 
{
  issigned = 0;
  zeroClear();
}

/*------------------------------------------------------------------------
 *
 *  Comparison operators
 *
 *------------------------------------------------------------------------
 */
int BigInt::operator==(const BigInt &b) const
{
  if (isSigned() != b.isSigned()) {
    BigInt t1;
    t1 = (*this);
    BigInt t2;
    t2 = b;
    t1.toUnsigned();
    t2.toUnsigned();
    return (t1 == t2);
  }

  int za, zb;
  int sa, sb;
  int i;
 
  za = isZero();
  zb = b.isZero();

  if (za && zb) return 1;
  if (za || zb) return 0;
  
  sa = isSigned() && isNegative();
  sb = b.isSigned() && b.isNegative();

  if (len > b.len) {
    for (i=len-1; i >= b.len; i--) {
      UNIT_TYPE mask = 0;
      if (sb) {
        mask = ~mask;
        if (getVal (i) != mask) {
          return 0;
        }
      } else {
        if (getVal(i) != mask) {
          return 0;
        }
      }
    }
  } else if (b.len > len) {
    for (i=b.len-1; i >= len; i--) {
      UNIT_TYPE mask = 0;
      if (sa) {
        mask = ~mask;
        if (b.getVal (i) != mask) {
          return 0;
        }
      } else {
        if (b.getVal (i) != mask) {
          return 0;
        }
      }
    }
  }
  for (i = std::min(len, b.len)-1; i >= 0; i--) {
    if (getVal (i) != b.getVal (i)) {
      return 0;
    }
  }
  return 1;
}

#ifdef BIGINT_TEST
int BigInt::operator==(unsigned long b)
{
  unsigned long mask;
  mask = 0;
  mask = ~mask;
  mask = mask << BIGINT_BITS_ONE;
  mask = ~mask;

  UNIT_TYPE tmp;

  for (auto i = 0; i < len; i++) {
    tmp = (b  >> (i * BIGINT_BITS_ONE)) & mask;
    if (tmp != getVal (i)) {
      return 0;
    }
  }

  return 1;

}

int BigInt::operator==(long b) const
{
  unsigned long mask;
  mask = 0;
  mask = ~mask;
  mask = mask << BIGINT_BITS_ONE;
  mask = ~mask;

  UNIT_TYPE tmp;

  for (auto i = 0; i < len; i++) {
    tmp = (b  >> (i * BIGINT_BITS_ONE)) & mask;
    if (tmp != getVal (i)) {
      return 0;
    }
  }

  return 1;

}
#endif

int BigInt::operator!=(const BigInt &b) const
{
  return !(*this == b);
}

#ifdef BIGINT_TEST
int BigInt::operator!=(unsigned long b)
{
  return !(*this == b);
}

int BigInt::operator!=(long b) 
{
  return !(*this == b);
}
#endif

int BigInt::operator<(const BigInt &b) const
{
  if (isSigned() != b.isSigned()) {
    BigInt t1, t2;
    t1 = (*this);
    t2 = b;
    t1.toUnsigned();
    t2.toUnsigned();
    return (t1 < t2);
  }

  int za, zb;
  int i;
  int res;
  int sa, sb;

  za = isZero();
  zb = b.isZero();

  /* zero cases */
  if (za && zb) {
    return 0;
  }

  if (isSigned() && b.isSigned()) {
    sa = isNegative();
    sb = b.isNegative();
  } else {
    sa = 0;
    sb = 0;
  }

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
      if (!sa) {
        if (getVal (i)) {
          res = 1;  // I am larger
          break;
        }
      } else {
        if (~getVal (i)) {
          res = 0;
          break;
        }
      }
    }
    /* either res is set, or i = b.len - 1 */
  } else {
    for (i=b.len-1; i >= len; i--) {
      if (!sa) {
        if (b.getVal (i)) {
          res = 0;
          break;
        }
      } else {
        if (~b.getVal (i)) {
          res = 1;
          break;
        }
      }
    }
    /* either res is set, or i = len - 1 */
  }
  if (res == -1) {
    for (; i >= 0; i--) {
      UNIT_TYPE me, bv;
      me = getVal (i);
      bv = b.getVal (i);
      if (!sa) {
        if (me > bv) {
          res = 1;
          break;
        } else if (me < bv) {
          res = 0;
          break;
        }
      } else {
        if (me > bv) {
          res = 1;
          break;
        } else if (me < bv) {
          res = 0;
          break;
        }
      }
    }
  }
  return res == 0 ? 1 : 0;
}

int BigInt::operator>(const BigInt &b) const
{
  return (b < *this);
}

int BigInt::operator<=(const BigInt &b) const
{
  return ((*this) < b) || ((*this) == b);
}

int BigInt::operator>=(const BigInt &b) const
{
  return ((*this) > b) || ((*this) == b);
}


/*------------------------------------------------------------------------
 *
 *   Arithmetic
 *
 *------------------------------------------------------------------------
 */
void BigInt::_add (const BigInt &b, int cin)
{
  if (isSigned() != b.isSigned()) {
    issigned = 0;
    BigInt tmp;
    tmp = b;
    tmp.toUnsigned ();
    _add (tmp, cin);
    return;
  }

  int i;
  int c, nc;
  int sa, sb;

  sa = isSigned() & isNegative();
  sb = b.isSigned() & b.isNegative();

  BigInt b_ext;

  if (len < b.len) {
    if (isDynamic()) {
      _adjlen (b.len);
      for (i=len; i < b.len; i++) {
        u.v[i] = 0;
        if (sa) {
          u.v[i] = ~u.v[i];
        }
      }
      len = b.len;
      width = b.width;
    }
  } else if (b.len < len) {
    b_ext._adjlen (len-b.len);
    b_ext.len = len - b.len;
    for (i=b.len; i < len; i++) {
      b_ext._setVal (i-b.len, 0);
      if (sb == 1) {
        b_ext._setVal (i-b.len, ~b_ext.getVal (i-b.len));
      }
    }
    b_ext.width = width - b.len*BIGINT_BITS_ONE;
  }

  c = 0;
  nc = cin;
  for (i = 0; i < len; i++) {
    UNIT_TYPE value;
    c = nc;
    if (i < b.len) {
      value = b.getVal (i);
    } else {
      value = b_ext.getVal (i-b.len);
    }
    _setVal (i, getVal (i) + value);
    if (getVal (i) <  value) {
      nc = 1;
    } else {
      nc = 0;
    }
    _setVal (i, getVal (i) + c);
    if (getVal (i) == 0 && c != 0) {
      nc = 1;
    }
  }

  int dif;
  UNIT_TYPE nx;
  if (width == len * BIGINT_BITS_ONE) {
    dif = 0;
  } else {
    dif = width - (len-1) * BIGINT_BITS_ONE;
  }
  nx = 0;
  nx = ~nx;
  if (dif != 0) {
    nx = nx >> (BIGINT_BITS_ONE - dif);
  }
  
  if (isDynamic()) {
    if (nc) {
      expandSpace(1);
      width++;
      sa = isSigned() && isNegative();
      if (!sa) {
	_setVal (len-1, getVal (len-1)+1);
      }
    } else {
      if (~nx & getVal (len-1)) {
        width++;
      }
    }
  } else {
    _setVal(len-1, (getVal(len-1) & nx));
  }

  if (isSigned()) {
    signExtend();
  }
}
 

BigInt &BigInt::operator+=(const BigInt &b)
{
  _add (b, 0);
  return *this;
}

BigInt &BigInt::operator-=(const BigInt &b)
{

  if (isSigned() != b.isSigned()) {
    issigned = 0;
    BigInt tmp;
    tmp = b;
    tmp.toUnsigned();
    (*this) -= tmp;
    return (*this);
  }

  if (b.isZero() == 1) {
    return *this;
  }

  BigInt nb;

  int i;
  int c, nc;
  int sa, sb;

  sa = isSigned() && isNegative();
  sb = b.isSigned() && b.isNegative();

  BigInt b_ext;
  if (len < b.len) {
    if (isDynamic()) {
      _adjlen (b.len);
      for (i=len; i < b.len; i++) {
        u.v[i] = 0;
        if (sa) {
          u.v[i] = ~u.v[i];
        }
      }
      if (sa) {  signExtend(); }
      width = b.width;
      len = b.len;
    }
  } else if (b.len < len) {
    b_ext._adjlen (len-b.len);
    b_ext.len = len - b.len;
    for (i=b.len; i < len; i++) {
      b_ext._setVal (i-b.len, 0);
      if (sb == 1) {
        b_ext._setVal (i-b.len, ~b_ext.getVal (i-b.len));
      }
    }
    if (sb) {  b_ext.signExtend(); }
    b_ext.width = width - b.len*BIGINT_BITS_ONE;
  }

  nb = (-b);
  b_ext = (~b_ext);
  
  c = 0;
  nc = 0;
  for (i = 0; i < len; i++) {
    UNIT_TYPE value;
    c = nc;
    if (i < nb.len) {
      value = nb.getVal (i);
    } else {
      value = b_ext.getVal (i-nb.len);
    }
    _setVal (i, getVal (i) + value);
    if (getVal (i) < value) {
      nc = 1;
    } else {
      nc = 0;
    }
    _setVal (i, getVal (i) + c);
    if (getVal (i) == 0 && c != 0) {
      nc = 1;
    }
  }

  if (isSigned()) {
    signExtend();
  } else {
    zeroClear();
  }

  return *this;
}


BigInt BigInt::operator*(BigInt &b)
{

  if (isSigned() != b.isSigned()) {
    issigned = 0;
    b.issigned = 0;
  }

  BigInt tmp(*this);
  for (int i = 0; i < len; i++) {
    tmp._setVal (i, 0);
  }

  tmp.toDynamic();
  tmp.expandSpace(b.width);
  tmp.width = width + b.width;

  if (isZero() == 1 || b.isZero() == 1) {
    return tmp;
  }

  if (isOne() == 1) {
    for (auto i = 0; i < b.len; i++) {
      tmp._setVal (i, b.getVal (i));
    }
    if (b.isSigned() && b.isNegative()) {
      UNIT_TYPE mask = 0;
      mask = ~mask;
      for (auto i = b.len; i < tmp.len; i++) {
        tmp._setVal (i, mask);
      }
    }
    return tmp;
  } else if (b.isOne() == 1) {
    for (auto i = 0; i < len; i++) {
      tmp._setVal (i, getVal (i));
    }
    if (isSigned() && isNegative()) {
      UNIT_TYPE mask = 0;
      mask = ~mask;
      for (auto i = len; i < tmp.len; i++) {
        tmp._setVal (i, mask);
      }
    }
    return tmp;
  }

  int sa = 0;

  BigInt x(b);
  if (b.isSigned() && b.isNegative()) {
    sa = sa ^ 0x1;
    x = (-x);
  }

  BigInt y(*this);
  if (isSigned() && isNegative()) {
    sa = sa ^ 0x1;
    y = (-y);
  }

  int k;
  int l;
  UNIT_TYPE carry = 0;

  UNIT_TYPE mid = 0;

  UNIT_TYPE par_prod1, par_prod2, par_prod3;
  UNIT_TYPE a1, a2;
  UNIT_TYPE b1, b2;

  UNIT_TYPE um; //upper half mask
  UNIT_TYPE lm; //lower half mask

  um = 0;
  um = ~um;
  um = um >> (BIGINT_BITS_ONE/2);
  lm = ~um;

  for (auto i = 0; i < len; i++) {
    for (auto j = 0; j < x.len; j++) {
      k = i + j;

      a1 = y.getVal (i) & um;
      b1 = x.getVal (j) & um;
      a2 = y.getVal (i) >> (BIGINT_BITS_ONE/2); 
      b2 = x.getVal (j) >> (BIGINT_BITS_ONE/2);

      par_prod1 = a1 * b1;

      carry = (UNIT_TYPE)(par_prod1 + tmp.getVal (k)) < par_prod1;

      tmp._setVal (k, tmp.getVal (k) + par_prod1);

      l = k+1;
      while (carry == 1) {
        tmp._setVal (l, tmp.getVal (l) + carry);
        if (tmp.getVal (l) == 0) {
          carry = 1;
          l++;
        } else {
          carry = 0;
          break;
        }
      }

      par_prod3 = a2 * b2;

      carry = (UNIT_TYPE)(par_prod3 + tmp.getVal (k+1)) < par_prod3;

      l = k+2;
      while (carry == 1) {
        tmp._setVal (l, tmp.getVal (l) + carry);
        if (tmp.getVal (l) == 0) {
          carry = 1;
          l++;
        } else {
          carry = 0;
          break;
        }
      }

      tmp._setVal (k+1, tmp.getVal (k+1) + par_prod3);

      mid = (tmp.getVal (k) >> BIGINT_BITS_ONE/2) | (tmp.getVal (k+1) << BIGINT_BITS_ONE/2);

      par_prod2 = a2 * b1;
      par_prod3 = a1 * b2;

      carry = (UNIT_TYPE)(par_prod2 + par_prod3) < par_prod2;

      par_prod2 = par_prod2 + par_prod3;

      carry += (UNIT_TYPE)(mid + par_prod2) < mid;

      mid = mid + par_prod2;

      tmp._setVal (k,  (tmp.getVal (k) & um) | ((mid & um) << BIGINT_BITS_ONE/2));
      tmp._setVal (k+1, (tmp.getVal (k+1) & lm) | ((mid & lm) >> BIGINT_BITS_ONE/2));

      if (carry > 0) {
        carry = carry << (BIGINT_BITS_ONE/2);
        
        l = k + 1;
        while (carry > 0) {
          if ((UNIT_TYPE)(tmp.getVal (l) + carry) < tmp.getVal (l)) {
            tmp._setVal (l, tmp.getVal (l) + carry);
            carry = 1;
            l++;
          } else {
            tmp._setVal (l, tmp.getVal (l) + carry);
            carry = 0;
          }
        }
      }
    }
  }

  if(sa) {
    tmp = (-tmp);
    tmp.toSigned();
  }

  return tmp;
}

void BigInt::_div(BigInt &b, int func)
{
  if (isSigned() != b.isSigned()) {
    issigned = 0;
    b.issigned = 0;
  }

  if (b.isZero()) {
    fatal_error("Ouch! Divide by zero...");
  }

  if (func == 0) {
    if (isZero() || b.isOne()){
      return;
    }
    
    if (*this == b) {
      for (auto i = 0; i < len; i++) {
        if (i == 0) {
          _setVal (i, 1);
        } else {
          _setVal (i, 0);
        }
      }
      return;
    }
  } else {
    if (*this == b || isZero() || b.isOne()) {
      for (auto i = 0; i < len; i++) {
        _setVal (i, 0);
      }
      return;
    }
  }

  int sa = 0;
  BigInt x;
  if (b.isSigned() && b.isNegative()) {
    x = (-b);
    sa = sa ^ 0x1;
  } else {
    x = b;
  }

  BigInt y;
  if (isSigned() && isNegative()) {
    y = (-(*this));
    sa = sa ^ 0x1;
  } else {
    y = *this;
  }

  if (width <= BIGINT_BITS_ONE && b.width <= BIGINT_BITS_ONE) {
    if (func == 0) {
      y._setVal (0, y.getVal (0)/x.getVal (0));
    } else {
      y._setVal (0, y.getVal (0)%x.getVal (0));
    }
    if (sa) {
      y = (-y);
    }
    *this = y;
    return;
  }

  x.toUnsigned();
  y.toUnsigned();

  BigInt tmp_q(*this);
  BigInt tmp_r(*this);
  for (int i = 0; i < len; i++) {
    tmp_q._setVal (i, 0);
    tmp_r._setVal (i, 0);
  }
  tmp_q.toUnsigned();
  tmp_q.toStatic();

  tmp_r.toUnsigned();
  tmp_r.toStatic();

  BigInt one;
  one.toStatic();
  one.toUnsigned();
  UNIT_TYPE l_one = 1;
  one = l_one;

  unsigned int word_num = len-1;
  unsigned int res = width % BIGINT_BITS_ONE;
  unsigned int shift = (res == 0) ? (BIGINT_BITS_ONE -1) : (res -1);
  
  for (auto k = width; k > 0; k--) {
    tmp_q <<= 1;
    tmp_r <<= 1;

    if (((y.getVal (word_num) >> shift) & 0x1) == 1) {
      tmp_r += one;
    }
    if (tmp_r >= x) {
      tmp_r -= x;
      tmp_q += one;
    }

    if (shift == 0) {
      word_num--;
      shift = BIGINT_BITS_ONE -1;
    } else {
      shift--;
    }
  }

  if (sa) {
    if (func == 0) {
      tmp_q = (-tmp_q);
      tmp_q.toSigned();
    } else {
      tmp_r = (-tmp_r);
      tmp_r.toSigned();
    } 
  }

  if (isDynamic() == 0) {
    if (func == 0) {
      tmp_q.adjlen(len);
    } else {
      tmp_r.adjlen(len);
    }
  }

  if (func == 0) {
    *this = tmp_q;
  } else {
    *this = tmp_r;
  }

}

BigInt BigInt::operator/(BigInt &b)
{
  _div(b, 0);
  return *this;
}

BigInt BigInt::operator%(BigInt &b)
{
  _div(b, 1);
  return *this;
}

/*------------------------------------------------------------------------
 *
 *   Return Nth bit
 *
 *------------------------------------------------------------------------
 */
unsigned int BigInt::nBit(unsigned long n)
{
  unsigned int num = n/BIGINT_BITS_ONE;
  unsigned int shift = n%BIGINT_BITS_ONE;

  unsigned int res = (getVal (num) >> shift) & 0x1;

  return res;
}

/*------------------------------------------------------------------------
 *
 *   Supporting private methods.
 *   isZero - returns 1 if the number is zero
 *   isOne - returns 1 if the number is one
 *
 *------------------------------------------------------------------------
 */
int BigInt::isZero () const
{
  if (len >= 2) {
    for (auto i = 0; i < len; i++) {
      if (u.v[i] != 0) return 0;
    }
  } else {
    if (u.value != 0) return 0;
  }
  return 1;
}

int BigInt::isOne () const
{
  if (getVal (0) == 1) {
    for (auto i = 1; i < len; i++) {
      if (getVal (i) != 0) {
        return 0;
      }
    }
    return 1;
  } else {
    return 0;
  }
}

int BigInt::isOneInt() const
{
  if (len >= 2) {
    for (auto i = 1; i < len; i++) {
      if (u.v[i] != 0) {
        return 0;
      }
    }
    //squeezeSpace((len-1) * BIGINT_BITS_ONE);
    return 1;
  } else {
    return 1;
  }
}
/*------------------------------------------------------------------------
 *
 *  Logical operations
 *
 *------------------------------------------------------------------------
 */
BigInt &BigInt::operator&=(const BigInt &b)
{
  if (isSigned() != b.isSigned()) {
    issigned = 0;
    BigInt tmp;
    tmp = b;
    tmp.toUnsigned ();
    (*this) &= tmp;
    return (*this);
  }

  int mil = std::min(len, b.len); //min length
  int mal = std::max(len, b.len); //max length
  adjlen(mal);
  len = mal;
  for (int i = 0; i < mal; i++) {
    if (i < mil) {
      setVal (i, getVal (i) & b.getVal (i));
    } else {
      setVal (i, 0);
    }
  }

  if (isDynamic()) {
    width = std::max(width, b.width);
  }
  
  return (*this);
}

BigInt &BigInt::operator|=(const BigInt &b)
{
  if (isSigned() != b.isSigned()) {
    issigned = 0;
    BigInt tmp;
    tmp = b;
    tmp.toUnsigned ();
    (*this) |= tmp;
    return (*this);
  }

  int mil = std::min(len, b.len); //min length
  int mal = std::max(len, b.len); //max length
  adjlen(mal);
  len = mal;
  for (int i = 0; i < mal; i++) {
    if (i < mil) {
      setVal (i, getVal (i) | b.getVal (i));
    } else {
      if (i < b.len) {
	setVal (i, b.getVal (i));
      }
    }
  }

  if (isDynamic()) {
    width = std::max(width, b.width);
  }
  
  return (*this);
}

BigInt &BigInt::operator^=(const BigInt &b)
{
  if (isSigned() != b.isSigned()) {
    issigned = 0;
    BigInt tmp;
    tmp = b;
    tmp.toUnsigned ();
    (*this) ^= tmp;
    return (*this);
  }

  int mil = std::min(len, b.len); //min length
  int mal = std::max(len, b.len); //max length
  adjlen(mal);
  len = mal;
  for (int i = 0; i < mal; i++) {
    if (i < mil) {
      setVal (i, getVal (i) ^ b.getVal (i));
    } else {
      if (i < b.len) {
	setVal (i, b.getVal (i));
      }
    }
  }

  if (isDynamic()) {
    width = std::max(width, b.width);
  }

  return (*this);
}

BigInt &BigInt::operator~()
{
  for (int i=0; i < len; i++) {
    setVal (i, ~getVal (i));
  }
  if (!isSigned()) {
    zeroClear ();
  }

  return (*this);
}

BigInt &BigInt::operator<<=(UNIT_TYPE x)
{

  if (x == 0) return *this;

  if (isDynamic()) {
    if ((width + x) > (len*BIGINT_BITS_ONE)) {
      expandSpace (x);
    }
    width += x;
  }

  int stride = x / BIGINT_BITS_ONE;
  x = x % BIGINT_BITS_ONE;

  UNIT_TYPE mask = 0;
  mask = ~mask << x;
  for (int i = len-1; i >= 0; i--) {
    if (i + stride < len) {
      if (i > 0 && (x != 0)) {
	_setVal (i+stride, (getVal (i) << x) | (getVal (i-1) >> (BIGINT_BITS_ONE-x)));
      }
      else {
	_setVal(i+stride, getVal(i) << x);
      }
    }
  }
  for (int i=0; i < stride; i++) {
    _setVal (i, 0);
  }

  return (*this);
}

BigInt &BigInt::operator>>=(UNIT_TYPE x)
{
  if (x == 0) return *this;

  int sa = isNegative() && isSigned();

  if (x >= width) {
    if (isDynamic()) {
      _adjlen (1);
      len = 1;
      width = 1;
      _setVal (0, 0);
      if (sa) {
        _setVal (0, ~getVal (0));
      }
    } else {
      for (int i=0; i < len; i++) {
        _setVal (i, 0);
        if (sa) {
          _setVal (i, ~getVal (i));
        }
      }
    }

    return *this;
  } else {
    if (isDynamic()) {
      width -= x;
    }
  }

  int stride = x / BIGINT_BITS_ONE;
  UNIT_TYPE mask = 0;

  x = x % BIGINT_BITS_ONE;

  mask = ~mask;
  mask = mask >> (BIGINT_BITS_ONE-x);

  for (auto i = 0; i < len-stride; i++) {
    _setVal (i, getVal (i+stride) >> x);
    if ((i + stride + 1)  < len) {
      if (x != 0) {
        _setVal (i, getVal (i) | (getVal (i+stride+1) & mask) << (BIGINT_BITS_ONE - x));
      }
    } 
  }

  UNIT_TYPE y = 0;
  if (sa) {
    y = ~y;
  }
  for (auto i = len-stride; i < len; i++) {
    _setVal (i, y);
  }
  if (x != 0) {
    y = y << (BIGINT_BITS_ONE - x);
  }
  _setVal (len-stride-1, getVal (len-stride-1) | y);

  if (isDynamic()) {
    if (stride > 0) {
      _adjlen (len-stride);
      len -= stride;
    }
    if (len > 1 && ((len-1)*BIGINT_BITS_ONE >= width)) {
      _adjlen (len-1);
      len--;
    }
  }

  return (*this);
}

BigInt &BigInt::operator<<=(const BigInt &b)
{
  Assert (b.isOneInt(), "Shift amounts have to be small enough to fit into a single intger");
  Assert (!b.isNegative(), "Non-negative shift amounts only");

  (*this) <<= b.getVal (0);
  return (*this);
}

BigInt &BigInt::operator>>=(const BigInt &b)
{
  Assert (b.isOneInt(), "Shift amounts have to be small enough to fit into a single intger");
  Assert (!b.isNegative(), "Non-negative shift amounts only");

  (*this) >>= b.getVal (0);
  return (*this);
}

/*------------------------------------------------------------------------
 *
 *   Print functions
 *   sPrint - prints to the string
 *   hPrint - prints HEX to the FILE
 *
 *------------------------------------------------------------------------
 */
std::string BigInt::sPrint() const
{
  char buf[64];
  std::string s = "";

  for (int i = len-1; i >= 0; i--) {
#ifndef BIGINT_TEST
    if (sizeof(UNIT_TYPE) == 8) {
      if (i == len-1) {
	snprintf(buf, 64, "%lx", getVal (i));
      }
      else {
	snprintf(buf, 64, "%016lx", getVal (i));
      }
    } 
#else
    if (sizeof(UNIT_TYPE) == 1) {
      snprintf(buf, 64, "%02x", getVal (i));
    }
#endif
    s += buf; 
  }

  return s;
}

void BigInt::hPrint (FILE *fp) const
{
  fprintf (fp, "{w=%d,bw=%d, dyn=%d,sgn=%d}0x", width, len, isdynamic, issigned);
  for (int i=len-1; i >= 0; i--) {
#ifndef BIGINT_TEST
    if (sizeof(UNIT_TYPE) == 8) {
      fprintf (fp, "%016lx_", getVal (i));
    } 
#else
    if (sizeof(UNIT_TYPE) == 1) {
      fprintf (fp, "%02x_", getVal (i));
    }
#endif
  }
}

void BigInt::hexPrint (FILE *fp) const
{
  for (int i=len-1; i >= 0; i--) {
#ifndef BIGINT_TEST
    if (sizeof(UNIT_TYPE) == 8) {
      if (i != len-1) {
  fprintf (fp, "%016lx", getVal (i));
      }
      else {
  fprintf (fp, "%lx", getVal (i));
      }
    } 
#else
    if (sizeof(UNIT_TYPE) == 1) {
      fprintf (fp, "%02x", getVal (i));
    }
#endif
  }
}

void BigInt::bitPrint (FILE *fp) const
{
  int top_bits;
  UNIT_TYPE u;
  top_bits = width % (sizeof (UNIT_TYPE)*8);
  if (top_bits == 0) {
    top_bits = 8*sizeof (UNIT_TYPE);
  }
  u = getVal (len-1);
  for (int j=top_bits-1; j >= 0; j--) {
    fprintf (fp, "%c", ((u >> j) & 1) ? '1' : '0');
  }
  for (int i=len-2; i >= 0; i--) {
    u = getVal (i);
    top_bits = 8*sizeof (UNIT_TYPE);
    for (int j=top_bits-1; j >= 0; j--) {
      fprintf (fp, "%c", ((u >> j) & 1) ? '1' : '0');
    }
  }
}

void BigInt::decPrint (FILE *fp, int w) const
{
 char *buf;
 BigInt tmp;
 int pos;
 int sign;
 tmp = (*this);
 if (tmp.isSigned()) {
   tmp.toUnsigned();
 }
 if (tmp.isZero()) {
   MALLOC (buf, char, 2);
   buf[0] = '0';
   buf[1] = '\0';
   pos = 1;
 }
 else {
   MALLOC (buf, char, 2 + (int)((tmp.len*sizeof (UNIT_TYPE)*8+0.0)/3.32));
   pos = 0;
   BigInt ten = BigInt(4,0,1);
   ten.setVal (0, 10);
   while (!tmp.isZero()) {
     BigInt rem = tmp;
     rem = rem % ten;
     tmp = tmp / ten;
     Assert (0 <= rem.getVal (0) && rem.getVal (0) <= 9, "What?");
     buf[pos++] = rem.getVal (0) + '0';
   }
   buf[pos] = '\0';
 }
 if (w < 0) {
   sign = 1;
   w = -w;
 }
 else {
   sign = 0;
 }

 if (w != 0 && w < pos) {
   w = 0;
 }

 if (w != 0 && sign == 0) {
   while (w > pos) {
     fputc (' ', fp);
     w--;
   }
   w = 0;
 }

 while (pos > 0) {
   fputc (buf[pos-1], fp);
   pos--;
 }

 if (w != 0 && sign == 1) {
   while (w > pos) {
     fputc (' ', fp);
     w--;
   }
 }

 FREE (buf);
}

BigInt BigInt::sscan (const char *s)
{
  BigInt tmp;
  int mode = 0; // 0 = decimal, 1 = hex, 2 = binary
  if (!*s) return tmp;
  if (*s == '0') {
    // could be binary or hex
    s++;
    if (*s == 'x') {
      mode = 1;
      s++;
    }
    else if (*s == 'b') {
      mode = 2;
      s++;
    }
  }
  tmp.toDynamic ();
  BigInt base(4,0,1);
  if (mode == 0) {
    base.setVal (0, 10);
  }
  else if (mode == 1) {
    base.setVal (0, 16);
  }
  while (*s) {
    BigInt d(4,0,1);
    if (mode == 0) {
      if ('0' <= *s && *s <= '9') {
	tmp = tmp * base;
	d.setVal (0, *s - '0');
	tmp += d;
      }
      else {
	break;
      }
    }
    else if (mode == 1) {
      if ('0' <= *s && *s <= '9') {
	tmp = tmp * base;
	d.setVal (0, *s - '0');
	tmp += d;
      }
      else if ('a' <= *s && *s <= 'f') {
	tmp = tmp * base;
	d.setVal (0, 10 + (*s - 'a'));
	tmp += d;
      }	
      else if ('A' <= *s && *s <= 'F') {
	tmp = tmp * base;
	d.setVal (0, 10 + (*s - 'A'));
	tmp += d;
      }
      else {
	break;
      }
    }
    else {
      if (*s == '0' || *s == '1') {
	tmp <<= 1;
	d.setVal (0, *s - '0');
	tmp |= d;
      }
      else {
	break;
      }
    }
    s++;
  }
  tmp.toStatic();
  return tmp;
}
