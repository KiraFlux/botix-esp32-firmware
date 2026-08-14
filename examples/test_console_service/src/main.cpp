#include <kf/main.hpp>

#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/cli/Config.hpp"
#include "botix/cli/Console.hpp"
#include "botix/transport/Kind.hpp"

void kf::main(kf::Init &init) {
    init.logger.debug("hello test");

    botix::cli::Config cli_console_config{
        .max_channel_count = 10,
    };

    botix::cli::Console console_service{init.arena, cli_console_config};

    auto maybe_channel = console_service.addChannel(init.arena, {.echo = true});
    if (maybe_channel.isNone()) {
        init.logger.error("channel fail");
        return;
    }

    auto &channel = maybe_channel.unwrap();

    auto &global_namespace = console_service.globalNamespace();

    auto maybe_command = global_namespace.addCommand(
        init.arena,
        {.name = "kek", .shortcut = kf::some('e')},
        [&init](botix::cli::Command::Context const &context) {
            auto const e = context.arguments[0].enumValue<botix::transport::Kind>();
            auto const flag = context.arguments[1].boolean();

            context.channel.output.print("cmd:'{}' enum: {}, flag={}", context.channel.input_line, static_cast<int>(e), flag);
        });

    if (maybe_command.isNone()) {
        init.logger.error("command fail");
        return;
    }

    auto &command = maybe_command.unwrap();

    botix::cli::Argument::EnumItem const transports[]{
        {{.name = "espnow"}, botix::transport::Kind::Espnow},
        {{.name = "wifi", .shortcut = kf::none}, botix::transport::Kind::Wifi},
    };

    (void) command.addEnumArgument({.name = "transport"}, {.items{transports}});

    (void) command.addBooleanArgument({.name = "my_flag"}, {.params{.default_value = kf::some(true)}});

    // (void) command.addIntegerArgument("my_int", {{.min_value = kf::none}});

    while (true) {
        auto const now = rtos::Clock::now();

        if (channel.output.availableForRead() > 0) {
            init.logger.debug("output: {}", channel.output.drain());
        }

        if (init.io.availableForRead() > 0) {
            auto read = init.io.readByte();

            if (read.isOk()) {
                (void) channel.input.feed({reinterpret_cast<char const *>(&read.ok()), 1});
            } else {
                init.logger.error("read error");
            }
        }

        console_service.poll(now);

        rtos::Task::sleep(1);
    }
}
