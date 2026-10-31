/**
 * @file PluginGraph.hpp
 * @brief Dependency resolution graph and topological sorter for engine plugins.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "core/IPlugin.hpp"

namespace engine {

/**
 * @brief Exception thrown when plugin dependency resolution fails (missing dependency or cycle).
 */
class PluginDependencyException : public std::runtime_error {
  public:
    /**
     * @brief Constructs exception with diagnostic message.
     * @param message Description of the dependency error.
     */
    explicit PluginDependencyException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief Manages registered plugins, resolves dependency constraints, and executes topological sorting.
 */
class PluginGraph {
  public:
    /** @brief Constructs an empty plugin graph. */
    PluginGraph() = default;

    /** @brief Destructor. */
    ~PluginGraph() = default;

    /** @brief Deleted copy constructor. */
    PluginGraph(const PluginGraph&) = delete;

    /** @brief Deleted copy assignment operator. */
    PluginGraph& operator=(const PluginGraph&) = delete;

    /** @brief Defaulted move constructor. */
    PluginGraph(PluginGraph&&) noexcept = default;

    /** @brief Defaulted move assignment operator. */
    PluginGraph& operator=(PluginGraph&&) noexcept = default;

    /**
     * @brief Adds a plugin instance to the graph.
     * @param plugin Unique pointer owning the plugin.
     * @param typeId Type index of the plugin.
     * @return Reference to the added plugin.
     * @throws PluginDependencyException If a plugin of the same type is already added.
     */
    IPlugin& addPlugin(std::unique_ptr<IPlugin> plugin, std::type_index typeId);

    /**
     * @brief Checks if a plugin of the given type has been added.
     * @param typeId Type index to check.
     * @return True if present.
     */
    [[nodiscard]] bool hasPlugin(std::type_index typeId) const noexcept;

    /**
     * @brief Retrieves a plugin by its type index.
     * @param typeId Type index to retrieve.
     * @return Reference to the plugin.
     * @throws PluginDependencyException If the plugin is not found.
     */
    [[nodiscard]] IPlugin& getPlugin(std::type_index typeId) const;

    /**
     * @brief Resolves all plugin dependencies and performs topological sorting.
     *
     * Calls `build()` on all registered plugins, validates required dependencies,
     * and computes the sorted execution order using Kahn's algorithm.
     *
     * @throws PluginDependencyException If a required dependency is missing or a cycle is detected.
     */
    void resolve();

    /**
     * @brief Checks whether the graph has been successfully resolved.
     * @return True if resolved.
     */
    [[nodiscard]] bool isResolved() const noexcept { return _resolved; }

    /**
     * @brief Returns registered plugins in topological execution order.
     * @return Ordered list of non-owning plugin pointers.
     */
    [[nodiscard]] const std::vector<IPlugin*>& executionOrder() const;

    /**
     * @brief Returns registered plugins in reverse topological order (for shutdown).
     * @return Reverse ordered list of non-owning plugin pointers.
     */
    [[nodiscard]] const std::vector<IPlugin*>& shutdownOrder() const;

    /**
     * @brief Clears all registered plugins and resets graph state.
     */
    void clear();

  private:
    /**
     * @brief Internal node representation for plugin dependency graph resolution.
     */
    struct PluginNode {
        /** @brief Type index identifier of the plugin. */
        std::type_index typeId = std::type_index(typeid(void));
        /** @brief Owned unique pointer to the plugin instance. */
        std::unique_ptr<IPlugin> plugin = nullptr;
        /** @brief List of mandatory dependency type indices. */
        std::vector<std::type_index> requiredDeps;
        /** @brief List of optional dependency type indices. */
        std::vector<std::type_index> optionalDeps;
    };

    /**
     * @brief Constructs the directed adjacency list and in-degree counts from plugin requirements.
     * @param adj Adjacency list mapping node index to dependent node indices.
     * @param inDegree Array tracking incoming edge count for each node.
     * @param typeToIndex Map translating type_index to internal vector indices.
     */
    void buildAdjacency(std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree,
                        const std::unordered_map<std::type_index, std::size_t>& typeToIndex);

    /**
     * @brief Performs Kahn's topological sort algorithm to order plugins sequentially.
     * @param adj Adjacency list of the graph.
     * @param inDegree In-degree counts per node.
     * @param sortedIndices Output vector populated with topologically sorted node indices.
     */
    void topologicalSort(const std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree,
                         std::vector<std::size_t>& sortedIndices) const;

    /**
     * @brief Maps each registered plugin's typeId to its index in the nodes vector.
     * @return Hash map of type index to node index.
     */
    [[nodiscard]] std::unordered_map<std::type_index, std::size_t> buildTypeIndexMap() const;

    /**
     * @brief Populates execution and reverse shutdown orders from sorted node indices.
     * @param sortedIndices Topologically sorted vector of node indices.
     */
    void populateExecutionOrders(const std::vector<std::size_t>& sortedIndices);

    /**
     * @brief Formats cyclic dependency details and throws a PluginDependencyException.
     * @param inDegree Array of in-degree counts where remaining non-zero counts indicate cycles.
     */
    [[noreturn]] void throwCyclicDependencyError(const std::vector<std::size_t>& inDegree) const;

    /** @brief Registered plugin nodes in insertion order. */
    std::vector<PluginNode> _nodes;
    /** @brief Resolved execution sequence for init, update, and render. */
    std::vector<IPlugin*> _executionOrder;
    /** @brief Reversed execution sequence for safe plugin shutdown. */
    std::vector<IPlugin*> _shutdownOrder;
    /** @brief Flag indicating whether the graph has been topologically sorted. */
    bool _resolved = false;
};

}  // namespace engine
