import 'dart:ffi';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:async';

class KaraokeFFIFixed {
  /// Singleton instance
  static final KaraokeFFIFixed _instance = KaraokeFFIFixed._internal();

  /// Factory constructor
  factory KaraokeFFIFixed() => _instance;

  /// Private constructor
  KaraokeFFIFixed._internal();

  /// Thư viện động đã load
  DynamicLibrary? _karaokeLib;

  /// Function pointer cho karaoke_test
  /// Đúng signature: void karaoke_test(const char* melody, const char* lyric)
  void Function(Pointer<Char>, Pointer<Char>)? _karaokeTest;

  /// Function pointer cho karaoke_pause
  /// Đúng signature: bool karaoke_pause()
  bool Function()? _karaokePause;

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

  int getDuration() {
    return 0;
  }
}
