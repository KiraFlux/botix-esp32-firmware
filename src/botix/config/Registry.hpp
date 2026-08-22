// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <type_traits>

#include <kf/Option.hpp>
#include <kf/Result.hpp>
#include <kf/Slice.hpp>
#include <kf/StringView.hpp>
#include <kf/core.hpp>

#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/ReprTo.hpp>

#include "botix/Parser.hpp"

namespace botix::internal {

enum class FieldKind : kf::u8 {

    // integers

    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,

    // float

    F32,
    F64,

    // mics

    Boolean,
    Char,
};

template<typename Impl> struct Field :

    kf::mixin::NonCopyable,
    kf::mixin::ReprTo<Field<Impl>>

{

    constexpr Field(kf::StringView name) noexcept :
        name{name} {}

    [[nodiscard]] bool set(kf::StringView lexeme) noexcept {
        return static_cast<Impl *>(this)->setImpl(lexeme);
    }

    [[nodiscard]] constexpr kf::usize reprValue(auto &char_writable) const noexcept {
        return static_cast<Impl const *>(this)->reprValueImpl(char_writable);
    }

    [[nodiscard]] constexpr kf::usize reprType(auto &char_writable) const noexcept {
        return static_cast<Impl const *>(this)->reprTypeImpl(char_writable);
    }

    kf::StringView const name;

private:
    KF_IMPL_REPR_TO(::botix::internal::Field<Impl>);
    constexpr kf::usize reprToImpl(auto &char_writable) const noexcept {
        return (
            char_writable.append(name) +
            char_writable.append(':') +
            char_writable.append(' ') +
            reprType(char_writable) +
            char_writable.append(' ') +
            char_writable.append('=') +
            char_writable.append(' ') +
            reprValue(char_writable));
    }
};

template<typename T> struct field_kind_by_type;

template<> struct field_kind_by_type<kf::i8> : std::integral_constant<FieldKind, FieldKind::I8> {};
template<> struct field_kind_by_type<kf::u8> : std::integral_constant<FieldKind, FieldKind::U8> {};
template<> struct field_kind_by_type<kf::i16> : std::integral_constant<FieldKind, FieldKind::I16> {};
template<> struct field_kind_by_type<kf::u16> : std::integral_constant<FieldKind, FieldKind::U16> {};
template<> struct field_kind_by_type<kf::i32> : std::integral_constant<FieldKind, FieldKind::I32> {};
template<> struct field_kind_by_type<kf::u32> : std::integral_constant<FieldKind, FieldKind::U32> {};
template<> struct field_kind_by_type<kf::i64> : std::integral_constant<FieldKind, FieldKind::I64> {};
template<> struct field_kind_by_type<kf::u64> : std::integral_constant<FieldKind, FieldKind::U64> {};
template<> struct field_kind_by_type<kf::f32> : std::integral_constant<FieldKind, FieldKind::F32> {};
template<> struct field_kind_by_type<kf::f64> : std::integral_constant<FieldKind, FieldKind::F64> {};
template<> struct field_kind_by_type<bool> : std::integral_constant<FieldKind, FieldKind::Boolean> {};
template<> struct field_kind_by_type<char> : std::integral_constant<FieldKind, FieldKind::Char> {};

template<> struct field_kind_by_type<kf::usize> : std::integral_constant<FieldKind, (4 == sizeof(kf::usize) ? FieldKind::U32 : FieldKind::U64)> {};

}// namespace botix::internal

namespace botix::config {

struct Registry

// : kf::mixin::NonCopyable

{

    struct ValueField : internal::Field<ValueField> {

        using Kind = internal::FieldKind;

        template<typename T> constexpr ValueField(kf::StringView name, T &value) noexcept :
            internal::Field<ValueField>{name},
            _ptr{static_cast<void *>(&value)},
            _length{kf::none},
            _kind{internal::field_kind_by_type<std::decay_t<T>>::value} {}

        template<typename T, kf::usize N> constexpr ValueField(kf::StringView name, T (&array)[N]) noexcept :
            internal::Field<ValueField>{name},
            _ptr{static_cast<void *>(array)},
            _length{N},
            _kind{internal::field_kind_by_type<std::decay_t<T>>::value} {}

    private:
        void *const _ptr;
        kf::Option<kf::usize> const _length;
        Kind const _kind;

        template<typename T> [[nodiscard]] constexpr bool setFrom(kf::StringView lexeme) const noexcept {
            if (_length.isNone()) {
                Parser<T> parser{};

                if (auto const parsed_value = parser.parse(lexeme); parsed_value.isSome()) {
                    *static_cast<T *>(_ptr) = parsed_value.unwrap();
                    return true;
                }

                return false;
            } else {

                if (Kind::Char == _kind) {

                    for (kf::usize i = 0; i < lexeme.length() and i < _length.unwrap(); i += 1) {
                        static_cast<char *>(_ptr)[i] = lexeme[i];
                    }
                    return true;
                }

                return false;
            }
        }

        [[nodiscard]] constexpr kf::StringView kindName() const noexcept {
            switch (_kind) {
                case Kind::I8: return "i8";
                case Kind::U8: return "u8";
                case Kind::I16: return "i16";
                case Kind::U16: return "u16";
                case Kind::I32: return "i32";
                case Kind::U32: return "u32";
                case Kind::I64: return "i64";
                case Kind::U64: return "u64";
                case Kind::F32: return "f32";
                case Kind::F64: return "f64";
                case Kind::Boolean: return "bool";
                case Kind::Char: return "char";
                default: return "?";
            }
        }

        template<typename T> [[nodiscard]] constexpr kf::usize reprValueTo(auto &char_writable) const noexcept {
            if (_length.isNone()) {
                return char_writable.append(*static_cast<T *>(_ptr));
            } else {
                return char_writable.append(kf::Slice<T>{
                    static_cast<T *>(_ptr),
                    _length.unwrap(),
                });
            }
        }

        // impl for internal::Field
        friend struct ::botix::internal::Field<ValueField>;

        bool setImpl(kf::StringView lexeme) noexcept {
            switch (_kind) {
                case Kind::I8: return setFrom<kf::i8>(lexeme);
                case Kind::U8: return setFrom<kf::u8>(lexeme);
                case Kind::I16: return setFrom<kf::i16>(lexeme);
                case Kind::U16: return setFrom<kf::u16>(lexeme);
                case Kind::I32: return setFrom<kf::i32>(lexeme);
                case Kind::U32: return setFrom<kf::u32>(lexeme);
                case Kind::I64: return setFrom<kf::i64>(lexeme);
                case Kind::U64: return setFrom<kf::u64>(lexeme);
                case Kind::F32: return setFrom<kf::f32>(lexeme);
                case Kind::F64: return setFrom<kf::f64>(lexeme);
                case Kind::Boolean: return setFrom<bool>(lexeme);
                case Kind::Char: return setFrom<char>(lexeme);
                default: return false;
            }
        }

        constexpr kf::usize reprTypeImpl(auto &char_writable) const noexcept {
            kf::usize written = char_writable.append(kindName());

            if (_length.isSome()) {
                written += char_writable.append('[');
                written += char_writable.append(_length.unwrap());
                written += char_writable.append(']');
            }

            return written;
        }

        constexpr kf::usize reprValueImpl(auto &char_writable) const noexcept {
            switch (_kind) {
                case Kind::I8: return reprValueTo<kf::i8>(char_writable);
                case Kind::U8: return reprValueTo<kf::u8>(char_writable);
                case Kind::I16: return reprValueTo<kf::i16>(char_writable);
                case Kind::U16: return reprValueTo<kf::u16>(char_writable);
                case Kind::I32: return reprValueTo<kf::i32>(char_writable);
                case Kind::U32: return reprValueTo<kf::u32>(char_writable);
                case Kind::I64: return reprValueTo<kf::i64>(char_writable);
                case Kind::U64: return reprValueTo<kf::u64>(char_writable);
                case Kind::F32: return reprValueTo<kf::f32>(char_writable);
                case Kind::F64: return reprValueTo<kf::f64>(char_writable);
                case Kind::Boolean: return reprValueTo<bool>(char_writable);
                case Kind::Char: return reprValueTo<char>(char_writable);
                default: return char_writable.append('?');
            }
        }
    };

    struct EnumField : internal::Field<EnumField> {

        using Underlying = kf::u8;

        struct Entry {

            constexpr Entry(kf::StringView key, kf::enum_type auto value) noexcept :
                key{key}, value{static_cast<Underlying>(value)} {}

            kf::StringView key;
            Underlying value;
        };

        using EntryCollection = kf::Slice<Entry const>;

        template<kf::enum_type T> constexpr EnumField(kf::StringView name, T &value, EntryCollection entries) noexcept :
            internal::Field<EnumField>{name}, value{reinterpret_cast<Underlying &>(value)}, entries{entries} {
            static_assert(sizeof(Underlying) == sizeof(T));
        }

    private:
        Underlying &value;
        EntryCollection entries;

        // impl for internal::Field
        friend struct ::botix::internal::Field<EnumField>;

        bool setImpl(kf::StringView target_key) noexcept {
            for (auto const &e: entries) {
                if (e.key == target_key) {
                    value = e.value;
                    return true;
                }
            }
            return false;
        }

        constexpr kf::usize reprTypeImpl(auto &char_writable) const noexcept {
            kf::usize written = 0;

            bool write_delimeter = false;

            for (auto const &e: entries) {
                if (write_delimeter) {
                    written += char_writable.append('|');
                }
                write_delimeter = true;
                written += char_writable.append(e.key);
            }

            return written;
        }

        constexpr kf::usize reprValueImpl(auto &char_writable) const noexcept {
            for (auto const &e: entries) {
                if (e.value == value) {
                    return char_writable.append(e.key);
                }
            }

            return char_writable.append('?');
        }
    };

    struct Error {
        kf::StringView message;
    };

    [[nodiscard]] auto findValueField(kf::StringView name) noexcept {
        return findField(value_fields, name);
    }

    [[nodiscard]] auto findEnumField(kf::StringView name) noexcept {
        return findField(enum_fields, name);
    }

    [[nodiscard]] auto set(kf::StringView name, kf::StringView lexeme) noexcept -> kf::Result<void, Error> {

        if (auto field = findValueField(name); field.isSome()) {
            if (field.unwrap().set(lexeme)) {
                return kf::ok();
            } else {
                return kf::error(Error{.message{"value parsing failed"}});
            }
        }

        if (auto field = findEnumField(name); field.isSome()) {
            if (field.unwrap().set(lexeme)) {
                return kf::ok();
            } else {
                return kf::error(Error{.message{"enum option not found"}});
            }
        }

        return kf::error(Error{.message{"field not found"}});
    }

    kf::Slice<ValueField> value_fields;
    kf::Slice<EnumField> enum_fields;

private:
    template<typename T> [[nodiscard]] static auto findField(kf::Slice<T> fields, kf::StringView name) noexcept -> kf::Option<T &> {
        return fields.firstWhere([name](auto const &field) {
            return field.name == name;
        });
    }
};

}// namespace botix::config