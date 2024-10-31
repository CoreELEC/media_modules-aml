/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <getopt.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <linux/version.h>
#include <linux/dma-buf.h>

#include "aml_dhp_common.h"
#include "aml_dhp_daemon.h"
#include "aml_dhp_core.h"

#define DHP_DAEMON_VER TAG('v', 1, 0, 0)

volatile sig_atomic_t keep_running = 1;
u32 dump_data = 0;
u32 g_idx = -1;
crc_ctx_t *g_crc;

/*
 * _kbhit() - Detects keyboard input without blocking.
 *
 * This function checks if there is any pending input from the keyboard. It uses
 * the `termios` interface to configure the terminal to non-canonical mode, which
 * allows the program to detect keystrokes without requiring the user to press
 * the Enter key.
 *
 * Return: The number of bytes waiting in the input buffer (0 if none).
 */
static int _kbhit()
{
    static const int STDIN = 0;
    static bool initialized = false;
    int bytesWaiting;

    if (!initialized) {
        struct termios term;
        tcgetattr(STDIN, &term);           // Get current terminal attributes
        term.c_lflag &= ~ICANON;           // Disable canonical mode (non-blocking input)
        tcsetattr(STDIN, TCSANOW, &term);  // Apply the settings immediately
        setbuf(stdin, NULL);               // Disable input buffering
        initialized = true;                // Mark as initialized to avoid repeating setup
    }

    ioctl(STDIN, FIONREAD, &bytesWaiting);  // Check how many bytes are waiting in stdin

    return bytesWaiting;
}

/*
 * handle_signal() - Signal handler for graceful termination.
 *
 * @sig: The received signal (SIGINT or SIGTERM).
 *
 * This function handles termination signals by setting a flag to indicate
 * that the application should stop running. It logs the received signal for
 * debugging purposes.
 */
static void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
        LOG_INFO("Received termination signal (%d), exiting...\n", sig);
    }
}

void *dhp_dbuf_mmap(int fd, u32 len, int prot, int flags, u32 offset)
{
    void *addr = NULL;

    LOG_TRACE("%s: DBUF mmap, fd:%d, len:%u, prot:%x, flags:%x, off:%u\n",
        __func__, fd, len, prot, flags, offset);

    addr = mmap(NULL, len, prot, flags, fd, offset);
    if (addr == MAP_FAILED) {
        LOG_ERROR("dmabuf mmap failed: fd=%d len=%u offset=%u\n", fd, len, offset);
        return NULL;
    }
    return addr;
}

void dhp_dbuf_munmap(void *vaddr, u32 len)
{
    LOG_TRACE("%s: DBUF munmap, vaddr:%p, len:%u\n",
        __func__, vaddr, len);

    if (munmap(vaddr, len) == -1) {
        LOG_ERROR("dmabuf munmap failed: addr=%p len=%u\n", vaddr, len);
    }
}

void dhp_dbuf_sync(int fd, u32 flags)
{
    const struct dma_buf_sync sync = { flags };

    LOG_TRACE("%s: DBUF sync, flags:%x, len:%u\n",
        __func__, flags);

    if (ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync)) {
        LOG_ERROR("dmabuf sync failed: fd=%d\n", fd);
    }
}

void *dhp_page_mmap(void *priv, u32 pfn, u32 uncached)
{
    Device *dev = priv;
    struct aml_dhp_ioctl_data io = {0};

    io.mem.type = AML_MEM_TYPE_PFN;
    io.mem.pfn = pfn;
    io.mem.size = PAGE_SIZE;

    LOG_TRACE("%s: Page mmap, PFN:%lx, uncached:%u\n",
        __func__, pfn, uncached);

    if (dhp_dev_ioctl(dev, IOCTL_DHP_MMAP, &io)) {
        LOG_ERROR("page mmap failed: PFN=%x\n", pfn);
        return NULL;
    }

    return (void *)io.mem.uptr;
}

void *dhp_mem_mmap(void *priv, u64 addr, u32 size, u32 uncached)
{
    Device *dev = priv;
    struct aml_dhp_ioctl_data io = {0};

    io.mem.type = AML_MEM_TYPE_PHY_ADDR;
    io.mem.addr = addr;
    io.mem.size = size;
    io.mem.uncached = uncached;

    LOG_TRACE("%s: MEM mmap, addr:%llx, size:%u, uncached:%u\n",
        __func__, addr, size, uncached);

    if (dhp_dev_ioctl(dev, IOCTL_DHP_MMAP, &io)) {
        LOG_ERROR("mem mmap failed: addr=%lx size=%u\n", addr, size);
        return NULL;
    }

    return (void *)io.mem.uptr;
}

void dhp_mem_sync(void *priv, u64 addr, u32 size, u32 flags)
{
    Device *dev = priv;
    struct aml_dhp_ioctl_data io = {0};

    io.mem.type = AML_MEM_TYPE_PHY_ADDR;
    io.mem.addr = addr;
    io.mem.size = size;
    io.mem.syncflag = flags;

    LOG_TRACE("%s: MEM sync, addr:%llx, size:%u, flags:%x\n",
        __func__, addr, size, flags);

    if (dhp_dev_ioctl(dev, IOCTL_DHP_MEM_SYNC, &io)) {
        LOG_ERROR("mem sync failed: addr=%lx size=%u flags=%x\n", addr, size, flags);
    }
}

int dhp_mem_sgt_mmap(void *priv, u64 *uptr_array, u64 *pfn_array, u32 num, u32 uncached)
{
    Device *dev = priv;
    struct aml_dhp_ioctl_data io = {0};

    io.base.src.type = AML_MEM_TYPE_SG_TBL;
    io.base.src.sgt = (unsigned long long)pfn_array;
    io.base.src.size = num;

    io.base.dst.type = AML_MEM_TYPE_SG_TBL;
    io.base.dst.sgt = (unsigned long long)uptr_array;
    io.base.dst.size = num;
    io.base.dst.uncached = uncached;

    LOG_TRACE("%s: SGT mmap, PFNs:%u, uncached:%u\n",
        __func__, num, uncached);

    if (dhp_dev_ioctl(dev, IOCTL_DHP_SGT_MAP, &io)) {
        LOG_ERROR("SGT mmap failed: PFNs=%u\n", num);
        return -1;
    }

    return io.base.dst.payload;
}

void dhp_mem_sgt_sync(void *priv, u64 *pfn_array, u32 num, u32 flags)
{
    Device *dev = priv;
    struct aml_dhp_ioctl_data io = {0};

    io.mem.type = AML_MEM_TYPE_SG_TBL;
    io.mem.sgt = (unsigned long long)pfn_array;
    io.mem.size = num;
    io.mem.syncflag = flags;

    LOG_TRACE("%s: SGT sync, PFNs:%u, flags:%x\n", __func__, num, flags);

    if (dhp_dev_ioctl(dev, IOCTL_DHP_MEM_SYNC, &io)) {
        LOG_ERROR("SGT sync failed: PFNs=%u flags=%x\n", num, flags);
    }
}

void dhp_mem_munmap(void *priv, u8 *vaddr, u32 len)
{
    LOG_TRACE("%s: MEM munmmap, vaddr:%p, len:%u\n",
        __func__, vaddr, len);

    if (munmap(vaddr, len) == -1) {
        LOG_ERROR("munmap failed: addr=%p len=%u\n", vaddr, len);
    }
}

/*
 * device_manager_init() - Initializes the device manager by opening a device file.
 *
 * @dev: Pointer to the Device structure to be initialized.
 * @device_path: Path to the device file to open.
 *
 * This function opens the device at the specified path and sets up the `pollfd`
 * structure for polling events like data input, errors, and urgent data. It logs
 * the successful device opening or an error if it fails.
 *
 * Return: 0 on success, or -1 on failure.
 */
static int device_manager_init(Device *dev, const char *device_path)
{
    dev->fd = open(device_path, O_RDWR);
    if (dev->fd == -1) {
        LOG_ERROR("Failed to open device: %s\n", device_path);
        return -1;
    }

    dev->pfd.fd = dev->fd;
    dev->pfd.events = POLLIN | POLLERR | POLLPRI;  // Watch for input, errors, and priority data
    LOG_VERBOSE("Device opened successfully: %s\n", device_path);

    return 0;
}

/*
 * device_manager_close() - Closes the device and releases its resources.
 *
 * @dev: Pointer to the Device structure to close.
 *
 * This function closes the file descriptor associated with the device and logs
 * the device closure.
 */
static void device_manager_close(Device *dev)
{
    if (dev->fd != -1) {
        close(dev->fd);
        LOG_VERBOSE("Device closed (fd: %d).\n", dev->fd);
        dev->fd = -1;  // Mark the device as closed
    }
}

int dhp_dev_ioctl(Device *dev, int request, void *arg)
{
    if (!dev || dev->fd < 0) {
        LOG_ERROR("Invalid device (fd: %d).\n", dev ? dev->fd : -1);
        return -1;
    }

    if (ioctl(dev->fd, request, arg) < 0) {
        LOG_ERROR("IOCTL operation failed: fd=%d, request=%d\n", dev->fd, request);
        return -1;
    }

    return 0;
}

/*
 * __handle_event() - Handles an incoming event for the DHP manager.
 *
 * @mgr: Pointer to the DhpManager structure, containing the device and thread pool.
 *
 * This function processes events by fetching data from the device through an IOCTL call,
 * handling the received data, logging timing information, and closing the file descriptor.
 */
static void __handle_event(DhpManager *mgr)
{
    Device *dev = mgr->dev;
    struct aml_dhp_ioctl_data io;
    struct timeval t0, t1;

    // Capture the start time for performance measurement
    gettimeofday(&t0, NULL);

    // Fetch device FD and data using ioctl
    if (dhp_dev_ioctl(dev, IOCTL_DHP_GET_FD, &io)) {
        LOG_ERROR("%s: IOCTL_DHP_GET_FD failed, unable to get FD\n", __func__);
        return;
    }

    // Process the fetched data based on its type and log the data type
    if (aml_data_handle(dev, io.type, io.data)) {
        LOG_ERROR("%s: Failed to handle data, type: %d\n", __func__, io.type);
    } else {
        LOG_INFO("%s: Successfully handled data, type: %d\n", __func__, io.type);
    }

    // Capture the end time and calculate the elapsed time
    gettimeofday(&t1, NULL);
    LOG_VERBOSE("%s: Total time elapsed: %lu ms\n",
        __func__, elapse_time_ms(&t0, &t1));

    // Close the file descriptor after handling the data
    close(io.fd);

    LOG_DEBUG("%s: Event handling complete\n", __func__);
}

/*
 * device_poll() - Polls the device for incoming events.
 *
 * @dev           : Pointer to the Device structure to be polled.
 * @event_pending : Pointer to a boolean that indicates if an event is pending.
 * @timeout       : Poll timeout in milliseconds.
 *
 * Polls the device file descriptor to check for any incoming events within the given
 * timeout period. If an event is detected, it sets the event_pending flag to true.
 *
 * Return: Number of file descriptors with events or -1 on error.
 */
int device_poll(Device *dev, bool *event_pending, int timeout)
{
    int ret = poll(&dev->pfd, 1, timeout);

    if (ret > 0 && (dev->pfd.revents & (POLLIN | POLLRDNORM | POLLPRI))) {
        *event_pending = true;
    } else if (ret == 0) {
        LOG_VERBOSE("Poll timed out.\n");
    } else {
        perror("Poll error");
    }

    return ret;
}

/*
 * dhp_poll() - Polling loop for event handling.
 *
 * @mgr: Pointer to the DhpManager structure, containing the device and thread pool.
 *
 * This function polls the device for events and, if an event is detected, processes
 * it using the __handle_event function. It then re-queues itself as a task in the
 * thread pool, allowing continuous polling in the background.
 */
static void dhp_poll(DhpManager *mgr)
{
    bool event_pending = false;

    device_poll(mgr->dev, &event_pending, 2000);

    if (!keep_running) {
        return;
    }

    if (event_pending) {
        __handle_event(mgr);
    }

    // Re-queue dhp_poll to continue polling in the thread pool
    thread_pool_add_task(mgr->pool, (void *)dhp_poll, mgr);
}


/*
 * dhp_server_start() - Starts the DHP server logic by adding the poll task to the thread pool.
 *
 * @mgr: Pointer to the DhpManager structure, containing the thread pool and device.
 *
 * This function initializes the server by scheduling the polling logic (`dhp_poll`) as a task
 * in the thread pool. This allows the event handling logic to run continuously in the background.
 *
 * Return: 0 on success, or an error code from thread_pool_add_task on failure.
 */
int dhp_server_start(DhpManager *mgr)
{
    return thread_pool_add_task(mgr->pool, (void *)dhp_poll, mgr);
}

/*
 * dhp_context_release() - Releases the resources used by the DHP manager.
 *
 * @mgr: Pointer to the DhpManager structure.
 *
 * This function cleans up the resources allocated for the thread pool and the device manager.
 * It ensures that the thread pool and the device are properly closed and their memory is freed.
 */
static void dhp_context_release(DhpManager *mgr)
{
    if (mgr->pool) {
        thread_pool_destroy(mgr->pool);
        free(mgr->pool);
    }

    if (mgr->dev) {
        device_manager_close(mgr->dev);
        free(mgr->dev);
    }
}

/*
 * dhp_context_init() - Initializes the DHP manager context.
 *
 * @mgr: Pointer to the DhpManager structure to be initialized.
 *
 * This function allocates memory for the thread pool and device manager and initializes them.
 * If any error occurs during the initialization process, it releases any partially allocated
 * resources to avoid memory leaks.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int dhp_context_init(DhpManager *mgr)
{
    int ret = 0;

    mgr->pool = malloc(sizeof(ThreadPool));
    if (!mgr->pool) {
        perror("ThreadPool allocation failed");
        return -ENOMEM;
    }

    mgr->dev = malloc(sizeof(Device));
    if (!mgr->dev) {
        perror("Device allocation failed");
        free(mgr->pool);
        return -ENOMEM;
    }

    // Initialize thread pool
    ret = thread_pool_init(mgr->pool, 4, 10);
    if (ret) {
        perror("Error: Failed to initialize thread pool.");
        goto err;
    }

    // Initialize the device manager
    ret = device_manager_init(mgr->dev, DEVICE_FILE);
    if (ret) {
        perror("Error: Failed to initialize manager.");
        goto err;
    }

    return 0;
err:
    dhp_context_release(mgr);

    return ret;
}

static void usage(char **argv)
{
    printf("Usage: %s [options]\n", argv[0]);
    printf("Options:\n");
    printf("  -v | --version     Show version information\n");
    printf("  -d | --debug       Set log debug level\n");
    printf("  -s | --save        Dump data enable\n");
    printf("  -t | --target      Set target frame ID\n");
    printf("  -h | --help        Show this help message\n");
}

static const char *shortOpt = "v:d:s:t:h";

static struct option longOpt[] = {
    {"version", required_argument,  NULL, 'v'},
    {"debug",   required_argument,  NULL, 'd'},
    {"save",    required_argument,  NULL, 's'},
    {"target",  required_argument,  NULL, 't'},
    {"help",    no_argument,        NULL, 'h'},
    {NULL, 0, NULL, 0},
};

int main(int argc, char *argv[])
{
    DhpManager aMgr;
    struct sigaction sa;
    int versionCode = DHP_DAEMON_VER;
    int debugLevel = DEBUG_LEVEL_INFO;
    int optChar = 0;
    int optIdx = 0;
    int ret = 0;

    // Parse command-line arguments
    while ((optChar = getopt_long(argc, argv, shortOpt, longOpt, &optIdx)) != -1) {
        switch (optChar) {
        case 'v': {
            char ver[32] = { 0 };
            ver_to_string(versionCode, ver);
            printf("Version: %s\n", ver);
            exit(EXIT_SUCCESS);
        }
        case 'd':
            debugLevel = atoi(optarg);
            // Initialize debug with specified level and default output (stdout)
            debug_init(debugLevel, NULL);
            LOG_INFO("Set log level: %d\n", debugLevel);
            break;
        case 's':
            dump_data = atoi(optarg);
            if (!g_crc)
                g_crc = crc_init(CRC32);
            LOG_INFO("Set dump data mode: %x\n", dump_data);
            break;
        case 't':
            g_idx = atoi(optarg);
            LOG_INFO("Set target index: %u\n", g_idx);
            break;
        case 'h':
            usage(argv);
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }

    LOG_INFO("Using device: %s\n", DEVICE_FILE);

    // Set up signal handlers
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Error: Cannot handle signals");
        exit(EXIT_FAILURE);
    }

    // Initialize context and resources
    ret = dhp_context_init(&aMgr);
    if (ret) {
        perror("Error: Failed to initialize context.");
        exit(EXIT_FAILURE);
    }

    // Start the server
    ret = dhp_server_start(&aMgr);
    if (ret) {
        perror("Error: Failed to start server.");
        goto err;
    }

    // Main loop for handling key input
    while (keep_running) {
        if (_kbhit()) { // Check for keyboard input
            int chr = getchar();
            LOG_INFO("Key input detected: %d (quit: 'q')\n", chr);
            if (chr == 'q') {
                LOG_INFO("Quit command received. Exiting...\n");
                break;
            }
        } else {
            usleep(10000); // Avoid busy waiting, sleep for 10ms
        }
    }

err:
    // Clean up resources
    dhp_context_release(&aMgr);

    // Cleanup debug
    debug_cleanup();

    if (g_crc) {
        crc_free(g_crc);
        g_crc = NULL;
    }

    printf("Main thread exiting...\n");

    return 0;
}

