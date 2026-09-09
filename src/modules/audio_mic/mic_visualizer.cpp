#include "mic_visualizer.h"
#if defined(MIC_SPM1423) || defined(MIC_INMP441)
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"
#include "driver/i2s_pdm.h"
#include <fft.h>
#include <math.h>

namespace AudioMic {

static const size_t FFT_POINTS = 256;
static i2s_chan_handle_t i2s_rx_chan = nullptr;

static float bandPeaks[16] = {0};
static float bandValues[16] = {0};
static const uint8_t BAND_BIN_MAP[17] = {
    1, 2, 3, 5, 7, 10, 14, 19, 25, 33, 43, 56, 72, 90, 110, 127, 128
};

MicVisualizer& MicVisualizer::getInstance() {
    static MicVisualizer instance;
    return instance;
}

MicVisualizer::MicVisualizer() {}
MicVisualizer::~MicVisualizer() {
    deinit();
}

bool MicVisualizer::init() {
    if (isInitialized) return true;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = FFT_POINTS;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &i2s_rx_chan);
    if (err != ESP_OK) return false;

    i2s_pdm_rx_clk_config_t clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(44100);
    i2s_pdm_rx_slot_config_t slot_cfg =
        I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;

    const i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .clk = (gpio_num_t)PIN_CLK,
            .din = (gpio_num_t)PIN_DATA,
            .invert_flags = {.clk_inv = false},
        },
    };

    err = i2s_channel_init_pdm_rx_mode(i2s_rx_chan, &pdm_cfg);
    if (err == ESP_OK) {
        err = i2s_channel_enable(i2s_rx_chan);
    }

    if (err == ESP_OK) {
        isInitialized = true;
        return true;
    } else {
        if (i2s_rx_chan) {
            i2s_del_channel(i2s_rx_chan);
            i2s_rx_chan = nullptr;
        }
        return false;
    }
}

void MicVisualizer::deinit() {
    if (!isInitialized) return;

    if (i2s_rx_chan) {
        i2s_channel_disable(i2s_rx_chan);
        i2s_del_channel(i2s_rx_chan);
        i2s_rx_chan = nullptr;
    }

    gpio_reset_pin((gpio_num_t)PIN_CLK);
    isInitialized = false;
}

float MicVisualizer::calculateRms(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0f;
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += (double)samples[i] * (double)samples[i];
    }
    return sqrtf((float)(sum / count));
}

void MicVisualizer::renderSpectrum(const int16_t* samples, size_t count) {
    // Perform 256-point FFT with tiny ~1KB memory footprint
    fft_config_t* plan = fft_init(FFT_POINTS, FFT_REAL, FFT_FORWARD, NULL, NULL);
    if (!plan) return;

    for (size_t i = 0; i < FFT_POINTS; i++) {
        plan->input[i] = (float)samples[i] / 32768.0f;
    }
    fft_execute(plan);

    const int barWidth = 12;
    const int gap = 2;
    const int startX = (tftWidth - (16 * (barWidth + gap) - gap)) / 2;
    const int baseLineY = tftHeight - 16;
    const int maxBarHeight = baseLineY - 26;

    // Header
    tft.fillRect(0, 0, tftWidth, 16, 0x0841);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, 0x0841);
    tft.setCursor(4, 4);
    tft.print("16-BAND SPECTRUM | SPM1423 PDM");

    for (int band = 0; band < 16; band++) {
        int binStart = BAND_BIN_MAP[band];
        int binEnd = BAND_BIN_MAP[band + 1];
        float magSum = 0.0f;
        for (int b = binStart; b < binEnd; b++) {
            float re = plan->output[2 * b];
            float im = plan->output[2 * b + 1];
            magSum += sqrtf(re * re + im * im);
        }
        float avgMag = magSum / (binEnd - binStart);
        // Boost high frequencies slightly for visual balance
        avgMag *= (1.0f + (float)band * 0.15f);

        // Smooth attack and release
        float targetHeight = avgMag * maxBarHeight * 8.0f;
        if (targetHeight > maxBarHeight) targetHeight = maxBarHeight;

        if (targetHeight > bandValues[band]) {
            bandValues[band] = targetHeight; // Fast attack
        } else {
            bandValues[band] *= 0.82f; // Smooth decay
        }

        // Peak hold
        if (bandValues[band] > bandPeaks[band]) {
            bandPeaks[band] = bandValues[band];
        } else {
            bandPeaks[band] -= 0.6f;
            if (bandPeaks[band] < 0) bandPeaks[band] = 0;
        }

        int x = startX + band * (barWidth + gap);
        int curH = (int)bandValues[band];
        int peakH = (int)bandPeaks[band];

        // Clear above bar
        tft.fillRect(x, 20, barWidth, maxBarHeight - curH, TFT_BLACK);

        // Draw segmented bar with gradient
        for (int segY = 0; segY < curH; segY += 2) {
            uint16_t segCol = 0x07E0; // Green
            if (segY > maxBarHeight * 0.70f) segCol = 0xF800; // Red
            else if (segY > maxBarHeight * 0.45f) segCol = 0xFD20; // Amber
            else if (segY > maxBarHeight * 0.25f) segCol = 0xFFE0; // Yellow

            tft.drawFastHLine(x, baseLineY - segY, barWidth, segCol);
        }

        // Draw Peak pip
        if (peakH > 0 && peakH <= maxBarHeight) {
            tft.drawFastHLine(x, baseLineY - peakH, barWidth, TFT_WHITE);
        }
    }

    fft_destroy(plan);

    // Footer
    tft.fillRect(0, tftHeight - 12, tftWidth, 12, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(startX, tftHeight - 10);
    tft.print("B: NEXT MODE | PWR/ESC: EXIT");
}

void MicVisualizer::renderOscilloscope(const int16_t* samples, size_t count) {
    tft.fillRect(0, 0, tftWidth, 16, 0x0841);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, 0x0841);
    tft.setCursor(4, 4);
    tft.print("OSCILLOSCOPE | 44.1kHz PCM");

    const int scopeY = 20;
    const int scopeH = tftHeight - 34;
    const int midY = scopeY + scopeH / 2;
    const int scopeW = tftWidth - 8;

    // Clear wave area
    tft.fillRect(4, scopeY, scopeW, scopeH, 0x0020); // Dark CRT phosphor tint
    tft.drawRect(4, scopeY, scopeW, scopeH, 0x03E0);

    // Center graticule line
    tft.drawFastHLine(4, midY, scopeW, 0x01E0);

    // Find trigger point (positive zero-crossing)
    size_t trigIdx = 0;
    for (size_t i = 1; i < count / 2; i++) {
        if (samples[i - 1] < 0 && samples[i] >= 0) {
            trigIdx = i;
            break;
        }
    }

    int prevX = 4;
    int prevY = midY;

    int16_t minVal = 32767;
    int16_t maxVal = -32768;

    for (int x = 0; x < scopeW; x++) {
        size_t sampleIdx = trigIdx + x;
        if (sampleIdx >= count) break;

        int16_t s = samples[sampleIdx];
        if (s < minVal) minVal = s;
        if (s > maxVal) maxVal = s;

        int y = midY - (int)((s / 32768.0f) * (scopeH / 2 - 2));
        if (y < scopeY + 1) y = scopeY + 1;
        if (y > scopeY + scopeH - 2) y = scopeY + scopeH - 2;

        if (x > 0) {
            tft.drawLine(prevX, prevY, 4 + x, y, 0x07E0); // Bright phosphor green
        }
        prevX = 4 + x;
        prevY = y;
    }

    // Telemetry
    tft.setTextColor(TFT_WHITE, 0x0020);
    tft.setCursor(8, scopeY + 4);
    tft.printf("Vpp: %d", maxVal - minVal);

    tft.fillRect(0, tftHeight - 12, tftWidth, 12, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(4, tftHeight - 10);
    tft.print("B: NEXT MODE | PWR/ESC: EXIT");
}

void MicVisualizer::renderSplMeter(const int16_t* samples, size_t count) {
    float rms = calculateRms(samples, count);
    float dbSpl = 0.0f;
    if (rms > 1.0f) {
        dbSpl = 20.0f * log10f(rms / 32768.0f) + 94.0f;
        if (dbSpl < 30.0f) dbSpl = 30.0f;
        if (dbSpl > 120.0f) dbSpl = 120.0f;
    } else {
        dbSpl = 30.0f;
    }

    static float maxDb = 30.0f;
    static float minDb = 120.0f;
    if (dbSpl > maxDb) maxDb = dbSpl;
    if (dbSpl < minDb && dbSpl > 35.0f) minDb = dbSpl;

    // Decay peak max slightly over time
    maxDb -= 0.05f;

    tft.fillRect(0, 0, tftWidth, 16, 0x0841);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, 0x0841);
    tft.setCursor(4, 4);
    tft.print("SPL DECIBEL / VU METER");

    // Big digital dB readout
    tft.fillRect(10, 22, tftWidth - 20, 36, 0x10A2);
    tft.drawRect(10, 22, tftWidth - 20, 36, TFT_CYAN);

    tft.setTextSize(3);
    uint16_t valCol = 0x07E0;
    if (dbSpl > 85.0f) valCol = 0xF800;
    else if (dbSpl > 70.0f) valCol = 0xFFE0;

    tft.setTextColor(valCol, 0x10A2);
    tft.setCursor(20, 28);
    tft.printf("%.1f dB", dbSpl);

    // Min / Max telemetry
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, 0x10A2);
    tft.setCursor(160, 26);
    tft.printf("MAX: %.1f", maxDb);
    tft.setCursor(160, 42);
    tft.printf("MIN: %.1f", minDb);

    // Segmented Horizontal VU Bar
    const int vuY = 66;
    const int vuW = tftWidth - 20;
    const int segCount = 24;
    const int segW = (vuW - (segCount - 1) * 2) / segCount;
    int activeSegs = map((int)constrain(dbSpl, 35.0f, 105.0f), 35, 105, 1, segCount);

    for (int s = 0; s < segCount; s++) {
        int sx = 10 + s * (segW + 2);
        uint16_t sc = 0x07E0; // Green
        if (s > 18) sc = 0xF800; // Red
        else if (s > 12) sc = 0xFFE0; // Yellow

        if (s < activeSegs) {
            tft.fillRect(sx, vuY, segW, 18, sc);
        } else {
            tft.fillRect(sx, vuY, segW, 18, 0x2104); // Dark segment placeholder
        }
    }

    // VU dB scale tick marks
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(10, vuY + 22);
    tft.print("40dB          60dB          80dB         100dB");

    tft.fillRect(0, tftHeight - 12, tftWidth, 12, TFT_BLACK);
    tft.setCursor(4, tftHeight - 10);
    tft.print("B: NEXT MODE | PWR/ESC: EXIT");
}

void MicVisualizer::run() {
    if (!init()) {
        displayError("SPM1423 I2S Init Failed", true);
        return;
    }

    tft.fillScreen(TFT_BLACK);

    int16_t sampleBuffer[FFT_POINTS];
    unsigned long lastFrame = 0;

    while (true) {
        InputHandler();

        if (check(EscPress)) break;

        // Button B (DW_BTN) cycles mode
        if (check(NextPress)) {
            currentMode = (VisualizerMode)((currentMode + 1) % MODE_COUNT);
            tft.fillScreen(TFT_BLACK);
        }

        size_t bytesRead = 0;
        esp_err_t res = i2s_channel_read(
            i2s_rx_chan, (char*)sampleBuffer, FFT_POINTS * sizeof(int16_t), &bytesRead, pdMS_TO_TICKS(50)
        );

        if (res == ESP_OK && bytesRead > 0) {
            if (millis() - lastFrame >= 25) { // 40 FPS
                lastFrame = millis();
                switch (currentMode) {
                    case MODE_SPECTRUM_16BAND:
                        renderSpectrum(sampleBuffer, FFT_POINTS);
                        break;
                    case MODE_OSCILLOSCOPE:
                        renderOscilloscope(sampleBuffer, FFT_POINTS);
                        break;
                    case MODE_SPL_VU_METER:
                        renderSplMeter(sampleBuffer, FFT_POINTS);
                        break;
                    default:
                        break;
                }
            }
        }
        delay(1);
    }

    deinit();
    tft.fillScreen(mopieConfig.bgColor);
}

} // namespace AudioMic
#endif
