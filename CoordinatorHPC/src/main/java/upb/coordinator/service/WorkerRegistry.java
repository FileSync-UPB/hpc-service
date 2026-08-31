package upb.coordinator.service;

import upb.coordinator.model.WorkerNode;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

public class WorkerRegistry {
    private final ConcurrentHashMap<String, WorkerNode> workers = new ConcurrentHashMap<>();

    public void registerWorker(WorkerNode worker) {
        workers.put(worker.getWorkerId(), worker);
        System.out.println("[Coordinator] Registered new worker: " + worker.getWorkerId() + " | Total Workers: " + workers.size());
    }

    public void unregisterWorker(String workerId) {
        if (workers.remove(workerId) != null) {
            System.out.println("[Coordinator] Unregistered worker: " + workerId + " | Total Workers: " + workers.size());
        }
    }

    public List<WorkerNode> getActiveWorkers() {
        return new ArrayList<>(workers.values());
    }

    public int getWorkerCount() {
        return workers.size();
    }
}
