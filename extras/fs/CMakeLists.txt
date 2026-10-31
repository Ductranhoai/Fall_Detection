# idf_component_register(
#     SRCS "fs.c"
#     INCLUDE_DIRS "."
#     REQUIRES spiffs
# )

idf_component_register(
    SRCS "fs.c"
    INCLUDE_DIRS "."
    REQUIRES fatfs wear_levelling
)