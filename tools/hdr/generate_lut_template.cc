// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include "lib/extras/codec.h"
#include "lib/extras/packed_image_convert.h"
#include "tools/cmdline.h"
#include "tools/file_io.h"
#include "tools/thread_pool_internal.h"

using jxl::Image3F;

int main(int argc, const char** argv) {
  jpegxl::tools::ThreadPoolInternal pool;

  jpegxl::tools::CommandLineParser parser;
  size_t N = 64;
  parser.AddOptionValue('N', "lut_size", "N", "linear size of the LUT", &N,
                        &jpegxl::tools::ParseUnsigned, 0);
  const char* output_filename = nullptr;
  auto output_filename_option = parser.AddPositionalOption(
      "output", true, "output LUT", &output_filename, 0);

  if (!parser.Parse(argc, argv)) {
    fprintf(stderr, "See -h for help.\n");
    return EXIT_FAILURE;
  }

  if (parser.HelpFlagPassed()) {
    parser.PrintHelp();
    return EXIT_SUCCESS;
  }

  if (!parser.GetOption(output_filename_option)->matched()) {
    fprintf(stderr, "Missing output filename.\nSee -h for help.\n");
    return EXIT_FAILURE;
  }

  JXL_ASSIGN_OR_RETURN(Image3F image, Image3F::Create(N * N, N));
  JXL_CHECK(jxl::RunOnPool(
      &pool, 0, N, jxl::ThreadPool::NoInit,
      [&](const uint32_t y, size_t /* thread */) {
        const float g = static_cast<float>(y) / (N - 1);
        float* const JXL_RESTRICT rows[3] = {
            image.PlaneRow(0, y), image.PlaneRow(1, y), image.PlaneRow(2, y)};
        for (size_t x = 0; x < N * N; ++x) {
          rows[0][x] = static_cast<float>(x % N) / (N - 1);
          rows[1][x] = g;
          rows[2][x] = static_cast<float>(x / N) / (N - 1);
        }
      },
      "GenerateTemplate"));

  JxlPixelFormat format = {3, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};
  jxl::extras::PackedPixelFile ppf =
      jxl::extras::ConvertImage3FToPackedPixelFile(
          image, jxl::ColorEncoding::SRGB(), format, &pool);
  std::vector<uint8_t> encoded;
  JXL_CHECK(jxl::Encode(ppf, output_filename, &encoded, &pool));
  JXL_CHECK(jpegxl::tools::WriteFile(output_filename, encoded));
}
