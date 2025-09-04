// Copyright 2025 Xenon Emulator Project. All rights reserved.

#pragma once

#include "Core/XCPU/PPU/PowerPC.h"

namespace XCPU {
	namespace MMU {

		// The CELL B/E manual states that any CELL arch based PPE's (like the ones inside the Xenon CPU) can have the 
		// 'small' 4KB Page size and any two out of the three 'large' page sizes described therein. The PPU's inside the 
		// Xenon use the 'large' 64Kb and 16Mb page sizes.
		enum ePageSize : u8 {
			pSize4Kb = 12,					// 4Kb 'small' page size. p = 12 bits.
			pSize64Kb = 16,				// 64Kb 'large' page size. p = 16 bits.
			pSize16Mb = 24,				// 16Mb 'large' page size. p = 24 bits.
			pSizeUnsupported = 0	// Unknown or unsupported (1Mb) page size.
		};

		// Xenon Memory Management Unit
		class XenonMMU
		{
		public:
			XenonMMU(XenonContext* inXenonContext) : xenonContext(inXenonContext) {};
			~XenonMMU() { xenonContext.reset(); };

			// Read/Write routines


		private:
			// Xenon CPU Context
			std::shared_ptr<XenonContext> xenonContext;

			// Calculates and returns the page size (p in the PPC arch).
			ePageSize GetCurrentPageSize(PPU_STATE* ppuState, bool L, u8 LP);
		};
	}
}