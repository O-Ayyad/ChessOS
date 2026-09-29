#pragma once
#include "boot_info.h"
#include "board_view.h"

void run_boot_screen(const BootInfo* boot_info, PiecePictures* pieces);
void wait_for_any_key(int max_milliseconds);
