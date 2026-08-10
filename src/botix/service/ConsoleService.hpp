// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/Arena.hpp>
#include <kf/Function.hpp>
#include <kf/Logger.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/Queue.hpp>
#include <kf/Slice.hpp>
#include <kf/String.hpp>
#include <kf/StringView.hpp>
#include <kf/concepts.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/Labeled.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/service/Service.hpp"

namespace botix::internal {

struct ConsoleServiceConfig : kf::mixin::Resettable<ConsoleServiceConfig> {

    kf::u8
        max_channels,
        max_commands,
        command_max_arguments;

    kf::u16
        channel_input_queue_length,
        channel_input_line_length,
        channel_output_line_length;

private:
    KF_IMPL_RESETTABLE(ConsoleServiceConfig);
    constexpr void resetImpl() noexcept {
        max_channels = 0x08;
        channel_input_queue_length = 0x02'00;
        channel_input_line_length = 0x00'80;
        channel_output_line_length = 0x00'80;
    }
};

// bypass trivial type concept constraint
template<typename T> [[nodiscard]] constexpr auto allocateNonTrivial(kf::Arena &arena, kf::usize count) noexcept -> kf::Slice<T> {
    auto bytes = arena.allocate(sizeof(T) * count, alignof(T));
    return {
        reinterpret_cast<T *>(bytes.data()),
        bytes.length() / sizeof(T),
    };
}

}// namespace botix::internal

namespace botix::service {

struct ConsoleService final :

    Service<ConsoleService>,
    kf::mixin::Configured<internal::ConsoleServiceConfig>

{
    using Self = ConsoleService;

    using Config = internal::ConsoleServiceConfig;

    struct Channel {

        struct Output : kf::mixin::NonCopyable {

            explicit constexpr Output(kf::Slice<char> buffer) noexcept :
                _line{buffer} {}

            template<typename... Args> void error(kf::internal::FormatString<Args...> fmt, Args const &...args) noexcept {
                _line.append("error: ");
                print(fmt, args...);
            }

            template<typename... Args> void print(kf::internal::FormatString<Args...> fmt, Args const &...args) noexcept {
                _line.format(fmt, args...);
                (void) _line.write('\n');
            }

            kf::String _line;
        };

        void feed(kf::StringView input) noexcept {
            for (char c: input) {
                if (not input_queue.write(c)) {
                    break;
                }
            }
        }

        kf::Queue<char> input_queue;
        kf::String input_line;
        Output output;
        kf::u8 id;
        bool echo;
    };

    struct Command {

        struct Argument {

            enum class Kind : kf::u8 {
                Enum,
                Boolean,
                Integer,
                Real,
                String,
            };

            struct ParseContext {
                Channel::Output &channel_output;
                kf::StringView lexeme;// not empty

                template<typename T> [[nodiscard]] constexpr auto parseEnumerated(kf::Slice<T const> items, auto item_name_provider) const noexcept -> kf::Option<T const &> {
                    for (auto const &item: items) {
                        if (item_name_provider(item) == lexeme) {
                            return kf::someRef(item);
                        }
                    }

                    channel_output.error("'{}' not allowed, use:", lexeme);
                    for (auto const &item: items) {
                        channel_output.error("\t'{}'", item_name_provider(item));
                    }

                    return kf::none;
                }
            };

            template<kf::trivial T> struct Parameters : kf::mixin::Resettable<Parameters<T>> {
                T value{};
                kf::Option<T> default_value;

            private:
                KF_IMPL_RESETTABLE(Parameters<T>);
                constexpr void resetImpl() noexcept {
                    if (default_value.isSome()) {
                        value = default_value.unwrap();
                    }
                }
            };

            // TODO: make as mixin Parsable<Impl, Args const &...>
            template<typename Impl> struct Parsable {
                [[nodiscard]] constexpr bool parse(ParseContext const &context) noexcept {
                    return static_cast<Impl *>(this)->parseImpl(context);
                }
            };

            template<typename Impl, kf::trivial ParametersType> struct Value : ParametersType, Parsable<Impl> {
                constexpr Value(ParametersType params) noexcept :
                    ParametersType{params} {}
            };

            template<kf::trivial T> struct ValueItem : kf::mixin::Labeled {
                using ValueType = T;

                constexpr ValueItem(kf::StringView label, ValueType value) noexcept :
                    kf::mixin::Labeled{label}, _value{value} {}

                [[nodiscard]] constexpr auto value() const noexcept {
                    return _value;
                }

            private:
                ValueType _value;
            };

            struct EnumItem : ValueItem<kf::usize> {

                constexpr EnumItem(kf::StringView label, kf::enum_type auto value) noexcept :
                    ValueItem<kf::usize>{label, static_cast<kf::usize>(value)} {
                    static_assert(sizeof(value) <= sizeof(kf::usize));
                }
            };

            struct EnumParameters {

                Parameters<EnumItem::ValueType> params;
                kf::Slice<EnumItem const> items;
            };

            struct Enum : Value<Enum, EnumParameters> {

                using Value<Enum, EnumParameters>::Value;

            private:
                friend struct Parsable<Enum>;
                constexpr bool parseImpl(ParseContext const &context) noexcept {
                    auto const name_provider = [](auto const &item) noexcept { return item.label(); };

                    if (auto maybe_item = context.parseEnumerated(items, name_provider); maybe_item.isSome()) {
                        this->params.value = maybe_item.unwrap().value();
                        return true;
                    }
                    return false;
                }
            };

            using BooleanItem = ValueItem<bool>;

            struct BooleanParameters {
                Parameters<BooleanItem::ValueType> params;
            };

            // TODO: make special case of Enum
            struct Boolean : Value<Boolean, BooleanParameters> {

                using Value<Boolean, BooleanParameters>::Value;

            private:
                friend struct Parsable<Boolean>;
                constexpr bool parseImpl(ParseContext const &context) noexcept {

                    BooleanItem const items[8]{
                        {"true", true},
                        {"false", false},

                        {"t", true},
                        {"f", false},

                        {"yes", true},
                        {"no", false},

                        {"y", true},
                        {"n", false},
                    };

                    auto const name_provider = [](auto const &item) noexcept { return item.label(); };

                    if (auto maybe_item = context.parseEnumerated<BooleanItem>(items, name_provider); maybe_item.isSome()) {
                        this->params.value = maybe_item.unwrap().value();
                        return true;
                    }

                    return false;
                }
            };

            template<kf::arithmetic T> struct NumberParameters {
                Parameters<T> params;
                kf::Option<T> min_value, max_value;
            };

            using IntegerParameters = NumberParameters<kf::i32>;

            struct Integer : Value<Integer, IntegerParameters> {

                using Value<Integer, IntegerParameters>::Value;

            private:
                friend struct Parsable<Integer>;
                constexpr bool parseImpl(ParseContext const &context) noexcept {
                    return false;// TODO: impl
                }
            };

            using RealParameters = NumberParameters<kf::f32>;

            struct Real : Value<Real, RealParameters> {

                using Value<Real, RealParameters>::Value;

            private:
                friend struct Parsable<Real>;
                constexpr bool parseImpl(ParseContext const &context) noexcept {
                    return false;// TODO: impl
                }
            };

            struct StringParameters {
                Parameters<kf::StringView> params;
                kf::Slice<kf::StringView> options;// constraint disabled if empty
            };

            struct String : Value<String, StringParameters> {

                using Value<String, StringParameters>::Value;

            private:
                friend struct Parsable<String>;
                constexpr bool parseImpl(ParseContext const &context) noexcept {
                    if (not this->options.empty()) {
                        auto const name_provider = [](auto s) { return s; };

                        if (auto maybe_item = context.parseEnumerated<kf::StringView>(options, name_provider); maybe_item.isSome()) {
                            this->params.value = maybe_item.unwrap();
                            return true;
                        }

                        return false;
                    }

                    this->params.value = context.lexeme;
                    return true;
                }
            };

            // construct

            explicit constexpr Argument(kf::StringView name, EnumParameters params) noexcept :
                _enum{params}, _name{name}, _kind{Kind::Enum} {}

            explicit constexpr Argument(kf::StringView name, BooleanParameters params) noexcept :
                _boolean{params}, _name{name}, _kind{Kind::Boolean} {}

            explicit constexpr Argument(kf::StringView name, IntegerParameters params) noexcept :
                _integer{params}, _name{name}, _kind{Kind::Integer} {}

            explicit constexpr Argument(kf::StringView name, RealParameters params) noexcept :
                _real{params}, _name{name}, _kind{Kind::Real} {}

            explicit constexpr Argument(kf::StringView name, StringParameters params) noexcept :
                _string{params}, _name{name}, _kind{Kind::String} {}

            // get

            [[nodiscard]] constexpr auto enumIndex() const noexcept {
                return _enum.params.value;
            }

            [[nodiscard]] constexpr auto enumName() const noexcept {
                return _enum.items[enumIndex()].label();
            }

            [[nodiscard]] constexpr auto boolean() const noexcept {
                return _boolean.params.value;
            }

            [[nodiscard]] constexpr auto integer() const noexcept {
                return _integer.params.value;
            }

            [[nodiscard]] constexpr auto real() const noexcept {
                return _real.params.value;
            }

            [[nodiscard]] constexpr auto string() const noexcept {
                return _string.params.value;
            }

            // properties

            [[nodiscard]] constexpr auto name() const noexcept {
                return _name;
            }

            [[nodiscard]] constexpr auto kind() const noexcept {
                return _kind;
            }

            [[nodiscard]] constexpr bool hasDefault() const noexcept {
                switch (_kind) {
                    case Kind::Enum: return _enum.params.default_value.isSome();
                    case Kind::Boolean: return _boolean.params.default_value.isSome();
                    case Kind::Integer: return _integer.params.default_value.isSome();
                    case Kind::Real: return _real.params.default_value.isSome();
                    case Kind::String: return _string.params.default_value.isSome();
                    default: return false;
                }
            }

            [[nodiscard]] constexpr bool parse(ParseContext const &context) noexcept {
                switch (_kind) {
                    case Kind::Enum: return _enum.parse(context);
                    case Kind::Boolean: return _boolean.parse(context);
                    case Kind::Integer: return _integer.parse(context);
                    case Kind::Real: return _real.parse(context);
                    case Kind::String: return _string.parse(context);
                    default: return false;
                };
            }

            void reset() noexcept {
                switch (_kind) {
                    case Kind::Enum: return _enum.params.reset();
                    case Kind::Boolean: _boolean.params.reset(); break;
                    case Kind::Integer: _integer.params.reset(); break;
                    case Kind::Real: _real.params.reset(); break;
                    case Kind::String: _string.params.reset(); break;
                };
            }

        private:
            union {
                Enum _enum;
                Boolean _boolean;
                Integer _integer;
                Real _real;
                String _string;
            };

            kf::StringView _name;
            Kind _kind;
        };

        struct Context {
            kf::Slice<Argument const> arguments;
            Channel::Output &output;
            kf::units::Milliseconds timestamp;
            kf::u8 channel_id;
        };

        explicit constexpr Command(kf::Slice<Argument> arguments_buffer, kf::StringView name, auto &&handler) noexcept :
            _arguments{arguments_buffer},
            _name{name},
            _handler{std::forward<decltype(handler)>(handler)} {}

        // properties

        [[nodiscard]] constexpr kf::StringView name() const noexcept {
            return _name;
        }

        [[nodiscard]] constexpr auto arguments() noexcept -> kf::Slice<Argument> {
            return _arguments.slice();
        }

        [[nodiscard]] constexpr auto arguments() const noexcept -> kf::Slice<Argument const> {
            return _arguments.slice();
        }

        [[nodiscard]] constexpr kf::usize positionalArgumentsCount() const noexcept {
            kf::usize count = _arguments.length();
            for (auto const &a: _arguments) {
                count -= static_cast<kf::usize>(a.hasDefault());
            }
            return count;
        }

        // argument
        // TODO: check if default out of constraint
        // TODO: check if non-default after default
        // TODO: check for name arg collision

        [[nodiscard]] constexpr bool addEnumArgument(kf::StringView name, Argument::EnumParameters params) noexcept {
            return _arguments.write(Argument{name, params});
        }

        [[nodiscard]] constexpr bool addBooleanArgument(kf::StringView name, Argument::BooleanParameters params) noexcept {
            return _arguments.write(Argument{name, params});
        }

        [[nodiscard]] constexpr bool addIntegerArgument(kf::StringView name, Argument::IntegerParameters params) noexcept {
            return _arguments.write(Argument{name, params});
        }

        [[nodiscard]] constexpr bool addRealArgument(kf::StringView name, Argument::RealParameters params) noexcept {
            return _arguments.write(Argument{name, params});
        }

        [[nodiscard]] constexpr bool addStringArgument(kf::StringView name, Argument::StringParameters params) noexcept {
            return _arguments.write(Argument{name, params});
        }

        // control

        void execute(Context const &context) const noexcept {
            _handler(context);
        }

    private:
        kf::StringView _name;
        kf::Stack<Argument> _arguments;
        kf::Function<void(Context const &)> _handler;
    };

    // TODO: impl Initable
    // TODO: help command

    explicit constexpr ConsoleService(Config const &config, kf::Arena &arena) noexcept :
        kf::mixin::Configured<Config>{config},
        _channels{arena.allocate<Channel>(config.max_channels)},
        _commands{internal::allocateNonTrivial<Command>(arena, config.max_commands)} {}

    [[nodiscard]] auto addChannel(kf::Arena &arena) noexcept -> kf::Option<Channel &> {
        auto const channel_alocation_length{
            this->config().channel_input_queue_length +
            this->config().channel_input_line_length +
            this->config().channel_output_line_length};

        if (_channels.full() or arena.available() < channel_alocation_length) { return kf::none; }

        auto input_queue_buffer = arena.allocate<char>(this->config().channel_input_queue_length);
        auto input_line_buffer = arena.allocate<char>(this->config().channel_input_line_length);
        auto output_line_buffer = arena.allocate<char>(this->config().channel_output_line_length);

        (void) _channels.write(Channel{
            .input_queue{input_queue_buffer},
            .input_line{input_line_buffer},
            .output{{output_line_buffer}},
            .id = static_cast<kf::u8>(_channels.length()),
            .echo = false,
        });

        return _channels.top();
    }

    // TODO: return Result<Command &, Error> (needs kf ok-reference)
    [[nodiscard]] auto addCommand(kf::Arena &arena, kf::StringView name, auto &&handler) noexcept -> kf::Option<Command &> {
        if (_commands.full() or getCommand(name).isSome()) { return kf::none; }

        auto arguments_buffer = arena.allocate<Command::Argument>(this->config().command_max_arguments);
        if (arguments_buffer.empty()) { return kf::none; }

        (void) _commands.write(Command{
            arguments_buffer,
            name,
            std::forward<decltype(handler)>(handler),
        });

        return _commands.top();
    }

private:
    kf::Logger _logger{"ConsoleService"};

    kf::Stack<Channel> _channels;
    kf::Stack<Command> _commands;

    void onInputLine(kf::units::Milliseconds timestamp, Channel &channel) noexcept {
        if (channel.echo) {
            channel.output.print("[#{}] >>> {}", channel.id, channel.input_line.view());
        }

        kf::StringView tokens_buffer[this->config().command_max_arguments]{};

        auto tokens = channel.input_line.view().trim().split({tokens_buffer, this->config().command_max_arguments});

        if (tokens.empty()) {
            return;
        }

        auto command_name = tokens[0];
        auto maybe_command = getCommand(command_name);

        if (maybe_command.isNone()) {
            channel.output.error("unknown command: {}", command_name);
            return;
        }

        auto &command = maybe_command.unwrap();
        auto argument_tokens = tokens.fromOffset(1);

        if (argument_tokens.length() < command.positionalArgumentsCount() or argument_tokens.length() > command.arguments().length()) {
            channel.output.error("expected {}..{} arguments, got {}", command.positionalArgumentsCount(), command.arguments().length(), argument_tokens.length());
            return;
        }

        auto argument_index = 0;
        bool parse_failed = false;
        while (argument_index < argument_tokens.length()) {
            auto lexeme = argument_tokens[argument_index];
            auto &argument = command.arguments()[argument_index];
            _logger.debug("{}: {}", argument.name(), argument.hasDefault());

            if (not argument.parse({.channel_output = channel.output, .lexeme = lexeme})) {
                parse_failed = true;
                channel.output.print("note: failed argument 'name': {}", argument.name());
            }

            argument_index += 1;
        }

        if (parse_failed) {
            channel.output.error("failed to parse positional argument(s)");
            return;
        }

        auto default_value_arguments = command.arguments().fromOffset(argument_index);
        for (auto &a: default_value_arguments) {
            a.reset();
        }

        command.execute({
            .arguments = command.arguments(),
            .output = channel.output,
            .timestamp = timestamp,
            .channel_id = channel.id,
        });
    }

    [[nodiscard]] auto getCommand(kf::StringView name) noexcept -> kf::Option<Command &> {
        for (auto &c: _commands) {
            if (c.name() == name) {
                return kf::someRef(c);
            }
        }
        return kf::none;
    }

    BOTIX_IMPL_SERVICE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        for (auto &channel: _channels) {
            while (channel.input_queue.availableForRead() > 0) {
                char const c = channel.input_queue.read().unwrap();

                switch (c) {
                    case '\b':
                    case '\x7F':
                        if (channel.input_line.read().isNone()) {
                            _logger.error("input buffer is empty");
                        }
                        break;

                    case '\t':
                        _logger.debug("tab");
                        break;

                    case '\n':
                        onInputLine(now, channel);
                        channel.input_line.reset();
                        break;

                    case 0x20 ... 0x7E:
                        if (not channel.input_line.write(c)) {
                            _logger.error("input buffer is full");
                        }
                        break;

                    default:
                        _logger.warn("unknown char '{}' ({})", c, static_cast<int>(c));
                        break;
                }
            }
        }
    }
};

}// namespace botix::service