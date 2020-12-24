#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required (VERSION 3.10)

project (hl_Azure_Sphere_IoT C)

azsphere_configure_tools(TOOLS_REVISION "20.10")
azsphere_configure_api(TARGET_API_SET "7")

# Create executable
add_executable (${PROJECT_NAME} main_a7.c  eventloop_timer_utilities.c)
target_link_libraries (${PROJECT_NAME} applibs pthread gcc_s c) 



target_include_directories(${PROJECT_NAME} PUBLIC ${AZURE_SPHERE_API_SET_DIR}/usr/include/azureiot 
                           ${AZURE_SPHERE_API_SET_DIR}/usr/include/azure_prov_client 
                           ${AZURE_SPHERE_API_SET_DIR}/usr/include/azure_c_shared_utility)
target_compile_definitions(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED)
target_link_libraries(${PROJECT_NAME} m azureiot applibs gcc_s c) 


#https://docs.microsoft.com/en-us/azure-sphere/app-development/manage-hardware-dependencies
#
azsphere_target_hardware_definition(${PROJECT_NAME} TARGET_DIRECTORY "HardwareDefinitions/mt3620_rdb" TARGET_DEFINITION "template_appliance.json")


azsphere_target_add_image_package(${PROJECT_NAME})

