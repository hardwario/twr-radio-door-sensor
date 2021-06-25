#include <application.h>

// Every 2 hours
#define PERIODIC_REPORT_INTERVAL (2 * 60 * 60 * 1000)
// Every 15 minutes
#define TEMPERATURE_UPDATE_INTERVAL (15 * 60 * 1000)
// Every 24 hours
#define BATTERY_UPDATE_INTERVAL (24 * 60 * 60 * 1000)

twr_led_t led;
twr_button_t button;
uint16_t button_event_count = 0;

// Thermometer instance
twr_tmp112_t tmp112;
event_param_t temperature_event_param = { .next_pub = 0 };

// Door sensors on channel A and B on the Sensor Module (pins P4, P5)
twr_switch_t door_sensor_a;
twr_switch_t door_sensor_b;

void door_sensor_event_handler(twr_switch_t *self, twr_switch_event_t event, void *event_param)
{
    char topic[64];
    char channel = (self == &door_sensor_a) ? 'a' : 'b';
    snprintf(topic, sizeof(topic), "door-sensor/%c/state", channel);

    twr_led_pulse(&led, 100);

    if (event == TWR_SWITCH_EVENT_OPENED)
    {
        twr_log_info("Door %c TWR_SWITCH_EVENT_OPENED", channel);
        bool event = true;
        twr_radio_pub_bool(topic, &event);
    }
    else if (event == TWR_SWITCH_EVENT_CLOSED)
    {
        twr_log_info("Door %c TWR_SWITCH_EVENT_CLOSED", channel);
        bool event = false;
        twr_radio_pub_bool(topic, &event);
    }

    twr_log_debug("Door state %s", twr_switch_get_state(self) ? "OPEN" : "CLOSE");
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    static uint16_t event_count = 0;
    static uint16_t hold_count = 0;

    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        twr_led_pulse(&led, 100);
        event_count++;
        twr_radio_pub_push_button(&event_count);
    }

    if (event == TWR_BUTTON_EVENT_HOLD)
    {
        twr_led_pulse(&led, 400);
        hold_count++;
        twr_radio_pub_int("push-button/-/hold-count", (int*)&hold_count);
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        if (twr_tmp112_get_temperature_celsius(self, &value))
        {
            twr_radio_pub_temperature(param->channel, &value);
        }
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
        }
    }
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DEBUG, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, &button_event_count);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    twr_switch_init(&door_sensor_a, TWR_GPIO_P4, TWR_SWITCH_TYPE_NC, TWR_SWITCH_PULL_UP_DYNAMIC);
    twr_switch_set_event_handler(&door_sensor_a, door_sensor_event_handler, NULL);
    //twr_switch_set_pull_advance_time(&door_sensor_a, 500);

    twr_switch_init(&door_sensor_b, TWR_GPIO_P5, TWR_SWITCH_TYPE_NC, TWR_SWITCH_PULL_UP_DYNAMIC);
    twr_switch_set_event_handler(&door_sensor_b, door_sensor_event_handler, NULL);

    // Initialize thermometer sensor on core module
    temperature_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_INTERVAL);

    twr_radio_pairing_request("door-sensor", VERSION);

    twr_led_pulse(&led, 2000);
}

void application_task()
{
    // Periodic reporting
    bool state_a = twr_switch_get_state(&door_sensor_a);
    bool state_b = twr_switch_get_state(&door_sensor_b);

    twr_radio_pub_bool("door-sensor/a/state", &state_a);
    twr_radio_pub_bool("door-sensor/b/state", &state_b);

    twr_scheduler_plan_current_relative(PERIODIC_REPORT_INTERVAL);
}
