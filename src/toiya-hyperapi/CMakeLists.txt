set(TOIYA_CXX_SOURCES src/date_calc.cpp)

add_library(
    libtoiya
    SHARED
    ${TOIYA_CXX_SOURCES}
)
target_link_libraries(libtoiya
    PRIVATE Tableau::tableauhyperapi-cxx
    PRIVATE nanoarrow
)
set_target_properties(nanoarrow
    PROPERTIES POSITION_INDEPENDENT_CODE
    ON
)

add_executable(toiya-hyperapi src/main.cpp)
target_link_libraries(toiya-hyperapi
    PRIVATE libtoiya
    PRIVATE Tableau::tableauhyperapi-cxx
)

if (WIN32)
    set(HYPER_LIB_DIR "bin")
    set(HYPERAPI_LIB_NAME "tableauhyperapi.dll")
    set(HYPERAPI_BIN_LOC "bin/hyper")
elseif (APPLE)
    set(HYPER_LIB_DIR "lib")
    set(HYPERAPI_LIB_NAME "libtableauhyperapi.dylib")
    set(HYPERAPI_BIN_LOC "lib/hyper")
    set_target_properties(
        libtoiya
        PROPERTIES
        INSTALL_RPATH "@loader_path"
    )
else ()
    set(HYPER_LIB_DIR "lib")
    set(HYPERAPI_LIB_NAME "libtableauhyperapi.so")
    set(HYPERAPI_BIN_LOC "lib/hyper")
    set_target_properties(
        libtoiya
        PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif ()

install(TARGETS libtoiya LIBRARY DESTINATION lib/)
install(FILES ${tableauhyperapi-cxx_SOURCE_DIR}/${HYPERAPI_LIB_DIR}/${HYPERAPI_LIB_NAME} DESTINATION lib/)
install(DIRECTORY "${tableauhyperapi-cxx_SOURCE_DIR}/${HYPERAPI_BIN_LOC}/"
        DESTINATION lib/hyper
        USE_SOURCE_PERMISSIONS
)
