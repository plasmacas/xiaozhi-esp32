#ifndef _POWER_MANAGER_H_
#define _POWER_MANAGER_H_

#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class PowerManager {
private:
    gpio_num_t power_usb_in_;
    adc_unit_t adc_unit_;
    adc_channel_t adc_channel_;
    esp_timer_handle_t battery_check_timer_;
    bool is_charging_;
    int battery_level_;
    std::function<void(bool)> on_charging_status_changed_;

    void InitializeAdc() {
        adc_oneshot_unit_handle_t adc_handle;
        adc_oneshot_unit_init_config_t init_config = {
            .unit_id = adc_unit_,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

        adc_oneshot_chan_cfg_t chan_config = {
            .bitwidth = ADC_BITWIDTH_12,
            .atten = ADC_ATTEN_DB_11,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel_, &chan_config));

        int raw;
        adc_oneshot_read(adc_handle, adc_channel_, &raw);
        adc_oneshot_delete_unit(adc_handle);
    }

    static void BatteryCheckTimerCb(void* param) {
        PowerManager* self = static_cast<PowerManager*>(param);
        // Read battery and update state
        // This is a simplified version - actual implementation would read ADC values
    }

public:
    PowerManager(gpio_num_t power_usb_in, adc_unit_t adc_unit = ADC_UNIT_1, adc_channel_t adc_channel = ADC_CHANNEL_6) :
        power_usb_in_(power_usb_in), adc_unit_(adc_unit), adc_channel_(adc_channel), is_charging_(false), battery_level_(100) {
        
        gpio_config_t config = {
            .pin_bit_mask = 1ULL << power_usb_in_,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&config);

        esp_timer_args_t timer_args = {
            .callback = BatteryCheckTimerCb,
            .arg = this,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &battery_check_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(battery_check_timer_, 1000000));
    }

    bool IsCharging() const { return is_charging_; }
    int BatteryLevel() const { return battery_level_; }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }

    void shutdown() {
        ESP_LOGI("PowerManager", "Shutting down...");
        esp_timer_stop(battery_check_timer_);
    }
};

#endif // _POWER_MANAGER_H_