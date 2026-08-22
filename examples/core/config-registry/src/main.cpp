#include <kf/core.hpp>
#include <kf/main.hpp>

#include "botix/config/Registry.hpp"

struct SomeConfig {

    enum class Mode : kf::u8 {
        Tank,
        Direct,
    };

    kf::u8 a8{64};
    kf::i8 b8{-64};

    kf::u16 a16{};
    kf::i16 b16{};

    kf::u32 a32{};
    kf::i32 b32{};

    kf::u64 a64{};
    kf::i64 b64{};

    char hostname[16]{"botix"};

    Mode mode{Mode::Tank};
    bool wifi_enabled{false};
    char magic_char{'c'};
};

void kf::main(kf::Init &init) {
    init.logger.debug("config registry example");

    SomeConfig config{};

    botix::config::Registry::ValueField value_fields[]{
        {"a8", config.a8},
        {"a16", config.a16},
        {"a32", config.a32},
        {"a64", config.a64},
        {"b8", config.b8},
        {"b16", config.b16},
        {"b32", config.b32},
        {"b64", config.b64},
        {"hostname", config.hostname},
        {"wifi_enabled", config.wifi_enabled},
        {"magic", config.magic_char},
    };

    botix::config::Registry::EnumField::Entry const modes[]{
        {"tank", SomeConfig::Mode::Tank},
        {"direct", SomeConfig::Mode::Direct},
    };

    botix::config::Registry::EnumField enum_fields[]{
        {"mode", config.mode, modes},
    };

    botix::config::Registry config_registry{
        .value_fields{value_fields},
        .enum_fields{enum_fields},
    };

    auto const show_registry = [&init, &config_registry]() -> void {
        for (auto const &field: config_registry.value_fields) {
            init.logger.debug("{}", field);
        }

        for (auto const &field: config_registry.enum_fields) {
            init.logger.debug("{}", field);
        }
    };

    show_registry();

    (void) config_registry.set("a8", "0xFF");
    (void) config_registry.set("hostname", "klyax");
    (void) config_registry.set("mode", "bark");

    show_registry();
}
