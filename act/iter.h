/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __ITER_H__
#define __ITER_H__

#include <act/namespaces.h>
#include <iterator>

#define ACTSTDITER(name,rettype)		\
  name (const name & c);			\
  name& operator++();				\
  name operator++(int);				\
  bool operator==(const name& rhs) const;	\
  bool operator!=(const name& rhs) const;	\
  rettype operator*();				\
  name begin();					\
  name end()
    

class ActNamespaceiter : public
      std::iterator<std::input_iterator_tag, ActNamespace *> {
  
  hash_bucket_t *b;
  int i;
  ActNamespace *top;

 public:
  ActNamespaceiter (ActNamespace *ns);
  ACTSTDITER(ActNamespaceiter, ActNamespace *);
};


class ActInstiter :
  public std::iterator<std::input_iterator_tag, ValueIdx *> {
  
  hash_bucket_t *b;
  int i;
  Scope *s;

 public:
  ActInstiter (Scope *s);
  ACTSTDITER(ActInstiter, ValueIdx *);
};

class ActConniter :
  public std::iterator<std::input_iterator_tag, act_connection *> {
  
  act_connection *start;
  act_connection *cur;

 public:
  ActConniter (act_connection *s);
  ACTSTDITER(ActConniter, act_connection *);
};

class ActTypeiter :
  public std::iterator<std::input_iterator_tag, Type *> {
  
  hash_bucket_t *b;
  int i;
  ActNamespace *top;

 public:
  ActTypeiter (ActNamespace *s);
  ACTSTDITER(ActTypeiter, Type *);
};

#undef ACTSTDITER


#endif /* __ITER_H__ */

