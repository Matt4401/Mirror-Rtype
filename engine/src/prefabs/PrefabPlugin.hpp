/**
 * @file PrefabPlugin.hpp
 * @brief Prefab management plugin registering entity templates and spawning helpers.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "prefabs/PrefabManager.hpp"

namespace prefabs {

/**
 * @brief Plugin responsible for registering entity prefabs and exposing PrefabManager via Mediator.
 */
class PrefabPlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs PrefabPlugin. */
    PrefabPlugin();

    /** @brief Destructor. */
    ~PrefabPlugin() override;

    /** @brief Deleted copy constructor. */
    PrefabPlugin(const PrefabPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    PrefabPlugin& operator=(const PrefabPlugin&) = delete;

    /** @brief Deleted move constructor. */
    PrefabPlugin(PrefabPlugin&&) = delete;

    /** @brief Deleted move assignment operator. */
    PrefabPlugin& operator=(PrefabPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "PrefabPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "PrefabPlugin"; }

    /**
     * @brief Declares plugin dependencies.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers PrefabManager in the engine Mediator.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unregisters PrefabManager from Mediator.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets mutable reference to internal PrefabManager. */
    [[nodiscard]] PrefabManager& manager() noexcept { return *_manager; }

    /** @brief Gets const reference to internal PrefabManager. */
    [[nodiscard]] const PrefabManager& manager() const noexcept { return *_manager; }

  private:
    /** @brief Shared pointer to internal PrefabManager registered into Mediator. */
    std::shared_ptr<PrefabManager> _manager;
};

}  // namespace prefabs
