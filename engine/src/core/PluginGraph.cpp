/**
 * @file PluginGraph.cpp
 * @brief Implementation of dependency resolution and topological sorting.
 */

#include "core/PluginGraph.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <queue>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace engine {

namespace {

void addEdge(std::size_t from, std::size_t to, std::vector<std::vector<std::size_t>>& adj,
             std::vector<std::size_t>& inDegree) {
    adj[from].push_back(to);
    inDegree[to]++;
}

void processRequiredDeps(std::size_t nodeIdx, std::string_view pluginName,
                         const std::vector<PluginDependency>& required,
                         const std::unordered_map<std::type_index, std::size_t>& typeToIndex,
                         std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree) {
    for (const auto& dep : required) {
        auto it = typeToIndex.find(dep.typeId);
        if (it == typeToIndex.end()) {
            std::ostringstream oss;
            oss << "Plugin '" << pluginName << "' requires missing dependency '" << dep.name << "'";
            throw PluginDependencyException(oss.str());
        }
        addEdge(it->second, nodeIdx, adj, inDegree);
    }
}

void processOptionalDeps(std::size_t nodeIdx, const std::vector<PluginDependency>& optional,
                         const std::unordered_map<std::type_index, std::size_t>& typeToIndex,
                         std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree) {
    for (const auto& dep : optional) {
        auto it = typeToIndex.find(dep.typeId);
        if (it != typeToIndex.end()) {
            addEdge(it->second, nodeIdx, adj, inDegree);
        }
    }
}

std::queue<std::size_t> findInitialReadyNodes(const std::vector<std::size_t>& inDegree) {
    std::queue<std::size_t> readyQueue;
    for (std::size_t i = 0; i < inDegree.size(); ++i) {
        if (inDegree[i] == 0) {
            readyQueue.push(i);
        }
    }
    return readyQueue;
}

void processKahnQueue(std::queue<std::size_t>& readyQueue, const std::vector<std::vector<std::size_t>>& adj,
                      std::vector<std::size_t>& inDegree, std::vector<std::size_t>& sortedIndices) {
    while (!readyQueue.empty()) {
        const std::size_t u = readyQueue.front();
        readyQueue.pop();
        sortedIndices.push_back(u);

        for (const std::size_t v : adj[u]) {
            inDegree[v]--;
            if (inDegree[v] == 0) {
                readyQueue.push(v);
            }
        }
    }
}

}  // namespace

IPlugin& PluginGraph::addPlugin(std::unique_ptr<IPlugin> plugin, std::type_index typeId) {
    if (!plugin) {
        throw PluginDependencyException("Cannot add a null plugin to PluginGraph");
    }
    if (hasPlugin(typeId)) {
        throw PluginDependencyException("Plugin of type '" + std::string(plugin->name()) +
                                        "' is already registered in PluginGraph");
    }

    _nodes.push_back(PluginNode{
        .typeId = typeId,
        .plugin = std::move(plugin),
        .requiredDeps = {},
        .optionalDeps = {},
    });
    _resolved = false;
    return *_nodes.back().plugin;
}

bool PluginGraph::hasPlugin(std::type_index typeId) const noexcept {
    return std::ranges::any_of(_nodes, [typeId](const PluginNode& node) { return node.typeId == typeId; });
}

IPlugin& PluginGraph::getPlugin(std::type_index typeId) const {
    for (const auto& node : _nodes) {
        if (node.typeId == typeId) {
            return *node.plugin;
        }
    }
    throw PluginDependencyException("Plugin not found in PluginGraph");
}

void PluginGraph::buildAdjacency(std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree,
                                 const std::unordered_map<std::type_index, std::size_t>& typeToIndex) {
    for (std::size_t i = 0; i < _nodes.size(); ++i) {
        PluginBuilder builder;
        _nodes[i].plugin->build(builder);
        processRequiredDeps(i, _nodes[i].plugin->name(), builder.requiredDependencies(), typeToIndex, adj, inDegree);
        processOptionalDeps(i, builder.optionalDependencies(), typeToIndex, adj, inDegree);
    }
}

void PluginGraph::throwCyclicDependencyError(const std::vector<std::size_t>& inDegree) const {
    std::ostringstream oss;
    oss << "Cyclic dependency detected in PluginGraph between plugins: ";
    bool first = true;
    for (std::size_t i = 0; i < inDegree.size(); ++i) {
        if (inDegree[i] > 0) {
            if (!first) {
                oss << ", ";
            }
            oss << "'" << _nodes[i].plugin->name() << "'";
            first = false;
        }
    }
    throw PluginDependencyException(oss.str());
}

void PluginGraph::topologicalSort(const std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree,
                                  std::vector<std::size_t>& sortedIndices) const {
    auto readyQueue = findInitialReadyNodes(inDegree);
    processKahnQueue(readyQueue, adj, inDegree, sortedIndices);

    if (sortedIndices.size() != inDegree.size()) {
        throwCyclicDependencyError(inDegree);
    }
}

std::unordered_map<std::type_index, std::size_t> PluginGraph::buildTypeIndexMap() const {
    std::unordered_map<std::type_index, std::size_t> typeToIndex;
    typeToIndex.reserve(_nodes.size());
    for (std::size_t i = 0; i < _nodes.size(); ++i) {
        typeToIndex[_nodes[i].typeId] = i;
    }
    return typeToIndex;
}

void PluginGraph::populateExecutionOrders(const std::vector<std::size_t>& sortedIndices) {
    _executionOrder.clear();
    _executionOrder.reserve(sortedIndices.size());
    for (const std::size_t idx : sortedIndices) {
        _executionOrder.push_back(_nodes[idx].plugin.get());
    }

    _shutdownOrder.clear();
    _shutdownOrder.reserve(sortedIndices.size());
    for (const std::size_t idx : std::views::reverse(sortedIndices)) {
        _shutdownOrder.push_back(_nodes[idx].plugin.get());
    }
}

void PluginGraph::resolve() {
    const std::size_t count = _nodes.size();
    if (count == 0) {
        clear();
        _resolved = true;
        return;
    }

    const auto typeToIndex = buildTypeIndexMap();
    std::vector<std::vector<std::size_t>> adj(count);
    std::vector<std::size_t> inDegree(count, 0);
    buildAdjacency(adj, inDegree, typeToIndex);

    std::vector<std::size_t> sortedIndices;
    sortedIndices.reserve(count);
    topologicalSort(adj, inDegree, sortedIndices);

    populateExecutionOrders(sortedIndices);
    _resolved = true;
}

const std::vector<IPlugin*>& PluginGraph::executionOrder() const {
    if (!_resolved) {
        throw PluginDependencyException("PluginGraph must be resolved before querying execution order");
    }
    return _executionOrder;
}

const std::vector<IPlugin*>& PluginGraph::shutdownOrder() const {
    if (!_resolved) {
        throw PluginDependencyException("PluginGraph must be resolved before querying shutdown order");
    }
    return _shutdownOrder;
}

void PluginGraph::clear() {
    _nodes.clear();
    _executionOrder.clear();
    _shutdownOrder.clear();
    _resolved = false;
}

}  // namespace engine
