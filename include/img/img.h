#ifndef IMG_H
#define IMG_H

#include "img_format.h"
#include <stddef.h>
#include <stdint.h>

/*Image/Frame structure */
typedef struct {
  uint32_t width;
  uint32_t height;
  size_t stride[3]; // number of bytes in a row of pixel data, excluding padding
  img_format_t format;
  uint8_t num_planes;
  uint8_t *planes[3];

  /*ISP and V4L2 Streaming Metadat*/
  uint8_t bit_depth; // Bit depth of the image (e.g., 8, 10, 12)
  uint64_t timestamp_us; // Microsecond kernel timestamp from V4L2
  uint32_t frame_seq; // Monotonic Frame counter from V4L2
  int dbuf_fd; // DMA buffer file descriptor (for zero-copy)
} img_t;

/*Pixel structure */
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} pixel_t;

img_t *img_create(uint32_t width, uint32_t height, img_format_t format);

void img_destroy(img_t *img);

int img_get_pixel(img_t *img, int x, int y, pixel_t *out);
int img_set_pixel(img_t *img, int x, int y, pixel_t p);
#endif
