set(DATAGENERATOR_SOURCES
    Source/main.cpp
    Source/devices.h
    Source/devices.cpp
    Source/filesystem.h
    Source/cmd_line_parser.h
    Source/cmd_line_parser.cpp
    Source/data_generator.h
    Source/data_generator.cpp
    Source/data_generator_params.h
    Source/utils.h
    Source/object_loader.h
    Source/object_loader.cpp
    Source/logging.h
    Source/logging.cpp
)

set(SOURCES
    ${DATAGENERATOR_SOURCES} Source/app_config.h Source/data_generator_params.h)

source_group("Source" FILES ${DATAGENERATOR_SOURCES})
    
add_executable(BaikalDataGenerator ${SOURCES})
target_compile_features(BaikalDataGenerator PRIVATE cxx_std_17)

#Add project root since BaikalStandalone directly includes Baikal/* files
target_include_directories(BaikalDataGenerator
    PRIVATE ${Baikal_SOURCE_DIR}
    PRIVATE .)
    
target_link_libraries(BaikalDataGenerator PRIVATE Baikal BaikalIO)

set_target_properties(BaikalDataGenerator
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${Baikal_SOURCE_DIR}/BaikalDataGenerator)

add_dependencies(BaikalDataGenerator ResourcesDir Copy3rdParty)

# Install target
install(TARGETS BaikalDataGenerator RUNTIME DESTINATION bin)
