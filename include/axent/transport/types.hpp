#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace axent {

enum class TransportKind {
    Unknown,
    Hid,
    WebSocket,
    Tcp,
    Axdp,
    Tea,
    Mock,
};

struct TransportSelector {
    TransportKind kind = TransportKind::Unknown;
    std::uint16_t vendor_id = 0;
    std::uint16_t product_id = 0;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    std::uint8_t report_id = 0;
    std::size_t input_report_size = 0;
    std::size_t output_report_size = 0;
    std::size_t read_buffer_size = 4096;
    std::size_t max_reports_per_poll = 32;
    std::string path;
    std::string serial_number;
};

struct TransportDescriptor {
    std::string id;
    TransportKind kind = TransportKind::Unknown;
    bool online = false;
    std::uint16_t vendor_id = 0;
    std::uint16_t product_id = 0;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    int interface_number = -1;
    std::string path;
    std::string serial_number;
    std::string manufacturer;
    std::string product;
    std::string bus_type;
};

struct TransportDiagnostics {
    bool open = false;
    std::size_t negotiated_input_report_size = 0;
    std::size_t negotiated_output_report_size = 0;
    std::size_t read_buffer_size = 0;
    std::size_t preferred_frame_size = 0;
    std::uint64_t read_reports = 0;
    std::uint64_t write_reports = 0;
    std::uint64_t read_errors = 0;
    std::uint64_t write_errors = 0;
    std::uint64_t dropped_reports = 0;
    std::uint64_t queued_reports = 0;
    std::string last_event;
    std::string last_error;
    std::uint32_t active_video_stream_id = 0;
    std::uint32_t active_audio_stream_id = 0;
    std::uint32_t active_media_streams = 0;
    std::uint32_t last_media_source_event_id = 0;
    std::string last_media_source_event_name;
    std::string last_media_source_event_source;
    std::string last_media_source_event_state;
    std::string last_media_source_event_reason;
    std::uint32_t last_media_source_event_active_stream_id = 0;
    bool last_media_source_event_has_active_stream_id = false;
};

} // namespace axent
