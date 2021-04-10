#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "machine.h"

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    MISSING();
    return 0;
}

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}
