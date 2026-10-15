#pragma once
#include <version>  // IWYU pragma: keep

// Apple libc++ does not ship std::move_only_function yet, so fall back to a minimal equivalent.
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L

#include <functional>

namespace ecs {

template <typename Signature>
using MoveOnlyFunction = std::move_only_function<Signature>;

}  // namespace ecs

#else

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace ecs {

template <typename Signature>
class MoveOnlyFunction;

template <typename R, typename... Args>
class MoveOnlyFunction<R(Args...)> {
  public:
    MoveOnlyFunction() noexcept = default;
    MoveOnlyFunction(std::nullptr_t) noexcept {}
    MoveOnlyFunction(MoveOnlyFunction&&) noexcept = default;
    MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;
    MoveOnlyFunction(const MoveOnlyFunction&) = delete;
    MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;
    ~MoveOnlyFunction() = default;

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, MoveOnlyFunction> &&
                                                      std::is_invocable_r_v<R, std::decay_t<F>&, Args...>>>
    MoveOnlyFunction(F&& f) : _impl(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(f))) {}

    MoveOnlyFunction& operator=(std::nullptr_t) noexcept {
        _impl.reset();
        return *this;
    }

    explicit operator bool() const noexcept { return _impl != nullptr; }

    R operator()(Args... args) { return _impl->call(std::forward<Args>(args)...); }

  private:
    struct Concept {
        virtual ~Concept() = default;
        virtual R call(Args... args) = 0;
    };

    template <typename F>
    struct Model final : Concept {
        template <typename U>
        explicit Model(U&& u) : fn(std::forward<U>(u)) {}

        R call(Args... args) override { return std::invoke(fn, std::forward<Args>(args)...); }

        F fn;
    };

    std::unique_ptr<Concept> _impl;
};

}  // namespace ecs

#endif
