//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
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
