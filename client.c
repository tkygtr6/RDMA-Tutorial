#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <assert.h>
#include <mpi.h>

#include "debug.h"
#include "config.h"
#include "setup_ib.h"
#include "ib.h"
#include "client.h"

void *client_thread_func (void *arg)
{
    int ret = 0;
    int i, j;
    long	thread_id	 = (long) arg;
    int         num_concurr_msgs= config_info.num_concurr_msgs;
    int         msg_size	 = config_info.msg_size;

    pthread_t   self;
    cpu_set_t   cpuset;

    int                  num_wc       = 20;
    struct ibv_qp	**qps	      = ib_res.qps;
    struct ibv_cq       *cq           = ib_res.cq;
    struct ibv_wc       *wc           = NULL;
    uint32_t             lkey	      = ib_res.mr->lkey;
    char		*buf_ptr      = ib_res.ib_buf;
    int			 buf_offset   = 0;
    uint32_t             rkey	      = ib_res.rkey;
    uint64_t             raddr_base   = ib_res.raddr;
    uint64_t             raddr        = raddr_base;
    volatile char       *msg_start    = buf_ptr;
    volatile char       *msg_end      = msg_start + msg_size - 1;

    wc = (struct ibv_wc *) calloc (num_wc, sizeof(struct ibv_wc));
    check (wc != NULL, "thread[%ld]: failed to allocate wc.", thread_id);

    /* set thread affinity */
    CPU_ZERO (&cpuset);
    CPU_SET  ((int)thread_id, &cpuset);
    self = pthread_self ();
    ret  = pthread_setaffinity_np (self, sizeof(cpu_set_t), &cpuset);
    check (ret == 0, "thread[%ld]: failed to set thread affinity", thread_id);

    MPI_Barrier(MPI_COMM_WORLD);
    usleep(500000);

    int sum = 0;
    int num_finished;

    struct timeval time1;
    struct timeval time2;

    gettimeofday(&time1, NULL);

    for(i = 0; i < num_concurr_msgs; i++){
        ret = post_read_signaled (msg_size, lkey, 0, ib_res.qps[i % ib_res.qp_num], buf_ptr + msg_size * i, raddr + msg_size * i, rkey);
    }

    printf("Wait phase begin\n");
    while(sum < num_concurr_msgs){
        num_finished = ibv_poll_cq (cq, num_wc, wc);
        sum += num_finished;
        for(j = 0; j < num_finished; j++){
            if (wc[j].status != IBV_WC_SUCCESS){
                printf("Error: ib_poll_cq failed. status: %d i = %d, sum = %d\n", wc->status, i, sum);
                if (wc[j].status == IBV_WC_RETRY_EXC_ERR){
                    printf("RETRANSMISSION ERROR\n");
                    exit(1);
                }
            }
        }
        /*printf("remaining: %d\n", num_concurr_msgs - sum);*/
    }
    gettimeofday(&time2, NULL);
    printf("Time: %f[s]\n", time2.tv_sec - time1.tv_sec +  (float)(time2.tv_usec - time1.tv_usec) / 1000000);

    usleep(500000);
    MPI_Barrier(MPI_COMM_WORLD);

    for(i = 0; i < num_concurr_msgs; i++){
        buf_offset = msg_size * i;
        msg_start  = buf_ptr + buf_offset;
        msg_end    = msg_start + msg_size - 1;
        raddr      = raddr_base + buf_offset;
        // printf("%d %d\n", *msg_start, (char) (i + 1));
        assert(*msg_start == (char) (i + 1));
        assert(*msg_end == (char) (i + 1));
    }
    printf("\t client all finishes\n");

    free (wc);
    pthread_exit ((void *)0);

 error:
    if (wc != NULL) {
    	free (wc);
    }
    pthread_exit ((void *)-1);
}

int run_client ()
{
    int		ret	    = 0;
    long	num_threads = 1;
    long	i	    = 0;
    
    pthread_t	   *client_threads = NULL;
    pthread_attr_t  attr;
    void	   *status;

    log (LOG_SUB_HEADER, "Run Client");
    
    /* initialize threads */
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

    client_threads = (pthread_t *) calloc (num_threads, sizeof(pthread_t));
    check (client_threads != NULL, "Failed to allocate client_threads.");

    for (i = 0; i < num_threads; i++) {
	ret = pthread_create (&client_threads[i], &attr, 
			      client_thread_func, (void *)i);
	check (ret == 0, "Failed to create client_thread[%ld]", i);
    }

    bool thread_ret_normally = true;
    for (i = 0; i < num_threads; i++) {
	ret = pthread_join (client_threads[i], &status);
	check (ret == 0, "Failed to join client_thread[%ld].", i);
	if ((long)status != 0) {
            thread_ret_normally = false;
            log ("thread[%ld]: failed to execute", i);
        }
    }

    if (thread_ret_normally == false) {
        goto error;
    }

    pthread_attr_destroy (&attr);
    free (client_threads);
    return 0;

 error:
    if (client_threads != NULL) {
        free (client_threads);
    }
    
    pthread_attr_destroy (&attr);
    return -1;
}
