#include "ring_buff.h"

static struct ring_buffer tx_ring_buff;
static struct ring_buffer rx_ring_buff;

void buffer_init(void)
{
    memset(&tx_ring_buff, 0x0, sizeof(ring_buffer));
    LOCK_INIT(&tx_ring_buff.lock);

    memset(&rx_ring_buff, 0x0, sizeof(ring_buffer));
    LOCK_INIT(&rx_ring_buff.lock);
}

__inline static int is_buffer_empty(struct ring_buffer *ring_buff)
{
    return (ring_buff->count <= 0);
}

__inline static int is_buffer_full(struct ring_buffer *ring_buff)
{
    return (ring_buff->count >= BUFFER_COUNT);
}

int buffer_enqueue(struct ring_buffer *ring_buff, u8 *buf, int len)
{
    if (is_buffer_full(ring_buff))
    {
        DEBUG_PRINT("ring buff count[%d]\n", ring_buff->count);
        return BUFFER_FULL;
    }

    memcpy(ring_buff->buffers[ring_buff->tail].buf, buf, len);
    ring_buff->buffers[ring_buff->tail].len = len;
    ring_buff->tail = (ring_buff->tail + 1) % BUFFER_COUNT;
    ring_buff->count++;
    return 0;
}

buffer *buffer_dequeue(struct ring_buffer *ring_buff)
{
    buffer *buf;

    if (is_buffer_empty(ring_buff))
    {
        DEBUG_PRINT("ring buff count[%d]\n", ring_buff->count);
        return NULL;
    }
        
    buf = &ring_buff->buffers[ring_buff->head];
    ring_buff->head = (ring_buff->head + 1) % BUFFER_COUNT;
    ring_buff->count--;

    return buf;
}

/* Tx Ring buff */
int is_tx_buffer_empty(void)
{
    return is_buffer_empty(&tx_ring_buff);
}

int is_tx_buffer_full(void)
{
    return is_buffer_full(&tx_ring_buff);
}

int tx_buffer_enqueue(u8 *buf, int len)
{
    return buffer_enqueue(&tx_ring_buff, buf, len);
}

buffer *tx_buffer_dequeue(void)
{
    return buffer_dequeue(&tx_ring_buff);
}

void tx_buffer_critical_section_lock(void)
{
    LOCK(&tx_ring_buff.lock);
}

void tx_buffer_critical_section_unlock(void)
{
    UNLOCK(&tx_ring_buff.lock);
}


/* RX Ring buff */
int is_rx_buffer_empty(void)
{
    return is_buffer_empty(&rx_ring_buff);
}

int is_rx_buffer_full(void)
{
    return is_buffer_full(&rx_ring_buff);
}

int rx_buffer_enqueue(u8 *buf, int len)
{
    return buffer_enqueue(&rx_ring_buff, buf, len);
}

buffer *rx_buffer_dequeue(void)
{
    return buffer_dequeue(&rx_ring_buff);
}

void rx_buffer_critical_section_lock(void)
{
    LOCK(&rx_ring_buff.lock);
}

void rx_buffer_critical_section_unlock(void)
{
    UNLOCK(&rx_ring_buff.lock);
}

void buffer_deinit(void)
{
    tx_buffer_critical_section_lock();
    memset(&tx_ring_buff, 0x0, sizeof(ring_buffer));
    tx_buffer_critical_section_unlock();

    rx_buffer_critical_section_lock();
    memset(&rx_ring_buff, 0x0, sizeof(ring_buffer));
    rx_buffer_critical_section_unlock();
}
