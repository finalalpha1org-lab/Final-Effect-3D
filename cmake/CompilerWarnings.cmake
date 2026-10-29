# Shared compiler-warning policy for all Final Effect 3D targets.
# Usage:  fe3d_apply_warnings(<target>)
set(FE3D_MSVC_WARNINGS
    /W4
    /permissive-
    /Zc:__cplusplus
    /Zc:preprocessor
    /utf-8
    /MP)

set(FE3D_GNU_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wcast-align
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wdouble-promotion
    -Wnull-dereference
    -Wformat=2
    -Wundef
    -Wmissing-declarations)

function(fe3d_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE ${FE3D_MSVC_WARNINGS})
        if(FE3D_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE ${FE3D_GNU_WARNINGS})
        if(FE3D_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
