#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <asm-generic/bug.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <asm/system_misc.h>
#include <linux/of_gpio.h>
#include <linux/vmalloc.h>


#define BUF_SIZE 64

extern char fps_hw_info[16];
typedef struct board_id {
	int index;
	const char *hw_version;
	const char *qcn_type;
	const char *model;
} boardid_match_t;

#define BOARDID_GPIO_MAX 2
#define SKUID_GPIO_MAX 3

struct boardid_df {
	unsigned int gpio;
	char gpio_name[32];
};

static struct boardid_df board_id_gpios[BOARDID_GPIO_MAX] = {
	{.gpio_name = "gpio,boardid0"}, // gpio32
	{.gpio_name = "gpio,boardid1"}, // gpio100
};

static struct boardid_df sku_id_gpios[SKUID_GPIO_MAX] = {
	{.gpio_name = "gpio,boardid2"}, // gpio33
	{.gpio_name = "gpio,boardid3"}, // gpio102
	{.gpio_name = "gpio,boardid4"}, // gpio84
};



int parse_dt_and_request_gpio(struct boardid_df *board_info, int max_num) {
	struct device_node *of_node;
	int i = 0;
	of_node = (struct device_node *)kmalloc(sizeof(struct device_node), GFP_KERNEL);
	of_node = of_find_node_by_name(NULL,"gpio_boardid");
	for (; i < max_num; i++) {
		board_info[i].gpio = of_get_named_gpio(of_node, board_info[i].gpio_name , 0);
		printk(KERN_INFO "board[%d] = %d name = %s", i, board_info[i].gpio, board_info[i].gpio_name);
		if (gpio_is_valid(board_info[i].gpio)) {
			if (gpio_request(board_info[i].gpio, board_info[i].gpio_name)) {
				printk(KERN_INFO "%s=%d request fail!", board_info[i].gpio_name, board_info[i].gpio);
				return -1;
			}
		}
	}
	return 0;
}

int get_gpio_value(struct boardid_df *board_info, int out_value) {
	if (gpio_is_valid(board_info->gpio)) {
		if (!!gpio_direction_output(board_info->gpio, out_value)) {
			printk(KERN_INFO "%s=%d set output fail!", board_info->gpio_name, board_info->gpio);
		}
		if (!!gpio_direction_input(board_info->gpio)) {
			printk(KERN_INFO "%s=%d set intput fail!", board_info->gpio_name, board_info->gpio);
		}
		return gpio_get_value(board_info->gpio);
	}
	return 0;
}

int get_board_rel_value(int up, int down) {
	if (up != down) {
		return 1;
	} else if (up == 0) {
		return 0;
	} else {
		return 2;
	}
}

/* Shadow PD->0, NP->1, PU->2
version gpio100 gpio32 value
EVT		PD		PD		0
DVT		PD		NP		1
PVT		PD		PU		2
MP 		NP		PD		3
*/
int get_board_id(void) {
	int board0 = 0;
	int board1 = 0;
	int board0_up = 0;
	int board1_up = 0;
	int board0_down = 0;
	int board1_down = 0;
	int i = 0;
	if (parse_dt_and_request_gpio(board_id_gpios, BOARDID_GPIO_MAX)) {
		goto request_error;
	}
	board0_up = get_gpio_value(&board_id_gpios[0], 1);
	printk(KERN_INFO "board0_up = %d", board0_up);
	board0_down = get_gpio_value(&board_id_gpios[0], 0);
	printk(KERN_INFO "board0_down = %d", board0_down);
	board0 = get_board_rel_value(board0_up, board0_down);
	printk(KERN_INFO "board0 = %d", board0);
	board1_up = get_gpio_value(&board_id_gpios[1], 1);
	printk(KERN_INFO "board1_up = %d", board1_up);
	board1_down = get_gpio_value(&board_id_gpios[1], 0);
	printk(KERN_INFO "board1_down = %d", board1_down);
	board1 = get_board_rel_value(board1_up, board1_down);
	printk(KERN_INFO "board1 = %d", board1);
request_error:
	for (; i < BOARDID_GPIO_MAX; i++) {
		if (gpio_is_valid(board_id_gpios[i].gpio)) {
			gpio_free(board_id_gpios[i].gpio);
		}
	}
	return 3 * board1 + board0 + 1; // 1:board_id start from 1
}

/* Shadow PD->0, NP->1, PU->2
version gpio84 gpio102 gpio33 value
SKU0	PD		PD		PD		0
SKU1	PD		PD		NP		1
SKU2	PD		PD		PU		2
SKU3 	PD		NP		PD		3
.......
*/
int get_sku_id(void) {
	int sku0 = 0;
	int sku1 = 0;
	int sku0_up = 0;
	int sku1_up = 0;
	int sku0_down = 0;
	int sku1_down = 0;
	int sku2_up = 0;
	int sku2_down = 0;
	int sku2 = 0;
	int i = 0;
	if(parse_dt_and_request_gpio(sku_id_gpios, SKUID_GPIO_MAX)) {
		goto request_error;
	}
	sku0_up = get_gpio_value(&sku_id_gpios[0], 1);
	printk(KERN_INFO "sku0_up = %d", sku0_up);
	sku0_down = get_gpio_value(&sku_id_gpios[0], 0);
	printk(KERN_INFO "sku0_down = %d", sku0_down);
	sku0 = get_board_rel_value(sku0_up, sku0_down);
	printk(KERN_INFO "sku0 = %d", sku0);
	sku1_up = get_gpio_value(&sku_id_gpios[1], 1);
	printk(KERN_INFO "sku1_up = %d", sku1_up);
	sku1_down = get_gpio_value(&sku_id_gpios[1], 0);
	printk(KERN_INFO "sku1_down = %d", sku1_down);
	sku1 = get_board_rel_value(sku1_up, sku1_down);
	printk(KERN_INFO "sku1 = %d", sku1);
	sku2_up = get_gpio_value(&sku_id_gpios[2], 1);
	printk(KERN_INFO "sku2_up = %d", sku2_up);
	sku2_down = get_gpio_value(&sku_id_gpios[2], 0);
	printk(KERN_INFO "sku2_down = %d", sku2_down);
	sku2 = get_board_rel_value(sku2_up, sku2_down);
	printk(KERN_INFO "sku2 = %d", sku2);
request_error:
	for (; i < SKUID_GPIO_MAX; i++) {
		if (gpio_is_valid(sku_id_gpios[i].gpio)) {
			gpio_free(sku_id_gpios[i].gpio);
		}
	}
	return 9 * sku2 + 3 * sku1 + sku0 + 1; // 1:sku start from 1
}

/* Shadow PD->0, NP->1, PU->2
 * version gpio100 gpio32 value
 * EVT		PD		PD		0
 * DVT		PD		NP		1
 * PVT		PD		PU		2
 * MP 		NP		PD		3
 * only care hw_version, DONOT care qcn_type and model.
 */
static boardid_match_t board_table[] = {
	{ .index = 0, .hw_version = "EVT",  .qcn_type = "unknown", .model = "primary" },
	{ .index = 1, .hw_version = "DVT", .qcn_type = "unknown", .model = "primary" },
	{ .index = 2, .hw_version = "DVT", .qcn_type = "unknown", .model = "primary" },
	{ .index = 3, .hw_version = "PVT", .qcn_type = "unknown", .model = "primary" },
	{ .index = 4, .hw_version = "PVT", .qcn_type = "unknown", .model = "primary" },
	{ .index = 5, .hw_version = "MP", .qcn_type = "unknown", .model = "primary" },
};

#define MAX_HWINFO_SIZE 64
#include "hwinfo.h"
typedef struct {
	char *hwinfo_name;
	char hwinfo_buf[MAX_HWINFO_SIZE];
} hwinfo_t;

#define KEYWORD(_name) \
	[_name] = {.hwinfo_name = __stringify(_name), \
		   .hwinfo_buf = {0}},

static hwinfo_t hwinfo[HWINFO_MAX] =
{
#include "hwinfo.h"
};
#undef KEYWORD

int get_cmdline_param_val(const char *param, char *val, int len)
{
	char *p, *q;
	char *s = "unknown";
	int n, rc;
	struct device_node *cmdline_node;
	const char *cmd_line;

	if ( (len <= 1) || !param || !val)
		return -1;

	cmdline_node = of_find_node_by_path("/chosen");
	if (cmdline_node)
		rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	p = strstr(cmd_line, param);
	if (p) {
		p = strstr(p, "=") + 1;
		if (*p == '"') {
			p++;
			q = strstr(p, "\"");
		} else
			q = strstr(p, " ");
		if (q)
			n = q - p;
		else
			n = strlen(p);
		n = min(len - 1, n);
		strncpy(val, p, n);
		val[n] = 0;
	} else {
		n = min_t(int, len - 1, strlen(s));
		strncpy(val, s, n);
		val[n] = 0;
	}
	return n;
}

#ifdef CONFIG_USB_SC27XX_TYPEC
extern int g_typec_cc_polarity;
#endif
static ssize_t get_typec_cc_status(void)
{
	char buf[BUF_SIZE] = {0};

#ifdef CONFIG_USB_SC27XX_TYPEC
        switch(g_typec_cc_polarity){
        case 0:strcpy(buf,"cc_1");
               break;
        case 1:strcpy(buf,"cc_2");
               break;
        default:strcpy(buf,"unknow");
             break;
        }
#endif
	printk(KERN_INFO "Typec cc status: %s\n", buf);

	memcpy(hwinfo[TYPEC_CC_STATUS].hwinfo_buf, buf, BUF_SIZE);

	return 0;
}

static void get_rfgpio_state(void)
{
	int ret = 0;
	ret = gpio_get_value(393);
	if(ret == 0)
		ret = gpio_get_value(446);
	pr_err("RF_GPIO=%d\n", ret);
	memset(hwinfo[RF_GPIO].hwinfo_buf, 0x00, sizeof(hwinfo[RF_GPIO].hwinfo_buf));
	strncpy(hwinfo[RF_GPIO].hwinfo_buf, ret?"1":"0", 1);
}

static void get_cablegpio_state(void)
{
        int ret = 0;
        ret = gpio_get_value(393);
        pr_err("cable_gpio=%d\n", ret);
        memset(hwinfo[cable_gpio].hwinfo_buf, 0x00, sizeof(hwinfo[cable_gpio].hwinfo_buf));
        strncpy(hwinfo[cable_gpio].hwinfo_buf, ret?"0":"1", 1);
}

static void get_card_present(void)
{
	char card_holder_present[BUF_SIZE];
// BEGIN Ontim, rd.zhigang.he, 8/28/2020, 9824711, St-result :PASS, detect card holder present
	int  gpio_cd = 449;

	memset(card_holder_present, '\0', BUF_SIZE);
	printk("%s: gpio(%d) value=%d\n", __func__, gpio_cd, gpio_get_value(gpio_cd));
	if (gpio_get_value(gpio_cd) == 1)
		strncpy(card_holder_present, "truly", 5);
	else
		strncpy(card_holder_present, "false", 5);
// END 9824711

	memset(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf, 0x00, sizeof(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf));
	strncpy(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf, card_holder_present,
	        ((strlen(card_holder_present) >= sizeof(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf) ?
	          sizeof(hwinfo[CARD_HOLDER_PRESENT].hwinfo_buf) : strlen(card_holder_present))));
}

static int get_pon_reason(void)
{
	char pon_reason_info[32] = "UNKNOWN";

	/*	switch ((get_boot_reason() & 0xFF))
		{
		case 0x20:
			pon_reason_info = "usb charger";
			break;
		case 0x21:
			pon_reason_info = "soft reboot";
			break;
		case 0xa0:
			pon_reason_info = "power key";
			break;
		case 0xa1:
			pon_reason_info = "hard reset";
			break;
		default:
			pon_reason_info = "unknow";
			break;
		}*/
	get_cmdline_param_val("bootcause=", pon_reason_info, sizeof(pon_reason_info));

	return sprintf(hwinfo[pon_reason].hwinfo_buf, "%s", pon_reason_info);
}

static int get_secure_boot_version(void)
{
	char is_secureboot[32] = "Unknown";
	/*	if (get_secure_boot_value())
			is_secureboot = "SE";
		else
			is_secureboot = "NSE";*/

	return sprintf(hwinfo[secboot_version].hwinfo_buf, "%s", is_secureboot);
}

static int get_dual_sim(void)
{
	unsigned int gpio_base = 343;
	unsigned int pin4 = 167;
	int pin_val = 0;

	pin_val |= (gpio_get_value(gpio_base + pin4) & 0x01) << 4;

	printk(KERN_ERR "%s: pin_val is %x ;\n", __func__, pin_val);

	return sprintf(hwinfo[dual_sim].hwinfo_buf, "%s", pin_val ? "sig" : "dual");
}

//Add board nfc flag begin --jwt 20200819
unsigned int platform_nfc_flag = 0;

static int get_nfc_flag(void)
{
	int id = platform_nfc_flag;
	return sprintf(hwinfo[board_nfc_flag].hwinfo_buf, "%04d", id);
}
//Add board nfc flag end --jwt 20200819

/* BEGIN Ontim, jiawentao, 28/09/2020, 10015326, St-result:PASS, Add project version drive device note. */
unsigned int platform_prj_ver_flag = 0;

static int get_project_version_flag(void)
{
	int id = platform_prj_ver_flag;
	return sprintf(hwinfo[prj_ver_flag].hwinfo_buf, "%04d", id);
}
/* END 10015326 */

static int get_hw_skuid(void)
{
	unsigned int id = get_sku_id();
	return sprintf(hwinfo[hw_sku].hwinfo_buf, "%04d", id);
}

static int get_version_id(void)
{
	int id = get_board_id();
	return sprintf(hwinfo[board_id].hwinfo_buf, "%04d", id);
}

static int get_hw_version(void)
{
  int id = get_board_id() - 1;
  if (id > (sizeof(board_table) / sizeof(boardid_match_t) - 1))
    id = sizeof(board_table) / sizeof(boardid_match_t) - 1;
  return sprintf(hwinfo[hw_version].hwinfo_buf, "%s", board_table[id].hw_version);
}

char NFC_BUF[MAX_HWINFO_SIZE] = {"Unknow"};
static void get_nfc_deviceinfo(void)
{
	strcpy(hwinfo[NFC_MFR].hwinfo_buf, NFC_BUF);
}

int _atoi(char * str)
{
	int value = 0;
	int sign = 1;
	int radix;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
	{
		radix = 16;
		str += 2;
	}
	else if (*str == '0')
	{
		radix = 8;
		str++;
	} else {
		radix = 10;
	}
	while (*str && *str != '\0')
	{
		if (radix == 16)
		{
			if (*str >= '0' && *str <= '9')
				value = value * radix + *str - '0';
			else if (*str >= 'A' && *str <= 'F')
				value = value * radix + *str - 'A' + 10;
			else if (*str >= 'a' && *str <= 'f')
				value = value * radix + *str - 'a' + 10;
		} else {
			value = value * radix + *str - '0';
		}
		str++;
	}
	return sign * value;
}

static char *get_bootdevice()
{
	static char bootdevice[BUF_SIZE] = {0};
	if ( ! *bootdevice )
		get_cmdline_param_val("androidboot.boot_devices=", bootdevice, sizeof(bootdevice));
	return bootdevice;
}

#ifdef CONFIG_MTK_BOOT //mtk remove get_boot_mode() function
extern unsigned int get_boot_mode(void);
#endif

/* BEGIN Ontim, jiawentao, 28/09/2020, 10015326, St-result:PASS, Add project version drive device note. */
#ifdef CONFIG_HBM_SUPPORT
//bool g_hbm_enable = false;
//EXPORT_SYMBOL(g_hbm_enable);
extern bool g_hbm_enable;
extern int hbm_set_backlight_level(unsigned int level);
extern int hbm_exit_set_backlight_level(void);

static int set_hbm_status(const char * buf, int n)
{
	printk("hbm user buf:%s\n", buf);

#ifdef SMT_VERSION
	printk("SMT version,No hbm");
#else
	switch (buf[0]){
		case '0':
			if (!g_hbm_enable) {
				printk("Have been disabled hbm, exit!\n");
				break;
			}
			//g_hbm_enable = false;
			hbm_exit_set_backlight_level();
			break;
		case '3':
			if (g_hbm_enable) {
				printk("Have been enabled hbm, exit!\n");
				break;
			}
			//g_hbm_enable = true;
			hbm_set_backlight_level(256);
			break;
		default:
			g_hbm_enable = false;
			break;
	}
#endif
	return 0;
}

static void get_hbm_status(void)
{
	char hbm_str_st[8] = {0};
	if (g_hbm_enable){
	    strcpy(hbm_str_st, "hbm:on");
	}else{
	    strcpy(hbm_str_st, "hbm:off");
	}
	sprintf(hwinfo[hbm].hwinfo_buf,"%s",hbm_str_st);
}
#endif

static ssize_t hwinfo_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int i = 0;
#ifdef CONFIG_MTK_BOOT
	static int flag = 0;
	int boot_mode ;
	boot_mode = (int)get_boot_mode();
	if (boot_mode == META_BOOT) {
			if(0==flag)
			//meta_camera_info();
		flag=1;
	}
#endif

	printk(KERN_INFO "hwinfo sys node %s \n", attr->attr.name);

	for (; i < HWINFO_MAX && strcmp(hwinfo[i].hwinfo_name, attr->attr.name) && ++i;);

	switch (i) {
	case NFC_MFR:
		get_nfc_deviceinfo();
		break;
	case dual_sim:
		get_dual_sim();
		break;
	case board_id:
		get_version_id();
		break;
	case hw_sku:
		get_hw_skuid();
		break;
	case board_nfc_flag:
		get_nfc_flag();
		break;
	case prj_ver_flag:
		get_project_version_flag();
		break;
	case hw_version:
		get_hw_version();
		break;
	case TYPEC_CC_STATUS:
		get_typec_cc_status();
		break;
	case CARD_HOLDER_PRESENT:
		get_card_present();
		break;
	case RF_GPIO:
		get_rfgpio_state();
		break;
	case cable_gpio:
                get_cablegpio_state();
                break;
	case pon_reason:
		get_pon_reason();
		break;
	case secboot_version:
		get_secure_boot_version();
		break;
#ifdef CONFIG_HBM_SUPPORT
	case hbm:
		get_hbm_status();
		break;
#endif
	default:
		break;
	}
	return sprintf(buf, "%s=%s \n",  attr->attr.name, ((i >= HWINFO_MAX || hwinfo[i].hwinfo_buf[0] == '\0') ? "unknow" : hwinfo[i].hwinfo_buf));
}
/* END 10015326 */

static ssize_t hwinfo_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int i = 0;
	printk(KERN_INFO "hwinfo sys node %s \n", attr->attr.name);

	for (; i < HWINFO_MAX && strcmp(hwinfo[i].hwinfo_name, attr->attr.name) && ++i;);

	switch (i) {
#ifdef CONFIG_HBM_SUPPORT
	case hbm:
		set_hbm_status(buf, n);
		break;
#endif
	default:
		break;
	};
	return n;
}

#define KEYWORD(_name) \
    static struct kobj_attribute hwinfo##_name##_attr = {   \
                .attr   = {                             \
                        .name = __stringify(_name),     \
                        .mode = 0644,                   \
                },                                      \
            .show   = hwinfo_show,                 \
            .store  = hwinfo_store,                \
        };

#include "hwinfo.h"
#undef KEYWORD

#define KEYWORD(_name)\
    [_name] = &hwinfo##_name##_attr.attr,

static struct attribute * g[] = {
#include "hwinfo.h"
	NULL
};
#undef KEYWORD

static struct attribute_group attr_group = {
	.attrs = g,
};

int ontim_hwinfo_register(enum HWINFO_E e_hwinfo, char *hwinfo_name)
{
	if ((e_hwinfo >= HWINFO_MAX) || (hwinfo_name == NULL))
		return -1;
	strncpy(hwinfo[e_hwinfo].hwinfo_buf, hwinfo_name, \
	        (strlen(hwinfo_name) >= 20 ? 19 : strlen(hwinfo_name)));
	return 0;
}

static int __init hwinfo_init(void)
{
	struct kobject *k_hwinfo = NULL;
	char *bootdevice = get_bootdevice();

	if ( (k_hwinfo = kobject_create_and_add("hwinfo", NULL)) == NULL ) {
		printk(KERN_ERR "%s:hwinfo sys node create error \n", __func__);
	}

	if ( sysfs_create_group(k_hwinfo, &attr_group) ) {
		printk(KERN_ERR "%s: sysfs_create_group failed\n", __func__);
	}
	printk(KERN_INFO "bootdevice=%s\n", bootdevice);

#ifdef CONFIG_HBM_SUPPORT
	ontim_hwinfo_register(hbm, "hbm");
#endif

	return 0;
}

static void __exit hwinfo_exit(void)
{
	return ;
}

late_initcall_sync(hwinfo_init);
module_exit(hwinfo_exit);
MODULE_AUTHOR("eko@ontim.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Product Hardward Info Exposure");
