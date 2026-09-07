*** Begin Patch
*** Update File: src/UI/UserInterfaceConstants.hpp
@@
-#endif /* SRC_UI_USERINTERFACECONSTANTS_HPP_ */
+#endif /* SRC_UI_USERINTERFACECONSTANTS_HPP_ */
+
+// Temperature limits for popups
+constexpr int ExtruderMinTemp = 0;
+constexpr int ExtruderMaxTemp = 400;
+constexpr int BedMinTemp = 0;
+constexpr int BedMaxTemp = 120;
+
*** End Patch
