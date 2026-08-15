// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Option.hpp>
#include <kf/Slice.hpp>
#include <kf/StringView.hpp>
#include <kf/core.hpp>

#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/ReprTo.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/cli/Channel.hpp"
#include "botix/cli/Identifier.hpp"

namespace botix::internal {

template<typename T> struct ValueItem;

template<typename T> struct ValueItemBase :

    cli::Identifier,
    kf::mixin::ReprTo<ValueItemBase<T>>

{
private:
    KF_IMPL_REPR_TO(ValueItemBase<T>);
    constexpr kf::usize reprToImpl(auto &char_writable) const noexcept {
        return char_writable.append(this->name);
    }
};

template<kf::trivial T> struct ValueItem<T> : ValueItemBase<T> {
    using ValueType = T;

    constexpr ValueItem(cli::Identifier id, ValueType value) noexcept :
        ValueItemBase<T>{id}, _value{value} {}

    [[nodiscard]] constexpr T value() const noexcept {
        return _value;
    }

private:
    ValueType _value;
};

template<> struct ValueItem<kf::StringView> : ValueItemBase<kf::StringView> {
    using ValueType = kf::StringView;

    constexpr ValueItem(cli::Identifier id) noexcept :
        ValueItemBase<kf::StringView>{id} {}

    [[nodiscard]] constexpr kf::StringView value() const noexcept {
        return this->name;
    }
};

struct ArgumentBase {

    struct ParseContext {
        cli::Channel::Output &channel_output;
        kf::StringView lexeme;// not empty

        template<kf::implements<cli::Identifier> T> [[nodiscard]] constexpr auto parseEnumerated(kf::Slice<T const> items) const noexcept -> kf::Option<T const &> {
            auto maybe_item = items.firstWhere([this](auto const &item) { return item.match(lexeme); });
            if (maybe_item.isNone()) {
                channel_output.error("'{}' not allowed, use: {}", lexeme, items);
            }
            return maybe_item;
        }
    };

    // TODO: make as kf::mixin::Parsable<Impl, InputType, OutputType> static interface
    template<typename Impl> struct Parsable {
        [[nodiscard]] constexpr bool parse(ParseContext const &context) noexcept {
            return static_cast<Impl *>(this)->parseImpl(context);
        }
    };

    // TODO: rename to one thing (no ..s)
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
                {{.name = "true"}, true},
                {{.name = "false"}, false},
                {{.name = "yes"}, true},
                {{.name = "no"}, false},
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
};

}// namespace botix::internal

namespace botix::cli {

// TODO: make as dyn interface. default getters returns safe mock values.
struct Argument :

    internal::ArgumentBase,
    Identifier,
    kf::mixin::NonCopyable,
    kf::mixin::Resettable<Argument>,
    kf::mixin::ReprTo<Argument>

{

    enum class Kind : kf::u8 {
        Enum,
        Boolean,
        Integer,
        Real,
        String,// TODO: add IPv4
    };

    // construct

    explicit constexpr Argument(Identifier id, EnumParameters params) noexcept :
        Identifier{id}, _enum{params}, _kind{Kind::Enum} {}

    explicit constexpr Argument(Identifier id, BooleanParameters params) noexcept :
        Identifier{id}, _boolean{params}, _kind{Kind::Boolean} {}

    explicit constexpr Argument(Identifier id, IntegerParameters params) noexcept :
        Identifier{id}, _integer{params}, _kind{Kind::Integer} {}

    explicit constexpr Argument(Identifier id, RealParameters params) noexcept :
        Identifier{id}, _real{params}, _kind{Kind::Real} {}

    explicit constexpr Argument(Identifier id, StringParameters params) noexcept :
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

    template<kf::implements<Identifier> T> static constexpr kf::usize reprList(auto &char_writable, kf::Slice<T const> items) noexcept {
        bool f = false;
        kf::usize write_count = 0;
        for (auto const &item: items) {
            if (f) {
                write_count += char_writable.append('|');
            }
            f = true;
            write_count += char_writable.append(item.name);
        }
        return write_count;
    }

    constexpr kf::usize reprType(auto &char_writable) const noexcept {
        switch (_kind) {
            case Kind::Enum: return reprList(char_writable, _enum.items);
            case Kind::Boolean: return char_writable.append("bool");
            case Kind::Integer: return char_writable.append("int");
            case Kind::Real: return char_writable.append("float");
            case Kind::String:
                if (_string.options.empty()) {
                    return char_writable.append("str");
                } else {
                    return reprList(char_writable, _string.options);
                }
        }
        return 0;
    }

    constexpr kf::usize reprDefault(auto &char_writable) const noexcept {
        switch (_kind) {
            case Kind::Enum: return char_writable.append(_enum.params.default_value.unwrap());
            case Kind::Boolean: return char_writable.append(_boolean.params.default_value.unwrap());
            case Kind::Integer: return char_writable.append(_integer.params.default_value.unwrap());
            case Kind::Real: return char_writable.append(_real.params.default_value.unwrap());
            case Kind::String: return (
                char_writable.append('"') +
                char_writable.append(_string.params.default_value.unwrap()) +
                char_writable.append('"'));
        }
        return 0;
    }

    KF_IMPL_REPR_TO(Argument);
    constexpr kf::usize reprToImpl(auto &char_writable) const noexcept {
        bool const has_default = not positional();

        kf::usize write_count = char_writable.append(has_default ? '[' : '<');
        write_count += char_writable.append(this->name);
        write_count += char_writable.append(':');
        write_count += char_writable.append(' ');

        write_count += this->reprType(char_writable);

        if (has_default) {
            write_count += char_writable.append('=');
            write_count += reprDefault(char_writable);
        }

        write_count += char_writable.append(has_default ? ']' : '>');
        return write_count;
    }

    KF_IMPL_RESETTABLE(Argument);
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

}// namespace botix::cli