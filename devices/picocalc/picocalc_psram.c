//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PSRAM bring-up for the Pimoroni Pico Plus 2 W (RP2350 + 8 MB APS6404 PSRAM).
//
//  The RP2350 has no SDK API for external PSRAM, so the QMI configuration here
//  follows the well-established community sequence (SparkFun / Pimoroni RP2350
//  PSRAM support): probe the device id over QMI direct mode to get the density,
//  then program the QMI M1 (chip-select 1) read/write formats and timing so the
//  PSRAM is memory-mapped and writable at PICOCALC_PSRAM_BASE.
//
//  This is hardware register code that cannot be exercised by the host test
//  suite. To stay safe regardless of QMI-config correctness, picocalc_psram_init
//  runs a cache-bypassing read-back self-test and returns 0 (→ SRAM-only) unless
//  the PSRAM actually stores and returns data. That prevents a wrong config from
//  handing the allocator a non-functional region (which hardfaults on use).
//
//  The QMI-touching functions run from SRAM (__no_inline_not_in_flash_func)
//  because they reconfigure the QMI that also serves XIP flash.
//

#include "devices/picocalc/picocalc_psram.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"

size_t picocalc_psram_detected_size = 0;
bool picocalc_psram_selftest_ok = false;

// Read the PSRAM device id over QMI direct mode and return its size in bytes,
// or 0 if no valid PSRAM (APS6404-family, "known good die" == 0x5D) responds.
static size_t __no_inline_not_in_flash_func(psram_detect_size)(void)
{
    size_t size = 0;
    uint8_t kgd = 0;
    uint8_t eid = 0;

    uint32_t save = save_and_disable_interrupts();

    // Enable QMI direct mode with a conservative clock divider.
    qmi_hw->direct_csr = 30u << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
    {
    }

    // Exit QPI mode (0xF5) in case the PSRAM powered up in it.
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
                        (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB) |
                        0xF5u;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
    {
    }
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

    // Read id (0x9F): command byte + 3 address bytes, then KGD and EID bytes.
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    for (size_t i = 0; i < 7; i++)
    {
        qmi_hw->direct_tx = (i == 0) ? 0x9Fu : 0xFFu;
        while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        {
        }
        if (i == 5)
        {
            kgd = (uint8_t)qmi_hw->direct_rx;
        }
        else if (i == 6)
        {
            eid = (uint8_t)qmi_hw->direct_rx;
        }
        else
        {
            (void)qmi_hw->direct_rx;
        }
    }

    // Leave direct mode.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    if (kgd == 0x5Du)
    {
        // Size = 1 MiB << density (EID bits 7:5). APS6404 8 MB reports density 3.
        size = (size_t)(1024u * 1024u) << ((eid >> 5) & 0x07u);
    }

    restore_interrupts(save);
    return size;
}

// Put the PSRAM into QPI mode (0x35) so the quad read/write commands below are
// clocked on all four lines. The command itself is sent single-line (the chip
// is in SPI mode after the 0xF5 exit-QPI issued during detection).
static void __no_inline_not_in_flash_func(psram_enter_qpi)(void)
{
    uint32_t save = save_and_disable_interrupts();

    qmi_hw->direct_csr = 30u << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
    {
    }
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = 0x35u; // Enter Quad mode (single-line command)
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
    {
    }
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    restore_interrupts(save);
}

// Program the QMI M1 read/write formats + timing for the APS6404.
static void __no_inline_not_in_flash_func(psram_configure_qmi)(void)
{
    uint32_t save = save_and_disable_interrupts();

    // Keep the QMI clock <= ~109 MHz; derive divisor and RX sample delay.
    uint32_t clock_hz = clock_get_hz(clk_sys);
    uint32_t divisor = (clock_hz + 109000000u - 1u) / 109000000u;
    uint32_t rxdelay = divisor;
    if (clock_hz / divisor > 100000000u)
    {
        rxdelay += 1u;
    }

    // APS6404 timing windows expressed in system clock cycles.
    uint32_t clock_mhz = clock_hz / 1000000u;
    uint32_t max_select = (125u * clock_mhz) / 1000u;             // ~8 us / 64
    uint32_t min_deselect = (50u * clock_mhz + 999u) / 1000u;     // ~50 ns

    qmi_hw->m[1].timing =
        (1u << QMI_M1_TIMING_COOLDOWN_LSB) |
        (QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB) |
        (max_select << QMI_M1_TIMING_MAX_SELECT_LSB) |
        (min_deselect << QMI_M1_TIMING_MIN_DESELECT_LSB) |
        (rxdelay << QMI_M1_TIMING_RXDELAY_LSB) |
        (divisor << QMI_M1_TIMING_CLKDIV_LSB);

    // Quad read (0xEB): 8-bit command, 24-bit address, dummy, data — all quad.
    qmi_hw->m[1].rcmd = 0xEBu << QMI_M1_RCMD_PREFIX_LSB;
    qmi_hw->m[1].rfmt =
        (QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB) |
        (QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB) |
        (QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB) |
        (QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB) |
        (QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB) |
        (QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB) |
        (QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB);

    // Quad write (0x38): 8-bit command, 24-bit address, no dummy — all quad.
    qmi_hw->m[1].wcmd = 0x38u << QMI_M1_WCMD_PREFIX_LSB;
    qmi_hw->m[1].wfmt =
        (QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB) |
        (QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB) |
        (QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB) |
        (QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB) |
        (QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB) |
        (QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB) |
        (QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB);

    restore_interrupts(save);

    // Allow XIP writes to the PSRAM window (M1) so it is usable as RAM.
    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
}

// Write distinct patterns to a few low offsets, then force a real PSRAM
// round-trip (flush + invalidate the XIP cache) and confirm they read back.
// Only touches the first 64 KB so a bogus-large detected size cannot fault.
static bool __no_inline_not_in_flash_func(psram_selftest)(void)
{
    volatile uint32_t *base = (volatile uint32_t *)PICOCALC_PSRAM_BASE;
    static const size_t off[4] = {0u, 1u, 0x1000u / 4u, 0xFFFCu / 4u};
    static const uint32_t pat[4] = {0xA5A5A5A5u, 0x5A5A5A5Au, 0xDEADBEEFu, 0x0BADF00Du};

    for (size_t i = 0; i < 4; i++)
    {
        base[off[i]] = pat[i];
    }
    xip_cache_clean_all();      // push dirty lines out to PSRAM
    xip_cache_invalidate_all(); // drop the cache so reads come from PSRAM

    for (size_t i = 0; i < 4; i++)
    {
        if (base[off[i]] != pat[i])
        {
            return false;
        }
    }
    return true;
}

// Determine the true PSRAM size by address aliasing: a chip smaller than the
// probed offset wraps, so writing at that offset corrupts offset 0. Returns the
// first power-of-two MB whose write wraps to 0, or 8 MB if none up to 4 MB do
// (APS6404 max). Only touches offsets up to 4 MB, so it cannot run off the QMI
// window. Must run after the QMI read/write format is configured.
static size_t __no_inline_not_in_flash_func(psram_probe_size)(void)
{
    volatile uint32_t *base = (volatile uint32_t *)PICOCALC_PSRAM_BASE;

    base[0] = 0u;
    xip_cache_clean_all();
    xip_cache_invalidate_all();

    for (size_t mb = 1; mb < 8; mb <<= 1)
    {
        base[(mb << 20) / 4u] = (uint32_t)mb; // nonzero marker at `mb` MB
        xip_cache_clean_all();
        xip_cache_invalidate_all();
        if (base[0] != 0u)
        {
            return mb << 20; // write at `mb` MB wrapped to 0 -> that is the size
        }
    }
    return (size_t)8u << 20; // no wrap by 4 MB -> full 8 MB part
}

size_t picocalc_psram_init(uint32_t cs_pin)
{
    picocalc_psram_detected_size = 0;
    picocalc_psram_selftest_ok = false;

    // Route the chip-select pin to the QMI second chip select.
    gpio_set_function(cs_pin, GPIO_FUNC_XIP_CS1);

    // Confirm a PSRAM chip is present (APS6404 "known good die"). The id density
    // is only a hint here — the true size is probed below by aliasing.
    size_t id_size = psram_detect_size();
    picocalc_psram_detected_size = id_size;
    if (id_size == 0)
    {
        return 0;
    }

    psram_enter_qpi();
    psram_configure_qmi();

    if (!psram_selftest())
    {
        return 0; // PSRAM does not reliably store data -> fall back to SRAM
    }
    picocalc_psram_selftest_ok = true;

    // Probe the real size (the id density under-reports on this board).
    size_t size = psram_probe_size();
    picocalc_psram_detected_size = size;
    return size;
}
