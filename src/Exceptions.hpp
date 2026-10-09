#ifndef DRIVERS_SVP_AML_DATALOGGER_EXCEPTIONS_HPP
#define DRIVERS_SVP_AML_DATALOGGER_EXCEPTIONS_HPP

#include <iodrivers_base/Driver.hpp>
#include <stdexcept>

namespace svp_aml_datalogger {
    /**
     * @brief Exception thrown when a device configuration validation fails.
     */
    class ConfigurationError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /**
     * @brief Exception thrown when a telemetry line's checksum fails to validate.
     */
    class ChecksumError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    /**
     * @brief Exception thrown when the physical device rejects a command.
     */
    class CommandRejectedException : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}

#endif // DRIVERS_SVP_AML_DATALOGGER_EXCEPTIONS_HPP
