#ifndef DRIVERS_SVP_AML_DATALOGGER_DRIVER_HPP
#define DRIVERS_SVP_AML_DATALOGGER_DRIVER_HPP

#include <iodrivers_base/Driver.hpp>
#include <svp_aml_datalogger/Configuration.hpp>
#include <svp_aml_datalogger/Exceptions.hpp>
#include <svp_aml_datalogger/Measurements.hpp>
#include <svp_aml_datalogger/CLIMode.hpp>

namespace svp_aml_datalogger {
    /**
     * @brief Represents the active operational state of the physical device.
     */
    enum DeviceMode {
        /** Command-Line Interface mode for configuration */
        CLI,
        /** Telemetry streaming mode for real-time measurements */
        MEASURING,
    };

    /**
     * @brief Driver for the AML Sound Velocity Profiler (SVP) datalogger.
     */
    class Driver : public iodrivers_base::Driver {
    public:
        /** Maximum telemetry packet or command response size in bytes */
        static const int MAX_PACKET_SIZE = 512;

        /**
         * @brief Constructor for the Driver.
         *
         * @param configuration Initial device configuration.
         * @param verbose If true, enables detailed debug logging.
         */
        Driver(Configuration const& configuration = Configuration(),
            bool verbose = false);

        /**
         * @brief Configures the physical sensor using the internal m_config settings.
         * @throws CommandRejectedException If any configuration command is rejected.
         */
        void configure();

        /**
         * @brief Configures the physical sensor with custom configuration settings.
         *
         * @param configuration The configuration settings to apply.
         * @throws CommandRejectedException If any configuration command is rejected.
         */
        void configure(Configuration const& configuration);

        /**
         * @brief Low-level packet extraction helper required by iodrivers_base.
         *
         * Identifies the boundaries of individual messages or lines in the stream buffer.
         *
         * @param buffer Pointer to the input stream data buffer.
         * @param buffer_size Number of bytes currently available in the buffer.
         * @return Positive size of the extracted packet, 0 if incomplete, or negative on
         * error.
         */
        int extractPacket(uint8_t const* buffer, size_t buffer_size) const;

        /**
         * @brief Queries the current active configuration from the physical device.
         * @return A Configuration struct loaded with retrieved device parameters.
         */
        Configuration queryConfiguration();

        /**
         * @brief Switches the device into measuring mode and starts telemetry streaming.
         *
         * Queries display metadata, enforces that the sensor's format matches the
         * driver's expectations, and issues the appropriate measurement command.
         *
         * @throws std::runtime_error If format enforcement or handshake fails.
         */
        void startMeasuring();

        /**
         * @brief Halts telemetry streaming and returns the device to CLI mode.
         *
         * Sends stop sequences (CR/LF) to terminate streaming. Confirms that measurement
         * streaming stopped by waiting for the CLI prompt.
         */
        void stopMeasuring();

        /**
         * @brief Reads a single measurement line from the streaming device telemetry.
         *
         * Blocks until a valid telemetry packet is read, stripped, validated against
         * its checksum, and parsed into a Measurements struct.
         *
         * @return the measurements
         */
        Measurements readMeasurements();

        /**
         * @brief Parses a raw, single-line CSV telemetry output into a Measurements
         * struct.
         *
         * PS: does not support AMLx format.
         *
         * @param raw_line The raw telemetry line with or without trailing control
         * characters.
         * @param columns List of expected ordered column headers corresponding to the
         * values.
         * @return the measurements
         * @throws std::runtime_error If parsing, splitting, or formatting checks fail.
         */
        static Measurements parseMeasurementLine(std::string const& raw_line,
            std::vector<MeasuredParameters> const& columns);

        /**
         * @brief Locates, validates, and strips the trailing hex checksum from a
         * telemetry line.
         *
         * @param line The raw payload line with trailing '*XX' checksum.
         * @return The payload string with checksum stripped.
         * @throws ChecksumError If no checksum is found, or if computed and received
         * checksums mismatch.
         */
        static std::string stripAndValidateChecksum(std::string const& line);

        /**
         * @brief Sends a command to the device, appending the required carriage return
         * separator.
         *
         * @param command The raw command string to send.
         * @param timeout Command transmission write timeout.
         */
        void sendCommand(std::string command, base::Time const& timeout);

        /**
         * @brief Sends a command and blocks until the corresponding response block is
         * complete.
         *
         * Consumes output lines until the next command prompt ('>') is reached.
         *
         * @param command The raw command string to send.
         * @param timeout Transaction read/write timeout.
         * @return The accumulated response text.
         * @throws CommandRejectedException If the device reports a CLI error string.
         */
        std::string sendCommandAndWaitForReply(std::string command,
            base::Time const& timeout);

        /**
         * @brief Verifies that the columns returned by the device match configured
         * parameters.
         *
         * @param meta Measurement metadata containing active columns reported by the
         * device.
         * @throws std::runtime_error If there is any mismatch between configured
         * parameters and device output columns.
         */
        void enforceConfiguration(MeasurementMetadata const& meta);

        /**
         * @brief Retrieves the active list of column headers currently mapped in the
         * driver.
         * @return std::vector<MeasuredParameters> The list of column headers.
         */
        std::vector<MeasuredParameters> getMeasurementColumns() const;

        /**
         * @brief Reads data blocks sequentially from the stream until a command prompt is
         * reached.
         *
         * @param timeout Maximum duration to wait before aborting.
         * @return The accumulated read lines.
         * @throws TimeoutError If the timeout is reached before a prompt is encountered.
         */
        std::string readUntilPrompt(base::Time const& timeout);

        /**
         * @brief Queries the sensor and returns a parsed model of all active channels and
         * parameters.
         *
         * @return ParsedDisplaySensors A parsed representation of the sensor
         * configuration.
         */
        ParsedDisplaySensors displaySensorsParsed();

        /**
         * @brief Calls "display options" and returns response text.
         *
         * @return the raw response
         */
        std::string displayOptions();
        /**
         * @brief Calls "display sensors" and returns response text.
         *
         * @return the raw response
         */
        std::string displaySensors();
        /**
         * @brief Calls "display version" and returns response text.
         *
         * @return the raw response
         */
        std::string displayVersion();
        /**
         * @brief Calls "display memory" and returns response text.
         *
         * @return the raw response
         */
        std::string displayMemory();
        /**
         * @brief Calls "display monitor" and returns response text.
         *
         * @return the raw response
         */
        std::string displayMonitor();

    protected:
        /** Active device configuration */
        Configuration m_config;
        /** Verbose logging flag */
        bool m_verbose = false;

        /** Active driver state machine mode */
        DeviceMode m_device_mode = CLI;
        /** Columns header mapping currently in use for telemetry decoding */
        std::vector<MeasuredParameters> m_columns;
    };
}

#endif // DRIVERS_SVP_AML_DATALOGGER_DRIVER_HPP
