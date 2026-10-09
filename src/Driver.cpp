#include "Measurements.hpp"
#include "base-logging/logging/logging_printf_style.h"
#include <base-logging/Logging.hpp>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iodrivers_base/Exceptions.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <svp_aml_datalogger/Columns.hpp>
#include <svp_aml_datalogger/Driver.hpp>
#include <unistd.h>

using namespace base;
using namespace svp_aml_datalogger;

Driver::Driver(Configuration const& configuration, bool verbose)
    : iodrivers_base::Driver(MAX_PACKET_SIZE)
    , m_config(configuration)
    , m_verbose(verbose)
{
    if (m_verbose) {
        LOG_CONFIGURE(DEBUG, stdout);
    }
}

void Driver::configure()
{
    configure(m_config);
}

void Driver::configure(Configuration const& configuration)
{
    m_config = configuration;
    try {
        auto commands = m_config.configurationCommands();
        LOG_DEBUG_S << "Configuring device with " << commands.size() << " commands...";
        for (auto cmd : commands) {
            LOG_DEBUG_S << "Sending: " << cmd;
            try {
                sendCommand(cmd, m_write_timeout);
                readUntilPrompt(m_read_timeout);
            }
            catch (CommandRejectedException const& e) {
                std::ostringstream msg;
                msg << cmd
                    << " was rejected with the following error message: " << e.what();
                throw CommandRejectedException(msg.str());
            }
        }
    }
    catch (std::exception const& e) {
        LOG_ERROR_S << "Error when trying to apply configuration: " << e.what();
        throw;
    }
}

void Driver::startMeasuring()
{
    if (m_device_mode == MEASURING) {
        LOG_WARN_S
            << "trying to start measure mode but the device mode is already measuring";
        return;
    }

    ParsedDisplaySensors parsed = displaySensorsParsed();
    LOG_DEBUG_S << "Enforcing configuration...";
    enforceConfiguration(parsed.measurement_metadata);

    // Send the measuring command after reading and verifying metadata
    std::string measuring_command =
        m_config.monitor_parameters.monitor_format == MonitorFormat::COLUMNS ? "monitor"
                                                                             : "mmonitor";

    LOG_DEBUG_S << "Starting measuring with command: " << measuring_command;
    sendCommandAndWaitForReply(measuring_command, m_write_timeout);

    m_columns = parsed.measurement_metadata.columns;
    m_device_mode = MEASURING;
}

void Driver::stopMeasuring()
{
    if (m_device_mode == CLI) {
        LOG_WARN_S << "trying to stop measure mode but the device mode is not measuring";
        return;
    }
    size_t number_of_carriage_returns = m_config.monitor_parameters.robust ? 3 : 1;
    std::ostringstream full_cmd;
    LOG_DEBUG_S << "Stopping measure mode ...";
    for (size_t i = 0; i < number_of_carriage_returns; i++) {
        full_cmd << "\r\n";
    }
    // Do not use #sendCommand because it automatically adds the carriage return
    writePacket(reinterpret_cast<uint8_t const*>(full_cmd.str().c_str()),
        full_cmd.str().size(),
        m_write_timeout);
    readUntilPrompt(m_read_timeout);

    m_device_mode = CLI;
    m_columns.clear();
}

void Driver::enforceConfiguration(MeasurementMetadata const& meta)
{
    if (meta.columns.empty()) {
        throw std::runtime_error("Received empty columns list from device metadata");
    }

    uint16_t metadata_mask = 0;
    for (MeasuredParameters param : meta.columns) {
        auto configurable_measure_param =
            measuredParameterToConfigurableMeasurement(param);
        if (configurable_measure_param.has_value()) {
            metadata_mask |= *configurable_measure_param;
        }
    }

    if (metadata_mask != m_config.measured_parameters) {
        throw std::runtime_error("Format enforcement failed: device output parameters do "
                                 "not match configured parameters");
    }
}

void Driver::sendCommand(std::string command, Time const& timeout)
{
    std::string full_cmd = command + '\r';
    writePacket(reinterpret_cast<uint8_t const*>(full_cmd.c_str()),
        full_cmd.size(),
        timeout);
}

std::string Driver::sendCommandAndWaitForReply(std::string command,
    const base::Time& timeout)
{
    try {
        sendCommand(command, m_write_timeout);
        return readUntilPrompt(m_read_timeout);
    }
    catch (CommandRejectedException const& e) {
        std::ostringstream msg;
        msg << command << " was rejected with the following error message: " << e.what();
        throw CommandRejectedException(msg.str());
    }
}

int Driver::extractPacket(uint8_t const* buffer, size_t buffer_size) const
{
    for (size_t i = 0; i < buffer_size; ++i) {
        if (buffer[i] == '\n' || buffer[i] == PROMPT) {
            return i + 1;
        }
    }
    if (buffer_size >= MAX_PACKET_SIZE) {
        return -1;
    }
    return 0;
}

Measurements Driver::readMeasurements()
{
    uint8_t buffer[MAX_PACKET_SIZE];
    while (true) {
        int packet_size = readPacket(buffer, MAX_PACKET_SIZE);
        if (packet_size <= 0) {
            continue;
        }
        std::string line(reinterpret_cast<char const*>(buffer), packet_size);
        try {
            return parseMeasurementLine(line, m_columns);
        }
        catch (ChecksumError const& e) {
            throw;
        }
        catch (std::exception const& e) {
            // Non-measurement line, such as a header or configuration.
            // Continue reading standard telemetry lines.
        }
    }
}

std::string Driver::stripAndValidateChecksum(std::string const& line)
{
    size_t star_pos = line.find_last_of('*');
    if (star_pos == std::string::npos) {
        throw ChecksumError("Checksum delimiter '*' not found in line: " + line);
    }

    std::string payload_str = line.substr(0, star_pos);
    std::string checksum_str = trim(line.substr(star_pos + 1));

    if (checksum_str.empty()) {
        throw ChecksumError("Empty checksum in line: " + line);
    }

    uint8_t calculated_checksum = 0;
    for (char c : payload_str) {
        calculated_checksum ^= static_cast<uint8_t>(c);
    }

    unsigned int received_checksum = 0;
    char extra;
    if (std::sscanf(checksum_str.c_str(), "%x%c", &received_checksum, &extra) != 1) {
        throw ChecksumError("Invalid checksum format: " + checksum_str);
    }

    if (calculated_checksum != static_cast<uint8_t>(received_checksum)) {
        std::ostringstream err;
        err << "Checksum mismatch: calculated 0x" << std::hex << std::uppercase
            << static_cast<int>(calculated_checksum) << ", received 0x" << std::hex
            << std::uppercase << received_checksum;
        throw ChecksumError(err.str());
    }
    return payload_str;
}

Measurements Driver::parseMeasurementLine(std::string const& raw_line,
    std::vector<MeasuredParameters> const& columns)
{
    std::string line = raw_line;
    // Strip \r and \n from the line. These characters can only be at the end of the line,
    // due to how extractPacket work
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }

    if (trim(line).empty()) {
        throw std::runtime_error("Empty line");
    }

    auto payload_str = stripAndValidateChecksum(line);

    std::vector<std::string> fields;
    std::string field;
    std::istringstream tokenStream(payload_str);
    while (std::getline(tokenStream, field, ',')) {
        fields.push_back(trim(field));
    }

    if (fields.size() < columns.size()) {
        throw std::runtime_error(
            "Invalid number of fields: " + std::to_string(fields.size()) +
            ", expected at least " + std::to_string(columns.size()));
    }

    return parseColumns(fields, columns);
}

std::vector<MeasuredParameters> Driver::getMeasurementColumns() const
{
    return m_columns;
}

std::string Driver::readUntilPrompt(Time const& timeout)
{
    Time deadline = Time::now() + timeout;
    uint8_t buffer[MAX_PACKET_SIZE];
    std::string accumulated;

    while (Time::now() < deadline) {
        try {
            int bytes_read =
                readPacket(buffer, MAX_PACKET_SIZE, Time::fromMilliseconds(1000));
            if (bytes_read <= 0) {
                continue;
            }
            std::string line(reinterpret_cast<char const*>(buffer), bytes_read);
            LOG_DEBUG_S << "readUntilPrompt read packet: " << trim(line);
            accumulated += line;
            std::string trimmed = trim(line);

            if (trimmed.find("Error") != std::string::npos) {
                throw CommandRejectedException(trimmed);
            }
            if (isPrompt(trimmed)) {
                return accumulated;
            }
        }
        catch (iodrivers_base::TimeoutError const&) {
            // Keep waiting until deadline
        }
    }
    throw iodrivers_base::TimeoutError(iodrivers_base::TimeoutError::PACKET,
        "readUntilPrompt timed out waiting for PROMPT");
}

ParsedDisplaySensors Driver::displaySensorsParsed()
{
    std::string text = sendCommandAndWaitForReply("display sensors", m_write_timeout);
    return parseDisplaySensors(text);
}

std::string Driver::displayOptions()
{
    return sendCommandAndWaitForReply("display options", m_write_timeout);
}

std::string Driver::displaySensors()
{
    return sendCommandAndWaitForReply("display sensors", m_write_timeout);
}

std::string Driver::displayVersion()
{
    return sendCommandAndWaitForReply("display version", m_write_timeout);
}

std::string Driver::displayMemory()
{
    return sendCommandAndWaitForReply("display memory", m_write_timeout);
}

std::string Driver::displayMonitor()
{
    return sendCommandAndWaitForReply("display monitor", m_write_timeout);
}

Configuration Driver::queryConfiguration()
{
    std::string text = displayOptions();
    return parseDisplayOptions(text);
}
