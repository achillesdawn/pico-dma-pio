#pragma once

#include "hardware/pio.h"
#include <cstdint>

int setup_pio_dma_two_channels(
    PIO pio, int sm, uint32_t *read_addr
);
