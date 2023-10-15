#ifndef _TX_RING_BUFF_H
#define _TX_RING_BUFF_H

#include "utils.h"

#define MAX_BUFFER_SIZE          (512)
#define BUFFER_COUNT            (10)
#define BUFFER_FULL             (-1)
#define BUFFER_EMPTY            (NULL)
#define BUFFER_ENQUEUE_SUCESS   (0)

typedef struct buffer
{
    u8 buf[MAX_BUFFER_SIZE];
    int len;
} buffer;

typedef struct ring_buffer
{
    buffer buffers[BUFFER_COUNT];
    int head;
    int tail;
    u8 count;
    lock_t lock;
} ring_buffer;

void buffer_init(void);
void buffer_deinit(void);

int is_tx_buffer_empty(void);
int is_tx_buffer_full(void);
int tx_buffer_enqueue(u8 *, int);
buffer *tx_buffer_dequeue(void);
void tx_buffer_critical_section_lock(void);
void tx_buffer_critical_section_unlock(void);

int is_rx_buffer_empty(void);
int is_rx_buffer_full(void);
int rx_buffer_enqueue(u8 *, int);
buffer *rx_buffer_dequeue(void);
void rx_buffer_critical_section_lock(void);
void rx_buffer_critical_section_unlock(void);

#endif
