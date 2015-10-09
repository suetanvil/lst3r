# Little Smalltalk (Refactored) 3.1 #

This is a fork of Timothy Budd's Little Smalltalk version 3 with the
goal of modernizing the C code and making it more readable and
idiomatic.  At the same time, I have made every effort to keep the
memory footprint small for the benefit of anyone who wants to try to
port it to a sufficiently powerful embedded device.

The companion book to this program, *A Little Smalltalk*, can be
downloaded from
[here](http://live.exept.de/doc/books/ALittleSmalltalk/ALittleSmalltalk.pdf).

There is also a manual included with the source tree.

## Changes

1. The source code has been broken into modules with public interfaces
and hidden private parts.  (I.e. for each source file `foo.c`, there
is a header `foo.h` which declares its public interface; anything
using a part of `foo.c` does so by `#include`ing `foo.h`.)

2. Most global variables have been eliminated or made static
(i.e. local to their source files only).  In addition, many static
variables have been eliminated or made local.

3. Function declarations have been converted from the old K&R style to
the new (ca. 1990) ANSI style.  In addition, the source code has been
re-indented to one of the popular styles and tabs converted to spaces
in both the C and Smalltalk code.

4. The implicit assumption of a 16-bit int type as (hopefully) been
fixed.

5. The header `common.h` is now the central point for including
headers into the entire codebase.  This includes `env.h`, which now
contains (I hope) all of the platform-specific configuration,
`types.h` for common data types and `util.h` for short common
functions.

6. Eliminated most macros, replacing the function-like ones with
inline functions and the constant-like ones with enums or const
variables.  The few that are left are either useful as macros or too
difficult to remove.

7. Made names of things more idiomatic for C and in some cases
renamed things to something more understandable.

8. Removed a bunch of trailing spaces from the command prompt, mostly
to play nice with `rlwrap`.

9. Added the `-e` argument to `lst`.  This takes one string as an
argument and evaluates it as soon as the VM starts up.

10. Rewrote the Makefile to properly do dependencies and to run the
unit tests when invoked with the `test` target.  (It also now requires
GNU make and a compiler that implements gcc's `-MM` flag--sorry about
that.)

11. Simplified memory management.  Ripped out all of the clever stuff
and replaced it with `calloc` and `free`, since those have many more
engineer-years of work in them than anything in the VM code.  Also
made the post-load garbage collector a bit smarter.

12. Memory reference counts can no longer wrap; if they reach their
maximum, the are simply never decremented.  (Saving and reloading the
image will garbage-collect such "stuck" objects, though.)

13. Will now exit immediately in the unlikely case that `malloc`
fails.

14. When building the initial image, now also creates a file called
`object-map.txt` which contains a textual description of the image.
The function that does this (`printObjectTable()`) can also be called
from the debugger.  These dumps are invaluable for tracking down image
corruption bugs.

15. I/O function calls replaced with ones not (as) vulnerable to
buffer overflow.

16. Removed all uses of the keyword `register`.

17. Added a bunch of assertions of type and memory sanity.


# Stuff Remaining

1. The main interpreter function (`execute()`) is a huge,
incomprehensible ball of code that should be refactored into something
more readable *without a loss of performance on modern hardware and
compilers*.

2. Many primitives do not adequately check the type and value of their
arguments to ensure memory safety (e.g. `42 basicAt: 5`).  Some of the
more egregious cases are now protected by assertions but these holes
need to be closed and there needs to be a way to gracefully report the
error back to the user.

3. Should switch to the standard `:=` operator for assignments instead
of (or in addition to) the  `<-` operator.

4. MS-Windows support.  This is strictly a Unix program.  Fortunately
the code doesn't any exotic API calls so a standard C library should
be sufficient to build it as a Windows console app.

5. Probably lots of other things.

This is not a particularly *useful* Smalltalk implementation--if you
want one of those, try [Squeak](http://squeak.org/),
[Pharo](http://pharo.org/) or (if you can deal with proprietary
software) [Cincom Smalltalk](http://www.cincomsmalltalk.com/).
However, it's very small and very simple, so it could be the starting
point for some other interesting project.
