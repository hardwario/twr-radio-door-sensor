#include <application.h>

// Every 2 hours
#define PERIODIC_REPORT_INTERVAL (2 * 60 * 60 * 1000)
// Every 15 minutes
#define TEMPERATURE_UPDATE_INTERVAL (15 * 60 * 1000)
// Every 24 hours
#define BATTERY_UPDATE_INTERVAL (24 * 60 * 60 * 1000)

bc_led_t led;
bc_button_t button;
uint16_t button_event_count = 0;

// Thermometer instance
bc_tmp112_t tmp112;
event_param_t temperature_event_param = { .next_pub = 0 };

// Door sensors on channel A and B on the Sensor Module (pins P4, P5)
bc_switch_t door_sensor_a;
bc_switch_t door_sensor_b;

void door_sensor_event_handler(bc_switch_t *self, bc_switch_event_t event, void *event_param)
{
    char topic[64];
    char channel = (self == &door_sensor_a) ? 'a' : 'b';
    snprintf(topic, sizeof(topic), "door-sensor/%c/state", channel);

    bc_led_pulse(&led, 100);

    if (event == BC_SWITCH_EVENT_OPENED)
    {
        bc_log_info("Door %c BC_SWITCH_EVENT_OPENED", channel);
        bool event = true;
        bc_radio_pub_bool(topic, &event);
    }
    else if (event == BC_SWITCH_EVENT_CLOSED)
    {
        bc_log_info("Door %c BC_SWITCH_EVENT_CLOSED", channel);
        bool event = false;
        bc_radio_pub_bool(topic, &event);
    }

    bc_log_debug("Door state %s", bc_switch_get_state(self) ? "OPEN" : "CLOSE");
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    static uint16_t event_count = 0;
    static uint16_t hold_count = 0;

    if (event == BC_BUTTON_EVENT_CLICK)
    {
        bc_led_pulse(&led, 100);
        event_count++;
        bc_radio_pub_push_button(&event_count);
    }

    if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_led_pulse(&led, 400);
        hold_count++;
        bc_radio_pub_int("push-button/-/hold-count", (int*)&hold_count);
    }
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &value))
        {
            bc_radio_pub_temperature(param->channel, &value);
        }
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

void application_init(void)
{
    bc_log_init(BC_LOG_LEVEL_DEBUG, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, &button_event_count);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);

    bc_switch_init(&door_sensor_a, BC_GPIO_P4, BC_SWITCH_TYPE_NC, BC_SWITCH_PULL_UP_DYNAMIC);
    bc_switch_set_event_handler(&door_sensor_a, door_sensor_event_handler, NULL);
    //bc_switch_set_pull_advance_time(&door_sensor_a, 500);

    bc_switch_init(&door_sensor_b, BC_GPIO_P5, BC_SWITCH_TYPE_NC, BC_SWITCH_PULL_UP_DYNAMIC);
    bc_switch_set_event_handler(&door_sensor_b, door_sensor_event_handler, NULL);

    // Initialize thermometer sensor on core module
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_INTERVAL);

    bc_radio_pairing_request("door-sensor", VERSION);

    bc_led_pulse(&led, 2000);
}

void application_task()
{
    // Periodic reporting
    bool state_a = bc_switch_get_state(&door_sensor_a);
    bool state_b = bc_switch_get_state(&door_sensor_b);

    bc_radio_pub_bool("door-sensor/a/state", &state_a);
    bc_radio_pub_bool("door-sensor/b/state", &state_b);

    bc_scheduler_plan_current_relative(PERIODIC_REPORT_INTERVAL);
}
