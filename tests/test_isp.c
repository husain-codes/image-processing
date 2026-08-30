#include <raw_frame.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_isp_bls_success(void) {
  // 1. Create context and config
  isp_context_t *ctx = isp_create();
  TEST_ASSERT_NOT_NULL(ctx);

  isp_config_t config = {.width = 4,
                         .height = 4,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_BAYER_RGGB,
                         .black_level = 16};

  TEST_ASSERT_EQUAL(0, isp_init(ctx, &config));

  // 2. Create test image with known pixel values
  img_t *img = img_create(4, 4, IMG_FMT_BAYER_RGGB);
  TEST_ASSERT_NOT_NULL(img);

  img->planes[0][0] = 20; // Above offset -> 20 - 16 = 4
  img->planes[0][1] = 16; // Exact offset -> 16 - 16 = 0
  img->planes[0][2] = 10; // Below offset -> clamped to 0

  // 3. Execute pass
  TEST_ASSERT_EQUAL(0, isp_pass_black_level_subtraction(ctx, img));

  // 4. Assert pixel outputs
  TEST_ASSERT_EQUAL_UINT8(4, img->planes[0][0]);
  TEST_ASSERT_EQUAL_UINT8(0, img->planes[0][1]);
  TEST_ASSERT_EQUAL_UINT8(0, img->planes[0][2]);

  // 5. Clean up
  img_destroy(img);
  free(ctx);
}

void test_isp_bls_invalid_format_fails(void) {
  isp_context_t *ctx = isp_create();
  isp_config_t config = {.width = 4,
                         .height = 4,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_BAYER_RGGB,
                         .black_level = 16};
  isp_init(ctx, &config);

  // Create non-Bayer frame (NV12)
  img_t *img = img_create(4, 4, IMG_FMT_NV12);

  // Pass should reject NV12 format
  TEST_ASSERT_EQUAL(-1, isp_pass_black_level_subtraction(ctx, img));

  img_destroy(img);
  free(ctx);
}

void test_isp_awb_stats_and_apply_success(void) {
  // 1. Initialize context and config
  isp_context_t *ctx = isp_create();
  TEST_ASSERT_NOT_NULL(ctx);

  isp_config_t config = {.width = 4,
                         .height = 4,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_BAYER_RGGB};
  TEST_ASSERT_EQUAL(0, isp_init(ctx, &config));

  // 2. Create 4x4 RGGB Bayer frame with artificial color cast
  // Red pixels = 10, Green pixels = 20, Blue pixels = 40
  img_t *img = img_create(4, 4, IMG_FMT_BAYER_RGGB);
  TEST_ASSERT_NOT_NULL(img);

  for (uint32_t y = 0; y < 4; y++) {
    for (uint32_t x = 0; x < 4; x++) {
      if (!(y & 1) && !(x & 1)) { // Red
        img->planes[0][y * 4 + x] = 10;
      } else if ((y & 1) && (x & 1)) { // Blue
        img->planes[0][y * 4 + x] = 40;
      } else { // Green
        img->planes[0][y * 4 + x] = 20;
      }
    }
  }

  // 3. Compute AWB Stats
  // Expected: avg_r = 10, avg_g = 20, avg_b = 40
  // Expected Gains: r_gain = 20/10 = 2.0f, b_gain = 20/40 = 0.5f
  TEST_ASSERT_EQUAL(0, isp_pass_auto_white_balance_stats(ctx, img));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, ctx->r_gain);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, ctx->g_gain);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, ctx->b_gain);

  // 4. Apply White Balance Gains
  TEST_ASSERT_EQUAL(0, isp_pass_apply_white_balance(ctx, img));

  // 5. Assert applied pixel values
  // Red:  10 * 2.0 = 20
  // Blue: 40 * 0.5 = 20
  // Green: Unchanged = 20
  TEST_ASSERT_EQUAL_UINT8(20, img->planes[0][0]); // Top-Left Red
  TEST_ASSERT_EQUAL_UINT8(20, img->planes[0][1]); // Top-Right Green
  TEST_ASSERT_EQUAL_UINT8(20, img->planes[0][5]); // Bottom-Right Blue

  // Clean up
  img_destroy(img);
  free(ctx);
}

void test_isp_awb_clamping_prevents_overflow(void) {
  isp_context_t *ctx = isp_create();
  TEST_ASSERT_NOT_NULL(ctx);

  isp_config_t config = {.width = 2,
                         .height = 2,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_BAYER_RGGB};
  isp_init(ctx, &config);

  // Manually set high gain that would cause 200 * 2.0 = 400 (>255)
  ctx->r_gain = 2.0f;
  ctx->b_gain = 1.0f;

  img_t *img = img_create(2, 2, IMG_FMT_BAYER_RGGB);
  TEST_ASSERT_NOT_NULL(img);
  img->planes[0][0] = 200; // High Red value

  TEST_ASSERT_EQUAL(0, isp_pass_apply_white_balance(ctx, img));

  // Assert Red pixel clamped to 255 (does not wrap to 144)
  TEST_ASSERT_EQUAL_UINT8(255, img->planes[0][0]);

  img_destroy(img);
  free(ctx);
}

void test_isp_awb_invalid_format_fails(void) {
  isp_context_t *ctx = isp_create();
  isp_config_t config = {.width = 4,
                         .height = 4,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_BAYER_RGGB};
  isp_init(ctx, &config);

  // Pass non-Bayer format frame (NV12)
  img_t *img = img_create(4, 4, IMG_FMT_NV12);

  TEST_ASSERT_EQUAL(-1, isp_pass_auto_white_balance_stats(ctx, img));
  TEST_ASSERT_EQUAL(-1, isp_pass_apply_white_balance(ctx, img));

  img_destroy(img);
  free(ctx);
}

void test_isp_demosaic_success(void) {
  isp_context_t *ctx = isp_create();
  TEST_ASSERT_NOT_NULL(ctx);

  isp_config_t config = {.width = 4,
                         .height = 4,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_RGB24};
  TEST_ASSERT_EQUAL(0, isp_init(ctx, &config));

  img_t *src = img_create(4, 4, IMG_FMT_BAYER_RGGB);
  img_t *dst = img_create(4, 4, IMG_FMT_RGB24);
  TEST_ASSERT_NOT_NULL(src);
  TEST_ASSERT_NOT_NULL(dst);

  /*
   * 4x4 RGGB Bayer Input Layout:
   *
   * Row 0: R(100)  G(50)   R(100)  G(50)
   * Row 1: G(150)  B(200)  G(150)  B(200)
   * Row 2: R(100)  G(50)   R(100)  G(50)
   * Row 3: G(150)  B(200)  G(150)  B(200)
   */
  for (uint32_t y = 0; y < 4; y++) {
    for (uint32_t x = 0; x < 4; x++) {
      if (!(y & 1) && !(x & 1)) {
        src->planes[0][y * 4 + x] = 100; // Red
      } else if ((y & 1) && (x & 1)) {
        src->planes[0][y * 4 + x] = 200; // Blue
      } else if (!(y & 1) && (x & 1)) {
        src->planes[0][y * 4 + x] = 50; // Green on Red row (G1)
      } else {
        src->planes[0][y * 4 + x] = 150; // Green on Blue row (G2)
      }
    }
  }

  TEST_ASSERT_EQUAL(0, isp_pass_demosaic(ctx, src, dst));

  // -------------------------------------------------------------
  // Test Interior Red Pixel at (2, 2)
  // -------------------------------------------------------------
  // R = 100 (exact)
  // G = (Left:50 + Right:50 + Top:150 + Bottom:150 + 2) / 4 = 100
  // B = (Top-Left:200 + Top-Right:200 + Bot-Left:200 + Bot-Right:200 + 2) / 4 =
  // 200
  uint8_t *p22 = dst->planes[0] + (2 * dst->stride[0]) + (2 * 3);
  TEST_ASSERT_EQUAL_UINT8(100, p22[0]); // R
  TEST_ASSERT_EQUAL_UINT8(100, p22[1]); // G
  TEST_ASSERT_EQUAL_UINT8(200, p22[2]); // B

  // -------------------------------------------------------------
  // Test Interior Blue Pixel at (1, 1)
  // -------------------------------------------------------------
  // R = (Top-Left:100 + Top-Right:100 + Bot-Left:100 + Bot-Right:100 + 2) / 4 =
  // 100 G = (Left:150 + Right:150 + Top:50 + Bottom:50 + 2) / 4 = 100 B = 200
  // (exact)
  uint8_t *p11 = dst->planes[0] + (1 * dst->stride[0]) + (1 * 3);
  TEST_ASSERT_EQUAL_UINT8(100, p11[0]); // R
  TEST_ASSERT_EQUAL_UINT8(100, p11[1]); // G
  TEST_ASSERT_EQUAL_UINT8(200, p11[2]); // B

  // -------------------------------------------------------------
  // Test Green Pixel on Red Row at (2, 1) [G1]
  // -------------------------------------------------------------
  // R = (Left:100 + Right:100 + 1) / 2 = 100
  // G = 50 (exact)
  // B = (Top:200 + Bottom:200 + 1) / 2 = 200
  uint8_t *p21 = dst->planes[0] + (2 * dst->stride[0]) + (1 * 3);
  TEST_ASSERT_EQUAL_UINT8(100, p21[0]); // R
  TEST_ASSERT_EQUAL_UINT8(50, p21[1]);  // G
  TEST_ASSERT_EQUAL_UINT8(200, p21[2]); // B

  // Convert dst back to BGR for saving
  TEST_ASSERT_EQUAL(0, img_toggle_rgb_bgr(dst));
  // Save the demosaiced image to verify visually
  TEST_ASSERT_EQUAL(0, img_save_bmp("output_demosaic.bmp", dst));

  img_destroy(src);
  img_destroy(dst);
  free(ctx);
}

void test_isp_demosaic_invalid_input_format_fails(void) {
  isp_context_t *ctx = isp_create();
  isp_config_t config = {.width = 2,
                         .height = 2,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_RGB24};
  isp_init(ctx, &config);

  // Pass NV12 instead of BAYER_RGGB
  img_t *src = img_create(2, 2, IMG_FMT_NV12);
  img_t *dst = img_create(2, 2, IMG_FMT_RGB24);

  TEST_ASSERT_EQUAL(-1, isp_pass_demosaic(ctx, src, dst));

  img_destroy(src);
  img_destroy(dst);
  free(ctx);
}

void test_isp_demosaic_invalid_output_format_fails(void) {
  isp_context_t *ctx = isp_create();
  isp_config_t config = {.width = 2,
                         .height = 2,
                         .input_format = IMG_FMT_BAYER_RGGB,
                         .output_format = IMG_FMT_RGB24};
  isp_init(ctx, &config);

  // Pass BAYER_RGGB destination instead of RGB24
  img_t *src = img_create(2, 2, IMG_FMT_BAYER_RGGB);
  img_t *dst = img_create(2, 2, IMG_FMT_BAYER_RGGB);

  TEST_ASSERT_EQUAL(-1, isp_pass_demosaic(ctx, src, dst));

  img_destroy(src);
  img_destroy(dst);
  free(ctx);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isp_bls_success);
  RUN_TEST(test_isp_bls_invalid_format_fails);
  RUN_TEST(test_isp_awb_stats_and_apply_success);
  RUN_TEST(test_isp_awb_clamping_prevents_overflow);
  RUN_TEST(test_isp_awb_invalid_format_fails);

  // Demosaic tests
  RUN_TEST(test_isp_demosaic_success);
  RUN_TEST(test_isp_demosaic_invalid_input_format_fails);
  RUN_TEST(test_isp_demosaic_invalid_output_format_fails);
  return UNITY_END();
}