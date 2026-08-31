package upb.coordinator.service;

import java.util.ArrayList;
import java.util.List;

public class TaskSplitter {

    /**
     * Divide una lista total de dataset en numPartitions sublistas balanceadas.
     */
    public <T> List<List<T>> split(List<T> fullDataset, int numPartitions) {
        List<List<T>> partitions = new ArrayList<>();
        if (fullDataset == null || fullDataset.isEmpty() || numPartitions <= 0) {
            return partitions;
        }

        int totalSize = fullDataset.size();
        int chunkSize = totalSize / numPartitions;
        int remainder = totalSize % numPartitions;

        int start = 0;
        for (int i = 0; i < numPartitions; i++) {
            int extra = (i < remainder) ? 1 : 0;
            int end = Math.min(start + chunkSize + extra, totalSize);

            if (start < end) {
                partitions.add(new ArrayList<>(fullDataset.subList(start, end)));
            } else {
                partitions.add(new ArrayList<>());
            }
            start = end;
        }

        return partitions;
    }
}
