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

    botix::cli::Console console{init.arena, cli_console_config};

    auto maybe_channel = console.addChannel(init.arena, {.echo = true});
    if (maybe_channel.isNone()) {
        init.logger.error("channel fail");
        return;
    }

    auto &channel = maybe_channel.unwrap();

    auto maybe_namespace = console.addGroup(init.arena, {.name = "transport"});

    auto maybe_command = maybe_namespace.unwrap().addCommand(
        init.arena,
        {
            .name = "kek",
            .description = "do important stuff",
            .shortcut = kf::some('e'),
        },
        [&init](botix::cli::Command::Context const &context) {
            auto const e = context.arguments[0]->enumValue<botix::transport::Kind>();
            auto const flag = context.arguments[1]->boolean();

            context.channel.output.print("cmd:'{}': {} {}", context.channel.input_line, e, flag);
        });

    if (maybe_command.isNone()) {
        init.logger.error("command fail");
        return;
    }

    auto &command = maybe_command.unwrap();

    botix::cli::Argument::EnumItem const transports[]{
        {{.name = "espnow", .description{"use ESPNOW transport"}}, botix::transport::Kind::Espnow},
        {{.name = "wifi", .description{"this option has no shortcut"}, .shortcut = kf::none}, botix::transport::Kind::Wifi},
    };

    (void) command.addEnumArgument(init.arena, {.name = "transport", .description{"select transport"}}, {.items{transports}});

    (void) command.addBooleanArgument(init.arena, {.name = "my_flag"}, {.params{.default_value = kf::some(true)}});

    init.logger.debug("arena available: {}", init.arena.available());

    while (true) {
        auto const now = rtos::Clock::now();

        if (channel.output.availableForRead() > 0) {
            init.logger.debug("output: {}", channel.output.drain());
        }

        while (init.io.availableForRead() > 0) {
            if (auto read = init.io.readByte(); read.isOk()) {
                (void) channel.input.feed({reinterpret_cast<char const *>(&read.ok()), 1});
            }
        }

        console.poll(now);

        rtos::Task::sleep(1);
    }
}
