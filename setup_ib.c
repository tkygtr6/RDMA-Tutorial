#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>

#include "mpi.h"
#include "ib.h"
#include "debug.h"
#include "config.h"
#include "setup_ib.h"

struct IBRes ib_res;

int connect_qp (struct ibv_qp *qp)
{
    int ret	      = 0, n = 0;
    struct QPInfo local_qp_info, remote_qp_info;

    local_qp_info.lid     = ib_res.port_attr.lid; 
    local_qp_info.qp_num  = qp->qp_num; 
    local_qp_info.rkey    = ib_res.mr->rkey;
    local_qp_info.raddr   = (uintptr_t) ib_res.ib_buf;
   
    if (config_info.is_server) {
        MPI_Send(&local_qp_info, sizeof(struct QPInfo), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&remote_qp_info, sizeof(struct QPInfo), MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else{
        MPI_Recv(&remote_qp_info, sizeof(struct QPInfo), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(&local_qp_info, sizeof(struct QPInfo), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }

    static int count = 0;
    FILE *fp;
    if (!count) {
        fp = fopen("QPs.txt", "w");
    } else{
        fp = fopen("QPs.txt", "a");
    }
    if (config_info.is_server) {
        fprintf(fp, "%d %d\n", local_qp_info.qp_num, remote_qp_info.qp_num);
    }
    fclose(fp);
    count++;

    /* store rkey and raddr info */
    ib_res.rkey  = remote_qp_info.rkey;
    ib_res.raddr = remote_qp_info.raddr;
    
    /* change QP state to RTS */    	
    ret = modify_qp_to_rts (qp, remote_qp_info.qp_num, 
			    remote_qp_info.lid);
    check (ret == 0, "Failed to modify qp to rts");

    log (LOG_SUB_HEADER, "IB Config");
    /*log ("\tqp[%"PRIu32"] <-> qp[%"PRIu32"]", 
	 ib_res.qp->qp_num, remote_qp_info.qp_num); */
    log ("\traddr[%"PRIu64"] <-> raddr[%"PRIu64"]", 
	 local_qp_info.raddr, ib_res.raddr);
    log (LOG_SUB_HEADER, "End of IB Config");

    return 0;

 error:
    return -1;
}

int setup_ib ()
{
    int	ret		         = 0;
    int i;
    struct ibv_device **dev_list = NULL;    
    memset (&ib_res, 0, sizeof(struct IBRes));

    ib_res.qp_num = config_info.qp_num;
    ib_res.qps = (struct ibv_qp **) malloc(sizeof(struct ibv_qp*) * ib_res.qp_num);

    /* get IB device list */
    int num_devices;
    dev_list = ibv_get_device_list(&num_devices);
    check(dev_list != NULL, "Failed to get ib device list.");

    /* create IB context */
    ib_res.ctx = ibv_open_device(dev_list[config_info.is_server ? 0:0]);
    check(ib_res.ctx != NULL, "Failed to open ib device.");

    /* allocate protection domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    check(ib_res.pd != NULL, "Failed to allocate protection domain.");

    /* query IB port attribute */
    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    check(ret == 0, "Failed to query IB port information.");
    
    /* register mr */
    ib_res.ib_buf_size = config_info.msg_size * (config_info.num_concurr_msgs) + 16384;
    ib_res.ib_buf      = (char *) memalign (4096, ib_res.ib_buf_size);
    check (ib_res.ib_buf != NULL, "Failed to allocate ib_buf");

    /*struct ibv_exp_reg_mr_in in;*/
    /*in.pd = ib_res.pd;*/

    /*in.addr = 0;*/
    /*in.length = IBV_IMPLICIT_MR_SIZE;*/
    /*_access = IBV_ACCESS_ON_DEMAND |*/
                    /*IBV_ACCESS_LOCAL_WRITE |*/
                    /*IBV_ACCESS_REMOTE_READ |*/
                    /*IBV_ACCESS_REMOTE_WRITE |*/
                    /*IBV_ACCESS_REMOTE_ATOMIC;*/
    /*in.comp_mask = 0;*/

    if ((config_info.is_server && config_info.odp_in_server) || 
            (!config_info.is_server && config_info.odp_in_receiver)) {
        ib_res.mr = ibv_reg_mr (ib_res.pd, ib_res.ib_buf, ib_res.ib_buf_size , IBV_ACCESS_ON_DEMAND |
                                                IBV_ACCESS_LOCAL_WRITE |
                                                IBV_ACCESS_REMOTE_READ |
                                                IBV_ACCESS_REMOTE_WRITE |
                                                IBV_ACCESS_REMOTE_ATOMIC);
    }else{
        ib_res.mr = ibv_reg_mr (ib_res.pd, ib_res.ib_buf, ib_res.ib_buf_size,
                                                IBV_ACCESS_LOCAL_WRITE |
                                                IBV_ACCESS_REMOTE_READ |
                                                IBV_ACCESS_REMOTE_WRITE |
                                                IBV_ACCESS_REMOTE_ATOMIC);
    }

    check (ib_res.mr != NULL, "Failed to register mr");
    
    /* initialize buffer */
    /* server:  [A, \0, 0, 1, 2, 3, 4, ...] */
    /* cliennt: [\0, A, \0, \0, \0, \0, \0, ...] */
    size_t buf_len = config_info.msg_size * (config_info.num_concurr_msgs);
    // memset (ib_res.ib_buf, '\0', buf_len);
    if (config_info.is_server) {
        // memset (ib_res.ib_buf, 'A', config_info.msg_size);
        for(i = 0; i < config_info.num_concurr_msgs; i++){
            memset (ib_res.ib_buf + config_info.msg_size * i, (char) i + 1, config_info.msg_size);
        }
    }else{
        memset (ib_res.ib_buf, 0, config_info.msg_size * config_info.num_concurr_msgs);
        // memset (ib_res.ib_buf + config_info.msg_size, 'A', config_info.msg_size);
    }

    /* query IB device attr */
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    check(ret==0, "Failed to query device");
    
    /* create cq */
    ib_res.cq = ibv_create_cq (ib_res.ctx, 131072, NULL, NULL, 0);
    check (ib_res.cq != NULL, "Failed to create cq");
    
    /* create qp */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .cap = {
            .max_send_wr = 8192,
            .max_recv_wr = 8192,
            .max_send_sge = 3,
            .max_recv_sge = 3,
        },
        .qp_type = IBV_QPT_RC,
    };

    for (i = 0; i < ib_res.qp_num; i++) {
        ib_res.qps[i] = ibv_create_qp (ib_res.pd, &qp_init_attr);
        check (ib_res.qps[i] != NULL, "Failed to create qp");

        /* connect QP */
        ret = connect_qp (ib_res.qps[i]);
        check (ret == 0, "Failed to connect qp");
    }

    ibv_free_device_list (dev_list);
    return 0;

 error:
    if (dev_list != NULL) {
	ibv_free_device_list (dev_list);
    }
    return -1;
}

void close_ib_connection ()
{
/*     if (ib_res.qp != NULL) {
	ibv_destroy_qp (ib_res.qp);
    } */

    if (ib_res.cq != NULL) {
	ibv_destroy_cq (ib_res.cq);
    }

    if (ib_res.mr != NULL) {
	ibv_dereg_mr (ib_res.mr);
    }

    if (ib_res.pd != NULL) {
        ibv_dealloc_pd (ib_res.pd);
    }

    if (ib_res.ctx != NULL) {
        ibv_close_device (ib_res.ctx);
    }

    if (ib_res.ib_buf != NULL) {
	free (ib_res.ib_buf);
    }
}
