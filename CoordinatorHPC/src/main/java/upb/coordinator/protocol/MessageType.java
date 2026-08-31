package upb.coordinator.protocol;

public enum MessageType {
    REGISTER_WORKER,
    SUBMIT_JOB,
    EXECUTE_CHUNK,
    CHUNK_RESULT,
    JOB_RESULT,
    ERROR
}
