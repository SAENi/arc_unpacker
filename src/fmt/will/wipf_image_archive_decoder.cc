#include "fmt/will/wipf_image_archive_decoder.h"
#include "algo/range.h"
#include "err.h"
#include "io/memory_stream.h"
#include "util/file_from_image.h"
#include "util/virtual_file_system.h"

using namespace au;
using namespace au::fmt::will;

static const bstr magic = "WIPF"_b;

namespace
{
    struct ArchiveEntryImpl final : fmt::ArchiveEntry
    {
        size_t offset;
        size_t width;
        size_t height;
        size_t size_comp;
        size_t size_orig;
        size_t depth;
        std::unique_ptr<res::Image> mask;
    };
}

// Modified LZSS routine
// - repetition count and look behind pos differs
// - non-standard initial dictionary pos
// - non-standard minimal match size
static bstr custom_lzss_decompress(const bstr &input, size_t output_size)
{
    io::MemoryStream input_stream(input);
    const size_t dict_size = 0x1000;
    size_t dict_pos = 1;
    u8 dict[dict_size] {};
    bstr output(output_size);
    auto output_ptr = output.get<u8>();
    auto output_end = output.end<const u8>();
    u16 control = 0;
    while (output_ptr < output_end && !input_stream.eof())
    {
        control >>= 1;
        if (!(control & 0x100))
            control = input_stream.read_u8() | 0xFF00;
        if (control & 1)
        {
            if (input_stream.eof())
                break;
            auto byte = input_stream.read_u8();
            dict[dict_pos++] = *output_ptr++ = byte;
            dict_pos %= dict_size;
            continue;
        }
        if (input_stream.eof())
            break;
        u8 di = input_stream.read_u8();
        if (input_stream.eof())
            break;
        u8 tmp = input_stream.read_u8();
        u32 look_behind_pos = (((di << 8) | tmp) >> 4);
        u16 repetitions = (tmp & 0xF) + 2;
        while (repetitions-- && output_ptr < output_end)
        {
            dict[dict_pos++] = *output_ptr++ = dict[look_behind_pos++];
            look_behind_pos %= dict_size;
            dict_pos %= dict_size;
        }
    }
    return output;
}

static std::unique_ptr<res::Image> read_image(
    io::File &input_file, const ArchiveEntryImpl &entry)
{
    input_file.stream.seek(entry.offset);

    std::unique_ptr<res::Palette> palette;
    if (entry.depth == 8)
    {
        palette = std::make_unique<res::Palette>(
            256, input_file.stream, res::PixelFormat::BGRA8888);
        for (auto &c : *palette)
            c.a ^= 0xFF;
    }

    auto data = input_file.stream.read(entry.size_comp);
    data = custom_lzss_decompress(data, entry.size_orig);

    const auto w = entry.width;
    const auto h = entry.height;

    std::unique_ptr<res::Image> image;
    if (entry.depth == 8)
    {
        image = std::make_unique<res::Image>(w, h, data, *palette);
    }
    else if (entry.depth == 24)
    {
        image = std::make_unique<res::Image>(w, h);
        for (const auto y : algo::range(h))
        for (const auto x : algo::range(w))
        {
            image->at(x, y).b = data[w * h * 0 + y * w + x];
            image->at(x, y).g = data[w * h * 1 + y * w + x];
            image->at(x, y).r = data[w * h * 2 + y * w + x];
            image->at(x, y).a = 0xFF;
        }
    }
    else
        throw err::UnsupportedBitDepthError(entry.depth);

    return image;
}

static std::unique_ptr<fmt::ArchiveMeta> read_meta(io::File &input_file)
{
    input_file.stream.seek(magic.size());
    auto meta = std::make_unique<fmt::ArchiveMeta>();
    const auto file_count = input_file.stream.read_u16_le();
    const auto depth = input_file.stream.read_u16_le();
    auto offset = input_file.stream.tell() + file_count * 24;
    for (const auto i : algo::range(file_count))
    {
        auto entry = std::make_unique<ArchiveEntryImpl>();
        entry->width = input_file.stream.read_u32_le();
        entry->height = input_file.stream.read_u32_le();
        input_file.stream.skip(12);
        entry->offset = offset;
        entry->size_comp = input_file.stream.read_u32_le();
        entry->size_orig = entry->width * entry->height * (depth >> 3);
        entry->depth = depth;
        offset += entry->size_comp + (entry->depth == 8 ? 256 * 4 : 0);
        meta->entries.push_back(std::move(entry));
    }
    return meta;
}

fmt::NamingStrategy WipfImageArchiveDecoder::naming_strategy() const
{
    return NamingStrategy::Sibling;
}

bool WipfImageArchiveDecoder::is_recognized_impl(io::File &input_file) const
{
    return input_file.stream.read(magic.size()) == magic;
}

std::unique_ptr<fmt::ArchiveMeta> WipfImageArchiveDecoder::read_meta_impl(
    const Logger &logger, io::File &input_file) const
{
    auto mask_file = util::VirtualFileSystem::get_by_name(
        io::path(input_file.path).change_extension("msk").name());
    std::unique_ptr<ArchiveMeta> mask_meta;
    if (mask_file && !input_file.path.has_extension("msk"))
        mask_meta = ::read_meta(*mask_file);

    auto meta = ::read_meta(input_file);
    if (!mask_file || !mask_meta)
        return meta;

    for (const auto i : algo::range(meta->entries.size()))
    {
        auto sprite_entry = static_cast<ArchiveEntryImpl*>(
            meta->entries[i].get());
        const auto mask_entry = static_cast<const ArchiveEntryImpl*>(
            mask_meta->entries.at(i).get());
        sprite_entry->mask = read_image(*mask_file, *mask_entry);
    }
    return meta;
}

std::unique_ptr<io::File> WipfImageArchiveDecoder::read_file_impl(
    const Logger &logger,
    io::File &input_file,
    const fmt::ArchiveMeta &m,
    const fmt::ArchiveEntry &e) const
{
    const auto entry = static_cast<const ArchiveEntryImpl*>(&e);
    auto image = read_image(input_file, *entry);
    if (entry->mask)
        image->apply_mask(*entry->mask);
    return util::file_from_image(*image, entry->path);
}

static auto dummy = fmt::register_fmt<WipfImageArchiveDecoder>("will/wipf");
