add_library(datawriter OBJECT
    CSVFormatStrategy.cpp
    DataFormatStrategyFactory.cpp
    DataWriter.cpp
)

target_include_directories(datawriter PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/datawriter
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/data_structures
)

target_link_libraries(datawriter PRIVATE concurrentqueue)
