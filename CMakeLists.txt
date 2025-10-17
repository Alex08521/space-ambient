cmake_minimum_required(VERSION 3.10)
project(space-ambient)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
find_package(Threads REQUIRED)
find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_AUTOMOC ON)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse)
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus)
find_package(KF6 REQUIRED COMPONENTS WindowSystem)
file(GLOB AUDIO_FILES "${CMAKE_SOURCE_DIR}/data/*.oga")

if(TARGET GenerateAudioHeaders)
    message(STATUS "Цель GenerateAudioHeaders уже существует, пропускаем повторное создание")
else()
    include(cmake/GenerateAudioHeaders.cmake)
    generate_audio_headers("${AUDIO_FILES}")
endif()

if(TARGET ${PROJECT_NAME})
    message(STATUS "Цель ${PROJECT_NAME} уже существует, используем существующую")
else()
    add_executable(${PROJECT_NAME}
        src/main.cpp
        ${GENERATED_HEADERS}
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    ${PULSE_INCLUDE_DIRS}
    ${VORBISFILE_INCLUDE_DIRS}
    ${KF6_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME}
    ${PULSE_LIBRARIES}
    ${VORBISFILE_LIBRARIES}
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    KF6::WindowSystem
    pulse
    pulse-simple
    Threads::Threads
)
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "RPM;DEB")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Alexandr Pluzhinskyi")
set(CPACK_PACKAGE_DESCRIPTION "Background ambient music daemon with audio monitoring")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_URL "https://github.com/Alex08521/space-ambient")
set(CPACK_RPM_PACKAGE_REQUIRES "pulseaudio-libs, libvorbis")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Alex08521/space-ambient")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Alexandr Pluzhinskyi")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpulse0, libvorbisfile3, libqt6core6, libqt6gui6, libqt6dbus6, libkf6windowsystem6")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pulseaudio")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "pulseaudio")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/cmake/postinst;${CMAKE_SOURCE_DIR}/cmake/prerm"
)
configure_file(
    "${CMAKE_SOURCE_DIR}/service/space-ambient.service.in"
    "${CMAKE_BINARY_DIR}/space-ambient.service"
    @ONLY
)
install(TARGETS ${PROJECT_NAME}
    DESTINATION /usr/bin
    PERMISSIONS 
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)
install(FILES "${CMAKE_BINARY_DIR}/space-ambient.service"
    DESTINATION /usr/lib/systemd/user
)
install(FILES 
    "${CMAKE_SOURCE_DIR}/cmake/postinst"
    "${CMAKE_SOURCE_DIR}/cmake/prerm"
    DESTINATION /usr/share/${PROJECT_NAME}/scripts
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/cmake/postinst")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/cmake/prerm")
add_custom_target(make_scripts_executable ALL
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/cmake/postinst
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/cmake/prerm
    COMMENT "Making package scripts executable"
)
include(CPack)
add_custom_target(package_rpm
    COMMAND cpack -G RPM
    DEPENDS ${PROJECT_NAME}
    COMMENT "Building RPM package"
)
add_custom_target(package_deb
    COMMAND cpack -G DEB
    DEPENDS ${PROJECT_NAME}
    COMMENT "Building DEB package"
)
add_custom_target(package_all
    COMMAND cpack
    DEPENDS ${PROJECT_NAME}
    COMMENT "Building all packages"
)