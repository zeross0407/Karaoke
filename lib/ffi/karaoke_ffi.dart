import 'dart:ffi';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:async';

class KaraokeFFI {
  /// Singleton instance
  static final KaraokeFFI _instance = KaraokeFFI._internal();

  /// Factory constructor
  factory KaraokeFFI() => _instance;

  /// Private constructor
  KaraokeFFI._internal();

  /// Thư viện động đã load
  DynamicLibrary? _karaokeLib;

  /// Function pointer cho karaoke_test
  /// Đúng signature: void karaoke_test(const char* melody, const char* lyric)
  void Function(Pointer<Char>, Pointer<Char>)? _karaokeTest;

  /// Function pointer cho karaoke_pause
  /// Đúng signature: bool karaoke_pause()
  bool Function()? _karaokePause;

  /// Function pointer cho getOggOpusDurationMs
  /// Đúng signature: double getOggOpusDurationMs(const char* path)
  double Function(Pointer<Char>)? _getOggOpusDurationMs;

  /// Function pointer cho set_vocal_volume
  /// Đúng signature: bool set_vocal_volume(float volume)
  bool Function(double)? _setVocalVolume;

  /// Function pointer cho set_mic_volume
  /// Đúng signature: bool set_mic_volume(float volume)
  bool Function(double)? _setMicVolume;
  
  /// Function pointer cho set_melody_volume
  /// Đúng signature: bool set_melody_volume(float volume)
  bool Function(double)? _setMelodyVolume;

  /// Function pointer cho seek_to_time
  /// Đúng signature: bool seek_to_time(uint32_t timeMs)
  bool Function(int)? _seekToTime;

  /// Trạng thái khởi tạo
  bool _isInitialized = false;
  bool get isInitialized => _isInitialized;

  /// Khởi tạo thư viện
  bool initialize() {
    try {
      if (Platform.isAndroid) {
        // Bước 1: Load thư viện C++ shared trước để giải quyết dependency
        try {
          debugPrint('Tải thư viện libc++_shared.so');
          DynamicLibrary.open('libc++_shared.so');
        } catch (e) {
          debugPrint('Lỗi khi tải libc++_shared.so: $e');
          // Tiếp tục vì có thể đã được load tự động
        }

        // Bước 2: Load thư viện karaoke
        debugPrint('Tải thư viện libkaraoke.so');
        _karaokeLib = DynamicLibrary.open('libkaraoke.so');

        // Bước 3: Tìm và map hàm karaoke_test
        debugPrint('Tìm hàm karaoke_test');
        final testFuncPtr = _karaokeLib!.lookup<
                NativeFunction<Void Function(Pointer<Char>, Pointer<Char>)>>(
            'karaoke_test');
        _karaokeTest = testFuncPtr
            .asFunction<void Function(Pointer<Char>, Pointer<Char>)>();

        // Bước 4: Tìm và map hàm karaoke_pause
        debugPrint('Tìm hàm karaoke_pause');
        try {
          final pauseFuncPtr = _karaokeLib!
              .lookup<NativeFunction<Bool Function()>>('karaoke_pause');
          _karaokePause = pauseFuncPtr.asFunction<bool Function()>();
          debugPrint('Hàm karaoke_pause được tìm thấy');
        } catch (e) {
          debugPrint('Lỗi khi tìm hàm karaoke_pause: $e');
          _karaokePause = null;
        }

        // Bước 5: Tìm và map hàm getOggOpusDurationMs
        debugPrint('Tìm hàm getOggOpusDurationMs');
        try {
          final durationFuncPtr = _karaokeLib!
              .lookup<NativeFunction<Double Function(Pointer<Char>)>>(
                  'getOggOpusDurationMs');
          _getOggOpusDurationMs = durationFuncPtr
              .asFunction<double Function(Pointer<Char>)>();
          debugPrint('Hàm getOggOpusDurationMs được tìm thấy');
        } catch (e) {
          debugPrint('Lỗi khi tìm hàm getOggOpusDurationMs: $e');
          _getOggOpusDurationMs = null;
        }
        
        // Bước 6: Tìm và map hàm set_vocal_volume
        debugPrint('Tìm hàm set_vocal_volume');
        try {
          final setVolumePtr = _karaokeLib!
              .lookup<NativeFunction<Bool Function(Float)>>(
                  'set_vocal_volume');
          _setVocalVolume = setVolumePtr
              .asFunction<bool Function(double)>();
          debugPrint('Hàm set_vocal_volume được tìm thấy');
        } catch (e) {
          debugPrint('Lỗi khi tìm hàm set_vocal_volume: $e');
          _setVocalVolume = null;
        }
        
        // Bước 7: Tìm và map hàm set_mic_volume
        debugPrint('Tìm hàm set_mic_volume');
        try {
          final setMicVolumePtr = _karaokeLib!
              .lookup<NativeFunction<Bool Function(Float)>>(
                  'set_mic_volume');
          _setMicVolume = setMicVolumePtr
              .asFunction<bool Function(double)>();
          debugPrint('Hàm set_mic_volume được tìm thấy');
        } catch (e) {
          debugPrint('Lỗi khi tìm hàm set_mic_volume: $e');
          _setMicVolume = null;
        }
        
        // Bước 8: Tìm và map hàm set_melody_volume
        debugPrint('Tìm hàm set_melody_volume');
        try {
          final setMelodyVolumePtr = _karaokeLib!
              .lookup<NativeFunction<Bool Function(Float)>>(
                  'set_melody_volume');
          _setMelodyVolume = setMelodyVolumePtr
              .asFunction<bool Function(double)>();
          debugPrint('Hàm set_melody_volume được tìm thấy');
        } catch (e) {
          debugPrint('Lỗi khi tìm hàm set_melody_volume: $e');
          _setMelodyVolume = null;
        }
        
        // Bước 9: Tìm và map hàm seek_to_time
        debugPrint('Tìm hàm seek_to_time');
        try {
          final seekPtr = _karaokeLib!
              .lookup<NativeFunction<Bool Function(Uint32)>>(
                  'seek_to_time');
          _seekToTime = seekPtr
              .asFunction<bool Function(int)>();
          debugPrint('Hàm seek_to_time được tìm thấy');
        } catch (e) {
          debugPrint('Lỗi khi tìm hàm seek_to_time: $e');
          _seekToTime = null;
        }

        _isInitialized = true;
        debugPrint('Khởi tạo KaraokeFFI thành công');
        return true;
      } else {
        debugPrint('Platform không hỗ trợ: ${Platform.operatingSystem}');
        return false;
      }
    } catch (e) {
      debugPrint('Lỗi khởi tạo KaraokeFFI: $e');
      _isInitialized = false;
      return false;
    }
  }

  /// Gọi hàm test karaoke với đường dẫn mặc định
  Future<bool> testKaraoke() async {
    return await testKaraokeWithAssets(
        "assets/cmbg_back.ogg", "assets/CMBG.mp3");
  }

  /// Copy file từ asset sang cache để có thể đọc được từ native code
  Future<String> _getAssetFilePath(String assetPath) async {
    // Lấy thư mục cache
    final cacheDir = await getTemporaryDirectory();
    final fileName = assetPath.split('/').last;
    final filePath = '${cacheDir.path}/$fileName';

    // Kiểm tra nếu file đã có sẵn
    if (await File(filePath).exists()) {
      debugPrint('File $fileName đã tồn tại trong cache');
      return filePath;
    }

    // Copy file từ asset sang cache
    debugPrint('Copy file $assetPath sang cache');
    final data = await rootBundle.load(assetPath);
    final bytes = data.buffer.asUint8List();
    await File(filePath).writeAsBytes(bytes);

    return filePath;
  }

  /// Gọi hàm test karaoke với các asset cụ thể
  Future<bool> testKaraokeWithAssets(
      String melodyAssetPath, String lyricAssetPath) async {
    if (!_isInitialized || _karaokeTest == null) {
      debugPrint('KaraokeFFI chưa được khởi tạo');
      return false;
    }

    try {
      // Copy asset files to a readable location
      final melodyPath = await _getAssetFilePath(melodyAssetPath);
      final lyricPath = await _getAssetFilePath(lyricAssetPath);

      debugPrint('Gọi hàm karaoke_test với:');
      debugPrint('- Melody: $melodyAssetPath -> $melodyPath');
      debugPrint('- Lyric: $lyricAssetPath -> $lyricPath');

      // Gọi hàm native với đường dẫn cache
      return testKaraokeWithFiles(melodyPath, lyricPath);
    } catch (e) {
      debugPrint('Lỗi khi chuẩn bị files: $e');
      return false;
    }
  }

  /// Gọi hàm test karaoke với các file cụ thể (đường dẫn đầy đủ)
  bool testKaraokeWithFiles(String melodyPath, String lyricPath) {
    if (!_isInitialized || _karaokeTest == null) {
      debugPrint('KaraokeFFI chưa được khởi tạo');
      return false;
    }

    try {
      debugPrint('Gọi hàm karaoke_test với $melodyPath và $lyricPath');

      // Chuyển string thành C string (char*)
      final melodyNative = melodyPath.toNativeUtf8();
      final lyricNative = lyricPath.toNativeUtf8();

      // Gọi hàm native
      _karaokeTest!(melodyNative.cast<Char>(), lyricNative.cast<Char>());

      // Giải phóng bộ nhớ
      malloc.free(melodyNative);
      malloc.free(lyricNative);

      debugPrint('Gọi hàm karaoke_test thành công');
      return true;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm karaoke_test: $e');
      return false;
    }
  }

  /// Tạm dừng phát nhạc karaoke đang chạy
  bool pauseKaraoke() {
    if (!_isInitialized || _karaokePause == null) {
      debugPrint(
          'KaraokeFFI chưa được khởi tạo hoặc hàm karaoke_pause không có sẵn');
      return false;
    }

    try {
      debugPrint('Gọi hàm karaoke_pause');

      // Gọi hàm native
      final result = _karaokePause!();

      if (result) {
        debugPrint('Đã tạm dừng karaoke thành công');
      } else {
        debugPrint('Không có phiên karaoke nào đang chạy để tạm dừng');
      }

      return result;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm karaoke_pause: $e');
      return false;
    }
  }

  /// Lấy thời lượng của file Ogg/Opus (milliseconds)
  Future<double> getOggOpusDuration(String assetPath) async {
    if (!_isInitialized || _getOggOpusDurationMs == null) {
      debugPrint(
          'KaraokeFFI chưa được khởi tạo hoặc hàm getOggOpusDurationMs không có sẵn');
      return -1;
    }

    try {
      // Đảm bảo file có thể đọc được từ native code
      final filePath = await _getAssetFilePath(assetPath);
      
      // Kiểm tra file có tồn tại không
      final fileExists = await File(filePath).exists();
      if (!fileExists) {
        debugPrint('LỖI: File $filePath không tồn tại trên thiết bị');
        return -1;
      }

      // Kiểm tra kích thước file
      final fileSize = await File(filePath).length();
      debugPrint('Kích thước file: $fileSize bytes');
      if (fileSize <= 0) {
        debugPrint('LỖI: File $filePath có kích thước không hợp lệ: $fileSize bytes');
        return -1;
      }
      
      debugPrint('Gọi hàm getOggOpusDurationMs với $filePath');
      
      // Chuyển string thành C string (char*)
      final pathNative = filePath.toNativeUtf8();
      
      // Gọi hàm native
      final durationMs = _getOggOpusDurationMs!(pathNative.cast<Char>());
      
      // Giải phóng bộ nhớ
      malloc.free(pathNative);
      
      if (durationMs < 0) {
        debugPrint('LỖI: Không thể đọc thời lượng file ${assetPath}. Mã lỗi: $durationMs');
        debugPrint('Chi tiết: Có thể file không phải định dạng Ogg/Opus hoặc bị hỏng');
        
        try {
          // Đọc phần đầu của file để kiểm tra signature
          final bytes = await File(filePath).openRead(0, 32).toList();
          final buffer = bytes.expand((x) => x).toList();
          final signature = buffer.length >= 4 ? buffer.sublist(0, 4) : [];
          
          // Convert signature bytes to hex string
          final hexSignature = signature.map((b) => b.toRadixString(16).padLeft(2, '0')).join(' ');
          debugPrint('Signature của file: $hexSignature');
          
          // Kiểm tra OGG signature
          if (buffer.length >= 4 && 
              buffer[0] == 0x4F && buffer[1] == 0x67 && 
              buffer[2] == 0x67 && buffer[3] == 0x53) {
            debugPrint('File có OGG signature nhưng không đọc được thời lượng');
          } else {
            debugPrint('File không có OGG signature hợp lệ');
          }
        } catch (e) {
          debugPrint('Không thể đọc signature của file: $e');
        }
        
        return -1;
      }
      
      debugPrint('Thời lượng file ${assetPath}: $durationMs ms');
      return durationMs;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm getOggOpusDurationMs: $e');
      return -1;
    }
  }
  
  /// Điều chỉnh âm lượng vocal (phần giọng hát)
  /// volume: giá trị từ 0.0 đến 1.0
  bool setVocalVolume(double volume) {
    if (!_isInitialized || _setVocalVolume == null) {
      debugPrint(
          'KaraokeFFI chưa được khởi tạo hoặc hàm set_vocal_volume không có sẵn');
      return false;
    }

    try {
      // Đảm bảo giá trị volume nằm trong khoảng hợp lệ 0.0 - 1.0
      final clampedVolume = volume.clamp(0.0, 1.0);
      debugPrint('Gọi hàm set_vocal_volume với giá trị: $clampedVolume');
      
      // Gọi hàm native
      final result = _setVocalVolume!(clampedVolume);
      
      if (result) {
        debugPrint('Đã điều chỉnh âm lượng vocal thành công');
      } else {
        debugPrint('Không thể điều chỉnh âm lượng (có thể chưa có phiên karaoke nào đang chạy)');
      }
      
      return result;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm set_vocal_volume: $e');
      return false;
    }
  }
  
  /// Điều chỉnh âm lượng micro
  /// volume: giá trị từ 0.0 đến 1.0
  bool setMicVolume(double volume) {
    if (!_isInitialized || _setMicVolume == null) {
      debugPrint(
          'KaraokeFFI chưa được khởi tạo hoặc hàm set_mic_volume không có sẵn');
      return false;
    }

    try {
      // Đảm bảo giá trị volume nằm trong khoảng hợp lệ 0.0 - 1.0
      final clampedVolume = volume.clamp(0.0, 1.0);
      debugPrint('Gọi hàm set_mic_volume với giá trị: $clampedVolume');
      
      // Gọi hàm native
      final result = _setMicVolume!(clampedVolume);
      
      if (result) {
        debugPrint('Đã điều chỉnh âm lượng micro thành công');
      } else {
        debugPrint('Không thể điều chỉnh âm lượng micro (có thể chưa có phiên karaoke nào đang chạy)');
      }
      
      return result;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm set_mic_volume: $e');
      return false;
    }
  }
  
  /// Điều chỉnh âm lượng nhạc nền (melody)
  /// volume: giá trị từ 0.0 đến 1.0
  bool setMelodyVolume(double volume) {
    if (!_isInitialized || _setMelodyVolume == null) {
      debugPrint(
          'KaraokeFFI chưa được khởi tạo hoặc hàm set_melody_volume không có sẵn');
      return false;
    }

    try {
      // Đảm bảo giá trị volume nằm trong khoảng hợp lệ 0.0 - 1.0
      final clampedVolume = volume.clamp(0.0, 1.0);
      debugPrint('Gọi hàm set_melody_volume với giá trị: $clampedVolume');
      
      // Gọi hàm native
      final result = _setMelodyVolume!(clampedVolume);
      
      if (result) {
        debugPrint('Đã điều chỉnh âm lượng nhạc nền thành công');
      } else {
        debugPrint('Không thể điều chỉnh âm lượng nhạc nền (có thể chưa có phiên karaoke nào đang chạy)');
      }
      
      return result;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm set_melody_volume: $e');
      return false;
    }
  }
  
  /// Tìm đến vị trí thời gian cụ thể trong bài hát
  /// timeMs: thời gian cần tìm đến, tính bằng mili-giây
  bool seekToTime(int timeMs) {
    if (!_isInitialized || _seekToTime == null) {
      debugPrint(
          'KaraokeFFI chưa được khởi tạo hoặc hàm seek_to_time không có sẵn');
      return false;
    }

    try {
      // Đảm bảo thời gian không âm
      final time = timeMs < 0 ? 0 : timeMs;
      debugPrint('Gọi hàm seek_to_time với thời gian: $time ms');
      
      // Gọi hàm native
      final result = _seekToTime!(time);
      
      if (result) {
        debugPrint('Đã tìm đến vị trí $time ms thành công');
      } else {
        debugPrint('Không thể tìm đến vị trí (có thể chưa có phiên karaoke nào đang chạy)');
      }
      
      return result;
    } catch (e) {
      debugPrint('Lỗi khi gọi hàm seek_to_time: $e');
      return false;
    }
  }
}
