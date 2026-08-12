#include <kf/main.hpp>

#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/service/ConsoleService.hpp"
#include "botix/transport/Kind.hpp"

void kf::main(kf::Init &init) {
    init.logger.debug("hello test");

    botix::service::ConsoleService::Config console_service_config{};
    console_service_config.reset();

    auto maybe_console_service = botix::service::ConsoleService::create(init.arena, console_service_config);

    auto &console_service = maybe_console_service.unwrap();

    auto maybe_channel = console_service.addChannel(init.arena, true);
    if (maybe_channel.isNone()) {
        init.logger.error("channel fail");
        return;
    }

    auto &channel = maybe_channel.unwrap();
    channel.echo = true;

    auto &global_namespace = console_service.globalNamespace();

    auto maybe_command = global_namespace.addCommand(init.arena, "cmd", [&init](botix::service::ConsoleService::Command::Context const &context) {
        auto const e = context.arguments[0].enumValue<botix::transport::Kind>();
        auto const flag = context.arguments[1].boolean();

        context.output.print("enum: {}, flag={}", static_cast<int>(e), flag);
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

    (void) command.addEnumArgument("transport", {.items{transports}});

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

        if (not channel.output._line.empty()) {
            init.logger.debug("output: {}", channel.output._line);
            channel.output._line.reset();
        }

        rtos::Task::sleep(1);
    }
}
