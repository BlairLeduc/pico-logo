//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  File stream primitives: open, close, closeall, setread, setwrite, reader, writer,
//                          allopen, readpos, setreadpos, writepos, setwritepos, filelen,
//                          dribble, nodribble, .timeout, .settimeout
//

#include "primitives.h"
#include "error.h"
#include "devices/io.h"
#include <string.h>

//==========================================================================

// open file or network connection - opens for read/write
// For files: creates if doesn't exist
// For network: connects to host:port
static Result prim_open(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *target = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if already open
    if (logo_io_is_open(io, target))
    {
        if (logo_io_is_network_address(target))
        {
            return result_error_arg(ERR_NETWORK_ALREADY_OPEN, NULL, target);
        }
        return result_error_arg(ERR_FILE_ALREADY_OPEN, NULL, target);
    }

    LogoStream *stream = logo_io_open(io, target);
    if (!stream)
    {
        // Check if we hit the file limit
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        
        // Different error for network vs file
        if (logo_io_is_network_address(target))
        {
            return result_error_arg(ERR_CANT_OPEN_NETWORK, NULL, target);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// close file or network connection - closes the named target
static Result prim_close(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *target = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if open
    if (!logo_io_is_open(io, target))
    {
        if (logo_io_is_network_address(target))
        {
            return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, target);
        }
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, target);
    }

    logo_io_close(io, target);
    return result_none();
}

// closeall - closes all open files (not dribble)
static Result prim_closeall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_close_all(io);
    }

    return result_none();
}

// setread file - sets current reader to file (empty list for keyboard)
static Result prim_setread(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Empty list means reset to keyboard
    if (args[0].type == VALUE_LIST && mem_is_nil(args[0].as.node))
    {
        logo_io_set_reader(io, NULL);
        return result_none();
    }

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const char *target = mem_word_ptr(args[0].as.node);
    
    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, target);
    if (!stream)
    {
        if (logo_io_is_network_address(target))
        {
            return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, target);
        }
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, target);
    }

    logo_io_set_reader(io, stream);
    return result_none();
}

// setwrite file - sets current writer to file (empty list for screen)
static Result prim_setwrite(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Empty list means reset to screen
    if (args[0].type == VALUE_LIST && mem_is_nil(args[0].as.node))
    {
        logo_io_set_writer(io, NULL);
        return result_none();
    }

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const char *target = mem_word_ptr(args[0].as.node);
    
    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, target);
    if (!stream)
    {
        if (logo_io_is_network_address(target))
        {
            return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, target);
        }
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, target);
    }

    logo_io_set_writer(io, stream);
    return result_none();
}

// reader - outputs the current reader name (empty list for keyboard)
static Result prim_reader(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    if (logo_io_reader_is_keyboard(io))
    {
        return result_ok(value_list(NODE_NIL));
    }

    const char *name = logo_io_get_reader_name(io);
    return result_ok(value_word(mem_atom_cstr(name)));
}

// writer - outputs the current writer name (empty list for screen)
static Result prim_writer(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    if (logo_io_writer_is_screen(io))
    {
        return result_ok(value_list(NODE_NIL));
    }

    const char *name = logo_io_get_writer_name(io);
    return result_ok(value_word(mem_atom_cstr(name)));
}

// allopen - outputs a list of all open files
static Result prim_allopen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    // Build list of open file names
    Node list = NODE_NIL;
    int count = logo_io_open_count(io);
    
    // Build in reverse order so first file ends up first in list
    for (int i = count - 1; i >= 0; i--)
    {
        LogoStream *stream = logo_io_get_open(io, i);
        if (stream)
        {
            Node name_node = mem_atom_cstr(stream->name);
            list = mem_cons(name_node, list);
        }
    }

    return result_ok(value_list(list));
}

// readpos - outputs the current read position in the current file
static Result prim_readpos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io || !io->reader)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if reader is keyboard (no file selected)
    if (logo_io_reader_is_keyboard(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if reader is a network connection
    if (logo_io_is_network_stream(io->reader))
    {
        return result_error_arg(ERR_CANT_ON_NETWORK, "readpos", NULL);
    }

    long pos = logo_stream_get_read_pos(io->reader);
    if (pos < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)pos));
}

// setreadpos integer - sets the read position in the current file
static Result prim_setreadpos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], pos_f);
    long pos = (long)pos_f;
    if (pos < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->reader)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if reader is keyboard (no file selected)
    if (logo_io_reader_is_keyboard(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if reader is a network connection
    if (logo_io_is_network_stream(io->reader))
    {
        return result_error_arg(ERR_CANT_ON_NETWORK, "setreadpos", NULL);
    }

    if (!logo_stream_set_read_pos(io->reader, pos))
    {
        return result_error(ERR_FILE_POS_OUT_OF_RANGE);
    }

    return result_none();
}

// writepos - outputs the current write position in the current file
static Result prim_writepos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io || !io->writer)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if writer is screen (no file selected)
    if (logo_io_writer_is_screen(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if writer is a network connection
    if (logo_io_is_network_stream(io->writer))
    {
        return result_error_arg(ERR_CANT_ON_NETWORK, "writepos", NULL);
    }

    long pos = logo_stream_get_write_pos(io->writer);
    if (pos < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)pos));
}

// setwritepos integer - sets the write position in the current file
static Result prim_setwritepos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], pos_f);
    long pos = (long)pos_f;
    if (pos < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->writer)
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if writer is screen (no file selected)
    if (logo_io_writer_is_screen(io))
    {
        return result_error(ERR_NO_FILE_SELECTED);
    }

    // Error if writer is a network connection
    if (logo_io_is_network_stream(io->writer))
    {
        return result_error_arg(ERR_CANT_ON_NETWORK, "setwritepos", NULL);
    }

    if (!logo_stream_set_write_pos(io->writer, pos))
    {
        return result_error(ERR_FILE_POS_OUT_OF_RANGE);
    }

    return result_none();
}

// filelen pathname - outputs the length in bytes of the file
// The file must be open. Returns error for network connections.
static Result prim_filelen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *target = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, target);
    if (!stream)
    {
        if (logo_io_is_network_address(target))
        {
            return result_error_arg(ERR_NETWORK_NOT_OPEN, NULL, target);
        }
        return result_error_arg(ERR_FILE_NOT_OPEN, NULL, target);
    }

    // Error if stream is a network connection
    if (logo_io_is_network_stream(stream))
    {
        return result_error_arg(ERR_CANT_ON_NETWORK, "filelen", NULL);
    }

    long len = logo_stream_get_length(stream);
    if (len < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)len));
}

// dribble file - starts dribbling output to file
static Result prim_dribble(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if already dribbling
    if (logo_io_is_dribbling(io))
    {
        return result_error(ERR_ALREADY_DRIBBLING);
    }

    if (!logo_io_start_dribble(io, pathname))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// nodribble - stops dribbling
static Result prim_nodribble(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_stop_dribble(io);
    }

    return result_none();
}

//==========================================================================
// Network timeout primitives
//==========================================================================

// .timeout - outputs the current network timeout value (in tenths of a second)
static Result prim_timeout(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_number(0));
    }

    int timeout = logo_io_get_timeout(io);
    return result_ok(value_number((float)timeout));
}

// .settimeout integer - sets the network timeout value (in tenths of a second)
// A timeout value of 0 indicates no timeout
static Result prim_settimeout(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], timeout_f);
    
    int timeout = (int)timeout_f;
    if (timeout < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    logo_io_set_timeout(io, timeout);
    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_files_init(void)
{
    // File management
    primitive_register("open", 1, prim_open);
    primitive_register("close", 1, prim_close);
    primitive_register("closeall", 0, prim_closeall);
    primitive_register("setread", 1, prim_setread);
    primitive_register("setwrite", 1, prim_setwrite);
    primitive_register("reader", 0, prim_reader);
    primitive_register("writer", 0, prim_writer);
    primitive_register("allopen", 0, prim_allopen);
    primitive_register("readpos", 0, prim_readpos);
    primitive_register("setreadpos", 1, prim_setreadpos);
    primitive_register("writepos", 0, prim_writepos);
    primitive_register("setwritepos", 1, prim_setwritepos);
    primitive_register("filelen", 1, prim_filelen);
    primitive_register("dribble", 1, prim_dribble);
    primitive_register("nodribble", 0, prim_nodribble);
    
    // Network timeout
    primitive_register(".timeout", 0, prim_timeout);
    primitive_register(".settimeout", 1, prim_settimeout);
}
