/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef __AML_DHP_COMMON_H_
#define __AML_DHP_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define TAG(a, b, c, d)\
    ((a << 24) | (b << 16) | (c << 8) | d)


/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

/*
 * enum debug_level_t - Expanded levels of debug messages for finer control.
 *
 * @DEBUG_LEVEL_NONE     : No debug messages will be printed.
 * @DEBUG_LEVEL_CRITICAL : Only critical errors or conditions are printed.
 * @DEBUG_LEVEL_ERROR    : Error messages and critical errors will be printed.
 * @DEBUG_LEVEL_WARNING  : Warning, error, and critical messages will be printed.
 * @DEBUG_LEVEL_NOTICE   : Notices (less severe than warnings) will also be printed.
 * @DEBUG_LEVEL_INFO     : Informational messages (high-level events) will be printed.
 * @DEBUG_LEVEL_VERBOSE  : Detailed messages for events that are normally less important.
 * @DEBUG_LEVEL_DEBUG    : Debug messages for developers (includes detailed information).
 * @DEBUG_LEVEL_TRACE    : Most granular level, useful for tracing function calls and fine details.
 *
 * This enum allows for more detailed control over the type of messages that can be printed,
 * with finer differentiation between levels like critical errors, verbose messages, and
 * tracing output.
 */
typedef enum {
    DEBUG_LEVEL_NONE = 0,      // No output
    DEBUG_LEVEL_CRITICAL,      // Critical errors
    DEBUG_LEVEL_ERROR,         // Errors
    DEBUG_LEVEL_WARNING,       // Warnings
    DEBUG_LEVEL_NOTICE,        // Notices (less severe than warnings)
    DEBUG_LEVEL_INFO,          // Informational messages
    DEBUG_LEVEL_VERBOSE,       // Verbose messages for more details
    DEBUG_LEVEL_DEBUG,         // Debug messages for developers
    DEBUG_LEVEL_TRACE          // Trace-level for function calls, very detailed
} debug_level_t;

/*
 * struct debug_context_t - Holds the current debug configuration.
 *
 * @level : The current debug level, which determines the verbosity of the output.
 * @output: The output stream where debug messages are written (e.g., stdout, file).
 *
 * This structure is used to store the global debug context, including the level of
 * debug messages that should be printed and the output stream (file or stdout).
 */
typedef struct {
    debug_level_t level;   // Current debug level
    FILE *output;          // Output stream (stdout, file, etc.)
} debug_context_t;

/*
 * Logging macros for each debug level.
 *
 * These macros wrap the debug_printf function, providing an easy way to log messages
 * at different debug levels using variadic arguments.
 */

/* Logs messages at the critical level. */
#define LOG_CRITICAL(...)   debug_printf(DEBUG_LEVEL_CRITICAL, __VA_ARGS__)

/* Logs messages at the error level. */
#define LOG_ERROR(...)      debug_printf(DEBUG_LEVEL_ERROR, __VA_ARGS__)

/* Logs messages at the warning level. */
#define LOG_WARNING(...)    debug_printf(DEBUG_LEVEL_WARNING, __VA_ARGS__)

/* Logs messages at the notice level (less severe than warnings). */
#define LOG_NOTICE(...)     debug_printf(DEBUG_LEVEL_NOTICE, __VA_ARGS__)

/* Logs informational messages (typically high-level events). */
#define LOG_INFO(...)       debug_printf(DEBUG_LEVEL_INFO, __VA_ARGS__)

/* Logs verbose messages for detailed system behavior. */
#define LOG_VERBOSE(...)    debug_printf(DEBUG_LEVEL_VERBOSE, __VA_ARGS__)

/* Logs messages useful for developers (debugging). */
#define LOG_DEBUG(...)      debug_printf(DEBUG_LEVEL_DEBUG, __VA_ARGS__)

/* Logs trace-level messages, ideal for tracking detailed execution flow. */
#define LOG_TRACE(...)      debug_printf(DEBUG_LEVEL_TRACE, __VA_ARGS__)


/*
 * debug_init() - Initializes the debug system.
 *
 * @level      : The initial debug level for controlling verbosity.
 * @output_file: The file where debug output will be written. If NULL, defaults to stdout.
 *
 * Initializes the global debug context with the specified debug level and output stream.
 * If an output file is provided, the function attempts to open the file for writing.
 * If opening the file fails, the output defaults to stdout.
 */
void debug_init(debug_level_t level, const char *output_file);
/*
 * debug_set_level() - Changes the current debug level.
 *
 * @level: The new debug level to set.
 *
 * Changes the current debug level, allowing dynamic control over which messages are printed.
 * Higher levels (e.g., DEBUG_LEVEL_DEBUG) enable more verbose output.
 */
void debug_set_level(debug_level_t level);

/*
 * debug_printf() - Conditional debug output based on the current debug level.
 *
 * @level : The debug level associated with the message.
 * @format: The format string (similar to printf) followed by optional arguments.
 *
 * Prints the formatted message to the specified output stream if the message's level is
 * less than or equal to the current global debug level. The function supports formatted
 * output similar to the standard printf function.
 */
void debug_printf(debug_level_t level, const char *format, ...);

/*
 * debug_cleanup() - Cleans up the debug context.
 *
 * Closes the debug output file if one was used, ensuring proper resource cleanup.
 * If stdout was used as the output, no action is taken.
 */

void debug_cleanup();
/*
 * LOG_XXX() - Macro to log an XXX message.
 *
 * A wrapper around debug_printf() that simplifies logging XXX-level messages.
 * This macro automatically passes DEBUG_LEVEL_ERROR to debug_printf.
 */

/* -------------------- Thread pool -------------------- */

/*
 * struct ThreadPoolTask - Represents a single task in the thread pool.
 *
 * @function : Function pointer representing the task to be executed.
 * @arg      : Argument to be passed to the task function.
 *
 * This structure encapsulates the task details, including the function
 * to be run by a thread and the corresponding argument.
 */
typedef struct {
    void (*function)(void *);  // Function pointer for the task
    void *arg;                 // Argument for the task
} ThreadPoolTask;

/*
 * struct ThreadPool - Manages a pool of worker threads and a task queue.
 *
 * @lock           : Mutex used to synchronize access to the task queue.
 * @cond_task      : Condition variable used to signal the presence of new tasks.
 * @cond_space     : Condition variable used to signal available space in the task queue.
 * @threads        : Array of worker threads that execute tasks.
 * @task_queue     : Circular queue for storing pending tasks.
 * @task_queue_size: Maximum number of tasks the queue can hold.
 * @task_count     : Current number of tasks in the queue.
 * @head           : Index of the next task to be processed in the queue.
 * @tail           : Index of the next empty slot in the queue for a new task.
 * @thread_count   : Number of worker threads in the thread pool.
 * @stop           : Flag to indicate whether the thread pool should stop processing tasks.
 *
 * This structure manages a pool of threads that continuously process tasks
 * from the task queue. The threads are synchronized to ensure correct access
 * to the shared task queue.
 */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  cond_task;
    pthread_cond_t  cond_space;
    pthread_t       *threads;
    ThreadPoolTask  *task_queue;
    int             task_queue_size;
    int             task_count;
    int             head;
    int             tail;
    int             thread_count;
    int             stop;
} ThreadPool;

/*
 * thread_pool_init() - Initialize the thread pool with worker threads and a task queue.
 *
 * @pool          : Pointer to the ThreadPool structure to be initialized.
 * @thread_count  : Number of worker threads to be created in the pool.
 * @task_queue_size: Maximum size of the task queue.
 *
 * Initializes the thread pool by creating the specified number of worker threads
 * and setting up the task queue with the given capacity. Returns 0 on success or
 * a negative error code on failure.
 *
 * Return: 0 on success, negative error code on failure.
 */
int thread_pool_init(ThreadPool *pool, int thread_count, int task_queue_size);

/*
 * thread_pool_destroy() - Destroy the thread pool and free resources.
 *
 * @pool: Pointer to the ThreadPool structure to be destroyed.
 *
 * Destroys the thread pool by stopping the worker threads and freeing all
 * allocated resources, including the task queue and the thread array.
 */
void thread_pool_destroy(ThreadPool *pool);

/*
 * thread_pool_add_task() - Add a task to the thread pool's task queue.
 *
 * @pool    : Pointer to the ThreadPool structure.
 * @function: Function pointer representing the task to be added.
 * @arg     : Argument to be passed to the task function.
 *
 * Adds a task to the task queue of the thread pool. If the queue is full,
 * the function will wait until space becomes available. Returns 0 on success
 * or a negative error code on failure.
 *
 * Return: 0 on success, negative error code on failure.
 */
int thread_pool_add_task(ThreadPool *pool, void (*function)(void *), void *arg);


/* -------------------- Dump File Definitions -------------------- */

/*
 * enum dump_format_t - Enum defining different formats for data dumping.
 *
 * @DUMP_FORMAT_HEX   : Dump data in hexadecimal format.
 * @DUMP_FORMAT_BIN   : Dump data in binary format.
 * @DUMP_FORMAT_ASCII : Dump data in ASCII format.
 * @DUMP_FORMAT_RAW   : Dump data as raw bytes without formatting.
 * @DUMP_FORMAT_CUSTOM: Dump data using a custom formatting function.
 *
 * This enumeration defines the different formats in which data can be dumped.
 * It provides a variety of options to control how the data is formatted during
 * the dump process.
 */
typedef enum {
    DUMP_FORMAT_HEX,
    DUMP_FORMAT_BIN,
    DUMP_FORMAT_ASCII,
    DUMP_FORMAT_RAW,
    DUMP_FORMAT_CUSTOM
} dump_format_t;

/*
 * dump_custom_formatter_t - Function pointer type for custom data formatting.
 *
 * @data: Pointer to the data to be formatted.
 * @size: Size of the data in bytes.
 * @out : Output file stream where the formatted data will be written.
 *
 * This function pointer type allows users to provide a custom function
 * for formatting data during the dump process. It gives full control over
 * how the data is presented in the output.
 */
typedef void (*dump_custom_formatter_t)(const void *data, size_t size, FILE *out);

/*
 * struct dump_file_t - File handling structure for dumping data.
 *
 * @file     : Pointer to the FILE structure for the file where data will be written.
 * @is_stdout: Flag indicating whether the output is being directed to stdout.
 *
 * This structure abstracts file handling during data dumping. It manages
 * whether data should be written to a file or printed to the console (stdout).
 */
typedef struct {
    FILE *file;
    int is_stdout;
} dump_file_t;

/*
 * dump_xxx_data() - Dump data using a raw/hex/bin/ascii/custom formatter function.
 *
 * @filename: Name of the file to write to. If NULL or empty, writes to stdout.
 * @data    : Pointer to the data buffer to be dumped.
 * @size    : Size of the data in bytes.
 * @append  : If non-zero, append to the file; otherwise, overwrite the file.
 * @cfunc   : Custom formatting function used to format the data before dumping.
 *
 * Dumps the provided data using the specified xxx formatting function.
 * The formatted data is written to the specified file or to stdout if no file
 * is specified. If append is set to 1, the function will append to the file;
 * otherwise, it will overwrite the file.
 */
void dump_raw_data(const char *filename, const uint8_t *data, size_t size, int append);
void dump_hex_data(const char *filename, const uint8_t *data, size_t size, int append);
void dump_bin_data(const char *filename, const uint8_t *data, size_t size, int append);
void dump_ascii_data(const char *filename, const uint8_t *data, size_t size, int append);
void dump_custom_data(const char *filename, const uint8_t *data, size_t size, int append, dump_custom_formatter_t cfunc);

/* -------------------- Time consumption statistics -------------------- */


/*
 * elapse_time_ms() - Calculates the elapsed time in milliseconds.
 *
 * @start: Pointer to the starting `timeval` structure.
 * @end: Pointer to the ending `timeval` structure.
 *
 * This function computes the time difference between two `timeval` structures,
 * returning the result in milliseconds. It calculates the time difference
 * in microseconds and then divides by 1000 to convert to milliseconds.
 *
 * Return: The elapsed time in milliseconds as an unsigned long.
 */
static inline unsigned long elapse_time_ms(struct timeval *start, struct timeval *end)
{
    return ((end->tv_sec - start->tv_sec) * 1000000 + (end->tv_usec - start->tv_usec)) / 1000;
}

/* -------------------- CRC -------------------- */

/*
 * crc_type_t - Enum representing different types of CRC algorithms.
 *
 * This enum allows the user to select the appropriate CRC algorithm
 * when initializing the CRC context. Currently, it supports:
 * - CRC32: 32-bit cyclic redundancy check.
 * - CRC16: 16-bit cyclic redundancy check.
 */
typedef enum {
    CRC32,
    CRC16
} crc_type_t;

/*
 * crc_ctx_t - Structure representing a CRC computation context.
 *
 * @type: Type of CRC algorithm to use (CRC32 or CRC16).
 * @crc: Current CRC32 value for CRC32 calculation.
 * @crc16: Current CRC16 value for CRC16 calculation.
 *
 * This structure holds the current state of the CRC computation. Depending on
 * the algorithm selected, either `crc` or `crc16` is used to store the intermediate
 * and final results.
 */
typedef struct {
    crc_type_t type;
    uint32_t crc;  // For CRC32
    uint16_t crc16; // For CRC16
} crc_ctx_t;

/*
 * crc_init() - Initializes a CRC context for the specified CRC type.
 *
 * @type: Type of CRC algorithm to use (CRC32 or CRC16).
 *
 * This function allocates and initializes a CRC context structure based on
 * the CRC algorithm specified by the `type` parameter. It also initializes
 * the CRC value to its default starting value (0xFFFFFFFF for CRC32, 0xFFFF for CRC16).
 *
 * Return: A pointer to the initialized `crc_ctx_t` structure, or NULL on failure.
 */
crc_ctx_t *crc_init(crc_type_t type);

/*
 * crc_compute() - Updates CRC with new data and returns the final CRC value.
 *
 * @ctx: Pointer to the CRC context structure.
 * @data: Pointer to the input data buffer.
 * @len: Length of the data buffer in bytes.
 *
 * This function combines both updating the CRC context with new data and
 * finalizing the calculation in one step. It processes the input data
 * and then returns the final CRC value.
 *
 * Return: The final CRC value (32-bit for CRC32, 16-bit for CRC16).
 */
uint32_t crc_compute(crc_ctx_t *ctx, const void *data, size_t len);

/*
 * crc_reset() - Resets the CRC context to its initial state.
 *
 * @ctx: Pointer to the CRC context structure.
 *
 * This function resets the CRC context to its initial value, allowing it to be reused
 * for a new CRC calculation without reallocating the context.
 */
void crc_reset(crc_ctx_t *ctx);

/*
 * crc_free() - Frees the CRC context.
 *
 * @ctx: Pointer to the CRC context structure.
 *
 * This function frees the memory associated with the CRC context structure,
 * ensuring that there are no memory leaks after the CRC calculation is complete.
 */
void crc_free(crc_ctx_t *ctx);

#endif //__AML_DHP_COMMON_H_

