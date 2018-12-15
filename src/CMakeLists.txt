set (DECOMP_SUBDIR "from_xml")
set (COMP_SUBDIR   "to_xml")

set (COMP_YACC_FILE   "${CMAKE_CURRENT_SOURCE_DIR}/${COMP_SUBDIR}/parser.y")
set (COMP_YACC_FILE_C "${CMAKE_CURRENT_BINARY_DIR}/gen/parser.tab.c")
set (COMP_YACC_FILE_H "${CMAKE_CURRENT_BINARY_DIR}/gen/parser.tab.h")

set (COMP_LEX_FILE   "${CMAKE_CURRENT_SOURCE_DIR}/${COMP_SUBDIR}/lexer.l")
set (COMP_LEX_FILE_C "${CMAKE_CURRENT_BINARY_DIR}/gen/lexer.c")
set (COMP_LEX_FILE_H "${CMAKE_CURRENT_BINARY_DIR}/gen/lexer.h")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen")

ADD_CUSTOM_COMMAND(
    OUTPUT "${COMP_YACC_FILE_C}"
           "${COMP_YACC_FILE_H}"
    DEPENDS "${COMP_YACC_FILE}"
    COMMAND "${BISON_EXECUTABLE}"
    -d
    --report=all
    --report-file="${CMAKE_CURRENT_BINARY_DIR}/gen/bison_report"
    -Wall
    -Wyacc
    --output="${COMP_YACC_FILE_C}"
    "${COMP_YACC_FILE}"
    COMMENT "Running GNU Bison"
)

ADD_CUSTOM_COMMAND(
    OUTPUT "${COMP_LEX_FILE_C}"
           "${COMP_LEX_FILE_H}"
    DEPENDS "${COMP_LEX_FILE}"
    COMMAND flex
    -f
    --outfile="${COMP_LEX_FILE_C}"
    --header-file="${COMP_LEX_FILE_H}"
    "${COMP_LEX_FILE}"
    COMMENT "Running GNU Flex"
)

################################################################################

add_library(bstring OBJECT
    contrib/bstring/bstring/additions.c
    contrib/bstring/bstring/b_list.c
    contrib/bstring/bstring/bstrlib.c
)

add_library(x4c-util OBJECT
    contrib/contrib.c
    util/find.c
    util/generic_list.c
    util/linked_list.c
    util/util.c
)

if (MINGW)
    include_directories("contrib/talloc")
    set (TALLOC_SRC "contrib/talloc/talloc.c")
endif()

add_executable(x4c 
    main.c
    from_xml/element_handler.c
    from_xml/decomp_main.c
    from_xml/decompile.c
    from_xml/xs_context.c
    from_xml/util.c
    to_xml/ast.c
    to_xml/comp_main.c
    "${COMP_YACC_FILE_C}"
    "${COMP_LEX_FILE_C}"
    ${TALLOC_SRC}
)

include_directories(
    "${COMP_SUBDIR}"
    "${DECOMP_SUBDIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/gen"
)

################################################################################
# Libraries

if (MINGW)
    target_link_libraries(x4c
        -lstdc++
    )
else()
    target_link_libraries(x4c
        -lrt -pthread -lunwind
    )
endif()

target_link_libraries(x4c
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXPAT_LIBRARIES}
    bstring
    x4c-util
    -lncurses
)
if (NOT MINGW)
    target_link_libraries(x4c ${TALLOC_LIBRARIES})
endif()

if (USE_JEMALLOC)
   target_link_libraries(x4c -ljemalloc -ldl)
endif()
