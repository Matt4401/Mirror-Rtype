#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"

namespace {

class ITestGreetingService {
  public:
    virtual ~ITestGreetingService() = default;
    [[nodiscard]] virtual std::string greet(const std::string& name) const = 0;
};

class GreetingService final : public ITestGreetingService {
  public:
    [[nodiscard]] std::string greet(const std::string& name) const override { return "Hello, " + name + "!"; }
};

class ITestCounterService {
  public:
    virtual ~ITestCounterService() = default;
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
    mediator.registerService<ITestGreetingService>(servicePtr);

    EXPECT_TRUE(mediator.hasService<ITestGreetingService>());
    auto retrieved = mediator.getService<ITestGreetingService>();
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->get().greet("World"), "Hello, World!");
    EXPECT_EQ(mediator.service<ITestGreetingService>().greet("R-Type"), "Hello, R-Type!");
}

TEST(Mediator, EmplaceService) {
    engine::Mediator mediator;
    mediator.emplaceService<ITestCounterService, CounterService>(10);

    EXPECT_TRUE(mediator.hasService<ITestCounterService>());
    EXPECT_EQ(mediator.service<ITestCounterService>().value(), 10);
    mediator.service<ITestCounterService>().increment();
    EXPECT_EQ(mediator.service<ITestCounterService>().value(), 11);
}

TEST(Mediator, MissingServiceReturnsErrorOrThrows) {
    engine::Mediator mediator;
    auto result = mediator.getService<ITestGreetingService>();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), engine::MediatorError::ServiceNotFound);

    EXPECT_THROW(static_cast<void>(mediator.service<ITestGreetingService>()), std::runtime_error);
}

TEST(Mediator, RemoveService) {
    engine::Mediator mediator;
    mediator.emplaceService<ITestCounterService, CounterService>(0);
    EXPECT_TRUE(mediator.hasService<ITestCounterService>());

    mediator.removeService<ITestCounterService>();
    EXPECT_FALSE(mediator.hasService<ITestCounterService>());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
