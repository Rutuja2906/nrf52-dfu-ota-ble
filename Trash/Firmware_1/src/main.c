#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/services/dis.h>
#include <zephyr/bluetooth/hci.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
//#include <zephyr/sys/crc.h>

LOG_MODULE_REGISTER(DFU,LOG_LEVEL_INF);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define CON_STATUS_LED DK_LED1
//Led blinking
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define RUN_LED_BLINK_INTERVAL 300

/* Add extra button for bond deleting function */

#define BOND_DELETE_BUTTON DK_BTN2_MSK

/* Add extra button for enablig pairing mode */

#define PAIRING_BUTTON DK_BTN3_MSK

void notify_central_device(struct bt_conn *conn, const struct bt_gatt_attr *attr)
{
    /* Your data to send to the central device */
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };

    /* Notify the central device */
    int err = bt_gatt_notify(conn, attr, &data, sizeof(data));
    if (err) {
        printk("Failed to send notification: %d\n", err);
    } else {
        printk("Sent notification to central device\n");
    }
}
/* Advertising parameter for no Accept List */
#define BT_LE_ADV_CONN_NO_ACCEPT_LIST BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

/* Advertising parameter for when Accept List is used */
#define BT_LE_ADV_CONN_ACCEPT_LIST BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN | BT_LE_ADV_OPT_ONE_TIME, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

static const struct bt_data ad[] ={
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL| BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
};

/* The callback function to add addreses to the Accept List */
static void setup_accept_list_cb(const struct bt_bond_info *info, void *user_data)
{
	int *bond_cnt = user_data;

	if ((*bond_cnt) < 0) {
		return;
	}

	int err = bt_le_filter_accept_list_add(&info->addr);
	LOG_INF("Added following peer to accept list: %x %x\n", info->addr.a.val[0],
		info->addr.a.val[1]);
	if (err) {
		LOG_INF("Cannot add peer to filter accept list (err: %d)\n", err);
		(*bond_cnt) = -EIO;
	} else {
		(*bond_cnt)++;
	}
}

/* The function to loop through the bond list */
static int setup_accept_list(uint8_t local_id)
{
	int err = bt_le_filter_accept_list_clear();

	if (err) {
		LOG_INF("Cannot clear accept list (err: %d)\n", err);
		return err;
	}
	int bond_cnt = 0;
	bt_foreach_bond(local_id, setup_accept_list_cb, &bond_cnt);
	return bond_cnt;
}

/* The function to advertise with the Accept List */
void advertise_with_acceptlist(struct k_work *work)
{
	int err = 0;
	int allowed_cnt = setup_accept_list(BT_ID_DEFAULT);
	if (allowed_cnt < 0) {
		LOG_INF("Acceptlist setup failed (err:%d)\n", allowed_cnt);
	} else {
		if (allowed_cnt == 0) {
			LOG_INF("Advertising with no Accept list \n");
			err = bt_le_adv_start(BT_LE_ADV_CONN_NO_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		} else {
			LOG_INF("Acceptlist setup number  = %d \n", allowed_cnt);
			err = bt_le_adv_start(BT_LE_ADV_CONN_ACCEPT_LIST, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
		}
		if (err) {
			LOG_INF("Advertising failed to start (err %d)\n", err);
			return;
		}
		LOG_INF("Advertising successfully started\n");
	}
}

K_WORK_DEFINE(advertise_acceptlist_work, advertise_with_acceptlist);

static bool bt_conn_is_bonded(struct bt_conn *conn)
{
bt_addr_le_t addr;
	bt_addr_le_copy(&addr, bt_conn_get_dst(conn));

	int allowed_cnt = setup_accept_list(BT_ID_DEFAULT);
	return allowed_cnt > 0;
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_INF("Connection failed (err %u)\n", err);
		return;
	}
	// Check if the device is already bonded
	if (!bt_conn_is_bonded(conn)) {
		// Set security level to require passkey if not bonded
		err = bt_conn_set_security(conn, BT_SECURITY_L2);
		if (err) {
			LOG_INF("Failed to set security level (err %d)\n", err);
		}
	}
	LOG_INF("Connected\n");
	dk_set_led_on(CON_STATUS_LED);// Turn on LED 2
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)\n", reason);
	dk_set_led_off(CON_STATUS_LED); // Turn off LED 2
	/* Start advertising with Accept List */
	k_work_submit(&advertise_acceptlist_work);
}

//Security changed to Passkey based security
static void on_security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);
	} else {
		LOG_INF("Security failed: %s level %u err %d\n", addr, level, err);
	}
}
struct bt_conn_cb connection_callbacks = {
	.connected = on_connected,
	.disconnected = on_disconnected,
	.security_changed = on_security_changed,
};
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}
static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing cancelled: %s\n", addr);
}
static void pairing_confirm(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	bt_conn_auth_pairing_confirm(conn);
	printk("Pairing confirmed: %s\n", addr);
}
static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
	.pairing_confirm = NULL
};

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	/* Add extra button handling to remove bond information */
	if (has_changed & BOND_DELETE_BUTTON) {
		uint32_t bond_delete_button_state = button_state & BOND_DELETE_BUTTON;
		if (bond_delete_button_state == 0) {
			int err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
			if (err) {
				LOG_INF("Cannot delete bond (err: %d)\n", err);
			} else {
				LOG_INF("Bond deleted succesfully");
			}
		}
	}

/* Add extra button handling to advertise without using Accept List */
	if (has_changed & PAIRING_BUTTON) {
		uint32_t pairing_button_state = button_state & PAIRING_BUTTON;
		if (pairing_button_state == 0) {
			int err_code = bt_le_adv_stop();
			if (err_code) {
				LOG_INF("Cannot stop advertising err= %d \n", err_code);
				return;
			}
			err_code = bt_le_filter_accept_list_clear();
			if (err_code) {
				LOG_INF("Cannot clear accept list (err: %d)\n", err_code);
			} else {
				LOG_INF("Accept list cleared succesfully");
			}
			err_code = bt_le_adv_start(BT_LE_ADV_CONN_NO_ACCEPT_LIST, ad,
						   ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

			if (err_code) {
				LOG_INF("Cannot start open advertising (err: %d)\n", err_code);
			} else {
				LOG_INF("Advertising in pairing mode started");
			}
		}
	}
}
static int init_button(void)
{
	int err;
	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_INF("Cannot init buttons (err: %d)\n", err);
	}
	return err;
}

int main(void)
{
	printk("Board: %s\n", CONFIG_BOARD);
	printk("Build time: " __DATE__ " " __TIME__ "\n");

	int err;
	int ret;

	LOG_INF("DFU firmware 2\n");

	err = dk_leds_init();
	if (err) {
		LOG_INF("LEDs init failed (err %d)\n", err);
		return err;
	}
	err = init_button();
	if (err) {
		LOG_INF("Button init failed (err %d)\n", err);
		return err;
	}
	
	bt_conn_cb_register(&connection_callbacks);
	unsigned int passkey = 123456;
	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
		LOG_INF("Failed to register authorization callbacks.\n");
		return err;
	}
	bt_passkey_set(passkey);

	err = bt_enable(NULL);
	if (err) {
	LOG_INF("Bluetooth init failed (err %d)\n", err);
	return err;
	}

	LOG_INF("Initialized BLE\n");
	settings_load();

	/* Start advertising with the Accept List */
	k_work_submit(&advertise_acceptlist_work);
	
	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
			printk("");
		}
		k_msleep(SLEEP_TIME_MS);
	}

}