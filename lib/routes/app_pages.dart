import 'package:get/get.dart';
import '../views/karaoke_view.dart';
import '../bindings/karaoke_binding.dart';

part 'app_routes.dart';

class AppPages {
  static const INITIAL = Routes.KARAOKE;

  static final routes = [
    GetPage(
      name: Routes.KARAOKE,
      page: () => KaraokeView(),
      binding: KaraokeBinding(),
    ),
  ];
} 