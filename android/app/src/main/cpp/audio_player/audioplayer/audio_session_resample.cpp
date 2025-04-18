#include "audio_session.hpp"
#include <rubberband/RubberBandStretcher.h>
#include <mutex>
#include <thread>

std::mutex rubberBandMutex;

void AudioSession::initResample() {
    int error;    
    // Khởi tạo RubberBand stretcher
    int options = RubberBand::RubberBandStretcher::OptionProcessRealTime | 
                  RubberBand::RubberBandStretcher::OptionTransientsCrisp |
                  RubberBand::RubberBandStretcher::OptionEngineFaster |
                  RubberBand::RubberBandStretcher::OptionWindowShort |
                  RubberBand::RubberBandStretcher::OptionThreadingAlways;
    
    // Luôn khởi tạo với 1 kênh (mono)
    rubberBand = new RubberBand::RubberBandStretcher(SAMPLE_RATE, 1, options);
    if (!rubberBand) {
        debugPrint("Lỗi khởi tạo RubberBand stretcher");
        return;
    }
    // Priming RubberBand với dữ liệu im lặng
    const size_t primingFrames = FRAME_SIZE * 3; // Tăng số frame priming
    
    // Sử dụng calloc để cấp phát và khởi tạo về 0 trong một bước
    float* silenceBuffer = static_cast<float*>(calloc(primingFrames, sizeof(float)));
    float* inputChannelsArray[1] = { silenceBuffer };
    
    // Thực hiện nhiều lần process để đảm bảo buffer được khởi tạo đầy đủ
    {
      std::lock_guard<std::mutex> lock(rubberBandMutex);
      rubberBand->setTimeRatio(0.8f);
      rubberBand->process(inputChannelsArray, primingFrames, false);
      rubberBand->available(); // Chỉ đếm số lượng frame có sẵn
      rubberBand->setTimeRatio(1.0f);
    }

    // Giải phóng bộ nhớ sau khi sử dụng
    free(silenceBuffer);
    debugPrint("Đã khởi tạo và priming RubberBand stretcher thành công");
}

void AudioSession::cleanupResample() {
    if (rubberBand) {
        delete rubberBand;
        rubberBand = nullptr;
    }
    
    debugPrint("Đã giải phóng resampler và RubberBand stretcher");
}
/*
 * Hàm lấy tốc độ phát hiện tại của audio session
 */
double AudioSession::getPlaybackSpeed()
{
  return timing.speed;
}

/*
 * Hàm thay đổi tốc độ phát của audio
 *
 * Tham số:
 * - speed: Tốc độ phát mới (speed > 0)
 *   + speed > 1.0: Phát nhanh hơn bình thường
 *   + speed = 1.0: Phát bình thường
 *   + 0 < speed < 1.0: Phát chậm hơn bình thường
 *
 * Cách hoạt động:
 * 1. Kiểm tra tính hợp lệ:
 *    - speed phải > 0
 *    - state phải là PLAYING, PAUSED hoặc READY
 *
 * 2. Nếu đang phát (state = PLAYING):
 *    - Tính thời gian đã phát với tốc độ cũ
 *    - Cộng dồn vào accumulated_time để giữ chính xác vị trí phát
 *
 * 3. Cập nhật tốc độ mới:
 *    - Lưu speed mới
 *    - Lưu thời điểm thay đổi speed
 *    - Các frame tiếp theo sẽ được resample theo tốc độ mới
 *
 * Trả về:
 * - Result::success() nếu thành công
 * - Result::error() nếu tham số không hợp lệ hoặc state không cho phép
 */
Result AudioSession::setPlaybackSpeed(double speed)
{
  if (speed <= 0.0)
  {
    return Result::error(ErrorCode::InvalidParameter, "Speed must be positive");
  }

  auto currentState = state.load();
  if (currentState != PlayState::PLAYING &&
      currentState != PlayState::PAUSED &&
      currentState != PlayState::READY)
  {
    return Result::error(ErrorCode::InvalidState, "Invalid state for changing speed");
  }

  // Tính toán thời gian đã phát với tốc độ cũ
  if (timing.hasStartTime && currentState == PlayState::PLAYING)
  {
    auto now = chrono::steady_clock::now();
    auto timeSinceSpeedChange = chrono::duration_cast<chrono::milliseconds>(
                                    now - timing.speedChangeTime)
                                    .count();

    // Cộng dồn thời gian đã phát với tốc độ cũ
    timing.accumulatedTime += timeSinceSpeedChange * timing.speed;
  }

  // Sử dụng mutex để đồng bộ hóa
  {
    std::lock_guard<std::mutex> lock(rubberBandMutex);
    // Cập nhật tốc độ mới và thời điểm thay đổi
    timing.speed = speed;
    // Cập nhật tỉ lệ thời gian (ngược với tốc độ phát)
    rubberBand->setTimeRatio(1.0f /speed);
  }

  timing.speedChangeTime = chrono::steady_clock::now();

  return Result::success();
}


/*
 * Hàm thực hiện việc resample (tái lấy mẫu) dữ liệu PCM để thay đổi tốc độ phát
 * sử dụng thư viện RubberBand với dữ liệu mono
 *
 * Tham số:
 * - input_frames: Số lượng frame âm thanh trong buffer đầu vào
 * - output_frames: Số lượng frame âm thanh mong muốn ở đầu ra
 * - in: Buffer chứa dữ liệu mono đầu vào
 * - out: Buffer sẽ chứa dữ liệu mono đã được resample
 *
 * Cách hoạt động:
 * 1. Chuẩn bị dữ liệu đầu vào theo định dạng yêu cầu của RubberBand
 * 2. Xử lý dữ liệu qua RubberBand stretcher
 * 3. Lấy dữ liệu đã xử lý và đưa vào buffer đầu ra
 *
 * Trả về:
 * - Result::success() nếu thành công
 * - Result::error() với mã lỗi tương ứng nếu thất bại
 */
Result AudioSession::resampleRubberBand(size_t input_frames, size_t output_frames,
                                        const float* in, float* out) {
    // Kiểm tra tính hợp lệ của dữ liệu đầu vào
    if (!in || !out) {
        return Result::error(ErrorCode::InvalidParameter, "Input or output buffer is null");
    }
    
    // Sử dụng mảng tĩnh thay vì vector để tránh cấp phát động
    float* inputChannelsArray[1];
    float* outputChannelsArray[1];
    
    // Gán trực tiếp con trỏ, tránh const_cast
    // RubberBand API yêu cầu float* không phải const float*, nên vẫn cần cast
    inputChannelsArray[0] = const_cast<float*>(in);
    outputChannelsArray[0] = out;
    
    // Sử dụng mutex khi gọi các hàm của RubberBand
    {
        std::lock_guard<std::mutex> lock(rubberBandMutex);
        rubberBand->process(inputChannelsArray, input_frames, false);
    }
    
    // Lấy dữ liệu đã xử lý
    size_t available = rubberBand->available();
    if (available == 0) {
        debugPrint("Không có dữ liệu đầu ra từ RubberBand {}, {}, {}", available, input_frames, output_frames);
        return Result::error(ErrorCode::ResampleError, "No output available from RubberBand");
    }
    
    // Giới hạn số lượng frame lấy ra không vượt quá kích thước buffer đầu ra
    size_t frames_to_retrieve = std::min(available, output_frames);
    
    // Lấy dữ liệu đã xử lý từ RubberBand
    size_t retrieved = rubberBand->retrieve(outputChannelsArray, frames_to_retrieve);
    
    if (retrieved == 0) {
        debugPrint("Không thể lấy dữ liệu từ RubberBand");
        return Result::error(ErrorCode::ResampleError, "Failed to retrieve data from RubberBand");
    }
    
    return Result::success();
}