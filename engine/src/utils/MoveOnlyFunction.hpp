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

/**
 * @brief Move-only callable wrapper fallback for platforms lacking std::move_only_function.
 * @tparam Signature Function signature.
 */
template <typename Signature>
class MoveOnlyFunction;

/**
 * @brief Specialization of MoveOnlyFunction for standard function signatures.
 * @tparam R Return type of the invocable.
 * @tparam Args Argument types accepted by the invocable.
 */
template <typename R, typename... Args>
class MoveOnlyFunction<R(Args...)> {
  public:
    /** @brief Default constructor creating an empty function wrapper. */
    MoveOnlyFunction() noexcept = default;

    /** @brief Constructs an empty function wrapper from nullptr. */
    MoveOnlyFunction(std::nullptr_t) noexcept {}

    /** @brief Move constructor transferring ownership of the callable. */
    MoveOnlyFunction(MoveOnlyFunction&&) noexcept = default;

    /** @brief Move assignment operator transferring ownership of the callable. */
    MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;

    /** @brief Deleted copy constructor. */
    MoveOnlyFunction(const MoveOnlyFunction&) = delete;

    /** @brief Deleted copy assignment operator. */
    MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;

    /** @brief Defaulted destructor. */
    ~MoveOnlyFunction() = default;

    /**
     * @brief Constructs a function wrapper from a compatible invocable object.
     * @tparam F Callable type.
     * @param f Callable instance to store.
     */
    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, MoveOnlyFunction> &&
                                                      std::is_invocable_r_v<R, std::decay_t<F>&, Args...>>>
    MoveOnlyFunction(F&& f) : _impl(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(f))) {}

    /**
     * @brief Clears the stored callable, resetting the wrapper to empty.
     * @return Reference to this function object.
     */
    MoveOnlyFunction& operator=(std::nullptr_t) noexcept {
        _impl.reset();
        return *this;
    }

    /**
     * @brief Checks whether the wrapper currently holds a callable.
     * @return True if a target is stored, false otherwise.
     */
    explicit operator bool() const noexcept { return _impl != nullptr; }

    /**
     * @brief Invokes the stored callable with the provided arguments.
     * @param args Arguments forwarded to the callable.
     * @return Result of the invocation.
     */
    R operator()(Args... args) { return _impl->call(std::forward<Args>(args)...); }

  private:
    /** @brief Type-erased abstract base concept for invocables. */
    struct Concept {
        /** @brief Virtual destructor for type erasure cleanup. */
        virtual ~Concept() = default;

        /**
         * @brief Pure virtual call operator.
         * @param args Forwarded arguments.
         * @return Invocation result.
         */
        virtual R call(Args... args) = 0;
    };

    /**
     * @brief Concrete implementation model wrapping a specific callable type F.
     * @tparam F Stored callable type.
     */
    template <typename F>
    struct Model final : Concept {
        /**
         * @brief Constructs model by forwarding the callable.
         * @tparam U Forwarding reference type.
         * @param u Invocable to move or copy into this model.
         */
        template <typename U>
        explicit Model(U&& u) : fn(std::forward<U>(u)) {}

        /**
         * @brief Executes the wrapped callable with the supplied arguments.
         * @param args Forwarded arguments.
         * @return Invocation result.
         */
        R call(Args... args) override { return std::invoke(fn, std::forward<Args>(args)...); }

        /** @brief The stored callable instance. */
        F fn;
    };

    /** @brief Owning pointer to the type-erased callable model. */
    std::unique_ptr<Concept> _impl;
};

}  // namespace ecs

#endif
