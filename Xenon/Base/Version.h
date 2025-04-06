// Copyright 2025 Xenon Emulator Project

#pragma once

namespace Base {

const std::string Version = "Experimental v0.0.1"
#ifdef COMMIT_COUNT
"-" COMMIT_COUNT
#endif
#ifdef BRANCH
" - " BRANCH
#endif
;

}
