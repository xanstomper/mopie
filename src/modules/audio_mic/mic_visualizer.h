#ifndef __MIC_VISUALIZER_H__
#define __MIC_VISUALIZER_H__

#include <Arduino.h>

namespace AudioMic {

enum VisualizerMode : uint8_t {
    MODE_SPECTRUM_16BAND = 0,
    MODE_OSCILLOSCOPE    = 1,
    MODE_SPL_VU_METER    = 2,
    MODE_COUNT           = 3
};

class MicVisualizer {
public:
    static MicVisualizer& getInstance();

    bool init();
    void deinit();
    void run();

private:
    MicVisualizer();
    ~MicVisualizer();

    bool isInitialized = false;
    VisualizerMode currentMode = MODE_SPECTRUM_16BAND;

    void renderSpectrum(const int16_t* samples, size_t count);
    void renderOscilloscope(const int16_t* samples, size_t count);
    void renderSplMeter(const int16_t* samples, size_t count);

    float calculateRms(const int16_t* samples, size_t count);
};

} // namespace AudioMic

#endif // __MIC_VISUALIZER_H__
