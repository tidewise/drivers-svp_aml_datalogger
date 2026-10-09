#include <base/Pressure.hpp>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <svp_aml_datalogger/Configuration.hpp>
#include <svp_aml_datalogger/Driver.hpp>
#include <svp_aml_datalogger/Measurements.hpp>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace base;
using namespace svp_aml_datalogger;

volatile sig_atomic_t g_stop_measuring = 0;

void handle_sigint(int sig)
{
    g_stop_measuring = 1;
}

int usage()
{
    cerr << "Usage: svp_aml_datalogger_bin URI COMMAND [OPTIONS]\n\n"
         << "URI is a valid iodrivers_base URI, e.g. serial:///dev/ttyUSB0:115200 or "
         << "tcp://192.168.1.63:23\n\n"
         << "Commands:\n"
         << "  display-options  - Show current sensor settings (runs \"display "
         << "options\")\n"
         << "  display-sensors  - Show connected sensors (runs \"display sensors\")\n"
         << "  display-version  - Show version, firmware, serial number (runs \"display "
         << "version\")\n"
         << "  display-memory   - Show SD card total/used memory (runs \"display "
         << "memory\")\n"
         << "  display-monitor  - Show current monitor settings (runs \"display "
         << "monitor\")\n"
         << "  configure        - Interactively configure the sensor settings and apply "
         << "them\n"
         << "  measure          - Connect, configure, and stream real-time measurements "
         << "(Ctrl+C to exit)\n"
         << "  raw <cmd>        - Execute a raw command on the sensor and display the "
         << "response\n\n"
         << "Options:\n"
         << "  --debug, -d      - Enable detailed debug and raw packet logs\n"
         << flush;
    return 1;
}

Driver setupDriver(string const& uri, Configuration const& config, bool debug)
{
    Driver driver(config, debug);
    driver.setReadTimeout(Time::fromMilliseconds(2000));
    driver.setWriteTimeout(Time::fromMilliseconds(1000));
    driver.openURI(uri);
    return driver;
}

void pushBackMonitoredParameter(uint16_t params,
    MeasuredParameters const& measured_param,
    string const& name,
    vector<string>& names)
{
    if (params & measured_param) {
        names.push_back(name);
    }
}
void pushBackDerivedParameter(uint16_t params,
    DerivedParameters const& derived_param,
    string const& name,
    vector<string>& names)
{
    if (params & derived_param) {
        names.push_back(name);
    }
}

string monitoredParametersToString(uint16_t params)
{
    vector<string> names;
    pushBackMonitoredParameter(params, MEASURE_DATE, "Date", names);
    pushBackMonitoredParameter(params, MEASURE_TIME, "Time", names);
    pushBackMonitoredParameter(params, MEASURE_BATTERY_VOLTAGE, "Vbat", names);
    pushBackMonitoredParameter(params, MEASURE_BATTERY_CURRENT, "Ibat", names);
    pushBackMonitoredParameter(params, MEASURE_EXTERNAL_VOLTAGE, "Vext", names);
    pushBackMonitoredParameter(params, MEASURE_EXTERNAL_CURRENT, "Iext", names);
    pushBackMonitoredParameter(params, MEASURE_CHARGE_STATE, "ChargeState", names);
    pushBackMonitoredParameter(params, MEASURE_DEPTH, "Depth", names);
    pushBackMonitoredParameter(params, MEASURE_SALINITY, "Salinity", names);
    pushBackMonitoredParameter(params, MEASURE_DENSITY, "Density", names);
    pushBackMonitoredParameter(params, MEASURE_SOUND_VELOCITY, "SoundVelocity", names);

    if (names.empty()) {
        return "None";
    }
    string res = "";
    for (size_t i = 0; i < names.size(); ++i) {
        res += names[i];
        if (i < names.size() - 1)
            res += ", ";
    }
    return res;
}

string derivedParametersToString(uint8_t params)
{
    vector<string> names;
    pushBackDerivedParameter(params, DEPTH_FROM_PRESSURE, "Depth From Pressure", names);
    pushBackDerivedParameter(params,
        SALINITY_FROM_CONDUCTIVITY,
        "Salinity from Conductivity",
        names);
    pushBackDerivedParameter(params,
        SALINITY_FROM_SOUND_VELOCITY,
        "Salinity from SV",
        names);
    pushBackDerivedParameter(params,
        DENSITY_FROM_SALINITY,
        "Density from Salinity",
        names);
    pushBackDerivedParameter(params,
        SOUND_VELOCITY_FROM_SALINITY,
        "SV from Salinity",
        names);

    if (names.empty()) {
        return "None";
    }
    string res = "";
    for (size_t i = 0; i < names.size(); ++i) {
        res += names[i];
        if (i < names.size() - 1)
            res += ", ";
    }
    return res;
}

void printCurrentConfiguration(Configuration const& config)
{
    cout << "\n============================================\n"
         << "     Interactive Configuration Menu\n"
         << "============================================\n";
    cout << "1. Sampling Mode:     "
         << (config.sampling_mode == SINGLE ? "Single" : "Burst") << "\n";
    cout << "2. Trigger Mode:      ";
    switch (config.sampling_trigger.mode) {
        case BY_TIME:
            cout << "By Time\n";
            break;
        case BY_SOUND_VELOCITY_INCREMENT:
            cout << "By Sound Velocity Increment\n";
            break;
        case BY_PRESSURE_INCREMENT:
            cout << "By Pressure Increment\n";
            break;
    }

    if (config.sampling_trigger.mode == BY_TIME) {
        cout << "3. Sample Rate:       " << config.sampling_trigger.samples_per_s
             << " Hz\n";
    }
    else if (config.sampling_trigger.mode == BY_SOUND_VELOCITY_INCREMENT) {
        cout << "3. SV Increment:      "
             << config.sampling_trigger.sound_velocity_increment << " m/s\n";
    }
    else {
        cout << "3. Press. Increment:  "
             << config.sampling_trigger.pressure_increment.toBar() << " Bar\n";
    }

    if (config.sampling_mode == BURST) {
        cout << "4. Burst Interval:    "
             << config.burst_parameters.burst_interval.toSeconds() << " s\n";
        cout << "5. Burst Samples:     " << config.burst_parameters.samples_per_burst
             << "\n";
        cout << "6. Burst Monitor:     "
             << (config.burst_parameters.monitor == SAMPLES ? "Samples" : "Average")
             << "\n";
    }

    int offset = (config.sampling_mode == BURST) ? 3 : 0;
    cout << (4 + offset) << ". Monitored Params:   "
         << monitoredParametersToString(config.measured_parameters) << "\n";
    cout << (5 + offset) << ". Derived Params:     "
         << derivedParametersToString(config.derived_parameters) << "\n";
    cout << (6 + offset) << ". Monitor Format:     "
         << (config.monitor_parameters.monitor_format == COLUMNS ? "Columns (CSV)"
                                                                 : "AMLx")
         << "\n";
    cout << (7 + offset) << ". Robust Monitor:     "
         << (config.monitor_parameters.robust ? "Enabled" : "Disabled") << "\n";
    cout << (8 + offset) << ". [Exit & Apply Configuration]\n";
    cout << (9 + offset) << ". [Cancel & Abort]\n";
    cout << "--------------------------------------------\n";
}

void configureSamplingMode(Configuration& config)
{
    cout << "\nSelect Sampling Mode:\n"
         << "1) Single\n"
         << "2) Burst\n"
         << "Choice [1-2]: " << flush;
    string sub_choice;
    if (getline(cin, sub_choice)) {
        if (sub_choice == "1") {
            config.sampling_mode = SINGLE;
        }
        else if (sub_choice == "2") {
            config.sampling_mode = BURST;
            config.sampling_trigger.mode = BY_TIME;
            if (config.sampling_trigger.samples_per_s <= 0) {
                config.sampling_trigger.samples_per_s = 5.0f;
            }
            if (config.burst_parameters.samples_per_burst < 2) {
                config.burst_parameters.samples_per_burst = 10;
            }
            if (config.burst_parameters.burst_interval.toSeconds() < 5) {
                config.burst_parameters.burst_interval = Time::fromSeconds(30);
            }
        }
        else {
            cout << "Invalid choice, mode unchanged.\n";
        }
    }
}

void configureTriggerMode(Configuration& config)
{
    cout << "\nSelect Trigger Mode:\n"
         << "1) By Time\n"
         << "2) By Sound Velocity Increment\n"
         << "3) By Pressure Increment\n"
         << "Choice [1-3]: " << flush;
    string sub_choice;
    if (getline(cin, sub_choice)) {
        if (sub_choice == "1") {
            config.sampling_trigger.mode = BY_TIME;
            if (config.sampling_trigger.samples_per_s <= 0) {
                config.sampling_trigger.samples_per_s = 5.0f;
            }
        }
        else if (sub_choice == "2") {
            config.sampling_trigger.mode = BY_SOUND_VELOCITY_INCREMENT;
            if (config.sampling_trigger.sound_velocity_increment == 0) {
                config.sampling_trigger.sound_velocity_increment = 0.5f;
            }
        }
        else if (sub_choice == "3") {
            config.sampling_trigger.mode = BY_PRESSURE_INCREMENT;
            if (config.sampling_trigger.pressure_increment.toBar() == 0) {
                config.sampling_trigger.pressure_increment =
                    base::Pressure::fromBar(0.01);
            }
        }
        else {
            cout << "Invalid choice, trigger mode unchanged.\n";
        }
    }
}

void configureRateOrIncrement(Configuration& config)
{
    if (config.sampling_trigger.mode == BY_TIME) {
        cout << "\nEnter Sample Rate (1 to 20 Hz, float): " << flush;
        string rate_str;
        if (getline(cin, rate_str)) {
            try {
                config.sampling_trigger.samples_per_s = stof(rate_str);
            }
            catch (...) {
                cout << "Invalid rate entered, unchanged.\n";
            }
        }
    }
    else if (config.sampling_trigger.mode == BY_SOUND_VELOCITY_INCREMENT) {
        cout << "\nEnter Sound Velocity Increment (m/s, float): " << flush;
        string inc_str;
        if (getline(cin, inc_str)) {
            try {
                config.sampling_trigger.sound_velocity_increment = stof(inc_str);
            }
            catch (...) {
                cout << "Invalid increment entered, unchanged.\n";
            }
        }
    }
    else {
        cout << "\nEnter Pressure Increment (Bar, float): " << flush;
        string inc_str;
        if (getline(cin, inc_str)) {
            try {
                config.sampling_trigger.pressure_increment =
                    base::Pressure::fromBar(stof(inc_str));
            }
            catch (...) {
                cout << "Invalid increment entered, unchanged.\n";
            }
        }
    }
}

void configureBurstInterval(Configuration& config)
{
    cout << "\nEnter Burst Interval (seconds, 5 to 14400): " << flush;
    string int_str;
    if (getline(cin, int_str)) {
        try {
            int interval = stoi(int_str);
            config.burst_parameters.burst_interval = Time::fromSeconds(interval);
        }
        catch (...) {
            cout << "Invalid interval entered, unchanged.\n";
        }
    }
}

void configureBurstSamples(Configuration& config)
{
    cout << "\nEnter Samples Per Burst (2 to 1200): " << flush;
    string count_str;
    if (getline(cin, count_str)) {
        try {
            config.burst_parameters.samples_per_burst = stoi(count_str);
        }
        catch (...) {
            cout << "Invalid count entered, unchanged.\n";
        }
    }
}

void configureBurstMonitor(Configuration& config)
{
    cout << "\nSelect Burst Monitor:\n"
         << "1) Individual Samples\n"
         << "2) Average of Burst\n"
         << "Choice [1-2]: " << flush;
    string sub_choice;
    if (getline(cin, sub_choice)) {
        if (sub_choice == "1") {
            config.burst_parameters.monitor = SAMPLES;
        }
        else if (sub_choice == "2") {
            config.burst_parameters.monitor = AVERAGE;
        }
        else {
            cout << "Invalid choice, unchanged.\n";
        }
    }
}

void printConfigurableMeasurementsMenu(Configuration const& config)
{
    cout << "\nToggle Monitored Parameters:\n";
#define SHOW_PARAM_TOGGLE(bit, name)                                                     \
    cout << "[" << ((config.measured_parameters & bit) ? "X" : " ") << "] " << name      \
         << "\n";

    cout << "1) ";
    SHOW_PARAM_TOGGLE(DATE, "Date");
    cout << "2) ";
    SHOW_PARAM_TOGGLE(TIME, "Time");
    cout << "3) ";
    SHOW_PARAM_TOGGLE(BATTERY_VOLTAGE, "Battery Voltage (Vbat)");
    cout << "4) ";
    SHOW_PARAM_TOGGLE(BATTERY_CURRENT, "Battery Current (Ibat)");
    cout << "5) ";
    SHOW_PARAM_TOGGLE(EXTERNAL_VOLTAGE, "External Voltage (Vext)");
    cout << "6) ";
    SHOW_PARAM_TOGGLE(EXTERNAL_CURRENT, "External Current (Iext)");
    cout << "7) ";
    SHOW_PARAM_TOGGLE(CHARGE_STATE, "Charge State");
    cout << "8) ";
    SHOW_PARAM_TOGGLE(DEPTH, "Depth (dep)");
    cout << "9) ";
    SHOW_PARAM_TOGGLE(SALINITY, "Salinity (sal)");
    cout << "10) ";
    SHOW_PARAM_TOGGLE(DENSITY, "Density (den)");
    cout << "11) ";
    SHOW_PARAM_TOGGLE(SOUND_VELOCITY, "Sound Velocity (sound)");
    cout << "12) Done Toggling\n";
    cout << "Select parameter to toggle [1-12]: " << flush;
}

void toggleMonitoredParameters(Configuration& config)
{
    while (true) {
        printConfigurableMeasurementsMenu(config);
        string toggle_choice_str;
        if (!getline(cin, toggle_choice_str))
            break;
        if (toggle_choice_str == "12")
            break;
        try {
            int p_choice = stoi(toggle_choice_str);
            switch (p_choice) {
                case 1:
                    config.measured_parameters ^= DATE;
                    break;
                case 2:
                    config.measured_parameters ^= TIME;
                    break;
                case 3:
                    config.measured_parameters ^= BATTERY_VOLTAGE;
                    break;
                case 4:
                    config.measured_parameters ^= BATTERY_CURRENT;
                    break;
                case 5:
                    config.measured_parameters ^= EXTERNAL_VOLTAGE;
                    break;
                case 6:
                    config.measured_parameters ^= EXTERNAL_CURRENT;
                    break;
                case 7:
                    config.measured_parameters ^= CHARGE_STATE;
                    break;
                case 8:
                    config.measured_parameters ^= DEPTH;
                    break;
                case 9:
                    config.measured_parameters ^= SALINITY;
                    break;
                case 10:
                    config.measured_parameters ^= DENSITY;
                    break;
                case 11:
                    config.measured_parameters ^= SOUND_VELOCITY;
                    break;
                default:
                    cout << "Invalid toggle choice.\n";
            }
        }
        catch (...) {
            cout << "Invalid input.\n";
        }
    }
}

void printDerivedParamsMenu(Configuration const& config)
{
    cout << "\nToggle Derived Parameters:\n";
#define SHOW_DERIVED_TOGGLE(bit, name)                                                   \
    cout << "[" << ((config.derived_parameters & bit) ? "X" : " ") << "] " << name       \
         << "\n";

    cout << "1) ";
    SHOW_DERIVED_TOGGLE(DEPTH_FROM_PRESSURE, "Depth from Pressure");
    cout << "2) ";
    SHOW_DERIVED_TOGGLE(SALINITY_FROM_CONDUCTIVITY, "Salinity from Conductivity");
    cout << "3) ";
    SHOW_DERIVED_TOGGLE(SALINITY_FROM_SOUND_VELOCITY, "Salinity from Sound Velocity");
    cout << "4) ";
    SHOW_DERIVED_TOGGLE(DENSITY_FROM_SALINITY, "Density from Salinity");
    cout << "5) ";
    SHOW_DERIVED_TOGGLE(SOUND_VELOCITY_FROM_SALINITY, "Sound Velocity from Salinity");
    cout << "6) Done Toggling\n";
    cout << "Select derived parameter to toggle [1-6]: " << flush;
}

void toggleDerivedParameters(Configuration& config)
{
    while (true) {
        printDerivedParamsMenu(config);
        string toggle_choice_str;
        if (!getline(cin, toggle_choice_str))
            break;
        if (toggle_choice_str == "6")
            break;
        try {
            int p_choice = stoi(toggle_choice_str);
            switch (p_choice) {
                case 1:
                    config.derived_parameters ^= DEPTH_FROM_PRESSURE;
                    break;
                case 2:
                    config.derived_parameters ^= SALINITY_FROM_CONDUCTIVITY;
                    break;
                case 3:
                    config.derived_parameters ^= SALINITY_FROM_SOUND_VELOCITY;
                    break;
                case 4:
                    config.derived_parameters ^= DENSITY_FROM_SALINITY;
                    break;
                case 5:
                    config.derived_parameters ^= SOUND_VELOCITY_FROM_SALINITY;
                    break;
                default:
                    cout << "Invalid toggle choice.\n";
            }
        }
        catch (...) {
            cout << "Invalid input.\n";
        }
    }
}

void configureMonitorFormat(Configuration& config)
{
    cout << "\nSelect Monitor Format:\n"
         << "1) Columns (CSV)\n"
         << "2) AMLx (Proprietary)\n"
         << "Choice [1-2]: " << flush;
    string sub_choice;
    if (getline(cin, sub_choice)) {
        if (sub_choice == "1") {
            config.monitor_parameters.monitor_format = COLUMNS;
        }
        else if (sub_choice == "2") {
            cout << "AMLx format is not supported by the driver\n";
        }
        else {
            cout << "Invalid choice, unchanged.\n";
        }
    }
}

void configureRobustMonitor(Configuration& config)
{
    cout << "\nEnable Robust Monitor? (y/n): " << flush;
    string sub_choice;
    if (getline(cin, sub_choice)) {
        if (sub_choice == "y" || sub_choice == "Y") {
            config.monitor_parameters.robust = true;
        }
        else if (sub_choice == "n" || sub_choice == "N") {
            config.monitor_parameters.robust = false;
        }
        else {
            cout << "Invalid choice, unchanged.\n";
        }
    }
}

bool processInteractiveMenuChoice(Configuration& config, int normalized_choice)
{
    if (normalized_choice == 1) {
        configureSamplingMode(config);
    }
    else if (normalized_choice == 2) {
        configureTriggerMode(config);
    }
    else if (normalized_choice == 3) {
        configureRateOrIncrement(config);
    }
    else if (normalized_choice == 4 && config.sampling_mode == BURST) {
        configureBurstInterval(config);
    }
    else if (normalized_choice == 5 && config.sampling_mode == BURST) {
        configureBurstSamples(config);
    }
    else if (normalized_choice == 6 && config.sampling_mode == BURST) {
        configureBurstMonitor(config);
    }
    else if (normalized_choice == 7) {
        toggleMonitoredParameters(config);
    }
    else if (normalized_choice == 8) {
        toggleDerivedParameters(config);
    }
    else if (normalized_choice == 9) {
        configureMonitorFormat(config);
    }
    else if (normalized_choice == 10) {
        configureRobustMonitor(config);
    }
    else if (normalized_choice == 11) {
        try {
            config.validate();
            cout << "\nConfiguration is valid! Applying changes...\n";
            return true; // Exit menu, apply
        }
        catch (std::exception const& e) {
            cout << "\n[ERROR] Configuration validation failed: " << e.what() << "\n";
            cout << "Please correct the issues before applying.\n";
            cout << "Press Enter to return to menu..." << flush;
            string temp;
            getline(cin, temp);
        }
    }
    else if (normalized_choice == 12) {
        cout << "\nConfiguration discarded.\n";
        exit(0);
    }
    else {
        cout << "Invalid option selected.\n";
    }
    return false; // Keep loop going
}

void runInteractiveConfiguration(Configuration& config)
{
    while (true) {
        printCurrentConfiguration(config);

        int max_option = (config.sampling_mode == BURST) ? 12 : 9;
        cout << "Select option (1-" << max_option << "): " << flush;

        string choice_str;
        if (!getline(cin, choice_str)) {
            break;
        }

        int choice = 0;
        try {
            choice = stoi(choice_str);
        }
        catch (...) {
            cout << "Invalid choice, please enter a number.\n";
            continue;
        }

        int normalized_choice = choice;
        if (config.sampling_mode != BURST && choice >= 4) {
            normalized_choice = choice + 3;
        }

        if (processInteractiveMenuChoice(config, normalized_choice)) {
            break;
        }
    }
}

void handleDisplayCommand(string const& uri,
    string const& cmd,
    Configuration const& config,
    bool debug)
{
    auto driver = setupDriver(uri, config, debug);

    if (cmd == "display-options") {
        cout << driver.displayOptions() << flush;
    }
    else if (cmd == "display-sensors") {
        ParsedDisplaySensors parsed = driver.displaySensorsParsed();

        cout << "\n======================================================================"
                "==================================================\n"
             << "                                             Parsed Connected Sensors "
                "Information\n"
             << "========================================================================"
                "================================================\n";

        cout << "Sensor Metadata Header Columns:\n  ";
        for (size_t i = 0; i < parsed.sensor_metadata.columns.size(); ++i) {
            cout << parsed.sensor_metadata.columns[i];
            if (i < parsed.sensor_metadata.columns.size() - 1)
                cout << ", ";
        }
        cout << "\n\nActive Sensor Channels:\n";

        cout << left << setw(6) << "Port" << left << setw(14) << "Model" << left
             << setw(12) << "Serial" << left << setw(10) << "Firmware" << left << setw(12)
             << "Parameter" << left << setw(10) << "Units" << left << setw(14)
             << "Cal. Date" << left << setw(12) << "Cal. Time" << left << setw(10)
             << "Accuracy" << left << setw(10) << "Min" << left << setw(10) << "Max"
             << "\n";
        cout << string(120, '-') << "\n";

        for (auto const& s : parsed.sensor_data) {
            cout << left << setw(6) << s.port << left << setw(14) << s.model << left
                 << setw(12) << s.serial_number << left << setw(10) << s.firmware << left
                 << setw(12) << s.parameter << left << setw(10) << s.units << left
                 << setw(14) << s.calibration_date << left << setw(12)
                 << s.calibration_time << left << setw(10) << s.accuracy << left
                 << setw(10) << s.range_min << left << setw(10) << s.range_max << "\n";
        }
        cout << string(120, '-') << "\n\n";

        cout << "Active Measurement Telemetry Format:\n";
        cout << "  Columns: ";
        for (size_t i = 0; i < parsed.measurement_metadata.columns.size(); ++i) {
            cout << measuredParameterToString(parsed.measurement_metadata.columns[i]);
            if (i < parsed.measurement_metadata.columns.size() - 1)
                cout << ", ";
        }
        cout << "\n  Units  : ";
        for (size_t i = 0; i < parsed.measurement_metadata.units.size(); ++i) {
            cout << parsed.measurement_metadata.units[i];
            if (i < parsed.measurement_metadata.units.size() - 1)
                cout << ", ";
        }
        cout << "\n======================================================================"
                "==================================================\n\n";
    }
    else if (cmd == "display-version") {
        cout << driver.displayVersion() << flush;
    }
    else if (cmd == "display-memory") {
        cout << driver.displayMemory() << flush;
    }
    else if (cmd == "display-monitor") {
        cout << driver.displayMonitor() << flush;
    }
}

void handleRawCommand(string const& uri,
    string const& raw_cmd,
    Configuration const& config,
    bool debug)
{
    auto driver = setupDriver(uri, config, debug);
    cout << driver.sendCommandAndWaitForReply(raw_cmd, Time::fromMilliseconds(1000))
         << flush;
}

void handleConfigureCommand(string const& uri, Configuration& config, bool debug)
{
    std::cout << "\n[Step 1/2] Connecting to " << uri << " ...\n";
    auto driver = setupDriver(uri, config, debug);
    std::cout << "  -> Connected successfully.\n";

    config = driver.queryConfiguration();
    runInteractiveConfiguration(config);

    std::cout << "[Step 2/2] Configuring sensor settings...\n";
    driver.configure(config);
    std::cout << "  -> Configuration successfully applied and confirmed.\n";
}

void printMeasurementHeader(std::vector<MeasuredParameters> const& columns)
{
    ostringstream header;
    header << left;
    if (hasColumn(columns, MEASURE_DATE) && hasColumn(columns, MEASURE_TIME)) {
        header << setw(32) << "Timestamp";
    }
    if (hasColumn(columns, MEASURE_BATTERY_VOLTAGE)) {
        header << left << setw(12) << "Vbat (V)";
    }
    if (hasColumn(columns, MEASURE_BATTERY_CURRENT)) {
        header << left << setw(12) << "Ibat (A)";
    }
    if (hasColumn(columns, MEASURE_EXTERNAL_VOLTAGE)) {
        header << left << setw(12) << "Vext (V)";
    }
    if (hasColumn(columns, MEASURE_EXTERNAL_CURRENT)) {
        header << left << setw(12) << "Iext (A)";
    }
    if (hasColumn(columns, MEASURE_PRESSURE)) {
        header << left << setw(18) << "Pressure (Pa)";
    }
    if (hasColumn(columns, MEASURE_SOUND_VELOCITY)) {
        header << left << setw(12) << "SV (m/s)";
    }
    if (hasColumn(columns, MEASURE_DEPTH)) {
        header << left << setw(12) << "Depth (m)";
    }
    if (hasColumn(columns, MEASURE_SALINITY)) {
        header << left << setw(12) << "Salinity (PSU)";
    }
    if (hasColumn(columns, MEASURE_DENSITY)) {
        header << left << setw(12) << "Density (kg/m3)";
    }
    if (hasColumn(columns, MEASURE_CHARGE_STATE)) {
        header << left << setw(8) << "Charged";
    }
    header << "\n" << string(header.str().size(), '-') << "\n";
    std::cout << header.str();
}

void printMeasurementRow(std::vector<MeasuredParameters> const& columns,
    Measurements const& m)
{
    ostringstream row;
    row << left;
    if (hasColumn(columns, MEASURE_DATE) || hasColumn(columns, MEASURE_TIME)) {
        row << setw(32) << m.time.toString(Time::Milliseconds);
    }
    if (hasColumn(columns, MEASURE_BATTERY_VOLTAGE)) {
        row << left << setw(12) << setprecision(2) << m.battery_voltage;
    }
    if (hasColumn(columns, MEASURE_BATTERY_CURRENT)) {
        row << left << setw(12) << setprecision(2) << m.battery_current;
    }
    if (hasColumn(columns, MEASURE_EXTERNAL_VOLTAGE)) {
        row << left << setw(12) << setprecision(2) << m.external_voltage;
    }
    if (hasColumn(columns, MEASURE_EXTERNAL_CURRENT)) {
        row << left << setw(12) << setprecision(2) << m.external_current;
    }
    if (hasColumn(columns, MEASURE_PRESSURE)) {
        row << left << setw(18) << setprecision(5) << m.pressure.toPa();
    }
    if (hasColumn(columns, MEASURE_SOUND_VELOCITY)) {
        row << left << setw(12) << setprecision(3) << m.sound_velocity;
    }
    if (hasColumn(columns, MEASURE_DEPTH)) {
        row << left << setw(12) << setprecision(5) << m.depth;
    }
    if (hasColumn(columns, MEASURE_SALINITY)) {
        row << left << setw(12) << setprecision(3) << m.salinity;
    }
    if (hasColumn(columns, MEASURE_DENSITY)) {
        row << left << setw(12) << setprecision(3) << m.density;
    }
    if (hasColumn(columns, MEASURE_CHARGE_STATE)) {
        row << left << setw(8) << m.charge_state;
    }
    std::cout << row.str() << "\n";
}

void handleMeasureCommand(string const& uri, Configuration const& config, bool debug)
{
    signal(SIGINT, handle_sigint);

    std::cout << "--- Starting Progressive Measurement Mode ---\n";
    std::cout << "[Step 1/5] Connecting to sensor URI: " << uri << " ...\n";
    auto driver = setupDriver(uri, config, debug);
    std::cout << "  -> Connected successfully.\n";

    std::cout << "[Step 2/5] Reading current configuration of the device \n";
    auto configuration = driver.queryConfiguration();

    std::cout << "[Step 3/5 Applying configuration to the device...\n";
    driver.configure(configuration);

    std::cout
        << "[Step 4/5] Handshaking and entering streaming mode (sending 'monitor')...\n";
    driver.startMeasuring();
    std::cout << "  -> Telemetry streaming activated.\n";

    std::cout << "[Step 5/5] Streaming parsed measurements (Press Ctrl+C to stop):\n\n";
    printMeasurementHeader(driver.getMeasurementColumns());

    while (!g_stop_measuring) {
        try {
            Measurements m = driver.readMeasurements();
            printMeasurementRow(driver.getMeasurementColumns(), m);
            usleep(100000);
        }
        catch (iodrivers_base::TimeoutError const&) {
            // Timeout occurred, check g_stop_measuring in next iteration
        }
        catch (std::exception const& e) {
            if (!g_stop_measuring) {
                cerr << "Error reading measurement: " << e.what() << "\n";
            }
        }
    }

    std::cout << "\n[Cleanup] Halting measurement stream and returning to CLI...\n";
    driver.stopMeasuring();
    std::cout << "  -> Successfully returned to prompt. Done.\n";
}

int main(int argc, char const* argv[])
{
    if (argc < 3) {
        return usage();
    }

    string uri(argv[1]);
    string command(argv[2]);

    // Parse options (like debug)
    bool debug = false;
    string raw_cmd_arg = "";
    for (int i = 3; i < argc; ++i) {
        string arg(argv[i]);
        if (arg == "--debug" || arg == "-d") {
            debug = true;
        }
        else if (command == "raw" && raw_cmd_arg.empty()) {
            raw_cmd_arg = arg;
        }
    }

    if (command == "raw" && raw_cmd_arg.empty() && argc > 3) {
        raw_cmd_arg = argv[3];
    }

    // Set up standard prototyping/driver configuration
    Configuration config;
    config.sampling_mode = SINGLE;
    config.sampling_trigger.mode = BY_TIME;
    config.sampling_trigger.samples_per_s = 5.0f; // 5 Hz sampling rate
    config.measured_parameters = DATE | TIME | BATTERY_VOLTAGE | DEPTH | SOUND_VELOCITY;
    config.derived_parameters = DEPTH_FROM_PRESSURE; // Calculate depth from pressure
    config.monitor_parameters.monitor_format = COLUMNS;
    config.monitor_parameters.robust = true;

    try {
        if (command == "display-options" || command == "display-sensors" ||
            command == "display-version" || command == "display-memory" ||
            command == "display-monitor") {
            handleDisplayCommand(uri, command, config, debug);
        }
        else if (command == "raw") {
            if (raw_cmd_arg.empty()) {
                cerr << "Error: 'raw' command requires a command string argument.\n";
                return 1;
            }
            handleRawCommand(uri, raw_cmd_arg, config, debug);
        }
        else if (command == "configure") {
            handleConfigureCommand(uri, config, debug);
        }
        else if (command == "measure") {
            handleMeasureCommand(uri, config, debug);
        }
        else {
            cerr << "Error: Unknown command '" << command << "'\n\n";
            return usage();
        }
    }
    catch (std::exception const& e) {
        cerr << "\nException caught: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
