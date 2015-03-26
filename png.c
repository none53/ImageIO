/**
 * @file png.c
 *
 * Copyright(c) 2015 大前良介(OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief PNGファイルの読み書き処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介(OHMAE Ryosuke)</a>
 * @date 2015/02/07
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <png.h>
#include "image.h"

/**
 * @brief PNG形式のファイルを読み込む。
 *
 * @param[in] filename ファイル名
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_png_file(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror(filename);
    return NULL;
  }
  image_t *img = read_png_stream(fp);
  fclose(fp);
  return img;
}

/**
 * @brief PNG形式のファイルを読み込む。
 *
 * @param[in] fp ファイルストリーム
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_png_stream(FILE *fp) {
  int i, x, y;
  image_t *img = NULL;
  png_structp png = NULL;
  png_infop info = NULL;
  png_bytepp rows;
  png_byte sig_bytes[8];
  if (fread(sig_bytes, sizeof(sig_bytes), 1, fp) != 1) {
    return NULL;
  }
  if (png_sig_cmp(sig_bytes, 0, sizeof(sig_bytes))) {
    return NULL;
  }
  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL) {
    goto error;
  }
  info = png_create_info_struct(png);
  if (info == NULL) {
    goto error;
  }
  if (setjmp(png_jmpbuf(png))) {
    goto error;
  }
  png_init_io(png, fp);
  png_set_sig_bytes(png, sizeof(sig_bytes));
  png_read_png(png, info, PNG_TRANSFORM_PACKING, NULL);
  rows = png_get_rows(png, info);
  // 画像形式に応じて詰め込み
  switch (info->color_type) {
    case PNG_COLOR_TYPE_PALETTE:  // インデックスカラー
      if ((img = allocate_image(info->width, info->height, COLOR_TYPE_INDEX))
          == NULL) {
        goto error;
      }
      img->palette_num = info->num_palette;
      for (i = 0; i < info->num_palette; i++) {
        png_color pc = info->palette[i];
        img->palette[i] = color_from_rgb(pc.red, pc.green, pc.blue);
      }
      for (y = 0; y < info->height; y++) {
        for (x = 0; x < info->width; x++) {
          img->map[y][x].i = rows[y][x];
        }
      }
      break;
    case PNG_COLOR_TYPE_GRAY:  // グレースケール
      if ((img = allocate_image(info->width, info->height, COLOR_TYPE_GRAY))
          == NULL) {
        goto error;
      }
      for (y = 0; y < info->height; y++) {
        for (x = 0; x < info->width; x++) {
          img->map[y][x].g = rows[y][x];
        }
      }
      break;
    case PNG_COLOR_TYPE_RGB:  // RGB
      if ((img = allocate_image(info->width, info->height, COLOR_TYPE_RGB))
          == NULL) {
        goto error;
      }
      for (y = 0; y < info->height; y++) {
        for (x = 0; x < info->width; x++) {
          img->map[y][x].c.r = rows[y][x * 3 + 0];
          img->map[y][x].c.g = rows[y][x * 3 + 1];
          img->map[y][x].c.b = rows[y][x * 3 + 2];
          img->map[y][x].c.a = 0xff;
        }
      }
      break;
    case PNG_COLOR_TYPE_RGB_ALPHA:  // RGBA
      if ((img = allocate_image(info->width, info->height, COLOR_TYPE_RGBA))
          == NULL) {
        goto error;
      }
      for (y = 0; y < info->height; y++) {
        for (x = 0; x < info->width; x++) {
          img->map[y][x].c.r = rows[y][x * 4 + 0];
          img->map[y][x].c.g = rows[y][x * 4 + 1];
          img->map[y][x].c.b = rows[y][x * 4 + 2];
          img->map[y][x].c.a = rows[y][x * 4 + 3];
        }
      }
      break;
  }
  error:
  png_destroy_read_struct(&png, &info, NULL);
  return img;
}

/**
 * @brief PNG形式としてファイルに書き出す。
 *
 * @param[in] filename 書き出すファイル名
 * @param[in] img      画像データ
 * @return 成否
 */
result_t write_png_file(const char *filename, image_t *img) {
  result_t result = FAILURE;
  if (img == NULL) {
    return result;
  }
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror(filename);
    return result;
  }
  result = write_png_stream(fp, img);
  fclose(fp);
  return result;
}

/**
 * @brief PNG形式としてファイルに書き出す。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
result_t write_png_stream(FILE *fp, image_t *img) {
  int i, x, y;
  result_t result = FAILURE;
  int row_size;
  int color_type;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytepp rows = NULL;
  png_colorp palette = NULL;
  if (img == NULL) {
    return result;
  }
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:  // インデックスカラー
      color_type = PNG_COLOR_TYPE_PALETTE;
      row_size = sizeof(png_byte) * img->width;
      break;
    case COLOR_TYPE_GRAY:  // グレースケール
      color_type = PNG_COLOR_TYPE_GRAY;
      row_size = sizeof(png_byte) * img->width;
      break;
    case COLOR_TYPE_RGB:  // RGB
      color_type = PNG_COLOR_TYPE_RGB;
      row_size = sizeof(png_byte) * img->width * 3;
      break;
    case COLOR_TYPE_RGBA:  // RGBA
      color_type = PNG_COLOR_TYPE_RGBA;
      row_size = sizeof(png_byte) * img->width * 4;
      break;
    default:
      return FAILURE;
  }
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL) {
    goto error;
  }
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    goto error;
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
    goto error;
  }
  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, img->width, img->height, 8,
      color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT);
  rows = png_malloc(png_ptr, sizeof(png_bytep) * img->height);
  if (rows == NULL) {
    goto error;
  }
  png_set_rows(png_ptr, info_ptr, rows);
  memset(rows, 0, sizeof(png_bytep) * img->height);
  for (y = 0; y < img->height; y++) {
    if ((rows[y] = png_malloc(png_ptr, row_size)) == NULL) {
      goto error;
    }
  }
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:  // インデックスカラー
      palette = png_malloc(png_ptr,
          sizeof(png_color) * img->palette_num);
      for (i = 0; i < img->palette_num; i++) {
        palette[i].red = img->palette[i].r;
        palette[i].green = img->palette[i].g;
        palette[i].blue = img->palette[i].b;
      }
      png_set_PLTE(png_ptr, info_ptr, palette, img->palette_num);
      png_free(png_ptr, palette);
      for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
          rows[y][x] = img->map[y][x].i;
        }
      }
      break;
    case COLOR_TYPE_GRAY:  // グレースケール
      for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
          rows[y][x] = img->map[y][x].g;
        }
      }
      break;
    case COLOR_TYPE_RGB:  // RGB
      for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
          rows[y][x * 3 + 0] = img->map[y][x].c.r;
          rows[y][x * 3 + 1] = img->map[y][x].c.g;
          rows[y][x * 3 + 2] = img->map[y][x].c.b;
        }
      }
      break;
    case COLOR_TYPE_RGBA:  // RGBA
      for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
          rows[y][x * 4 + 0] = img->map[y][x].c.r;
          rows[y][x * 4 + 1] = img->map[y][x].c.g;
          rows[y][x * 4 + 2] = img->map[y][x].c.b;
          rows[y][x * 4 + 3] = img->map[y][x].c.a;
        }
      }
      break;
  }
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
  result = SUCCESS;
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return result;
  error:
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return FAILURE;
}


