# /src

set (PARSER_SUBDIR "lyparser")
include_directories(
    "${CMAKE_CURRENT_BINARY_DIR}/gen"
    "${CMAKE_CURRENT_SOURCE_DIR}/${PARSER_SUBDIR}"
)

set (YACC_FILE   "${CMAKE_CURRENT_SOURCE_DIR}/${PARSER_SUBDIR}/parser.y")
set (YACC_FILE_C "${CMAKE_CURRENT_BINARY_DIR}/gen/parser.tab.c")
set (YACC_FILE_H "${CMAKE_CURRENT_BINARY_DIR}/gen/parser.tab.h")

set (LEX_FILE   "${CMAKE_CURRENT_SOURCE_DIR}/${PARSER_SUBDIR}/lexer.l")
set (LEX_FILE_C "${CMAKE_CURRENT_BINARY_DIR}/gen/lexer.c")
set (LEX_FILE_H "${CMAKE_CURRENT_BINARY_DIR}/gen/lexer.h")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen")

ADD_CUSTOM_COMMAND(
    OUTPUT "${YACC_FILE_C}"
           "${YACC_FILE_H}"
    DEPENDS "${YACC_FILE}"
    COMMAND "${BISON_EXECUTABLE}"
    -d
    --report=all
    --report-file="${CMAKE_CURRENT_BINARY_DIR}/gen/bison_report"
    -Wall
    -Wyacc
    --output="${YACC_FILE_C}"
    "${YACC_FILE}"
    COMMENT "Running GNU Bison"
)

ADD_CUSTOM_COMMAND(
    OUTPUT "${LEX_FILE_C}"
           "${LEX_FILE_H}"
    DEPENDS "${LEX_FILE}"
    COMMAND flex
    -f
    --outfile="${LEX_FILE_C}"
    --header-file="${LEX_FILE_H}"
    "${LEX_FILE}"
    COMMENT "Running GNU Flex"
)

################################################################################

add_executable(somekindaparser
    main.c
    "${YACC_FILE_C}"
    "${LEX_FILE_C}"
)

add_library(bstring OBJECT
    contrib/bstring/additions.c
    contrib/bstring/b_list.c
    contrib/bstring/bstrlib.c
)
add_library(util OBJECT
    util/util.c
    util/generic_list.c
    util/linked_list.c
)

################################################################################
# Libraries

if (MINGW)
    target_link_libraries(somekindaparser
        -lstdc++
    )
else()
    target_link_libraries(somekindaparser
        -lrt -pthread
        ${LIBEV_LIBRARIES}
    )
endif()

target_link_libraries(somekindaparser
    ${CMAKE_THREAD_LIBS_INIT}
    ${TALLOC_LIBRARIES}
    bstring
    util
)
