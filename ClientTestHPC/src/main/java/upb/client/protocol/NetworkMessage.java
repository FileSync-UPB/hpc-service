package upb.client.protocol;

import java.util.List;

public class NetworkMessage {
    private MessageType type;
    private String jobId;
    private String workerId;
    private String code;
    private List<Object> datasetChunk;
    private List<Object> resultChunk;
    private String error;
    private Long executionTimeMs;

    public NetworkMessage() {
    }

    public NetworkMessage(MessageType type) {
        this.type = type;
    }

    public MessageType getType() {
        return type;
    }

    public void setType(MessageType type) {
        this.type = type;
    }

    public String getJobId() {
        return jobId;
    }

    public void setJobId(String jobId) {
        this.jobId = jobId;
    }

    public String getWorkerId() {
        return workerId;
    }

    public void setWorkerId(String workerId) {
        this.workerId = workerId;
    }

    public String getCode() {
        return code;
    }

    public void setCode(String code) {
        this.code = code;
    }

    public List<Object> getDatasetChunk() {
        return datasetChunk;
    }

    public void setDatasetChunk(List<Object> datasetChunk) {
        this.datasetChunk = datasetChunk;
    }

    public List<Object> getResultChunk() {
        return resultChunk;
    }

    public void setResultChunk(List<Object> resultChunk) {
        this.resultChunk = resultChunk;
    }

    public String getError() {
        return error;
    }

    public void setError(String error) {
        this.error = error;
    }

    public Long getExecutionTimeMs() {
        return executionTimeMs;
    }

    public void setExecutionTimeMs(Long executionTimeMs) {
        this.executionTimeMs = executionTimeMs;
    }
}
