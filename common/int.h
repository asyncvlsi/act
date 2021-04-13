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
#ifndef __COMMON_INT_H__
#define __COMMON_INT_H__

#include <common/misc.h>


#define BIGINT_BITS_ONE (8*sizeof (unsigned long))

class BigInt {
 public:
  BigInt ();			// default int is 1-bit wide, signed, and
				// dynamic
  
  BigInt (int w, int s = 1);	// static bitwidth bigint, signed
  
  ~BigInt ();

  BigInt (BigInt &&);		// move constructor
  BigInt (BigInt &);		// copy constructor

  BigInt& operator=(BigInt &);         // assignment
  BigInt& operator=(BigInt &&);	       // move
    
  int operator<(BigInt &);
  int operator<=(BigInt &);
  int operator>(BigInt &);
  int operator>=(BigInt &);
  int operator==(BigInt &);

  BigInt &operator+(BigInt &);
  BigInt &operator-(BigInt &);
  BigInt operator-();
  BigInt &operator*(BigInt &);
  BigInt &operator/(BigInt &);
  BigInt &operator%(BigInt &);

  BigInt &operator&(BigInt &);
  BigInt &operator|(BigInt &);
  BigInt &operator^(BigInt &);
  BigInt &operator~();

  BigInt &operator<<(unsigned long x);
  BigInt &operator>>(unsigned long x);
  BigInt &operator<<(BigInt &b);
  BigInt &operator>>(BigInt &b);

  inline int isOneInt() { return len == 1 ? 1 : 0; };

  inline int isNegative();

  static BigInt dynInt (int x);

  void Print (FILE *fp);

 private:
  unsigned int issigned:1;	// 1 if signed, 0 if unsigned
  unsigned int isdynamic:1;	// 1 if the bitwidth is dynamic

  int width;			// actual bitwidth
  
  int len;			// storage needed
  unsigned long *v;		// actual bits; 2's complement
				// rep. The number is sign-extended to
				// the maximum width of the rep

  void expandSpace(int amt = 1); // expand bitwidth by specified number
				// of bits
  void signExtend ();
  void zeroExtend (int w);
  void zeroClear ();

};


#endif /* __ACT_INT_H__ */
