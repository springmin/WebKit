/*
 * Copyright (C) 2008-2022 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.cpp>

namespace WTF {

// Returns a convenient bounded value, suitable for functions taking a lowest/highest bound.
using SpecificBounds = std::numeric_limits<int>;
inline constexpr int lowestBound = SpecificBounds::lowest();
inline constexpr int highestBound = SpecificBounds::max();

inline constexpr size_t MB = 1024 * 1024;
inline constexpr size_t KB = 1024;
inline constexpr size_t GiB = 1024 * 1024 * 1024;

// This allows you to both determine if a cast will lose information and to do a saturating cast in the into-losing-information case.
// clang-format off
#define POTENTIALLY_LOSING_CAST(macro, lowerBound, upperBound, sourceValue, destinationType) \
    (static_cast<std::make_unsigned_t<decltype(sourceValue)>>((sourceValue) - (lowerBound)) > \
     static_cast<std::make_unsigned_t<decltype(sourceValue)>>((upperBound) - (lowerBound)) ? \
      ((sourceValue) < (lowerBound) ? macro(lowerBound) : macro(upperBound)) : \
      destinationType(sourceValue))
// clang-format on

#define SATURATE(sourceValue, destinationType) POTENTIALLY_LOSING_CAST(static_cast, destinationType(WTF::lowestBound), destinationType(WTF::highestBound), sourceValue, destinationType)

static constexpr double D_PI = 3.141592653589793;

/*
 * C++'s idea of a reinterpret_cast lacks sufficient cojones.
 */
template<typename TO, typename FROM>
ALWAYS_INLINE constexpr TO bitwise_cast(FROM from)
{
    static_assert(sizeof(TO) == sizeof(FROM), "bitwise_cast size of source and destination must be identical");

    // The cast must also adhere to the rules of reinterpret_cast. This means no cast between integers and pointers and no cast between floats, or float and non-floats, or float and int.
    static_assert(std::is_pointer_v<FROM> == std::is_pointer_v<TO>);
    static_assert(std::is_floating_point_v<FROM> == std::is_floating_point_v<TO>);

    // We also need our implementation to be safe with respect to Effective C++'s item 39:
    // Avoid casts for core language functionality
    return *reinterpret_cast<const TO*>(&from);
}

template<typename TO, typename FROM>
ALWAYS_INLINE constexpr TO binaryMaybeInvertedCast(FROM from)
{
    static_assert(sizeof(TO) == sizeof(FROM), "binaryMaybeInvertedCast size of source and destination must be identical");

    // We want to do a bit flip if source is negative, but not negative for the source's type.
    constexpr FROM doNothing = 0;
    constexpr FROM doInvert = static_cast<FROM>(-1);
    FROM flipIfNeeded;
    if (from < 0)
        flipIfNeeded = doInvert;
    else
        flipIfNeeded = doNothing;

    // We use this mask so that we never actually do any numerical operation on the real data that affects the stored value.
    FROM originalValue = from ^ flipIfNeeded;
    return bitwise_cast<TO>(originalValue) ^ bitwise_cast<TO>(flipIfNeeded);
}


// This is needed by HashFunctions.h to have a compatible StringHasher with base implementation.
inline uint32_t rotateRight(uint32_t x, uint32_t shift)
{
    RELEASE_ASSERT(shift < 32);
    return (x >> shift) | (x << (32 - shift));
}

// Some calls may be inlined or optimized away by the compiler, but
// some compilers may still sound warnings for variables that are
// defined but not used. A call to this function will avoid those
// warnings in cases where variables are only used within asserts.
// This function also prevents compiler warnings for variables
// that are only used in unreachable code (e.g. UNREACHABLE_FOR_PLATFORM()
// parts with variables that cause unused variable warnings).
inline void UNUSED_VARIABLE_DECORATOR(const void*, ...) { }
#define UNUSED_VARIABLE(x, ...) do { if (true) { WTF::UNUSED_VARIABLE_DECORATOR(&x, ##__VA_ARGS__); } } while (0);

template<class T>
ALWAYS_INLINE bool isPointerAligned(T* ptr)
{
    constexpr uintptr_t alignmentMask = std::alignment_of<T>::value - 1;
    return !((reinterpret_cast<uintptr_t>(ptr) & alignmentMask));
}

template<class T>
ALWAYS_INLINE bool is8ByteAligned(T* ptr)
{
    return !(reinterpret_cast<uintptr_t>(ptr) & 7);
}

/*
 * This enum makes it possible to use if clauses to test for the availability of
 * compile-time features from specific versions of the WWDR-provided toolchain.
 */
enum class FeatureAvailability {
    BuiltIn,
    AlwaysOn,
    AlwaysOff,
};

#define FEATURE_AVAILABLE_IFF_BUILTIN(featureMacro) constexpr bool isFeatureEnabled() {                        \
    if constexpr (featureMacro == FeatureAvailability::BuiltIn)                                                \
        return __has_builtin(__builtin_available) && __builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, *); \
    if constexpr (featureMacro == FeatureAvailability::AlwaysOn)                                               \
        return true;                                                                                           \
    if constexpr (featureMacro == FeatureAvailability::AlwaysOff)                                              \
        return false;                                                                                          \
    __builtin_unreachable();                                                                                   \
    return false;                                                                                              \
}                                                                                                              \
static_assert(isFeatureEnabled() || !isFeatureEnabled(), "To allow inlining of the function");                 \
return isFeatureEnabled();

// The following macros are included but no longer used, as they are being migrated to C++14 features:
#define CONSTEXPR constexpr
#define HAS_CONSTEXPR_STRLEN 1
#ifndef NODISCARD
#define NODISCARD [[nodiscard]]
#endif // !NODISCARD

#ifndef NO_RETURN_DUE_TO_EXIT
#define NO_RETURN_DUE_TO_EXIT WTF_NO_RETURN_DUE_TO_CRASH
#endif

#ifndef INLINE_DECLARATION
#ifdef COMPILER(CLANG)
#define INLINE_DECLARATION __attribute__((always_inline))
#else
#define INLINE_DECLARATION inline
#endif
#endif

template<typename T, size_t N>
constexpr size_t computeArraySize(T (&)[N]) { return N; }

// Define an array with initialized values. If N is smaller or equal to arraySize,
// the remaining slots will be 0, false, or nullptrs.
// Use as: filledArray<TYPE, N>({entries});
template<typename T, size_t arraySize>
class filledArray final : public std::array<T, arraySize> {
public:
    template<typename... Elements>
    constexpr filledArray(std::initializer_list<T> list)
    {
        auto iter = list.begin();
        for (size_t i = 0; i < arraySize; i++) {
            if (i < list.size())
                this->at(i) = *iter++;
            else
                this->at(i) = T { };
        }
    }
};

// OBJECT_OFFSETOF: Like the C++ offsetof macro, but you can use it with classes.
// The magic number 0x4000 is insignificant. We use it to avoid using NULL, which
// would cause compiler problems, especially in cases where the compiler needs to
// find out if a constant is big enough to overflow a register.
#define OBJECT_OFFSETOF(class, field) (reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<class*>(0x4000)->field)) - 0x4000)

template<typename T>
ALWAYS_INLINE constexpr int argPositionFromBit(T data)
{
    return std::numeric_limits<T>::digits - 1 - __builtin_clz(data);
}

// Returns false for 0.
template<typename T> 
ALWAYS_INLINE constexpr bool isPowerOfTwo(T data)
{
#if !defined(NDEBUG)
    static_assert(std::is_integral<T>::value, "isPowerOfTwo only makes sense with integers");
#endif
    static_assert(std::is_unsigned<T>::value, "isPowerOfTwo only makes sense for unsigned integers");
    return data && !(data & (data - 1));
}

template<typename T>
ALWAYS_INLINE constexpr bool hasOneBitSet(T data)
{
    return isPowerOfTwo(data);
}

// DEFINE_STATIC_LOCAL is used to declare a static variable in the scope of a C++ function (or method).
// The advantage of using this macro instead of a simple static declaration is that the static variable
// is initialized lazily upon first use (unlike a direct static declaration which would be initialized
// at ctor time). But note there's no guarantee that it works when a static variable is declared in
// a function being inlined into multiple compilation units. Thus, for a template class's member functions or
// static inlined methods or functions of WTF, it would be safer to use WTF::LazyNeverDestroyed<T> to avoid
// multiple initializations of the same variable.
#ifndef DEFINE_STATIC_LOCAL
#define DEFINE_STATIC_LOCAL(type, name, arguments) \
    static LazyNeverDestroyed<type> name;          \
    static std::once_flag name##OnceFlag;          \
    std::call_once(name##OnceFlag, [&] {           \
        name.construct arguments;                  \
    });                                            \
    type& name = name.get()
#endif // ifndef DEFINE_STATIC_LOCAL

template<typename T>
ALWAYS_INLINE constexpr unsigned getLSBIndex(T value)
{
#if !defined(NDEBUG)
    static_assert(std::is_integral<T>::value, "getLSBIndex only makes sense with integers");
#endif
    static_assert(std::is_unsigned<T>::value, "getLSBIndex only makes sense for unsigned integers");
    return value ? __builtin_ctz(value) : 0;
}

// From http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline unsigned roundUpToPowerOfTwo(unsigned val)
{
    --val;
    val |= (val >> 1);
    val |= (val >> 2);
    val |= (val >> 4);
    val |= (val >> 8);
    val |= (val >> 16);
    return val + 1;
}

// Returns a / b, rounding up.
template<typename T>
ALWAYS_INLINE constexpr T divRoundUp(T a, T b)
{
    return (a + (b - 1)) / b;
}

template<typename T>
ALWAYS_INLINE constexpr T ceilIntegerDivision(T x, T y)
{
    return divRoundUp(x, y);
}

// Returns the number of 2^divisor chunks a number is into. In other words, floor(num / 2^divisor).
template <typename T>
ALWAYS_INLINE constexpr T rightShift(T value, size_t divisor)
{
    return value >> divisor;
}

template<typename T>
ALWAYS_INLINE constexpr T roundUpToMultipleOf(T x, T y)
{
    ASSERT(y);
    return divRoundUp(x, y) * y;
}

template<typename T>
ALWAYS_INLINE constexpr T roundDownToMultipleOf(T x, T y)
{
    ASSERT(y);
    return (x / y) * y;
}

// A specialization allowing to use it for non power of two numbers.
template<typename T>
ALWAYS_INLINE constexpr T roundUpToMultipleOfNonPowerOfTwo(T value, T divisor)
{
    return ((value + divisor - 1) / divisor) * divisor;
}

// Bounds checking against max integers.
template<typename T>
inline bool safeAdd(T& result, T a, T b)
{
    if (std::is_signed<T>::value) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b)
            return false;
        if (b < 0 && a < std::numeric_limits<T>::min() - b)
            return false;
    } else {
        if (a > std::numeric_limits<T>::max() - b)
            return false;
    }

    result = a + b;
    return true;
}

template<typename T, typename S>
inline bool safeCast(S value, T& result)
{
    bool castSafe = value >= std::numeric_limits<T>::lowest() && value <= std::numeric_limits<T>::max();
    result = static_cast<T>(value);
    return castSafe;
}

template <typename T> 
inline T safeCast(int64_t x)
{
    ASSERT(static_cast<int64_t>(static_cast<T>(x)) == x);
    return static_cast<T>(x);
}

template <typename T>
inline T convertFromUnscopedEnum(T value)
{
    return value;
}

template <typename T>
inline auto convertFromUnscopedEnum(std::remove_reference_t<T> value)
{
    using ValueType = std::remove_reference_t<T>;
    return static_cast<std::conditional_t<std::is_unsigned<ValueType>::value,
        std::make_unsigned_t<std::underlying_type_t<ValueType>>,
        std::make_signed_t<std::underlying_type_t<ValueType>>>>(value);
}

template <typename T>
inline auto toTwosComplement(T value)
{
    using ValueType = std::remove_reference_t<T>;
    return static_cast<std::conditional_t<std::is_unsigned<ValueType>::value,
        std::make_signed_t<std::underlying_type_t<ValueType>>,
        std::make_unsigned_t<std::underlying_type_t<ValueType>>>>(value);
}

template <typename T>
inline auto unsignedCast(T value)
{
    using ValueType = std::remove_reference_t<T>;
    return static_cast<std::conditional_t<std::is_unsigned<ValueType>::value,
        ValueType,
        std::make_unsigned_t<std::underlying_type_t<ValueType>>>>(value);
}

template <size_t boundary, typename T>
inline bool isInBounds(T&& value)
{
    return !(value & ~(boundary - 1));
}

template <typename T, typename P>
inline T valueOrDefault(const Optional<T>& optional, P&& prependValue)
{
    return optional ? optional.value() : prependValue;
}

template <typename T, typename P>
inline T valueOrCompute(const Optional<T>& optional, P callback)
{
    return optional ? optional.value() : callback();
}

template <typename T, typename P>
inline bool mergeDeduplicatedSorted(T& a, P&& b)
{
    typename T::iterator aIterator = a.begin();
    typename T::iterator aEnd = a.end();
    bool changed = false;

    for (auto& value : b) {
        while (aIterator != aEnd && *aIterator < value)
            ++aIterator;
        
        if (aIterator == aEnd || *aIterator != value) {
            a.insert(aIterator, value);
            changed = true;
        }
    }
    
    return changed;
}

template <class Vector, class T, class Compare = std::less<>>
std::optional<size_t> tryBinarySearch(const Vector& array, const T& value, size_t size, Compare compareFunction = Compare())
{
    size_t offset = 0;
    while (size > 1) {
        size_t pos = offset + size / 2;
        const auto val = array[pos];
        if (compareFunction(val, value)) {
            offset = pos;
            size -= size / 2;
        } else if (compareFunction(value, val))
            size /= 2;
        else
            return pos;
    }

    if (size == 1 && offset < array.size() && !compareFunction(array[offset], value) && !compareFunction(value, array[offset]))
        return offset;

    return {};
}

template <typename T, typename U>
constexpr T lazyInitialize(T& storage, U function)
{
    U functionToCall = function;
    RELEASE_ASSERT(functionToCall);
    T result = functionToCall();
    RELEASE_ASSERT(result);
    storage = result;
    return result;
}

template <typename T, typename U>
inline auto skip(T&& range, U numberToSkip)
{
    auto begin = std::begin(range);
    auto end = std::end(range);

    if (std::distance(begin, end) < numberToSkip)
        return end;
    
    advance(begin, numberToSkip);
    return begin;
}

template <class Span>
bool spansOverlap(const Span& spanA, const Span& spanB)
{
    return spanA.data() + spanA.size() > spanB.data() && spanB.data() + spanB.size() > spanA.data();
}

template <class ElementType, size_t N>
std::span<ElementType, N> singleElementSpan(ElementType (&element)[N])
{
    return { &element, 1 };
}

template <class ElementType>
std::span<ElementType, 1> singleElementSpan(ElementType& element)
{
    return { &element, 1 };
}

template <class To, class From>
ALWAYS_INLINE To spanReinterpretCast(const From& span)
{
    using ToElement = typename To::element_type;
    using FromElement = typename std::remove_reference_t<From>::element_type;

    static_assert(sizeof(ToElement) == sizeof(FromElement));
    static_assert(alignof(ToElement) == alignof(FromElement));
    
    auto* p = reinterpret_cast<ToElement*>(const_cast<FromElement*>(span.data()));
    auto size = span.size();
    
    return { p, size };
}

template <class To, class From>
ALWAYS_INLINE To spanConstCast(const From& span)
{
    using ToElement = typename To::element_type;
    using FromElement = typename std::remove_reference_t<From>::element_type;

    static_assert(std::is_same_v<std::remove_const_t<ToElement>, std::remove_const_t<FromElement>>);
    
    auto* p = const_cast<ToElement*>(span.data());
    auto size = span.size();
    
    return { p, size };
}

template <class SpanA, class SpanB>
bool spanHasPrefix(const SpanA& span, const SpanB& prefix)
{
    if (span.size() < prefix.size())
        return false;
    
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (span[i] != prefix[i])
            return false;
    }
    
    return true;
}

template <class SpanA, class SpanB>
bool spanHasSuffix(const SpanA& span, const SpanB& suffix)
{
    if (span.size() < suffix.size())
        return false;
    
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (span[span.size() - suffix.size() + i] != suffix[i])
            return false;
    }
    
    return true;
}

template <class To, class From>
ALWAYS_INLINE To reinterpretCastSpanStartTo(const From& span)
{
    static_assert(std::is_pointer_v<To>);
    
    using ToTarget = std::remove_pointer_t<To>;
    using FromElement = typename std::remove_reference_t<From>::element_type;
    
    // Requires that the pointer can be dereferenced after reinterpret_cast.
    // So we statically verify that To is aligned at less or equal to From.
    static_assert(alignof(ToTarget) <= alignof(FromElement));
    
    return reinterpret_cast<To>(const_cast<FromElement*>(span.data()));
}

// memset, memcpy, and memmove work for span, with their traditional semantics.
template <class Span> void memsetSpan(Span& span, int c)
{
    memset(span.data(), c, span.size_bytes());
}

template <class Span> void secureMemsetSpan(Span& span, int c)
{
    secureZeroMemory(span.data(), span.size_bytes());
}

template <class Target, class Source>
void memcpySpan(Target& target, const Source& source)
{
    using TargetElement = typename std::remove_reference_t<Target>::element_type;
    using SourceElement = typename std::remove_reference_t<Source>::element_type;
    static_assert(sizeof(TargetElement) == sizeof(SourceElement));
    ASSERT(target.size() >= source.size());
    memcpy(target.data(), source.data(), source.size_bytes());
}

template <class Span, class Element>
ALWAYS_INLINE std::span<Element, std::dynamic_extent> unsafeMakeSpan(const Span& span, Element*)
{
    using SpanElement = typename std::remove_reference_t<Span>::element_type;
    
    // Requires that the pointer can be dereferenced after reinterpret_cast.
    // So we statically verify that To is aligned at less or equal to From.
    static_assert(alignof(Element) <= alignof(SpanElement));
    
    auto* p = reinterpret_cast<Element*>(const_cast<SpanElement*>(span.data()));
    auto size = span.size_bytes() / sizeof(Element);
    
    return { p, size };
}

template <class Target, class Source>
void memmoveSpan(Target& target, const Source& source)
{
    using TargetElement = typename std::remove_reference_t<Target>::element_type;
    using SourceElement = typename std::remove_reference_t<Source>::element_type;
    static_assert(sizeof(TargetElement) == sizeof(SourceElement));
    ASSERT(target.size() >= source.size());
    memmove(target.data(), source.data(), source.size_bytes());
}

// Helper for creating a lambda that should be evaluated if a given class is available, e.g.
// if constexpr (isStatelessLambda(&T::func)) // ...
template <typename F>
constexpr bool isStatelessLambda(F)
{
    using ReturnType = decltype(std::declval<F>()());
    using FunctorType = std::remove_pointer_t<F>;
    return std::is_same<ReturnType, FunctorType>::value;
}

// isCompilationThread() will return true when called from any thread where we
// can reasonably expect that no useful work can be done. Currently, this means
// that this returns true when called from the WebCore Compilation thread.
INLINE_DECLARATION bool isCompilationThread();

// Support fusing 32-bit integers into and out of 64-bit.
template<typename IntegralType>
ALWAYS_INLINE std::enable_if_t<sizeof(IntegralType) == 8, IntegralType> fuse(uint32_t high, uint32_t low)
{
    RELEASE_ASSERT(!(high & 0x80000000));
    return (static_cast<IntegralType>(high) << 32) | low;
}

template<typename IntegralType>
ALWAYS_INLINE std::enable_if_t<sizeof(IntegralType) == 8, uint32_t> high(IntegralType value)
{
    return static_cast<uint32_t>(value >> 32);
}

template<typename IntegralType>
ALWAYS_INLINE std::enable_if_t<sizeof(IntegralType) == 8, uint32_t> low(IntegralType value)
{
    return static_cast<uint32_t>(value);
}

template<typename T, typename U>
inline std::unique_ptr<T> makeUnique(U&& u)
{
    return std::unique_ptr<T>(new T(std::forward<U>(u)));
}

// For the WTF_MAKE_FAST_ALLOCATED friendly allocator.
template<typename T, typename... Args>
inline std::unique_ptr<T> makeUniqueWithoutFastMallocCheck(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// For the WTF_MAKE_NONCOPYABLE and using RefCounted.
template<typename T, typename... Args>
inline std::unique_ptr<T> makeUniqueWithoutRefCountedCheck(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// We want to think of functions as pointers so we can do if (function).
// But we can't specialization function pointers.
// So we need to switch to a class that holds a function pointer.
template<typename FunctionType>
class FunctionWrapper {
public:
    constexpr FunctionWrapper()
        : m_function(nullptr)
    {
    }
    
    constexpr FunctionWrapper(FunctionType function)
        : m_function(function)
    {
    }
    
    constexpr operator bool() const
    {
        return !!m_function;
    }
    
    // We use these instead of Function() so that we don't accidentally
    // call an unexpected function.
    
    template<typename... ArgumentTypes>
    typename std::invoke_result<FunctionType, ArgumentTypes...>::type operator()(ArgumentTypes... arguments) const
    {
        RELEASE_ASSERT(m_function);
        return m_function(std::forward<ArgumentTypes>(arguments)...);
    }
    
    // We can't define these inside the class since we can't place
    // constexpr objects inside the class.
    constexpr bool operator==(const FunctionWrapper& other) const
    {
        return m_function == other.m_function;
    }
    
    constexpr bool operator!=(const FunctionWrapper& other) const
    {
        return !(*this == other);
    }
    
    friend constexpr bool operator==(std::nullptr_t, const FunctionWrapper& wrapper)
    {
        return !wrapper;
    }
    
    friend constexpr bool operator!=(std::nullptr_t, const FunctionWrapper& wrapper)
    {
        return !!wrapper;
    }
    
    friend constexpr bool operator==(const FunctionWrapper& wrapper, std::nullptr_t)
    {
        return !wrapper;
    }
    
    friend constexpr bool operator!=(const FunctionWrapper& wrapper, std::nullptr_t)
    {
        return !!wrapper;
    }
    
private:
    FunctionType m_function;
};

} // namespace WTF

using WTF::argPositionFromBit;
using WTF::binaryMaybeInvertedCast;
using WTF::bitwise_cast;
using WTF::ceilIntegerDivision;
using WTF::convertFromUnscopedEnum;
using WTF::divRoundUp;
using WTF::filledArray;
using WTF::fuse;
using WTF::hasOneBitSet;
using WTF::high;
using WTF::is8ByteAligned;
using WTF::isCompilationThread;
using WTF::isPointerAligned;
using WTF::isStatelessLambda;
using WTF::KB;
using WTF::lazyInitialize;
using WTF::makeUnique;
using WTF::makeUniqueWithoutFastMallocCheck;
using WTF::makeUniqueWithoutRefCountedCheck;
using WTF::MB;
using WTF::memcpySpan;
using WTF::memmoveSpan;
using WTF::memsetSpan;
using WTF::mergeDeduplicatedSorted;
using WTF::reinterpretCastSpanStartTo;
using WTF::secureMemsetSpan;
using WTF::singleElementSpan;
using WTF::skip;
using WTF::spanConstCast;
using WTF::spanHasPrefix;
using WTF::spanHasSuffix;
using WTF::spanReinterpretCast;
using WTF::spansOverlap;
using WTF::stringToDouble;
using WTF::toTwosComplement;
using WTF::tryBinarySearch;
using WTF::unsafeMakeSpan;
using WTF::unsignedCast;
using WTF::valueOrCompute;
using WTF::valueOrDefault;