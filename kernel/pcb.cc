#include "pcb.h"
 
int PCB::find_empty(int type) {
    // Debug::printf("in find empty\n");
    if (type == FILE) {
        for (int i = 3; i < 10; i++) {
            if (files[i] == nullptr) return i;
        }
    }
    if (type == CHILD) {
        for (int i = 0; i < 10; i++) {
            if (children[i] == nullptr) return i;
        }
    }
    if (type == SEM) {
        for (int i = 0; i < 10; i++) {
            if (sems[i] == nullptr) return i;
        }
    }
    return -1;
}

bool PCB::check_idx(int type, int i) {
    if (type == FILE) {
        return i >= 0 && i < 10 && files[i] != nullptr;
    }
    if (type == CHILD) {
        return i >= 10 && i < 20 && children[i-10] != nullptr;
    }
    if (type == SEM) {
        return i >= 20 && i < 30 && sems[i-20] != nullptr;
    }
    return false;
}

PCB::PCB() {
    for (int i = 0; i < 10; i++) {
        files[i] = nullptr;
        children[i] = nullptr;
        sems[i] = nullptr;
    }
}

void PCB::copy(const PCB* rhs) {
    for (int i = 0; i < 10; i++) {
        files[i] = rhs->files[i];
        children[i] = nullptr;
        sems[i] = rhs->sems[i];
    }
}

PCB::~PCB() {}