import 'package:get/get.dart';
import '../controllers/karaoke_controller.dart';

class KaraokeBinding extends Bindings {
  @override
  void dependencies() {
    Get.lazyPut<KaraokeController>(
      () => KaraokeController(),
    );
  }
} 