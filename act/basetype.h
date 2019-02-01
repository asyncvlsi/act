/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#ifndef __ACT_BASETYPE_H__
#define __ACT_BASETYPE_H__

class ActNamespace;
class Scope;
struct inst_param;

/**
 * The base class for all types in the system
 */
class Type {
 public:
  Type() { };
  ~Type() { };

  /**
   * @return a string corresponding to the full type name for the
   * specified type. This should not be free'd.
   */
  virtual const char *getName () = 0;
  
  /**
   * @return an expanded type
   */
  virtual Type *Expand (ActNamespace *, Scope *, int, inst_param *) = 0;

  /**
   * Initialize static members. This also calls the static functions
   * for the TypeFactory
   */
  static void Init();
 
  enum direction {
    NONE = 0,
    IN = 1,
    OUT = 2,
    INOUT = 3,
    OUTIN = 4
  };

  static const char *dirstring (direction d) {
    switch (d) {
    case Type::NONE: return ""; break;
    case Type::IN: return "?"; break;
    case Type::OUT: return "!"; break;
    case Type::INOUT: return "?!"; break;
    case Type::OUTIN: return "!?"; break;
    }
    return "-err-";
  };

 protected:
  friend class TypeFactory;
};

#endif /* __ACT_BASETYPE_H__ */
