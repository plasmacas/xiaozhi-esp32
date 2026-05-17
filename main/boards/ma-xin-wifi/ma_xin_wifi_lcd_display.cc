#include "ma_xin_wifi_lcd_display.h"
#include "lvgl_theme.h"
#include "display_lock_guard.h"
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "MaXinWifiLcdDisplay";

static const char* WEEKDAY_NAMES[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char* MONTH_NAMES[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

MaXinWifiLcdDisplay::MaXinWifiLcdDisplay(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_handle_t panel,
    int width, int height,
    int offset_x, int offset_y,
    bool mirror_x, bool mirror_y,
    bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height),
      custom_ui_initialized_(false),
      current_view_(0) {
    init_check_timer_ = lv_timer_create(InitCheckTimerCb, 100, this);
    if (init_check_timer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create init check timer");
    } else {
        ESP_LOGI(TAG, "Init check timer created (100ms interval)");
    }
}

MaXinWifiLcdDisplay::~MaXinWifiLcdDisplay() {
    if (init_check_timer_ != nullptr) {
        lv_timer_del(init_check_timer_);
        init_check_timer_ = nullptr;
    }
}

void MaXinWifiLcdDisplay::InitCheckTimerCb(lv_timer_t* timer) {
    MaXinWifiLcdDisplay* self = static_cast<MaXinWifiLcdDisplay*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        lv_timer_del(timer);
        return;
    }

    if (self->IsSetupUICalled() && !self->custom_ui_initialized_) {
        ESP_LOGI(TAG, "SetupUI complete, initializing custom weather/calendar UI");
        self->custom_ui_initialized_ = true;
        lv_timer_del(timer);
        self->init_check_timer_ = nullptr;
    }
}

void MaXinWifiLcdDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() already called, skipping");
        return;
    }

    LcdDisplay::SetupUI();
    
    if (!custom_ui_initialized_) {
        return;
    }

    DisplayLockGuard lock(this);
    
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto screen = lv_screen_active();

    // Create tab bar at the bottom
    tab_bar_ = lv_obj_create(screen);
    lv_obj_set_size(tab_bar_, LV_HOR_RES, text_font->line_height + 16);
    lv_obj_align(tab_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(tab_bar_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_radius(tab_bar_, 0, 0);
    lv_obj_set_style_border_width(tab_bar_, 0, 0);
    lv_obj_set_flex_flow(tab_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_bar_, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tab_bar_, 4, 0);

    // Tab: Main
    tab_main_ = lv_btn_create(tab_bar_);
    lv_obj_set_size(tab_main_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(tab_main_, 4, 0);
    lv_obj_set_style_bg_color(tab_main_, lvgl_theme->border_color(), 0);
    lv_obj_t* lbl_main = lv_label_create(tab_main_);
    lv_label_set_text(lbl_main, "[Home]");
    lv_obj_set_style_text_font(lbl_main, icon_font, 0);
    lv_obj_add_event_cb(tab_main_, TabMainClick, LV_EVENT_CLICKED, this);

    // Tab: Weather
    tab_weather_ = lv_btn_create(tab_bar_);
    lv_obj_set_size(tab_weather_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(tab_weather_, 4, 0);
    lv_obj_set_style_bg_color(tab_weather_, lvgl_theme->border_color(), 0);
    lv_obj_t* lbl_weather = lv_label_create(tab_weather_);
    lv_label_set_text(lbl_weather, "[Weather]");
    lv_obj_set_style_text_font(lbl_weather, icon_font, 0);
    lv_obj_add_event_cb(tab_weather_, TabWeatherClick, LV_EVENT_CLICKED, this);

    // Tab: Calendar
    tab_calendar_ = lv_btn_create(tab_bar_);
    lv_obj_set_size(tab_calendar_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(tab_calendar_, 4, 0);
    lv_obj_set_style_bg_color(tab_calendar_, lvgl_theme->border_color(), 0);
    lv_obj_t* lbl_calendar = lv_label_create(tab_calendar_);
    lv_label_set_text(lbl_calendar, "[Calendar]");
    lv_obj_set_style_text_font(lbl_calendar, icon_font, 0);
    lv_obj_add_event_cb(tab_calendar_, TabCalendarClick, LV_EVENT_CLICKED, this);

    // Initially hide content and show only main view
    if (content_) {
        lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }

    ESP_LOGI(TAG, "Custom MaXin WiFi UI with weather/calendar initialized");
}

void MaXinWifiLcdDisplay::TabMainClick(lv_event_t* e) {
    MaXinWifiLcdDisplay* self = static_cast<MaXinWifiLcdDisplay*>(lv_event_get_user_data(e));
    self->ShowMainView();
}

void MaXinWifiLcdDisplay::TabWeatherClick(lv_event_t* e) {
    MaXinWifiLcdDisplay* self = static_cast<MaXinWifiLcdDisplay*>(lv_event_get_user_data(e));
    self->ShowWeatherView();
}

void MaXinWifiLcdDisplay::TabCalendarClick(lv_event_t* e) {
    MaXinWifiLcdDisplay* self = static_cast<MaXinWifiLcdDisplay*>(lv_event_get_user_data(e));
    self->ShowCalendarView();
}

void MaXinWifiLcdDisplay::ShowMainView() {
    if (!custom_ui_initialized_) return;
    
    current_view_ = 0;
    if (content_) {
        lv_obj_remove_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }
    if (weather_screen_) {
        lv_obj_add_flag(weather_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (calendar_screen_) {
        lv_obj_add_flag(calendar_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Highlight active tab
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    if (tab_main_) {
        lv_obj_set_style_bg_color(tab_main_, lvgl_theme->text_color(), 0);
    }
    if (tab_weather_) {
        lv_obj_set_style_bg_color(tab_weather_, lvgl_theme->border_color(), 0);
    }
    if (tab_calendar_) {
        lv_obj_set_style_bg_color(tab_calendar_, lvgl_theme->border_color(), 0);
    }
}

void MaXinWifiLcdDisplay::ShowWeatherView() {
    if (!custom_ui_initialized_) return;
    
    current_view_ = 1;
    if (content_) {
        lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }
    if (weather_screen_) {
        lv_obj_remove_flag(weather_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (calendar_screen_) {
        lv_obj_add_flag(calendar_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    if (tab_main_) {
        lv_obj_set_style_bg_color(tab_main_, lvgl_theme->border_color(), 0);
    }
    if (tab_weather_) {
        lv_obj_set_style_bg_color(tab_weather_, lvgl_theme->text_color(), 0);
    }
    if (tab_calendar_) {
        lv_obj_set_style_bg_color(tab_calendar_, lvgl_theme->border_color(), 0);
    }
}

void MaXinWifiLcdDisplay::ShowCalendarView() {
    if (!custom_ui_initialized_) return;
    
    current_view_ = 2;
    if (content_) {
        lv_obj_add_flag(content_, LV_OBJ_FLAG_HIDDEN);
    }
    if (weather_screen_) {
        lv_obj_add_flag(weather_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (calendar_screen_) {
        lv_obj_remove_flag(calendar_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    if (tab_main_) {
        lv_obj_set_style_bg_color(tab_main_, lvgl_theme->border_color(), 0);
    }
    if (tab_weather_) {
        lv_obj_set_style_bg_color(tab_weather_, lvgl_theme->border_color(), 0);
    }
    if (tab_calendar_) {
        lv_obj_set_style_bg_color(tab_calendar_, lvgl_theme->text_color(), 0);
    }
}

void MaXinWifiLcdDisplay::UpdateWeatherData(const char* icon, const char* temp, const char* condition, const char* humidity) {
    if (!custom_ui_initialized_) return;
    
    DisplayLockGuard lock(this);
    
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto icon_font = lvgl_theme->icon_font()->font();
    auto text_font = lvgl_theme->text_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();
    
    // Lazy create weather screen
    if (weather_screen_ == nullptr) {
        auto screen = lv_screen_active();
        weather_screen_ = lv_obj_create(screen);
        lv_obj_set_size(weather_screen_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_radius(weather_screen_, 0, 0);
        lv_obj_set_style_bg_color(weather_screen_, lvgl_theme->background_color(), 0);
        lv_obj_set_style_border_width(weather_screen_, 0, 0);
        lv_obj_align(weather_screen_, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_flag(weather_screen_, LV_OBJ_FLAG_HIDDEN);
        
        // Weather icon (large emoji or symbol)
        weather_icon_label_ = lv_label_create(weather_screen_);
        lv_obj_center(weather_icon_label_);
        lv_obj_set_style_text_font(weather_icon_label_, large_icon_font, 0);
        lv_obj_set_style_text_color(weather_icon_label_, lvgl_theme->text_color(), 0);
        lv_label_set_text(weather_icon_label_, icon ? icon : "[Weather]");
        
        // Temperature
        weather_temp_label_ = lv_label_create(weather_screen_);
        lv_obj_align(weather_icon_label_, LV_ALIGN_BOTTOM_MID, 0, -large_icon_font->line_height * 3);
        lv_obj_set_style_text_font(weather_temp_label_, text_font, 0);
        lv_obj_set_style_text_color(weather_temp_label_, lvgl_theme->text_color(), 0);
        lv_label_set_text(weather_temp_label_, temp ? temp : "--掳C");
        
        // Condition text
        weather_condition_label_ = lv_label_create(weather_screen_);
        lv_obj_align(weather_temp_label_, LV_ALIGN_BOTTOM_MID, 0, -text_font->line_height * 2);
        lv_obj_set_style_text_font(weather_condition_label_, text_font, 0);
        lv_obj_set_style_text_color(weather_condition_label_, lvgl_theme->text_color(), 0);
        lv_label_set_text(weather_condition_label_, condition ? condition : "Loading...");
        
        // Humidity
        weather_humidity_label_ = lv_label_create(weather_screen_);
        lv_obj_align(weather_condition_label_, LV_ALIGN_BOTTOM_MID, 0, -text_font->line_height * 2);
        lv_obj_set_style_text_font(weather_humidity_label_, text_font, 0);
        lv_obj_set_style_text_color(weather_humidity_label_, lvgl_theme->text_color(), 0);
        lv_label_set_text(weather_humidity_label_, humidity ? humidity : "Humidity: --%");
        
        // Date
        weather_date_label_ = lv_label_create(weather_screen_);
        lv_obj_align(weather_humidity_label_, LV_ALIGN_BOTTOM_MID, 0, -text_font->line_height * 2);
        lv_obj_set_style_text_font(weather_date_label_, text_font, 0);
        lv_obj_set_style_text_color(weather_date_label_, lvgl_theme->text_color(), 0);
        lv_label_set_text(weather_date_label_, "");
        
        ESP_LOGI(TAG, "Weather screen created");
    }
    
    // Update weather data
    if (weather_icon_label_ && icon) {
        lv_label_set_text(weather_icon_label_, icon);
    }
    if (weather_temp_label_ && temp) {
        lv_label_set_text(weather_temp_label_, temp);
    }
    if (weather_condition_label_ && condition) {
        lv_label_set_text(weather_condition_label_, condition);
    }
    if (weather_humidity_label_ && humidity) {
        lv_label_set_text(weather_humidity_label_, humidity);
    }
}

void MaXinWifiLcdDisplay::UpdateCalendar(int year, int month, int day, int weekday) {
    if (!custom_ui_initialized_) return;
    
    DisplayLockGuard lock(this);
    
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();
    
    // Lazy create calendar screen
    if (calendar_screen_ == nullptr) {
        auto screen = lv_screen_active();
        calendar_screen_ = lv_obj_create(screen);
        lv_obj_set_size(calendar_screen_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_radius(calendar_screen_, 0, 0);
        lv_obj_set_style_bg_color(calendar_screen_, lvgl_theme->background_color(), 0);
        lv_obj_set_style_border_width(calendar_screen_, 0, 0);
        lv_obj_align(calendar_screen_, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_flag(calendar_screen_, LV_OBJ_FLAG_HIDDEN);
        
        // Large day number
        calendar_date_label_ = lv_label_create(calendar_screen_);
        lv_obj_center(calendar_date_label_);
        lv_obj_set_style_text_font(calendar_date_label_, large_icon_font, 0);
        lv_obj_set_style_text_color(calendar_date_label_, lvgl_theme->text_color(), 0);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", day);
        lv_label_set_text(calendar_date_label_, buf);
        
        // Weekday name
        calendar_weekday_label_ = lv_label_create(calendar_screen_);
        lv_obj_align(calendar_date_label_, LV_ALIGN_BOTTOM_MID, 0, -large_icon_font->line_height * 3);
        lv_obj_set_style_text_font(calendar_weekday_label_, text_font, 0);
        lv_obj_set_style_text_color(calendar_weekday_label_, lvgl_theme->text_color(), 0);
        const char* wday = (weekday >= 0 && weekday <= 6) ? WEEKDAY_NAMES[weekday] : "Unknown";
        lv_label_set_text(calendar_weekday_label_, wday);
        
        // Month + Year
        calendar_month_label_ = lv_label_create(calendar_screen_);
        lv_obj_align(calendar_weekday_label_, LV_ALIGN_BOTTOM_MID, 0, -text_font->line_height * 2);
        lv_obj_set_style_text_font(calendar_month_label_, text_font, 0);
        lv_obj_set_style_text_color(calendar_month_label_, lvgl_theme->text_color(), 0);
        snprintf(buf, sizeof(buf), "%s %d", (month >= 1 && month <= 12) ? MONTH_NAMES[month - 1] : "Unknown", year);
        lv_label_set_text(calendar_month_label_, buf);
        
        ESP_LOGI(TAG, "Calendar screen created");
    }
    
    // Update calendar data
    if (calendar_date_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", day);
        lv_label_set_text(calendar_date_label_, buf);
    }
    if (calendar_weekday_label_) {
        const char* wday = (weekday >= 0 && weekday <= 6) ? WEEKDAY_NAMES[weekday] : "Unknown";
        lv_label_set_text(calendar_weekday_label_, wday);
    }
    if (calendar_month_label_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s %d", (month >= 1 && month <= 12) ? MONTH_NAMES[month - 1] : "Unknown", year);
        lv_label_set_text(calendar_month_label_, buf);
    }
}

void MaXinWifiLcdDisplay::ShowWeather(const char* icon, const char* temperature, const char* condition, const char* humidity) {
    ShowWeatherView();
    UpdateWeatherData(icon, temperature, condition, humidity);
}

bool MaXinWifiLcdDisplay::Lock(int timeout_ms) {
    return LcdDisplay::Lock(timeout_ms);
}

void MaXinWifiLcdDisplay::Unlock() {
    LcdDisplay::Unlock();
}