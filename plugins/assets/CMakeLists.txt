# assets plugin: asynchronous mission-based asset loading and memory management.
# Output: assets.dll / assets.so / assets.dylib

crayon_add_plugin(assets
    SOURCES
        src/assets_plugin.cpp
        src/bundle_manager.cpp
    INCLUDES
        src
)
