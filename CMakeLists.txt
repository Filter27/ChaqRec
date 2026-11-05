cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0091 NEW)
project(ChaqRec VERSION 0.3.0 LANGUAGES CXX RC)

add_executable(ChaqRec WIN32
    src/main.cpp
    src/Audio.cpp
    src/Settings.cpp
    src/SettingsDialog.cpp
    src/Renderer.cpp
    src/M4aRecorder.cpp
    src/Transcriber.cpp
    src/Audio.h
    src/Settings.h
    src/SettingsDialog.h
    src/Renderer.h
    src/M4aRecorder.h
    src/Transcriber.h
    src/resources.h
    assets/chaqrec.rc
)
target_compile_features(ChaqRec PRIVATE cxx_std_17)
# Link the MSVC runtime statically, keeping the portable executable independent
# of an extra Visual C++ Redistributable installation.
set_property(TARGET ChaqRec PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_definitions(ChaqRec PRIVATE
    UNICODE _UNICODE _WIN32_WINNT=0x0A00
)
target_include_directories(ChaqRec PRIVATE src assets)
target_link_libraries(ChaqRec PRIVATE
    ole32 uuid shell32 user32 gdi32 gdiplus advapi32 comctl32 mfplat mfreadwrite mfuuid winhttp bcrypt
)
if(MSVC)
    target_compile_options(ChaqRec PRIVATE /W4 /EHsc /utf-8 /O2 /GS /permissive-)
    target_link_options(ChaqRec PRIVATE /SUBSYSTEM:WINDOWS)
endif()
set_target_properties(ChaqRec PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "ChaqRec"
)
