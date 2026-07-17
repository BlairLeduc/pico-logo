//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Implements the LogoStream wrapper functions.
//

#include "devices/stream.h"

#include <string.h>

//
// Stream initialization
//

void logo_stream_init(LogoStream *stream, LogoStreamType type,
                      const LogoStreamOps *ops, void *context, const char *name)
{
    if (!stream)
    {
        return;
    }

    stream->type = type;
    stream->ops = ops;
    stream->context = context;
    stream->is_open = true;
    stream->write_error = false;
    stream->disk_full = false;

    if (name)
    {
        strncpy(stream->name, name, LOGO_STREAM_NAME_MAX - 1);
        stream->name[LOGO_STREAM_NAME_MAX - 1] = '\0';
    }
    else
    {
        stream->name[0] = '\0';
    }
}

//
// Reading operations
//

int logo_stream_read_char(LogoStream *stream)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->read_char)
    {
        return -1;
    }

    return stream->ops->read_char(stream);
}

int logo_stream_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->read_chars)
    {
        return -1;
    }

    if (!buffer || count <= 0)
    {
        return 0;
    }

    return stream->ops->read_chars(stream, buffer, count);
}

int logo_stream_read_line(LogoStream *stream, char *buffer, size_t size)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->read_line)
    {
        return -1;
    }

    if (!buffer || size == 0)
    {
        return 0;
    }

    return stream->ops->read_line(stream, buffer, size);
}

bool logo_stream_can_read(LogoStream *stream)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->can_read)
    {
        return false;
    }

    return stream->ops->can_read(stream);
}

//
// Writing operations
//

void logo_stream_write(LogoStream *stream, const char *text)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->write || !text)
    {
        return;
    }

    stream->ops->write(stream, text);
}

void logo_stream_write_bytes(LogoStream *stream, const char *buffer, size_t len)
{
    if (!stream || !stream->is_open || !stream->ops || !buffer)
    {
        return;
    }

    if (!stream->ops->write_bytes)
    {
        // No binary-safe write on this backend: refuse rather than risk a
        // NUL-truncated text write that would silently corrupt the data.
        stream->write_error = true;
        return;
    }

    stream->ops->write_bytes(stream, buffer, len);
}

bool logo_stream_copy(LogoStream *in, LogoStream *out)
{
    if (!in || !out || !out->ops || !out->ops->write_bytes)
    {
        return false;
    }

    // A freshly opened stream may carry a stale write-error flag from a backend
    // that does not initialise it; start clean so the flag means "this copy
    // failed".
    logo_stream_clear_write_error(out);

    char buf[256];
    int n;
    while ((n = logo_stream_read_chars(in, buf, (int)sizeof(buf))) > 0)
    {
        logo_stream_write_bytes(out, buf, (size_t)n);
        if (logo_stream_has_write_error(out))
        {
            return false;
        }
    }
    if (n < 0)
    {
        return false; // read error mid-copy
    }
    logo_stream_flush(out);
    return true;
}

void logo_stream_write_line(LogoStream *stream, const char *text)
{
    if (!stream || !stream->is_open)
    {
        return;
    }

    if (text)
    {
        logo_stream_write(stream, text);
    }
    logo_stream_write(stream, "\n");
}

void logo_stream_flush(LogoStream *stream)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->flush)
    {
        return;
    }

    stream->ops->flush(stream);
}

//
// Write error checking
//

bool logo_stream_has_write_error(LogoStream *stream)
{
    if (!stream)
    {
        return false;
    }
    return stream->write_error;
}

void logo_stream_clear_write_error(LogoStream *stream)
{
    if (stream)
    {
        stream->write_error = false;
        stream->disk_full = false;
    }
}

//
// Position operations (return -1 or false if not seekable)
//

long logo_stream_get_read_pos(LogoStream *stream)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->get_read_pos)
    {
        return -1;
    }

    return stream->ops->get_read_pos(stream);
}

bool logo_stream_set_read_pos(LogoStream *stream, long pos)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->set_read_pos)
    {
        return false;
    }

    return stream->ops->set_read_pos(stream, pos);
}

long logo_stream_get_write_pos(LogoStream *stream)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->get_write_pos)
    {
        return -1;
    }

    return stream->ops->get_write_pos(stream);
}

bool logo_stream_set_write_pos(LogoStream *stream, long pos)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->set_write_pos)
    {
        return false;
    }

    return stream->ops->set_write_pos(stream, pos);
}

long logo_stream_get_length(LogoStream *stream)
{
    if (!stream || !stream->is_open || !stream->ops || !stream->ops->get_length)
    {
        return -1;
    }

    return stream->ops->get_length(stream);
}

//
// Lifecycle
//

void logo_stream_close(LogoStream *stream)
{
    if (!stream || !stream->is_open)
    {
        return;
    }

    if (stream->ops && stream->ops->close)
    {
        stream->ops->close(stream);
    }

    stream->is_open = false;
}
