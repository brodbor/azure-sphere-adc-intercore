#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required (VERSION 3.11)

project (rt_Azure_Sphere_ADC C)

azsphere_configure_tools(TOOLS_REVISION "20.10")

# Create executable
add_executable(${PROJECT_NAME} main_m4.c Socket.c lib/VectorTable.c lib/ADC.c lib/GPIO.c lib/UART.c lib/Print.c lib/GPT.c lib/Mbox.c)

target_link_libraries (${PROJECT_NAME})
set_target_properties (${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld)

azsphere_target_add_image_package(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC -L"${CMAKE_SOURCE_DIR}")
