cmake_minimum_required(VERSION 3.20)

# ---- Options ----

option(COPY_BUILD "Copy the build output to the Fallout 4 directory." OFF)

# ---- Cache build vars ----

if (NOT DEFINED VCPKG_ROOT AND DEFINED ENV{VCPKG_ROOT})
	set(VCPKG_ROOT $ENV{VCPKG_ROOT})
endif ()

if (DEFINED VCPKG_ROOT)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
else ()
	message(
		WARNING
		"Variable VCPKG_ROOT is not set. Continuing without vcpkg."
	)
endif ()

set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")

# ---- Project ----

project(umbrella)

# ---- Include guards ----

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(
		FATAL_ERROR
		"In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
	)
endif ()

# ---- Globals ----

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_DEBUG_RUNTIME ON)

if (MSVC)
	add_compile_options(
		/MP	# Build with Multiple Processes
	)
endif()

# ---- Add source files ----

function (conditionally_add_subdirectory SRC_DIR)
	if (NOT TARGET ${SUBDIRECTORY})
		if (${ARGC} GREATER 1)
			add_subdirectory(${SRC_DIR} ${ARGV1})
		else ()
			add_subdirectory(${SRC_DIR})
		endif ()
	endif ()
endfunction ()

conditionally_add_subdirectory(CommonLibF4)
conditionally_add_subdirectory(NanakoFramework)
conditionally_add_subdirectory(DoomFist)
conditionally_add_subdirectory(ShieldFramework)
conditionally_add_subdirectory(AVF)
conditionally_add_subdirectory(DeBadWeapons)
conditionally_add_subdirectory(DragonBlade)
conditionally_add_subdirectory(PlayerVoiceFrequency)
conditionally_add_subdirectory(TacticalAction)
conditionally_add_subdirectory(PUBGAiming)
conditionally_add_subdirectory(AmmoUIHelper)
conditionally_add_subdirectory(SimpleImpact)
conditionally_add_subdirectory(NanakoSurgeon)
conditionally_add_subdirectory(TagSlots)
conditionally_add_subdirectory(WheelMenuHelper)
conditionally_add_subdirectory(BodyPartsUI)
conditionally_add_subdirectory(UneducatedShooter)
conditionally_add_subdirectory(NVIDIAPatcher)
conditionally_add_subdirectory(SightHelper)


include(FetchContent)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
  GIT_TAG v3.9.1)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()