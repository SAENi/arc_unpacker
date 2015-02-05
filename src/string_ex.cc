#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iconv.h>
#include <zlib.h>
#include "logger.h"
#include "string_ex.h"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

char *strndup(const char *source, const size_t size)
{
    char *target = new char[size + 1];
    assert(target != NULL);
    memcpy(target, source, size);
    target[size] = '\0';
    return target;
}

char *strdup(const char *source)
{
    return strndup(source, strlen(source));
}

void trim_right(char *target, const char *chars)
{
    char *end = target + strlen(target) - 1;
    while (end >= target && strchr(chars, *end) != NULL)
        end --;
    end[1] = '\0';
}

bool zlib_inflate(
    const char *input,
    size_t input_size,
    char **output,
    size_t *output_size)
{
    z_stream stream;
    assert(input != NULL);
    assert(output != NULL);

    size_t tmp;
    if (output_size == NULL)
        output_size = &tmp;

    if (input_size < SIZE_MAX / 10)
        *output_size = input_size;
    else
        *output_size = input_size;
    *output = (char*)malloc(*output_size + 1);
    if (*output == NULL)
    {
        log_error("Zlib: failed to allocate initial memory");
        *output_size = 0;
        return false;
    }

    stream.next_in = (unsigned char *)input;
    stream.avail_in = input_size;
    stream.next_out = (unsigned char *)*output;
    stream.avail_out = *output_size;
    stream.zalloc = (alloc_func)NULL;
    stream.zfree = (free_func)NULL;
    stream.opaque = (voidpf)NULL;
    stream.total_out = 0;

    if (inflateInit(&stream) != Z_OK)
    {
        *output_size = 0;
        *output = NULL;
        log_error("Zlib: failed to initialize stream");
        return false;
    }

    while (1)
    {
        int result = inflate(&stream, Z_FINISH);
        switch (result)
        {
            case Z_STREAM_END:
            {
                if (inflateEnd(&stream) != Z_OK)
                {
                    log_warning("Zlib: failed to uninitialize stream");
                }
                char *new_output = (char*)realloc(*output, stream.total_out+1);
                if (new_output == NULL)
                {
                    log_error("Zlib: failed to allocate memory for output");
                    free(*output);
                    *output = NULL;
                    *output_size = 0;
                    return false;
                }
                *output = new_output;
                *output_size = stream.total_out;
                (*output)[*output_size] = '\0';
                return true;
            }

            case Z_BUF_ERROR:
            case Z_OK:
            {
                if (*output_size < SIZE_MAX / 2)
                {
                    *output_size *= 2;
                }
                else if (*output_size == SIZE_MAX - 1)
                {
                    log_error("Zlib: input is too large");
                    free(*output);
                    *output = NULL;
                    *output_size = 0;
                    return false;
                }
                else
                {
                    *output_size = SIZE_MAX - 1;
                }

                char *new_output = (char*)realloc(*output, *output_size + 1);
                if (new_output == NULL)
                {
                    log_error("Zlib: failed to allocate memory for output");
                    free(*output);
                    *output = NULL;
                    *output_size = 0;
                    return false;
                }
                *output = new_output;

                stream.next_out = (unsigned char *)*output + stream.total_out;
                stream.avail_out = *output_size - stream.total_out;
                break;
            }

            default:
                log_error("Zlib: inflate failed (error code = %d)", result);
                free(*output);
                *output = NULL;
                *output_size = 0;
                return false;
        }
    }

    log_error("Zlib: reached unexpected state");
    return false;
}

bool convert_encoding(
    const char *input,
    size_t input_size,
    char **output,
    size_t *output_size,
    const char *from,
    const char *to)
{
    assert(input != NULL);
    assert(output != NULL);
    assert(from != NULL);
    assert(to != NULL);

    size_t tmp;
    if (output_size == NULL)
        output_size = &tmp;

    iconv_t conv = iconv_open(to, from);

    *output = NULL;
    *output_size = 0;

    char *output_old, *output_new;
    char *input_ptr = (char*) input;
    char *output_ptr = NULL;
    size_t input_bytes_left = input_size;
    size_t output_bytes_left = *output_size;

    while (1)
    {
        output_old = *output;
        output_new = (char*)realloc(*output, *output_size);
        if (!output_new)
        {
            log_error("Encoding: Failed to allocate memory");
            free(*output);
            *output = NULL;
            *output_size = 0;
            return false;
        }
        *output = output_new;

        if (output_old == NULL)
            output_ptr = *output;
        else
            output_ptr += *output - output_old;

        int result = iconv(
            conv,
            &input_ptr,
            &input_bytes_left,
            &output_ptr,
            &output_bytes_left);

        if (result != -1)
            break;

        switch (errno)
        {
            case EINVAL:
            case EILSEQ:
                log_error("Encoding: Invalid byte sequence");
                free(*output);
                *output = NULL;
                *output_size = 0;
                return false;

            case E2BIG:
                *output_size += 1;
                output_bytes_left += 1;
                continue;
        }
    }

    *output = (char*)realloc(*output, (*output_size) + 1);
    (*output)[(*output_size)] = '\0';
    iconv_close(conv);
    return true;
}
