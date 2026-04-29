# micropython.cmake
add_library(spi_displays INTERFACE)
target_sources(spi_displays INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modspidisplay.c
    ${CMAKE_CURRENT_LIST_DIR}/display.c
    ${CMAKE_CURRENT_LIST_DIR}/fonts.c
    ${CMAKE_CURRENT_LIST_DIR}/draw.c
    ${CMAKE_CURRENT_LIST_DIR}/st7735.c
    ${CMAKE_CURRENT_LIST_DIR}/st7789.c
)
target_include_directories(spi_displays INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(spi_displays INTERFACE MICROPY_PY_SPI_DISPLAYS=1)
target_link_libraries(usermod INTERFACE spi_displays)
