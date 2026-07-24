#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GESTURE_PREVIEW_WIDTH 320
#define GESTURE_PREVIEW_HEIGHT 180
#define GESTURE_PREVIEW_BYTES (GESTURE_PREVIEW_WIDTH * GESTURE_PREVIEW_HEIGHT * 2)
typedef enum {
    GESTURE_STATE_STOPPED = 0,
    GESTURE_STATE_LOADING,
    GESTURE_STATE_RUNNING,
    GESTURE_STATE_PAUSED,
    GESTURE_STATE_STOPPING,
    GESTURE_STATE_ERROR,
} gesture_mode_state_t;

typedef enum {
    GESTURE_START_MCP = 0,
    GESTURE_START_TOUCH,
} gesture_start_source_t;

typedef enum {
    GESTURE_STOP_USER = 0,
    GESTURE_STOP_CRITICAL_STATE,
    GESTURE_STOP_ERROR,
} gesture_stop_reason_t;

typedef struct {
    bool active;
    gesture_mode_state_t state;
    uint32_t timeout_seconds;
    uint32_t preview_sequence;
    char gesture[16];
    float confidence;
    uint8_t confirmation_count;
    uint8_t confirmation_required;
    int16_t box_x;
    int16_t box_y;
    int16_t box_width;
    int16_t box_height;
    char last_action[32];
    char status[64];
    char error[96];
} gesture_mode_snapshot_t;

bool gesture_mode_init(void);
bool gesture_mode_start(gesture_start_source_t source);
void gesture_mode_stop(gesture_stop_reason_t reason);
bool gesture_mode_is_active(void);
gesture_mode_state_t gesture_mode_get_state(void);
const char *gesture_mode_state_name(gesture_mode_state_t state);
bool gesture_mode_get_snapshot(gesture_mode_snapshot_t *snapshot);

// Copies and horizontally mirrors the newest RGB565 preview frame. The input
// sequence is used to skip duplicate frames and is updated after a successful copy.
bool gesture_mode_copy_preview(uint8_t *output, size_t output_len, uint32_t *sequence);

#ifdef __cplusplus
}
#endif
