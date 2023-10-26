#ifndef MEDIAPROXY_H
#define MEDIAPROXY_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/device.h>

#define DEV_NAME "mediaproxy"
//TODO： 加debug sys

#define KFIFO_MAX_SIZE  1024 /*the number of elements(bytes) in the fifo, this must be a power of 2*/
#define MAX_SESSION 4

#define READ_TIME_OUT   10000   /* Time to wait for read in millisecond */

#define MEDIAPROXY_MAGIC 'm'
#define MEDIAPROXY_CONNECT _IOW(MEDIAPROXY_MAGIC, 0, int)
#define MEDIAPROXY_DISCONNECT _IOW(MEDIAPROXY_MAGIC, 1, int)
#define MEDIAPROXY_GET_CONSUMER_COUNT _IOW(MEDIAPROXY_MAGIC, 2, int)

#define MP_ROLE_STRING(role) \
    ((role == MP_ROLE_PRODUCER) ? "producer" : \
    (role == MP_ROLE_CONSUMER) ? "consumer" : \
    "invalid")

typedef enum {
	MP_ROLE_INVALID,
	MP_ROLE_PRODUCER,
	MP_ROLE_CONSUMER,
} mp_role_e;

union mediaproxy_ioctl_args
{
    mp_role_e role;
};


/*
* struct mediaproxy_session - the session for connect
* @session_id       index of the mp_session array
* @role             producer or consumer
* @lock             a pointer to the p_lock/c_lock within the dev structure
* @session_entry    a pointer to the producers/consumers session array within the dev
* @msg_kfifo        the FIFO in the session is used to store message data
*/
typedef struct mediaproxy_session {
    int session_id;
    mp_role_e role;
    struct mutex *lock;
    struct mediaproxy_session **session_entry;
    DECLARE_KFIFO_PTR(msg_kfifo, unsigned char);
} mp_session;

/*
* struct mediaproxy_dev - the metadata of the mediaproxy device node
* @major:                       the major device number of mediaproxy dev
* @has_consumer                 the consumer count
* @all_producer_fifo_empty      used to determine if there is data in the producer's fifo
* @producers:                   array for Storing Producer Sessions
* @consumers:                   array for Storing Consumers Sessions
* @p_lock                       mutex lock for producer
* @c_lock                       mutex lock for consumer
* @read_queue                   consumer wait queue for message
* @transfer_queue               wait queue fot transfer thread (mediaproxy_thread)
*/
struct mediaproxy_dev {
    int major;
    unsigned int has_consumer;
    bool all_producer_fifo_empty;
    mp_session* producers[MAX_SESSION];
    mp_session* consumers[MAX_SESSION];
    struct mutex p_lock;
    struct mutex c_lock;
    wait_queue_head_t read_queue;
    wait_queue_head_t transfer_queue;
};

static int mediaproxy_connect(mp_role_e role, mp_session *session);
static int mediaproxy_disconnect(mp_session *session);
static int mediaproxy_thread_fn(void* data);
static int copy_data_between_kfifo(mp_session **producer, mp_session **consumer, unsigned char *msg);
static int mediaproxy_get_consumer_count(void);

extern struct file_operations mediaproxy_fops;

#endif