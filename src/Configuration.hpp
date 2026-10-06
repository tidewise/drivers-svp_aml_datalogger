#ifndef DRIVERS_SVP_AML_DATALOGGER_CONFIGURATION_HPP
#define DRIVERS_SVP_AML_DATALOGGER_CONFIGURATION_HPP

#include <base/Pressure.hpp>
#include <base/Time.hpp>
#include <iodrivers_base/Driver.hpp>
#include <vector>

namespace svp_aml_datalogger {
    /**
     * @brief Mode describing how the device will perform sampling operations.
     */
    enum SamplingMode {
        /** Perform continuous single-point samples */
        SINGLE,
        /** Perform scheduled bursts of samples */
        BURST
    };

    /**
     * @brief Mode describing how burst telemetry should be formatted/reported.
     */
    enum BurstMonitor {
        /** Display all individual samples taken in the burst */
        SAMPLES,
        /** Display only the mathematically averaged value of all samples in the burst */
        AVERAGE
    };

    /**
     * @brief Detailed parameters governing BURST mode sampling.
     */
    struct BurstModeParameters {
        /** Number of samples taken per burst (min 2, max 1200) */
        uint16_t samples_per_burst = 0;
        /** Interval between burst cycles (min 5s, max 14400s) */
        base::Time burst_interval;
        /** Output display mode (all samples or average) */
        BurstMonitor monitor = SAMPLES;
    };

    /**
     * @brief Enumeration of trigger types that initiate a single sampling operation.
     */
    enum SamplingTrigger {
        /** Sample periodically based on time intervals */
        BY_TIME,
        /** Sample whenever a specified sound velocity increment is exceeded */
        BY_SOUND_VELOCITY_INCREMENT,
        /** Sample whenever a specified pressure increment is exceeded */
        BY_PRESSURE_INCREMENT
    };

    /**
     * @brief Parameters governing how sampling triggers behave.
     */
    struct SamplingTriggerParameters {
        /** Trigger mode (defaults to periodic time-based triggers) */
        SamplingTrigger mode = BY_TIME;
        /** Periodic sample rate (samples per second), max 20Hz */
        float samples_per_s = 0;
        /** Target sound velocity increment threshold for BY_SOUND_VELOCITY_INCREMENT */
        float sound_velocity_increment = 0;
        /** Target pressure increment threshold for BY_PRESSURE_INCREMENT */
        base::Pressure pressure_increment;
    };

    enum ConfigurableMeasurements {
        DATE = 0x01,
        TIME = 0x02,
        BATTERY_VOLTAGE = 0x04,
        BATTERY_CURRENT = 0x08,
        EXTERNAL_VOLTAGE = 0x10,
        EXTERNAL_CURRENT = 0x20,
        CHARGE_STATE = 0x40,
        DEPTH = 0x80,
        SALINITY = 0x100,
        DENSITY = 0x200,
        SOUND_VELOCITY = 0x400,
    };

    /**
     * @brief Bitfield for virtual parameters computed on-board by the sensor.
     */
    enum DerivedParameters {
        /** Calculate water depth from hydrostatic pressure */
        DEPTH_FROM_PRESSURE = 0x01,
        /** Calculate salinity from conductivity measurements */
        SALINITY_FROM_CONDUCTIVITY = 0x02,
        /** Calculate salinity from measured sound velocity */
        SALINITY_FROM_SOUND_VELOCITY = 0x04,
        /** Calculate density from salinity */
        DENSITY_FROM_SALINITY = 0x08,
        /** Calculate sound velocity from salinity */
        SOUND_VELOCITY_FROM_SALINITY = 0x10
    };

    /**
     * @brief Enumeration of supported telemetry display formats.
     */
    enum MonitorFormat {
        /**
         * CSV-like output format that is human-readable.
         * Units are statically consulted by measurement metadata.
         *
         * Example:
         * [MeasurementData]
         * 2026-09-16,11:26:01.10,8.11,0.25346,1499.773,0.25183
         */
        COLUMNS,
        /**
         * AML proprietary format that embeds units directly in the packet.
         * Currently unsupported by this driver.
         */
        AMLX
    };

    /**
     * @brief Parameters governing telemetry reporting and monitor stream settings.
     */
    struct MonitoringParameters {
        /** Expected output telemetry format */
        MonitorFormat monitor_format = COLUMNS;
        /**
         * When enabled, stream is halted robustly via 3 carriage returns instead of 1.
         */
        bool robust = true;
        /**
         * Automatic streaming start interval after inactivity.
         * Set to 0 to disable. Minimum non-zero value is 5s.
         */
        base::Time auto_start_after_inactivity;
    };

    /**
     * @brief Comprehensive configuration structure for the AML SVP device.
     */
    struct Configuration {
        /** Active sampling mode (continuous single vs burst) */
        SamplingMode sampling_mode = SINGLE;
        /** Parameters for trigger mode setting */
        SamplingTriggerParameters sampling_trigger;
        /** Parameters for burst mode scheduling */
        BurstModeParameters burst_parameters;

        /** @meta bitfield /svp_aml_datalogger/MeasuredParameters */
        uint16_t measured_parameters = 0;
        /** @meta bitfield /svp_aml_datalogger/DerivedParameters */
        uint8_t derived_parameters = 0;

        /** Monitor streaming and interface formats */
        MonitoringParameters monitor_parameters;

        /**
         * @brief Validates the consistency and limits of the configured parameters.
         * @throws ConfigurationError If any boundary constraint or mode compatibility is
         * violated.
         */
        void validate() const;

        /**
         * @brief Generates the list of hardware commands required to apply this
         * configuration.
         * @return std::vector<std::string> The list of command strings.
         */
        std::vector<std::string> configurationCommands() const;
    };

    /**
     * @brief Parses the raw display options text response into a Configuration struct.
     *
     * @param raw_text The raw text returned from executing a "display options" command.
     * @return The populated Configuration struct.
     */
    Configuration parseDisplayOptions(std::string const& raw_text);
}

#endif // DRIVERS_SVP_AML_DATALOGGER_CONFIGURATION_HPP
