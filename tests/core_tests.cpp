// SPDX-License-Identifier: GPL-3.0-only
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "finaleffect/core/Error.hpp"
#include "finaleffect/core/Json.hpp"
#include "finaleffect/core/Log.hpp"
#include "finaleffect/core/Version.hpp"

namespace {
using fe::core::JsonValue;
using fe::core::LogLevel;
using fe::core::MemoryLogSink;
using fe::core::Logger;
}  // namespace

FE3D_TEST(core, version_string_is_semantic) {
    const std::string version(fe::core::versionString());
    FE3D_CHECK(!version.empty());
    FE3D_CHECK_EQ(version.find('.'), std::size_t(1));
    FE3D_CHECK(fe::core::buildInfo().find("Final Effect 3D") != std::string_view::npos);
}

FE3D_TEST(core, result_success_and_failure) {
    const fe::core::Result<int> good = fe::core::Result<int>::success(41);
    FE3D_CHECK(good.ok());
    FE3D_CHECK_EQ(good.value(), 41);
    FE3D_CHECK_EQ(good.valueOr(7), 41);

    const fe::core::Result<int> bad = fe::core::Result<int>::failure(
        fe::core::makeError(fe::core::ErrorCode::NotFound, "missing", "test"));
    FE3D_CHECK(!bad.ok());
    FE3D_CHECK(bad.error().code() == fe::core::ErrorCode::NotFound);
    FE3D_CHECK_EQ(bad.valueOr(7), 7);
    FE3D_CHECK_EQ(bad.error().toString(), std::string("NotFound: missing (test)"));

    const fe::core::Status ok = fe::core::successStatus();
    FE3D_CHECK(ok.ok());
    FE3D_CHECK(fe::core::isFailure(fe::core::ErrorCode::IoFailure));
    FE3D_CHECK(!fe::core::isFailure(fe::core::ErrorCode::Ok));
}

FE3D_TEST(core, json_round_trip_preserves_values) {
    JsonValue root = JsonValue::object();
    root.set("name", JsonValue(std::string("scene")));
    root.set("version", JsonValue(3.0));
    root.set("visible", JsonValue(true));
    root.set("nothing", JsonValue());

    JsonValue array = JsonValue::array();
    array.append(JsonValue(1.0));
    array.append(JsonValue(2.5));
    root.set("values", std::move(array));

    const std::string text = root.dump(2);
    const fe::core::JsonParseResult parsed = JsonValue::parse(text);
    FE3D_REQUIRE(parsed.ok);
    FE3D_CHECK(parsed.value.isObject());
    FE3D_CHECK_EQ(parsed.value.stringOr("name"), std::string("scene"));
    FE3D_CHECK_NEAR(parsed.value.numberOr("version"), 3.0, 0.0);
    FE3D_CHECK(parsed.value.booleanOr("visible", false));
    FE3D_CHECK(parsed.value.find("nothing")->isNull());
    FE3D_CHECK_EQ(parsed.value.find("values")->items().size(), std::size_t(2));
    FE3D_CHECK_NEAR(parsed.value.find("values")->items()[1].asNumber(), 2.5, 0.0);

    // Member order is preserved, so files are byte-for-byte stable.
    FE3D_CHECK_EQ(text.find("\"name\""), std::size_t(4));  // "{\n" + two spaces of indentation
    const std::string compact = root.dump(0);
    FE3D_CHECK(compact.find('\n') == std::string::npos);
}

FE3D_TEST(core, json_handles_escapes_and_unicode) {
    const std::string source = R"({"text":"line\nquote\"tab\t\\slash \u00e9\u0041"})";
    const fe::core::JsonParseResult parsed = JsonValue::parse(source);
    FE3D_REQUIRE(parsed.ok);
    const std::string text = parsed.value.stringOr("text");
    FE3D_CHECK_EQ(text, std::string("line\nquote\"tab\t\\slash \xc3\xa9" "A"));

    // Re-serializing and re-parsing must be lossless.
    const fe::core::JsonParseResult second = JsonValue::parse(parsed.value.dump(0));
    FE3D_REQUIRE(second.ok);
    FE3D_CHECK_EQ(second.value.stringOr("text"), text);
}

FE3D_TEST(core, json_rejects_malformed_input) {
    const std::vector<std::string> malformed = {
        "",
        "{",
        R"({"a": })",
        R"({"a" 1})",
        R"({"a": 01})",
        R"({"a": 1} trailing)",
        R"([1,2,])",
        R"({"a": 'single'})",
        R"({"a": "unterminated})",
    };
    for (const std::string& text : malformed) {
        const fe::core::JsonParseResult parsed = JsonValue::parse(text);
        FE3D_CHECK_MSG(!parsed.ok, "should reject: " + text);
        FE3D_CHECK_MSG(!parsed.error.empty(), "error message missing for: " + text);
        FE3D_CHECK_MSG(parsed.line >= 1 && parsed.column >= 1, "position missing for: " + text);
    }
}

FE3D_TEST(core, json_enforces_depth_limit) {
    std::string deep;
    for (int i = 0; i < 200; ++i) {
        deep.push_back('[');
    }
    for (int i = 0; i < 200; ++i) {
        deep.push_back(']');
    }
    const fe::core::JsonParseResult parsed = JsonValue::parse(deep);
    FE3D_CHECK(!parsed.ok);
    FE3D_CHECK(parsed.error.find("depth") != std::string::npos);
}

FE3D_TEST(core, json_writes_non_finite_numbers_as_null) {
    JsonValue root = JsonValue::object();
    root.set("nan", JsonValue(std::nan("")));
    root.set("inf", JsonValue(std::numeric_limits<double>::infinity()));
    const std::string text = root.dump(0);
    FE3D_CHECK_EQ(text, std::string(R"({"nan":null,"inf":null})"));
}

FE3D_TEST(core, logger_filters_by_level_and_captures_records) {
    Logger logger(LogLevel::Info, "test");
    auto sink = std::make_shared<MemoryLogSink>();
    logger.addSink(sink);

    logger.log(LogLevel::Debug, "cat", "hidden");
    logger.log(LogLevel::Info, "cat", "visible");
    logger.log(LogLevel::Error, "cat", "problem");

    FE3D_CHECK_EQ(sink->size(), std::size_t(2));
    FE3D_CHECK(sink->contains(LogLevel::Info, "visible"));
    FE3D_CHECK(sink->contains(LogLevel::Error, "problem"));
    FE3D_CHECK(!sink->contains(LogLevel::Debug, "hidden"));
    FE3D_CHECK_EQ(sink->at(1).category, std::string("cat"));
    FE3D_CHECK(sink->at(0).timeSeconds >= 0.0);

    logger.setMinimumLevel(LogLevel::Off);
    logger.log(LogLevel::Critical, "cat", "also hidden");
    FE3D_CHECK_EQ(sink->size(), std::size_t(2));

    FE3D_CHECK(logger.shouldLog(LogLevel::Error) == false);
    logger.setMinimumLevel(LogLevel::Warning);
    FE3D_CHECK(logger.shouldLog(LogLevel::Error));
    FE3D_CHECK(!logger.shouldLog(LogLevel::Info));
}

FE3D_TEST(core, log_stream_writes_a_single_record) {
    Logger logger(LogLevel::Trace, "test");
    auto sink = std::make_shared<MemoryLogSink>();
    logger.addSink(sink);

    fe::core::LogStream(logger, LogLevel::Warning, "scene") << "objects=" << 4 << " ready=" << true;
    FE3D_REQUIRE(sink->size() == 1);
    FE3D_CHECK_EQ(sink->at(0).message, std::string("objects=4 ready=1"));
    FE3D_CHECK_EQ(std::string(fe::core::toString(LogLevel::Warning)), std::string("WARN"));

    LogLevel parsed = LogLevel::Off;
    FE3D_CHECK(fe::core::tryParseLogLevel("dEbUg", parsed));
    FE3D_CHECK(parsed == LogLevel::Debug);
    FE3D_CHECK(!fe::core::tryParseLogLevel("loud", parsed));
    FE3D_CHECK(parsed == LogLevel::Debug);
}
