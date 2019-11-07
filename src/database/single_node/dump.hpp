#pragma once

#include <ostream>

// TODO: Move this whole file to query folder
#include "query/db_accessor.hpp"
#ifndef MG_SINGLE_NODE_V2
#include "storage/common/constraints/unique_constraints.hpp"
#endif

namespace database {

/// Class which generates sequence of openCypher queries which can be used to
/// dump the database state.
///
/// Currently only dumps index keys, vertices and edges, one-by-one in multiple
/// queries.
class CypherDumpGenerator {
 public:
  explicit CypherDumpGenerator(query::DbAccessor *dba);

  CypherDumpGenerator(const CypherDumpGenerator &other) = delete;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  CypherDumpGenerator(CypherDumpGenerator &&other) = default;
  CypherDumpGenerator &operator=(const CypherDumpGenerator &other) = delete;
  CypherDumpGenerator &operator=(CypherDumpGenerator &&other) = delete;
  ~CypherDumpGenerator() = default;

  bool NextQuery(std::ostream *os);

 private:
  // A helper class that keeps container and its iterators.
  template <typename TContainer>
  class ContainerState {
   public:
    explicit ContainerState(TContainer container)
        : container_(std::move(container)),
          current_(container_.begin()),
          end_(container_.end()),
          empty_(current_ == end_) {}

    ContainerState(const ContainerState &other) = delete;
    // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
    ContainerState(ContainerState &&other) = default;
    ContainerState &operator=(const ContainerState &other) = delete;
    ContainerState &operator=(ContainerState &&other) = delete;
    ~ContainerState() = default;

    auto GetCurrentAndAdvance() {
      auto to_be_returned = current_;
      if (current_ != end_) ++current_;
      return to_be_returned;
    }

    bool ReachedEnd() const { return current_ == end_; }

    // Returns true if the container is empty.
    bool Empty() const { return empty_; }

   private:
    TContainer container_;

    using TIterator = decltype(container_.begin());
    TIterator current_;
    TIterator end_;

    bool empty_;
  };

  class EdgesState {
   private:
    using TVertices = decltype(std::declval<query::DbAccessor>().Vertices(
        std::declval<storage::View>()));

   public:
    explicit EdgesState(TVertices vertices)
        : vertices_state_(std::move(vertices)) {
      FindNext();
    }

    EdgesState(const EdgesState &other) = delete;
    // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
    EdgesState(EdgesState &&other) = default;
    EdgesState &operator=(const EdgesState &other) = delete;
    EdgesState &operator=(EdgesState &&other) = delete;
    ~EdgesState() = default;

    auto GetCurrentAndAdvance() {
      auto edge = *current_edge_;
      FindNext();
      return edge;
    }

    bool ReachedEnd() const { return !current_edge_; }

    // Returns true if the container is empty.
    bool Empty() const { return !current_edge_; }

   private:
    void FindNext();

    std::optional<ContainerState<TVertices>> vertices_state_;
    std::optional<ContainerState<std::vector<query::EdgeAccessor>>>
        edges_list_state_;
    std::optional<query::EdgeAccessor> current_edge_;
  };

  query::DbAccessor *dba_;

  bool created_internal_index_;
  bool cleaned_internal_index_;
  bool cleaned_internal_label_property_;

#ifndef MG_SINGLE_NODE_V2
  std::optional<ContainerState<std::vector<LabelPropertyIndex::Key>>>
      indices_state_;
  std::optional<
      ContainerState<std::vector<storage::constraints::ConstraintEntry>>>
      unique_constraints_state_;
#endif
  std::optional<ContainerState<decltype(dba_->Vertices(storage::View::OLD))>>
      vertices_state_;
  std::optional<EdgesState> edges_state_;
};

}  // namespace database
