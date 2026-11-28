# CMake generated Testfile for 
# Source directory: /data/data/com.termux/files/home/sonionos
# Build directory: /data/data/com.termux/files/home/sonionos/build-cmake
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[sonion_artifacts]=] "/data/data/com.termux/files/usr/bin/python3.13" "/data/data/com.termux/files/home/sonionos/scripts/check_artifacts.py" "/data/data/com.termux/files/home/sonionos/disk/EFI/BOOT/BOOTX64.EFI" "/data/data/com.termux/files/home/sonionos/disk/USER/INIT.BIN" "/data/data/com.termux/files/home/sonionos/disk/USER/DESKTOP.SON")
set_tests_properties([=[sonion_artifacts]=] PROPERTIES  DEPENDS "sonionos" _BACKTRACE_TRIPLES "/data/data/com.termux/files/home/sonionos/CMakeLists.txt;165;add_test;/data/data/com.termux/files/home/sonionos/CMakeLists.txt;0;")
