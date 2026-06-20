#include <hgl/shadergen/registry/ErrorCodeRegistry.h>

#include <cstdio>

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))
#define CHECK_NE(a, b)  CHECK_TRUE((a) != (b))

using namespace hgl::graph::mtl;

static void test_whitelist_accepts_known_keys()
{
    CHECK_TRUE(IsKnownSFMAnnotationKey("surface_type"));
    CHECK_TRUE(IsKnownSFMAnnotationKey("supports_phase"));
    CHECK_TRUE(IsKnownSFMAnnotationKey("require"));
    CHECK_TRUE(IsKnownSFMAnnotationKey("optional"));
    CHECK_TRUE(IsKnownSFMAnnotationKey("derive"));

    CHECK_TRUE(IsKnownSFMAnnotationKey("@sfm:require"));
    CHECK_TRUE(IsKnownSFMAnnotationKey("@sfm:supports_phase"));
}

static void test_whitelist_rejects_unknown_keys()
{
    CHECK_TRUE(!IsKnownSFMAnnotationKey("require_ubo"));
    CHECK_TRUE(!IsKnownSFMAnnotationKey("phase"));
    CHECK_TRUE(!IsKnownSFMAnnotationKey("@sfm:foo"));

    CHECK_EQ(GetSFMAnnotationKeyIndex("@sfm:foo"), static_cast<uint8_t>(0xFFu));
}

static void test_key_index_roundtrip()
{
    const uint8_t idx = GetSFMAnnotationKeyIndex("@sfm:derive");
    CHECK_NE(idx, static_cast<uint8_t>(0xFFu));
    CHECK_TRUE(GetSFMAnnotationKeyName(idx) == "derive");
}

static void test_error_code_encode_decode_and_format()
{
    const uint8_t key_index = GetSFMAnnotationKeyIndex("require");
    const uint32_t code = EncodeSFMAnnotationError(SFMAnnotationError::UnknownKey,
                                                   key_index,
                                                   37,
                                                   0);
    const auto decoded = DecodeSFMAnnotationError(code);

    CHECK_EQ(decoded.error, SFMAnnotationError::UnknownKey);
    CHECK_EQ(decoded.key_index, key_index);
    CHECK_EQ(decoded.line_mod_256, static_cast<uint8_t>(37));

    const std::string text = FormatSFMAnnotationError(code);
    CHECK_TRUE(text.find("UnknownKey") != std::string::npos);
    CHECK_TRUE(text.find("require") != std::string::npos);
}

int main()
{
    test_whitelist_accepts_known_keys();
    test_whitelist_rejects_unknown_keys();
    test_key_index_roundtrip();
    test_error_code_encode_decode_and_format();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All SFMAnnotationPolicy tests passed.\n");
    return 0;
}
