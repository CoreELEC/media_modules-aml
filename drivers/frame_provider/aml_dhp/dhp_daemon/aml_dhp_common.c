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
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "aml_dhp_common.h"

static debug_context_t global_debug_ctx = { DEBUG_LEVEL_INFO, NULL };

void debug_init(debug_level_t level, const char *output_file) {
    global_debug_ctx.level = level;
    if (output_file) {
        global_debug_ctx.output = fopen(output_file, "w");
        if (!global_debug_ctx.output) {
            global_debug_ctx.output = stdout;  // Fallback to stdout if file fails
        }
    } else {
        global_debug_ctx.output = stdout;      // Default to stdout
    }
}

void debug_set_level(debug_level_t level) {
    global_debug_ctx.level = level;
}

void debug_printf(debug_level_t level, const char *format, ...) {
    if (level <= global_debug_ctx.level && global_debug_ctx.output) {
        va_list args;
        va_start(args, format);
        vfprintf(global_debug_ctx.output, format, args);
        va_end(args);
        fflush(global_debug_ctx.output);  // Ensure immediate output
    }
}

void debug_cleanup() {
    if (global_debug_ctx.output && global_debug_ctx.output != stdout) {
        fclose(global_debug_ctx.output);
    }
}

/* -------------------- Thread pool -------------------- */

/*
 * thread_pool_worker() - Function executed by each worker thread in the thread pool.
 *
 * @arg: Pointer to the ThreadPool structure passed during thread creation.
 *
 * This function represents the main loop for a worker thread in the pool.
 * The worker thread waits for new tasks to become available in the task queue.
 * When a task is available, it removes the task from the queue, executes it,
 * and signals any waiting threads that space is available in the queue.
 * The thread exits when the thread pool is stopping and no tasks are left.
 *
 * The thread checks the pool's stop flag to determine if it should terminate.
 * If the stop flag is set and no tasks are left in the queue, the thread
 * exits its loop and terminates.
 *
 * Return: NULL, since the function always exits the thread using pthread_exit().
 */
static void *thread_pool_worker(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        // Wait for tasks if the queue is empty and the pool hasn't been asked to stop
        while (pool->task_count == 0 && !pool->stop) {
            pthread_cond_wait(&pool->cond_task, &pool->lock);
        }

        // Exit if the pool is stopping and there are no tasks left
        if (pool->stop && pool->task_count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        // Fetch the next task from the queue
        ThreadPoolTask task = pool->task_queue[pool->head];
        pool->head = (pool->head + 1) % pool->task_queue_size;
        pool->task_count--;

        // Signal that there is space for new tasks
        pthread_cond_signal(&pool->cond_space);
        pthread_mutex_unlock(&pool->lock);

        // Execute the task
        if (task.function) {
            task.function(task.arg);
        }
    }

    pthread_exit(NULL);

    return NULL;
}

int thread_pool_init(ThreadPool *pool, int thread_count, int task_queue_size)
{
    int ret = 0;

    // Initialize thread pool structure
    pool->thread_count = thread_count;
    pool->task_queue_size = task_queue_size;
    pool->task_count = 0;
    pool->head = 0;
    pool->tail = 0;
    pool->stop = 0;

    // Allocate memory for the threads
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        perror("ThreadPool allocation threads failed.");
        return -ENOMEM;
    }

    // Allocate memory for the task queue
    pool->task_queue = malloc(sizeof(ThreadPoolTask) * task_queue_size);
    if (!pool->task_queue) {
        perror("ThreadPool allocation task_queue failed.");
        free(pool->threads);
        return -ENOMEM;
    }

    // Initialize mutex and condition variables
    if ((ret = pthread_mutex_init(&pool->lock, NULL)) != 0 ||
        (ret = pthread_cond_init(&pool->cond_task, NULL)) != 0 ||
        (ret = pthread_cond_init(&pool->cond_space, NULL)) != 0) {
        perror("ThreadPool mutex or cond init failed.");
        if (ret == 0)
            pthread_mutex_destroy(&pool->lock);
        if (ret == 0)
            pthread_cond_destroy(&pool->cond_task);
        if (ret == 0)
            pthread_cond_destroy(&pool->cond_space);
        free(pool->threads);
        free(pool->task_queue);

        return ret;
    }

    // Create worker threads
    int created_threads_count = 0;
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_pool_worker, (void *)pool) != 0) {
            perror("ThreadPool thread creation failed.");
            pool->stop = 1;
            for (int j = 0; j < created_threads_count; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            thread_pool_destroy(pool);
            return -1;
        }
        created_threads_count++;
    }

    return 0;
}

void thread_pool_destroy(ThreadPool *pool)
{
    if (!pool || !pool->threads)
        return;

    // Stop the thread pool and wake up any waiting threads
    pthread_mutex_lock(&pool->lock);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->cond_task);
    pthread_cond_broadcast(&pool->cond_space);
    pthread_mutex_unlock(&pool->lock);

    // Join all worker threads
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // Clean up resources
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond_task);
    pthread_cond_destroy(&pool->cond_space);

    free(pool->threads);
    free(pool->task_queue);
}

int thread_pool_add_task(ThreadPool *pool, void (*function)(void *), void *arg)
{
    if (!function) {
        LOG_ERROR("Task function is NULL\n");
        return -1;
    }

    pthread_mutex_lock(&pool->lock);

    // Wait for space in the queue if it's full
    while (pool->task_count == pool->task_queue_size && !pool->stop) {
        pthread_cond_wait(&pool->cond_space, &pool->lock);
    }

    if (pool->stop) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    // Add the task to the queue
    pool->task_queue[pool->tail].function = function;
    pool->task_queue[pool->tail].arg = arg;
    pool->tail = (pool->tail + 1) % pool->task_queue_size;
    pool->task_count++;

    // Signal that a task is available
    pthread_cond_signal(&pool->cond_task);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}

/* -------------------- Dump File Definitions -------------------- */

static int dump_file_init(dump_file_t *dump_file, const char *filename, const char *mode)
{
    if (strcmp(filename, "stdout") == 0) {
        dump_file->file = stdout;
        dump_file->is_stdout = 1;
    } else {
        dump_file->file = fopen(filename, mode);
        dump_file->is_stdout = 0;
        if (dump_file->file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

static void dump_file_close(dump_file_t *dump_file)
{
    if (dump_file->file != NULL && !dump_file->is_stdout) {
        fclose(dump_file->file);
    }
    dump_file->file = NULL;
}

static void dump_file_write(dump_file_t *dump_file, const void *data, size_t size, dump_format_t format, dump_custom_formatter_t cfunc)
{
    if (dump_file->file == NULL) {
        LOG_ERROR("Invalid file handle.\n");
        return;
    }

    switch (format) {
    case DUMP_FORMAT_HEX:
        for (size_t i = 0; i < size; i++) {
            fprintf(dump_file->file, "%02X ", ((const uint8_t *)data)[i]);
            if ((i + 1) % 16 == 0) {
                fprintf(dump_file->file, "\n");
            }
        }
        fprintf(dump_file->file, "\n");
        break;

    case DUMP_FORMAT_BIN:
        for (size_t i = 0; i < size; i++) {
            for (int j = 7; j >= 0; j--) {
                fprintf(dump_file->file, "%c", (((const uint8_t *)data)[i] & (1 << j)) ? '1' : '0');
            }
            fprintf(dump_file->file, " ");
            if ((i + 1) % 8 == 0) {
                fprintf(dump_file->file, "\n");
            }
        }
        fprintf(dump_file->file, "\n");
        break;

    case DUMP_FORMAT_ASCII:
        for (size_t i = 0; i < size; i++) {
            fprintf(dump_file->file, "%c", ((const uint8_t *)data)[i]);
        }
        fprintf(dump_file->file, "\n");
        break;

    case DUMP_FORMAT_RAW:
        // Directly write raw data to the file
        if (fwrite(data, 1, size, dump_file->file) != size) {
            fprintf(stderr, "Failed to write raw data to file.\n");
        }
        break;

    case DUMP_FORMAT_CUSTOM:
        if (cfunc != NULL) {
            cfunc(data, size, dump_file->file);
        } else {
            fprintf(stderr, "Custom formatter is NULL.\n");
        }
        break;

    default:
        fprintf(stderr, "Unknown dump format.\n");
        break;
    }
}

static void __dump_data(const char *filename, const uint8_t *data, size_t size, int append, dump_format_t fmt, dump_custom_formatter_t cfunc)
{
    dump_file_t dump_file;
    const char *mode = append ? "ab" : "wb";

    // Initialize the file for writing data
    if (dump_file_init(&dump_file, filename, mode) != 0) {
        return;
    }

    // Write the data
    dump_file_write(&dump_file, data, size, fmt, cfunc);

    // Close the file
    dump_file_close(&dump_file);
}

void dump_raw_data(const char *filename, const uint8_t *data, size_t size, int append)
{
    __dump_data(filename, data, size, append, DUMP_FORMAT_RAW, NULL);
}

void dump_hex_data(const char *filename, const uint8_t *data, size_t size, int append)
{
    __dump_data(filename, data, size, append, DUMP_FORMAT_HEX, NULL);
}

void dump_bin_data(const char *filename, const uint8_t *data, size_t size, int append)
{
    __dump_data(filename, data, size, append, DUMP_FORMAT_BIN, NULL);
}

void dump_ascii_data(const char *filename, const uint8_t *data, size_t size, int append)
{
    __dump_data(filename, data, size, append, DUMP_FORMAT_ASCII, NULL);
}

void dump_custom_data(const char *filename, const uint8_t *data, size_t size, int append, dump_custom_formatter_t cfunc)
{
    __dump_data(filename, data, size, append, DUMP_FORMAT_CUSTOM, cfunc);
}

/* -------------------- CRC -------------------- */

// Precomputed CRC-32 table for faster calculations
static uint32_t crc32_table[256];

/*
 * crc32_init_table() - Generates a lookup table for CRC32 computation.
 *
 * This function precomputes the CRC32 lookup table for fast computation of
 * the CRC32 checksum. It uses the polynomial 0xEDB88320, which is the standard
 * polynomial for CRC32.
 *
 * The lookup table is filled with 256 entries, where each entry corresponds to
 * the CRC value for one byte of data.
 */
static void crc32_init_table(void)
{
    uint32_t poly = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (uint32_t j = 8; j > 0; j--) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
}

crc_ctx_t *crc_init(crc_type_t type)
{
    crc_ctx_t *ctx = malloc(sizeof(crc_ctx_t));
    if (!ctx) {
        perror("Failed to allocate CRC context");
        return NULL;
    }

    ctx->type = type;
    if (type == CRC32) {
        ctx->crc = 0xFFFFFFFF;  // Initial CRC32 value
        crc32_init_table();     // Initialize CRC32 lookup table
    } else if (type == CRC16) {
        ctx->crc16 = 0xFFFF;    // Initial CRC16 value
    }

    return ctx;
}

uint32_t crc_compute(crc_ctx_t *ctx, const void *data, size_t len)
{
    if (!ctx)
        return 0;

    // Update CRC with the input data
    if (ctx->type == CRC32) {
        const uint8_t *bytes = (const uint8_t *)data;
        for (size_t i = 0; i < len; i++) {
            ctx->crc = (ctx->crc >> 8) ^ crc32_table[(ctx->crc ^ bytes[i]) & 0xFF];
        }
        // Final XOR for CRC32 and return the result
        return ctx->crc ^ 0xFFFFFFFF;
    } else if (ctx->type == CRC16) {
        const uint8_t *bytes = (const uint8_t *)data;
        for (size_t i = 0; i < len; i++) {
            ctx->crc16 ^= (uint16_t)bytes[i] << 8;
            for (int j = 0; j < 8; j++) {
                if (ctx->crc16 & 0x8000) {
                    ctx->crc16 = (ctx->crc16 << 1) ^ 0x1021;
                } else {
                    ctx->crc16 <<= 1;
                }
            }
        }
        // Return the final CRC16 result (no final XOR needed)
        return ctx->crc16;
    }

    return 0;
}

void crc_reset(crc_ctx_t *ctx)
{
    if (!ctx)
        return;

    if (ctx->type == CRC32) {
        ctx->crc = 0xFFFFFFFF;
    } else if (ctx->type == CRC16) {
        ctx->crc16 = 0xFFFF;
    }
}

void crc_free(crc_ctx_t *ctx)
{
    if (ctx)
        free(ctx);
}


