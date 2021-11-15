#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "machine.h"

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    if (eax == 0) {
        Debug::shutdown();
    }
    if (eax == 1) {
        uint32_t* usr_stack = (uint32_t*) frame[3];
        size_t len = (size_t) usr_stack[3];
        char* buf = (char*) usr_stack[2];
        for (uint32_t i = 0; i < len; i++) {
            Debug::printf("%c", buf[i]);
        }
        return len;
    }
    return 0;
}

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}
