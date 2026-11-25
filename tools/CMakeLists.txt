# Programs that help run the runtime but are not part of it. Coverage leaves them out.

# Streams made-up EEG shaped like a device config.json, for running experiments without a device.
add_executable(fake_eeg fake_eeg.cpp)
target_link_libraries(fake_eeg PRIVATE config logging lsl)
