#ifndef __CAM_ERR_CODE_H__
#define __CAM_ERR_CODE_H__

/*
cam_err_code = 0xFFFFFFFF;
31-30:platform(00:MTK;01:Qcom;10:ZhanXun;11:reserve);
29-0:cam error code
*/

extern u32 cam_err_code;

/*
#define		CAM_XXX_PLATFORM						(1L<<31)
*/
#define		CAM_QCOM_PLATFORM						(1L<<30)

#define		ISP_CSI2_OCP_ERR_IRQ					(1L)
#define		ISP_CSI2_SHORT_PACKET_IRQ				(1L<<1)
#define		ISP_CSI2_ECC_CORRECTION_IRQ				(1L<<2)
#define		ISP_CSI2_ECC_NO_CORRECTION_IRQ			(1L<<3)
#define		ISP_CSI2_COMPLEXIO2_ERR_IRQ				(1L<<4)
#define		ISP_CSI2_COMPLEXIO1_ERR_IRQ				(1L<<5)
#define		ISP_CSI2_FIFO_OVF_IRQ					(1L<<6)
#define		ISP_CSIA_IRQ							(1L<<7)
#define		ISP_CSI2C_IRQ							(1L<<8)
#define		ISP_CCP2_LCM_IRQ						(1L<<9)
#define		ISP_CCP2_LC0_IRQ						(1L<<10)
#define		ISP_CSIB_IRQ			(ISP_CCP2_LCM_IRQ | \
						ISP_CCP2_LC0_IRQ)

#define		ISP_CSIB_LC1_IRQ				(1L<<11)
#define		ISP_CSIB_LC2_IRQ				(1L<<12)
#define		ISP_CSIB_LC3_IRQ				(1L<<13)
#define		ISP_CCDC_VD0_IRQ				(1L<<14)
#define		ISP_CCDC_VD1_IRQ				(1L<<15)
#define		ISP_CCDC_VD2_IRQ				(1L<<16)
#define		ISP_CCDC_ERR_IRQ				(1L<<17)
#define		ISP_H3A_AF_DONE_IRQ				(1L<<18)
#define		ISP_H3A_AWB_DONE_IRQ			(1L<<19)
#define		ISP_HIST_DONE_IRQ				(1L<<20)
#define		ISP_CCDC_LSC_DONE_IRQ			(1L<<21)
#define		ISP_CCDC_LSC_PREF_COMP_IRQ		(1L<<22)
#define		ISP_CCDC_LSC_PREF_ERR_IRQ		(1L<<23)
#define		ISP_PRV_RSZ_DONE_IRQ			(1L<<24)
#define		ISP_OVF_IRQ						(1L<<25)
#define		ISP_PING_PONG_IRQ				(1L<<26)
#define		ISP_MMU_SEC_ERR_IRQ				(1L<<27)
#define		ISP_HS_VS_IRQ					(1L<<28)

/* #define		ERROR					(1L<<29) */
#endif