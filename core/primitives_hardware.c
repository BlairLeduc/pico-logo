//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Hardware primitives: hw.battery, hw.temperature, hw.light?, hw.setlight,
//                       hw.frequency, hw.setfrequency
//

#include "primitives.h"
#include "core/limits.h"
#include "procedures.h"
#include "memory.h"
#include "format.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"

#include <math.h>
#include <stdio.h>
#include <strings.h>

// hw.battery
static Result prim_battery_level(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    int level = -1;
    bool charging = false;

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->get_battery_level)
    {
        io->hardware->ops->get_battery_level(&level, &charging);
    }

    char buf[32];
    format_number(buf, sizeof(buf), level);
    Node level_atom = mem_atom_cstr(buf);
    if (mem_is_nil(level_atom))
    {
        return result_error(ERR_OUT_OF_SPACE); // atom table exhausted
    }

    Node list = mem_cons(charging ? mem_true_node : mem_false_node, NODE_NIL);
    if (mem_is_nil(list))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    list = mem_cons(level_atom, list);
    if (mem_is_nil(list))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    return result_ok(value_list(list));
}

// hw.temperature
// The on-chip sensor reads the die, not the room, and is uncalibrated, so
// tenths are the most the number can honestly carry -- round there rather
// than print six significant digits of ADC noise.
static Result prim_temperature(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->get_temperature)
    {
        float celsius = io->hardware->ops->get_temperature();
        return result_ok(value_number(roundf(celsius * 10.0f) / 10.0f));
    }

    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// hw.frequency
// The system clock in MHz, read from the hardware rather than remembered, so it
// still answers after anything else has retuned it.
static Result prim_frequency(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->get_cpu_khz)
    {
        return result_ok(value_number((float)io->hardware->ops->get_cpu_khz() / 1000.0f));
    }

    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// hw.setfrequency
//
// THE RP2350 IS RATED TO 150 MHz AND THIS ACCEPTS 300. Everything above the
// rating is an overclock, which is why the range is a pair of named limits
// rather than a magic number, and why `hw.temperature` is worth reading after a
// long run at one. The device layer is what makes the change survivable -- the
// core rail, the LCD's SPI divisor and the sound engine's mix rate all have to
// be dealt with, see devices/picocalc/picocalc_hardware.c.
//
// The frequency is refused rather than rounded if the PLL cannot make it
// exactly. Rounding would leave a program believing a number the hardware never
// took, and every figure it went on to measure would be against the wrong clock.
static Result prim_setfrequency(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], mhz);

    if (mhz < (float)LOGO_CPU_MHZ_MIN || mhz > (float)LOGO_CPU_MHZ_MAX)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->set_cpu_khz)
    {
        if (!io->hardware->ops->set_cpu_khz((uint32_t)(mhz * 1000.0f)))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
        }
        return result_none();
    }

    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// hw.light?
// Reads the LED rather than remembering what was last written, so it still
// answers after anything else has driven it.
static Result prim_lightp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    bool on = false;
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->get_status_led &&
        io->hardware->ops->get_status_led(&on))
    {
        return result_ok(value_bool(on));
    }

    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// hw.setlight
static Result prim_setlight(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    const char *str = value_to_string(args[0]);
    bool on;

    if (str == NULL)
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
    }
    if (strcasecmp(str, "true") == 0)
    {
        on = true;
    }
    else if (strcasecmp(str, "false") == 0)
    {
        on = false;
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, str);
    }

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->set_status_led &&
        io->hardware->ops->set_status_led(on))
    {
        return result_none();
    }

    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

static Result prim_goodbye(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->power_off)
    {
        bool success = io->hardware->ops->power_off();
        if (success)
        {
            // Power off has a forced delay before powering off, so close I/O now
            logo_io_close_all(io); // Close all I/O before powering off

            // Wait indefinitely, the device will power off and we won't return
            while (1)
            {
                io->hardware->ops->sleep(1000);
                logo_io_write(io, ".");
            }
        }
    }
    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// .bootsel
// Reboot the device into the USB bootloader (BOOTSEL mode) so a new firmware
// UF2 can be dragged onto it. Does not return on real hardware; errors on
// devices with no USB bootloader (e.g. the host).
static Result prim_bootsel(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->reboot_bootloader)
    {
        logo_io_close_all(io); // Flush/close I/O before the reset takes effect
        io->hardware->ops->reboot_bootloader();
        // Does not return on real hardware; if it does (e.g. in tests), succeed.
        return result_none();
    }

    return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
}

// `toot`'s fixed voices: left ear voice 0, right ear voice 4 (the first
// tone voice of each ear). See docs/sound-design.md §5.1.
#define TOOT_LEFT_VOICE 0
#define TOOT_RIGHT_VOICE 4

// A voice is "audible" for toot only within the documented 100-2000 Hz
// range; anything else is a rest (frequency 0). This is the range
// enforcement that used to live in the PIO backend.
static uint32_t toot_gate_freq(uint32_t freq)
{
    return (freq >= 100 && freq <= 2000) ? freq : 0;
}

// toot duration frequency
// (toot duration leftfrequency rightfrequency)
// Plays a tone for the specified duration.
// Duration is in 1/1000ths of a second (milliseconds).
// Frequency is in Hz. The actual playable range is 100-2000 Hz; per
// reference §2823, frequencies outside that range are not an error --
// they behave as a rest (by convention 0 Hz).
//
// `toot` is a square wave at volume 15 with an instant envelope on voices
// 0 and 4 (docs/sound-design.md §5.1) -- it forces that timbre so a beep
// sounds the same regardless of any setwave/setenv on those voices, and it
// waits (like a second toot) for the previous toot to finish.
static Result prim_toot(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    // Validate argument count (2 or 3)
    if (argc < 2 || argc > 3)
    {
        if (argc < 2)
        {
            return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
        }
        return result_error_arg(ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    // Get duration (first argument)
    REQUIRE_NUMBER(args[0], duration_f);
    int duration_ms = (int)duration_f;
    if (duration_ms < 0)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", duration_ms);
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, buf);
    }

    // Get frequency/frequencies
    uint32_t left_freq, right_freq;

    if (argc == 2)
    {
        // Single frequency for both channels
        REQUIRE_NUMBER(args[1], freq_f);
        int freq = (int)freq_f;
        if (freq < 0)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", freq);
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, buf);
        }
        left_freq = right_freq = (uint32_t)freq;
    }
    else
    {
        // Separate frequencies for left and right channels
        REQUIRE_NUMBER(args[1], lfreq_f);
        REQUIRE_NUMBER(args[2], rfreq_f);
        int lfreq = (int)lfreq_f;
        int rfreq = (int)rfreq_f;
        if (lfreq < 0)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", lfreq);
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, buf);
        }
        if (rfreq < 0)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", rfreq);
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, buf);
        }
        left_freq = (uint32_t)lfreq;
        right_freq = (uint32_t)rfreq;
    }

    // Play the tone if hardware supports it. Route through the sound
    // engine's per-voice gate (docs/sound-design.md §5.1).
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->sound_gate)
    {
        LogoHardwareOps *ops = io->hardware->ops;

        // Wait for a previous toot on either channel to finish, like the
        // reference's second-toot behaviour. Interruptible by BREAK.
        if (ops->sound_status)
        {
            while (ops->sound_status(TOOT_LEFT_VOICE).sounding ||
                   ops->sound_status(TOOT_RIGHT_VOICE).sounding)
            {
                if (logo_io_check_user_interrupt(io))
                {
                    return result_none();
                }
                logo_io_sleep(io, 1);
            }
        }

        // Gate both toot voices at full volume. They keep their default
        // square, click-free timbre unless the program has changed voice 0/4
        // with setwave/setenv.
        ops->sound_gate(TOOT_LEFT_VOICE, toot_gate_freq(left_freq), (uint32_t)duration_ms, 15);
        ops->sound_gate(TOOT_RIGHT_VOICE, toot_gate_freq(right_freq), (uint32_t)duration_ms, 15);
    }
    // If no audio hardware, silently succeed (command has no output)

    return result_none();
}

void primitives_hardware_init(void)
{
    primitive_register("hw.battery", 0, prim_battery_level);
    primitive_register("hw.temperature", 0, prim_temperature);
    primitive_register("hw.light?", 0, prim_lightp);
    primitive_register("hw.setlight", 1, prim_setlight);
    primitive_register("hw.frequency", 0, prim_frequency);
    primitive_register("hw.setfrequency", 1, prim_setfrequency);
    primitive_register("goodbye", 0, prim_goodbye);
    primitive_register(".bootsel", 0, prim_bootsel);
    primitive_register("toot", 2, prim_toot);
}