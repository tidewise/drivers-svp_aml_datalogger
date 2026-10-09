#include <base/Pressure.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <svp_aml_datalogger/Driver.hpp>
#include <sys/socket.h>
#include <unistd.h>

using namespace svp_aml_datalogger;

class TestDriver : public Driver {
public:
    using Driver::extractPacket;
};

TEST(DriverTest, it_parses_a_valid_measurement_line)
{
    std::string line = "2026-09-16,11:26:01.10,8.11,0.25346,1499.773,0.25183*0B";
    Measurements m = Driver::parseMeasurementLine(line,
        {MEASURE_DATE,
            MEASURE_TIME,
            MEASURE_BATTERY_VOLTAGE,
            MEASURE_PRESSURE,
            MEASURE_SOUND_VELOCITY,
            MEASURE_DEPTH});

    base::Time expected_time = base::Time::fromTimeValues(2026, 9, 16, 11, 26, 1, 100, 0);
    ASSERT_EQ(m.time, expected_time);
    ASSERT_NEAR(8.11, m.battery_voltage, 1e-5);
    ASSERT_NEAR(0.025346, m.pressure.toBar(), 1e-5);
    ASSERT_NEAR(1499.773, m.sound_velocity, 1e-3);
    ASSERT_NEAR(0.25183, m.depth, 1e-5);
}

TEST(DriverTest, it_parses_a_valid_measurement_line_with_extra_spaces_and_newlines)
{
    std::string line =
        "  2026-09-16, 11:26:01.10 , 8.11 , 0.25346 , 1499.773 , 0.25183 *0B\r\n";
    Measurements m = Driver::parseMeasurementLine(line,
        {MEASURE_DATE,
            MEASURE_TIME,
            MEASURE_BATTERY_VOLTAGE,
            MEASURE_PRESSURE,
            MEASURE_SOUND_VELOCITY,
            MEASURE_DEPTH});

    base::Time expected_time = base::Time::fromTimeValues(2026, 9, 16, 11, 26, 1, 100, 0);
    ASSERT_EQ(expected_time, m.time);
    ASSERT_NEAR(8.11, m.battery_voltage, 1e-5);
    ASSERT_NEAR(0.025346, m.pressure.toBar(), 1e-5);
    ASSERT_NEAR(1499.773, m.sound_velocity, 1e-3);
    ASSERT_NEAR(0.25183, m.depth, 1e-5);
}

TEST(DriverTest, it_throws_on_missing_checksum)
{
    std::string line = "2026-09-16,11:26:01.10,8.11,0.25346,1499.773,0.25183";
    ASSERT_THROW(Driver::parseMeasurementLine(line,
                     {MEASURE_DATE,
                         MEASURE_TIME,
                         MEASURE_BATTERY_VOLTAGE,
                         MEASURE_PRESSURE,
                         MEASURE_SOUND_VELOCITY,
                         MEASURE_DEPTH}),
        ChecksumError);
}

TEST(DriverTest, it_throws_on_invalid_checksum)
{
    std::string line = "2026-09-16,11:26:01.10,8.11,0.25346,1499.773,0.25183*AA";
    ASSERT_THROW(Driver::parseMeasurementLine(line,
                     {MEASURE_DATE,
                         MEASURE_TIME,
                         MEASURE_BATTERY_VOLTAGE,
                         MEASURE_PRESSURE,
                         MEASURE_SOUND_VELOCITY,
                         MEASURE_DEPTH}),
        ChecksumError);
}

TEST(DriverTest, it_throws_on_malformed_checksum)
{
    std::string line = "2026-09-16,11:26:01.10,8.11,0.25346,1499.773,0.25183*";
    ASSERT_THROW(Driver::parseMeasurementLine(line,
                     {MEASURE_DATE,
                         MEASURE_TIME,
                         MEASURE_BATTERY_VOLTAGE,
                         MEASURE_PRESSURE,
                         MEASURE_SOUND_VELOCITY,
                         MEASURE_DEPTH}),
        ChecksumError);
}

TEST(DriverTest, it_throws_on_empty_or_invalid_lines)
{
    // Empty line
    ASSERT_THROW(Driver::parseMeasurementLine("", {MEASURE_DATE}), std::runtime_error);
    // Too few fields
    ASSERT_THROW(
        Driver::parseMeasurementLine("2026-09-16,11:26:01.10,8.11",
            {MEASURE_DATE, MEASURE_TIME, MEASURE_BATTERY_VOLTAGE, MEASURE_PRESSURE}),
        std::runtime_error);
    // Invalid date format
    ASSERT_THROW(Driver::parseMeasurementLine(
                     "2026/09/16,11:26:01.10,8.11,0.25346,1499.773,0.25183",
                     {MEASURE_DATE,
                         MEASURE_TIME,
                         MEASURE_BATTERY_VOLTAGE,
                         MEASURE_PRESSURE,
                         MEASURE_SOUND_VELOCITY,
                         MEASURE_DEPTH}),
        std::runtime_error);
    // Invalid time format
    ASSERT_THROW(Driver::parseMeasurementLine(
                     "2026-09-16,11-26-01.10,8.11,0.25346,1499.773,0.25183",
                     {MEASURE_DATE,
                         MEASURE_TIME,
                         MEASURE_BATTERY_VOLTAGE,
                         MEASURE_PRESSURE,
                         MEASURE_SOUND_VELOCITY,
                         MEASURE_DEPTH}),
        std::runtime_error);
    // Non-numeric fields
    ASSERT_THROW(Driver::parseMeasurementLine(
                     "2026-09-16,11:26:01.10,abc,0.25346,1499.773,0.25183",
                     {MEASURE_DATE,
                         MEASURE_TIME,
                         MEASURE_BATTERY_VOLTAGE,
                         MEASURE_PRESSURE,
                         MEASURE_SOUND_VELOCITY,
                         MEASURE_DEPTH}),
        std::runtime_error);
}

TEST(DriverTest, it_extracts_packets_correctly)
{
    TestDriver driver;

    // Buffer without a newline
    std::string data1 = "partial data";
    int res1 = driver.extractPacket(reinterpret_cast<uint8_t const*>(data1.c_str()),
        data1.size());
    ASSERT_EQ(0, res1);

    // Buffer with a newline
    std::string data2 = "line 1\nline 2\n";
    int res2 = driver.extractPacket(reinterpret_cast<uint8_t const*>(data2.c_str()),
        data2.size());
    ASSERT_EQ(7, res2); // "line 1\n" is 7 bytes

    // Buffer exceeding max packet size with no newline
    std::string data3(Driver::MAX_PACKET_SIZE, 'a');
    int res3 = driver.extractPacket(reinterpret_cast<uint8_t const*>(data3.c_str()),
        data3.size());
    ASSERT_EQ(-1, res3);
}

class TestDriverStartMeasuring : public ::testing::Test {
protected:
    int fds[2];
    Driver* driver;

    void SetUp() override
    {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
            throw std::runtime_error("socketpair failed");
        }
    }

    void TearDown() override
    {
        delete driver;
        close(fds[1]); // fds[0] is closed by driver destructor
    }

    void initDriver(Configuration const& config)
    {
        driver = new Driver(config);
        driver->setFileDescriptor(fds[0], true);
        // Set small timeouts for fast tests
        driver->setReadTimeout(base::Time::fromMilliseconds(100));
        driver->setWriteTimeout(base::Time::fromMilliseconds(100));
    }

    void writeMockData(std::string const& data)
    {
        ssize_t bytes_written = write(fds[1], data.c_str(), data.size());
        (void)bytes_written;
    }

    std::string readWrittenCommand()
    {
        char buf[256];
        ssize_t bytes_read = read(fds[1], buf, sizeof(buf) - 1);
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            return std::string(buf);
        }
        return "";
    }
};

TEST_F(TestDriverStartMeasuring, it_starts_measuring_and_enforces_correct_configuration)
{
    Configuration config;
    config.measured_parameters = DATE | TIME | BATTERY_VOLTAGE | DEPTH | SOUND_VELOCITY;
    initDriver(config);

    // Two prompts needed at the end, one for the return of display sensors, and other
    // for the return of monitor
    writeMockData("[MeasurementMetadata]\n"
                  "Columns=Date,Time,Vbat,Pressure,SV,Depth\n"
                  "Units=yyyy-mm-dd,hh:mm:ss.ss,V,dBar,m/s,m\n"
                  "[MeasurementData]\n>\n>\n");

    ASSERT_NO_THROW(driver->startMeasuring());
    ASSERT_EQ(readWrittenCommand(), "display sensors\rmonitor\r");
}

TEST_F(TestDriverStartMeasuring,
    it_supports_arbitrary_column_orders_for_enforcement_and_parsing)
{
    Configuration config;
    config.measured_parameters = DATE | TIME | BATTERY_VOLTAGE | DEPTH | SOUND_VELOCITY;
    initDriver(config);

    // Two prompts needed at the end, one for the return of display sensors, and other
    // for the return of monitor
    writeMockData("[MeasurementMetadata]\n"
                  "Columns=Time,Date,SV,Depth,Vbat,Pressure\n"
                  "Units=hh:mm:ss.ss,yyyy-mm-dd,m/s,m,V,dBar\n"
                  "[MeasurementData]\n>\n>\n");

    ASSERT_NO_THROW(driver->startMeasuring());
    ASSERT_EQ(readWrittenCommand(), "display sensors\rmonitor\r");

    // Feed a line corresponding to the custom column order
    // Time,Date,SV,Depth,Vbat,Pressure
    writeMockData("11:26:01.10,2026-09-16,1499.773,0.25183,8.11,0.25346*0B\n");

    Measurements m = driver->readMeasurements();
    base::Time expected_time = base::Time::fromTimeValues(2026, 9, 16, 11, 26, 1, 100, 0);
    ASSERT_EQ(expected_time, m.time);
    ASSERT_NEAR(8.11, m.battery_voltage, 1e-5);
    ASSERT_NEAR(0.025346, m.pressure.toBar(), 1e-5);
    ASSERT_NEAR(1499.773, m.sound_velocity, 1e-3);
    ASSERT_NEAR(0.25183, m.depth, 1e-5);
}

TEST_F(TestDriverStartMeasuring, it_throws_if_a_configured_parameter_is_missing)
{
    Configuration config;
    config.measured_parameters = DATE | TIME | BATTERY_VOLTAGE | DEPTH | SOUND_VELOCITY;
    initDriver(config);

    writeMockData("[MeasurementMetadata]\n"
                  "Columns=Date,Time,Pressure,SV,Depth\n" // Missing Vbat
                  "Units=yyyy-mm-dd,hh:mm:ss.ss,dBar,m/s,m\n"
                  "[MeasurementData]\n>\n");

    ASSERT_THROW(driver->startMeasuring(), std::runtime_error);
}

TEST_F(TestDriverStartMeasuring, it_throws_if_an_unconfigured_parameter_is_present)
{
    Configuration config;
    config.measured_parameters =
        DATE | TIME | DEPTH | SOUND_VELOCITY; // No BATTERY_VOLTAGE
    initDriver(config);

    writeMockData("[MeasurementMetadata]\n"
                  "Columns=Date,Time,Vbat,Pressure,SV,Depth\n" // Vbat is present
                  "Units=yyyy-mm-dd,hh:mm:ss.ss,V,dBar,m/s,m\n"
                  "[MeasurementData]\n>\n");

    ASSERT_THROW(driver->startMeasuring(), std::runtime_error);
}

TEST_F(TestDriverStartMeasuring, it_throws_on_metadata_timeout)
{
    Configuration config;
    initDriver(config);

    writeMockData(
        "[MeasurementMetadata]\nColumns=Date,Time\n"); // No [MeasurementData] to end the
                                                       // metadata block

    ASSERT_THROW(driver->startMeasuring(), std::runtime_error);
}

std::string mockDisplayOptions(Configuration const& config)
{
    std::string out = "[Header]\n";
    out +=
        "ScanMode=" + std::string(config.sampling_mode == SINGLE ? "Single" : "Burst") +
        "\n";

    std::string trigger_str = "Time";
    if (config.sampling_trigger.mode == BY_SOUND_VELOCITY_INCREMENT)
        trigger_str = "SV Inc";
    else if (config.sampling_trigger.mode == BY_PRESSURE_INCREMENT)
        trigger_str = "P Inc";
    out += "ScanTrigger=" + trigger_str + "\n";

    if (config.sampling_trigger.samples_per_s >= 1.0f) {
        out += "SensorSampleRate=" +
               std::to_string(
                   static_cast<int>(round(config.sampling_trigger.samples_per_s))) +
               "\n";
        out += "SensorSampleRateUnits=/sec\n";
    }
    else {
        out += "SensorSampleRate=" +
               std::to_string(static_cast<int>(
                   round(1.0f / config.sampling_trigger.samples_per_s))) +
               "\n";
        out += "SensorSampleRateUnits=seconds\n";
    }

    out +=
        "SoundInc=" + std::to_string(config.sampling_trigger.sound_velocity_increment) +
        "\n";
    out += "PressureInc=" +
           std::to_string(config.sampling_trigger.pressure_increment.toBar() * 10) + "\n";
    out += "BurstIntervalSecs=" +
           std::to_string(config.burst_parameters.burst_interval.toSeconds()) + "\n";
    out += "BurstSamples=" + std::to_string(config.burst_parameters.samples_per_burst) +
           "\n";
    out +=
        "BurstMonitor=" +
        std::string(config.burst_parameters.monitor == SAMPLES ? "Samples" : "Average") +
        "\n";

    auto add_bool = [&](std::string const& key, bool val) {
        out += key + "=" + (val ? "yes" : "no") + "\n";
    };

    add_bool("DisplayDate", (config.measured_parameters & DATE) != 0);
    add_bool("DisplayTime", (config.measured_parameters & TIME) != 0);
    add_bool("DisplayVbat", (config.measured_parameters & BATTERY_VOLTAGE) != 0);
    add_bool("DisplayIbat", (config.measured_parameters & BATTERY_CURRENT) != 0);
    add_bool("DisplayVext", (config.measured_parameters & EXTERNAL_VOLTAGE) != 0);
    add_bool("DisplayIext", (config.measured_parameters & EXTERNAL_CURRENT) != 0);
    add_bool("DisplayChargeState", (config.measured_parameters & CHARGE_STATE) != 0);
    add_bool("DisplayDepth", (config.measured_parameters & DEPTH) != 0);
    add_bool("DisplaySalinity", (config.measured_parameters & SALINITY) != 0);
    add_bool("DisplayDensity", (config.measured_parameters & DENSITY) != 0);
    add_bool("DisplaySoundVelocity", (config.measured_parameters & SOUND_VELOCITY) != 0);

    add_bool("DeriveDepth", (config.derived_parameters & DEPTH_FROM_PRESSURE) != 0);
    add_bool("DeriveSalinity_C",
        (config.derived_parameters & SALINITY_FROM_CONDUCTIVITY) != 0);
    add_bool("DeriveSalinity_SV",
        (config.derived_parameters & SALINITY_FROM_SOUND_VELOCITY) != 0);
    add_bool("DeriveDensity", (config.derived_parameters & DENSITY_FROM_SALINITY) != 0);
    add_bool("DeriveSoundvelocity",
        (config.derived_parameters & SOUND_VELOCITY_FROM_SALINITY) != 0);

    out += "MonitorFormat=" +
           std::string(
               config.monitor_parameters.monitor_format == COLUMNS ? "columns" : "amlx") +
           "\n";
    add_bool("RobustMonitor", config.monitor_parameters.robust);
    out += "AutoMonitor=" +
           std::to_string(
               config.monitor_parameters.auto_start_after_inactivity.toSeconds()) +
           "\n";

    out += ">\n";
    return out;
}

TEST_F(TestDriverStartMeasuring, it_configures_successfully_when_no_errors_are_returned)
{
    Configuration config;
    initDriver(config);

    auto commands = config.configurationCommands();
    std::string mock_responses;
    for (size_t i = 0; i < commands.size(); ++i) {
        mock_responses += "Command successful\n>\n";
    }
    mock_responses += mockDisplayOptions(config);
    writeMockData(mock_responses);

    ASSERT_NO_THROW(driver->configure());
}

TEST_F(TestDriverStartMeasuring,
    it_throws_if_the_device_returns_an_error_during_configuration)
{
    Configuration config;
    initDriver(config);

    auto commands = config.configurationCommands();
    std::string mock_responses;
    mock_responses += "Command successful\n>\n";
    mock_responses += "Error: value out of range\n>\n";

    writeMockData(mock_responses);

    ASSERT_THROW(driver->configure(), std::runtime_error);
}

TEST(DriverTest, it_parses_display_sensors_text_correctly)
{
    std::string text =
        "[SensorMetaData]\n"
        "Columns=Port,Model,SerialNumber,Firmware,Parameter,Units,CalDate,CalTime,"
        "Accuracy,RangeMin,RangeMax\n"
        "[SensorData]\n"
        "1,SV-Xchange,12345,1.2,SV,m/s,2026-09-16,11:26:00,0.01,1375.0,1625.0\n"
        "2,P-Xchange,54321,2.0,Pressure,dBar,2026-09-16,11:26:00,0.05,0.0,500.0\n"
        "[MeasurementMetadata]\n"
        "Columns = Date, Time, SV, Depth, Salinity, Density\n"
        "Units = yyyy-mm-dd, hh:mm:ss.ss, m/s, m, PSU, kg/m3\n"
        ">\n";

    ParsedDisplaySensors parsed = parseDisplaySensors(text);

    // Verify SensorMetadata
    ASSERT_EQ(11, parsed.sensor_metadata.columns.size());
    ASSERT_EQ("Port", parsed.sensor_metadata.columns[0]);
    ASSERT_EQ("Parameter", parsed.sensor_metadata.columns[4]);

    // Verify SensorData
    ASSERT_EQ(2, parsed.sensor_data.size());
    ASSERT_EQ("1", parsed.sensor_data[0].port);
    ASSERT_EQ("SV-Xchange", parsed.sensor_data[0].model);
    ASSERT_EQ("12345", parsed.sensor_data[0].serial_number);
    ASSERT_EQ("1.2", parsed.sensor_data[0].firmware);
    ASSERT_EQ("SV", parsed.sensor_data[0].parameter);
    ASSERT_EQ("m/s", parsed.sensor_data[0].units);
    ASSERT_EQ("2026-09-16", parsed.sensor_data[0].calibration_date);
    ASSERT_EQ("11:26:00", parsed.sensor_data[0].calibration_time);
    ASSERT_EQ("0.01", parsed.sensor_data[0].accuracy);
    ASSERT_EQ("1375.0", parsed.sensor_data[0].range_min);
    ASSERT_EQ("1625.0", parsed.sensor_data[0].range_max);

    ASSERT_EQ("2", parsed.sensor_data[1].port);
    ASSERT_EQ("P-Xchange", parsed.sensor_data[1].model);

    // Verify MeasurementMetadata
    ASSERT_EQ(6, parsed.measurement_metadata.columns.size());
    ASSERT_EQ(MEASURE_DATE, parsed.measurement_metadata.columns[0]);
    ASSERT_EQ(MEASURE_SOUND_VELOCITY, parsed.measurement_metadata.columns[2]);

    ASSERT_EQ(6, parsed.measurement_metadata.units.size());
    ASSERT_EQ("yyyy-mm-dd", parsed.measurement_metadata.units[0]);
    ASSERT_EQ("PSU", parsed.measurement_metadata.units[4]);
}

TEST(DriverTest, it_parses_all_remaining_telemetry_columns)
{
    std::string payload = "2026-09-16,11:26:01.10,1.25,12.4,0.45,done,35.12,1024.5";
    auto add_checksum = [](std::string const& p) -> std::string {
        uint8_t checksum = 0;
        for (char c : p) {
            checksum ^= static_cast<uint8_t>(c);
        }
        char buf[16];
        std::sprintf(buf, "*%02X", checksum);
        return p + buf;
    };

    std::string line = add_checksum(payload);
    Measurements m = Driver::parseMeasurementLine(line,
        {MEASURE_DATE,
            MEASURE_TIME,
            MEASURE_BATTERY_CURRENT,
            MEASURE_EXTERNAL_VOLTAGE,
            MEASURE_EXTERNAL_CURRENT,
            MEASURE_CHARGE_STATE,
            MEASURE_SALINITY,
            MEASURE_DENSITY});

    base::Time expected_time = base::Time::fromTimeValues(2026, 9, 16, 11, 26, 1, 100, 0);
    ASSERT_EQ(expected_time, m.time);
    ASSERT_NEAR(1.25, m.battery_current, 1e-5);
    ASSERT_NEAR(12.4, m.external_voltage, 1e-5);
    ASSERT_NEAR(0.45, m.external_current, 1e-5);
    ASSERT_EQ("done", m.charge_state);
    ASSERT_NEAR(35.12, m.salinity, 1e-5);
    ASSERT_NEAR(1024.5, m.density, 1e-5);

    // Verify charge state is false when it is not "done"
    std::string payload_charging =
        "2026-09-16,11:26:01.10,1.25,12.4,0.45,charging,35.12,1024.5";
    Measurements m_charging = Driver::parseMeasurementLine(add_checksum(payload_charging),
        {MEASURE_DATE,
            MEASURE_TIME,
            MEASURE_BATTERY_CURRENT,
            MEASURE_EXTERNAL_VOLTAGE,
            MEASURE_EXTERNAL_CURRENT,
            MEASURE_CHARGE_STATE,
            MEASURE_SALINITY,
            MEASURE_DENSITY});
    ASSERT_EQ("charging", m_charging.charge_state);
}

TEST(ConfigurationTest, it_validates_burst_limits_correctly)
{
    Configuration config;
    config.sampling_mode = BURST;
    config.sampling_trigger.mode = BY_TIME;
    config.sampling_trigger.samples_per_s = 5.0f;
    config.burst_parameters.burst_interval = base::Time::fromSeconds(10);
    config.burst_parameters.samples_per_burst = 10;

    // Valid burst config
    ASSERT_NO_THROW(config.validate());

    // Trigger must be BY_TIME in burst mode
    config.sampling_trigger.mode = BY_SOUND_VELOCITY_INCREMENT;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.sampling_trigger.mode = BY_TIME;

    // Burst interval minimum 5s
    config.burst_parameters.burst_interval = base::Time::fromSeconds(4);
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.burst_parameters.burst_interval = base::Time::fromSeconds(10);

    // Burst interval maximum 14400s
    config.burst_parameters.burst_interval = base::Time::fromSeconds(14401);
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.burst_parameters.burst_interval = base::Time::fromSeconds(10);

    // Samples per burst minimum 2
    config.burst_parameters.samples_per_burst = 1;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.burst_parameters.samples_per_burst = 10;

    // Samples per burst maximum 1200
    config.burst_parameters.samples_per_burst = 1201;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.burst_parameters.samples_per_burst = 10;

    // Samples per second must be > 0 and <= 20
    config.sampling_trigger.samples_per_s = -1.0f;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.sampling_trigger.samples_per_s = 21.0f;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.sampling_trigger.samples_per_s = 5.0f;

    // Burst duration cannot exceed burst interval
    config.burst_parameters.burst_interval = base::Time::fromSeconds(9);
    config.burst_parameters.samples_per_burst = 50;
    ASSERT_THROW(config.validate(), ConfigurationError);
}

TEST(ConfigurationTest, it_validates_single_mode_triggers)
{
    Configuration config;
    config.sampling_mode = SINGLE;

    // Valid SINGLE BY_TIME trigger
    config.sampling_trigger.mode = BY_TIME;
    config.sampling_trigger.samples_per_s = 10.0f;
    ASSERT_NO_THROW(config.validate());

    // Invalid SINGLE BY_TIME: rate too high
    config.sampling_trigger.samples_per_s = 25.0f;
    ASSERT_THROW(config.validate(), ConfigurationError);

    // Invalid SINGLE BY_TIME: rate negative
    config.sampling_trigger.samples_per_s = -1.0f;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.sampling_trigger.samples_per_s = 10.0f;

    // BY_SOUND_VELOCITY_INCREMENT: increment cannot be 0
    config.sampling_trigger.mode = BY_SOUND_VELOCITY_INCREMENT;
    config.sampling_trigger.sound_velocity_increment = 0.0f;
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.sampling_trigger.sound_velocity_increment = 0.5f;
    ASSERT_NO_THROW(config.validate());

    // BY_PRESSURE_INCREMENT: increment cannot be 0
    config.sampling_trigger.mode = BY_PRESSURE_INCREMENT;
    config.sampling_trigger.pressure_increment = base::Pressure::fromBar(0);
    ASSERT_THROW(config.validate(), ConfigurationError);
    config.sampling_trigger.pressure_increment = base::Pressure::fromBar(10.0f);
    ASSERT_NO_THROW(config.validate());
}

TEST(ConfigurationTest, it_parses_display_options_correctly)
{
    std::string text = "[Header]\n"
                       "ScanMode=Burst\n"
                       "ScanTrigger=P Inc\n"
                       "SensorSampleRate=2.5\n"
                       "SensorSampleRateUnits=/sec\n"
                       "SoundInc=0.15\n"
                       "PressureInc=0.8\n"
                       "BurstIntervalSecs=3600.0\n"
                       "BurstSamples=100\n"
                       "BurstMonitor=Average\n"
                       "DisplayDate=yes\n"
                       "DisplayTime=yes\n"
                       "DisplayVbat=no\n"
                       "DisplayIbat=yes\n"
                       "DisplayVext=no\n"
                       "DisplayIext=yes\n"
                       "DisplayChargeState=no\n"
                       "DisplayDepth=yes\n"
                       "DisplaySalinity=no\n"
                       "DisplayDensity=yes\n"
                       "DisplaySoundVelocity=yes\n"
                       "DeriveDepth=yes\n"
                       "DeriveSalinity_C=no\n"
                       "DeriveSalinity_SV=yes\n"
                       "DeriveDensity=no\n"
                       "DeriveSoundvelocity=yes\n"
                       "MonitorFormat=amlx\n"
                       "RobustMonitor=yes\n"
                       "AutoMonitor=60.0\n"
                       ">\n";

    Configuration config = parseDisplayOptions(text);

    ASSERT_EQ(BURST, config.sampling_mode);
    ASSERT_EQ(BY_PRESSURE_INCREMENT, config.sampling_trigger.mode);
    ASSERT_NEAR(2.5f, config.sampling_trigger.samples_per_s, 1e-5);
    ASSERT_NEAR(0.15f, config.sampling_trigger.sound_velocity_increment, 1e-5);
    ASSERT_NEAR(0.08f, config.sampling_trigger.pressure_increment.toBar(), 1e-5);
    ASSERT_EQ(3600.0, config.burst_parameters.burst_interval.toSeconds());
    ASSERT_EQ(100, config.burst_parameters.samples_per_burst);
    ASSERT_EQ(AVERAGE, config.burst_parameters.monitor);

    // Verify bitfields
    ASSERT_TRUE((config.measured_parameters & DATE) != 0);
    ASSERT_TRUE((config.measured_parameters & TIME) != 0);
    ASSERT_FALSE((config.measured_parameters & BATTERY_VOLTAGE) != 0);
    ASSERT_TRUE((config.measured_parameters & BATTERY_CURRENT) != 0);
    ASSERT_FALSE((config.measured_parameters & EXTERNAL_VOLTAGE) != 0);
    ASSERT_TRUE((config.measured_parameters & EXTERNAL_CURRENT) != 0);
    ASSERT_FALSE((config.measured_parameters & CHARGE_STATE) != 0);
    ASSERT_TRUE((config.measured_parameters & DEPTH) != 0);
    ASSERT_FALSE((config.measured_parameters & SALINITY) != 0);
    ASSERT_TRUE((config.measured_parameters & DENSITY) != 0);
    ASSERT_TRUE((config.measured_parameters & SOUND_VELOCITY) != 0);

    ASSERT_TRUE((config.derived_parameters & DEPTH_FROM_PRESSURE) != 0);
    ASSERT_FALSE((config.derived_parameters & SALINITY_FROM_CONDUCTIVITY) != 0);
    ASSERT_TRUE((config.derived_parameters & SALINITY_FROM_SOUND_VELOCITY) != 0);
    ASSERT_FALSE((config.derived_parameters & DENSITY_FROM_SALINITY) != 0);
    ASSERT_TRUE((config.derived_parameters & SOUND_VELOCITY_FROM_SALINITY) != 0);

    ASSERT_EQ(AMLX, config.monitor_parameters.monitor_format);
    ASSERT_TRUE(config.monitor_parameters.robust);
    ASSERT_EQ(60.0, config.monitor_parameters.auto_start_after_inactivity.toSeconds());
}
