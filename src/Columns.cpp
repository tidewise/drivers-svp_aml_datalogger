#include <base/Pressure.hpp>
#include <svp_aml_datalogger/Columns.hpp>
#include <svp_aml_datalogger/Measurements.hpp>

namespace svp_aml_datalogger {
    double parseDouble(std::string const& data)
    {
        std::string trimmed = trim(data);
        if (trimmed.empty()) {
            throw std::runtime_error("Empty numeric field");
        }
        size_t processed = 0;
        double val = 0.0;
        try {
            val = std::stod(trimmed, &processed);
        }
        catch (std::exception const& e) {
            throw std::runtime_error("Failed to parse double: " + trimmed);
        }
        if (processed != trimmed.length()) {
            throw std::runtime_error("Extra characters in numeric field: " + trimmed);
        }
        return val;
    }

    base::Time parseTime(std::string date_str, std::string time_data)
    {
        std::string trimmed_date = trim(date_str);
        if (trimmed_date.length() != 10) {
            throw std::runtime_error("Invalid date format length: " + trimmed_date);
        }
        if (trimmed_date[4] != '-' || trimmed_date[7] != '-') {
            throw std::runtime_error("Invalid date delimiters: " + trimmed_date);
        }
        int year = 0, month = 0, day = 0;
        char extra;
        if (std::sscanf(trimmed_date.c_str(),
                "%d-%d-%d%c",
                &year,
                &month,
                &day,
                &extra) != 3) {
            throw std::runtime_error("Failed to parse date: " + trimmed_date);
        }
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            throw std::runtime_error("Invalid date values in: " + trimmed_date);
        }

        std::string trimmed_time = trim(time_data);
        if (trimmed_time.length() < 8) {
            throw std::runtime_error("Invalid time format length: " + trimmed_time);
        }
        if (trimmed_time[2] != ':' || trimmed_time[5] != ':') {
            throw std::runtime_error("Invalid time delimiters: " + trimmed_time);
        }
        int hour = 0, minute = 0;
        double seconds_val = 0.0;
        if (std::sscanf(trimmed_time.c_str(),
                "%d:%d:%lf%c",
                &hour,
                &minute,
                &seconds_val,
                &extra) != 3) {
            throw std::runtime_error("Failed to parse time: " + trimmed_time);
        }
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || seconds_val < 0.0 ||
            seconds_val >= 60.0) {
            throw std::runtime_error("Invalid time values in: " + trimmed_time);
        }

        int sec = static_cast<int>(seconds_val);
        float sec_decimals = seconds_val - sec;
        int millis = static_cast<int>(round(sec_decimals * 1000.0));

        return base::Time::fromTimeValues(year, month, day, hour, minute, sec, millis, 0);
    }

    std::string parseChargeState(std::string data)
    {
        auto trimmed = trim(data);
        if (trimmed.empty()) {
            throw std::runtime_error("Empty charge state");
        }
        return trimmed;
    }

    Measurements parseColumns(std::vector<std::string> const& fields,
        std::vector<MeasuredParameters> const& columns)
    {
        Measurements m;
        int date_idx = -1;
        int time_idx = -1;
        try {
            for (size_t i = 0; i < columns.size(); ++i) {
                switch (columns[i]) {
                    case MEASURE_DATE:
                        date_idx = i;
                        break;
                    case MEASURE_TIME:
                        time_idx = i;
                        break;
                    case MEASURE_BATTERY_VOLTAGE:
                        m.battery_voltage = parseDouble(fields[i]);
                        break;
                    case MEASURE_BATTERY_CURRENT:
                        m.battery_current = parseDouble(fields[i]);
                        break;
                    case MEASURE_EXTERNAL_VOLTAGE:
                        m.external_voltage = parseDouble(fields[i]);
                        break;
                    case MEASURE_EXTERNAL_CURRENT:
                        m.external_current = parseDouble(fields[i]);
                        break;
                    case MEASURE_CHARGE_STATE:
                        m.charge_state = parseChargeState(fields[i]);
                        break;
                    case MEASURE_DEPTH:
                        m.depth = parseDouble(fields[i]);
                        break;
                    case MEASURE_PRESSURE: {
                        auto pressure_dbar = parseDouble(fields[i]);
                        m.pressure = base::Pressure::fromBar(pressure_dbar / 10);
                        break;
                    }
                    case MEASURE_SALINITY:
                        m.salinity = parseDouble(fields[i]);
                        break;
                    case MEASURE_DENSITY:
                        m.density = parseDouble(fields[i]);
                        break;
                    case MEASURE_SOUND_VELOCITY:
                        m.sound_velocity = parseDouble(fields[i]);
                        break;
                }
            }

            if (date_idx != -1 && time_idx != -1) {
                m.time = parseTime(fields[date_idx], fields[time_idx]);
            }
        }
        catch (std::exception const& e) {
            throw std::runtime_error("Failed to parse fields: " + std::string(e.what()));
        }
        return m;
    }
}
