#include "audio_session.hpp"
#include "ring_buffer.hpp"
#include <cstring>
#include <chrono>
#include <mutex>

using namespace std;

Result AudioSession::decodeAndResample(
    const unsigned char* packet,
    int bytes,
    int skipSamples,
    int maxSamples,
    bool applySpeed)
{
    // Decode packet opus
    int frames = opus_decode_float(oggFile->decoder,
                                 packet,
                                 bytes,
                                 pcmBuffer.get(),
                                 5760,
                                 0);
                                 
    if (frames < 0) {
        return Result::error(ErrorCode::OpusDecodeError, "Failed to decode Opus packet");
    }
    
    // Tính số lượng samples cần xử lý
    int samplesToProcess = frames;
    if (skipSamples >= frames) {
        return Result::success(); // Bỏ qua toàn bộ frame này
    }
    
    if (skipSamples > 0) {
        samplesToProcess = frames - skipSamples;
    }
    
    if (maxSamples > 0 && samplesToProcess > maxSamples) {
        samplesToProcess = maxSamples;
    }
    
    // Chuẩn bị dữ liệu mono cho RingBuffer
    float* monoBuffer = new float[samplesToProcess];
    
    if (oggFile->header.channels == 1) {
        // Sao chép trực tiếp cho mono, bỏ qua skipSamples
        memcpy(monoBuffer, pcmBuffer.get() + skipSamples, samplesToProcess * sizeof(float));
    } else {
        // Chuyển đổi stereo thành mono bằng cách lấy trung bình giá trị của 2 kênh
        const float* pTemp = pcmBuffer.get() + skipSamples * 2;        
        for (int i = 0; i < samplesToProcess; i++) {
            float left = *pTemp++;
            float right = *pTemp++;
            monoBuffer[i] = (left + right) * 0.5f; // Lấy trung bình của 2 kênh
        }
    }
    
    // Resample dữ liệu nếu speed khác 1.0 và cần áp dụng speed
    if (applySpeed && timing.speed != 1.0)
    {
        // Tính số lượng output frames sau khi resample
        size_t output_frames = static_cast<size_t>(samplesToProcess / timing.speed);
        
        // Tạo buffer tạm cho dữ liệu đã resample
        float* resampledBuffer = new float[output_frames];
        
        // Sử dụng hàm resampleRubberBand mới chỉ hỗ trợ mono
        Result result = resampleRubberBand(
            samplesToProcess,
            output_frames,
            monoBuffer,
            resampledBuffer
        );
        
        if (!result.isSuccess())
        {
            debugPrint("Resampling error during decode: {}", result.getErrorString());
            delete[] monoBuffer;
            delete[] resampledBuffer;
            return result;
        }
        
        // Ghi dữ liệu đã resample vào buffer (chỉ dùng cho mono)
        size_t framesWritten = buffer->write(resampledBuffer, output_frames);
        
        if (framesWritten < output_frames)
        {
            delete[] monoBuffer;
            delete[] resampledBuffer;
            return Result::error(ErrorCode::BufferOverflow, "Buffer overflow during decode");
        }
        
        // Giải phóng bộ nhớ
        delete[] resampledBuffer;
    }
    else
    {
        // Ghi trực tiếp vào buffer nếu speed = 1.0 hoặc không áp dụng speed
        size_t framesWritten = buffer->write(monoBuffer, samplesToProcess);
        
        if (framesWritten < samplesToProcess)
        {
            delete[] monoBuffer;
            return Result::error(ErrorCode::BufferOverflow, "Buffer overflow during decode");
        }
    }

    // Giải phóng bộ nhớ sau khi sử dụng
    delete[] monoBuffer;
    
    return Result::success();
}

Result AudioSession::fillBuffer()
{
  // Pre-allocate buffer cho việc đọc file
  char *readBuffer = ogg_sync_buffer(&oggFile->oy, OGG_BUFFER_SIZE);
  if (!readBuffer)
  {
    return Result::error(ErrorCode::MemoryAllocFailed, "Failed to allocate read buffer");
  }

  // Đọc và decode cho đến khi buffer đầy 50%
  while (buffer->availableForWrite() >= RING_BUFFER_SIZE / 2)
  {
    // Đọc packet từ ogg stream
    ogg_packet op;
    while (ogg_stream_packetout(&oggFile->os, &op) != 1)
    {
      // Đọc dữ liệu từ file
      int bytes = fread(readBuffer, 1, OGG_BUFFER_SIZE, oggFile->fin);
      if (bytes == 0)
      {
        debugPrint("fillBuffer EOF {}", timing.totalLoop);
        // EOF - quay lại đầu file nếu loop
        return Result::success(); // Hết file
      }
      ogg_sync_wrote(&oggFile->oy, bytes);

      // Xử lý tất cả các page trong buffer
      ogg_page og;
      while (ogg_sync_pageout(&oggFile->oy, &og) == 1)
      {
        ogg_stream_pagein(&oggFile->os, &og);

        // Cập nhật currentTime
        ogg_int64_t granulepos = ogg_page_granulepos(&og);
        if (granulepos >= 0)
        {
          timing.currentTime = static_cast<uint32_t>(
              ((granulepos - oggFile->header.preskip) * 1000.0) / SAMPLE_RATE);
        }
      }
    }

    // Kiểm tra kích thước packet
    if (op.bytes <= 0)
    {
      continue;
    }

    // Bỏ qua header và comment packets
    if (op.bytes >= 8)
    {
      if (memcmp(op.packet, "OpusHead", 8) == 0 ||
          memcmp(op.packet, "OpusTags", 8) == 0)
      {
        continue;
      }
    }

    // Sử dụng hàm decodeAndResample để xử lý
    Result result = decodeAndResample(
        op.packet,
        op.bytes,
        0,  // Không bỏ qua samples
        0,  // Lấy tất cả samples
        true // Áp dụng speed
    );
    
    if (!result.isSuccess()) {
        return result;
    }
  }
  return Result::success();
}

/*
Hàm callback xử lý audio cho định dạng Ogg:

1. Xử lý dữ liệu PCM:
   - Nếu speed = 1.0: Đọc trực tiếp từ buffer vào output
   - Nếu speed != 1.0:
     + Tính số lượng input frames cần thiết (input_frames = output_frames * speed)
     + Đọc đủ dữ liệu vào buffer tạm
     + Thực hiện resampling để tạo ra đúng số output frames yêu cầu

2. Cập nhật timing và callback:
   - Tính thời gian phát thực tế dựa trên tốc độ phát
   - Gọi callback để thông báo tiến độ phát

3. Xử lý loop và kết thúc:
   - Kiểm tra nếu đã phát hết duration
   - Xử lý loop hoặc dừng phát tùy theo cấu hình

Trả về: Số frames đã xử lý (output_frames) hoặc 0 nếu có lỗi
*/
size_t AudioSession::audioCallbackOgg(float* pcm_to_speaker, size_t frames)
{
    if (state.load() != PlayState::PLAYING)
        return 0;
    
    // Không cần nhân với số kênh vì API mới đã xử lý từng kênh riêng biệt
    size_t framesRead = buffer->read(pcm_to_speaker, frames);

    if (!timing.hasStartTime) {
        timing.startTime = chrono::steady_clock::now();
        timing.speedChangeTime = timing.startTime;
        timing.hasStartTime = true;
        timing.accumulatedTime = 0.0;
    }

    // Tính thời gian từ lần start/resume gần nhất
    auto now = chrono::steady_clock::now();
    auto timeSinceSpeedChange = chrono::duration_cast<chrono::milliseconds>(
        now - timing.speedChangeTime).count();
    
    // Tính thời gian phát thực tế (real playback time)
    uint32_t currentPlayTime = static_cast<uint32_t>(
        timing.accumulatedTime + (timeSinceSpeedChange * timing.speed)
    );

    // Cập nhật callback với thời gian hiện tại
    if (this->playbackCallback) {
        this->playbackCallback(PlaybackInfo {
            timing.seekTime + currentPlayTime,  // Vị trí tổng từ đầu file
            currentPlayTime,                    // Thời gian đã phát
            oggFile->file_duration              // Tổng thời lượng file
        });
    }

    // Kiểm tra kết thúc
    if (currentPlayTime >= timing.duration) {
        debugPrint("audioCallbackOgg current loop: {}/{}", timing.currentLoop + 1, timing.totalLoop);
        debugPrint("currentPlayTime: {} duration: {} speed: {}", 
                  currentPlayTime, timing.duration, timing.speed);

        if (timing.totalLoop == 0 || timing.currentLoop < timing.totalLoop - 1) {
            timing.currentLoop++;
            if (timing.target_pcm_pos == 0) {
                seekBeginOfFile();
            } else {
                preroll_seek(timing.prerollFilePos, timing.prerollGranulePos, timing.target_pcm_pos);
            }
            return framesRead;
        } else {
            debugPrint("File finished");
            stop();
            return framesRead;
        }
    }
    
    if (framesRead < frames) {
        fillBuffer();
        // Đọc thêm dữ liệu vào phần còn lại của buffer
        size_t additionalFrames = buffer->read(pcm_to_speaker + framesRead, frames - framesRead);
        framesRead += additionalFrames;
    }

    return framesRead;
}