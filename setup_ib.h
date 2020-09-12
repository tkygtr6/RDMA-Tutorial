#ifndef SETUP_IB_H_
#define SETUP_IB_H_

#include <infiniband/verbs.h>

#define BUF_SIZE 100000000

struct IBRes {
    struct ibv_context		*ctx;
    struct ibv_pd		*pd;
    struct ibv_mr		*mr;
    struct ibv_cq		*cq;
    struct ibv_srq		*srq;
    struct ibv_qp		**qps;
    struct ibv_port_attr	 port_attr;
    struct ibv_device_attr	 dev_attr;

    char   *ib_buf;
    size_t  ib_buf_size;

    uint32_t rkey;
    uint64_t raddr;
    int qp_num;
};

extern struct IBRes ib_res;

int  setup_ib ();
void close_ib_connection ();

int  connect_qp ();

#endif /*setup_ib.h*/
