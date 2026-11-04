/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Base/Types.h"

#include <string>
#include <vector>

namespace Xe::NandImage {

  struct Image {
    std::vector<u8> data; // ECC-stripped logical image with recognized bad blocks replaced
    bool hasSpare = false;
    bool bigBlock = false; // Raw NAND geometry used by J-Runner's update-patch traversal
  };

  // Reads a retail raw dump, a big-page/ECC dump, or an ECC-stripped image and presents logical NAND offsets.
  bool ReadLogicalImage(const std::string &path, Image &image);

} // namespace Xe::NandImage
