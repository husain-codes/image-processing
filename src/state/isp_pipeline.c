#include <img/isp_pipeline.h>
#include <stdio.h>
#include <stdlib.h>

isp_context_t *isp_create(void) {
  printf("[ISP_API] %s()\n", __func__);
  isp_context_t *ctx = (isp_context_t)calloc(1, sizeof(isp_context_t));
  if (ctx)
    ctx->state = ISP_STATE_UNINIT;
  return ctx;
}

int isp_init(isp_context_t *ctx, const isp_config_t *config) {
  printf("[ISP_API] %s()\n", __func__);
  if (!ctx || !config)
    return -1;

  ctx->config = *config;
  ctx->r_gain = 1.0f;
  ctx->g_gain = 1.0f;
  ctx->b_gain = 1.0f;
  ctx->state = ISP_STATE_CONFIGURED;
  return 0;
}

int isp_start(isp_context_t *ctx) {
  printf("[ISP_API] %s()\n", __func__);
  if (!ctx || ctx->state != ISP_STATE_CONFIGURED)
    return -1;
  ctx->state = ISP_STATE_RUNNING;
  return 0;
}

int isp_stop(isp_context_t *ctx) {
  printf("[ISP_API] %s()\n", __func__);
  if (!ctx)
    return -1;
  ctx->state = ISP_STATE_CONFIGURED;
  return 0;
}

void isp_destroy(isp_context_t *ctx) {
  printf("[ISP_API] %s()\n", __func__);
  if (ctx)
    free(ctx);
  ctx->state = ISP_STATE_UNINIT;
}

/* Stubs for each Pass */
int isp_pass_black_level_subtraction(isp_context_t *ctx, img_t *frame) {
  printf("  [PASS] %s()\n", __func__);
  return 0;
}

int isp_pass_auto_white_balance_stats(isp_context_t *ctx, const img_t *frame) {
  printf("  [PASS] %s()\n", __func__);
  return 0;
}

int isp_pass_apply_white_balance(isp_context_t *ctx, img_t *frame) {
  printf("  [PASS] %s()\n", __func__);
  return 0;
}

int isp_pass_demosaic(isp_context_t *ctx, const img_t *src, img_t *dst) {
  printf("  [PASS] %s()\n", __func__);
  return 0;
}

int isp_pass_blur(isp_context_t *ctx, const img_t *src, img_t *dst) {
  printf("  [PASS] %s()\n", __func__);
  return 0;
}

/* Orchestrator Function */
int isp_process_frame(isp_context_t *ctx, const img_t *src, img_t *dst) {
  printf("[ISP_PIPELINE] %s() [Frame #%u]\n", __func__, src->frame_seq);

  if (!ctx || ctx->state != ISP_STATE_RUNNING) {
    printf(" -> ERROR: ISP Engine not running!\n");
    return -1;
  }

  if (ctx->config.enable_bls) {
    isp_pass_black_level_subtraction(ctx, (img_t *)src);
  }

  if (ctx->config.enable_awb) {
    isp_pass_auto_white_balance_stats(ctx, src);
    isp_pass_apply_white_balance(ctx, (img_t *)src);
  }

  if (ctx->config.enable_demosaic) {
    isp_pass_demosaic(ctx, src, dst);
  }

  if (ctx->config.enable_blur) {
    isp_pass_blur(ctx, dst, dst);
  }

  return 0;
}
