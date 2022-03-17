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
#ifndef __ITER_H__
#define __ITER_H__

#include <act/namespaces.h>
#include <iterator>

#define ACTSTDITER(name,rettype)		\
  name (const name & c);			\
  name& operator=(const name & c) = default;			\
  name& operator++();				\
  name operator++(int);				\
  bool operator==(const name& rhs) const;	\
  bool operator!=(const name& rhs) const;	\
  rettype operator*();				\
  name begin();					\
  name end()
    

class ActNamespaceiter 
#if __cplusplus < 201703L
/* deprecated in C++17 onward */
  : public std::iterator<std::input_iterator_tag, ActNamespace *>
#endif
{
  hash_bucket_t *b;
  int i;
  ActNamespace *top;

 public:
#if __cplusplus >= 201703L
  using iterator_category = std::input_iterator_tag;
  using value_type = ActNamespace *;
#endif  

  ActNamespaceiter (ActNamespace *ns);
  ACTSTDITER(ActNamespaceiter, ActNamespace *);
};


class ActInstiter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, ValueIdx *>
#endif
{
  hash_bucket_t *b;
  int i;
  Scope *s;

 public:
#if __cplusplus >= 201703L
  using iterator_category = std::input_iterator_tag;
  using value_type = ValueIdx *;
#endif  
  
  ActInstiter (Scope *s);
  ACTSTDITER(ActInstiter, ValueIdx *);
};

class ActConniter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, act_connection *>
#endif
{
  act_connection *start;
  act_connection *cur;

 public:
#if __cplusplus >= 201703L
  using iterator_category = std::input_iterator_tag;
  using value_type = act_connection *;
#endif  
  
  ActConniter (act_connection *s);
  ACTSTDITER(ActConniter, act_connection *);
};

class ActTypeiter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, Type *>
#endif
{
  hash_bucket_t *b;
  int i;
  ActNamespace *top;

 public:
#if __cplusplus >= 201703L
  using iterator_category = std::input_iterator_tag;
  using value_type = Type *;
#endif  
 
  ActTypeiter (ActNamespace *s);
  ACTSTDITER(ActTypeiter, Type *);
};

class ActUniqProcInstiter
#if __cplusplus < 201703L
  : public std::iterator<std::input_iterator_tag, ValueIdx *>
#endif
{
  hash_bucket_t *b;
  int i;
  Scope *s;

 public:
#if __cplusplus >= 201703L
  using iterator_category = std::input_iterator_tag;
  using value_type = ValueIdx *;
#endif  

  ActUniqProcInstiter (Scope *s);
  ACTSTDITER(ActUniqProcInstiter, ValueIdx *);
};


#undef ACTSTDITER


#endif /* __ITER_H__ */

