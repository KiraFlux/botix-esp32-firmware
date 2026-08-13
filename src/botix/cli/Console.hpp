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

#include <kf/mixin/Callbacked.hpp>
#include <kf/mixin/Configured.hpp>
#include <kf/mixin/Flush.hpp>
#include <kf/mixin/Labeled.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>
#include <kf/mixin/TimedPollable.hpp>

namespace botix::internal {// TODO: move to botix/cli and split it

struct ConsoleConfig : kf::mixin::Resettable<ConsoleConfig> {

    kf::u8
        max_channel_count{0x08},
        max_namespace_count{0x10},
        max_command_count{0x10},
        max_command_argument_count{0x08};

    kf::u16
        channel_input_queue_length{0x02'00},
        channel_input_line_length{0x00'80},
        channel_output_line_length{0x00'80};

private:
    // TODO: extract as mixin::ResettableConfig<Impl>
    KF_IMPL_RESETTABLE(ConsoleConfig);
    constexpr void resetImpl() noexcept {
        *this = ConsoleConfig{};
    }
};

struct CreateWithArenaAndConfigTag {};

template<typename Impl> struct CreateWithArenaAndConfig : CreateWithArenaAndConfigTag {// TODO: kf::mixin::CreateWithArena<typename ...AllocLengthGetterArgs>

    // TODO: create Impl instance on arena too. returns Option<Impl &>
    template<typename... Args> [[nodiscard]] static constexpr auto create(
        kf::Arena &arena, ConsoleConfig const &config, Args &&...args) noexcept -> kf::Option<Impl> {
        if (arena.available() < Impl::allocationLength(config)) { return kf::none; }
        return kf::some(Impl{arena, config, std::forward<Args>(args)...});
    }
};

// TODO: impl ReprTo for Identifier
struct Identifier {

    kf::StringView name;
    kf::Option<char> shortcut{name.empty() ? kf::none : kf::some(name[0])};

    [[nodiscard]] constexpr bool match(kf::StringView name_or_shortcut) const noexcept {
        return (name == name_or_shortcut) or (name_or_shortcut.length() == 1 and shortcut.unwrapOr(0) == name_or_shortcut[0]);
    }
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
    template<typename... Args> [[nodiscard]] auto add(kf::Arena &arena, Args &&...args) noexcept -> kf::Option<T &> {
        if (_items.full()) { return kf::none; }

        auto maybe_item = T::create(arena, std::forward<Args>(args)...);
        if (maybe_item.isNone()) { return kf::none; }

        (void) _items.write(std::move(maybe_item).unwrap());
        return _items.top();
    }

private:
    kf::Stack<T> _items;
};

template<kf::implements<Identifier> T> struct NamedItemContainer : ItemContainer<T> {
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

template<kf::trivial T> struct ValueItem<T> : Identifier {
    using ValueType = T;

    constexpr ValueItem(Identifier id, ValueType value) noexcept :
        Identifier{id}, _value{value} {}

    [[nodiscard]] constexpr T value() const noexcept {
        return _value;
    }

private:
    ValueType _value;
};

template<> struct ValueItem<kf::StringView> : Identifier {
    using ValueType = kf::StringView;

    constexpr ValueItem(Identifier id) noexcept :
        Identifier{id} {}

    [[nodiscard]] constexpr kf::StringView value() const noexcept {
        return this->name;
    }
};

//

struct ConsoleChannel :

    kf::mixin::NonCopyable,
    CreateWithArenaAndConfig<ConsoleChannel>

{

    struct Parameters {
        bool echo;
    };

    struct Input :

        kf::mixin::NonCopyable

    {

        enum class Status {
            Idle,         // Queue is empty, nothing to process
            LineReady,    // Line can be consumed (reach enter or buffer is full)
            HintRequested,// Tabulation
        };

        explicit constexpr Input(kf::Slice<char> input_queue_buffer, kf::Slice<char> input_line_buffer) noexcept :
            _input_queue{input_queue_buffer}, _input_line{input_line_buffer} {}

        [[nodiscard]] bool feed(kf::StringView str) noexcept {
            for (auto const c: str) {
                if (not _input_queue.write(c)) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] auto peekLine() noexcept {
            return _input_line.view();
        }

        [[nodiscard]] auto consumeLine() noexcept {
            auto const line = _input_line.view();
            _input_line.reset();// just pointer got moved, no string modification
            return line;        // still view at line
        }

        [[nodiscard]] Status process() noexcept {
            while (_input_queue.availableForRead() > 0) {
                char const c = _input_queue.read().unwrap();

                switch (c) {
                    case '\t':
                        return Status::HintRequested;

                    case '\n':
                        return Status::LineReady;

                    case '\b':
                    case '\x7F':
                        (void) _input_line.read();// discarded: "common behavior"
                        break;

                    case 0x20 ... 0x7E:
                        if (not _input_line.write(c)) {
                            return Status::LineReady;
                        }
                        break;
                }
            }
            return Status::Idle;
        }

    private:
        kf::Queue<char> _input_queue;
        kf::String _input_line;
    };

    struct Output :

        kf::mixin::NonCopyable,
        kf::mixin::Callbacked<void(kf::StringView)>,// TODO: actually, should not be callbacked. just need consume() -> StringView
        kf::mixin::Flush<Output>

    {

        explicit constexpr Output(kf::Slice<char> buffer) noexcept :
            _output_string{buffer} {}

        template<typename... Args> void error(kf::internal::FormatString<Args...> const &fmt, Args const &...args) noexcept {
            _output_string.append("error: ");
            print(fmt, args...);
        }

        template<typename... Args> void print(kf::internal::FormatString<Args...> const &fmt, Args const &...args) noexcept {
            _output_string.format(fmt, args...);
            (void) _output_string.write('\n');
            this->flush();
        }

    private:
        kf::String _output_string;

        KF_IMPL_FLUSH(Output);
        constexpr void flushImpl() noexcept {
            if (not _output_string.empty()) {
                this->invoke(_output_string.view());
            }
            _output_string.reset();
        }
    };

    struct Context {
        kf::StringView input_line;
        Parameters const &parameters;
        Output &output;
        kf::units::Milliseconds timestamp;
        kf::u8 num;
    };

    Parameters parameters;
    Input input;
    Output output;

private:
    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleChannel>;
    explicit constexpr ConsoleChannel(kf::Arena &arena, ConsoleConfig const &config, Parameters parameters) noexcept :
        parameters{parameters},
        input{arena.allocate<char>(config.channel_input_queue_length), arena.allocate<char>(config.channel_input_line_length)},
        output{arena.allocate<char>(config.channel_output_line_length)} {}

    static constexpr auto allocationLength(ConsoleConfig const &config) noexcept {
        return static_cast<kf::usize>(config.channel_input_queue_length + config.channel_input_line_length + config.channel_output_line_length);
    }
};

struct ConsoleArgument :// TODO: make as dyn interface. default getters returns safe mock values.

                        Identifier,
                        kf::mixin::NonCopyable,
                        kf::mixin::Resettable<ConsoleArgument>

{

    enum class Kind : kf::u8 {
        Enum,
        Boolean,
        Integer,
        Real,
        String,// TODO: add IPv4
    };

    struct ParseContext {
        ConsoleChannel::Output &channel_output;
        kf::StringView lexeme;// not empty

        template<kf::implements<Identifier> T> [[nodiscard]] constexpr auto parseEnumerated(kf::Slice<T const> items) const noexcept -> kf::Option<T const &> {
            for (auto const &item: items) {// TODO: impl for Slice firstWhere(F: bool(T)) -> Option<T &>
                if (item.match(lexeme)) {
                    return kf::someRef(item);
                }
            }

            channel_output.error("'{}' not allowed, use:", lexeme);
            for (auto const &item: items) {
                if (item.shortcut.isSome()) {
                    channel_output.error("\t'{}' ('{}')", item.name, item.shortcut.unwrap());
                } else {
                    channel_output.error("\t'{}'", item.name);
                }
            }

            return kf::none;
        }
    };

    // TODO: rename to one thins (no ..s)
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

    // TODO: make as kf::mixin::Parsable<Impl, InputType, OutputType> static interface
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
                    this->params.value = maybe_item.unwrap().name;
                    return true;
                }

                return false;
            }

            this->params.value = context.lexeme;
            return true;
        }
    };

    // construct

    explicit constexpr ConsoleArgument(Identifier id, EnumParameters params) noexcept :
        Identifier{id}, _enum{params}, _kind{Kind::Enum} {}

    explicit constexpr ConsoleArgument(Identifier id, BooleanParameters params) noexcept :
        Identifier{id}, _boolean{params}, _kind{Kind::Boolean} {}

    explicit constexpr ConsoleArgument(Identifier id, IntegerParameters params) noexcept :
        Identifier{id}, _integer{params}, _kind{Kind::Integer} {}

    explicit constexpr ConsoleArgument(Identifier id, RealParameters params) noexcept :
        Identifier{id}, _real{params}, _kind{Kind::Real} {}

    explicit constexpr ConsoleArgument(Identifier id, StringParameters params) noexcept :
        Identifier{id}, _string{params}, _kind{Kind::String} {}

    // get

    [[nodiscard]] constexpr auto enumIndex() const noexcept {
        return _enum.params.value;
    }

    template<kf::enum_type E> [[nodiscard]] constexpr auto enumValue() const noexcept {
        return static_cast<E>(enumIndex());
    }

    [[nodiscard]] constexpr auto enumName() const noexcept {
        return _enum.items[enumIndex()].name;
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

    [[nodiscard]] constexpr bool positional() const noexcept {
        auto const is_positional = [](auto const &x) {
            return x.params.default_value.isNone();
        };
        switch (_kind) {
            case Kind::Enum: return is_positional(_enum);
            case Kind::Boolean: return is_positional(_boolean);
            case Kind::Integer: return is_positional(_integer);
            case Kind::Real: return is_positional(_real);
            case Kind::String: return is_positional(_string);
            default: return true;
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

private:
    union {
        Enum _enum;
        Boolean _boolean;
        Integer _integer;
        Real _real;
        String _string;
    };

    Kind _kind;

    KF_IMPL_RESETTABLE(ConsoleArgument);
    void resetImpl() noexcept {
        switch (_kind) {
            case Kind::Enum: _enum.params.reset(); break;
            case Kind::Boolean: _boolean.params.reset(); break;
            case Kind::Integer: _integer.params.reset(); break;
            case Kind::Real: _real.params.reset(); break;
            case Kind::String: _string.params.reset(); break;
        };
    }
};

struct ConsoleCommand :

    kf::mixin::NonCopyable,
    Identifier,
    CreateWithArenaAndConfig<ConsoleCommand>

{
    using Argument = ConsoleArgument;

    struct Context {
        ConsoleChannel::Context const &channel;
        kf::Slice<Argument const> arguments;
    };

    // properties

    [[nodiscard]] constexpr auto arguments() noexcept -> kf::Slice<Argument> {
        return _arguments.slice();
    }

    [[nodiscard]] constexpr auto arguments() const noexcept -> kf::Slice<Argument const> {
        return _arguments.slice();
    }

    [[nodiscard]] constexpr kf::usize positionalArgumentsCount() const noexcept {
        kf::usize count = 0;
        for (auto const &a: _arguments) {
            count += static_cast<kf::usize>(a.positional());
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

    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleCommand>;
    explicit constexpr ConsoleCommand(kf::Arena &arena, ConsoleConfig const &config, Identifier id, auto &&handler) noexcept :
        Identifier{id},
        _arguments{arena.allocate<Argument>(config.max_command_argument_count)},
        _handler{std::forward<decltype(handler)>(handler)} {}

    static constexpr auto allocationLength(ConsoleConfig const &config) noexcept {
        return static_cast<kf::usize>(config.max_command_argument_count * sizeof(Argument));
    }
};

struct ConsoleNamespace :

    kf::mixin::NonCopyable,
    kf::mixin::Configured<internal::ConsoleConfig>,
    Identifier,
    CreateWithArenaAndConfig<ConsoleNamespace>,
    private NamedItemContainer<ConsoleCommand>

{

    [[nodiscard]] auto getCommand(kf::StringView name_or_shortcut) noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] auto addCommand(kf::Arena &arena, Identifier id, auto &&handler) noexcept {
        return this->add(arena, this->config(), std::move(id), std::forward<decltype(handler)>(handler));
    }

private:
    friend struct ::botix::internal::CreateWithArenaAndConfig<ConsoleNamespace>;
    explicit constexpr ConsoleNamespace(kf::Arena &arena, ConsoleConfig const &config, Identifier id) noexcept :
        kf::mixin::Configured<ConsoleConfig>{config},
        Identifier{id},
        NamedItemContainer<ConsoleCommand>{arena, config.max_command_count} {}

    static constexpr auto allocationLength(ConsoleConfig const &config) noexcept {
        return static_cast<kf::usize>(config.max_command_count * sizeof(ConsoleCommand));
    }
};

using ConsoleChannelContainer = ItemContainer<ConsoleChannel>;

using ConsoleNamespaceContainer = NamedItemContainer<ConsoleNamespace>;

}// namespace botix::internal

namespace botix::cli {

struct Console final :

    kf::mixin::Configured<internal::ConsoleConfig>,
    kf::mixin::TimedPollable<Console>,
    internal::CreateWithArenaAndConfig<Console>,
    private internal::ConsoleChannelContainer,
    private internal::ConsoleNamespaceContainer

{
    using Config = internal::ConsoleConfig;
    using Channel = internal::ConsoleChannel;
    using Command = internal::ConsoleCommand;
    using Namespace = internal::ConsoleNamespace;

    // TODO: help command

    // channel

    [[nodiscard]] decltype(auto) channels() const noexcept {
        return internal::ConsoleChannelContainer::items();
    }

    [[nodiscard]] decltype(auto) channels() noexcept {
        return internal::ConsoleChannelContainer::items();
    }

    [[nodiscard]] decltype(auto) addChannel(kf::Arena &arena, Channel::Parameters params) noexcept {
        return internal::ConsoleChannelContainer::add(arena, this->config(), params);
    }

    // command namespace

    [[nodiscard]] decltype(auto) namespaces() const noexcept {
        return internal::ConsoleNamespaceContainer::items();
    }

    [[nodiscard]] decltype(auto) namespaces() noexcept {
        return internal::ConsoleNamespaceContainer::items();
    }

    [[nodiscard]] decltype(auto) globalNamespace() noexcept {
        return namespaces()[0];
    }

    [[nodiscard]] decltype(auto) getNamespace(kf::StringView name_or_shortcut) noexcept {
        return internal::ConsoleNamespaceContainer::get(name_or_shortcut);
    }

    [[nodiscard]] decltype(auto) addNamespace(kf::Arena &arena, internal::Identifier id) {
        return internal::ConsoleNamespaceContainer::add(arena, this->config(), id);
    }

private:
    kf::Logger _logger{"Console"};

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

    void onInputLineReady(Channel::Context const &channel_context) noexcept {
        if (channel_context.parameters.echo) {
            channel_context.output.print("[#{}]>>> {}", channel_context.num, channel_context.input_line);
        }

        kf::StringView tokens_buffer[this->config().max_command_argument_count]{};

        auto tokens = channel_context.input_line.trim().split({tokens_buffer, this->config().max_command_argument_count});

        if (tokens.empty()) {
            return;
        }

        auto const name = tokens[0];
        auto maybe_command = resolveCommand(name);

        if (maybe_command.isNone()) {
            channel_context.output.error("unknown command: {}", name);
            return;
        }

        auto &command = maybe_command.unwrap();
        auto argument_tokens = tokens.fromOffset(1);

        if (argument_tokens.length() < command.positionalArgumentsCount() or argument_tokens.length() > command.arguments().length()) {
            channel_context.output.error("expected {}..{} arguments, got {}", command.positionalArgumentsCount(), command.arguments().length(), argument_tokens.length());
            return;
        }

        auto argument_index = 0;
        bool parse_failed = false;
        while (argument_index < argument_tokens.length()) {
            auto lexeme = argument_tokens[argument_index];
            auto &argument = command.arguments()[argument_index];

            if (not argument.parse({.channel_output = channel_context.output, .lexeme = lexeme})) {
                parse_failed = true;
                channel_context.output.print("note: failed argument '{}'", argument.name);
            }

            argument_index += 1;
        }

        if (parse_failed) {
            channel_context.output.error("failed to parse positional argument(s)");
            return;
        }

        auto default_value_arguments = command.arguments().fromOffset(argument_index);
        for (auto &a: default_value_arguments) {
            a.reset();
        }

        command.execute({
            .channel = channel_context,
            .arguments = command.arguments(),
        });
    }

    KF_IMPL_TIMED_POLLABLE(Console);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        for (kf::u8 channel_num = 0; channel_num < this->channels().length(); channel_num += 1) {
            auto &channel = this->channels()[channel_num];

            switch (channel.input.process()) {
                case Channel::Input::Status::Idle:
                    break;

                case Channel::Input::Status::HintRequested:
                    _logger.debug("hint: '{}'", channel.input.peekLine());// TODO: parse, suggestion depends on it.
                    break;

                case Channel::Input::Status::LineReady:
                    onInputLineReady({
                        .input_line = channel.input.consumeLine(),
                        .parameters = channel.parameters,
                        .output = channel.output,
                        .timestamp = now,
                        .num = channel_num,
                    });
                    break;
            }
        }
    }

    friend struct ::botix::internal::CreateWithArenaAndConfig<Console>;
    explicit Console(kf::Arena &arena, Config const &config) noexcept :
        kf::mixin::Configured<Config>{config},
        internal::ConsoleChannelContainer{arena, config.max_channel_count},
        internal::ConsoleNamespaceContainer{arena, config.max_namespace_count} {
        (void) this->addNamespace(arena, {.name = "global", .shortcut = kf::none});
    }

    static constexpr auto allocationLength(Config const &config) noexcept {
        return static_cast<kf::usize>(config.max_channel_count * sizeof(Channel) + config.max_namespace_count * sizeof(Namespace));
    }
};

}// namespace botix::cli