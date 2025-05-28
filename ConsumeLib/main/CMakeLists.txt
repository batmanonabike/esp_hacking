idf_component_register(
    SRCS "consume_lib_main.c"
    INCLUDE_DIRS "."
    REQUIRES "ProvideLib"  # This tells the build system that we depend on ProvideLib
)
