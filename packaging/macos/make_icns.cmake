# Builds a macOS .icns from a square PNG with sips and iconutil.
#   cmake -DSOURCE=icon.png -DOUTPUT=EyeTimer.icns -P make_icns.cmake
get_filename_component(_out_dir "${OUTPUT}" DIRECTORY)
set(_iconset "${_out_dir}/EyeTimer.iconset")
file(REMOVE_RECURSE "${_iconset}")
file(MAKE_DIRECTORY "${_iconset}")

foreach(_size 16 32 128 256 512)
    math(EXPR _double "${_size} * 2")
    execute_process(COMMAND sips -z ${_size} ${_size} "${SOURCE}" --out "${_iconset}/icon_${_size}x${_size}.png"
                    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND sips -z ${_double} ${_double} "${SOURCE}" --out "${_iconset}/icon_${_size}x${_size}@2x.png"
                    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
endforeach()

execute_process(COMMAND iconutil -c icns "${_iconset}" -o "${OUTPUT}" COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE_RECURSE "${_iconset}")
