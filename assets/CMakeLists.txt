# Make sure the user is not executing this script directly
if(NOT InOpenJK)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InOpenJK)

# Generate PK3 file
set(PK3FILE GalaxyRP.pk3)
set(PK3PATH ${CMAKE_SOURCE_DIR}/assets/client)
set(TMPFILE ${CMAKE_CURRENT_BINARY_DIR}/${PK3FILE})
set(OUTFILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${PK3FILE})

set(PK3FILE_2 GalaxyRP-Extras.pk3)
set(PK3PATH_2 ${CMAKE_SOURCE_DIR}/assets/client-extras)
set(TMPFILE_2 ${CMAKE_CURRENT_BINARY_DIR}/${PK3FILE_2})
set(OUTFILE_2 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${PK3FILE_2})

file(GLOB PK3ASSETS "${PK3PATH}/*")
file(GLOB PK3ASSETS_2 "${PK3PATH_2}/*")

include(InstallZIP)
add_zip_command(${TMPFILE} ${OUTFILE} DIR "${PK3PATH}" FILES "${PK3ASSETS}" )
add_zip_command(${TMPFILE_2} ${OUTFILE_2} DIR "${PK3PATH_2}" FILES "${PK3ASSETS_2}" )

# Finish and cleanup
add_custom_target(assets DEPENDS "${TMPFILE}")
add_custom_command(TARGET assets POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove -f "${TMPFILE}"
)
add_custom_target(assets-extras DEPENDS "${TMPFILE_2}")
add_custom_command(TARGET assets-extras POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove -f "${TMPFILE_2}"
)
