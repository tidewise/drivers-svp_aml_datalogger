#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <svp_aml_datalogger/CLIMode.hpp>
#include <svp_aml_datalogger/Configuration.hpp>

using namespace svp_aml_datalogger;
using namespace std;

namespace svp_aml_datalogger {
    bool isPrompt(std::string msg)
    {
        return !msg.empty() && msg[0] == PROMPT;
    }

    string samplePerSecondCommand(float samples_per_s)
    {
        ostringstream builder;
        builder << "set sample ";
        if (samples_per_s > 0) {
            builder << round(samples_per_s) << " /second";
        }
        else {
            uint8_t sample = round(1 / samples_per_s);
            builder << sample << " seconds";
        }
        return builder.str();
    }

    vector<string> singleSamplingModeCommands(
        SamplingTriggerParameters const& sampling_trigger)
    {
        vector<string> commands;
        commands.push_back("set scan mode single");
        switch (sampling_trigger.mode) {
            case BY_TIME:
                commands.push_back("set trigger time");
                commands.push_back(
                    samplePerSecondCommand(sampling_trigger.samples_per_s));
                break;
            case BY_SOUND_VELOCITY_INCREMENT:
                commands.push_back("set trigger sound");
                {
                    ostringstream out;
                    out << "set sound inc " << sampling_trigger.sound_velocity_increment
                        << setprecision(2);
                    commands.push_back(out.str());
                }
                break;
            case BY_PRESSURE_INCREMENT:
                commands.push_back("set trigger pressure");
                {
                    ostringstream out;
                    out << "set pressure inc "
                        << sampling_trigger.pressure_increment.toBar() * 10
                        << setprecision(2);
                    commands.push_back(out.str());
                }
                break;
        }
        return commands;
    }

    vector<string> burstParametersCommands(BurstModeParameters const& burst_parameters)
    {
        vector<string> commands;

        ostringstream interval;
        interval << "set burst interval " << burst_parameters.burst_interval.toSeconds()
                 << setprecision(2);
        commands.push_back(interval.str());

        ostringstream samples;
        samples << "set burst samples " << burst_parameters.samples_per_burst;
        commands.push_back(samples.str());

        auto monitor_str = burst_parameters.monitor == SAMPLES ? "samples" : "average";
        ostringstream monitor;
        monitor << "set burst monitor " << monitor_str;
        commands.push_back(monitor.str());

        return commands;
    }

    vector<string> burstSamplingModeCommands(
        SamplingTriggerParameters const& sampling_trigger,
        BurstModeParameters const& burst_parameters)
    {
        vector<string> commands;
        commands.push_back("set scan mode burst");
        commands.push_back("set trigger time");
        commands.push_back(samplePerSecondCommand(sampling_trigger.samples_per_s));

        auto burst_params_cmd = burstParametersCommands(burst_parameters);
        commands.insert(commands.end(), burst_params_cmd.begin(), burst_params_cmd.end());
        return commands;
    }

    vector<string> samplingCommands(SamplingMode const& sampling_mode,
        SamplingTriggerParameters const& sampling_trigger,
        BurstModeParameters const& burst_parameters)
    {
        vector<string> commands;

        string mode_cmd;
        switch (sampling_mode) {
            case SINGLE: {
                auto single_mode_cmds = singleSamplingModeCommands(sampling_trigger);
                commands.insert(commands.end(),
                    single_mode_cmds.begin(),
                    single_mode_cmds.end());
            } break;
            case BURST: {
                auto burst_mode_cmds =
                    burstSamplingModeCommands(sampling_trigger, burst_parameters);
                commands.insert(commands.end(),
                    burst_mode_cmds.begin(),
                    burst_mode_cmds.end());
            } break;
        }
        return commands;
    }

    string togglableScanParameterCommands(uint16_t bitfield,
        ConfigurableMeasurements const& bit,
        string parameter)
    {
        ostringstream out;
        out << "set scan ";
        if ((bitfield & bit) != 0) {
            out << parameter;
        }
        else {
            out << "no" << parameter;
        }
        return out.str();
    }

    string togglableDeriveParameterCommands(uint8_t bitfield,
        DerivedParameters const& bit,
        string parameter)
    {
        ostringstream out;
        out << "set derive " << parameter;
        if ((bitfield & bit) != 0) {
            out << " y";
        }
        else {
            out << " n";
        }
        return out.str();
    }

    vector<string> configurableMeasurementCommands(uint16_t monitored_parameters)
    {
        vector<string> commands;

        commands.push_back(
            togglableScanParameterCommands(monitored_parameters, DATE, "date"));
        commands.push_back(
            togglableScanParameterCommands(monitored_parameters, TIME, "time"));
        commands.push_back(togglableScanParameterCommands(monitored_parameters,
            BATTERY_VOLTAGE,
            "vbat"));
        commands.push_back(togglableScanParameterCommands(monitored_parameters,
            BATTERY_CURRENT,
            "ibat"));
        commands.push_back(togglableScanParameterCommands(monitored_parameters,
            EXTERNAL_VOLTAGE,
            "vext"));
        commands.push_back(togglableScanParameterCommands(monitored_parameters,
            EXTERNAL_CURRENT,
            "iext"));
        commands.push_back(togglableScanParameterCommands(monitored_parameters,
            CHARGE_STATE,
            "chargestate"));
        commands.push_back(
            togglableScanParameterCommands(monitored_parameters, DEPTH, "dep"));
        commands.push_back(
            togglableScanParameterCommands(monitored_parameters, SALINITY, "sal"));
        commands.push_back(
            togglableScanParameterCommands(monitored_parameters, DENSITY, "den"));
        commands.push_back(togglableScanParameterCommands(monitored_parameters,
            SOUND_VELOCITY,
            "sound"));
        return commands;
    }

    vector<string> derivedParametersCommands(uint8_t derived_parameters)
    {
        vector<string> commands;

        commands.push_back(togglableDeriveParameterCommands(derived_parameters,
            DEPTH_FROM_PRESSURE,
            "depth"));
        commands.push_back(togglableDeriveParameterCommands(derived_parameters,
            SALINITY_FROM_CONDUCTIVITY,
            "salc"));
        commands.push_back(togglableDeriveParameterCommands(derived_parameters,
            SALINITY_FROM_SOUND_VELOCITY,
            "salsv"));
        commands.push_back(togglableDeriveParameterCommands(derived_parameters,
            DENSITY_FROM_SALINITY,
            "density"));
        commands.push_back(togglableDeriveParameterCommands(derived_parameters,
            SOUND_VELOCITY_FROM_SALINITY,
            "sv"));
        return commands;
    }

    vector<string> monitoringParametersCommands(MonitoringParameters const& parameters)
    {
        vector<string> commands;

        auto format_str = parameters.monitor_format == COLUMNS ? "columns" : "amlx";
        ostringstream format;
        format << "set monitor format " << format_str;
        commands.push_back(format.str());

        string robust = parameters.robust ? " y" : " n";
        ostringstream robust_str;
        robust_str << "set monitor robust " << robust;
        commands.push_back(robust_str.str());

        ostringstream start_inactivity;
        start_inactivity << "set monitor auto "
                         << parameters.auto_start_after_inactivity.toSeconds()
                         << setprecision(2);
        commands.push_back(start_inactivity.str());

        // Hardcode these to be consistent with the driver implementation
        commands.push_back("set monitor startup n");
        commands.push_back("set monitor checksum y");
        ostringstream delimiter;
        delimiter << "set monitor delimiter comma";
        commands.push_back(delimiter.str());

        return commands;
    }
}
