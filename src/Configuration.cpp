#include <sstream>
#include <string>
#include <svp_aml_datalogger/CLIMode.hpp>
#include <svp_aml_datalogger/Configuration.hpp>
#include <svp_aml_datalogger/Exceptions.hpp>
#include <svp_aml_datalogger/Measurements.hpp>

using namespace std;
using namespace svp_aml_datalogger;

void Configuration::validate() const
{
    switch (sampling_mode) {
        case SINGLE:
            switch (sampling_trigger.mode) {
                case BY_TIME:
                    if (sampling_trigger.samples_per_s <= 0) {
                        ostringstream msg;
                        msg << "cannot have " << sampling_trigger.samples_per_s
                            << " samples per second";
                        throw ConfigurationError(msg.str());
                    }
                    if (sampling_trigger.samples_per_s > 20) {
                        ostringstream msg;
                        msg << "cannot take more than 20 samples per second";
                        throw ConfigurationError(msg.str());
                    }
                    break;
                case BY_SOUND_VELOCITY_INCREMENT:
                    if (sampling_trigger.sound_velocity_increment == 0) {
                        ostringstream msg;
                        msg << "selected sound velocity increments as trigger, but the "
                            << "increment is 0";
                        throw ConfigurationError(msg.str());
                    }
                    break;
                case BY_PRESSURE_INCREMENT:
                    if (sampling_trigger.pressure_increment.toPa() == 0) {
                        ostringstream msg;
                        msg << "selected pressure increments as trigger, but the "
                            << "increment is 0";
                        throw ConfigurationError(msg.str());
                    }
                    break;
            }
            break;
        case BURST:
            if (sampling_trigger.mode != BY_TIME) {
                ostringstream msg;
                msg << "cannot use burst mode alongside a " << sampling_trigger.mode
                    << " trigger mode";
                throw ConfigurationError(msg.str());
            }

            if (burst_parameters.burst_interval.toSeconds() < 5) {
                ostringstream msg;
                msg << "cannot use a burst interval smaller than 5 seconds";
                throw ConfigurationError(msg.str());
            }
            if (burst_parameters.burst_interval.toSeconds() > 14400) {
                ostringstream msg;
                msg << "cannot use a burst interval greater than 14400 seconds";
                throw ConfigurationError(msg.str());
            }

            if (burst_parameters.samples_per_burst < 2) {
                ostringstream msg;
                msg << "cannot take less than 2 samples per burst";
                throw ConfigurationError(msg.str());
            }
            if (burst_parameters.samples_per_burst > 1200) {
                ostringstream msg;
                msg << "cannot take more than 1200 samples per burst";
                throw ConfigurationError(msg.str());
            }

            if (sampling_trigger.samples_per_s <= 0) {
                ostringstream msg;
                msg << "cannot have " << sampling_trigger.samples_per_s
                    << " samples per second";
                throw ConfigurationError(msg.str());
            }
            if (sampling_trigger.samples_per_s > 20) {
                ostringstream msg;
                msg << "cannot take more than 20 samples per second";
                throw ConfigurationError(msg.str());
            }
            auto burst_duration =
                burst_parameters.samples_per_burst / sampling_trigger.samples_per_s;
            if (burst_duration > burst_parameters.burst_interval.toSeconds()) {
                ostringstream msg;
                msg << "invalid burst configuration, each burst would take "
                    << burst_duration << " seconds, and the configured burst interval is "
                    << burst_parameters.burst_interval.toSeconds();
                throw ConfigurationError(msg.str());
            }
            break;
    }

    auto auto_start_after_inactivity =
        monitor_parameters.auto_start_after_inactivity.toSeconds();
    if (auto_start_after_inactivity != 0 && auto_start_after_inactivity < 5) {
        ostringstream msg;
        msg << "auto start after inactivity is non-zero, but it is less than 5 seconds, "
            << "which is the minimum value";
        throw ConfigurationError(msg.str());
    }

    if (monitor_parameters.monitor_format == AMLX) {
        ostringstream msg;
        msg << "AMLx monitor format is not supported by the driver";
        throw ConfigurationError(msg.str());
    }
}

vector<string> Configuration::configurationCommands() const
{
    vector<string> commands;

    // Turn on secure mode
    commands.push_back("secure on");

    auto sampling_cmds =
        samplingCommands(sampling_mode, sampling_trigger, burst_parameters);
    commands.insert(commands.end(), sampling_cmds.begin(), sampling_cmds.end());

    auto measured_parameters_cmds = configurableMeasurementCommands(measured_parameters);
    commands.insert(commands.end(),
        measured_parameters_cmds.begin(),
        measured_parameters_cmds.end());

    auto derived_parameters_cmds = derivedParametersCommands(derived_parameters);
    commands.insert(commands.end(),
        derived_parameters_cmds.begin(),
        derived_parameters_cmds.end());

    auto monitoring_parameters_cmds = monitoringParametersCommands(monitor_parameters);
    commands.insert(commands.end(),
        monitoring_parameters_cmds.begin(),
        monitoring_parameters_cmds.end());
    return commands;
}

Configuration svp_aml_datalogger::parseDisplayOptions(std::string const& raw_text)
{
    Configuration config;
    std::map<std::string, std::string> options;

    std::istringstream stream(raw_text);
    std::string line;
    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '[') {
            continue;
        }
        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }
        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val = trim(trimmed.substr(eq_pos + 1));
        options[key] = val;
    }

    if (options.count("ScanMode")) {
        std::string mode = options["ScanMode"];
        if (mode == "Single" || mode == "single") {
            config.sampling_mode = SINGLE;
        }
        else if (mode == "Burst" || mode == "burst") {
            config.sampling_mode = BURST;
        }
    }

    if (options.count("ScanTrigger")) {
        std::string trigger = options["ScanTrigger"];
        if (trigger == "Time" || trigger == "time") {
            config.sampling_trigger.mode = BY_TIME;
        }
        else if (trigger == "SV Inc" || trigger == "sound" || trigger == "SV inc") {
            config.sampling_trigger.mode = BY_SOUND_VELOCITY_INCREMENT;
        }
        else if (trigger == "P Inc" || trigger == "pressure" || trigger == "P inc") {
            config.sampling_trigger.mode = BY_PRESSURE_INCREMENT;
        }
    }

    if (options.count("SensorSampleRate")) {
        float rate = std::stof(options["SensorSampleRate"]);
        std::string units = "";
        if (options.count("SensorSampleRateUnits")) {
            units = options["SensorSampleRateUnits"];
        }
        if (units == "/sec" || units == "/second" || units == "Hz") {
            config.sampling_trigger.samples_per_s = rate;
        }
        else if (units == "seconds" || units == "sec" || units == "second") {
            config.sampling_trigger.samples_per_s = 1.0f / rate;
        }
        else {
            config.sampling_trigger.samples_per_s = rate;
        }
    }

    if (options.count("SoundInc")) {
        config.sampling_trigger.sound_velocity_increment = std::stof(options["SoundInc"]);
    }

    if (options.count("PressureInc")) {
        config.sampling_trigger.pressure_increment =
            base::Pressure::fromBar(std::stof(options["PressureInc"]) / 10);
    }

    if (options.count("BurstIntervalSecs")) {
        config.burst_parameters.burst_interval =
            base::Time::fromSeconds(std::stod(options["BurstIntervalSecs"]));
    }

    if (options.count("BurstSamples")) {
        config.burst_parameters.samples_per_burst = std::stoi(options["BurstSamples"]);
    }

    if (options.count("BurstMonitor")) {
        std::string monitor = options["BurstMonitor"];
        if (monitor == "Samples" || monitor == "samples") {
            config.burst_parameters.monitor = SAMPLES;
        }
        else if (monitor == "Average" || monitor == "average") {
            config.burst_parameters.monitor = AVERAGE;
        }
    }

    config.measured_parameters = 0;
    auto get_bool_option = [&options](std::string const& key) -> bool {
        auto it = options.find(key);
        return (it != options.end() && it->second == "yes");
    };

    if (get_bool_option("DisplayDate")) {
        config.measured_parameters |= DATE;
    }
    if (get_bool_option("DisplayTime")) {
        config.measured_parameters |= TIME;
    }
    if (get_bool_option("DisplayVbat")) {
        config.measured_parameters |= BATTERY_VOLTAGE;
    }
    if (get_bool_option("DisplayIbat")) {
        config.measured_parameters |= BATTERY_CURRENT;
    }
    if (get_bool_option("DisplayVext")) {
        config.measured_parameters |= EXTERNAL_VOLTAGE;
    }
    if (get_bool_option("DisplayIext")) {
        config.measured_parameters |= EXTERNAL_CURRENT;
    }
    if (get_bool_option("DisplayChargeState")) {
        config.measured_parameters |= CHARGE_STATE;
    }
    if (get_bool_option("DisplayDepth")) {
        config.measured_parameters |= DEPTH;
    }
    if (get_bool_option("DisplaySalinity")) {
        config.measured_parameters |= SALINITY;
    }
    if (get_bool_option("DisplayDensity")) {
        config.measured_parameters |= DENSITY;
    }
    if (get_bool_option("DisplaySoundVelocity")) {
        config.measured_parameters |= SOUND_VELOCITY;
    }

    config.derived_parameters = 0;
    if (get_bool_option("DeriveDepth")) {
        config.derived_parameters |= DEPTH_FROM_PRESSURE;
    }
    if (get_bool_option("DeriveSalinity_C")) {
        config.derived_parameters |= SALINITY_FROM_CONDUCTIVITY;
    }
    if (get_bool_option("DeriveSalinity_SV")) {
        config.derived_parameters |= SALINITY_FROM_SOUND_VELOCITY;
    }
    if (get_bool_option("DeriveDensity")) {
        config.derived_parameters |= DENSITY_FROM_SALINITY;
    }
    if (get_bool_option("DeriveSoundvelocity")) {
        config.derived_parameters |= SOUND_VELOCITY_FROM_SALINITY;
    }

    if (options.count("MonitorFormat")) {
        std::string format = options["MonitorFormat"];
        if (format == "columns" || format == "Columns") {
            config.monitor_parameters.monitor_format = COLUMNS;
        }
        else if (format == "amlx" || format == "AMLX") {
            config.monitor_parameters.monitor_format = AMLX;
        }
    }

    if (options.count("RobustMonitor")) {
        config.monitor_parameters.robust = get_bool_option("RobustMonitor");
    }

    if (options.count("AutoMonitor")) {
        config.monitor_parameters.auto_start_after_inactivity =
            base::Time::fromSeconds(std::stod(options["AutoMonitor"]));
    }

    return config;
}
