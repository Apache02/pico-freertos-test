add_library(task-blink-sos INTERFACE)

target_sources(task-blink-sos INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/task-blink-sos.cpp
        )

target_include_directories(task-blink-sos INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        )
