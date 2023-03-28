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


/**
 * @file iter.h
 * 
 * @brief This contains the implementation of a number of C++
 * iterators to make it easier to walk through an ACT data structure
 */
    

/**
 * @class ActNamespaceiter
 *
 * @brief This is an iterator that, when passed an ActNamespace pointer,
 * allows you to iterate through nested namespaces within the
 * specified namespace.
 *
 * The iterator returns an ActNamespace pointer corresponding to the
 * subnamespace. 
 */
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


/**
 * @class ActInstiter
 *
 * @brief This is an iterator that, when passed a Scope pointer, 
 * allows you to iterate through all instances within the Scope. Note
 * that if two instances are connected to each other, they will both
 * be visited by the iterator.
 *
 * The iterator returns a ValueIdx pointer corresponding to the instance. 
 */
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

/**
 * @class ActConniter
 *
 * @brief This is an iterator that, when passed an act_connection
 * pointer, allows you to iterate through all the connections made to
 * this particular connection pointer.
 *
 * The iterator returns an act_connection pointer corresponding to the
 * connection visited.
 */
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

/**
 * @class ActTypeiter
 *
 * @brief This is an iterator that, when passed an ActNamespace
 * pointer, allows you to iterate through all user-defined types that
 * were defined within the namespace.
 *
 * The iterator returns a Type pointer that can be dynamically cast to
 * a user-defined type. These can be processes, channels, data types,
 * interface types, structures, or functions.
 */
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

/**
 * @class ActUniqProcInstiter
 *
 * @brief This is an iterator that, when passed a Scope pointer,
 * allows you to iterate through all the uinque process instances
 * within the scope.
 *
 * The iterator returns a ValueIdx pointer correspoding to the process
 * instance. Note that if this is an array instance, then not all
 * elements of the array may be unique (there could be connections to
 * them), so that must also be checked when iterating through the
 * array elements. This iterator is the standard way to iterate
 * through all design components that may have a circuit in them.
 */
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

