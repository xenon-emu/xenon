/***************************************************************/
/* Copyright 2025 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

namespace Base {

const std::string Version = "Experimental " APP_VERSION
#ifdef COMMIT_COUNT
"-" COMMIT_COUNT
#endif
#ifdef BRANCH
" - " BRANCH
#endif
;

} // namespace Base
