#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

// Health of a live protocol session.  This is deliberately transport-agnostic
// so product hosts can expose diagnostics without depending on runtime types.
enum class SessionHealthState {
    Healthy,
    Suspect,
    Recovering,
    Failed,
};

// Product-neutral liveness policy.  The adapter selects the effective mode
// for each physical session; callers never need to know the underlying AXTP
// runtime type or wire opcode.
enum class SessionProbeMode {
    Auto,
    ControlHeartbeat,
    LegacyRpc,
};

inline const char* session_probe_mode_name(SessionProbeMode mode)
{
    switch (mode) {
    case SessionProbeMode::ControlHeartbeat:
        return "control-heartbeat";
    case SessionProbeMode::LegacyRpc:
        return "legacy-rpc";
    case SessionProbeMode::Auto:
    default:
        return "auto";
    }
}

inline const char* session_health_state_name(SessionHealthState state)
{
    switch (state) {
    case SessionHealthState::Healthy:
        return "healthy";
    case SessionHealthState::Suspect:
        return "suspect";
    case SessionHealthState::Recovering:
        return "recovering";
    case SessionHealthState::Failed:
        return "failed";
    }
    return "unknown";
}

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
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
    std::uint64_t control_queue_depth = 0;
    std::uint64_t control_in_flight = 0;
    std::uint64_t control_outstanding_high_water = 0;
    std::uint64_t media_dispatch_queue_depth = 0;
    std::uint64_t media_dispatch_queue_high_water = 0;
    std::uint64_t media_frames_dispatched_during_control_call = 0;
    std::string last_event;
    std::string last_error;
    // Generic media negotiation diagnostics. The adapter records the peer
    // capability intersection and result without owning product preference.
    std::vector<std::string> device_video_codecs;
    std::string requested_video_codec;
    std::string negotiated_video_codec;
    std::string video_codec_fallback_reason;
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
    SessionHealthState session_health = SessionHealthState::Healthy;
    std::uint32_t health_probe_failures = 0;
    std::uint64_t session_recoveries = 0;
    std::string last_session_recovery_reason;
    SessionProbeMode requested_probe_mode = SessionProbeMode::Auto;
    SessionProbeMode effective_probe_mode = SessionProbeMode::Auto;
    std::uint32_t negotiated_heartbeat_interval_ms = 0;
    std::uint64_t inbound_activity_generation = 0;
    std::uint64_t heartbeat_attempts = 0;
    std::uint64_t heartbeat_acks = 0;
    std::uint64_t heartbeat_timeouts = 0;
    std::uint64_t legacy_probe_attempts = 0;
    std::uint64_t legacy_probe_successes = 0;
    std::uint64_t legacy_fallbacks = 0;
    std::string legacy_fallback_reason;
    struct MediaRetryDiagnostics {
        std::uint64_t configure_attempts = 0;
        std::string last_error;
        std::uint64_t next_retry_in_ms = 0;
        bool terminal = false;
    } video_retry, audio_retry;
};

} // namespace axent
