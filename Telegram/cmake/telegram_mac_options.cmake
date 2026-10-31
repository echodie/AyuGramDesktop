set(AYUGRAM_MAC_BUNDLE_ID "" CACHE STRING "macOS bundle identifier (empty uses the default).")
set(AYUGRAM_MAC_APP_NAME "AyuGram" CACHE STRING "macOS application name.")

if (NOT AYUGRAM_MAC_BUNDLE_ID STREQUAL ""
    AND NOT AYUGRAM_MAC_BUNDLE_ID MATCHES "^[A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)+$")
    message(FATAL_ERROR "invalid macOS bundle identifier")
endif()
if (NOT AYUGRAM_MAC_APP_NAME MATCHES "^[A-Za-z0-9]([A-Za-z0-9 _-]*[A-Za-z0-9_-])?$")
    message(FATAL_ERROR "invalid macOS application name: use letters, digits, spaces, underscores or hyphens")
endif()
