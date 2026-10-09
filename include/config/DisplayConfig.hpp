#ifndef DISPLAYCONFIG_HPP
#define DISPLAYCONFIG_HPP

// The screen the participant looks at (`display` in `config.json`). It belongs
// to the lab setup, not the experiment: the same experiment file runs on
// whichever monitor faces the participant at this particular machine.
struct DisplayConfig {
    static constexpr int kDefaultWindowWidth  = 1280;
    static constexpr int kDefaultWindowHeight = 720;

    int  index      = 0;                     // display.index: monitor the stimulus window opens on
    bool fullscreen = true;                  // display.fullscreen: native-resolution fullscreen
    int  width      = kDefaultWindowWidth;   // display.width: window size, windowed mode only
    int  height     = kDefaultWindowHeight;  // display.height

    // Throws std::invalid_argument on a negative index or a non-positive window
    // size. Whether `index` names a connected monitor can only be checked when
    // the window is created.
    void validate() const;
};

#endif  // DISPLAYCONFIG_HPP
