/*
	Little Smalltalk, version 2
	Written by Tim Budd, Oregon State University, July 1987
*/

#ifndef __INTERP_H__
#define __INTERP_H__

/*
	symbolic definitions for the bytecodes
*/
enum ByteCodes {
    BC_Extended = 0,
    BC_PushInstance = 1,
    BC_PushArgument = 2,
    BC_PushTemporary = 3,
    BC_PushLiteral = 4,
    BC_PushConstant = 5,
    BC_AssignInstance = 6,
    BC_AssignTemporary = 7,
    BC_MarkArguments = 8,
    BC_SendMessage = 9,
    BC_SendUnary = 10,
    BC_SendBinary = 11,
    BC_DoPrimitive = 13,
    BC_DoSpecial = 15,
};

/* a few constants that can be pushed by BC_PushConstant.  (The first
 * three are kind of redundant but I added them for consistency.) */
enum CommonConstants {
    CC_zero         = 0,        /* Literal 0 */
    CC_one          = 1,        /* Literal 1 */
    CC_two          = 2,        /* Literal 2 */
    
    CC_minusOne     = 3,		/* the value -1 */
    CC_contextConst = 4,		/* the current context */
    CC_nilConst     = 5,		/* the constant nil */
    CC_trueConst    = 6,		/* the constant true */
    CC_falseConst   = 7,		/* the constant false */
};

/* types of special instructions (opcode 15) */

enum SpecialByteCodes {
    SBC_SelfReturn  = 1,
    SBC_StackReturn = 2,
    SBC_Duplicate = 4,
    SBC_PopTop = 5,
    SBC_Branch = 6,
    SBC_BranchIfTrue = 7,
    SBC_BranchIfFalse = 8,
    SBC_AndBranch = 9,
    SBC_OrBranch = 10,
    SBC_SendToSuper = 11
};


// These are here because primitives.c needs them; nothing else should
// ever access these variables.
extern object processStack;
extern int linkPointer;

extern void flushCache(object messageToSend, object class);
extern boolean execute(object aProcess, int maxsteps);

#endif
