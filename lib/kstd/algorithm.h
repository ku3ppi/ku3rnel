#ifndef KSTD_ALGORITHM_H
#define KSTD_ALGORITHM_H

namespace kstd {

template <typename T>
constexpr const T& min(const T& a, const T& b) {
    return (b < a) ? b : a;
}

template <typename T>
constexpr const T& max(const T& a, const T& b) {
    return (a < b) ? b : a;
}

template <typename T>
void swap(T& a, T& b) noexcept {
    T temp = static_cast<T&&>(a); // Equivalent to kstd::move(a)
    a = static_cast<T&&>(b);      // Equivalent to kstd::move(b)
    b = static_cast<T&&>(temp);   // Equivalent to kstd::move(temp)
}

// Add other common algorithms like kstd::fill, kstd::copy if needed.

} // namespace kstd

#endif // KSTD_ALGORITHM_H
