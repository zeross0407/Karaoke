import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';
import 'package:path_provider/path_provider.dart';
import 'package:flutter/services.dart' show rootBundle;
import 'dart:async';

// Định nghĩa hàm khởi tạo player
typedef InitPlayerNative = Bool Function();
typedef InitPlayer = bool Function();

// Định nghĩa các hàm native với tham số đường dẫn file
typedef PlayAudioNative = Bool Function(Pointer<Utf8> filePath);
typedef PlayAudio = bool Function(Pointer<Utf8> filePath);

// Định nghĩa các hàm điều khiển phát nhạc
typedef StopNative = Void Function();
typedef Stop = void Function();

// Định nghĩa hàm pause
typedef PauseNative = Void Function();
typedef Pause = void Function();

// Định nghĩa hàm resume
typedef ResumeNative = Void Function();
typedef Resume = void Function();

// Định nghĩa hàm seek
typedef SeekNative = Void Function(Int32 time);
typedef Seek = void Function(int time);

// Định nghĩa hàm thay đổi tốc độ phát
typedef ChangeSpeedNative = Double Function(Double newSpeed);
typedef ChangeSpeed = double Function(double newSpeed);

// Định nghĩa callback từ C++ sang Dart
typedef DartPlaybackCallbackNative = Void Function(
    Int32 currentTime, Int32 elapsedTime, Int32 duration);
typedef DartPlaybackCallback = void Function(
    int currentTime, int elapsedTime, int duration);

// Định nghĩa hàm đăng ký callback
typedef RegisterDartCallbackNative = Void Function(
    Pointer<NativeFunction<DartPlaybackCallbackNative>> callback);
typedef RegisterDartCallback = void Function(
    Pointer<NativeFunction<DartPlaybackCallbackNative>> callback);

// Định nghĩa hàm xử lý thông báo đang chờ
typedef ProcessPendingNotificationsNative = Void Function();
typedef ProcessPendingNotifications = void Function();

class CAudioPlayer {
  static DynamicLibrary? _nativeLib;
  static InitPlayer? _initPlayer;
  static PlayAudio? _playAudio;
  static Stop? _stop;
  static Pause? _pause;
  static Resume? _resume;
  static Seek? _seek;
  static ChangeSpeed? _changeSpeed;
  static RegisterDartCallback? _registerDartCallback;
  static ProcessPendingNotifications? _processPendingNotifications;

  // Timer để gọi process_pending_notifications định kỳ
  static Timer? _notificationTimer;

  // Biến để theo dõi trạng thái seek
  static bool _isSeeking = false;
  static int _lastSeekTime = 0;
  static DateTime _lastSeekDateTime = DateTime.now();

  // Biến để theo dõi trạng thái phát
  static bool _isPlaybackFinished = false;

  // Callback handler để cập nhật UI
  static void Function(int currentTime, int elapsedTime, int duration)?
      _playbackUpdateHandler;

  // Đặt callback handler
  static void setPlaybackUpdateHandler(
    void Function(int currentTime, int elapsedTime, int duration) handler,
  ) {
    _playbackUpdateHandler = handler;
  }

  // Callback từ C++ sẽ gọi hàm này
  static void _dartPlaybackCallback(
    int currentTime,
    int elapsedTime,
    int duration,
  ) {
    // Kiểm tra nếu đang trong quá trình seek
    if (_isSeeking) {
      // Kiểm tra xem callback này có phải là từ vị trí seek mới không
      final timeSinceLastSeek =
          DateTime.now().difference(_lastSeekDateTime).inMilliseconds;

      // Tăng khoảng thời gian lọc từ 200ms lên 400ms để lọc tốt hơn
      if (timeSinceLastSeek < 500 &&
          (currentTime - _lastSeekTime).abs() > 400) {
        print(
            "Bỏ qua callback cũ trong quá trình seek: current=$currentTime, expected=$_lastSeekTime");
        return;
      }

      // Nếu callback này gần với giá trị seek, đánh dấu là đã seek thành công
      if ((currentTime - _lastSeekTime).abs() < 100) {
        print("Callback phù hợp với vị trí seek: $currentTime");
        _isSeeking =
            false; // Ngay lập tức tắt trạng thái seeking khi nhận được callback phù hợp
      }
    }

    // Các phần còn lại giữ nguyên
    if (duration > 0 && currentTime >= duration - 100) {
      print(
          "Playback completed, currentTime: $currentTime, duration: $duration");
      _isPlaybackFinished = true;
      currentTime = duration;
    } else {
      _isPlaybackFinished = false;
    }

    // Gọi handler nếu đã được đăng ký
    if (_playbackUpdateHandler != null) {
      _playbackUpdateHandler!(currentTime, elapsedTime, duration);
    }
  }

  // Bắt đầu timer để xử lý thông báo
  static void _startNotificationTimer() {
    _notificationTimer?.cancel();
    _notificationTimer = Timer.periodic(Duration(milliseconds: 16), (timer) {
      if (_processPendingNotifications != null) {
        _processPendingNotifications!();
      }
    });
  }

  // Dừng timer xử lý thông báo
  static void _stopNotificationTimer() {
    _notificationTimer?.cancel();
    _notificationTimer = null;
  }

  // Khởi tạo thư viện an toàn
  static bool initLibrary() {
    if (_nativeLib != null) return true;

    try {
      if (Platform.isAndroid) {
        _nativeLib = DynamicLibrary.open("libplayer.so");
      } else if (Platform.isIOS) {
        _nativeLib = DynamicLibrary.process();
      } else {
        print("Nền tảng không được hỗ trợ");
        return false;
      }

      // Khởi tạo các hàm từ native
      _initPlayer = _nativeLib!
          .lookup<NativeFunction<InitPlayerNative>>('init_player')
          .asFunction();

      _playAudio = _nativeLib!
          .lookup<NativeFunction<PlayAudioNative>>('play_audio')
          .asFunction();

      // Khởi tạo các hàm điều khiển
      _stop =
          _nativeLib!.lookup<NativeFunction<StopNative>>('stop').asFunction();
      _pause = _nativeLib!
          .lookup<NativeFunction<PauseNative>>('pause_audio')
          .asFunction();
      _resume = _nativeLib!
          .lookup<NativeFunction<ResumeNative>>('resume')
          .asFunction();
      _seek =
          _nativeLib!.lookup<NativeFunction<SeekNative>>('seek').asFunction();

      // Khởi tạo hàm thay đổi tốc độ phát
      _changeSpeed = _nativeLib!
          .lookup<NativeFunction<ChangeSpeedNative>>('change_speed')
          .asFunction();

      // Khởi tạo hàm đăng ký callback
      _registerDartCallback = _nativeLib!
          .lookup<NativeFunction<RegisterDartCallbackNative>>(
            'register_dart_callback',
          )
          .asFunction();

      // Khởi tạo hàm xử lý thông báo đang chờ
      _processPendingNotifications = _nativeLib!
          .lookup<NativeFunction<ProcessPendingNotificationsNative>>(
            'process_pending_notifications',
          )
          .asFunction();

      // Đăng ký callback từ Dart
      final callbackPointer = Pointer.fromFunction<DartPlaybackCallbackNative>(
        _dartPlaybackCallback,
      );
      _registerDartCallback!(callbackPointer);
      print("Đã đăng ký callback từ Dart");

      // Bắt đầu timer để xử lý thông báo
      _startNotificationTimer();

      // Khởi tạo player
      if (_initPlayer != null) {
        bool success = _initPlayer!();
        if (!success) {
          print("Lỗi khi khởi tạo player");
          return false;
        }
        print("Đã khởi tạo player thành công");
      }

      return true;
    } catch (e) {
      print("Lỗi khi khởi tạo thư viện native: $e");
      return false;
    }
  }

  // Hàm để copy file từ assets vào thư mục tạm để có thể truy cập
  static Future<String?> _getAssetFilePath(String assetPath) async {
    try {
      // Đọc file từ assets
      final byteData = await rootBundle.load(assetPath);

      // Lấy thư mục tạm
      final tempDir = await getTemporaryDirectory();
      final tempPath = tempDir.path;

      // Tạo tên file dựa trên assetPath
      final fileName = assetPath.split('/').last;
      final filePath = '$tempPath/$fileName';

      // Ghi file vào thư mục tạm
      final file = File(filePath);
      await file.writeAsBytes(byteData.buffer.asUint8List());

      return filePath;
    } catch (e) {
      print("Lỗi khi copy file từ assets: $e");
      return null;
    }
  }

  // Wrapper cho hàm play_audio với xử lý lỗi
  static Future<bool> playAssetAudio(String filePath) async {
    try {
      if (!initLibrary() || _playAudio == null) {
        return false;
      }

      // Reset trạng thái phát hết
      _isPlaybackFinished = false;

      // Chuyển đổi String sang Pointer<Utf8>
      final filePathPointer = filePath.toNativeUtf8();
      final assetFilePath = await _getAssetFilePath(filePath);
      final assetFilePathPointer = assetFilePath!.toNativeUtf8();

      try {
        print('Playing audio file: $filePath');
        bool success = _playAudio!(assetFilePathPointer);
        return success;
      } finally {
        // Giải phóng bộ nhớ
        malloc.free(filePathPointer);
      }
    } catch (e) {
      print("Lỗi khi gọi play_audio: $e");
      return false;
    }
  }

  // Dừng phát âm thanh
  static bool stopAudio() {
    try {
      if (!initLibrary() || _stop == null) {
        return false;
      }
      print('Stopping audio playback');
      _stop!();
      // Reset playback finished state when stopping
      _isPlaybackFinished = false;
      return true;
    } catch (e) {
      print("Lỗi khi gọi stop: $e");
      return false;
    }
  }

  // Tạm dừng phát âm thanh
  static bool pauseAudio() {
    try {
      if (!initLibrary() || _pause == null) {
        return false;
      }
      print('Pausing audio playback');
      _pause!();
      return true;
    } catch (e) {
      print("Lỗi khi gọi pause: $e");
      return false;
    }
  }

  // Tiếp tục phát âm thanh
  static bool resumeAudio() {
    try {
      if (!initLibrary() || _resume == null) {
        return false;
      }
      print('Resuming audio playback');
      _resume!();
      return true;
    } catch (e) {
      print("Lỗi khi gọi resume: $e");
      return false;
    }
  }

  // Thay đổi vị trí phát
  static bool seekTo(int timeMs) {
    try {
      if (!initLibrary() || _seek == null) {
        return false;
      }
      //_stop!();
      pauseAudio();
      // Đặt trạng thái seek
      _isSeeking = true;
      _lastSeekTime = timeMs;
      _lastSeekDateTime = DateTime.now();

      // Reset trạng thái phát hết khi seek
      _isPlaybackFinished = false;

      print('Seeking to $timeMs ms');
      _seek!(timeMs);

      // Tăng thời gian giữ trạng thái seeking từ 300ms lên 800ms
      // để đảm bảo đủ thời gian để nhận và xử lý callback
      Future.delayed(Duration(milliseconds: 300), () {
        if (_isSeeking) {
          _isSeeking = false;
          print("Tự động reset trạng thái seek sau timeout");
        }
      });

      return true;
    } catch (e) {
      print("Lỗi khi gọi seek: $e");
      _isSeeking = false;
      return false;
    }
  }

  // Kiểm tra xem phát nhạc đã kết thúc chưa
  static bool isPlaybackFinished() {
    // Just use the internal flag since we don't have direct access to player state
    return _isPlaybackFinished;
  }

  // Thay đổi tốc độ phát
  static bool changePlaybackSpeed(double speed) {
    try {
      if (!initLibrary() || _changeSpeed == null) {
        return false;
      }
      print('Changing playback speed to $speed');
      double resultSpeed = _changeSpeed!(speed);
      print('New speed set to: $resultSpeed');
      return true;
    } catch (e) {
      print("Lỗi khi gọi change_speed: $e");
      return false;
    }
  }

  // Dọn dẹp tài nguyên
  static void dispose() {
    _stopNotificationTimer();
  }
}
