#include <algorithm>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <svp_aml_datalogger/Measurements.hpp>

namespace svp_aml_datalogger {
    std::string trim(std::string const& str)
    {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    bool hasColumn(std::vector<std::string> const& cols, std::string const& name)
    {
        return std::find(cols.begin(), cols.end(), name) != cols.end();
    }

    std::optional<ConfigurableMeasurements> measuredParameterToConfigurableMeasurement(
        MeasuredParameters const& parameter)
    {
        switch (parameter) {
            case MEASURE_DATE:
                return DATE;
            case MEASURE_TIME:
                return TIME;
            case MEASURE_BATTERY_VOLTAGE:
                return BATTERY_VOLTAGE;
            case MEASURE_BATTERY_CURRENT:
                return BATTERY_CURRENT;
            case MEASURE_EXTERNAL_VOLTAGE:
                return EXTERNAL_VOLTAGE;
            case MEASURE_EXTERNAL_CURRENT:
                return EXTERNAL_CURRENT;
            case MEASURE_CHARGE_STATE:
                return CHARGE_STATE;
            case MEASURE_DEPTH:
                return DEPTH;
            case MEASURE_SALINITY:
                return SALINITY;
            case MEASURE_DENSITY:
                return DENSITY;
            case MEASURE_SOUND_VELOCITY:
                return SOUND_VELOCITY;
            default:
                return {};
        }
    }

    SensorMetadata parseSensorMetadata(std::string const& line)
    {
        std::string trimmed = trim(line);
        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            throw std::runtime_error("Invalid SensorMetadata line: " + line);
        }
        std::string cols_str = trimmed.substr(eq_pos + 1);
        std::vector<std::string> columns;
        std::string col;
        std::istringstream stream(cols_str);
        while (std::getline(stream, col, ',')) {
            columns.push_back(trim(col));
        }
        return SensorMetadata{columns};
    }

    SensorData parseSensorData(std::string const& line)
    {
        std::string trimmed = trim(line);
        std::vector<std::string> fields;
        std::string field;
        std::istringstream stream(trimmed);
        while (std::getline(stream, field, ',')) {
            fields.push_back(trim(field));
        }
        if (fields.size() < 11) {
            throw std::runtime_error(
                "Invalid SensorData line, expected at least 11 fields: " + line);
        }
        SensorData s;
        s.port = fields[0];
        s.model = fields[1];
        s.serial_number = fields[2];
        s.firmware = fields[3];
        s.parameter = fields[4];
        s.units = fields[5];
        s.calibration_date = fields[6];
        s.calibration_time = fields[7];
        s.accuracy = fields[8];
        s.range_min = fields[9];
        s.range_max = fields[10];
        return s;
    }

    MeasuredParameters stringToMeasuredParameter(std::string const& name)
    {
        std::string t_name = trim(name);
        if (t_name == "Date")
            return MEASURE_DATE;
        if (t_name == "Time")
            return MEASURE_TIME;
        if (t_name == "Vbat")
            return MEASURE_BATTERY_VOLTAGE;
        if (t_name == "Ibat")
            return MEASURE_BATTERY_CURRENT;
        if (t_name == "Vext")
            return MEASURE_EXTERNAL_VOLTAGE;
        if (t_name == "Iext")
            return MEASURE_EXTERNAL_CURRENT;
        if (t_name == "chargestate")
            return MEASURE_CHARGE_STATE;
        if (t_name == "Depth")
            return MEASURE_DEPTH;
        if (t_name == "Salinity")
            return MEASURE_SALINITY;
        if (t_name == "Density")
            return MEASURE_DENSITY;
        if (t_name == "SV")
            return MEASURE_SOUND_VELOCITY;
        if (t_name == "Pressure")
            return MEASURE_PRESSURE;
        throw std::runtime_error("Unknown column parameter: " + t_name);
    }

    std::string measuredParameterToString(MeasuredParameters param)
    {
        switch (param) {
            case MEASURE_DATE:
                return "Date";
            case MEASURE_TIME:
                return "Time";
            case MEASURE_BATTERY_VOLTAGE:
                return "Vbat";
            case MEASURE_BATTERY_CURRENT:
                return "Ibat";
            case MEASURE_EXTERNAL_VOLTAGE:
                return "Vext";
            case MEASURE_EXTERNAL_CURRENT:
                return "Iext";
            case MEASURE_CHARGE_STATE:
                return "chargestate";
            case MEASURE_DEPTH:
                return "Depth";
            case MEASURE_PRESSURE:
                return "Pressure";
            case MEASURE_SALINITY:
                return "Salinity";
            case MEASURE_DENSITY:
                return "Density";
            case MEASURE_SOUND_VELOCITY:
                return "SV";
        }
        return "Unknown";
    }

    bool hasColumn(std::vector<MeasuredParameters> const& cols,
        MeasuredParameters const& param)
    {
        return std::find(cols.begin(), cols.end(), param) != cols.end();
    }

    MeasurementMetadata parseMeasurementMetadata(std::vector<std::string> const& lines)
    {
        MeasurementMetadata meta;
        for (auto const& raw_line : lines) {
            std::string line = trim(raw_line);
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos)
                continue;
            std::string key = trim(line.substr(0, eq_pos));
            std::string val = trim(line.substr(eq_pos + 1));
            std::vector<std::string> fields;
            std::string field;
            std::istringstream stream(val);
            while (std::getline(stream, field, ',')) {
                fields.push_back(trim(field));
            }
            if (key == "Columns") {
                for (auto const& f : fields) {
                    meta.columns.push_back(stringToMeasuredParameter(f));
                }
            }
            else if (key == "Units") {
                meta.units = fields;
            }
        }
        return meta;
    }

    ParsedDisplaySensors parseDisplaySensors(std::string const& raw_text)
    {
        ParsedDisplaySensors parsed;
        std::istringstream stream(raw_text);
        std::string line;
        enum Section {
            NONE,
            SENSOR_METADATA,
            SENSOR_DATA,
            MEASUREMENT_METADATA
        };
        Section current_section = NONE;
        std::vector<std::string> measurement_metadata_lines;

        while (std::getline(stream, line)) {
            std::string trimmed = trim(line);
            if (trimmed.empty()) {
                continue;
            }
            if (trimmed == "[SensorMetaData]") {
                current_section = SENSOR_METADATA;
                continue;
            }
            else if (trimmed == "[SensorData]") {
                current_section = SENSOR_DATA;
                continue;
            }
            else if (trimmed == "[MeasurementMetadata]") {
                current_section = MEASUREMENT_METADATA;
                continue;
            }

            if (current_section == SENSOR_METADATA) {
                std::string meta_line = trimmed;
                if (meta_line.find('=') == std::string::npos) {
                    meta_line = "Columns=" + meta_line;
                }
                parsed.sensor_metadata = parseSensorMetadata(meta_line);
            }
            else if (current_section == SENSOR_DATA) {
                parsed.sensor_data.push_back(parseSensorData(trimmed));
            }
            else if (current_section == MEASUREMENT_METADATA) {
                measurement_metadata_lines.push_back(trimmed);
            }
        }
        parsed.measurement_metadata =
            parseMeasurementMetadata(measurement_metadata_lines);
        return parsed;
    }
}
