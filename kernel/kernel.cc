#include "debug.h"
#include "ide.h"
#include "ext2.h"
#include "elf.h"
#include "machine.h"
#include "libk.h"
#include "config.h"
#include "kernel.h"

Shared<Ext2> fs; // global file system for sys calls

Shared<Node> checkFile(const char* name, Shared<Node> node) {
    CHECK(node != nullptr);
    CHECK(node->is_file());
    // Debug::printf("*** file %s is ok\n",name);
    return node;
}

Shared<Node> getFile(Shared<Ext2> fs, Shared<Node> node, const char* name) {
    return checkFile(name,fs->find(node,name));
}

Shared<Node> checkDir(const char* name, Shared<Node> node) {
    // Debug::printf("*** checking %s\n",name);
    CHECK (node != nullptr);
    CHECK (node->is_dir());
    // Debug::printf("*** directory %s is ok\n",name);
    return node;
}

Shared<Node> getDir(Shared<Ext2> fs, Shared<Node> node, const char* name) {
    return checkDir(name,fs->find(node,name));
}

void kernelMain(void) {
    auto d = Shared<Ide>::make(1);
    // Debug::printf("*** mounting drive 1\n");
    fs = Shared<Ext2>::make(d);
    auto root = checkDir("/",fs->root);
    auto sbin = getDir(fs,root,"sbin");
    auto init = getFile(fs,sbin,"init");

    // Debug::printf("*** loading init\n");
    uint32_t e = ELF::load(init);
    // Debug::printf("*** entry %x\n",e);
    auto userEsp = K::min(kConfig.ioAPIC,kConfig.localAPIC);
    // Debug::printf("*** user esp %x\n",userEsp);
    // Current state:
    //     - %eip points somewhere in the middle of kernelMain
    //     - %cs contains kernelCS (CPL = 0)
    //     - %esp points in the middle of the thread's kernel stack
    //     - %ss contains kernelSS
    //     - %eflags has IF=1
    // Desired state:
    //     - %eip points at e
    //     - %cs contains userCS (CPL = 3)
    //     - %eflags continues to have IF=1
    //     - %esp points to the bottom of the user stack
    //     - %ss contain userSS
    // User mode will never "return" from switchToUser. It will
    // enter the kernel through interrupts, exceptions, and system
    // calls.
    userEsp -= 0x1000;
    *((uint32_t*)userEsp) = 1;
    *(((uint32_t*)userEsp) + 1) = (uint32_t)(((uint32_t*)userEsp) + 2);
    *(((uint32_t*)userEsp) + 2) = (uint32_t)(((uint32_t*)userEsp) + 3);
    *(((char*)userEsp) + 12) = 'i';
    *(((char*)userEsp) + 13) = 'n';
    *(((char*)userEsp) + 14) = 'i';
    *(((char*)userEsp) + 15) = 't';
    *(((char*)userEsp) + 16) = 0;
    switchToUser(e,userEsp,0);
    Debug::panic("*** implement switchToUser in machine.S\n");
}

