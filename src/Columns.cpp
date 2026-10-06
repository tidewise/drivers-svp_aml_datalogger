#include <base/Pressure.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
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
        std::istringstream ss(date_str + " " + time_data);
        std::tm t = {};
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            throw std::runtime_error("Failed to parse time: " + ss.str());
        }

        int millis = 0;
        if (ss.peek() == '.') {
            // Consume '.'
            ss.get();
            std::string millis_str;
            ss >> millis_str;
            if (ss.fail()) {
                throw std::runtime_error(
                    "Failed to parse time milliseconds: " + ss.str());
            }

            float millis_f = 0;
            std::istringstream millis_ss("0." + millis_str);
            millis_ss >> millis_f;
            millis = millis_f * 1000;
            if (millis_ss.fail()) {
                throw std::runtime_error(
                    "Failed to parse time milliseconds: " + ss.str());
            }
        }

        return base::Time::fromTimeValues(t.tm_year + 1900, // tm counts years from 1900
            t.tm_mon + 1,                                   // tm months start from 0
            t.tm_mday,
            t.tm_hour,
            t.tm_min,
            t.tm_sec,
            millis,
            0);
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
