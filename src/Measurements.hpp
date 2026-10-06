#ifndef DRIVERS_SVP_AML_DATALOGGER_MEASUREMENTS_HPP
#define DRIVERS_SVP_AML_DATALOGGER_MEASUREMENTS_HPP

#include <base/Float.hpp>
#include <base/Pressure.hpp>
#include <base/Time.hpp>
#include <optional>
#include <string>
#include <svp_aml_datalogger/Configuration.hpp>
#include <vector>

namespace svp_aml_datalogger {
    /**
     * @brief Bitfield representing which primary parameters are measured by the device.
     *
     * This is slightly different from {ConfigurableMeasurements} in Configuration.hpp,
     * since it also includes fields that are not configurable (ie Pressure).
     */
    enum MeasuredParameters {
        MEASURE_DATE = 0x01,
        MEASURE_TIME = 0x02,
        MEASURE_BATTERY_VOLTAGE = 0x04,
        MEASURE_BATTERY_CURRENT = 0x08,
        MEASURE_EXTERNAL_VOLTAGE = 0x10,
        MEASURE_EXTERNAL_CURRENT = 0x20,
        MEASURE_CHARGE_STATE = 0x40,
        MEASURE_DEPTH = 0x80,
        MEASURE_SALINITY = 0x100,
        MEASURE_DENSITY = 0x200,
        MEASURE_SOUND_VELOCITY = 0x400,
        MEASURE_PRESSURE = 0x800,
    };

    std::optional<ConfigurableMeasurements> measuredParameterToConfigurableMeasurement(
        MeasuredParameters const& parameter);

    /**
     * Structure representing parsed measurements from the AML SVP datalogger.
     */
    struct Measurements {
        /** Timestamp parsed from Date and Time fields */
        base::Time time;
        /** Battery voltage (V) */
        double battery_voltage = base::unset<double>();
        /** Battery current (A) */
        double battery_current = base::unset<double>();
        /** External voltage (V) */
        double external_voltage = base::unset<double>();
        /** External current (A) */
        double external_current = base::unset<double>();
        /** Charge State string (e.g. "done", "charging") */
        std::string charge_state;
        /** Pressure (Pa) */
        base::Pressure pressure;
        /** Sound velocity (m/s) */
        double sound_velocity = base::unset<double>();
        /** Salinity (PSU) */
        double salinity = base::unset<double>();
        /** Density (kg/m3) */
        double density = base::unset<double>();
        /** Depth (m) */
        double depth = base::unset<double>();
    };

    /**
     * @brief Structure representing metadata for the connected sensors list.
     */
    struct SensorMetadata {
        /** The names of the columns defining the sensor parameters */
        std::vector<std::string> columns;
    };

    /**
     * @brief Detailed information of an individual sensor configuration.
     */
    struct SensorData {
        /** Port ID or slot number where the sensor is mounted */
        std::string port;
        /** Sensor manufacturer model name or identifier */
        std::string model;
        /** Unique manufacturer serial number of the sensor */
        std::string serial_number;
        /** Embedded firmware version installed on the sensor */
        std::string firmware;
        /** Physical parameter measured by this sensor (e.g. Sound Velocity, Pressure) */
        std::string parameter;
        /** Measurement units associated with the parameter (e.g. m/s, dbar) */
        std::string units;
        /** Date when the sensor was last calibrated (YYYY-MM-DD) */
        std::string calibration_date;
        /** Time when the sensor was last calibrated (HH:MM:SS) */
        std::string calibration_time;
        /** Calibration accuracy specification */
        std::string accuracy;
        /** Minimum range value supported by this sensor */
        std::string range_min;
        /** Maximum range value supported by this sensor */
        std::string range_max;
    };

    /**
     * @brief Metadata definitions for telemetry measurements.
     */
    struct MeasurementMetadata {
        /** The columns in the standard telemetry line represented as enum parameters */
        std::vector<MeasuredParameters> columns;
        /** Corresponding measurement units for each telemetry column */
        std::vector<std::string> units;
    };

    /**
     * @brief Combined collection representing the fully parsed display sensors
     * configuration.
     */
    struct ParsedDisplaySensors {
        /** Layout columns for the physical sensors */
        SensorMetadata sensor_metadata;
        /** Specifications for each individual sensor */
        std::vector<SensorData> sensor_data;
        /** Layout columns and units for standard telemetry output */
        MeasurementMetadata measurement_metadata;
    };

    /**
     * @brief Parses the raw output of the 'display sensors' command.
     *
     * Parses the multi-section response containing [SensorMetaData], [SensorData],
     * and [MeasurementMetadata] blocks.
     *
     * @param raw_text The raw multiline string response from the device.
     * @return Fully structured ParsedDisplaySensors object.
     */
    ParsedDisplaySensors parseDisplaySensors(std::string const& raw_text);

    /**
     * @brief Parses a sensor metadata header line.
     *
     * @param line A single metadata string line, e.g., "Columns=Port,Model,..."
     * @return Structured SensorMetadata containing individual column names.
     */
    SensorMetadata parseSensorMetadata(std::string const& line);

    /**
     * @brief Parses a comma-separated row of sensor characteristics.
     *
     * @param line A single sensor configuration CSV line.
     * @return Structured SensorData containing mapped characteristics.
     * @throws std::runtime_error If the line has fewer than 11 fields.
     */
    SensorData parseSensorData(std::string const& line);

    /**
     * @brief Parses measurement metadata block lines.
     *
     * @param lines A vector of individual strings representing the metadata lines.
     * @return Structured MeasurementMetadata containing telemetry schema and units.
     */
    MeasurementMetadata parseMeasurementMetadata(std::vector<std::string> const& lines);

    /**
     * @brief Translates a protocol column string name to a MeasuredParameters enum.
     *
     * @param name The column string name (e.g. "Date", "SV", "Vbat").
     * @return The corresponding MeasuredParameters enum value.
     * @throws std::runtime_error If the column string name is unrecognized.
     */
    MeasuredParameters stringToMeasuredParameter(std::string const& name);

    /**
     * @brief Translates a MeasuredParameters enum to its protocol column string
     * representation.
     *
     * @param param The MeasuredParameters enum value.
     * @return The corresponding column string representation.
     */
    std::string measuredParameterToString(MeasuredParameters param);

    /**
     * @brief Trims leading and trailing whitespaces and newlines from a string.
     *
     * @param data The input string to trim.
     * @return The trimmed string.
     */
    std::string trim(std::string const& data);

    /**
     * @brief Utility helper to check if a specific column is in a column name list.
     *
     * @param cols List of available column names.
     * @param name Name of the target column to find.
     * @return True if the column is present, false otherwise.
     */
    bool hasColumn(std::vector<std::string> const& cols, std::string const& name);

    /**
     * @brief Utility helper to check if a specific measured parameter is in a parameter
     * list.
     *
     * @param cols List of available measured parameters.
     * @param param The target parameter to find.
     * @return True if the parameter is present, false otherwise.
     */
    bool hasColumn(std::vector<MeasuredParameters> const& cols,
        MeasuredParameters const& param);
}

#endif // DRIVERS_SVP_AML_DATALOGGER_MEASUREMENTS_HPP
