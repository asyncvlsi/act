# A mini-scheme interpreter library

This library implements a mini-Scheme interpreter. See the README for a bit
of history about this implementation. It's designed to be used to augment a
a command-line interface, with minimal ineraction with an existing set of
commands.

## Supported core functions

The subset of Scheme supported is summarized below.

### Predicates

* `(boolean? x)` : `#t` if it is a Boolean, `#f` otherwise
* `(symbol? x)` : test for symbol
* `(list? x)` : test for list
* `(pair? x)` : test for dotted pair
* `(number? x)` : test for number
* `(string? x)` : test for string
* `(procedure? x)` : test for a procedure
* `(null? l)` : test for empty list
* `(zero? n)` : test if a number is zero
* `(positive? n)` : test if a number is positive
* `(negative? n)` : test if a number is negative
* `(eqv? a b)` : test if two objects are equivalent

### Basic list manipulation

* `(car l)` : return the first element
* `(cdr l)` : return the rest of the list
* `(cons a b)` : construct a list with `a` as the first element and `b` as the rest of the list
* `(list a b ...)` : return a list of the specified objects
* `(length l)` : returns the length of the list

### Math

* `(+ a b)` : returns sum
* `(- a b)` : returns difference
* `(* a b)` : returns product
* `(/ a b)` : returns quotient (floating-point)
* `(truncate f)` : returns integer part 

### Strings

* `(symbol->string s)` : convert symbol to string
* `(string->symbol s)` : convert string to symbol
* `(number->string s)` : convert number to string
* `(string->number s)` : convert string to number
* `(string-append a b)` : concatenate two strings
* `(string-length s)` : returns length of string
* `(string-compare s1 s2)` : compares two strings (ala strcmp)
* `(substring s i1 i2)`: returns a substring of a string
* `(string-ref s k)`: returns integer corresponding to character of `s` at position `k`

### Execution

* `(eval obj)`: evaluate object
* `(apply f l)`: apply `f` to `l`
* `(quote l1 l2 ...)`: return quoted list
* `(error str)` : abort evaluation and report error
* `(showframe)` : displays  the current frame
* `(collect-garbage)` : force garbage collection
* `(spawn pgm a1 a2...)` : spawn program with arguments (returns pid)
* `(wait pid)` : wait for spawned program to terminate
* `(builtin x)` : forces `x` to be a builtin function

### Functions and control

* `(lambda (a1 .. aN) body)` : return a lambda expression with arguments `a1` to `aN`
* `(define a obj)` : bind `a` to `obj`
* `(let ( (a1 obj) (a2 obj) ...) ret)` : bind symbols to objects, and return the result
* `(let* ( (a1 obj) (a2 obj) ...) ret)` : like `let`, but sequentially bind
* `(letrec ( (a1 obj) (a2 obj) ...) ret)` : like `let`, but permits recursive references
* `(cond (c1 res1) (c2 res2) .. (cN resN))` : evaluate condition, and execute result if condition is true; order is left to right.
* `(begin a1 a2 ... aN)` : evaluate arguments from left to right, and return the result of the last argument
* `(if c tcase fcase)` : evaluate condition `c`, evaluate `tcase` if it is true, `fcase` otherwise

### I/O

* `(load-scm fname)` : loads Scheme file
* `(save-scm fname obj)` : write object to file
* `(display-object obj)` : displays object
* `(print-object obj)` : prints object (no newline)

### Side-effects

* `(set-car! pair obj)` : updates the car field of the pair  (side-effects)
* `(set-cdr! pair obj)` : updates the cdr field of the pair  (side-effects)
* `(set! x v)` : modify binding of `x` to `v`
* `(string-set! s k v)`: sets character of `s` at position `k` to `v`


