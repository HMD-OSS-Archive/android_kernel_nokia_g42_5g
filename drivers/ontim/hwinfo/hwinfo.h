#ifndef KEYWORD

#define KEYWORD_ENUM
#define KEYWORD(symbol) symbol,
/* BEGIN Ontim, jiawentao, 28/09/2020, 10015326, St-result:PASS, Add project version drive device note. */
enum HWINFO_E{
#endif

KEYWORD(NFC_MFR)
KEYWORD(pon_reason)
KEYWORD(secboot_version)
KEYWORD(board_id)
KEYWORD(hw_sku)
KEYWORD(board_nfc_flag)
KEYWORD(prj_ver_flag)
KEYWORD(hw_version)

KEYWORD(dual_sim)
KEYWORD(TYPEC_CC_STATUS)      //Typec  mfr

KEYWORD(CARD_HOLDER_PRESENT)  //card hold detect

#ifdef CONFIG_HBM_SUPPORT
KEYWORD(hbm)
#endif

KEYWORD(RF_GPIO)
KEYWORD(cable_gpio)
#ifdef KEYWORD_ENUM
KEYWORD(HWINFO_MAX)
};
/* END 10015326 */
int smartisan_hwinfo_register(enum HWINFO_E e_hwinfo,char *hwinfo_name);
#undef KEYWORD_ENUM
#undef KEYWORD

#endif
