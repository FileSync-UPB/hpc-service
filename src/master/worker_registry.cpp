#include "hpc/master/worker_registry.hpp"

namespace hpc {

WorkerInfo WorkerRegistry::register_worker(const std::string& name, const std::string& remote_endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);

    WorkerId assigned_id = next_worker_id_++;
    auto now = std::chrono::steady_clock::now();

    WorkerInfo info;
    info.id = assigned_id;
    info.name = name.empty() ? ("Worker-" + std::to_string(assigned_id)) : name;
    info.remote_endpoint = remote_endpoint;
    info.state = WorkerNodeState::Connected;
    info.connected_at = now;
    info.last_seen = now;

    workers_[assigned_id] = info;
    return info;
}

bool WorkerRegistry::unregister_worker(WorkerId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = workers_.find(id);
    if (it != workers_.end()) {
        it->second.state = WorkerNodeState::Disconnected;
        it->second.last_seen = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool WorkerRegistry::update_heartbeat(WorkerId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = workers_.find(id);
    if (it != workers_.end() && it->second.state == WorkerNodeState::Connected) {
        it->second.last_seen = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool WorkerRegistry::get_worker_info(WorkerId id, WorkerInfo& out_info) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = workers_.find(id);
    if (it != workers_.end()) {
        out_info = it->second;
        return true;
    }
    return false;
}

std::vector<WorkerInfo> WorkerRegistry::get_active_workers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WorkerInfo> result;
    for (const auto& pair : workers_) {
        if (pair.second.state == WorkerNodeState::Connected) {
            result.push_back(pair.second);
        }
    }
    return result;
}

size_t WorkerRegistry::get_active_worker_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& pair : workers_) {
        if (pair.second.state == WorkerNodeState::Connected) {
            ++count;
        }
    }
    return count;
}

void WorkerRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    workers_.clear();
}

} // namespace hpc
