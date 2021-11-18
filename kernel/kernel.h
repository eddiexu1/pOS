#ifndef _KERNEL_H_
#define _KERNEL_H_

#include "shared.h"
#include "ext2.h"

extern Shared<Ext2> fs;

void kernelMain(void);

#endif
