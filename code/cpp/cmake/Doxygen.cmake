# Doxygen.cmake - Documentation generation module

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")

    set(DOXYGEN_PROJECT_NAME "Actisense SDK")
    set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF "Modern C++17 SDK for Actisense device communication")

    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_MAN NO)

    # Input settings
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE NO)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_RECURSIVE YES)

    # Source browser
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_INLINE_SOURCES NO)

    # Diagrams
    set(DOXYGEN_HAVE_DOT NO)
    set(DOXYGEN_CLASS_DIAGRAMS YES)
    set(DOXYGEN_COLLABORATION_GRAPH NO)

    # Warnings
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)

    # HTML styling
    set(DOXYGEN_HTML_COLORSTYLE_HUE 220)
    set(DOXYGEN_HTML_COLORSTYLE_SAT 100)
    set(DOXYGEN_HTML_COLORSTYLE_GAMMA 80)

    # Preprocessing
    set(DOXYGEN_ENABLE_PREPROCESSING YES)
    set(DOXYGEN_MACRO_EXPANSION YES)
    set(DOXYGEN_EXPAND_ONLY_PREDEF NO)

    # Use \brief, \param style
    set(DOXYGEN_JAVADOC_AUTOBRIEF NO)
    set(DOXYGEN_QT_AUTOBRIEF NO)

    doxygen_add_docs(docs
        "${CMAKE_CURRENT_SOURCE_DIR}/src/public"
        COMMENT "Generating API documentation with Doxygen"
    )

    message(STATUS "Documentation target 'docs' available (run: cmake --build . --target docs)")
else()
    message(STATUS "Doxygen not found - documentation target not available")
endif()
