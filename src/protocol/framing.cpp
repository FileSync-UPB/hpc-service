#include "hpc/protocol/framing.hpp"

namespace hpc {

size_t MessageFraming::peek_expected_frame_size(const uint8_t* data, size_t size) {
    if (size < MESSAGE_HEADER_SIZE) {
        return 0;
    }
    // deserialize_header validará magic, versión, tipo y límite de payload.
    MessageHeader header = deserialize_header(data, size);
    return MESSAGE_HEADER_SIZE + header.payload_size;
}

bool MessageFraming::try_extract_message(std::vector<uint8_t>& buffer, Message& out_msg) {
    if (buffer.size() < MESSAGE_HEADER_SIZE) {
        return false;
    }

    size_t needed = peek_expected_frame_size(buffer.data(), buffer.size());
    if (buffer.size() < needed) {
        return false;
    }

    out_msg = deserialize_message(buffer.data(), needed);
    buffer.erase(buffer.begin(), buffer.begin() + needed);
    return true;
}

} // namespace hpc
