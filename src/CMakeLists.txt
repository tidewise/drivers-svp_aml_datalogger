rock_library(svp_aml_datalogger
    SOURCES Driver.cpp Measurements.cpp CLIMode.cpp Configuration.cpp Columns.cpp
    HEADERS Driver.hpp Measurements.hpp CLIMode.hpp Configuration.hpp Exceptions.hpp
            Columns.hpp
    DEPS_PKGCONFIG iodrivers_base)

rock_executable(svp_aml_datalogger_bin Main.cpp
    DEPS svp_aml_datalogger)
