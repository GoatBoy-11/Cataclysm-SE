if (RELEASE)
    # Turn the unbundled mod list into install() exclusions.  PATTERN matches the
    # last path component, so naming the directory drops it and everything under it.
    set(CSE_MOD_EXCLUDES)
    foreach(_cse_mod IN LISTS CSE_UNBUNDLED_MODS)
        list(APPEND CSE_MOD_EXCLUDES PATTERN "${_cse_mod}" EXCLUDE)
    endforeach()

    if (USE_PREFIX_DATA_DIR)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                TYPE DATA
                PATTERN "CMakeLists.txt" EXCLUDE
                PATTERN "XDG" EXCLUDE
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE
                # Bright Nights' release history.  Nothing reads it, and it is not
                # CSE's changelog, so it has no business in a CSE package.
                PATTERN "changelog.txt" EXCLUDE
                ${CSE_SOURCE_ART_EXCLUDES}
                ${CSE_MOD_EXCLUDES})
    else()
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                DESTINATION data
                PATTERN "CMakeLists.txt" EXCLUDE
                PATTERN "XDG" EXCLUDE
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE
                # Bright Nights' release history.  Nothing reads it, and it is not
                # CSE's changelog, so it has no business in a CSE package.
                PATTERN "changelog.txt" EXCLUDE
                ${CSE_SOURCE_ART_EXCLUDES}
                ${CSE_MOD_EXCLUDES})
    endif()
endif ()
