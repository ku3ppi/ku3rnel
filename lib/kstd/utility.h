#ifndef KSTD_UTILITY_H
#define KSTD_UTILITY_H

#include <kstd/cstddef.h> // For kstd::size_t (though not directly used by move/pair here)

namespace kstd {

// kstd::remove_reference (similar to std::remove_reference)
template <typename T> struct remove_reference      { using type = T; };
template <typename T> struct remove_reference<T&>  { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;


// kstd::move (similar to std::move)
// Converts a value to an rvalue reference, enabling move semantics.
template <typename T>
constexpr kstd::remove_reference_t<T>&& move(T&& arg) noexcept {
    return static_cast<kstd::remove_reference_t<T>&&>(arg);
}

// kstd::forward (similar to std::forward)
// For perfect forwarding of arguments.
template <typename T>
constexpr T&& forward(kstd::remove_reference_t<T>& arg) noexcept {
    return static_cast<T&&>(arg);
}

template <typename T>
constexpr T&& forward(kstd::remove_reference_t<T>&& arg) noexcept {
    // This static_assert would prevent forwarding an rvalue as an lvalue,
    // which is a common use case for std::forward with rvalue arguments.
    // static_assert(!is_lvalue_reference<T>::value, "cannot forward an rvalue as an lvalue");
    return static_cast<T&&>(arg);
}


// kstd::pair (similar to std::pair)
template <typename T1, typename T2>
struct pair {
    T1 first;
    T2 second;

    // Default constructor
    constexpr pair() : first(), second() {}

    // Initialization constructor
    constexpr pair(const T1& x, const T2& y) : first(x), second(y) {}

    // Move constructor
    template <typename U1, typename U2>
    constexpr pair(U1&& x, U2&& y) : first(kstd::forward<U1>(x)), second(kstd::forward<U2>(y)) {}

    // Copy constructor
    constexpr pair(const pair& p) = default;
    // constexpr pair(const pair& p) : first(p.first), second(p.second) {}


    // Move assignment
    // pair& operator=(pair&& p) noexcept {
    //     first = kstd::forward<T1>(p.first);
    //     second = kstd::forward<T2>(p.second);
    //     return *this;
    // }

    // Copy assignment
    // pair& operator=(const pair& p) {
    //     first = p.first;
    //     second = p.second;
    //     return *this;
    // }


    // Basic comparison operators (optional, add as needed)
    bool operator==(const pair<T1, T2>& other) const {
        return first == other.first && second == other.second;
    }
    bool operator!=(const pair<T1, T2>& other) const {
        return !(*this == other);
    }
    // Add <, <=, >, >= if ordering is needed
};

// kstd::make_pair (similar to std::make_pair)
template <typename T1, typename T2>
constexpr kstd::pair<T1, T2> make_pair(T1&& t, T2&& u) {
    return kstd::pair<T1, T2>(kstd::forward<T1>(t), kstd::forward<T2>(u));
}

} // namespace kstd

#endif // KSTD_UTILITY_H
