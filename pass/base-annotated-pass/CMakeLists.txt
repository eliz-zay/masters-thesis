add_library(BaseAnnotatedPass BaseAnnotatedPass.cpp)

target_include_directories(BaseAnnotatedPass PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(BaseAnnotatedPass PROPERTIES
    COMPILE_FLAGS "-fno-rtti -std=c++20"
)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(BaseAnnotatedPass PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)