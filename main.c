#include <stdio.h>

#include "debug.h"
#include "config.h"
#include "ib.h"
#include "setup_ib.h"
#include "client.h"
#include "server.h"
#include "mpi.h"

FILE	*log_fp	     = NULL;

int	init_env    ();
void	destroy_env ();


#include "unistd.h"
#include "stdlib.h"

int main (int argc, char *argv[])
{
    int	ret = 0;
    int myrank, nproc;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);
    printf("myrank: %d %d\n", myrank, nproc);

    if (nproc != 2) {
        printf(" Error: the number of processes should be 2.\n");
    }

    if (!myrank) {
        if (argc == 1) {
            config_info.msg_size = 100;
        } else {
            config_info.msg_size = atoi(argv[1]);
        }

        char *num_message_str;
        if (num_message_str = getenv("NUM_MESSAGE")) {
            config_info.num_concurr_msgs = atoi(num_message_str);
        } else {
            config_info.num_concurr_msgs = 100;
        }

        char *sleep_time_str;
        if (sleep_time_str = getenv("SLEEP_TIME")) {
            config_info.sleep_time = atoi(sleep_time_str);
        }else{
            config_info.sleep_time = 1000;
        }

        char *retry_cnt_str;
        if (retry_cnt_str = getenv("RETRY_COUNT")) {
            config_info.retry_cnt = atoi(retry_cnt_str);
        }else{
            config_info.retry_cnt = 0;
        }

        char *timeout_str;
        if (timeout_str = getenv("TIMEOUT")) {
            config_info.timeout = atoi(timeout_str);
        }else{
            config_info.timeout = 18;
        }

        char *rnr_timer_str;
        if (rnr_timer_str = getenv("RNR_TIMER")) {
            config_info.rnr_timer = atoi(rnr_timer_str);
        }else{
            config_info.rnr_timer = 13;
        }

        char *qp_num_str;
        if (qp_num_str = getenv("QP_NUM")) {
            config_info.qp_num = atoi(qp_num_str);
        }else{
            config_info.qp_num = 1;
        }

        char *odp_flag_str;
        int odp_flag;
        if (odp_flag_str = getenv("ODP")) {
            odp_flag = atoi(odp_flag_str);
        }else{
            odp_flag = 3;
        }
        config_info.odp_in_server = odp_flag & 0x1;
        config_info.odp_in_receiver = (odp_flag & 0x2) >> 1;

        printf("size: %d\n", config_info.msg_size);
        printf("num_message: %d\n", config_info.num_concurr_msgs);
        printf("sleep_time: %d\n", config_info.sleep_time);
        printf("timeout: %d\n", config_info.timeout);
        printf("retry_cnt: %d\n", config_info.retry_cnt);
        printf("rnr_timer: %d\n", config_info.rnr_timer);
        printf("qp_num: %d\n", config_info.qp_num);
        printf("ODP in server: %d\n", config_info.odp_in_server);
        printf("ODP in receiver: %d\n", config_info.odp_in_receiver);
    }

    MPI_Bcast(&config_info, sizeof(config_info), MPI_BYTE, 0, MPI_COMM_WORLD);
	config_info.is_server	     = myrank ? false : true;

    ret = init_env ();
    check (ret == 0, "Failed to init env");

    ret = setup_ib ();
    check (ret == 0, "Failed to setup IB");

    if (config_info.is_server) {
        ret = run_server ();
    } else {
        ret = run_client ();
    }
    check (ret == 0, "Failed to run workload");

 error:
    close_ib_connection ();
    destroy_env         ();
    MPI_Finalize();
    return ret;
}    

int init_env ()
{
    if (config_info.is_server) {
	log_fp = fopen ("server.log", "w");
    } else {
	log_fp = fopen ("client.log", "w");
    }
    check (log_fp != NULL, "Failed to open log file");

    log (LOG_HEADER, "IB Echo Server");
    print_config_info ();

    return 0;
 error:
    return -1;
}

void destroy_env ()
{
    log (LOG_HEADER, "Run Finished");
    if (log_fp != NULL) {
        fclose (log_fp);
    }
}
