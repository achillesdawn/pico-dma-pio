#include "hardware/dma.h"
#include "hardware/pio.h"

// the problem is dma channels have a transfer count, when
// the transfer count hits 0 it stops transfering even if
// there are dreq this is being solved by chaining two
// channels one to the end of the other config channel in
// this case does nothing but establish the chain so that
// the main channel gets restarted
// on the rp2350, endless transfer counts are possible
// using the dma_encode_endless_transfer_count func
int setup_pio_dma_two_channels(
    PIO pio, int sm, uint32_t *read_addr
) {

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
        dma_encode_transfer_count(100),
        true
    );

    dma_channel_configure(
        control_ch, &control_ch_config, NULL, NULL, 1, false

    );

    return dma_ch;
}

// even if there is a ring wrap on the read addresses, this
// still transfer up to transfer_count transfers
int setup_pio_dma_ring(
    PIO pio, int sm, const volatile void *read_addr
) {
    int dma_ch = dma_claim_unused_channel(true);

    dma_channel_config config =
        dma_channel_get_default_config(dma_ch);

    channel_config_set_transfer_data_size(
        &config, DMA_SIZE_32
    );

    channel_config_set_write_increment(&config, false);
    channel_config_set_read_increment(&config, true);

    channel_config_set_dreq(
        &config, pio_get_dreq(pio, sm, true)
    );

    // 1 << size_bits, in this case the address wrapping is
    // 2^3 = 8 which is two 4 byte uint32_t's
    channel_config_set_ring(&config, false, 3);

    dma_channel_configure(
        dma_ch,
        &config,
        &pio->txf[sm],
        read_addr,
        dma_encode_transfer_count(100),
        true
    );

    return dma_ch;
}
