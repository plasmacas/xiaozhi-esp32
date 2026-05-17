#ifndef _MA_XIN_WIFI_LCD_DISPLAY_H_
#define _MA_XIN_WIFI_LCD_DISPLAY_H_

#include "display/lcd_display.h"
#include "lvgl_theme.h"
#include <string>

// Forward declarations
class LvglTheme;

class MaXinWifiLcdDisplay : public LcdDisplay {
private:
    // View containers
    lv_obj_t* main_screen_ = nullptr;      // Original chat/voice screen
    lv_obj_t* weather_screen_ = nullptr;  // Weather display screen
    lv_obj_t* calendar_screen_ = nullptr; // Calendar display screen
    lv_obj_t* tab_bar_ = nullptr;         // Tab navigation bar
    
    // Tab buttons
    lv_obj_t* tab_main_ = nullptr;
    lv_obj_t* tab_weather_ = nullptr;
    lv_obj_t* tab_calendar_ = nullptr;
    
    // Current view
    int current_view_ = 0;  // 0=main, 1=weather, 2=calendar
    
    // Weather data display elements
    lv_obj_t* weather_icon_label_ = nullptr;
    lv_obj_t* weather_temp_label_ = nullptr;
    lv_obj_t* weather_condition_label_ = nullptr;
    lv_obj_t* weather_humidity_label_ = nullptr;
    lv_obj_t* weather_date_label_ = nullptr;
    
    // Calendar data display elements
    lv_obj_t* calendar_date_label_ = nullptr;
    lv_obj_t* calendar_weekday_label_ = nullptr;
    lv_obj_t* calendar_month_label_ = nullptr;
    lv_obj_t* calendar_year_label_ = nullptr;
    
    bool custom_ui_initialized_ = false;
    lv_timer_t* init_check_timer_ = nullptr;

    void CreateWeatherScreen(lv_obj_t* parent);
    void CreateCalendarScreen(lv_obj_t* parent);
    void CreateTabBar(lv_obj_t* parent);
    
    static void InitCheckTimerCb(lv_timer_t* timer);
    static void TabMainClick(lv_event_t* e);
    static void TabWeatherClick(lv_event_t* e);
    static void TabCalendarClick(lv_event_t* e);

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    MaXinWifiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, 
                        esp_lcd_panel_handle_t panel,
                        int width,
                        int height,
                        int offset_x,
                        int offset_y,
                        bool mirror_x,
                        bool mirror_y,
                        bool swap_xy);
    virtual ~MaXinWifiLcdDisplay();
    
    virtual void SetupUI() override;
    
    // Weather display methods
    void ShowWeather(const char* icon, const char* temperature, const char* condition, const char* humidity);
    void UpdateWeatherData(const char* icon, const char* temp, const char* condition, const char* humidity);
    
    // Calendar display methods
    void UpdateCalendar(int year, int month, int day, int weekday);
    
    // Tab navigation
    void ShowMainView();
    void ShowWeatherView();
    void ShowCalendarView();
    
    bool IsCustomUiInitialized() const { return custom_ui_initialized_; }
};

#endif // _MA_XIN_WIFI_LCD_DISPLAY_H_