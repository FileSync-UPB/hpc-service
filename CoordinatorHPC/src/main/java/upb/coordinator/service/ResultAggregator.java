package upb.coordinator.service;

import java.util.ArrayList;
import java.util.List;

public class ResultAggregator {

    /**
     * Consolida una lista de resultados parciales provenientes de los trabajadores.
     */
    public List<Object> aggregate(List<List<Object>> partialResults) {
        List<Object> combined = new ArrayList<>();
        if (partialResults != null) {
            for (List<Object> part : partialResults) {
                if (part != null) {
                    combined.addAll(part);
                }
            }
        }
        return combined;
    }
}
