#ifndef DRIVERS_SVP_AML_DATALOGGER_COLUMNS_HPP
#define DRIVERS_SVP_AML_DATALOGGER_COLUMNS_HPP

#include <base/Float.hpp>
#include <base/Time.hpp>
#include <svp_aml_datalogger/Measurements.hpp>

namespace svp_aml_datalogger {
    /**
     * @brief Parses a string to a double-precision floating-point number.
     *
     * Trims leading/trailing whitespaces and validates that the entire string
     * represents a valid numeric value with no trailing non-numeric characters.
     *
     * @param data The input string representing a numeric value.
     * @return The parsed double value.
     * @throws std::runtime_error If the string is empty, cannot be parsed, or contains extra characters.
     */
    double parseDouble(std::string const& data);

    /**
     * @brief Parses date and time string fields into a base::Time timestamp.
     *
     * Expected date format: "YYYY-MM-DD"
     * Expected time format: "HH:MM:SS.SS..." (with optional subsecond decimals)
     *
     * @param date_str The date string field to parse.
     * @param time_data The time string field to parse.
     * @return A base::Time object representing the combined date and time.
     * @throws std::runtime_error If formats are invalid or values are out of bounds.
     */
    base::Time parseTime(std::string date_str, std::string time_data);

    /**
     * @brief Parses the charge state field to a string.
     *
     * @param data The input charge state string (e.g. "done" or "charging").
     * @return The parsed charge state string.
     * @throws std::runtime_error If the input string is empty.
     */
    std::string parseChargeState(std::string data);

    /**
     * @brief Parses raw data fields according to the corresponding header columns.
     *
     * Iterates through columns list, maps each column header (like "Vbat", "Pressure")
     * to the corresponding field index, parses the value, and populates the Measurements struct.
     *
     * @param fields The list of raw split telemetry string values.
     * @param columns The corresponding ordered list of column headers from metadata.
     * @return The populated Measurements struct.
     * @throws std::runtime_error If parsing fails or field lists are mismatched.
     */
    Measurements parseColumns(std::vector<std::string> const& fields,
        std::vector<MeasuredParameters> const& columns);
}

#endif // DRIVERS_SVP_AML_DATALOGGER_COLUMNS_HPP
