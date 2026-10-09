#ifndef DRIVERS_SVP_AML_DATALOGGER_CLIMODE_HPP
#define DRIVERS_SVP_AML_DATALOGGER_CLIMODE_HPP

#include <string>
#include <svp_aml_datalogger/Configuration.hpp>

namespace svp_aml_datalogger {
    /** @brief The standard command line prompt character returned by the sensor */
    static constexpr char PROMPT = '>';

    /**
     * @brief Checks if a returned line or message represents the CLI prompt.
     *
     * @param msg The string to evaluate.
     * @return true if the string ends with or matches the prompt character, false
     * otherwise.
     */
    bool isPrompt(std::string msg);

    /**
     * @brief Formulates the measuring starting command based on the desired format.
     *
     * @param format The requested monitor format.
     * @return The starting command string.
     */
    std::string measureCommand(MonitorFormat const& format);

    /**
     * @brief Generates the sample-rate setting command.
     *
     * @param samples_per_s Target sample rate in Hz or sample interval in seconds.
     * @return The command string.
     */
    std::string samplePerSecondCommand(float samples_per_s);

    /**
     * @brief Generates the commands to configure SINGLE sampling mode triggers.
     *
     * @param sampling_trigger The trigger settings.
     * @return The list of command strings.
     */
    std::vector<std::string> singleSamplingModeCommands(
        SamplingTriggerParameters const& sampling_trigger);

    /**
     * @brief Generates the commands to configure BURST sampling mode.
     *
     * @param sampling_trigger The trigger settings for sampling within a burst.
     * @param burst_parameters General burst interval and size settings.
     * @return The list of command strings.
     */
    std::vector<std::string> burstSamplingModeCommands(
        SamplingTriggerParameters const& sampling_trigger,
        BurstModeParameters const& burst_parameters);

    /**
     * @brief Generates command strings to configure details of burst sampling.
     *
     * @param burst_parameters The burst parameters.
     * @return The list of command strings.
     */
    std::vector<std::string> burstParametersCommands(
        BurstModeParameters const& burst_parameters);

    /**
     * @brief Generates the command list for the specified sampling configuration.
     *
     * @param sampling_mode The target sampling mode (SINGLE or BURST).
     * @param sampling_trigger Trigger parameters.
     * @param burst_parameters Burst scheduler parameters.
     * @return The list of commands.
     */
    std::vector<std::string> samplingCommands(SamplingMode const& sampling_mode,
        SamplingTriggerParameters const& sampling_trigger,
        BurstModeParameters const& burst_parameters);

    /**
     * @brief Formulates a command to enable or disable a single scan parameter.
     *
     * @param bitfield The active monitored parameters bitfield.
     * @param bit The specific parameter bit to evaluate.
     * @param parameter The target hardware parameter command string name.
     * @return The command string.
     */
    std::string togglableScanParameterCommands(uint16_t bitfield,
        ConfigurableMeasurements const& bit,
        std::string parameter);

    /**
     * @brief Formulates a command to enable or disable a single derived parameter.
     *
     * @param bitfield The active derived parameters bitfield.
     * @param bit The specific derived parameter bit to evaluate.
     * @param parameter The target hardware parameter command string name.
     * @return The command string.
     */
    std::string togglableDeriveParameterCommands(uint8_t bitfield,
        DerivedParameters const& bit,
        std::string parameter);

    /**
     * @brief Formulates the list of commands to enable/disable all monitored primary
     * parameters.
     *
     * @param measured_parameters The active bitfield of measured parameters.
     * @return The list of commands.
     */
    std::vector<std::string> configurableMeasurementCommands(
        uint16_t measured_parameters);

    /**
     * @brief Formulates the list of commands to enable/disable all derived parameters.
     *
     * @param derived_parameters The active bitfield of derived parameters.
     * @return The list of commands.
     */
    std::vector<std::string> derivedParametersCommands(uint8_t derived_parameters);

    /**
     * @brief Formulates the commands required to apply the specified monitor settings.
     *
     * @param parameters The active monitoring parameters.
     * @return The list of commands.
     */
    std::vector<std::string> monitoringParametersCommands(
        MonitoringParameters const& parameters);
}

#endif // DRIVERS_SVP_AML_DATALOGGER_CLIMODE_HPP
