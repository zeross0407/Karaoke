import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:get/get.dart';
import 'package:highlighpro/ffi/audio_player_ffi.dart';
import 'package:highlighpro/ffi/karaoke_ffi.dart';
import 'package:highlighpro/models/lyric_model.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';

class KaraokeController extends GetxController {
  final ScrollController scrollController = ScrollController();
  final KaraokeFFI ffi = KaraokeFFI();

  Rx<LyricsData?> lyricsData = Rx<LyricsData?>(null);
  RxBool isLoading = true.obs;
  RxDouble currentTime = 0.0.obs;
  RxBool isPlaying = false.obs;
  RxDouble totalDuration = 0.0.obs;
  RxBool showControls = true.obs;
  RxBool isFFILoading = false.obs;
  RxString status = ''.obs;
  RxInt activeLineIndex = (-1).obs;
  RxDouble vocalVolume = 0.7.obs; // Default volume at 70%
  RxDouble micVolume = 0.7.obs; // Default mic volume at 70%
  RxDouble melodyVolume = 0.7.obs; // Default melody volume at 70%

  Timer? timer;
  Timer? controlsTimer;
  DateTime? lastUpdateTime;
  List<GlobalKey> lineKeys = [];

  @override
  void onInit() {
    super.onInit();
    loadResources();
    setupControlsTimer();
    CAudioPlayer.initLibrary();
    requestPermissions();
    initializeFFI();
  }

  @override
  void onClose() {
    stopTimer();
    controlsTimer?.cancel();
    scrollController.dispose();
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.portraitUp,
      DeviceOrientation.portraitDown,
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    super.onClose();
  }

  void setupControlsTimer() {
    controlsTimer?.cancel();
    controlsTimer = Timer(const Duration(seconds: 3), () {
      if (showControls.value) {
        showControls.value = false;
      }
    });
  }

  Future<void> requestPermissions() async {
    try {
      final permissionStatus = await Permission.microphone.request();
      status.value =
          'Quyền ghi âm: ${permissionStatus.isGranted ? "Đã cấp" : "Chưa cấp"}';
    } catch (e) {
      status.value = 'Lỗi khi yêu cầu quyền: $e';
    }
  }

  Future<void> initializeFFI() async {
    if (isFFILoading.value) return;

    final permissionStatus = await Permission.microphone.status;
    if (!permissionStatus.isGranted) {
      await requestPermissions();
      if (!(await Permission.microphone.isGranted)) {
        status.value = 'Khởi tạo thất bại: Không có quyền ghi âm';
        return;
      }
    }

    isFFILoading.value = true;
    status.value = 'Đang khởi tạo...';

    try {
      final result = ffi.initialize();
      status.value = result ? 'Khởi tạo thành công' : 'Khởi tạo thất bại';
    } catch (e) {
      status.value = 'Lỗi: $e';
    } finally {
      isFFILoading.value = false;
    }
  }

  Future<void> testKaraoke() async {
    isFFILoading.value = true;
    status.value = 'Đang kiểm tra...';

    try {
      final result = await ffi.testKaraokeWithAssets(
        'assets/cmbg_back.ogg',
        'assets/cmbg_vo.ogg',
      );
      status.value = result ? 'Kiểm tra thành công' : 'Kiểm tra thất bại';
    } catch (e) {
      status.value = 'Lỗi: $e';
    } finally {
      isFFILoading.value = false;
    }
  }

  Future<void> loadResources() async {
    try {
      await loadLyrics();
      await loadAudio();
      
      // Get duration in milliseconds from FFI
      double durationMs = await ffi.getOggOpusDuration('assets/cmbg_back.ogg');
      // Convert milliseconds to seconds for the slider
      totalDuration.value = durationMs > 0 ? durationMs / 1000.0 : 0.0;
      
      print('totalDuration: ${totalDuration.value} seconds (from ${durationMs} ms)');
    } catch (e) {
      print('Error loading resources: $e');
    }
  }

  Future<void> loadLyrics() async {
    try {
      final String response = await rootBundle.loadString('assets/cmbg.json');
      final data = await json.decode(response);

      lyricsData.value = LyricsData.fromJson(data);

      if (lyricsData.value != null && lyricsData.value!.segments.isNotEmpty) {
        lineKeys = List.generate(
          lyricsData.value!.segments.length,
          (_) => GlobalKey(),
        );
      }
    } catch (e) {
      print('Error loading lyrics: $e');
    }
  }

  Future<void> loadAudio() async {
    try {
      final bytes = await rootBundle.load('assets/CMBG.mp3');
      final dir = await getTemporaryDirectory();
      final file = File('${dir.path}/song.mp3');
      await file.writeAsBytes(
        bytes.buffer.asUint8List(bytes.offsetInBytes, bytes.lengthInBytes),
      );

      isLoading.value = false;
    } catch (e) {
      isLoading.value = false;
      print('Error loading audio: $e');
    }
  }

  void startPlayback() {
    if (!isPlaying.value) {
      isPlaying.value = true;
      testKaraoke();
      ffi.setVocalVolume(vocalVolume.value);
      ffi.setMicVolume(micVolume.value);
      ffi.setMelodyVolume(melodyVolume.value);
      startTimer();
    }
  }

  void stopPlayback() {
    if (isPlaying.value) {
      isPlaying.value = false;
      stopTimer();
    }
  }

  void startTimer() {
    stopTimer();
    lastUpdateTime = DateTime.now();

    timer = Timer.periodic(const Duration(milliseconds: 16), (timer) {
      final now = DateTime.now();

      if (lastUpdateTime != null) {
        final elapsedSeconds =
            now.difference(lastUpdateTime!).inMicroseconds / 1000000.0;
        final newTime = currentTime.value + elapsedSeconds;
        updateActiveLine(newTime);

        currentTime.value = newTime;

        if (currentTime.value >= totalDuration.value) {
          currentTime.value = totalDuration.value;
          stopPlayback();
        }
      }

      lastUpdateTime = now;
    });
  }

  void stopTimer() {
    timer?.cancel();
    timer = null;
    lastUpdateTime = null;
  }

  void updateActiveLine(double time) {
    if (lyricsData.value == null || lyricsData.value!.segments.isEmpty) return;

    int newActiveLine = -1;

    for (int i = 0; i < lyricsData.value!.segments.length; i++) {
      final segment = lyricsData.value!.segments[i];
      if (time >= segment.start && time <= segment.end) {
        newActiveLine = i;
        break;
      }
    }

    if (newActiveLine == -1) {
      for (int i = 0; i < lyricsData.value!.segments.length; i++) {
        final segment = lyricsData.value!.segments[i];
        if (time < segment.start) {
          newActiveLine = i > 0 ? i - 1 : 0;
          break;
        }
      }

      if (newActiveLine == -1 && lyricsData.value!.segments.isNotEmpty) {
        newActiveLine = lyricsData.value!.segments.length - 1;
      }
    }

    if (newActiveLine != activeLineIndex.value && newActiveLine != -1) {
      activeLineIndex.value = newActiveLine;

      // This part needs to be called from the view since it requires BuildContext
      scrollToActiveLine();
    }
  }

  void scrollToActiveLine() {
    if (Get.context != null &&
        lineKeys.isNotEmpty &&
        activeLineIndex.value < lineKeys.length &&
        activeLineIndex.value >= 0) {
      final RenderObject? renderObject =
          lineKeys[activeLineIndex.value].currentContext?.findRenderObject();
      if (renderObject != null && renderObject is RenderBox) {
        final position = renderObject.localToGlobal(Offset.zero);
        final centerPosition = position.dy + renderObject.size.height / 2;
        final screenCenter = MediaQuery.of(Get.context!).size.height / 2;
        final scrollTo =
            scrollController.offset + (centerPosition - screenCenter);

        scrollController.animateTo(
          scrollTo,
          duration: const Duration(milliseconds: 500),
          curve: Curves.easeInOut,
        );
      }
    }
  }

  void togglePlayPause() {
    if (isPlaying.value) {
      stopPlayback();
    } else {
      startPlayback();
    }
    showControlsTemporarily();
  }

  void resetAnimation() {
    stopPlayback();
    seekToPosition(0.0);
    showControlsTemporarily();
  }

  void seekToPosition(double seconds) {
    final newPosition = seconds.clamp(0.0, totalDuration.value);
    updateActiveLine(newPosition);

    currentTime.value = newPosition;

    if (isPlaying.value) {
      // Convert seconds to milliseconds for the native FFI call
      final timeMs = (newPosition * 1000).toInt();
      ffi.seekToTime(timeMs);
    }

    showControlsTemporarily();
  }

  void showControlsTemporarily() {
    showControls.value = true;
    setupControlsTimer();
  }

  void pauseKaraoke() {
    ffi.pauseKaraoke();
  }

  String formatDuration(double seconds) {
    int totalSeconds = seconds.round();
    int min = totalSeconds ~/ 60;
    int sec = totalSeconds % 60;
    return '$min:${sec.toString().padLeft(2, '0')}';
  }
  
  // Điều chỉnh âm lượng vocal
  void setVocalVolume(double volume) {
    vocalVolume.value = volume;
    if (isPlaying.value) {
      ffi.setVocalVolume(volume);
    }
  }
  
  // Điều chỉnh âm lượng micro
  void setMicVolume(double volume) {
    micVolume.value = volume;
    if (isPlaying.value) {
      ffi.setMicVolume(volume);
    }
  }
  
  // Điều chỉnh âm lượng nhạc nền (melody)
  void setMelodyVolume(double volume) {
    melodyVolume.value = volume;
    if (isPlaying.value) {
      ffi.setMelodyVolume(volume);
    }
  }
}
