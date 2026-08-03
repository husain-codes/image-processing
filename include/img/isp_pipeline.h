#ifndef ISP_PIPELINE_H
#define ISP_PIPELINE_H

#include <img/img.h>

// Lifecycle States
typedef enum {
    ISP_STATE_UNINIT = 0,
    ISP_STATE_CONFIGURED,
    ISP_STATE_RUNNING,
    ISP_STATE_PAUSED,
    ISP_STATE_STOPPED,
    ISP_STATE_ERROR
} isp_state_t;

// Configuration options set by app
typedef struct {
    uint32_t width;
    uint32_t height;
    img_format_t input_format;
    img_format_t output_format;
    
    uint16_t black_level;  // e.g., 16 for 8-bit or 64 for 10-bit
    int enable_bls;
    int enable_awb;
    int enable_demosaic;
    int enable_blur;
} isp_config_t;

// ISP Context (State Machinery)
typedef struct {
    isp_state_t state;
    isp_config_t config;
    
    // Dynamic 3A Gains
    float r_gain;
    float g_gain;
    float b_gain;
} isp_context_t;

// API Prototypes
isp_context_t* isp_create(void);
int isp_init(isp_context_t* ctx, const isp_config_t* config);
int isp_start(isp_context_t* ctx);
int isp_stop(isp_context_t* ctx);
void isp_destroy(isp_context_t* ctx);

// Top-Level Pipeline Runner
int isp_process_frame(isp_context_t* ctx, const img_t* src, img_t* dst);

// Empty Pass Stubs
int isp_pass_black_level_subtraction(isp_context_t* ctx, img_t* frame);
int isp_pass_auto_white_balance_stats(isp_context_t* ctx, const img_t* frame);
int isp_pass_apply_white_balance(isp_context_t* ctx, img_t* frame);
int isp_pass_demosaic(isp_context_t* ctx, const img_t* src, img_t* dst);
int isp_pass_blur(isp_context_t* ctx, const img_t* src, img_t* dst);

#endif // ISP_PIPELINE_H
