#include <img/isp_pipeline.h>
#include <stdio.h>
#include <stdlib.h>

isp_context_t *isp_create(void) {
  printf("[ISP_API] %s()\n", __func__);
  isp_context_t *ctx = (isp_context_t *)calloc(1, sizeof(isp_context_t));
  if (!ctx) {
    printf(" -> ERROR: Memory allocation failed!\n");
    return NULL;
  }
  ctx->state = ISP_STATE_UNINIT;
  return ctx;
}

int isp_init(isp_context_t *ctx, const isp_config_t *config) {
  printf("[ISP_API] %s()\n", __func__);
  if (!ctx || !config)
    return -1;

  if (ctx->state != ISP_STATE_UNINIT) {
    printf(" -> ERROR: ISP Engine already initialized!\n");
    return -1;
  }

  if (config->width == 0 || config->height == 0) {
    printf(" -> ERROR: Invalid dimensions!\n");
    ctx->state = ISP_STATE_ERROR;
    return -1;
  }

  if (config->input_format == IMG_FMT_UNKNOWN ||
      config->output_format == IMG_FMT_UNKNOWN) {
    printf(" -> ERROR: Invalid image format!\n");
    ctx->state = ISP_STATE_ERROR;
    return -1;
  }

  if (config->width & 1 || config->height & 1) {
    printf(" -> ERROR: Width and Height must be even for Bayer 2x2 grid!\n");
    ctx->state = ISP_STATE_ERROR;
    return -1;
  }

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
  if (!ctx || !frame) {
    printf("%s: Input is NULL\n", __func__);
    return -1;
  }

  if (frame->format != IMG_FMT_BAYER_RGGB &&
      frame->format != IMG_FMT_BAYER_BGGR &&
      frame->format != IMG_FMT_BAYER_GBRG &&
      frame->format != IMG_FMT_BAYER_GRBG) {
    printf(" -> ERROR: %s: Input format must be a RAW Bayer pattern\n",
           __func__);
    return -1;
  }

  if (frame->planes[0] == NULL) {
    printf("%s: NULL plane\n", __func__);
    return -1;
  }

  uint32_t total_pixel = frame->height * frame->width;
  uint8_t *pixel = frame->planes[0];
  uint16_t black_level = ctx->config.black_level;
  for (size_t i = 0; i < total_pixel; i++) {
    if (pixel[i] > black_level) {
      pixel[i] -= (uint8_t)black_level;
    } else {
      pixel[i] = 0;
    }
  }
  return 0;
}

int isp_pass_auto_white_balance_stats(isp_context_t *ctx, const img_t *frame) {
  printf("  [PASS] %s()\n", __func__);
  if (!ctx || !frame || !frame->planes[0]) {
    printf(" -> ERROR: NULL pointer passed to %s\n", __func__);
    return -1;
  }

  if (frame->format != IMG_FMT_BAYER_RGGB) {
    printf(
        " -> ERROR: Unsupported Bayer format in %s (Currently expects RGGB)\n",
        __func__);
    return -1;
  }

  uint32_t width = frame->width;
  uint32_t height = frame->height;
  const uint8_t *pixel = frame->planes[0];
  size_t sum_r = 0, sum_g = 0, sum_b = 0;
  size_t count_r = 0, count_g = 0, count_b = 0;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint8_t value = pixel[y * width + x];
      if (!(y & 1) && !(x & 1)) {
        count_r++;
        sum_r += value;
      } else if ((y & 1) && (x & 1)) {
        sum_b += value;
        count_b++;
      } else {
        sum_g += value;
        count_g++;
      }
    }
  }

  if (count_r == 0 || count_b == 0 || count_g == 0) {
    printf(" -> ERROR: No pixels found for one of the channels!\n");
    return -1;
  }
  float avg_r = (float)sum_r / count_r;
  float avg_g = (float)sum_g / count_g;
  float avg_b = (float)sum_b / count_b;

  // Prevent division by zero if a channel is completely dark
  if (avg_r < 1.0f)
    avg_r = 1.0f;
  if (avg_g < 1.0f)
    avg_g = 1.0f;
  if (avg_b < 1.0f)
    avg_b = 1.0f;

  ctx->r_gain = avg_g / avg_r;
  ctx->g_gain = 1.0f;
  ctx->b_gain = avg_g / avg_b;
  return 0;
}

int isp_pass_apply_white_balance(isp_context_t *ctx, img_t *frame) {
  printf("  [PASS] %s()\n", __func__);
  if (!ctx || !frame || !frame->planes[0]) {
    printf(" -> ERROR: NULL pointer passed to %s\n", __func__);
    return -1;
  }

  if (frame->format != IMG_FMT_BAYER_RGGB) {
    printf(" -> ERROR: Unsupported Bayer format in %s\n", __func__);
    return -1;
  }

  uint32_t width = frame->width;
  uint32_t height = frame->height;
  uint8_t *pixel = frame->planes[0];
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      size_t idx = y * width + x;
      uint8_t value = pixel[idx];
      if (!(y & 1) && !(x & 1)) {
        // Red pixel
        float new_val = value * ctx->r_gain;
        // printf(" -> DEBUG: R pixel at (%d,%d): %u * %.2f = %.2f\n", x, y,
        // value, ctx->r_gain, new_val);
        if (new_val > 255.0f)
          new_val = 255.0f;
        pixel[idx] = (uint8_t)(new_val + 0.5f);
      } else if ((y & 1) && (x & 1)) {
        // Blue pixel
        float new_val = value * ctx->b_gain;
        if (new_val > 255.0f)
          new_val = 255.0f;
        pixel[idx] = (uint8_t)(new_val + 0.5f);
      }
    }
  }
  return 0;
}

int isp_pass_demosaic(isp_context_t *ctx, const img_t *src, img_t *dst) {
  printf("  [PASS] %s()\n", __func__);
  if (!ctx || !src || !dst) {
    printf(" -> ERROR: NULL pointer passed to %s\n", __func__);
    return -1;
  }
  uint32_t width = src->width;
  uint32_t height = src->height;
  if (dst->width != width || dst->height != height) {
    printf(" -> ERROR: source and destination metadata are different %s\n",
           __func__);
    return -1;
  }

  if (src->format != IMG_FMT_BAYER_RGGB) {
    printf(" -> ERROR: src image format must be BAYER_RGGB in %s\n", __func__);
    return -1;
  }

  if (dst->format != IMG_FMT_RGB24) {
    printf(" -> ERROR:  dst image format should be RGB24 %s\n", __func__);
    return -1;
  }
  int bpp = img_format_bytes_per_pixel(dst->format);
  const uint8_t *src_pixel = src->planes[0];
  uint8_t *dst_pixel = dst->planes[0];

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      size_t src_idx = y * width + x;
      uint8_t value = src_pixel[src_idx];
      dst_pixel = dst->planes[0] + y * dst->stride[0] + x * bpp;

      int left = (x > 0) ? x - 1 : x;
      int right = (x < width - 1) ? x + 1 : x;
      int top = (y > 0) ? y - 1 : y;
      int bot = (y < height - 1) ? y + 1 : y;

      if (!(y & 1) && !(x & 1)) {
        // Red Pixel
        dst_pixel[0] = value; // R
        uint32_t g_sum =
            src_pixel[y * width + right] + src_pixel[y * width + left] +
            src_pixel[top * width + x] + src_pixel[bot * width + x];
        uint32_t b_sum =
            src_pixel[bot * width + right] + src_pixel[top * width + right] +
            src_pixel[bot * width + left] + src_pixel[top * width + left];
        dst_pixel[1] =
            (uint8_t)((g_sum + 2) /
                      4); // Adding 2 gives nearest-integer rounding--> Green
        dst_pixel[2] =
            (uint8_t)((b_sum + 2) /
                      4); // Adding 2 gives nearest-integer rounding--> Blue

      } else if ((y & 1) && (x & 1)) {
        // Blue pixel
        dst_pixel[2] = value; // B
        uint32_t g_sum =
            src_pixel[top * width + x] + src_pixel[bot * width + x] +
            src_pixel[y * width + right] + src_pixel[y * width + left];

        uint32_t r_sum =
            src_pixel[top * width + left] + src_pixel[top * width + right] +
            src_pixel[bot * width + right] + src_pixel[bot * width + left];

        dst_pixel[0] =
            (uint8_t)((r_sum + 2) /
                      4); // Adding 2 gives nearest-integer rounding--> Red
        dst_pixel[1] =
            (uint8_t)((g_sum + 2) /
                      4); // Adding 2 gives nearest-integer rounding--> Green
      } else {
        // green pixel
        dst_pixel[1] = value; // G
        if (y & 1) {
          // Green pixel on Blue row
          uint32_t b_sum =
              src_pixel[y * width + right] + src_pixel[y * width + left];
          uint32_t r_sum =
              src_pixel[top * width + x] + src_pixel[bot * width + x];
          dst_pixel[2] =
              (uint8_t)((b_sum + 1) /
                        2); // Adding 1 gives nearest-integer rounding--> Blue
          dst_pixel[0] =
              (uint8_t)((r_sum + 1) /
                        2); // Adding 1 gives nearest-integer rounding--> Red
        } else {
          // Green pixel on Red row
          uint32_t r_sum =
              src_pixel[y * width + right] + src_pixel[y * width + left];
          uint32_t b_sum =
              src_pixel[top * width + x] + src_pixel[bot * width + x];
          dst_pixel[0] =
              (uint8_t)((r_sum + 1) /
                        2); // Adding 1 gives nearest-integer rounding--> Red
          dst_pixel[2] =
              (uint8_t)((b_sum + 1) /
                        2); // Adding 1 gives nearest-integer rounding--> Blue
        }
      }
    }
  }
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
