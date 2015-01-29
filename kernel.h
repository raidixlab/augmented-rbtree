#ifndef _KERNEL_H
#define _KERNEL_H

#include "stddef.h"
//#include "compiler.h"

/**
 * * container_of - cast a member of a structure out to the containing structure
 * * @ptr: the pointer to the member.
 * * @type: the type of the container struct this is embedded in.
 * * @member: the name of the member within the struct.
 * *
 * */
#define container_of(ptr, type, member) ({ const typeof( ((type *)0)->member ) *__mptr = (ptr); (type *)( (char *)__mptr - offsetof(type,member) );})

#endif /* _KERNEL_H */
