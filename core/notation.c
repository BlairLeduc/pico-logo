//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Note-word notation parser for `play` (P8). See notation.h.
//

#include "notation.h"

#include <math.h>
#include <ctype.h>
#include <stddef.h>

void notation_state_init(NotationState *s)
{
    s->tempo = 120;
    s->octave = 4;
    s->length = 4;
    s->volume = 12;
}

// Semitone (pitch class) of a note letter within an octave, C = 0.
// Returns -1 if the letter is not a-g.
static int pitch_class(char c)
{
    switch (c)
    {
    case 'c': return 0;
    case 'd': return 2;
    case 'e': return 4;
    case 'f': return 5;
    case 'g': return 7;
    case 'a': return 9;
    case 'b': return 11;
    default: return -1;
    }
}

// Read a run of decimal digits at *p into *out; advance *p. Returns true if
// at least one digit was consumed.
static bool read_uint(const char **p, int *out)
{
    if (!isdigit((unsigned char)**p))
    {
        return false;
    }
    int v = 0;
    while (isdigit((unsigned char)**p))
    {
        v = v * 10 + (**p - '0');
        if (v > 100000) // clamp runaway; callers range-check anyway
        {
            v = 100000;
        }
        (*p)++;
    }
    *out = v;
    return true;
}

// Milliseconds for a note of `length` (1=whole..) at `tempo` BPM, dotted or
// not. A quarter note (length 4) at 120 BPM is 500 ms.
static uint16_t duration_ms(int tempo, int length, bool dotted)
{
    float ms = (60000.0f / (float)tempo) * (4.0f / (float)length);
    if (dotted)
    {
        ms *= 1.5f;
    }
    if (ms < 0.0f)
    {
        ms = 0.0f;
    }
    if (ms > 65535.0f)
    {
        ms = 65535.0f;
    }
    return (uint16_t)(ms + 0.5f);
}

// Frequency in Hz for a note, given octave (1-8) and pitch class + accidental.
// A4 (octave 4, pitch class 9) = 440 Hz.
static uint16_t note_freq(int octave, int semitone)
{
    int midi = (octave + 1) * 12 + semitone;
    float f = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
    if (f < 0.0f)
    {
        f = 0.0f;
    }
    if (f > 65535.0f)
    {
        f = 65535.0f;
    }
    return (uint16_t)(f + 0.5f);
}

// Handle an in-list control word (t/o/l/v + digits). Returns true and updates
// state on success; false if the value is out of range or malformed.
static bool parse_control(NotationState *s, char kind, const char *digits)
{
    const char *p = digits;
    int v;
    if (!read_uint(&p, &v) || *p != '\0')
    {
        return false;
    }
    switch (kind)
    {
    case 't':
        if (v < 40 || v > 300) return false;
        s->tempo = v;
        return true;
    case 'o':
        if (v < 1 || v > 8) return false;
        s->octave = v;
        return true;
    case 'l':
        if (v < 1 || v > 32) return false;
        s->length = v;
        return true;
    case 'v':
        if (v < 0 || v > 15) return false;
        s->volume = v;
        return true;
    default:
        return false;
    }
}

NotationKind notation_parse_token(NotationState *s, const char *token, SoundEvent *out)
{
    if (!token || token[0] == '\0')
    {
        return NOTATION_ERROR;
    }

    // Lowercase into a small local buffer so the parser is case-insensitive.
    char buf[16];
    size_t n = 0;
    for (const char *q = token; *q; q++)
    {
        if (n >= sizeof(buf) - 1)
        {
            return NOTATION_ERROR; // token too long to be valid notation
        }
        buf[n++] = (char)tolower((unsigned char)*q);
    }
    buf[n] = '\0';

    // Control word: a leading t/o/l/v (not a note letter) followed by digits.
    if ((buf[0] == 't' || buf[0] == 'o' || buf[0] == 'l' || buf[0] == 'v') &&
        isdigit((unsigned char)buf[1]))
    {
        return parse_control(s, buf[0], buf + 1) ? NOTATION_CONTROL : NOTATION_ERROR;
    }

    const char *p = buf;

    // Optional leading duration (length override).
    int length = s->length;
    int dur;
    if (read_uint(&p, &dur))
    {
        if (dur < 1 || dur > 32)
        {
            return NOTATION_ERROR;
        }
        length = dur;
    }

    // Rest: 'r' [.]
    if (*p == 'r')
    {
        p++;
        bool dotted = false;
        if (*p == '.')
        {
            dotted = true;
            p++;
        }
        if (*p != '\0')
        {
            return NOTATION_ERROR;
        }
        out->freq_hz = 0;
        out->dur_ms = duration_ms(s->tempo, length, dotted);
        out->vol = (uint8_t)s->volume;
        out->flags = 0;
        return NOTATION_NOTE;
    }

    // Note letter a-g.
    int pc = pitch_class(*p);
    if (pc < 0)
    {
        return NOTATION_ERROR;
    }
    p++;

    // Optional accidental: '#'/'s' sharp, 'b' flat.
    if (*p == '#' || *p == 's')
    {
        pc += 1;
        p++;
    }
    else if (*p == 'b')
    {
        pc -= 1;
        p++;
    }

    // Optional octave digit 1-8.
    int octave = s->octave;
    if (isdigit((unsigned char)*p))
    {
        octave = *p - '0';
        if (octave < 1 || octave > 8)
        {
            return NOTATION_ERROR;
        }
        p++;
    }

    // Optional dot.
    bool dotted = false;
    if (*p == '.')
    {
        dotted = true;
        p++;
    }

    if (*p != '\0')
    {
        return NOTATION_ERROR;
    }

    out->freq_hz = note_freq(octave, pc);
    out->dur_ms = duration_ms(s->tempo, length, dotted);
    out->vol = (uint8_t)s->volume;
    out->flags = 0;
    return NOTATION_NOTE;
}
