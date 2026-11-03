#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/PluginGraph.hpp"

namespace {

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

}  // namespace

TEST(PluginGraph, ResolvesTopologicalOrder) {
    engine::PluginGraph graph;

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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
