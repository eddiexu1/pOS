#include "libc.h"

/**
 * PURPOSE:
 *  - Tests nested fork calls (fork() in children)
 *  - Tests simultaneous open and close calls in children
 *  - Tests file sharing semantics in children (closing a file in children does not close file in parent)
 *  - Tests waiting on children
 *  - Tests reading open files and writing to console
 * 
 * ASSUMPTIONS:
 *  - Parent and children share pointers to open files but have separate arrays
 *  - open() assigns next available fd
 *  - fd 0, 1, 2 are already open (console devices)
 *  - all other sys calls implemented correctly
**/

void one(int fd) {
    printf("*** fd = %d\n",fd);
    printf("*** len = %d\n",len(fd));

    cp(fd,2);
}

int main(int argc, char** argv) {
    int fd = open("/etc/data.txt",0);
    one(fd);

    int id = fork();

    if (id < 0) {
        printf("fork failed");
    } else if (id == 0) {
        /* child */
        printf("*** in child\n");
        int id2 = fork();
        if (id2 < 0) {
            printf("fork failed");
        } else if (id2 == 0) {
            printf("*** in child of child\n");
            int rc = execl("/sbin/shell","shell","a","b","c",0);
            printf("*** execl failed, rc = %d\n",rc);
        } else {
            uint32_t status = 42;
            wait(id2, &status);
            printf("*** back from wait %ld\n", status);
            close(3);
            int fd = open("/data/data.txt", 0);
            one(fd);
            cp(fd,1);
            return 124;
        }
    } else {
        /* parent */
        uint32_t status = 42;
        wait(id,&status);
        printf("*** back from wait %ld\n",status);

        close(3);
        int fd = open("/etc/panic.txt",0);
        one(fd);
        cp(fd,1);
    }

    shutdown();
    return 0;
}
