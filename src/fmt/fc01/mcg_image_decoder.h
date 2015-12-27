#pragma once

#include "fmt/base_image_decoder.h"

namespace au {
namespace fmt {
namespace fc01 {

    class McgImageDecoder final : public BaseImageDecoder
    {
    public:
        McgImageDecoder();
        ~McgImageDecoder();
        void register_cli_options(ArgParser &arg_parser) const;
        void parse_cli_options(const ArgParser &arg_parser);
        void set_key(const u8 key);

    protected:
        bool is_recognized_impl(io::File &input_file) const override;
        res::Image decode_impl(
            const Logger &logger, io::File &input_file) const override;

    private:
        struct Priv;
        std::unique_ptr<Priv> p;
    };

} } }
