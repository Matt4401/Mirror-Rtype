#include <gtest/gtest.h>

#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"

namespace {

class ITestGreetingService {
  public:
    ITestGreetingService() = default;
    virtual ~ITestGreetingService() = default;
    ITestGreetingService(const ITestGreetingService&) = default;
    ITestGreetingService& operator=(const ITestGreetingService&) = default;
    ITestGreetingService(ITestGreetingService&&) noexcept = default;
    ITestGreetingService& operator=(ITestGreetingService&&) noexcept = default;
    [[nodiscard]] virtual std::string greet(const std::string& name) const = 0;
};

class GreetingService final : public ITestGreetingService {
  public:
    [[nodiscard]] std::string greet(const std::string& name) const override { return "Hello, " + name + "!"; }
};

class ITestCounterService {
  public:
    ITestCounterService() = default;
    virtual ~ITestCounterService() = default;
    ITestCounterService(const ITestCounterService&) = default;
    ITestCounterService& operator=(const ITestCounterService&) = default;
    ITestCounterService(ITestCounterService&&) noexcept = default;
    ITestCounterService& operator=(ITestCounterService&&) noexcept = default;
    virtual void increment() = 0;
    [[nodiscard]] virtual int value() const = 0;
};

class CounterService final : public ITestCounterService {
  public:
    explicit CounterService(int initial) : _value(initial) {}
    void increment() override { ++_value; }
    [[nodiscard]] int value() const override { return _value; }

  private:
    int _value;
};

}  // namespace

TEST(Mediator, RegisterAndRetrieveService) {
    engine::Mediator mediator;
    EXPECT_FALSE(mediator.hasService<ITestGreetingService>());

    auto servicePtr = std::make_shared<GreetingService>();
    [[maybe_unused]] const auto regRes = mediator.registerService<ITestGreetingService>(servicePtr);

    EXPECT_TRUE(mediator.hasService<ITestGreetingService>());
    auto retrieved = mediator.getService<ITestGreetingService>();
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->get().greet("World"), "Hello, World!");
    EXPECT_EQ(mediator.service<ITestGreetingService>().greet("R-Type"), "Hello, R-Type!");
}

TEST(Mediator, EmplaceService) {
    engine::Mediator mediator;
    [[maybe_unused]] const auto empRes = mediator.emplaceService<ITestCounterService, CounterService>(10);

    EXPECT_TRUE(mediator.hasService<ITestCounterService>());
    EXPECT_EQ(mediator.service<ITestCounterService>().value(), 10);
    mediator.service<ITestCounterService>().increment();
    EXPECT_EQ(mediator.service<ITestCounterService>().value(), 11);
}

TEST(Mediator, MissingServiceReturnsErrorOrThrows) {
    const engine::Mediator mediator;
    auto result = mediator.getService<ITestGreetingService>();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), engine::MediatorError::ServiceNotFound);

    EXPECT_THROW(static_cast<void>(mediator.service<ITestGreetingService>()), engine::MediatorError);
    EXPECT_THROW(static_cast<void>(mediator.service<ITestGreetingService>()), std::exception);
}

TEST(Mediator, DuplicateRegistrationRejectsWithAlreadyRegistered) {
    engine::Mediator mediator;
    auto servicePtr1 = std::make_shared<GreetingService>();
    auto servicePtr2 = std::make_shared<GreetingService>();

    auto result1 = mediator.registerService<ITestGreetingService>(servicePtr1);
    EXPECT_TRUE(result1.has_value());

    auto result2 = mediator.registerService<ITestGreetingService>(servicePtr2);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error(), engine::MediatorError::AlreadyRegistered);

    // Stored service must not be replaced
    EXPECT_EQ(mediator.service<ITestGreetingService>().greet("Alice"), "Hello, Alice!");
}

TEST(Mediator, DuplicateEmplaceRejectsWithAlreadyRegistered) {
    engine::Mediator mediator;
    auto result1 = mediator.emplaceService<ITestCounterService, CounterService>(10);
    ASSERT_TRUE(result1.has_value());

    auto result2 = mediator.emplaceService<ITestCounterService, CounterService>(20);
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error(), engine::MediatorError::AlreadyRegistered);

    // Stored service must not be replaced
    EXPECT_EQ(mediator.service<ITestCounterService>().value(), 10);
}

TEST(Mediator, RemoveService) {
    engine::Mediator mediator;
    [[maybe_unused]] const auto empRes = mediator.emplaceService<ITestCounterService, CounterService>(0);
    EXPECT_TRUE(mediator.hasService<ITestCounterService>());

    mediator.removeService<ITestCounterService>();
    EXPECT_FALSE(mediator.hasService<ITestCounterService>());
}

namespace {

/** @brief Service appending an identifier to a shared log when destroyed. */
template <int Id>
struct DestructionRecorder {
    explicit DestructionRecorder(std::vector<int>& log) : log(&log) {}
    DestructionRecorder(const DestructionRecorder&) = delete;
    DestructionRecorder& operator=(const DestructionRecorder&) = delete;
    DestructionRecorder(DestructionRecorder&&) = delete;
    DestructionRecorder& operator=(DestructionRecorder&&) = delete;
    ~DestructionRecorder() { log->push_back(Id); }
    std::vector<int>* log;
};

}  // namespace

TEST(Mediator, ReleasesServicesInReverseRegistrationOrder) {
    std::vector<int> log;
    {
        engine::Mediator mediator;
        ASSERT_TRUE(mediator.registerService(std::make_shared<DestructionRecorder<1>>(log)).has_value());
        ASSERT_TRUE((mediator.emplaceService<DestructionRecorder<2>, DestructionRecorder<2>>(log).has_value()));
        ASSERT_TRUE(mediator.registerService(std::make_shared<DestructionRecorder<3>>(log)).has_value());
        ASSERT_TRUE(mediator.registerService(std::make_shared<DestructionRecorder<4>>(log)).has_value());

        mediator.removeService<DestructionRecorder<3>>();
        EXPECT_EQ(log, std::vector<int>{3});
    }
    EXPECT_EQ(log, (std::vector<int>{3, 4, 2, 1}));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
