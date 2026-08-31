package upb.worker.executor;

import java.util.ArrayList;
import java.util.List;

public class TaskExecutor {

    public List<Object> execute(String codeInstruction, List<Object> datasetChunk) {
        List<Object> results = new ArrayList<>();
        if (datasetChunk == null) {
            return results;
        }

        System.out.println("[TaskExecutor] Processing chunk of size: " + datasetChunk.size() + " with code rule: [" + codeInstruction + "]");

        for (Object item : datasetChunk) {
            Object computedResult = processItem(codeInstruction, item);
            results.add(computedResult);
        }

        return results;
    }

    private Object processItem(String codeInstruction, Object item) {
        if (item == null) return null;
        if (codeInstruction == null || codeInstruction.isBlank()) {
            return item;
        }

        String op = codeInstruction.trim().toUpperCase();

        if (item instanceof Number number) {
            double val = number.doubleValue();
            switch (op) {
                case "SQUARE":
                    return val * val;
                case "DOUBLE":
                    return val * 2;
                case "INCREMENT":
                    return val + 1;
                default:
                    if (op.startsWith("MULTIPLY_BY:")) {
                        double factor = Double.parseDouble(op.substring("MULTIPLY_BY:".length()));
                        return val * factor;
                    }
                    return val;
            }
        } else if (item instanceof String str) {
            switch (op) {
                case "UPPERCASE":
                    return str.toUpperCase();
                case "LOWERCASE":
                    return str.toLowerCase();
                case "REVERSE":
                    return new StringBuilder(str).reverse().toString();
                default:
                    return str;
            }
        }

        return item;
    }
}
