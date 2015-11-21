#include "test_support/catch.hh"
#include "util/program_path.h"

using namespace au::util;

TEST_CASE("Getting program path", "[util]")
{
    REQUIRE(get_program_path().str().find("run_tests") != std::string::npos);
}

TEST_CASE("Getting 'extra/' directory path", "[util]")
{
    REQUIRE(get_extra_dir_path().str().find("extra") != std::string::npos);
}
