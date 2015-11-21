#include "fmt/image_decoder.h"
#include "err.h"
#include "fmt/naming_strategies.h"
#include "util/file_from_grid.h"

using namespace au;
using namespace au::fmt;

ImageDecoder::~ImageDecoder()
{
}

std::unique_ptr<INamingStrategy> ImageDecoder::naming_strategy() const
{
    return std::make_unique<SiblingNamingStrategy>();
}

void ImageDecoder::register_cli_options(ArgParser &) const
{
}

void ImageDecoder::parse_cli_options(const ArgParser &)
{
}

bool ImageDecoder::is_recognized(io::File &file) const
{
    try
    {
        file.stream.seek(0);
        return is_recognized_impl(file);
    }
    catch (...)
    {
        return false;
    }
}

void ImageDecoder::unpack(
    io::File &input_file, const FileSaver &file_saver) const
{
    auto output_grid = decode(input_file);
    auto output_file = util::file_from_grid(output_grid, input_file.name);
    // discard any directory information
    output_file->name = io::path(output_file->name).name();
    file_saver.save(std::move(output_file));
}

pix::Grid ImageDecoder::decode(io::File &file) const
{
    if (!is_recognized(file))
        throw err::RecognitionError();
    file.stream.seek(0);
    return decode_impl(file);
}
