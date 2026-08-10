#include <kf/main.hpp>

#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/service/ConsoleService.hpp"
#include "botix/transport/Kind.hpp"

void kf::main(kf::Init &init) {
    init.logger.debug("hello test");

    botix::service::ConsoleService::Config const console_service_config{
        .max_channels = 4,
        .max_commands = 10,
        .command_max_arguments = 8,
        .channel_input_queue_length = 100,
        .channel_input_line_length = 100,
        .channel_output_line_length = 512,
    };

    botix::service::ConsoleService console_service{
        console_service_config,
        init.arena,
    };

    auto maybe_channel = console_service.addChannel(init.arena);
    if (maybe_channel.isNone()) {
        init.logger.error("channel fail");
        return;
    }

    auto &channel = maybe_channel.unwrap();
    channel.echo = true;

    channel.output.sink([&init](kf::StringView line) {
        (void) init.io.writeBuffer({
            reinterpret_cast<kf::u8 const *>(line.data()),
            line.length(),
        });
        init.io.flush();
    });

    auto maybe_command = console_service.addCommand(init.arena, "cmd", [&init](botix::service::ConsoleService::Command::Context const &context) {
        auto const e = context.arguments[0].enumIndex();
        auto const str = context.arguments[1].string();
        auto const flag = context.arguments[2].boolean();

        context.output.print("e: {}, mode: '{}', flag={}", e, str, flag);
    });

    if (maybe_command.isNone()) {
        init.logger.error("command fail");
        return;
    }

    auto &command = maybe_command.unwrap();

    botix::service::ConsoleService::Command::Argument::EnumItem const transports[]{
        {"espnow", botix::transport::Kind::Espnow},
        {"wifi", botix::transport::Kind::Wifi},
    };

    kf::StringView my_string_options[]{
        "wifi",
        "espnow",
        "auto",
    };

    (void) command.addEnumArgument("transport", {.items{transports}});

    (void) command.addStringArgument(
        "my_string",
        {
            .params{.default_value = kf::some(kf::StringView{"auto"})},
            .options{my_string_options},
        });

    (void) command.addBooleanArgument("my_flag", {.params{.default_value = kf::some(true)}});

    // (void) command.addIntegerArgument("my_int", {{.min_value = kf::none}});

    auto read_buffer = init.arena.allocate(100);

    while (true) {
        auto const now = rtos::Clock::now();

        if (init.io.availableForRead() > 0) {
            auto read = init.io.readBuffer(read_buffer);

            if (read.isOk()) {
                channel.feed({
                    reinterpret_cast<char const *>(read.ok().data()),
                    read.ok().length(),
                });
            } else {
                init.logger.error("read error");
            }
        }

        console_service.poll(now);

        rtos::Task::sleep(1);
    }
}
