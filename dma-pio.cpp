#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <pico/stdio_usb.h>
#include <stdio.h>

// Data will be copied from src to dst
const char src[] = "Hello, world! (from DMA)";
char dst[count_of(src)];

#include "blink.pio.h"

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {

    pio_gpio_init(pio, pin);

    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config config = blink_program_get_default_config(offset);

    sm_config_set_set_pins(&config, pin, 1);

    pio_sm_init(pio, sm, offset, &config);

    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    printf("alarm");

    return 1'000'000;
}

void restart() {
    watchdog_reboot(0, 0, 200);
    busy_wait_ms(200);
}

int main() {
    stdio_init_all();

    while (!stdio_usb_connected()) {
        busy_wait_ms(200);
    }

    printf("ready to go\n");

    // Get a free channel, panic() if there are none
    int chan = dma_claim_unused_channel(true);

    // 8 bit transfers. Both read and write address increment after each
    // transfer (each pointing to a location in src or dst respectively).
    // No DREQ is selected, so the DMA transfers as fast as it can.

    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);

    dma_channel_configure(
        chan,          // Channel to be configured
        &c,            // The configuration we just created
        dst,           // The initial write address
        src,           // The initial read address
        count_of(src), // Number of transfers; in this case each is 1 byte.
        true           // Start immediately.
    );

    // We could choose to go and do something else whilst the DMA is doing its
    // thing. In this case the processor has nothing else to do, so we just
    // wait for the DMA to finish.
    dma_channel_wait_for_finish_blocking(chan);

    // The DMA has now copied our text from the transmit buffer (src) to the
    // receive buffer (dst), so we can print it out from there.
    puts(dst);

    // PIO Blinking example
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);

    blink_pin_forever(pio, 0, offset, 6, 3);

    // For more pio examples see
    // https://github.com/raspberrypi/pico-examples/tree/master/pio

    add_alarm_in_ms(1000, alarm_callback, NULL, true);

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
