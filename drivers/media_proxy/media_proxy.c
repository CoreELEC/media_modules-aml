#include "media_proxy.h"

static struct mediaproxy_dev *mediaproxy;
static struct class *mediaproxy_class;
static struct device *mediaproxy_device;
static struct task_struct *mediaproxy_thread;

static void mediaproxy_init(void)
{
    pr_info("mediaproxy init\n");
    mediaproxy = kzalloc(sizeof(struct mediaproxy_dev), GFP_KERNEL);
    if (!mediaproxy) {
        pr_err("Failed to malloc mediaproxy_dev\n");
        return;
    }
    mutex_init(&mediaproxy->p_lock);
    mutex_init(&mediaproxy->c_lock);
    init_waitqueue_head(&mediaproxy->read_queue);
    init_waitqueue_head(&mediaproxy->transfer_queue);
    mediaproxy->has_consumer = 0;
    mediaproxy->all_producer_fifo_empty = true;
    return;
}

static void mediaproxy_cleanup(void)
{
    int i;
    if (mediaproxy) {
        for (i = 0; i < MAX_SESSION; i++) {
            if (mediaproxy->producers[i]) {
                pr_info("mediaproxy_cleanup free producers[%d]\n", i);
                kfifo_free(&mediaproxy->producers[i]->msg_kfifo);
                kfree(mediaproxy->producers[i]);
                mediaproxy->producers[i] = NULL;
            }
            if (mediaproxy->consumers[i]) {
                pr_info("mediaproxy_cleanup free consumers[%d]\n", i);
                kfifo_free(&mediaproxy->consumers[i]->msg_kfifo);
                kfree(mediaproxy->consumers[i]);
                mediaproxy->consumers[i] = NULL;
            }
        }
        kfree(mediaproxy);
        mediaproxy = NULL;
    }
}

int mediaproxy_open(struct inode *inode, struct file *filp)
{
    int ret;
    mp_session *session;
    pr_info("Mediaproxy is opened\n");
    session = kzalloc(sizeof(*session), GFP_KERNEL);
    try_module_get(THIS_MODULE);
    if (IS_ERR(session))
        return PTR_ERR(session);
    session->session_id = -1;
    session->role = MP_ROLE_INVALID;
    session->subscribe_msg_type = 0xFF;
    session->lock = NULL;
    session->session_entry = NULL;
    filp->private_data = session;
    ret = kfifo_alloc(&session->msg_kfifo, KFIFO_MAX_SIZE, GFP_KERNEL);
    if (ret) {
        pr_err("Failed to alloc kfifo\n");
        return ret;
    }
    return 0;
}

int mediaproxy_release(struct inode *inode, struct file *filp)
{
    mp_session *session = filp->private_data;
    if (session->session_id > 0) {
        mutex_lock(session->lock);
        session->session_entry[session->session_id] = NULL;
        if (session->role == MP_ROLE_CONSUMER) {
            mediaproxy->has_consumer--;
        }
        mutex_lock(session->lock);
        pr_info("release session %s-%d\n", MP_ROLE_STRING(session->role), session->session_id);
    }
    kfifo_free(&session->msg_kfifo);
    kfree(session);
    filp->private_data = NULL;
    module_put(THIS_MODULE);
    return 0;
}


static int mediaproxy_connect(mp_role_e role, mp_session * session) {
    int i;
    if (!mediaproxy) {
        pr_err("Mediaproxy is not initialized\n");
        return -EFAULT;
    }
    if (session->session_id != -1) {
        pr_err("Session %s-%d is already connected\n", MP_ROLE_STRING(role), session->session_id);
        return -EEXIST;
    }
    switch (role)
    {
        case MP_ROLE_PRODUCER:
            mutex_lock(&mediaproxy->p_lock);
            for (i = 0; i < MAX_SESSION; i++) {
                if (!mediaproxy->producers[i]) {
                    session->session_id = i;
                    session->role = role;
                    session->lock = &mediaproxy->p_lock;
                    session->session_entry = mediaproxy->producers;
                    mediaproxy->producers[i] = session;
                    pr_info("Session %s-%d is connected\n", MP_ROLE_STRING(role), session->session_id);
                    break;
                }
            }
            mutex_unlock(&mediaproxy->p_lock);
            return 0;
        case MP_ROLE_CONSUMER:
            mutex_lock(&mediaproxy->c_lock);
            for (i = 0; i < MAX_SESSION; i++) {
                if (!mediaproxy->consumers[i]) {
                    session->session_id = i;
                    session->role = role;
                    session->lock = &mediaproxy->c_lock;
                    session->session_entry = mediaproxy->consumers;
                    mediaproxy->consumers[i] = session;
                    mediaproxy->has_consumer++;
                    wake_up_interruptible(&mediaproxy->transfer_queue);
                    pr_info("Session %s-%d is connected\n", MP_ROLE_STRING(role), session->session_id);
                    break;
                }
            }
            mutex_unlock(&mediaproxy->c_lock);
            return 0;
        case MP_ROLE_INVALID:
        default:
            pr_err("Invalid role\n");
            return -EINVAL;
    }
    pr_err("No available session\n");
    return -ENOMEM;
}

static int mediaproxy_disconnect(mp_session * session){
    mutex_lock(session->lock);
    session->session_entry[session->session_id] = NULL;
    if (session->role == MP_ROLE_CONSUMER) {
        mediaproxy->has_consumer--;
    }
    mutex_unlock(session->lock);
    pr_info("Session %s-%d is disconnected\n", MP_ROLE_STRING(session->role), session->session_id);
    session->session_id = -1;
    session->role = MP_ROLE_INVALID;
    session->subscribe_msg_type = 0xFF;
    session->lock = NULL;
    session->session_entry = NULL;
    return 0;
}

ssize_t mediaproxy_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    int ret;
    unsigned int copied;
    mp_session *session = filp->private_data;
    if (!mediaproxy) {
        pr_err("Mediaproxy is not initialized\n");
        return -EFAULT;
    }
    if (kfifo_is_empty(&session->msg_kfifo)) {
        if (filp->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        ret = wait_event_interruptible_timeout(mediaproxy->read_queue, !kfifo_is_empty(&session->msg_kfifo), msecs_to_jiffies(READ_TIME_OUT));
        if (ret == 0) {
            // timeout
            pr_err("read timeout\n");
            return -ETIMEDOUT;
        } else if (ret == -ERESTARTSYS) {
            return -ERESTARTSYS;
        }
    }

    if (kfifo_to_user(&session->msg_kfifo, buf, count, &copied)) {
        pr_err("consumer kfifo_to_user failed\n");
        return -EFAULT;
    }
    pr_info("Mediaproxy is read success, copied:%d, fifo len: %d\n", copied, kfifo_len(&session->msg_kfifo));
    return copied;
}

ssize_t mediaproxy_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    unsigned int copied;
    mp_session *session = filp->private_data;
    if (!mediaproxy) {
        pr_err("Mediaproxy is not initialized\n");
        return -EFAULT;
    }
    if (kfifo_from_user(&session->msg_kfifo, buf, count, &copied)) {
        pr_err("producer fifo_from_user failed\n");
        return -EFAULT;
    }
    mutex_lock(&mediaproxy->p_lock);
    mediaproxy->all_producer_fifo_empty = kfifo_is_empty(&session->msg_kfifo);
    wake_up_interruptible(&mediaproxy->transfer_queue);
    mutex_unlock(&mediaproxy->p_lock);
    pr_info("mediaproxy write success, copied size: %d fifo len: %d\n", copied, kfifo_len(&session->msg_kfifo));
    return copied;
}


static long mediaproxy_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int result = 0;
    union mediaproxy_ioctl_args args;
    mp_session* session = filp->private_data;

    if (_IOC_SIZE(cmd) > sizeof(args)) {
        pr_err("ioctl size is too large\n");
        return -EINVAL;
    }
    if ((void*)arg != NULL && copy_from_user(&args, (void *)arg, sizeof(args))) {
        pr_err("ioctl copy from user failed\n");
        return -EFAULT;
    }
    switch (cmd)
    {
    case MEDIAPROXY_CONNECT:
        pr_info("MEDIAPROXY_CONNECT\n");
        result = mediaproxy_connect(args.role, session);
        break;
    case MEDIAPROXY_DISCONNECT:
        pr_info("MEDIAPROXY_DISCONNECT\n");
        result = mediaproxy_disconnect(session);
        break;
    case MEDIAPROXY_GET_CONSUMER_COUNT:
        result = mediaproxy_get_consumer_count();
        break;
    case MEDIAPROXY_MSG_TYPE__SUBSCRIBE:
        session->subscribe_msg_type = args.subscribe_msg_type;
        break;
    default:
        pr_err("Unknown ioctl cmd\n");
        result = -EINVAL;
        break;
    }
    return result;
}

static int mediaproxy_thread_fn(void* data) {
    struct AmlVideoUserdata msg;
    while (!kthread_should_stop()) {
        if (wait_event_interruptible(mediaproxy->transfer_queue, ((mediaproxy->has_consumer > 0) && !mediaproxy->all_producer_fifo_empty) || kthread_should_stop())) {
            return -ERESTARTSYS;
        }
        if (copy_data_between_kfifo(mediaproxy->producers, mediaproxy->consumers, &msg) < 0) {
            return -ERESTARTSYS;
        }
    }
    return 0;
}

static int copy_data_between_kfifo(mp_session **producer, mp_session **consumer, struct AmlVideoUserdata *msg) {
    int i,j,data_copied;
    for (i = 0; i < MAX_SESSION; i++) {
        mutex_lock(&mediaproxy->p_lock);
        if (producer[i] && !kfifo_is_empty(&producer[i]->msg_kfifo)) {
            data_copied = kfifo_out(&producer[i]->msg_kfifo, msg, 1);
            mediaproxy->all_producer_fifo_empty = kfifo_is_empty(&producer[i]->msg_kfifo);
            for (j = 0; j < MAX_SESSION; j++) {
                mutex_lock(&mediaproxy->c_lock);
                if (consumer[j] && (msg->messageType & consumer[j]->subscribe_msg_type)) {
                    kfifo_in(&consumer[j]->msg_kfifo, msg, data_copied);
                }
                mutex_unlock(&mediaproxy->c_lock);
            }
            wake_up_interruptible(&mediaproxy->read_queue);
            memset(msg, 0, sizeof(struct AmlVideoUserdata));
        }
        mutex_unlock(&mediaproxy->p_lock);
    }
    return 0;
}

static int mediaproxy_get_consumer_count(void) {
    return mediaproxy->has_consumer;
}

struct file_operations mediaproxy_fops = {
    .owner = THIS_MODULE,
    .read = mediaproxy_read,
    .write = mediaproxy_write,
    .open = mediaproxy_open,
    .release = mediaproxy_release,
    .unlocked_ioctl = mediaproxy_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = mediaproxy_ioctl,
#endif
};

static int __init mediaproxy_init_module(void)
{
    int result;

    mediaproxy_init();

    mediaproxy_thread = kthread_create(mediaproxy_thread_fn, NULL, "mediaproxy_thread");
    if (IS_ERR(mediaproxy_thread)) {
        pr_err("Failed to create mediaproxy thread\n");
        kfree(mediaproxy);
        return PTR_ERR(mediaproxy_thread);
    }
    wake_up_process(mediaproxy_thread);

    result = register_chrdev(0, DEV_NAME, &mediaproxy_fops);
    if (result < 0) {
        pr_err("Failed to register mediaproxy device\n");
        mediaproxy_cleanup();
        return result;
    }
    mediaproxy->major = result;

    mediaproxy_class = class_create(THIS_MODULE, DEV_NAME);
    if (IS_ERR(mediaproxy_class)) {
        pr_err("Failed to create mediaproxy class\n");
        unregister_chrdev(mediaproxy->major, DEV_NAME);
        mediaproxy_cleanup();
        return PTR_ERR(mediaproxy_class);
    }

    mediaproxy_device = device_create(mediaproxy_class, NULL, MKDEV(mediaproxy->major, 0), NULL, DEV_NAME);
    if (IS_ERR(mediaproxy_device)) {
        pr_err("Failed to create mediaproxy device\n");
        class_destroy(mediaproxy_class);
        unregister_chrdev(mediaproxy->major, DEV_NAME);
        mediaproxy_cleanup();
        return PTR_ERR(mediaproxy_device);
    }
    pr_info("Mediaproxy device loaded with major number %d\n", mediaproxy->major);

    return 0;
}

static void __exit mediaproxy_exit_module(void)
{
    device_destroy(mediaproxy_class, MKDEV(mediaproxy->major, 0));
    class_destroy(mediaproxy_class);
    unregister_chrdev(mediaproxy->major, DEV_NAME);
    if (mediaproxy_thread) {
        kthread_stop(mediaproxy_thread);
        pr_info("Mediaproxy thread stopped\n");
    }
    mediaproxy_cleanup();
    pr_info("Mediaproxy device unloaded\n");
}

module_init(mediaproxy_init_module);
module_exit(mediaproxy_exit_module);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amlogic mediaproxy driver");
