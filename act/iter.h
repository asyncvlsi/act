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

class ActNamespaceiter : public
      std::iterator<std::input_iterator_tag, ActNamespace *> {
  
  hash_bucket_t *b;
  int i;
  ActNamespace *top;

 public:
  ActNamespaceiter (ActNamespace *ns);
  ActNamespaceiter (const ActNamespaceiter &c);
  
  ActNamespaceiter& operator++();
  ActNamespaceiter operator++(int);
  bool operator==(const ActNamespaceiter& rhs) const;
  bool operator!=(const ActNamespaceiter& rhs) const;
  ActNamespace *operator*();
  ActNamespaceiter begin();
  ActNamespaceiter end();
};


class ActInstiter :
  public std::iterator<std::input_iterator_tag, ValueIdx *> {
  
  hash_bucket_t *b;
  int i;
  Scope *s;

 public:
  ActInstiter (Scope *s);
  ActInstiter (const ActInstiter &c);
  
  ActInstiter& operator++();
  ActInstiter operator++(int);
  bool operator==(const ActInstiter& rhs) const;
  bool operator!=(const ActInstiter& rhs) const;
  ValueIdx *operator*();
  ActInstiter begin();
  ActInstiter end();
};






#endif /* __ITER_H__ */
