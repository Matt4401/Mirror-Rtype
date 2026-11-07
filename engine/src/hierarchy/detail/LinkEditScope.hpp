/**
 * @file LinkEditScope.hpp
 * @brief Internal marker telling the hierarchy hooks that the API is already keeping both sides of a link in sync.
 */

#pragma once

namespace hierarchy::detail {

/**
 * @brief Whether a hierarchy API call is editing links on this thread (registry listeners run synchronously).
 * @return Reference to the per-thread flag.
 */
inline bool& linkEditInProgress() noexcept {
    thread_local bool inProgress = false;
    return inProgress;
}

/**
 * @brief RAII scope during which `HierarchyHooks` skips its manual-removal fix-ups.
 *
 * The fix-ups scan every `Parent` / `Children` component; the API repairs both sides itself, so it opens this scope
 * around its own removals to keep reparenting proportional to the sibling count instead of the hierarchy size.
 */
class LinkEditScope {
  public:
    /** @brief Opens the scope, remembering the previous state so scopes can nest. */
    LinkEditScope() noexcept : _previous(linkEditInProgress()) { linkEditInProgress() = true; }

    /** @brief Restores the previous state. */
    ~LinkEditScope() { linkEditInProgress() = _previous; }

    /** @brief Deleted copy constructor. */
    LinkEditScope(const LinkEditScope&) = delete;
    /** @brief Deleted copy assignment operator. */
    LinkEditScope& operator=(const LinkEditScope&) = delete;
    /** @brief Deleted move constructor. */
    LinkEditScope(LinkEditScope&&) = delete;
    /** @brief Deleted move assignment operator. */
    LinkEditScope& operator=(LinkEditScope&&) = delete;

  private:
    /** @brief State before this scope was opened. */
    bool _previous;
};

}  // namespace hierarchy::detail
