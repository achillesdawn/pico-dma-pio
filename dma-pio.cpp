#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <bitset>
#include <cstdint>
#include <ctime>
#include <hardware/gpio.h>
#include <hardware/structs/dma.h>
#include <pico/stdio_usb.h>
#include <stdio.h>

#include "blink.pio.h"

using u8 = uint8_t;

constexpr u8 PIN_8 = 8;
constexpr u8 PIN_9 = 9;
constexpr u8 PIN_10 = 10;
constexpr u8 PIN_11 = 11;
constexpr u8 PIN_12 = 12;
constexpr u8 PIN_13 = 13;
constexpr u8 PIN_14 = 14;
constexpr u8 PIN_15 = 15;

template <u8... Pins>

constexpr uint32_t create_pin_mask() {
    return ((1u << Pins) | ...);
}

constexpr uint32_t GPIO_MASK =
    create_pin_mask<8, 9, 10, 11, 12, 13, 14, 15>();

void blink_init(PIO pio, uint sm, uint offset) {

    pio_gpio_init(pio, PIN_8);
    pio_gpio_init(pio, PIN_9);
    pio_gpio_init(pio, PIN_10);
    pio_gpio_init(pio, PIN_11);
    pio_gpio_init(pio, PIN_12);
    pio_gpio_init(pio, PIN_13);
    pio_gpio_init(pio, PIN_14);
    pio_gpio_init(pio, PIN_15);

    // set pin direction
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_8, 8, true);

    pio_sm_config config =
        blink_program_get_default_config(offset);

    // set pins
    sm_config_set_out_pins(&config, PIN_8, 8);
    // sm_config_set_out_shift(&config, false, true, 32);

    pio_sm_init(pio, sm, offset, &config);

    pio_sm_set_enabled(pio, sm, true);

    printf("Blinkin program start");

    // set divisor to 48mhz/2^16
    pio->sm[sm].clkdiv = 0xFFFF0000;
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    printf("alarm\n");

    return 1'000'000;
}

// the problem is dma channels have a transfer count, when
// the transfer count hits 0 it stops transfering even if
// there are dreq this is being solved by chaining two
// channels one to the end of the other config channel in
// this case does nothing but establish the chain so that
// the main channel gets restarted
int setup_pio_dma(PIO pio, int sm, uint32_t *read_addr) {

    int dma_ch = dma_claim_unused_channel(true);
    int control_ch = dma_claim_unused_channel(true);

    dma_channel_config dma_ch_config =
        dma_channel_get_default_config(dma_ch);

    dma_channel_config control_ch_config =
        dma_channel_get_default_config(control_ch);

    // tx fifo is 32 bits
    channel_config_set_transfer_data_size(
        &dma_ch_config, DMA_SIZE_32
    );
    channel_config_set_transfer_data_size(
        &control_ch_config, DMA_SIZE_32
    );

    channel_config_set_write_increment(
        &dma_ch_config, false
    );
    channel_config_set_write_increment(
        &control_ch_config, false
    );

    channel_config_set_read_increment(
        &dma_ch_config, false
    );
    channel_config_set_read_increment(
        &control_ch_config, false
    );

    channel_config_set_dreq(
        &dma_ch_config, pio_get_dreq(pio, sm, true)
    );

    channel_config_set_chain_to(&dma_ch_config, control_ch);
    channel_config_set_chain_to(&control_ch_config, dma_ch);

    dma_channel_configure(
        dma_ch,
        &dma_ch_config,
        &pio->txf[sm],
        read_addr,
        100,
        true
    );

    dma_channel_configure(
        control_ch, &control_ch_config, NULL, NULL, 1, false

    );

    return dma_ch;
}

void restart() {
    watchdog_reboot(0, 0, 200);
    busy_wait_ms(200);
}

int main() {
    stdio_init_all();

    set_sys_clock_48mhz();

    while (!stdio_usb_connected()) {
        busy_wait_ms(200);
    }

    printf("ready to go\n");

    add_alarm_in_ms(1000, alarm_callback, NULL, true);

    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &blink_program);

    blink_init(pio, sm, offset);

    uint32_t data_src = 2;

    int dma_chan = setup_pio_dma(pio, sm, &data_src);

    while (true) {

        data_src += 1;

        uint32_t count =
            dma_hw->ch[dma_chan].transfer_count;

        printf("transfer count: %d\n", count);

        sleep_ms(500);
    }
}
