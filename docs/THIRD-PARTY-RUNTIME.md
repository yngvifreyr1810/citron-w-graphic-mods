# Windows runtime notices

This archive contains dynamically linked, unmodified Qt 6.9.3 libraries/plugins, LLVM libc++/libunwind, and libusb. Existing copyright and licence texts are under LICENSES/. The emulator's own corresponding source is identified in SOURCE.txt. Mod assets are downloaded separately and are not bundled.

- Qt: [Qt 6.9.3 source archives](https://download.qt.io/archive/qt/6.9/6.9.3/single/), [licensing](https://doc.qt.io/qt-6.9/licensing.html). Qt libraries can be replaced/relinked with compatible builds under the applicable LGPL/GPL terms. The archive keeps them as separate DLLs. Qt base, SVG and image-format components retain their upstream notices.
- LLVM libc++ and libunwind: [LLVM source](https://github.com/llvm/llvm-project), Apache-2.0 with LLVM exceptions; bundled notices are in LICENSES/libc++ and LICENSES/libunwind.
- libusb: [source and licence](https://github.com/libusb/libusb), LGPL-2.1-or-later; see LICENSES/libusb. Source for the unmodified runtime package is available through [MSYS2 package sources](https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-libusb).

Other statically linked dependencies are identified by the source tree's CPM declarations, externals and retained copyright notices. This document supplements those notices; it does not replace upstream licences.
