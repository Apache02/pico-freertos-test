add_library(task-bootsel-pool INTERFACE)

target_sources(task-bootsel-pool INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/task-bootsel-pool.cpp
        )

target_include_directories(task-bootsel-pool INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        )
