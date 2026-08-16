#include <img/img_format.h>
#include <stdint.h>

size_t img_format_bytes_per_pixel(img_format_t format) {
  switch (format) {
  case IMG_FMT_UNKNOWN:
    return 0;
  case IMG_FMT_GRAY8:
  case IMG_FMT_BAYER_RGGB:
  case IMG_FMT_BAYER_BGGR:
  case IMG_FMT_BAYER_GBRG:
  case IMG_FMT_BAYER_GRBG:
    return 1;
  case IMG_FMT_RGB24:
  case IMG_FMT_BGR24:
  case IMG_FMT_YUV444:
    return 3;
  case IMG_FMT_RGBA32:
    return 4;
  case IMG_FMT_NV12:
    return 0; // NV12 unique in that it has a separate plane for chroma data, so
              // bytes per pixel is not straightforward
  default:
    return 0;
  }
}

bool img_format_is_planar(img_format_t format) {
  switch (format) {
  case IMG_FMT_NV12:
  case IMG_FMT_YUV420P:
    return true;
  default:
    return false;
  }
}

uint32_t img_format_plane_height(img_format_t format, uint32_t height,
                                 int plane_index) {
  if (format == IMG_FMT_NV12) {
    if (plane_index == 0) {
      return height; // Y plane has full height
    } else if (plane_index == 1) {
      return height / 2; // UV plane has half the height of Y plane
    }
  }

  if (format == IMG_FMT_YUV420P) {
    if (plane_index == 0) return height;
    else return height / 2; // U and V planes are half height
  }

  return height; // For non-planar formats, all planes have the same height as
                 // the image
}