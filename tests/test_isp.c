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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isp_bls_success);
  RUN_TEST(test_isp_bls_invalid_format_fails);
  RUN_TEST(test_isp_awb_stats_and_apply_success);
  RUN_TEST(test_isp_awb_clamping_prevents_overflow);
  RUN_TEST(test_isp_awb_invalid_format_fails);
  return UNITY_END();
}