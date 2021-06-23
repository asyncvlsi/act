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
BigInt::BigInt()
{
  len = 1;
  width = 1;
  MALLOC (v, UNIT_TYPE, len);
  v[0] = 0;
  isdynamic = 1;
  issigned = 0;
}

BigInt::BigInt(int w, int s, int d)
{
  len = 0;
  width = w;
  do {
    len++;
    w = w - BIGINT_BITS_ONE;
  } while (w > 0);
  Assert (len > 0, "What?");
  MALLOC (v, UNIT_TYPE, len);
  for (int i=0; i < len; i++) {
    v[i] = 0;
  }
  isdynamic = d;
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

/*-- copy constructor --*/
BigInt::BigInt (BigInt &b)
{
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  if (len > 0) {
    MALLOC (v, UNIT_TYPE, b.len);
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
	if (&b == this) { return *this; }
  FREE (v);
  isdynamic = b.isdynamic;
  issigned = b.issigned;
  len = b.len;
  width = b.width;
  MALLOC (v, UNIT_TYPE, len);
  for (int i=0; i < len; i++) {
    v[i] = b.v[i];
  }
  return *this;
}

BigInt& BigInt::operator=(UNIT_TYPE &b)
{
  FREE (v);
  isdynamic = 0;
  issigned = 0;
  len = 1;
  width = 8*sizeof(UNIT_TYPE);
  MALLOC (v, UNIT_TYPE, len);
  v[0] = b;
  return *this;
}

BigInt& BigInt::operator=(const std::string &b)
{
	FREE(v);

	int word_cnt = 0;
	int word_len = BIGINT_BITS_ONE/4;
	int word_num = 0;
	std::string word_str = "";
	std::string filler = "";

	isdynamic = 0;
	issigned = 0;

	if ((4*(b.size()-2)) % BIGINT_BITS_ONE == 0) {
		len = 4*(b.size()-2)/BIGINT_BITS_ONE;
		MALLOC(v, UNIT_TYPE, len);
	} else {
		len = 1 + 4*(b.size()-2)/BIGINT_BITS_ONE;
		MALLOC(v, UNIT_TYPE, len);
	}

	std::string hex_test = b.substr(0,2);
	if (hex_test != "0x") {
		fatal_error("unknown input format");
	}

	width = (b.size()-2) * 4;

	for (auto it = b.rbegin(); it != b.rend()-2; ++it) {
		if (word_cnt == word_len-1){
			word_str = *it + word_str;
			v[word_num] = strtoul(word_str.c_str(),0,16);
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
				v[word_num] = strtoul(word_str.c_str(),0,16);
				break;
			}
			word_cnt++;
		}
	}

	return *this;
}

BigInt& BigInt::operator=(BigInt &&b)
{
	if (&b == this) { return *this; }
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

#ifdef BIGINT_TEST
void BigInt::SetV (int n, UNIT_TYPE nv)
{
	if (n > len) {
		expandSpace(sizeof(UNIT_TYPE));
	}
	v[n] = nv;
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
	sa = isSigned() & isNegative();

	if (sa) {
		signExtend();
	}

  REALLOC (v, UNIT_TYPE, x);

  for (; len < x; len++) {
    v[len] = 0;
		if (sa) {
			v[len] = ~v[len];
		}
  }
}

void BigInt::squeezeSpace (int amt)
{
	unsigned int tmp, x;
	tmp = width - (len-1)*BIGINT_BITS_ONE;

	if (tmp <= amt) {
		x = (width - amt)/BIGINT_BITS_ONE;
		REALLOC(v, UNIT_TYPE, x);
		len = x;
	}
	width = width - amt;

	tmp = len*BIGINT_BITS_ONE - width;
	UNIT_TYPE res = 0;
	res = ~res;
	res = res >> tmp;
	v[len-1] = v[len-1] & res;

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
BigInt BigInt::operator-()
{
  int i;
  BigInt b(*this);
  int c = 1;
  int sa = isSigned() && isNegative();

  for (int i=0; i < len; i++) {
    b.v[i] = ~b.v[i];
  }

	for (int i = 0; c == 1 && (i<len); i++) {
		b.v[i] = b.v[i] + c;
		if (b.v[i] == 0) {
			c = 1;
		} else {
			c = 0;
			break;
		}
	}
  if (c && isDynamic()) {
    b.expandSpace (1);
    b.v[len-1] = 0;
    if (sa) {
      b.v[len-1] = ~b.v[len-1];
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
int BigInt::isNegative ()
{
  /* residual width */
  if (!issigned) {
    return 0;
  }
  
  return (v[len-1] >> (BIGINT_BITS_ONE-1)) & 0x1;
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
  	sa = (v[len-1] >> (res-1)) & 0x1;
	}

  if (!issigned) {
    sa = 0;
  }

  UNIT_TYPE x = 0;
  x = ~x;
  x = x << res;

  if (sa) {
    /* negative */
    v[len-1] |= x;
  } else {
    v[len-1] &= ~x;
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
  int res = width - (len-1)*BIGINT_BITS_ONE;
  UNIT_TYPE x;
  x = 0;
  x = ~x;
  x = x << res;
	x = ~x;
	if (x != 0) {
  	v[len-1] = v[len-1] & x;
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

int BigInt::isSigned()
{
	return issigned;
}

void BigInt::toDynamic()
{
	isdynamic = 1;
}

void BigInt::toStatic()
{
	isdynamic = 0;
}

int BigInt::isDynamic()
{
	return isdynamic;
}
/*------------------------------------------------------------------------
 *
 *  Comparison operators
 *
 *------------------------------------------------------------------------
 */
int BigInt::operator==(BigInt &b)
{

	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

 int za, zb;
 int sa, sb;
 int i;
 
	za = isZero();
	zb = b.isZero();
	if (za && zb) return 1;
	if (za || zb) return 0;
	
	sa = isSigned() & isNegative();
	sb = b.isSigned() & b.isNegative();

	if (len > b.len) {
	  for (i=len-1; i >= b.len; i--) {
			UNIT_TYPE mask = 0;
	    if (sb == 1) {
				mask = ~mask;
	      if (v[i] != mask) {
				  return 0;
	      }
	    } else {
	      if (v[i] != mask) {
	  			return 0;
	      }
	    }
	  }
	}	else {
	  for (i=b.len-1; i >= len; i--) {
			UNIT_TYPE mask = 0;
	    if (sa == 1) {
				mask = ~mask;
	      if (b.v[i] != mask) {
	 				return 0;
	      }
	    } else {
	      if (b.v[i] != mask) {
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
		if (tmp != v[i]) {
			return 0;
		}
	}

	return 1;

}

int BigInt::operator==(long b)
{
	unsigned long mask;
	mask = 0;
	mask = ~mask;
	mask = mask << BIGINT_BITS_ONE;
	mask = ~mask;

	UNIT_TYPE tmp;

	for (auto i = 0; i < len; i++) {
		tmp = (b  >> (i * BIGINT_BITS_ONE)) & mask;
		if (tmp != v[i]) {
			return 0;
		}
	}

	return 1;

}
#endif

int BigInt::operator!=(BigInt &b)
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

int BigInt::operator<(BigInt &b)
{

	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
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
      if (sa == 0) {
				if (v[i]) {
	  			res = 1;  // I am larger
	  			break;
				}
      } else {
				if (~v[i]) {
	  			res = 0;
	  			break;
				}
      }
    }
    /* either res is set, or i = b.len - 1 */
  } else {
    for (i=b.len-1; i >= len; i--) {
      if (sa == 0) {
				if (b.v[i]) {
	  			res = 0;
	  			break;
				}
      } else {
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
				} else if (v[i] < b.v[i]) {
	  			res = 0;
	  			break;
				}
      } else {
				if (v[i] > b.v[i]) {
	  			res = 1;
	  			break;
				} else if (v[i] < b.v[i]) {
	  			res = 0;
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

	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

  int i;
  int c, nc;
  int sa, sb;

  sa = isSigned() & isNegative();
  sb = b.isSigned() & b.isNegative();

	BigInt b_ext;
  if (len < b.len) {
    REALLOC (v, UNIT_TYPE, b.len);
    for (i=len; i < b.len; i++) {
      v[i] = 0;
      if (sa == 1) {
				v[i] = ~v[i];
      }
    }
		if (isDynamic()) {
    	len = b.len;
    	width = b.width;
		}
  } else if (b.len < len) {
		REALLOC (b_ext.v, UNIT_TYPE, len-b.len);
    for (i=b.len; i < len; i++) {
      b_ext.v[i-b.len] = 0;
      if (sb == 1) {
				b_ext.v[i-b.len] = ~b_ext.v[i-b.len];
      }
    }
    b_ext.len = len - b.len;
    b_ext.width = width - b.width;
  }

	c = 0;
	nc = 0;
	for (i = 0; i < len; i++) {
		c = nc;
		if (i < b.len) {
			v[i] = v[i] + b.v[i];
			if (v[i] < b.v[i]) {
				nc = 1;
			} else {
				nc = 0;
			}
		} else {
			v[i] = v[i] + b_ext.v[i-b.len];
			if (v[i] < b_ext.v[i-b.len]) {
				nc = 1;
			} else {
				nc = 0;
			}
		}

		v[i] = v[i] + c;
		if (v[i] == 0 && c != 0) {
			nc = 1;
		}
	}

	int dif;
	UNIT_TYPE x, nx;
	if (nc && isDynamic()){
		if (width == len * BIGINT_BITS_ONE) {
			dif = 0;
		} else {
			dif = width - (len-1) * BIGINT_BITS_ONE;
		}
		if (dif != 0) {
			nx = 0;
			nx = ~nx;
			nx = nx >> (BIGINT_BITS_ONE - dif);
			width++;
			expandSpace(1);
			v[len-1]++;
		} 
	}

	if (isSigned()) {
		signExtend();
	}

  return *this;

}

BigInt &BigInt::operator-(BigInt &b)
{

	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

	if (b.isZero() == 1) {
		return *this;
	}

	BigInt nb;

  int i;
  int c, nc;
  int sa, sb;

  sa = issigned & isNegative();
  sb = b.issigned & b.isNegative();
  
	BigInt b_ext;
  if (len < b.len) {
    REALLOC (v, UNIT_TYPE, b.len);
    for (i=len; i < b.len; i++) {
      v[i] = 0;
      if (sa) {
				v[i] = ~v[i];
      }
    }
		if (isDynamic()) {
    	len = b.len;
    	width = b.width;
		}
  } else if (b.len < len) {
		REALLOC (b_ext.v, UNIT_TYPE, len-b.len);
    for (i=b.len; i < len; i++) {
      b_ext.v[i-b.len] = 0;
      if (sb == 1) {
				b_ext.v[i-b.len] = ~b_ext.v[i-b.len];
      }
    }
    b_ext.len = len - b.len;
    b_ext.width = width - b.width;
  }
 
	nb = (-b);
	b_ext = ~b_ext;

	c = 0;
	nc = 0;
	for (i = 0; i < len; i++) {
		c = nc;
		if (i < nb.len) {
			v[i] = v[i] + nb.v[i];
			if (v[i] < nb.v[i]) {
				nc = 1;
			} else {
				nc = 0;
			}
		} else {
			v[i] = v[i] + b_ext.v[i-nb.len];
			if (v[i] < b_ext.v[i-nb.len]) {
				nc = 1;
			} else {
				nc = 0;
			}
		}
		v[i] = v[i] + c;
		if (v[i] == 0 && c != 0) {
			nc = 1;
		}
	}
  
	if (isSigned()) {
		signExtend();
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
		tmp.v[i] = 0;
	}

	tmp.toDynamic();
	tmp.expandSpace(b.width);
	tmp.width = width + b.width;

	if (isZero() == 1 || b.isZero() == 1) {
		return tmp;
	}

	if (isOne() == 1) {
		for (auto i = 0; i < b.len; i++) {
			tmp.v[i] = b.v[i];
		}
		if (b.isSigned() && b.isNegative()) {
			UNIT_TYPE mask = 0;
			mask = ~mask;
			for (auto i = b.len; i < tmp.len; i++) {
				tmp.v[i] = mask;
			}
		}
		return tmp;
	} else if (b.isOne() == 1) {
		for (auto i = 0; i < len; i++) {
			tmp.v[i] = v[i];
		}
		if (isSigned() && isNegative()) {
			UNIT_TYPE mask = 0;
			mask = ~mask;
			for (auto i = len; i < tmp.len; i++) {
				tmp.v[i] = mask;
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

	UNIT_TYPE par_prod1, par_prod2, par_prod3, par_prod4;
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

			a1 = y.v[i] & um;
			b1 = x.v[j] & um;
			a2 = y.v[i] >> (BIGINT_BITS_ONE/2);	
			b2 = x.v[j] >> (BIGINT_BITS_ONE/2);

			par_prod1 = a1 * b1;

			carry = (UNIT_TYPE)(par_prod1 + tmp.v[k]) < par_prod1;

			tmp.v[k] += par_prod1;

			l = k+1;
			while (carry == 1) {
				tmp.v[l] += carry;
				if (tmp.v[l] == 0) {
					carry = 1;
					l++;
				} else {
					carry = 0;
					break;
				}
			}

			par_prod3 = a2 * b2;

			carry = (UNIT_TYPE)(par_prod3 + tmp.v[k+1]) < par_prod3;

			l = k+2;
			while (carry == 1) {
				tmp.v[l] += carry;
				if (tmp.v[l] == 0) {
					carry = 1;
					l++;
				} else {
					carry = 0;
					break;
				}
			}

			tmp.v[k+1] += par_prod3;

			mid = (tmp.v[k] >> BIGINT_BITS_ONE/2) | (tmp.v[k+1] << BIGINT_BITS_ONE/2);

			par_prod2 = a2 * b1;
			par_prod3 = a1 * b2;

			carry = (UNIT_TYPE)(par_prod2 + par_prod3) < par_prod2;

			par_prod2 = par_prod2 + par_prod3;

			carry += (UNIT_TYPE)(mid + par_prod2) < mid;

			mid = mid + par_prod2;

			tmp.v[k] = (tmp.v[k] & um) | ((mid & um) << BIGINT_BITS_ONE/2);
			tmp.v[k+1] = (tmp.v[k+1] & lm) | ((mid & lm) >> BIGINT_BITS_ONE/2);

			if (carry > 0) {
				carry = carry << (BIGINT_BITS_ONE/2);

				l = k + 1;
				while (carry > 0) {
					if ((UNIT_TYPE)(tmp.v[l] + carry) < tmp.v[l]) {
						tmp.v[l] = tmp.v[l] + carry;
						carry = 1;
						l++;
					} else {
						tmp.v[l] = tmp.v[l] + carry;
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

BigInt BigInt::operator/(BigInt &b)
{

	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

	if (b.isZero()) {
		fatal_error("Ouch! Divide by zero...");
	}

	if (isZero() || b.isOne()){
		return (*this);
	}

	if (*this == b) {
		for (auto i = 0; i < len; i++) {
			if (i == 0) {
				v[i] = 1;
			} else {
				v[i] = 0;
			}
		}
		return *this;
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
		y.v[0] = y.v[0]/x.v[0];
		if (sa) {
			y = (-y);
		}
		return y;
	}

	x.toUnsigned();
	y.toUnsigned();

	BigInt tmp_q(*this);
	BigInt tmp_r(*this);
	for (int i = 0; i < len; i++) {
		tmp_q.v[i] = 0;
		tmp_r.v[i] = 0;
		tmp_r.toUnsigned();
		tmp_q.toStatic();
		tmp_r.toStatic();
	}

	BigInt one;
	one.toStatic();
	one.toUnsigned();
	UNIT_TYPE l_one = 1;
	one = l_one;

	unsigned int word_num = len-1;
	unsigned int res = width % BIGINT_BITS_ONE;
	unsigned int shift = (res == 0) ? (BIGINT_BITS_ONE -1) : (res -1);

	for (auto k = width; k > 0; k--) {
		tmp_q = tmp_q << 1;
		tmp_r = tmp_r << 1;

		if (((y.v[word_num] >> shift) & 0x1) == 1) {
			tmp_r = tmp_r + one;
		}
		if (tmp_r >= x) {
			tmp_r = tmp_r - x;
			tmp_q = tmp_q + one;
		}

		if (shift == 0) {
			word_num--;
			shift = BIGINT_BITS_ONE -1;
		} else {
			shift--;
		}
	}

	if (sa) {
		tmp_q = (-tmp_q);
		tmp_q.toSigned();
	}

	return tmp_q;
}

BigInt BigInt::operator%(BigInt &b)
{
	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

	if (b.isZero()) {
		fatal_error("Ouch! MOD(Divide) by zero...");
	}

	if (*this == b || isZero() || b.isOne()) {
		for (auto i = 0; i < len; i++) {
				v[i] = 0;
		}
		return *this;
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
	if (isSigned()  && isNegative()) {
		y = (-(*this));
		sa = sa ^ 0x1;
	} else {
		y = *this;
	}

	if (width <= BIGINT_BITS_ONE && b.width <= BIGINT_BITS_ONE) {
		y.v[0] = y.v[0]%x.v[0];
		if (sa) {
			y = (-y);
		}
		return y;
	}

	x.toUnsigned();
	y.toUnsigned();

	BigInt tmp_r(*this);
	for (int i = 0; i < len; i++) {
		tmp_r.v[i] = 0;
		tmp_r.toUnsigned();
		tmp_r.toStatic();
	}

	BigInt one;
	one.toStatic();
	one.toUnsigned();
	UNIT_TYPE l_one = 1;
	one = l_one;

	unsigned int word_num = len-1;
	unsigned int res = width % BIGINT_BITS_ONE;
	unsigned int shift = (res == 0) ? BIGINT_BITS_ONE -1 : res -1;

	for (auto k = width; k > 0; k--) {
		tmp_r = tmp_r << 1;

		if (((y.v[word_num] >> shift) & 0x1) == 1) {
			tmp_r = tmp_r + one;
		}

		if (tmp_r >= x) {
			tmp_r = tmp_r - x;
		}

		if (shift == 0) {
			word_num--;
			shift = BIGINT_BITS_ONE -1;
		} else {
			shift--;
		}
	}

	if (sa) {
		tmp_r = (-tmp_r);
		tmp_r.toSigned();
	}

	return tmp_r;
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

	unsigned int res = (v[num] >> shift) & 0x1;

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
int BigInt::isZero ()
{
	for (auto i = 0; i < len; i++) {
		if (v[i] != 0) {
			return 0;
		}
	}
	return 1;
}

int BigInt::isOne ()
{
	if (v[0] == 1) {
		for (auto i = 1; i < len; i++) {
			if (v[i] != 0) {
				return 0;
			}
		}
		return 1;
	} else {
		return 0;
	}
}

/*------------------------------------------------------------------------
 *
 *  Logical operations
 *
 *------------------------------------------------------------------------
 */
#define LOGICAL_SETUP				\
  BigInt x = b;					\
  x.setWidth (width)
  
BigInt &BigInt::operator&(BigInt &b)
{
	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

  LOGICAL_SETUP;

  for (int i=0; i < len; i++) {
    v[i] = v[i] & x.v[i];
  }

  return (*this);
}

BigInt &BigInt::operator|(BigInt &b)
{
	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

  LOGICAL_SETUP;
  
  for (int i=0; i < len; i++) {
    v[i] = v[i] | x.v[i];
  }

  return (*this);
}

BigInt &BigInt::operator^(BigInt &b)
{
	if (isSigned() != b.isSigned()) {
		issigned = 0;
		b.issigned = 0;
	}

  LOGICAL_SETUP;
  
  for (int i=0; i < len; i++) {
    v[i] = v[i] ^ x.v[i];
  }

  return (*this);
}

BigInt &BigInt::operator~()
{
  for (int i=0; i < len; i++) {
    v[i] = ~v[i];
  }
  zeroClear ();

  return (*this);
}

BigInt &BigInt::operator<<(UNIT_TYPE x)
{

  if (x == 0) return *this;

  if (isDynamic()) {
    expandSpace (x);
    width += x;
  }

  int stride = x / BIGINT_BITS_ONE;
  x = x % BIGINT_BITS_ONE;

  for (int i=len-1-stride; i >= 0; i--) {
		if (i + stride < len) {
	    v[i+stride] = (v[i] << x);
		}
    if (i > 0) {
      v[i+stride] |= (v[i-1] >> (BIGINT_BITS_ONE - x));
    }
  }
  for (int i=0; i < stride; i++) {
    v[i] = 0;
  }

  return (*this);
}

BigInt &BigInt::operator>>(UNIT_TYPE x)
{
  if (x == 0) return *this;

  int sa = isNegative() & isSigned();

  if (x >= width) {
    if (isdynamic) {
      FREE (v);
      len = 1;
      width = 1;
      MALLOC (v, UNIT_TYPE, 1);
      v[0] = 0;
      if (sa) {
				v[0] = ~v[0];
      }
    }	else {
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
  UNIT_TYPE mask = 0;

  if (isDynamic()) {
    width -= x;
  }

  x = x % BIGINT_BITS_ONE;

  mask = ~mask;
  mask = mask >> (BIGINT_BITS_ONE-x);

  for (auto i = 0; i < len-stride; i++) {
    v[i] = (v[i+stride] >> x);
	  if ((i + stride + 1)  < len) {
      v[i] |= (v[i+stride+1] & mask) << (BIGINT_BITS_ONE - x);
    } 
  }

	if (sa) {
		UNIT_TYPE y = 0;
		y = ~y;
		for (auto i = len-stride; i < len; i++) {
			v[i] = y;
		}
		y = y << (BIGINT_BITS_ONE - x);
		v[len-stride-1] |= y;
	}

  if (stride > 0) {
    REALLOC (v, UNIT_TYPE, len-stride);
    len -= stride;
  }

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

/*------------------------------------------------------------------------
 *
 *   Print functions
 *   sPrint - prints to the string
 *   hPrint - prints HEX to the FILE
 *
 *------------------------------------------------------------------------
 */
std::string BigInt::sPrint()
{
	char buf[64];
	std::string s = "";

	for (int i = len-1; i >= 0; i--) {
#ifndef BIGINT_TEST
		if (sizeof(UNIT_TYPE) == 8) {
			sprintf(buf, "%016lx", v[i]);
		} 
#else
		if (sizeof(UNIT_TYPE) == 1) {
			sprintf(buf, "%02x", v[i]);
		}
#endif
		s += buf; 
	}

	return s;
}

void BigInt::hPrint (FILE *fp)
{
	fprintf (fp, "{w=%d,bw=%d, dyn=%d,sgn=%d}0x", width, len, isdynamic, issigned);
	for (int i=len-1; i >= 0; i--) {
#ifndef BIGINT_TEST
		if (sizeof(UNIT_TYPE) == 8) {
			fprintf (fp, "%016lx_", v[i]);
		} 
#else
		if (sizeof(UNIT_TYPE) == 1) {
			fprintf (fp, "%02x_", v[i]);
		}
#endif
	}
}
