#ifndef _pcb_h_
#define _pcb_h_

#include "shared.h"
#include "ext2.h"
#include "semaphore.h"
#include "future.h"
#include "threads.h"

enum p_type {
    FILE,
    CHILD,
    SEM
};

struct OpenFile {
    Atomic<int> ref_count;
    Shared<Node> file;
    uint32_t offset;

    OpenFile(Shared<Node> file) : ref_count(0), file(file), offset(0) {}

};

struct ChildProcess {
    Atomic<int> ref_count;
    Future<int> status {};

    ChildProcess() : ref_count(0) {}
};

struct PCB {
    Shared<OpenFile> files[10];
    Shared<ChildProcess> children[10];
    Shared<Semaphore> sems[10];
    
    int find_empty(int type);

    bool check_idx(int type, int index);

    PCB();

    void copy(const PCB* rhs);

    virtual ~PCB();
};


#endif