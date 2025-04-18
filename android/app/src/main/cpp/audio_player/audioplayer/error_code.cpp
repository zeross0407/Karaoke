#include "error_code.hpp"

std::string_view getErrorString(ErrorCode code) noexcept {
    switch(code) {
        case ErrorCode::NoError:
            return "No error";
        case ErrorCode::Unknown:
            return "Unknown error";
        case ErrorCode::MemoryAllocFailed:
            return "Memory allocation failed";
        case ErrorCode::FileNotFound:
            return "File not found";
        case ErrorCode::FileReadError:
            return "File read error";
        case ErrorCode::InvalidFormat:
            return "Invalid format";
        case ErrorCode::UnsupportedFormat:
            return "Unsupported format";
        case ErrorCode::DecoderError:
            return "Decoder error";
        case ErrorCode::AudioSetupError:
            return "Audio setup error";
        case ErrorCode::OggSyncError:
            return "Ogg sync error";
        case ErrorCode::OggStreamError:
            return "Ogg stream error";
        case ErrorCode::OggInvalidFormat:
            return "Invalid Ogg format";
        case ErrorCode::OggPacketCorrupt:
            return "Corrupted Ogg packet";
        case ErrorCode::OggMetadataError:
            return "Ogg metadata error";
        case ErrorCode::OpusInvalidHeader:
            return "Invalid Opus header";
        case ErrorCode::OpusHeaderVersionUnsupported:
            return "Unsupported Opus version";
        case ErrorCode::OpusDecodeError:
            return "Opus decode error";
        case ErrorCode::ResampleError:
            return "Resampling error";
        case ErrorCode::BufferFull:
            return "Buffer full";
        case ErrorCode::EndOfFile:
            return "End of file reached";
        case ErrorCode::MaxSessionsReached:
            return "Maximum sessions reached";
        default:
            return "Unknown error code";
    }
}

constexpr bool isSuccess(ErrorCode code) noexcept {
    return code == ErrorCode::NoError;
}

constexpr bool isError(ErrorCode code) noexcept {
    return code != ErrorCode::NoError;
}