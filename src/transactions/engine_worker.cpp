#include <chrono>

#include "glog/logging.h"

#include "transactions/engine_rpc_messages.hpp"
#include "transactions/engine_worker.hpp"
#include "utils/atomic.hpp"

namespace tx {

WorkerEngine::WorkerEngine(const io::network::Endpoint &endpoint)
    : rpc_client_pool_(endpoint) {}

WorkerEngine::~WorkerEngine() {
  for (auto &kv : active_.access()) {
    delete kv.second;
  }
}

Transaction *WorkerEngine::Begin() {
  auto data = rpc_client_pool_.Call<BeginRpc>()->member;
  UpdateOldestActive(data.snapshot, data.tx_id);
  Transaction *tx = new Transaction(data.tx_id, data.snapshot, *this);
  auto insertion = active_.access().insert(data.tx_id, tx);
  CHECK(insertion.second) << "Failed to start creation from worker";
  return tx;
}

command_id_t WorkerEngine::Advance(transaction_id_t tx_id) {
  auto res = rpc_client_pool_.Call<AdvanceRpc>(tx_id);
  auto access = active_.access();
  auto found = access.find(tx_id);
  CHECK(found != access.end())
      << "Can't advance a transaction not in local cache";
  found->second->cid_ = res->member;
  return res->member;
}

command_id_t WorkerEngine::UpdateCommand(transaction_id_t tx_id) {
  command_id_t cmd_id = rpc_client_pool_.Call<CommandRpc>(tx_id)->member;

  // Assume there is no concurrent work being done on this worker in the given
  // transaction. This assumption is sound because command advancing needs to be
  // done in a synchronized fashion, while no workers are executing in that
  // transaction. That assumption lets us freely modify the command id in the
  // cached transaction object, and ensures there are no race conditions on
  // caching a transaction object if it wasn't cached already.

  auto access = active_.access();
  auto found = access.find(tx_id);
  if (found != access.end()) {
    found->second->cid_ = cmd_id;
  }
  return cmd_id;
}

void WorkerEngine::Commit(const Transaction &t) {
  auto res = rpc_client_pool_.Call<CommitRpc>(t.id_);
  ClearSingleTransaction(t.id_);
}

void WorkerEngine::Abort(const Transaction &t) {
  auto res = rpc_client_pool_.Call<AbortRpc>(t.id_);
  ClearSingleTransaction(t.id_);
}

CommitLog::Info WorkerEngine::Info(transaction_id_t tid) const {
  auto info = clog_.fetch_info(tid);
  // If we don't know the transaction to be commited nor aborted, ask the
  // master about it and update the local commit log.
  if (!(info.is_aborted() || info.is_committed())) {
    // @review: this version of Call is just used because Info has no
    // default constructor.
    info = rpc_client_pool_.Call<ClogInfoRpc>(tid)->member;
    if (!info.is_active()) {
      if (info.is_committed()) clog_.set_committed(tid);
      if (info.is_aborted()) clog_.set_aborted(tid);
      ClearSingleTransaction(tid);
    }
  }

  return info;
}

Snapshot WorkerEngine::GlobalGcSnapshot() {
  auto snapshot = std::move(rpc_client_pool_.Call<GcSnapshotRpc>()->member);
  UpdateOldestActive(snapshot, local_last_.load());
  return snapshot;
}

Snapshot WorkerEngine::GlobalActiveTransactions() {
  auto snapshot =
      std::move(rpc_client_pool_.Call<ActiveTransactionsRpc>()->member);
  UpdateOldestActive(snapshot, local_last_.load());
  return snapshot;
}

transaction_id_t WorkerEngine::LocalLast() const { return local_last_; }

void WorkerEngine::LocalForEachActiveTransaction(
    std::function<void(Transaction &)> f) {
  for (auto pair : active_.access()) f(*pair.second);
}

transaction_id_t WorkerEngine::LocalOldestActive() const {
  return oldest_active_;
}

Transaction *WorkerEngine::RunningTransaction(transaction_id_t tx_id) {
  auto accessor = active_.access();
  auto found = accessor.find(tx_id);
  if (found != accessor.end()) return found->second;

  auto snapshot = std::move(rpc_client_pool_.Call<SnapshotRpc>(tx_id)->member);
  UpdateOldestActive(snapshot, local_last_.load());
  return RunningTransaction(tx_id, snapshot);
}

Transaction *WorkerEngine::RunningTransaction(transaction_id_t tx_id,
                                              const Snapshot &snapshot) {
  auto accessor = active_.access();
  auto found = accessor.find(tx_id);
  if (found != accessor.end()) return found->second;

  auto new_tx = new Transaction(tx_id, snapshot, *this);
  auto insertion = accessor.insert(tx_id, new_tx);
  if (!insertion.second) delete new_tx;
  utils::EnsureAtomicGe(local_last_, tx_id);
  return insertion.first->second;
}

void WorkerEngine::ClearTransactionalCache(
    transaction_id_t oldest_active) const {
  auto access = active_.access();
  for (auto kv : access) {
    if (kv.first < oldest_active) {
      auto transaction_ptr = kv.second;
      if (access.remove(kv.first)) {
        delete transaction_ptr;
      }
    }
  }
}

void WorkerEngine::ClearSingleTransaction(transaction_id_t tx_id) const {
  auto access = active_.access();
  auto found = access.find(tx_id);
  if (found != access.end()) {
    auto transaction_ptr = found->second;
    if (access.remove(found->first)) {
      delete transaction_ptr;
    }
  }
}

void WorkerEngine::UpdateOldestActive(const Snapshot &snapshot,
                                      transaction_id_t alternative) {
  if (snapshot.empty()) {
    oldest_active_.store(std::max(alternative, oldest_active_.load()));
  } else {
    oldest_active_.store(snapshot.front());
  }
}
}  // namespace tx
