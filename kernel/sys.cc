#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "machine.h"
#include "kernel.h"
#include "threads.h"
#include "pcb.h"
#include "physmem.h"
#include "elf.h"
#include "libk.h"

template <typename T>
int fork(uint32_t* usr_stack, PCB* pcb, T work) {
    using namespace gheith;
    int i = pcb->find_empty(CHILD);
    if (i == -1) return -1;
    pcb->children[i] = Shared<ChildProcess>::make();
    TCB* child_tcb = new TCBImpl<T>(work);
    child_tcb->pcb->copy(pcb);
    
    auto parent_tcb = gheith::current();
    // copy pd and pt
    for (int i = 512; i < 1024; i++) {
        auto parent_pde = parent_tcb->pd[i];
        if (parent_pde & 1) {
            child_tcb->pd[i] = PhysMem::alloc_frame() | (parent_pde & 0xFFF);
            auto parent_pt = (uint32_t*) (parent_pde & 0xFFFFF000);
            auto child_pt = (uint32_t*) (child_tcb->pd[i] & 0xFFFFF000);
            for (int j = 0; j < 1024; j++) {
                auto parent_pte = parent_pt[j];
                if (parent_pte & 1 && parent_pte & 4) {
                    // deep copy privates
                    child_pt[j] = PhysMem::alloc_frame() | (parent_pte & 0xFFF);
                    auto parent_p = (uint32_t*) (parent_pte & 0xFFFFF000);
                    auto child_p = (uint32_t*) (child_pt[j] & 0xFFFFF000);
                    memcpy(child_p, parent_p, PhysMem::FRAME_SIZE);
                } else {
                    child_pt[j] = parent_pte;
                }
            }
        }
    }

    // copy registers
    child_tcb->saveArea.ebx = usr_stack[1];
	child_tcb->saveArea.esi = usr_stack[2];
	child_tcb->saveArea.edi = usr_stack[3];
	child_tcb->saveArea.ebp = usr_stack[4];

    // self identity
    child_tcb->pid = i+10;
    child_tcb->parent = parent_tcb;
    Debug::printf("child idle? %d\n", child_tcb->isIdle);
    schedule(child_tcb);

    return i+10;
}

char *store_args(const char **args, uint32_t num_args) {
	uint32_t total_len = 0;
	for (uint32_t i = 0; i < num_args; i++) {
		uint32_t arg_len;
		for (arg_len = 0; args[i][arg_len]; arg_len++) {}
		total_len += arg_len + 1;
	}
	char *args_store = new char[total_len];
	uint32_t idx = 0;
	for (uint32_t i = 0; i < num_args; i++) {
		for (uint32_t j = 0; args[i][j]; j++)
			args_store[idx++] = args[i][j];
		args_store[idx++] = 0;
	}
	return args_store;
}

void copy_args(uint32_t *user_stack, const char *args_store, uint32_t num_args) {
	user_stack[0] = num_args;
	user_stack[1] = (uint32_t)&user_stack[2];
	char *string_base = (char*)(user_stack + 2 + num_args);
	uint32_t args_idx = 0;
	uint32_t string_idx = 0;
	for (uint32_t i = 0; i < num_args; i++) {
		user_stack[2 + i] = (uint32_t)(string_base + string_idx);
		for (; args_store[args_idx]; args_idx++)
			string_base[string_idx++] = args_store[args_idx];
		string_base[string_idx++] = 0;
		args_idx++;
	}
}

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    uint32_t* usr_stack = (uint32_t*) frame[3];
    auto pcb = gheith::current()->pcb;

    switch (eax) {
        case 0: { // exit
            for (int i = 0; i < 10; i++) {
                if (pcb->children[i] != nullptr) {
                    pcb->children[i]->status.get();
                    pcb->children[i] = nullptr;
                }
            }
            int status = (int) usr_stack[1];
            auto parent = gheith::current()->parent;
            auto my_pid = gheith::current()->pid - 10;
            if (parent != nullptr && parent->pcb->children[my_pid] != nullptr) {
                parent->pcb->children[my_pid]->status.set(status);
                stop();
            } 
            return -1;
        }
        case 1: { // write
            // Debug::printf("in write\n");
            int fd = (int) usr_stack[1];
            char* buf = (char*) usr_stack[2];
            size_t len = (size_t) usr_stack[3];
            
            // Debug::printf("%d %p %d %c\n", fd, buf, len, buf[0]);
            if (fd != 1 && fd != 2) return -1;
            for (uint32_t i = 0; i < len; i++) {
                Debug::printf("%c", buf[i]);
            }
            return len;
        }
        case 2: { // fork
            Debug::printf("in fork\n");
            uint32_t eip = frame[0];
            uint32_t esp = frame[3];
            return fork(usr_stack, pcb, [eip, esp]() { switchToUser(eip, esp, 0); });
        }
        case 3: { // sem
            uint32_t initial = (uint32_t) usr_stack[1];
            int i = pcb->find_empty(SEM);
            if (i == -1) return -1;
            pcb->sems[i] = Shared<Semaphore>::make(initial);
            return i+10;
        }
        case 4: { // up
            int id = (int) usr_stack[1];
            if (!pcb->check_idx(SEM, id)) return -1;
            pcb->sems[id-20]->up();
            return 0;
        }
        case 5: { // down
            int id = (int) usr_stack[1];
            if (!pcb->check_idx(SEM, id)) return -1;
            pcb->sems[id-20]->down();
            return 0;
        }
        case 6: { // close 
            int id = (int) usr_stack[1];
            if (id < 10) {
                if (!pcb->check_idx(FILE, id)) return -1;
                pcb->files[id] = nullptr;
            } else if (id < 20) {
                if (!pcb->check_idx(CHILD, id)) return -1;
                pcb->children[id-10] = nullptr;
            } else {
                if (!pcb->check_idx(SEM, id)) return -1;
                pcb->sems[id-20] = nullptr;
            }
            return 0;
        }
        case 7: // shutdown
            Debug::shutdown();
            return -1;
        case 8: { // wait
            int id = (int) usr_stack[1];
            uint32_t* ptr = (uint32_t*) usr_stack[2];

            if (!pcb->check_idx(CHILD, id)) return -1;
            *ptr = pcb->children[id-10]->status.get();
            return 0;
        }
        case 9: { // execl
            char* path = (char*) usr_stack[1];
            const char** args = (const char**) (usr_stack + 2);
            Shared<Node> node = fs->find(fs->root, path);
            if (node == nullptr) return -1;

            uint32_t num_args;
            for (num_args = 0; args[num_args]; num_args++) {}
            char *args_store = store_args(args, num_args);
            if (args_store == nullptr) return -1;
            uint32_t entry = ELF::load(node);
            uint32_t user_esp = K::min(kConfig.ioAPIC, kConfig.localAPIC);
            user_esp -= 0x1000;
            copy_args((uint32_t*)user_esp, args_store, num_args);
            delete [] args_store;
            switchToUser(entry, user_esp, 0);
            return -1;
        }
        case 10: { // open
            // Debug::printf("in open\n");
            char* path = (char*) usr_stack[1];

            Shared<Node> node = fs->find(fs->root, path);
            if (node->is_file()) {
                // Debug::printf("found file\n");
                Shared<OpenFile> f = Shared<OpenFile>::make(node);
                int i = pcb->find_empty(FILE);
                // Debug::printf("i = %d\n", i);
                if (i == -1) return -1;
                pcb->files[i] = f;
                return i;
            }
            return -1;
        }
        case 11: { // len
            int fd = (int) usr_stack[1];

            if (!pcb->check_idx(FILE, fd)) return -1;
            return pcb->files[fd]->file->size_in_bytes();
        }
        case 12: { // read
            // Debug::printf("in read\n");
            int fd = (int) usr_stack[1];
            char* buf = (char*) usr_stack[2];
            size_t n = (size_t) usr_stack[3];
            
            // special case: stdin

            // Debug::printf("%d %s %d\n", fd, buf, n);
            if (!pcb->check_idx(FILE, fd)) return -1;
            Shared<OpenFile> f = pcb->files[fd];
            uint32_t bytes_left = f->file->size_in_bytes() - f->offset;
            if (bytes_left >= n) { 
                f->file->read_all(f->offset, n, buf);
                f->offset += n;
                return n;
            } else { // don't go past EOF
                f->file->read_all(f->offset, bytes_left, buf);
                f->offset = f->file->size_in_bytes();
                return bytes_left;
            }   
        }
        case 13: { // seek
            int fd = (int) usr_stack[1];
            uint32_t off = (uint32_t) usr_stack[2];

            if (!pcb->check_idx(FILE, fd)) return -1;
            return pcb->files[fd]->offset = off;
        }
        default:
            return -1;
    }

}

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}
