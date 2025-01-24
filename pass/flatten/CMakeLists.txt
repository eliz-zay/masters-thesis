add_library(FlattenPass MODULE Flatten.cpp)

target_link_libraries(FlattenPass PRIVATE BaseAnnotatedPass)

set_target_properties(FlattenPass PROPERTIES
    COMPILE_FLAGS "-fno-rtti -std=c++20"
)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(FlattenPass PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)