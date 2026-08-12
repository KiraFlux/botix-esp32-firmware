// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>// For std::forward, std::move

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
        max_channel_count,
        max_namespace_count,
        max_command_count,
        max_command_argument_count;

    kf::u16
        channel_input_queue_length,
        channel_input_line_length,
        channel_output_line_length;

private:
    KF_IMPL_RESETTABLE(ConsoleServiceConfig);
    constexpr void resetImpl() noexcept {
        max_channel_count = 0x08;
        max_namespace_count = 0x10;
        max_command_count = 0x10;
        max_command_argument_count = 0x08;
        channel_input_queue_length = 0x02'00;
        channel_input_line_length = 0x00'80;
        channel_output_line_length = 0x00'80;
    }
};

struct CreateWithArenaAndConfigTag {};

template<typename Impl> struct CreateWithArenaAndConfig : CreateWithArenaAndConfigTag {

    template<typename... Args> [[nodiscard]] static constexpr auto create(
        kf::Arena &arena, ConsoleServiceConfig const &config, Args &&...args) noexcept -> kf::Option<Impl> {
        if (arena.available() < Impl::allocationLength(config)) { return kf::none; }
        return kf::some(Impl{arena, config, std::forward<Args>(args)...});
    }
};

struct Identifier {
    kf::StringView name;
    kf::Option<char> shortcut{name.empty() ? kf::none : kf::some(name[0])};
};

// like mixin::Labeled, but without setter
struct HasIdentifier {

    explicit constexpr HasIdentifier(Identifier id) noexcept :
        _id{id} {}

    [[nodiscard]] constexpr auto name() const noexcept {
        return _id.name;
    }

    [[nodiscard]] constexpr auto shortcut() const noexcept {
        return _id.shortcut;
    }

    [[nodiscard]] constexpr bool match(kf::StringView name_or_shortcut) const noexcept {
        return (not name_or_shortcut.empty()) and (_id.name == name_or_shortcut or _id.shortcut.unwrapOr(0) == name_or_shortcut[0]);
    }

private:
    Identifier _id;
};

template<kf::implements<CreateWithArenaAndConfigTag> T> struct ItemContainer {

    explicit constexpr ItemContainer(kf::Arena &arena, kf::usize count) noexcept :
        _items{arena.allocate<T>(count)} {}

    constexpr auto items() noexcept {
        return _items.slice();
    }

    constexpr auto items() const noexcept {
        return _items.slice();
    }

    // TODO: return Result<Command &, Error> (needs kf ok-reference)
    template<typename... Args> [[nodiscard]] auto add(kf::Arena &arena, ConsoleServiceConfig const &config, Args &&...args) noexcept -> kf::Option<T &> {
        if (_items.full()) { return kf::none; }

        auto maybe_item = T::create(arena, config, std::forward<Args>(args)...);
        if (maybe_item.isNone()) { return kf::none; }

        (void) _items.write(std::move(maybe_item).unwrap());
        return _items.top();
    }

private:
    kf::Stack<T> _items;
};

template<kf::implements<HasIdentifier> T> struct NamedItemContainer : ItemContainer<T> {
    using ItemContainer<T>::ItemContainer;

    auto get(kf::StringView name_or_shortcut) noexcept -> kf::Option<T &> {
        for (auto &item: this->items()) {
            if (item.match(name_or_shortcut)) {
                return kf::someRef(item);
            }
        }
        return kf::none;
    }
};

template<typename T> struct ValueItem;

template<kf::trivial T> struct ValueItem<T> : HasIdentifier {
    using ValueType = T;

    constexpr ValueItem(Identifier id, ValueType value) noexcept :
        HasIdentifier{id}, _value{value} {}

    [[nodiscard]] constexpr T value() const noexcept {
        return _value;
    }

private:
    ValueType _value;
};

template<> struct ValueItem<kf::StringView> : HasIdentifier {
    using ValueType = kf::StringView;

    constexpr ValueItem(Identifier id) noexcept :
        HasIdentifier{id} {}

    [[nodiscard]] constexpr kf::StringView value() const noexcept {
        return this->name();
    }
};

//

struct ConsoleServiceChannel :

    kf::mixin::NonCopyable,
    CreateWithArenaAndConfig<ConsoleServiceChannel>

{

    struct Output : kf::mixin::NonCopyable {

        explicit constexpr Output(kf::Slice<char> buffer) noexcept :
            _line{buffer} {}

        Output(Output &&other) noexcept : _line(std::move(other._line)) {}

        Output &operator=(Output &&other) noexcept {
            _line = std::move(other._line);
            return *this;
        }

        template<typename... Args> void error(kf::internal::FormatString<Args...> const &fmt, Args const &...args) noexcept {
            _line.append("error: ");
            print(fmt, args...);
        }

        template<typename... Args> void print(kf::internal::FormatString<Args...> const &fmt, Args const &...args) noexcept {
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

    ConsoleServiceChannel(ConsoleServiceChannel &&other) noexcept :
        input_queue(std::move(other.input_queue)),
        input_line(std::move(other.input_line)),
        output(std::move(other.output)),
        echo(other.echo) {}

    ConsoleServiceChannel &operator=(ConsoleServiceChannel &&other) noexcept {
        input_queue = std::move(other.input_queue);
        input_line = std::move(other.input_line);
        output = std::move(other.output);
        echo = other.echo;
        return *this;
    }

    [[nodiscard]] static constexpr auto allocationLength(ConsoleServiceConfig const &config) noexcept {
        return static_cast<kf::usize>(config.channel_input_queue_length + config.channel_input_line_length + config.channel_output_line_length);
    }

    // TODO: Callbacked (sink)

    // TODO: make private
    kf::Queue<char> input_queue;
    kf::String input_line;
    Output output;
    bool echo;

    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleServiceChannel>;
    explicit constexpr ConsoleServiceChannel(kf::Arena &arena, ConsoleServiceConfig const &config, bool echo) noexcept :
        input_queue{arena.allocate<char>(config.channel_input_queue_length)},
        input_line{arena.allocate<char>(config.channel_input_line_length)},
        output{arena.allocate<char>(config.channel_output_line_length)},
        echo{echo} {}
};

struct ConsoleServiceArgument :

    kf::mixin::NonCopyable,
    HasIdentifier

{

    enum class Kind : kf::u8 {
        Enum,
        Boolean,
        Integer,
        Real,
        String,
    };

    struct ParseContext {
        ConsoleServiceChannel::Output &channel_output;
        kf::StringView lexeme;// not empty

        template<kf::implements<HasIdentifier> T> [[nodiscard]] constexpr auto parseEnumerated(kf::Slice<T const> items) const noexcept -> kf::Option<T const &> {
            for (auto const &item: items) {
                if (item.match(lexeme)) {
                    return kf::someRef(item);
                }
            }

            channel_output.error("'{}' not allowed, use:", lexeme);
            for (auto const &item: items) {
                if (item.shortcut().isSome()) {
                    channel_output.error("\t'{}' ('{}')", item.name(), item.shortcut().unwrap()); // TODO: impl ReprTo for HasIdentifier
                } else {
                    channel_output.error("\t'{}'", item.name());
                }
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

    // TODO: make as Parsable<Impl, InputType, OutputType> static interface
    template<typename Impl> struct Parsable {
        [[nodiscard]] constexpr bool parse(ParseContext const &context) noexcept {
            return static_cast<Impl *>(this)->parseImpl(context);
        }
    };

    template<typename Impl, kf::trivial ParametersType> struct Value : ParametersType, Parsable<Impl> {
        constexpr Value(ParametersType params) noexcept :
            ParametersType{params} {}
    };

    struct EnumItem : ValueItem<kf::usize> {

        constexpr EnumItem(Identifier id, kf::enum_type auto value) noexcept :
            ValueItem<kf::usize>{id, static_cast<kf::usize>(value)} {
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
            if (auto maybe_item = context.parseEnumerated(items); maybe_item.isSome()) {
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

    struct Boolean : Value<Boolean, BooleanParameters> {

        using Value<Boolean, BooleanParameters>::Value;

    private:
        friend struct Parsable<Boolean>;
        constexpr bool parseImpl(ParseContext const &context) noexcept {

            BooleanItem const items[4]{
                {{"true"}, true},
                {{"false"}, false},
                {{"yes"}, true},
                {{"no"}, false},
            };

            if (auto maybe_item = context.parseEnumerated<BooleanItem>(items); maybe_item.isSome()) {
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
        kf::Slice<ValueItem<kf::StringView> const> options;// constraint disabled if empty
    };

    struct String : Value<String, StringParameters> {

        using Value<String, StringParameters>::Value;

    private:
        friend struct Parsable<String>;
        constexpr bool parseImpl(ParseContext const &context) noexcept {
            if (not this->options.empty()) {
                auto const name_provider = [](auto s) { return s; };

                if (auto maybe_item = context.parseEnumerated(options); maybe_item.isSome()) {
                    this->params.value = maybe_item.unwrap().name();
                    return true;
                }

                return false;
            }

            this->params.value = context.lexeme;
            return true;
        }
    };

    // construct

    explicit constexpr ConsoleServiceArgument(Identifier id, EnumParameters params) noexcept :
        HasIdentifier{id}, _enum{params}, _kind{Kind::Enum} {}

    explicit constexpr ConsoleServiceArgument(Identifier id, BooleanParameters params) noexcept :
        HasIdentifier{id}, _boolean{params}, _kind{Kind::Boolean} {}

    explicit constexpr ConsoleServiceArgument(Identifier id, IntegerParameters params) noexcept :
        HasIdentifier{id}, _integer{params}, _kind{Kind::Integer} {}

    explicit constexpr ConsoleServiceArgument(Identifier id, RealParameters params) noexcept :
        HasIdentifier{id}, _real{params}, _kind{Kind::Real} {}

    explicit constexpr ConsoleServiceArgument(Identifier id, StringParameters params) noexcept :
        HasIdentifier{id}, _string{params}, _kind{Kind::String} {}

    // get

    [[nodiscard]] constexpr auto enumIndex() const noexcept {
        return _enum.params.value;
    }

    template<kf::enum_type E> [[nodiscard]] constexpr auto enumValue() const noexcept {
        return static_cast<E>(enumIndex());
    }

    [[nodiscard]] constexpr auto enumName() const noexcept {
        return _enum.items[enumIndex()].name();
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

    Kind _kind;
};

struct ConsoleServiceCommand :

    kf::mixin::NonCopyable,
    HasIdentifier,
    CreateWithArenaAndConfig<ConsoleServiceCommand>

{
    using Argument = ConsoleServiceArgument;

    struct Context {
        kf::Slice<Argument const> arguments;
        ConsoleServiceChannel::Output &output;
        kf::units::Milliseconds timestamp;
        kf::u8 channel_num;
    };

    [[nodiscard]] static constexpr auto allocationLength(ConsoleServiceConfig const &config) noexcept {
        return static_cast<kf::usize>(config.max_command_argument_count * sizeof(Argument));
    }

    // properties

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

    [[nodiscard]] constexpr bool addEnumArgument(Identifier id, Argument::EnumParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addBooleanArgument(Identifier id, Argument::BooleanParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addIntegerArgument(Identifier id, Argument::IntegerParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addRealArgument(Identifier id, Argument::RealParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addStringArgument(Identifier id, Argument::StringParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    // control

    void execute(Context const &context) const noexcept {
        _handler(context);
    }

private:
    kf::Stack<Argument> _arguments;
    kf::Function<void(Context const &)> _handler;

    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleServiceCommand>;
    explicit constexpr ConsoleServiceCommand(kf::Arena &arena, ConsoleServiceConfig const &config, Identifier id, auto &&handler) noexcept :
        HasIdentifier{id},
        _arguments{arena.allocate<Argument>(config.max_command_argument_count)},
        _handler{std::forward<decltype(handler)>(handler)} {}
};

struct ConsoleServiceNamespace :

    kf::mixin::NonCopyable,
    kf::mixin::Configured<internal::ConsoleServiceConfig>,
    HasIdentifier,
    CreateWithArenaAndConfig<ConsoleServiceNamespace>,
    private NamedItemContainer<ConsoleServiceCommand>

{

    [[nodiscard]] static constexpr auto allocationLength(ConsoleServiceConfig const &config) noexcept {
        return static_cast<kf::usize>(config.max_command_count * sizeof(ConsoleServiceCommand));
    }

    [[nodiscard]] auto getCommand(kf::StringView name_or_shortcut) noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] auto addCommand(kf::Arena &arena, Identifier id, auto &&handler) noexcept {
        return this->add(arena, this->config(), std::move(id), std::forward<decltype(handler)>(handler));
    }

private:
    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleServiceNamespace>;
    explicit constexpr ConsoleServiceNamespace(kf::Arena &arena, ConsoleServiceConfig const &config, Identifier id) noexcept :
        kf::mixin::Configured<ConsoleServiceConfig>{config},
        HasIdentifier{id},
        NamedItemContainer<ConsoleServiceCommand>{arena, config.max_command_count} {}
};

using ConsoleServiceChannelContainer = ItemContainer<ConsoleServiceChannel>;

using ConsoleServiceNamespaceContainer = NamedItemContainer<ConsoleServiceNamespace>;

}// namespace botix::internal

namespace botix::service {

struct ConsoleService final :

    Service<ConsoleService>,
    kf::mixin::Configured<internal::ConsoleServiceConfig>,
    internal::CreateWithArenaAndConfig<ConsoleService>,
    private internal::ConsoleServiceChannelContainer,
    private internal::ConsoleServiceNamespaceContainer

{
    using Config = internal::ConsoleServiceConfig;
    using Channel = internal::ConsoleServiceChannel;
    using Command = internal::ConsoleServiceCommand;
    using Namespace = internal::ConsoleServiceNamespace;

    [[nodiscard]] static constexpr auto allocationLength(Config const &config) noexcept {
        return static_cast<kf::usize>(config.max_channel_count * sizeof(Channel) + config.max_namespace_count * sizeof(Namespace));
    }

    // TODO: help command

    // channel

    [[nodiscard]] decltype(auto) channels() const noexcept {
        return internal::ConsoleServiceChannelContainer::items();
    }

    [[nodiscard]] decltype(auto) channels() noexcept {
        return internal::ConsoleServiceChannelContainer::items();
    }

    [[nodiscard]] decltype(auto) addChannel(kf::Arena &arena, bool echo) noexcept {
        return internal::ConsoleServiceChannelContainer::add(arena, this->config(), std::move(echo));
    }

    // command namespace

    [[nodiscard]] decltype(auto) namespaces() const noexcept {
        return internal::ConsoleServiceNamespaceContainer::items();
    }

    [[nodiscard]] decltype(auto) namespaces() noexcept {
        return internal::ConsoleServiceNamespaceContainer::items();
    }

    [[nodiscard]] decltype(auto) globalNamespace() noexcept {
        return namespaces()[0];
    }

    [[nodiscard]] decltype(auto) getNamespace(kf::StringView name_or_shortcut) noexcept {
        return internal::ConsoleServiceNamespaceContainer::get(name_or_shortcut);
    }

    [[nodiscard]] decltype(auto) addNamespace(kf::Arena &arena, internal::Identifier id) {
        return internal::ConsoleServiceNamespaceContainer::add(arena, this->config(), id);
    }

private:
    kf::Logger _logger{"ConsoleService"};

    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleService>;
    explicit ConsoleService(kf::Arena &arena, Config const &config) noexcept :
        kf::mixin::Configured<Config>{config},
        internal::ConsoleServiceChannelContainer{arena, config.max_channel_count},
        internal::ConsoleServiceNamespaceContainer{arena, config.max_namespace_count} {
        (void) this->addNamespace(arena, {.name = "global", .shortcut = kf::none});
    }

    [[nodiscard]] auto resolveCommand(kf::StringView lexeme) noexcept -> kf::Option<Command &> {
        auto const maybe_delimeter_index = lexeme.indexOf('.');

        if (maybe_delimeter_index.isNone()) {
            return globalNamespace().getCommand(lexeme);
        }

        auto const delimeter_index = maybe_delimeter_index.unwrap();

        auto namespace_ = getNamespace(lexeme.first(delimeter_index));
        if (namespace_.isNone()) { return kf::none; }

        return namespace_.unwrap().getCommand(lexeme.fromOffset(delimeter_index + 1));
    }

    void onInputLine(kf::units::Milliseconds timestamp, Channel &channel, kf::u8 channel_num) noexcept {
        if (channel.echo) {
            channel.output.print("[#{}]>>> {}", channel_num, channel.input_line.view());
        }

        kf::StringView tokens_buffer[this->config().max_command_argument_count]{};

        auto tokens = channel.input_line.view().trim().split({tokens_buffer, this->config().max_command_argument_count});

        if (tokens.empty()) {
            return;
        }

        auto const name = tokens[0];
        auto maybe_command = resolveCommand(name);

        if (maybe_command.isNone()) {
            channel.output.error("unknown command: {}", name);
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
                channel.output.print("note: failed argument name: '{}'", argument.name());
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
            .channel_num = channel_num,
        });
    }

    void processChannel(kf::units::Milliseconds timestamp, Channel &channel, kf::u8 channel_num) noexcept {
        while (channel.input_queue.availableForRead() > 0) {
            char const c = channel.input_queue.read().unwrap();

            switch (c) {
                case '\b':
                case '\x7F':
                    if (channel.input_line.read().isNone()) {
                        _logger.debug("input buffer is empty");
                    }
                    break;

                case '\t':
                    _logger.debug("tab");
                    break;

                case '\n':
                    onInputLine(timestamp, channel, channel_num);
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

    BOTIX_IMPL_SERVICE(ConsoleService);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        for (kf::u8 channel_num = 0; channel_num < this->channels().length(); channel_num += 1) {
            processChannel(now, this->channels()[channel_num], channel_num);
        }
    }
};

}// namespace botix::service