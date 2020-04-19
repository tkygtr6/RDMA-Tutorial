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
int main (int argc, char *argv[])
{
    int	ret = 0;
    int myrank, nproc;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    if (nproc != 2) {
        printf(" Error: the number of processes should be 2.\n");
    }

	config_info.is_server	     = myrank ? false : true;

    if (config_info.is_server) {
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

        printf("size: %d\n", config_info.msg_size);
        printf("num_message: %d\n", config_info.num_concurr_msgs);
        printf("sleep_time: %d\n", config_info.sleep_time);
    }

    MPI_Bcast(&config_info.msg_size, sizeof(config_info.msg_size), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config_info.num_concurr_msgs, sizeof(config_info.msg_size), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&config_info.sleep_time, sizeof(config_info.sleep_time), MPI_BYTE, 0, MPI_COMM_WORLD);

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
