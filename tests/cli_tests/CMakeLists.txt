# Run the NeuronIDE binary the way the launcher does and check what it reports.
# CTest ignores the exit code once PASS_REGULAR_EXPRESSION is set.

add_test(NAME NeuronIDECli.PrintsUsageWithoutArguments COMMAND NeuronIDE)
set_tests_properties(NeuronIDECli.PrintsUsageWithoutArguments PROPERTIES
    LABELS "cli"
    PASS_REGULAR_EXPRESSION "Usage: .* \\[config\\.json\\] <experiment\\.neuroz>"
)

add_test(NAME NeuronIDECli.LogsWhyARunAborted
    COMMAND NeuronIDE no-such-config.json no-such-experiment.neuroz
)
set_tests_properties(NeuronIDECli.LogsWhyARunAborted PROPERTIES
    LABELS "cli"
    PASS_REGULAR_EXPRESSION
        "CRITICAL +\\[app\\] Run aborted: ConfigParser: cannot open file: no-such-config\\.json"
)

add_test(NAME NeuronIDECli.WarnsAboutAnUnknownLogLevel COMMAND NeuronIDE)
set_tests_properties(NeuronIDECli.WarnsAboutAnUnknownLogLevel PROPERTIES
    LABELS "cli"
    ENVIRONMENT "NEURONIDE_LOG_LEVEL=verbose"
    PASS_REGULAR_EXPRESSION "WARNING +\\[app\\] Ignoring NEURONIDE_LOG_LEVEL='verbose'"
)
