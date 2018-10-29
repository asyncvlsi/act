/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
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
