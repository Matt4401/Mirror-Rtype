#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/PluginGraph.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/plugins/TimePlugin.hpp"
#include "ecs/time/Time.hpp"

namespace {

// =============================================================================
// Test Services for Mediator
// =============================================================================

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

// =============================================================================
// Test Plugins for Graph and Engine
// =============================================================================

class PluginA final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "PluginA"; }
    void build(engine::PluginBuilder& /*builder*/) override {}
};

class PluginB final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "PluginB"; }
    void build(engine::PluginBuilder& builder) override { builder.require<PluginA>(); }
};

class PluginC final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "PluginC"; }
    void build(engine::PluginBuilder& builder) override { builder.require<PluginB>(); }
};

class CycleNode1 final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "CycleNode1"; }
    void build(engine::PluginBuilder& builder) override;
};

class CycleNode2 final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "CycleNode2"; }
    void build(engine::PluginBuilder& builder) override { builder.require<CycleNode1>(); }
};

void CycleNode1::build(engine::PluginBuilder& builder) { builder.require<CycleNode2>(); }

class OptionalConsumer final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "OptionalConsumer"; }
    void build(engine::PluginBuilder& builder) override { builder.optionally<PluginA>(); }
};

class LifecycleRecorderPlugin final : public engine::IPlugin {
  public:
    explicit LifecycleRecorderPlugin(std::vector<std::string>& log) : _log(log) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "LifecycleRecorderPlugin"; }

    void build(engine::PluginBuilder& /*builder*/) override { _log.push_back("build"); }

    void init(engine::Engine& /*engine*/) override { _log.push_back("init"); }

    void fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) override {
        _log.push_back("fixedUpdate");
    }

    void update(engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) override { _log.push_back("update"); }

    void render(engine::Engine& /*engine*/) override { _log.push_back("render"); }

    void shutdown(engine::Engine& /*engine*/) override { _log.push_back("shutdown"); }

  private:
    std::vector<std::string>& _log;
};

}  // namespace

// =============================================================================
// Tests : Mediator
// =============================================================================

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

// =============================================================================
// Tests : PluginGraph (DAG & Topological Sorting)
// =============================================================================

TEST(PluginGraph, ResolvesTopologicalOrder) {
    engine::PluginGraph graph;

    // Add in reverse order: C requires B, B requires A, A has no deps
    graph.addPlugin(std::make_unique<PluginC>(), std::type_index(typeid(PluginC)));
    graph.addPlugin(std::make_unique<PluginA>(), std::type_index(typeid(PluginA)));
    graph.addPlugin(std::make_unique<PluginB>(), std::type_index(typeid(PluginB)));

    EXPECT_FALSE(graph.isResolved());
    graph.resolve();
    EXPECT_TRUE(graph.isResolved());

    const auto& order = graph.executionOrder();
    ASSERT_EQ(order.size(), 3U);
    EXPECT_EQ(order[0]->name(), "PluginA");
    EXPECT_EQ(order[1]->name(), "PluginB");
    EXPECT_EQ(order[2]->name(), "PluginC");

    const auto& shutdown = graph.shutdownOrder();
    ASSERT_EQ(shutdown.size(), 3U);
    EXPECT_EQ(shutdown[0]->name(), "PluginC");
    EXPECT_EQ(shutdown[1]->name(), "PluginB");
    EXPECT_EQ(shutdown[2]->name(), "PluginA");
}

TEST(PluginGraph, ThrowsOnMissingDependency) {
    engine::PluginGraph graph;
    // B requires A, but A is not added
    graph.addPlugin(std::make_unique<PluginB>(), std::type_index(typeid(PluginB)));

    EXPECT_THROW(graph.resolve(), engine::PluginDependencyException);
}

TEST(PluginGraph, ThrowsOnCycle) {
    engine::PluginGraph graph;
    graph.addPlugin(std::make_unique<CycleNode1>(), std::type_index(typeid(CycleNode1)));
    graph.addPlugin(std::make_unique<CycleNode2>(), std::type_index(typeid(CycleNode2)));

    EXPECT_THROW(graph.resolve(), engine::PluginDependencyException);
}

TEST(PluginGraph, HandlesOptionalDependencyWhenPresent) {
    engine::PluginGraph graph;
    graph.addPlugin(std::make_unique<OptionalConsumer>(), std::type_index(typeid(OptionalConsumer)));
    graph.addPlugin(std::make_unique<PluginA>(), std::type_index(typeid(PluginA)));

    graph.resolve();
    const auto& order = graph.executionOrder();
    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0]->name(), "PluginA");
    EXPECT_EQ(order[1]->name(), "OptionalConsumer");
}

TEST(PluginGraph, HandlesOptionalDependencyWhenAbsent) {
    engine::PluginGraph graph;
    graph.addPlugin(std::make_unique<OptionalConsumer>(), std::type_index(typeid(OptionalConsumer)));

    EXPECT_NO_THROW(graph.resolve());
    const auto& order = graph.executionOrder();
    ASSERT_EQ(order.size(), 1U);
    EXPECT_EQ(order[0]->name(), "OptionalConsumer");
}

// =============================================================================
// Tests : Engine Lifecycle & Integration
// =============================================================================

TEST(Engine, AutomaticallyLoadsImplicitPlugins) {
    engine::Engine engineInstance;
    EXPECT_FALSE(engineInstance.hasPlugin<engine::CorePlugin>());
    EXPECT_FALSE(engineInstance.hasPlugin<engine::TimePlugin>());

    engineInstance.init();

    EXPECT_TRUE(engineInstance.hasPlugin<engine::CorePlugin>());
    EXPECT_TRUE(engineInstance.hasPlugin<engine::TimePlugin>());
    EXPECT_TRUE(engineInstance.registry().hasResource<ecs::time::Time>());
}

TEST(Engine, ExecutesPhasesInSequence) {
    std::vector<std::string> log;
    {
        engine::Engine engineInstance;
        engineInstance.addPlugin<LifecycleRecorderPlugin>(log);

        EXPECT_TRUE(log.empty());

        engineInstance.step(0.016F);

        // Expect: build, init, fixedUpdate, update, render
        ASSERT_GE(log.size(), 5U);
        EXPECT_EQ(log[0], "build");
        EXPECT_EQ(log[1], "init");
        EXPECT_EQ(log[2], "fixedUpdate");
        EXPECT_EQ(log[3], "update");
        EXPECT_EQ(log[4], "render");

        engineInstance.shutdown();
        EXPECT_EQ(log.back(), "shutdown");
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
