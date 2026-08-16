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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isp_bls_success);
  RUN_TEST(test_isp_bls_invalid_format_fails);
  return UNITY_END();
}