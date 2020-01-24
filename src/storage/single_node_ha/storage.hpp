#pragma once

#include <filesystem>
#include <optional>

#include "data_structures/concurrent/concurrent_map.hpp"
#include "kvstore/kvstore.hpp"
#include "storage/common/constraints/unique_constraints.hpp"
#include "storage/common/types/types.hpp"
#include "storage/single_node_ha/edge.hpp"
#include "storage/single_node_ha/indexes/key_index.hpp"
#include "storage/single_node_ha/indexes/label_property_index.hpp"
#include "storage/single_node_ha/mvcc/version_list.hpp"
#include "storage/single_node_ha/vertex.hpp"
#include "transactions/type.hpp"

namespace database {
class GraphDb;
};

namespace database {

/** A data structure containing the main data members of a graph database. */
class Storage {
 public:
  explicit Storage(const std::vector<std::string> &properties_on_disk)
      : properties_on_disk_{properties_on_disk} {}

 public:
  ~Storage() {
    // Delete vertices and edges which weren't collected before, also deletes
    // records inside version list
    for (auto &id_vlist : vertices_.access()) delete id_vlist.second;
    for (auto &id_vlist : edges_.access()) delete id_vlist.second;
  }

  Storage(const Storage &) = delete;
  Storage(Storage &&) = delete;
  Storage &operator=(const Storage &) = delete;
  Storage &operator=(Storage &&) = delete;

  storage::GidGenerator &VertexGenerator() { return vertex_generator_; }
  storage::GidGenerator &EdgeGenerator() { return edge_generator_; }
  LabelPropertyIndex &label_property_index() { return label_property_index_; }

  /// Gets the local address for the given gid. Fails if not present.
  template <typename TRecord>
  mvcc::VersionList<TRecord> *LocalAddress(storage::Gid gid) const {
    const auto &map = GetMap<TRecord>();
    auto access = map.access();
    auto found = access.find(gid);
    CHECK(found != access.end())
        << "Failed to find "
        << (std::is_same<TRecord, Vertex>::value ? "vertex" : "edge")
        << " for gid: " << gid.AsUint();
    return found->second;
  }

  /// Gets names of properties stored on disk
  std::vector<std::string> &PropertiesOnDisk() { return properties_on_disk_; }

 private:
  friend class GraphDbAccessor;
  // Needed for GraphDb::RefreshStat.
  friend class GraphDb;
  friend class StorageGc;

  storage::GidGenerator vertex_generator_;
  storage::GidGenerator edge_generator_;

  // main storage for the graph
  ConcurrentMap<storage::Gid, mvcc::VersionList<Vertex> *> vertices_;
  ConcurrentMap<storage::Gid, mvcc::VersionList<Edge> *> edges_;

  // indexes
  KeyIndex<storage::Label, Vertex> labels_index_;
  LabelPropertyIndex label_property_index_;

  // unique constraints
  storage::constraints::UniqueConstraints unique_constraints_;

  std::vector<std::string> properties_on_disk_;

  /// Gets the Vertex/Edge main storage map.
  template <typename TRecord>
  const ConcurrentMap<storage::Gid, mvcc::VersionList<TRecord> *> &GetMap()
      const;
};

template <>
inline const ConcurrentMap<storage::Gid, mvcc::VersionList<Vertex> *>
    &Storage::GetMap() const {
  return vertices_;
}

template <>
inline const ConcurrentMap<storage::Gid, mvcc::VersionList<Edge> *>
    &Storage::GetMap() const {
  return edges_;
}

}  // namespace database
