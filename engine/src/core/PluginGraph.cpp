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
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace engine {

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
    const std::size_t count = _nodes.size();
    for (std::size_t i = 0; i < count; ++i) {
        PluginBuilder builder;
        _nodes[i].plugin->build(builder);

        for (const auto& dep : builder.requiredDependencies()) {
            auto it = typeToIndex.find(dep.typeId);
            if (it == typeToIndex.end()) {
                std::ostringstream oss;
                oss << "Plugin '" << _nodes[i].plugin->name() << "' requires missing dependency '" << dep.name << "'";
                throw PluginDependencyException(oss.str());
            }
            const std::size_t dependencyIndex = it->second;
            adj[dependencyIndex].push_back(i);
            inDegree[i]++;
        }

        for (const auto& dep : builder.optionalDependencies()) {
            auto it = typeToIndex.find(dep.typeId);
            if (it != typeToIndex.end()) {
                const std::size_t dependencyIndex = it->second;
                adj[dependencyIndex].push_back(i);
                inDegree[i]++;
            }
        }
    }
}

void PluginGraph::topologicalSort(const std::vector<std::vector<std::size_t>>& adj, std::vector<std::size_t>& inDegree,
                                  std::vector<std::size_t>& sortedIndices) const {
    const std::size_t count = inDegree.size();
    std::queue<std::size_t> readyQueue;
    for (std::size_t i = 0; i < count; ++i) {
        if (inDegree[i] == 0) {
            readyQueue.push(i);
        }
    }

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

    if (sortedIndices.size() != count) {
        std::ostringstream oss;
        oss << "Cyclic dependency detected in PluginGraph between plugins: ";
        bool first = true;
        for (std::size_t i = 0; i < count; ++i) {
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
}

void PluginGraph::resolve() {
    const std::size_t count = _nodes.size();
    if (count == 0) {
        _executionOrder.clear();
        _shutdownOrder.clear();
        _resolved = true;
        return;
    }

    std::unordered_map<std::type_index, std::size_t> typeToIndex;
    for (std::size_t i = 0; i < count; ++i) {
        typeToIndex[_nodes[i].typeId] = i;
    }

    std::vector<std::vector<std::size_t>> adj(count);
    std::vector<std::size_t> inDegree(count, 0);
    buildAdjacency(adj, inDegree, typeToIndex);

    std::vector<std::size_t> sortedIndices;
    sortedIndices.reserve(count);
    topologicalSort(adj, inDegree, sortedIndices);

    _executionOrder.clear();
    _executionOrder.reserve(count);
    for (const std::size_t idx : sortedIndices) {
        _executionOrder.push_back(_nodes[idx].plugin.get());
    }

    _shutdownOrder.clear();
    _shutdownOrder.reserve(count);
    for (const std::size_t idx : std::views::reverse(sortedIndices)) {
        _shutdownOrder.push_back(_nodes[idx].plugin.get());
    }

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
