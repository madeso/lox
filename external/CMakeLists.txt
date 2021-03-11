add_subdirectory(fmt-7.1.1)
add_subdirectory(catchy)

###################################################################################################
# catch
add_library(catch INTERFACE)
target_include_directories(catch
    INTERFACE
        catch
)
