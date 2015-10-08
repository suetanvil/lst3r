/*
	Little Smalltalk, version two
	Written by Tim Budd, Oregon State University, July 1987

	environmental factors

	This include file gathers together environmental factors that
	are likely to change from one C compiler to another, or from
	one system to another.  Please refer to the installation
	notes for more information.

	for systems using the Make utility, the system name is set
	by the make script.
	other systems (such as the Mac) should put a define statement
	at the beginning of the file, as shown below
*/

#ifndef __ENV_H
#define __ENV_H

#include <limits.h>
#include <stdint.h>


// Data size configuration:
//
// Either use 1980's sizes for things (SMALL_MEM) or take (some)
// advantage of modern architectures (LARGE_MEM).

//#define LARGE_MEM
#define SMALL_MEM

// Size of the text buffer; must be large enough to hold both a line
// of input and the entire text of a method.
#define TEXT_BUFFER_SIZE 1024

// Max size of parsed token.
#define TOKEN_MAX 80

// Maximum size of the object table.  (Warning: there are still a few
// O(n) loops that walk the entire table.  Increasing this will make
// those slower.)
#define OBJECT_TABLE_MAX 6500


#if defined(LARGE_MEM)

//
// Underlying integer type for object.
//


typedef int32_t object_int;
static const int32_t OBJINT_MAX = (INT32_MAX >> 1);
static const int32_t OBJINT_MIN = (INT32_MIN >> 1);

typedef uint16_t count_int;
static const uint16_t COUNT_MAX = UINT16_MAX;
static const uint16_t COUNT_MIN = 0;

typedef int16_t size_int;
static const int16_t OBJSIZE_MAX = INT16_MAX;
static const int16_t OBJSIZE_MIN = INT16_MIN;

#elif defined(SMALL_MEM)

//
// Configuration for reduced memory footprint.
//

typedef int16_t object_int;
static const int16_t OBJINT_MAX = (INT16_MAX >> 1);
static const int16_t OBJINT_MIN = (INT16_MIN >> 1);

typedef uint8_t count_int;
static const uint8_t COUNT_MAX = UINT8_MAX;
static const uint8_t COUNT_MIN = 0;

// TO DO: look into making this 8 bits.  This could save us 6k of RAM,
// which is a lot on an embedded device. Currently, dictionary is too
// big.
typedef int16_t size_int;
static const int16_t OBJSIZE_MAX = INT16_MAX;
static const int16_t OBJSIZE_MIN = INT16_MIN;


#else
#   error "No memory model set."
#endif


#endif
