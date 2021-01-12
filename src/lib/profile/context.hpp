// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2019-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef HPCTOOLKIT_PROFILE_CONTEXT_H
#define HPCTOOLKIT_PROFILE_CONTEXT_H

#include "metric.hpp"

#include "util/locked_unordered.hpp"
#include "util/atomic_unordered.hpp"
#include "scope.hpp"
#include "util/ragged_vector.hpp"
#include "util/ref_wrappers.hpp"

#include <unordered_set>

namespace hpctoolkit {

class SuperpositionedContext;

// A single calling Context.
class Context {
public:
  using ud_t = util::ragged_vector<const Context&>;

  ~Context() noexcept;

private:
  using children_t = util::locked_unordered_uniqued_set<Context>;

public:
  /// Access to the children of this Context.
  // MT: Externally Synchronized
  children_t& children() noexcept { return *children_p; }
  const children_t& children() const noexcept { return *children_p; }

  /// The direct parent of this Context.
  // MT: Safe (const)
  Context* direct_parent() noexcept { return u_parent; }
  const Context* direct_parent() const noexcept { return u_parent; }

  /// The Scope that this Context represents in the tree.
  // MT: Safe (const)
  const Scope& scope() const noexcept { return u_scope; }

  /// Userdata storage and access.
  // MT: See ragged_vector.
  mutable ud_t userdata;

  /// Reference to the Statistic data for this Context.
  // MT: Safe (const), Unstable (before `metrics` wavefront)
  const util::locked_unordered_map<const Metric*, StatisticAccumulator>&
  statistics() const noexcept { return data; }

  /// Traverse the subtree rooted at this Context.
  // MT: Safe (const), Unstable (before `contexts` wavefront)
  void iterate(const std::function<void(Context&)>& pre,
               const std::function<void(Context&)>& post);
  void citerate(const std::function<void(const Context&)>& pre,
                const std::function<void(const Context&)>& post) const;

private:
  std::unique_ptr<children_t> children_p;
  util::locked_unordered_set<std::unique_ptr<SuperpositionedContext>> superpositionRoots;

  Context(ud_t::struct_t& rs) : userdata(rs, std::ref(*this)) {};
  Context(ud_t::struct_t& rs, const Scope& l) : Context(rs, nullptr, l) {};
  Context(ud_t::struct_t& rs, Scope&& l) : Context(rs, nullptr, l) {};
  Context(ud_t::struct_t& rs, Context* p, const Scope& l)
    : Context(rs, p, Scope(l)) {};
  Context(ud_t::struct_t&, Context*, Scope&&);
  Context(Context&& c);

  friend class Metric;
  util::locked_unordered_map<const Metric*, StatisticAccumulator> data;

  friend class ProfilePipeline;
  /// Get the child Context for a given Scope, creating one if none exists.
  // MT: Internally Synchronized
  std::pair<Context&,bool> ensure(Scope&&);
  template<class... Args> std::pair<Context&,bool> ensure(Args&&... args) {
    return ensure(Scope(std::forward<Args>(args)...));
  }

  /// Create a child SuperpositionedContext for the given set of child Contexts.
  /// The created Context will distribute from this location based on the
  /// relative value of the given Metric.
  SuperpositionedContext& superposition(std::vector<std::reference_wrapper<Context>>);

  util::uniqable_key<Context*> u_parent;
  util::uniqable_key<Scope> u_scope;

  friend class util::uniqued<Context>;
  util::uniqable_key<Scope>& uniqable_key() { return u_scope; }
};

/// A calling context (similar to Context) but that is "in superposition" across
/// multiple individual target Contexts. The thread-local metrics associated
/// with this "Context" are distributed across the targets based on the given
/// Metric.
class SuperpositionedContext {
public:
  ~SuperpositionedContext() = default;

  const std::vector<std::reference_wrapper<Context>> targets() const {
    return m_targets;
  }

private:
  std::vector<std::reference_wrapper<Context>> m_targets;

  // Compressed subtree structure overlaying the normal Context tree. Each node
  // corresponds to a Context directly after a branching point, or otherwise in
  // need of value distribution.
  struct Node {
    Node(Context& c, bool t) : location(c), target(t) {};
    ~Node() = default;

    Context& location;
    bool target;
    std::unordered_set<std::unique_ptr<Node>> children;
  };
  std::unordered_set<std::unique_ptr<Node>> m_subtree;

  friend class ProfilePipeline;
  friend class Context;
  friend class Metric;
  SuperpositionedContext(std::vector<std::reference_wrapper<Context>>);
};

/// Generic reference to any of the Context-like classes.
/// Use ContextRef::const_t for a constant reference to a Context-like.
using ContextRef = util::variant_ref<Context, SuperpositionedContext>;

}

#endif // HPCTOOLKIT_PROFILE_CONTEXT_H
