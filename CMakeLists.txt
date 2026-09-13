cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for OBS frontend events" ON)
option(ENABLE_QT "Use Qt functionality" ON)
set(OBS_DEV_DIR "" CACHE PATH "Portable OBS directory used by the local install target")

include(compilerconfig)
include(defaults)
include(helpers)

if(OBS_DEV_DIR AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${OBS_DEV_DIR}" CACHE PATH "Install prefix" FORCE)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(OS_WINDOWS)
	target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE user32 gdi32)
elseif(OS_LINUX)
	# The layer-shell protocol gives Wayland compositors the information needed
	# for a non-interactive, always-visible overlay. Keep the Qt window fallback
	# for systems without the protocol development tools.
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
	endif()
	find_program(WAYLAND_SCANNER wayland-scanner)
else()
	message(FATAL_ERROR "OBS Status Indicators has no renderer for ${CMAKE_HOST_SYSTEM_NAME}")
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Core Gui Svg Widgets REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets)
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
	src/indicator-controller.cpp
    src/obs-state-provider.cpp
    src/overlay-settings.cpp
    src/overlay-settings-dialog.cpp
    src/plugin-main.cpp
)

if(OS_WINDOWS)
	target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/windows-overlay-renderer.cpp)
elseif(OS_LINUX)
	target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/linux-overlay-renderer.cpp)
	if(TARGET PkgConfig::WAYLAND_CLIENT AND WAYLAND_SCANNER)
		set(LAYER_SHELL_PROTOCOL_XML
			${CMAKE_CURRENT_SOURCE_DIR}/src/wlr-layer-shell-unstable-v1.xml)
		set(LAYER_SHELL_PROTOCOL_HEADER
			${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h)
		set(LAYER_SHELL_PROTOCOL_CODE
			${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c)
		add_custom_command(
			OUTPUT ${LAYER_SHELL_PROTOCOL_HEADER} ${LAYER_SHELL_PROTOCOL_CODE}
			COMMAND ${WAYLAND_SCANNER} client-header ${LAYER_SHELL_PROTOCOL_XML}
				${LAYER_SHELL_PROTOCOL_HEADER}
			COMMAND ${WAYLAND_SCANNER} private-code ${LAYER_SHELL_PROTOCOL_XML}
				${LAYER_SHELL_PROTOCOL_CODE}
			DEPENDS ${LAYER_SHELL_PROTOCOL_XML}
			VERBATIM)
		target_sources(${CMAKE_PROJECT_NAME} PRIVATE
			${LAYER_SHELL_PROTOCOL_HEADER}
			${LAYER_SHELL_PROTOCOL_CODE})
		target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
		target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE PkgConfig::WAYLAND_CLIENT)
		target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
			OBS_STATUS_INDICATORS_HAVE_WAYLAND_LAYER_SHELL=1)
	endif()
endif()

include(CTest)
if(BUILD_TESTING)
	add_executable(obs-status-indicators-controller-test tests/indicator-controller-test.cpp
		src/indicator-controller.cpp)
	target_include_directories(obs-status-indicators-controller-test PRIVATE src)
	add_test(NAME indicator-controller COMMAND obs-status-indicators-controller-test)

	add_executable(obs-status-indicators-settings-test tests/overlay-settings-test.cpp)
	target_include_directories(obs-status-indicators-settings-test PRIVATE src)
	add_test(NAME overlay-settings COMMAND obs-status-indicators-settings-test)

	add_executable(obs-status-indicators-microphone-activity-test tests/microphone-activity-test.cpp)
	target_include_directories(obs-status-indicators-microphone-activity-test PRIVATE src)
	add_test(NAME microphone-activity COMMAND obs-status-indicators-microphone-activity-test)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(OBS_DEV_DIR)
  install(
    TARGETS ${CMAKE_PROJECT_NAME}
    RUNTIME DESTINATION "${OBS_DEV_DIR}/obs-plugins/64bit"
    LIBRARY DESTINATION "${OBS_DEV_DIR}/obs-plugins/64bit"
  )
  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/"
    DESTINATION "${OBS_DEV_DIR}/data/obs-plugins/${_name}"
  )
endif()
