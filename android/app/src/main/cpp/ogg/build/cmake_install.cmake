# Install script for directory: E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/ogg")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/Debug/ogg.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/Release/ogg.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/MinSizeRel/ogg.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/RelWithDebInfo/ogg.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ogg" TYPE FILE FILES
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/include/ogg/config_types.h"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/include/ogg/ogg.h"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/include/ogg/os_types.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg/OggTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg/OggTargets.cmake"
         "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/CMakeFiles/Export/dee6fd410a50d06b294b496f57355584/OggTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg/OggTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg/OggTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg" TYPE FILE FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/CMakeFiles/Export/dee6fd410a50d06b294b496f57355584/OggTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg" TYPE FILE FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/CMakeFiles/Export/dee6fd410a50d06b294b496f57355584/OggTargets-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg" TYPE FILE FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/CMakeFiles/Export/dee6fd410a50d06b294b496f57355584/OggTargets-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg" TYPE FILE FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/CMakeFiles/Export/dee6fd410a50d06b294b496f57355584/OggTargets-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg" TYPE FILE FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/CMakeFiles/Export/dee6fd410a50d06b294b496f57355584/OggTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Ogg" TYPE FILE FILES
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/OggConfig.cmake"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/OggConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/ogg.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/ogg/html" TYPE FILE FILES
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/framing.html"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/index.html"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/oggstream.html"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/ogg-multiplex.html"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/fish_xiph_org.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/multiplex1.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/packets.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/pages.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/stream.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/vorbisword2.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/white-ogg.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/white-xifish.png"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/rfc3533.txt"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/rfc5334.txt"
    "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/skeleton.html"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/ogg/html" TYPE DIRECTORY FILES "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/doc/libogg")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "E:/nativeplayermul/nativeplayer/android/app/src/main/cpp/ogg/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
