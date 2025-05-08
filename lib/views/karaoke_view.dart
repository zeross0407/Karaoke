import 'package:flutter/material.dart';
import 'package:get/get.dart';
import 'package:highlighpro/controllers/karaoke_controller.dart';
import 'package:highlighpro/widgets/karaoke_line.dart';

class KaraokeView extends GetView<KaraokeController> {
  const KaraokeView({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Obx(() {
      if (controller.isLoading.value) {
        return const Scaffold(
          backgroundColor: Colors.black,
          body: Center(child: CircularProgressIndicator(color: Colors.white)),
        );
      }

      if (controller.lyricsData.value == null) {
        return const Scaffold(
          backgroundColor: Colors.black,
          body: Center(
            child: Text(
              'Không thể tải lời bài hát',
              style: TextStyle(color: Colors.white, fontSize: 20),
            ),
          ),
        );
      }

      return GestureDetector(
        onTap: controller.showControlsTemporarily,
        child: Scaffold(
          // floatingActionButton: FloatingActionButton(
          //   onPressed: controller.pauseKaraoke,
          //   child: const Icon(Icons.play_arrow),
          // ),
          backgroundColor: Colors.black,
          body: Stack(
            children: [
              // Main content - lyrics display
              Row(
                children: [
                  Expanded(
                    child: Center(
                      child: Container(
                        child: ClipRRect(
                          child: ShaderMask(
                            shaderCallback: (Rect rect) {
                              return LinearGradient(
                                begin: Alignment.topCenter,
                                end: Alignment.bottomCenter,
                                colors: [
                                  Colors.transparent,
                                  Colors.black,
                                  Colors.black,
                                  Colors.transparent,
                                ],
                                stops: const [0.0, 0.1, 0.9, 1.0],
                              ).createShader(rect);
                            },
                            blendMode: BlendMode.dstIn,
                            child: ListView.builder(
                              controller: controller.scrollController,
                              padding: EdgeInsets.symmetric(
                                vertical:
                                    MediaQuery.of(context).size.height * 0.4,
                              ),
                              itemCount:
                                  controller.lyricsData.value!.segments.length,
                              itemBuilder: (context, index) {
                                final segment = controller
                                    .lyricsData.value!.segments[index];
                                final isActive =
                                    index == controller.activeLineIndex.value;

                                return Container(
                                  key: controller.lineKeys[index],
                                  margin: const EdgeInsets.symmetric(
                                    vertical: 20.0,
                                  ),
                                  child: KaraokeLineV3(
                                    segment: segment,
                                    currentTime: controller.currentTime.value,
                                    fontSize: isActive ? 32 : 26,
                                    isCurrentLine: isActive,
                                  ),
                                );
                              },
                            ),
                          ),
                        ),
                      ),
                    ),
                  ),
                ],
              ),

              // Controls bar - only visible when showControls is true
              Positioned(
                left: 0,
                right: 0,
                bottom: 0,
                child: AnimatedOpacity(
                  opacity: controller.showControls.value ? 1.0 : 0.0,
                  duration: const Duration(milliseconds: 300),
                  child: Container(
                    color: Colors.black.withOpacity(0.7),
                    padding: const EdgeInsets.symmetric(
                      vertical: 12,
                      horizontal: 16,
                    ),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Row(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            // Current time
                            Text(
                              controller
                                  .formatDuration(controller.currentTime.value),
                              style: const TextStyle(color: Colors.white),
                            ),

                            const SizedBox(width: 8),

                            // Play/Pause button
                            IconButton(
                              padding: EdgeInsets.zero,
                              onPressed: controller.togglePlayPause,
                              icon: Icon(
                                controller.isPlaying.value
                                    ? Icons.pause_circle_filled
                                    : Icons.play_circle_filled,
                                color: Colors.white,
                                size: 40,
                              ),
                            ),

                            // Progress slider
                            Expanded(
                              child: SliderTheme(
                                data: SliderTheme.of(context).copyWith(
                                  thumbShape: const RoundSliderThumbShape(
                                    enabledThumbRadius: 8,
                                  ),
                                  trackHeight: 4.0,
                                  trackShape:
                                      const RoundedRectSliderTrackShape(),
                                  activeTrackColor: Colors.deepPurple,
                                  inactiveTrackColor: Colors.grey[700],
                                  thumbColor: Colors.white,
                                  overlayColor:
                                      Colors.deepPurple.withOpacity(0.4),
                                ),
                                child: Slider(
                                  value: controller.currentTime.value.clamp(
                                      0.0, controller.totalDuration.value),
                                  min: 0.0,
                                  max: controller.totalDuration.value,
                                  onChanged: controller.seekToPosition,
                                ),
                              ),
                            ),

                            // Total duration
                            Text(
                              controller.formatDuration(
                                  controller.totalDuration.value),
                              style: const TextStyle(color: Colors.white),
                            ),

                            const SizedBox(width: 8),

                            // Reset button
                            IconButton(
                              padding: EdgeInsets.zero,
                              onPressed: controller.resetAnimation,
                              icon: const Icon(
                                Icons.replay,
                                color: Colors.white,
                                size: 30,
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ),
              ),

              // Volume slider - on left side, appears with other controls
              Positioned(
                left: 0,
                top: 0,
                bottom: 0,
                child: AnimatedOpacity(
                  opacity: controller.showControls.value ? 1.0 : 0.0,
                  duration: const Duration(milliseconds: 300),
                  child: Container(
                    margin: EdgeInsets.symmetric(vertical: 50),
                    width: 100,
                    color: Colors.black.withOpacity(0.5),
                    padding: const EdgeInsets.symmetric(vertical: 20),
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Expanded(
                          child: RotatedBox(
                            quarterTurns: 3, // Rotate to make it vertical
                            child: SliderTheme(
                              data: SliderTheme.of(context).copyWith(
                                thumbShape: const RoundSliderThumbShape(
                                  enabledThumbRadius: 8,
                                ),
                                trackHeight: 4.0,
                                trackShape: const RoundedRectSliderTrackShape(),
                                activeTrackColor: Colors.deepPurple,
                                inactiveTrackColor: Colors.grey[700],
                                thumbColor: Colors.white,
                                overlayColor:
                                    Colors.deepPurple.withOpacity(0.4),
                              ),
                              child: Slider(
                                value: controller.vocalVolume.value,
                                min: 0.0,
                                max: 1.0,
                                onChanged: controller.setVocalVolume,
                              ),
                            ),
                          ),
                        ),
                        RotatedBox(
                          quarterTurns: 3,
                          child: Slider(
                            value: controller.micVolume.value,
                            min: 0.0,
                            max: 1.0,
                            onChanged: controller.setMicVolume,
                          ),
                        ),
                        RotatedBox(
                          quarterTurns: 3,
                          child: Slider(
                            value: controller.melodyVolume.value,
                            min: 0.0,
                            max: 1.0,
                            onChanged: controller.setMelodyVolume,
                          ),
                        )
                      ],
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
      );
    });
  }
}
