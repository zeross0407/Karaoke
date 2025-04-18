#include "audio_session.hpp"
#include "ring_buffer.hpp"
#include <chrono>

using namespace std;
/*
Tìm kiếm page trong page_table có granule_pos >= position và page trước đó có granule_pos < position.
Trả index của page tìm được, file_offset của page tìm được, granule_pos của page trước đó.
*/
OggPageStartPos AudioSession::findPageStartPos(OggOpusFile *opusFile, ogg_int64_t position)
{
    if (!opusFile || opusFile->page_table.empty())
    {
        return {0, -1, -1}; // Trả về giá trị invalid
    }

    auto it = lower_bound(
        opusFile->page_table.begin(),
        opusFile->page_table.end(),
        position,
        [](const OggPageIndex &page, ogg_int64_t pos)
        {
            return page.granule_pos < pos;
        });

    if (it == opusFile->page_table.end())
    {
        return {0, -1, -1}; // Trả về giá trị invalid
    }

    return {
        it->index,
        it->file_offset,
        (it == opusFile->page_table.begin()) ? 0 : (it - 1)->granule_pos // granule_pos của page trước đó
    };
}
/*
Seek đến đầu file, đọc buffer ra
*/
Result AudioSession::seekBeginOfFile()
{
    buffer->clear();
    timing.hasStartTime = false;
    timing.seekTime = 0;
    timing.target_pcm_pos = oggFile->header.preskip;

    // Seek về đầu file
    if (fseek(oggFile->fin, 0, SEEK_SET) != 0)
    {
        return Result::error(ErrorCode::SeekError, "Failed to seek to beginning");
    }

    // Reset decoder states
    ogg_sync_reset(&oggFile->oy);
    ogg_stream_reset(&oggFile->os);

    // Fill buffer và return
    return fillBuffer();
}

/*
Từ vị trí preroll_granulepos là granule_pos của ogg_page trước đó ogg_page chứa điểm preroll_pos,
1. Đọc các packet, decode thành PCM samples
2. Tính tổng số PCM samples đã decode được rồi cộng với preroll_granulepos
3. So sánh với target_pcm_pos
4. Nếu chưa đến target_pcm_pos thì tiếp tục đọc các packet tiếp theo
5. Nếu đã đến target_pcm_pos thì tính số lượng samples cần bỏ qua
6. Chỉ copy các samples tính từ target_pcm_pos đến số lượng samples đã decode được
7. Trả về kết quả
*/
Result AudioSession::preroll_decode(ogg_int64_t target_pcm_pos, ogg_int64_t preroll_granulepos)
{
    int64_t decoded_pos = preroll_granulepos;
    bool header_packets_skipped = false;
    while (true)
    {
        // Đọc packet từ ogg stream
        int result = ogg_stream_packetout(&oggFile->os, &oggFile->op);

        if (result == 0)
        {
            // Cần thêm dữ liệu
            char *ogg_buffer = ogg_sync_buffer(&oggFile->oy, 4096);
            size_t bytes = fread(ogg_buffer, 1, 4096, oggFile->fin);
            if (bytes == 0)
            {
                return Result::error(ErrorCode::FileReadError, "Unexpected EOF during preroll");
            }
            ogg_sync_wrote(&oggFile->oy, bytes);

            if (ogg_sync_pageout(&oggFile->oy, &oggFile->og) != 1)
            {
                return Result::error(ErrorCode::OggSyncError, "Failed to sync page during preroll");
            }

            if (ogg_stream_pagein(&oggFile->os, &oggFile->og) < 0)
            {
                return Result::error(ErrorCode::OggStreamError, "Failed to submit page during preroll");
            }
            continue;
        }
        else if (result < 0)
        {
            return Result::error(ErrorCode::OggPacketCorrupt, "Corrupted packet during preroll");
        }

        // Bỏ qua các header packet nếu chưa được skip
        if (!header_packets_skipped)
        {
            if (oggFile->op.b_o_s)
            { // Beginning of stream packet (OpusHead)
                continue;
            }
            if (oggFile->op.packet[0] == 'O' && oggFile->op.packet[1] == 'p')
            { // OpusTags
                header_packets_skipped = true;
                continue;
            }
        }

        // Decode packet opus
        int frames = opus_decode_float(oggFile->decoder,
                                       oggFile->op.packet,
                                       oggFile->op.bytes,
                                       pcmBuffer.get(),
                                       5760,
                                       0);

        if (frames < 0)
        {
            return Result::error(ErrorCode::OpusDecodeError, "Failed to decode Opus packet");
        }

        decoded_pos += frames;
        // Kiểm tra đã đến target chưa
        if (decoded_pos > target_pcm_pos)
        {
            // Tính số lượng samples cần bỏ qua
            int samples_to_skip = frames - (decoded_pos - target_pcm_pos);
            // Sử dụng hàm decodeAndResample để xử lý
            return decodeAndResample(
                oggFile->op.packet,
                oggFile->op.bytes,
                samples_to_skip,
                decoded_pos - target_pcm_pos,
                false // Không áp dụng speed trong preroll
            );
        }
    }
}
/*
Seek file stream đến offset của preroll_page
Reset trạng thái của Ogg decoder
Lấy buffer tạm từ ogg sync state
Thực hiện preroll
Fill thêm dữ liệu vào buffer cho đến khi đạt 50% buffer size
*/
Result AudioSession::preroll_seek(int64_t prerollFilePos, int64_t prerollGranulePos, int64_t target_pcm_pos)
{
    // Seek file stream đến offset của preroll_page
    if (fseek(oggFile->fin, prerollFilePos, SEEK_SET) != 0)
    {
        return Result::error(ErrorCode::SeekError, "Failed to seek to preroll position");
    }

    // Reset trạng thái của Ogg decoder
    ogg_sync_reset(&oggFile->oy);
    ogg_stream_reset(&oggFile->os);

    // Lấy buffer tạm từ ogg sync state
    char *ogg_buffer = ogg_sync_buffer(&oggFile->oy, 4096);
    size_t bytes = fread(ogg_buffer, 1, 4096, oggFile->fin);
    ogg_sync_wrote(&oggFile->oy, bytes);

    // Thực hiện preroll
    Result prerollResult = preroll_decode(target_pcm_pos, prerollGranulePos);
    if (!prerollResult.isSuccess())
    {
        return prerollResult;
    }

    // Fill thêm dữ liệu vào buffer cho đến khi đạt 50% buffer size
    Result fillResult = fillBuffer();
    if (!fillResult.isSuccess())
    {
        return fillResult;
    }
    // 8. Cập nhật trạng thái
    // Reset các biến theo dõi thời gian
    timing.elapsedTime = 0;
    timing.startTime = chrono::steady_clock::now();
    timing.hasStartTime = true;

    // Debug thông tin seek
    debugPrint("Seek completed: time={}, buffer_size={}",
               timing.seekTime, this->buffer->availableForRead());

    return Result::success();
}
Result AudioSession::seekToTime(uint32_t timeMs)
{
    // 1. Kiểm tra điều kiện tiên quyết
    // - Kiểm tra trạng thái hợp lệ (PLAYING, PAUSED, READY)
    auto currentState = state.load();
    if (currentState != PlayState::PLAYING &&
        currentState != PlayState::PAUSED &&
        currentState != PlayState::READY)
    {
        return Result::error(ErrorCode::NotReady, "Invalid state for seeking");
    }
    // Nếu timeMs = 0, seek đến đầu file bỏ qua tất cả các bước còn lại
    if (timeMs == 0)
    {
        return seekBeginOfFile();
    }

    // - Kiểm tra và giới hạn thời gian seek không vượt quá độ dài file
    timeMs = min(timeMs, this->oggFile->file_duration);
    timing.seekTime = timeMs;
    // 2. Chuẩn bị buffer và trạng thái
    // - Xóa buffer hiện tại
    // - Reset các biến trạng thái
    buffer->clear();
    timing.hasStartTime = false;  // Reset thời điểm bắt đầu
    timing.accumulatedTime = 0.0; // Reset accumulated time

    // 3. Tính toán vị trí seek
    // - Chuyển đổi thời gian (ms) sang số mẫu (samples)
    // - Tính target_pcm_pos (thêm preskip)

    // thay vì
    // timing.target_pcm_pos = static_cast<ogg_int64_t>((timeMs * SAMPLE_RATE) / 1000.0) + oggFile->header.preskip;
    // thì
    int64_t target_page = timeMs / 1000;
    int64_t target_offset = timeMs % 1000;
    int64_t target_granule = (target_page * 48000) + (target_offset * 48);
    timing.target_pcm_pos = target_granule + oggFile->header.preskip;

    debugPrint("seekToTime={}  target_pcm_pos={}", timeMs, timing.target_pcm_pos);

    // 4. Tìm page chứa preroll_granulepos
    // Tính số samples cho preroll (80ms)
    const int PREROLL_MS = 40;
    int64_t preroll_samples = (PREROLL_MS * SAMPLE_RATE) / 1000;

    // Tính preroll_granulepos
    ogg_int64_t preroll_pos = timing.target_pcm_pos - preroll_samples;
    if (preroll_pos < 0)
        preroll_pos = 0;

    // Tìm preroll_page
    OggPageStartPos pageStartPos = findPageStartPos(this->oggFile.get(), preroll_pos);

    if (pageStartPos.file_offset < 0)
    {
        return Result::error(ErrorCode::SeekError, "Failed to find preroll page");
    }
    timing.prerollFilePos = pageStartPos.file_offset;
    timing.prerollGranulePos = pageStartPos.granule_pos;

    debugPrint("preroll: target={}, preroll={}, page_idx={}, preroll_granule={}, file_offset={}",
               timing.target_pcm_pos,
               preroll_pos,
               pageStartPos.index,
               pageStartPos.granule_pos,
               pageStartPos.file_offset);

    return preroll_seek(timing.prerollFilePos, timing.prerollGranulePos, timing.target_pcm_pos);
}

/*
Phát âm thanh khi file ogg đã được load thành công
@param seekTime Thời điểm bắt đầu phát (mặc định = 0: phát từ đầu)
@param duration Thời lượng phát (mặc định = 0: phát đến hết file)
@param loop Số lần lặp lại (mặc định = 0: phát 1 lần không lặp)
*/
Result AudioSession::playAt(uint32_t seekTime, uint32_t duration, int loop)
{
    auto currentState = state.load();
    if (currentState != PlayState::READY &&
        currentState != PlayState::STOPPED)
    {
        setState(PlayState::ERROR);
        return Result::error(ErrorCode::NotReady, "Invalid state for playback");
    }

    if (!oggFile)
    {
        setState(PlayState::ERROR);
        return Result::error(ErrorCode::NotInitialized, "File not loaded");
    }
    if (seekTime >= this->oggFile->file_duration)
    {
        return Result::error(ErrorCode::InvalidParameter, "Seek time exceeds file duration");
    }

    // Reset các biến timing
    timing.totalLoop = loop;
    timing.currentLoop = 0;
    timing.hasStartTime = false;
    timing.accumulatedTime = 0.0;                         // Reset accumulated time
    timing.speed = 1.0;                                   // Reset speed về mặc định khi bắt đầu phát mới
    timing.speedChangeTime = chrono::steady_clock::now(); // Reset thời điểm thay đổi speed

    // Nếu duration = 0, phát đến hết file
    if (duration == 0)
    {
        duration = this->oggFile->file_duration - seekTime;
    }

    // Đảm bảo endTime không vượt quá this->oggFile->file_duration
    timing.endTime = seekTime + duration;
    if (timing.endTime > this->oggFile->file_duration)
    {
        timing.endTime = this->oggFile->file_duration;
    }
    timing.duration = timing.endTime - seekTime; // Gán lại thời lượng phát âm thanh
    timing.seekTime = seekTime;

    if (seekTime > 0)
    {
        // Thực hiện seek - buffer đã được fill trong seekToTime
        Result seekResult = seekToTime(seekTime);
        if (!seekResult.isSuccess())
        {
            setState(PlayState::ERROR);
            return seekResult;
        }
    }
    else
    {
        // Fill buffer trước khi bắt đầu phát
        Result result = fillBuffer();
        if (!result.isSuccess())
        {
            setState(PlayState::ERROR);
            return result;
        }
    }
    return setState(PlayState::PLAYING);
}

