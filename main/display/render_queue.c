#include "render_queue.h"

QueueHandle_t render_queue_init(uint32_t queue_size) {
    return xQueueCreate(queue_size, sizeof(gfx_render_request_t));
}

BaseType_t render_queue_send(QueueHandle_t queue,
                             const gfx_render_request_t *request,
                             TickType_t timeout) {
    if (!queue || !request) {
        return pdFALSE;
    }
    return xQueueSend(queue, request, timeout);
}

BaseType_t render_queue_recv(QueueHandle_t queue,
                             gfx_render_request_t *request,
                             TickType_t timeout) {
    if (!queue || !request) {
        return pdFALSE;
    }
    return xQueueReceive(queue, request, timeout);
}

UBaseType_t render_queue_count(QueueHandle_t queue) {
    if (!queue) {
        return 0;
    }
    return uxQueueMessagesWaiting(queue);
}

void render_queue_delete(QueueHandle_t queue) {
    if (queue) {
        vQueueDelete(queue);
    }
}
