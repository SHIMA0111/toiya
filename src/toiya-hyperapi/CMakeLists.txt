# ビルド時の出力先を明示的に指定
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(TOIYA_CXX_SOURCES src/reader_sample.cpp src/hyper_reader.cpp)

add_library(
    toiya
    SHARED
    ${TOIYA_CXX_SOURCES}
)
target_link_libraries(toiya
    PRIVATE Tableau::tableauhyperapi-cxx
    PRIVATE nanoarrow
)
set_target_properties(nanoarrow
    PROPERTIES POSITION_INDEPENDENT_CODE
    ON
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
        toiya
        PROPERTIES
        INSTALL_RPATH "@loader_path"
    )
else ()
    set(HYPER_LIB_DIR "lib")
    set(HYPERAPI_LIB_NAME "libtableauhyperapi.so")
    set(HYPERAPI_BIN_LOC "lib/hyper")
    set_target_properties(
        toiya
        PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif ()

install(TARGETS toiya LIBRARY DESTINATION lib/)
install(FILES ${tableauhyperapi-cxx_SOURCE_DIR}/${HYPERAPI_LIB_DIR}/${HYPERAPI_LIB_NAME} DESTINATION lib/)
install(DIRECTORY "${tableauhyperapi-cxx_SOURCE_DIR}/${HYPERAPI_BIN_LOC}/"
        DESTINATION lib/hyper
        USE_SOURCE_PERMISSIONS
)
