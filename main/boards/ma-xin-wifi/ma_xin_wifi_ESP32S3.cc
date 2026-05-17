#include "dual_network_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "power_manager.h"
#include "power_save_timer.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include "esp_private/sdmmc_common.h"
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define TAG "MAXIN_WIFIESP32S3"

class MAXINWIFIESP32S3 : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    SpiLcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_err_t err;
    bool is_sdcard_found = false;
    QueueHandle_t vibrate_event_queue_;
    TaskHandle_t vibrate_task_handle_;

    void InitializePowerManager() {
        power_manager_ = new PowerManager(POWER_USB_IN);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            power_manager_->shutdown();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;
        io_config.dc_gpio_num = DISPLAY_DC;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.flags.spiv3_cs_connect_active_low = 0;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io_));

        ESP_LOGD(TAG, "Install ST7789 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.env_work_mode = 0;
        panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeSdcard() {
        if (SD_SPI_HOST < 0) {
            ESP_LOGI(TAG, "SD card not supported on this board");
            return;
        }
        spi_bus_config_t bus_cnf = {};
        bus_cnf.mosi_io_num = SD_DATA0;
        bus_cnf.miso_io_num = SD_CMD;
        bus_cnf.sclk_io_num = SD_CLK;
        bus_cnf.quadwp_io_num = GPIO_NUM_NC;
        bus_cnf.quadhd_io_num = GPIO_NUM_NC;
        bus_cnf.host_id = SD_SPI_HOST;
        bus_cnf.gpio_cd = GPIO_NUM_NC;
        bus_cnf.gpio_wp = GPIO_NUM_NC;
        bus_cnf.gpio_int = GPIO_NUM_NC;

        esp_err_t err = spi_bus_initialize(SD_SPI_HOST, &bus_cnf, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
            return;
        }

        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = SD_CS;
        slot_config.host = SD_SPI_HOST;

        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = 10000;

        sdmmc_card_t* card;
        err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &card);
        if (err == ESP_OK) {
            is_sdcard_found = true;
            ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
        } else {
            ESP_LOGW(TAG, "SD card mount failed: %s", esp_err_to_name(err));
        }
    }

    void InitializeAudio() {
        auto audio_codec = new Es8311AudioCodec(AUDIO_CODEC_ES8311_ADDR);
        audio_codec->SetInputMode(AUDIO_INPUT_REFERENCE ? AudioInputMode::AUDIO_INPUT_REFERENCE : AudioInputMode::AUDIO_INPUT_MIC);
        audio_codec->SetVolume(70);
        audio_codec->SetOutputMode(AudioOutputMode::AUDIO_OUTPUT_DAC);

        static gpio_num_t s_audio_i2s_pins[] = {
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
        };
        static SingleLed led(BUILTIN_LED_GPIO);

        InitializeCodecI2c();
        auto codec = new BoxAudioCodec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_I2C_SDA_PIN, AUDIO_CODEC_I2C_SCL_PIN);

        codec->SetPaPin(AUDIO_CODEC_I2C_PA_EN);
        SetAudioCodec(codec);
        SetBacklight(new PwmBacklight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT));
        SetLed(&led);
    }

public:
    MAXINWIFIESP32S3() : DualNetworkBoard("MAXINWIFIESP32S3") {
    }

    void InitializeButtons() override {
        boot_button_.OnClick([this]() {
            if (power_save_timer_) {
                power_save_timer_->UserActivityDetected();
            }
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            if (codec) {
                int vol = codec->output_volume();
                vol = std::min(100, vol + 10);
                codec->SetOutputVolume(vol);
                GetDisplay()->ShowNotification("Volume: " + std::to_string(vol));
            }
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            if (codec) {
                int vol = codec->output_volume();
                vol = std::max(0, vol - 10);
                codec->SetOutputVolume(vol);
                GetDisplay()->ShowNotification("Volume: " + std::to_string(vol));
            }
        });
    }

    void InitializeDisplay() override {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
    }

    void InitializePowerManagement() override {
        InitializePowerManager();
        InitializePowerSaveTimer();
    }

    void InitializeStorage() override {
        InitializeSdcard();
    }

    void InitializeAudio() override {
        InitializeAudio();
    }

    Display* GetDisplay() override {
        return display_;
    }

    void ProcessNetworkEvent(NetworkEvent network_event) override {
        switch (network_event) {
        case NetworkEvent::kNetworkConnected:
            ESP_LOGI(TAG, "Network connected");
            GetDisplay()->SetStatus("WiFi OK");
            break;
        case NetworkEvent::kNetworkDisconnected:
            ESP_LOGI(TAG, "Network disconnected");
            GetDisplay()->SetStatus("WiFi ...");
            break;
        case NetworkEvent::kNetworkFailure:
            ESP_LOGI(TAG, "Network failure");
            GetDisplay()->SetStatus("WiFi Err");
            break;
        default:
            break;
        }
    }
};

REGISTER_BOARD(MAXINWIFIESP32S3);