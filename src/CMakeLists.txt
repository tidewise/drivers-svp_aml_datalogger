rock_library(svp_aml_datalogger
    SOURCES Dummy.cpp
    HEADERS Dummy.hpp)

rock_executable(svp_aml_datalogger_bin Main.cpp
    DEPS svp_aml_datalogger)

