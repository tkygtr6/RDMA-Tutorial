#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <mpi.h>
#include <assert.h>

#include "debug.h"
#include "ib.h"
#include "setup_ib.h"
#include "config.h"
#include "server.h"

void *server_thread (void *arg)
{
    int ret = 0;
    long        thread_id       = (long) arg;
    int         num_concurr_msgs= config_info.num_concurr_msgs;
    int         msg_size	 = config_info.msg_size;

    pthread_t   self;
    cpu_set_t   cpuset;

    int			 buf_offset   = 0;
    char		*buf_ptr      = ib_res.ib_buf;
    uint64_t             raddr_base   = ib_res.raddr;
    uint64_t             raddr        = raddr_base;
    volatile char       *msg_start    = buf_ptr;
    volatile char       *msg_end      = msg_start + msg_size - 1;
    int i;

    /* set thread affinity */
    CPU_ZERO (&cpuset);
    CPU_SET  ((int)thread_id, &cpuset);
    self = pthread_self ();
    ret  = pthread_setaffinity_np (self, sizeof(cpu_set_t), &cpuset);
    check (ret == 0, "thread[%ld]: failed to set thread affinity", thread_id);

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    printf("\t server received ACK from client\n");

    for(i = 0; i < num_concurr_msgs; i++){
        buf_offset = msg_size * i;
        msg_start  = buf_ptr + buf_offset;
        msg_end    = msg_start + msg_size - 1;
        raddr      = raddr_base + buf_offset;
        // printf("%d %d\n", *msg_start, (char) (i + 1));
        assert(*msg_start == (char) (i + 1));
        assert(*msg_end == (char) (i + 1));
    }

    pthread_exit ((void *)0);

 error:
    pthread_exit ((void *)-1);
}

int run_server ()
{
    int   ret         = 0;
    long  num_threads = 1;
    long  i           = 0;

    pthread_t           *threads = NULL;
    pthread_attr_t       attr;
    void                *status;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

    threads = (pthread_t *) calloc (num_threads, sizeof(pthread_t));
    check (threads != NULL, "Failed to allocate threads.");

    for (i = 0; i < num_threads; i++) {
	ret = pthread_create (&threads[i], &attr, server_thread, (void *)i);
	check (ret == 0, "Failed to create server_thread[%ld]", i);
    }

    bool thread_ret_normally = true;
    for (i = 0; i < num_threads; i++) {
        ret = pthread_join (threads[i], &status);
        check (ret == 0, "Failed to join thread[%ld].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            log ("server_thread[%ld]: failed to execute", i);
        }
    }

    if (thread_ret_normally == false) {
        goto error;
    }

    pthread_attr_destroy    (&attr);
    free (threads);

    return 0;

 error:
    if (threads != NULL) {
        free (threads);
    }
    pthread_attr_destroy    (&attr);
    
    return -1;
}
