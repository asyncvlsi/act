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
 * @class Type
 *
 * @brief The abstract base class for all types in the system.
 *
 * Any type in the system inherits from this base type. It has a
 * number of common methods that all types are expected to
 * provide. Types tend to never be deleted, since they are only
 * created when needed and then persist until the program no longer
 * needs to use the ACT library.
 *
 * Common types are cached by the TypeFactory, and types should only
 * be created via the TypeFactory class.
 */
class Type {
 public:
  Type() { }; ///< constructor
  ~Type() { }; ///< destructor

  /**
   * @return a string corresponding to the full type name for the
   * specified type. This should not be free'd.
   */
  virtual const char *getName () = 0;
  
  /**
   * This is used to expand the type, substituting any template
   * parameters. These parameters are used for templated built-in
   * types like int<>, chan(), and ptype().
   * @param ns is the namespace
   * @param s is the evaluation scope
   * @param nt are the number of template parameters for this type
   * @param ip is the parameter name
   * @return an expanded type
   */
  virtual Type *Expand (ActNamespace *ns, Scope *s, int nt, inst_param *ip) = 0;

  /**
   * Check if two types are equal to each other.
   * @param t is the type to compare against
   * @return the result of comparing two types
   */
  virtual int isEqual (const Type *t) const = 0;

  /**
   * Initialize static members. This also calls the static
   * initialization function for the TypeFactory
   */
  static void Init();

  /**
   * A type can have direction flags. The supported direction flags
   * are either none, "?" (for input), and "!" (for
   * output). Additional flags "?!" (inout) and "!?" (outin) can be
   * used in the port list of user-defined data or channel types.
   */
  enum direction {
    NONE = 0, ///< no direction flag
    IN = 1,   ///< direction flag is ?
    OUT = 2,  ///< direction flag is !
    INOUT = 3, ///< direction flag is ?!
    OUTIN = 4  ///< direction flag is !?
  };

  /**
   * Used for converting a direction into a string for printing.
   * @param d is the direction flag
   * @return the string corresponding to the flag
   */
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
