//====================================
/*
V3 :(1)���¼���Ч��
V4 :(1)���Ӵ�������
v5 :(1)���ƽ�������CRCУ��

*/
//====================================
`define FRAME_BUFFER
// `define CONTRAST_BRIGHT_EN
// `define UVC_EN 
`define HDMI_OUT_EN
module top #(
parameter AXI_DATA_WIDTH = 512,

parameter AXI_MUX_EN = 1'b1,
parameter I_VID_WIDTH    = 32,
parameter O_VID_WIDTH    = 16,
parameter AXI_ADDR_WIDTH = 33,
parameter WR_FIFO_DEPTH	 = 256,    
parameter RD_FIFO_DEPTH  = 256,
parameter MAX_VID_WIDTH	 = 1920 ,//video width 
parameter MAX_VID_HIGHT	 = 1080 ,//wideo height
parameter START_ADDR     = 33'h000000000,
parameter FB_NUM		 = 3,//2 buffer ,3 buffer   
parameter BURST_LEN      = 63,
parameter   AXI_ID_WIDTH    = 6,
parameter   S_COUNT 					= 2,
parameter   M_COUNT 					= 1,  
// parameter HACT		     = 13'd3840,
parameter PACK_BIT          = 40,
parameter	HACT		    = 13'd1920,
parameter	VACT		    = 13'd1080,
parameter	HSP				= 13'd4,
parameter	HBP				= 13'd88,
parameter	HFP				= 13'd120,
parameter	VSP				= 13'd2,
parameter	VBP				= 13'd20,
parameter	VFP				= 13'd20


)(
  (* syn_peri_port = 0 *) input mipi_clk,
  (* syn_peri_port = 0 *) input clk_74p25m,
  (* syn_peri_port = 0 *) input ddr_clk_ref,
  (* syn_peri_port = 0 *) input [1:0] i_sw,
  (* syn_peri_port = 0 *) input sys_pll_lock,
  (* syn_peri_port = 0 *) input ddr_pll_lock,
  (* syn_peri_port = 0 *) input MIPI_TX_PLL_LOCKED,
  (* syn_peri_port = 0 *) input pll_byteclk_locked,
  (* syn_peri_port = 0 *) input hdmi_tx_fast_clk,
  (* syn_peri_port = 0 *) input CLK_5M,
  (* syn_peri_port = 0 *) input i_sysclk_div2,
  (* syn_peri_port = 0 *) input pll_inst1_CLKOUT0,
  (* syn_peri_port = 0 *) input hdmi_tx_slow_clk,
  (* syn_peri_port = 0 *) input pll_inst2_CLKOUT0,
  (* syn_peri_port = 0 *) input axi0_ACLK,
  (* syn_peri_port = 0 *) input mipi_rx_ck0_CLKOUT,
  (* syn_peri_port = 0 *) input mipi_rx_ck1_CLKOUT,
  (* syn_peri_port = 0 *) input mipi_dphy_tx_FASTCLK_C,
  (* syn_peri_port = 0 *) input mipi_dphy_tx_FASTCLK_D,
  (* syn_peri_port = 0 *) input gpio_clk_50m,
  (* syn_peri_port = 0 *) input mipi_dphy_tx_SLOWCLK,
  (* syn_peri_port = 0 *) input i_fb_clk,
  (* syn_peri_port = 0 *) input jtag_inst1_CAPTURE,
  (* syn_peri_port = 0 *) input jtag_inst1_DRCK,
  (* syn_peri_port = 0 *) input jtag_inst1_RESET,
  (* syn_peri_port = 0 *) input jtag_inst1_RUNTEST,
  (* syn_peri_port = 0 *) input jtag_inst1_SEL,
  (* syn_peri_port = 0 *) input jtag_inst1_SHIFT,
  (* syn_peri_port = 0 *) input jtag_inst1_TCK,
  (* syn_peri_port = 0 *) input jtag_inst1_TDI,
  (* syn_peri_port = 0 *) input jtag_inst1_TMS,
  (* syn_peri_port = 0 *) input jtag_inst1_UPDATE,
  (* syn_peri_port = 0 *) input jtag_inst2_CAPTURE,
  (* syn_peri_port = 0 *) input jtag_inst2_DRCK,
  (* syn_peri_port = 0 *) input jtag_inst2_RESET,
  (* syn_peri_port = 0 *) input jtag_inst2_RUNTEST,
  (* syn_peri_port = 0 *) input jtag_inst2_SEL,
  (* syn_peri_port = 0 *) input jtag_inst2_SHIFT,
  (* syn_peri_port = 0 *) input jtag_inst2_TCK,
  (* syn_peri_port = 0 *) input jtag_inst2_TDI,
  (* syn_peri_port = 0 *) input jtag_inst2_TMS,
  (* syn_peri_port = 0 *) input jtag_inst2_UPDATE,
  (* syn_peri_port = 0 *) input axi0_ARREADY,
  (* syn_peri_port = 0 *) input axi1_ARREADY,
  (* syn_peri_port = 0 *) input axi0_AWREADY,
  (* syn_peri_port = 0 *) input axi1_AWREADY,
  (* syn_peri_port = 0 *) input [5:0] axi0_BID,
  (* syn_peri_port = 0 *) input [5:0] axi1_BID,
  (* syn_peri_port = 0 *) input [1:0] axi0_BRESP,
  (* syn_peri_port = 0 *) input [1:0] axi1_BRESP,
  (* syn_peri_port = 0 *) input axi0_BVALID,
  (* syn_peri_port = 0 *) input axi1_BVALID,
  (* syn_peri_port = 0 *) input ddr_inst_CFG_DONE,
  (* syn_peri_port = 0 *) input [511:0] axi0_RDATA,
  (* syn_peri_port = 0 *) input [511:0] axi1_RDATA,
  (* syn_peri_port = 0 *) input [5:0] axi0_RID,
  (* syn_peri_port = 0 *) input [5:0] axi1_RID,
  (* syn_peri_port = 0 *) input axi0_RLAST,
  (* syn_peri_port = 0 *) input axi1_RLAST,
  (* syn_peri_port = 0 *) input [1:0] axi0_RRESP,
  (* syn_peri_port = 0 *) input [1:0] axi1_RRESP,
  (* syn_peri_port = 0 *) input axi0_RVALID,
  (* syn_peri_port = 0 *) input axi1_RVALID,
  (* syn_peri_port = 0 *) input axi0_WREADY,
  (* syn_peri_port = 0 *) input axi1_WREADY,
  (* syn_peri_port = 0 *) input S0_io_cam_scl_IN,
  (* syn_peri_port = 0 *) input S0_io_cam_sda_IN,
  (* syn_peri_port = 0 *) input S1_io_cam_scl_IN,
  (* syn_peri_port = 0 *) input S1_io_cam_sda_IN,
  (* syn_peri_port = 0 *) input clk_25m,
 
  (* syn_peri_port = 0 *) output sys_pll_rstn,
  (* syn_peri_port = 0 *) output ddr_pll_rstn,
  (* syn_peri_port = 0 *) output MIPI_TX_PLL_RSTN,
  (* syn_peri_port = 0 *) output pll_byteclk_rstn,
  (* syn_peri_port = 0 *) output jtag_inst1_TDO,
  (* syn_peri_port = 0 *) output jtag_inst2_TDO,
  (* syn_peri_port = 0 *) output [32:0] axi0_ARADDR,
  (* syn_peri_port = 0 *) output [32:0] axi1_ARADDR,
  (* syn_peri_port = 0 *) output axi0_ARAPCMD,
  (* syn_peri_port = 0 *) output axi1_ARAPCMD,
  (* syn_peri_port = 0 *) output [1:0] axi0_ARBURST,
  (* syn_peri_port = 0 *) output [1:0] axi1_ARBURST,
  (* syn_peri_port = 0 *) output [5:0] axi0_ARID,
  (* syn_peri_port = 0 *) output [5:0] axi1_ARID,
  (* syn_peri_port = 0 *) output [7:0] axi0_ARLEN,
  (* syn_peri_port = 0 *) output [7:0] axi1_ARLEN,
  (* syn_peri_port = 0 *) output axi0_ARLOCK,
  (* syn_peri_port = 0 *) output axi1_ARLOCK,
  (* syn_peri_port = 0 *) output axi0_ARQOS,
  (* syn_peri_port = 0 *) output axi1_ARQOS,
  (* syn_peri_port = 0 *) output [2:0] axi0_ARSIZE,
  (* syn_peri_port = 0 *) output [2:0] axi1_ARSIZE,
  (* syn_peri_port = 0 *) output axi0_ARESETn,
  (* syn_peri_port = 0 *) output axi1_ARESETn,
  (* syn_peri_port = 0 *) output axi0_ARVALID,
  (* syn_peri_port = 0 *) output axi1_ARVALID,
  (* syn_peri_port = 0 *) output [32:0] axi0_AWADDR,
  (* syn_peri_port = 0 *) output [32:0] axi1_AWADDR,
  (* syn_peri_port = 0 *) output axi0_AWALLSTRB,
  (* syn_peri_port = 0 *) output axi1_AWALLSTRB,
  (* syn_peri_port = 0 *) output axi0_AWAPCMD,
  (* syn_peri_port = 0 *) output axi1_AWAPCMD,
  (* syn_peri_port = 0 *) output [1:0] axi0_AWBURST,
  (* syn_peri_port = 0 *) output [1:0] axi1_AWBURST,
  (* syn_peri_port = 0 *) output [3:0] axi0_AWCACHE,
  (* syn_peri_port = 0 *) output [3:0] axi1_AWCACHE,
  (* syn_peri_port = 0 *) output axi0_AWCOBUF,
  (* syn_peri_port = 0 *) output axi1_AWCOBUF,
  (* syn_peri_port = 0 *) output [5:0] axi0_AWID,
  (* syn_peri_port = 0 *) output [5:0] axi1_AWID,
  (* syn_peri_port = 0 *) output [7:0] axi0_AWLEN,
  (* syn_peri_port = 0 *) output [7:0] axi1_AWLEN,
  (* syn_peri_port = 0 *) output axi0_AWLOCK,
  (* syn_peri_port = 0 *) output axi1_AWLOCK,
  (* syn_peri_port = 0 *) output axi0_AWQOS,
  (* syn_peri_port = 0 *) output axi1_AWQOS,
  (* syn_peri_port = 0 *) output [2:0] axi0_AWSIZE,
  (* syn_peri_port = 0 *) output [2:0] axi1_AWSIZE,
  (* syn_peri_port = 0 *) output axi0_AWVALID,
  (* syn_peri_port = 0 *) output axi1_AWVALID,
  (* syn_peri_port = 0 *) output axi0_BREADY,
  (* syn_peri_port = 0 *) output axi1_BREADY,
  (* syn_peri_port = 0 *) output ddr_inst_CFG_RST,
  (* syn_peri_port = 0 *) output ddr_inst_CFG_SEL,
  (* syn_peri_port = 0 *) output ddr_inst_CFG_START,
  (* syn_peri_port = 0 *) output axi0_RREADY,
  (* syn_peri_port = 0 *) output axi1_RREADY,
  (* syn_peri_port = 0 *) output [511:0] axi0_WDATA,
  (* syn_peri_port = 0 *) output [511:0] axi1_WDATA,
  (* syn_peri_port = 0 *) output axi0_WLAST,
  (* syn_peri_port = 0 *) output axi1_WLAST,
  (* syn_peri_port = 0 *) output [63:0] axi0_WSTRB,
  (* syn_peri_port = 0 *) output [63:0] axi1_WSTRB,
  (* syn_peri_port = 0 *) output axi0_WVALID,
  (* syn_peri_port = 0 *) output axi1_WVALID,
  (* syn_peri_port = 0 *) output tmds_clk_TX_OE,
  (* syn_peri_port = 0 *) output [9:0] tmds_clk_TX_DATA,
  (* syn_peri_port = 0 *) output tmds_clk_TX_RST,
  (* syn_peri_port = 0 *) output tmds_data0_TX_OE,
  (* syn_peri_port = 0 *) output [9:0] tmds_data0_TX_DATA,
  (* syn_peri_port = 0 *) output tmds_data0_TX_RST,
  (* syn_peri_port = 0 *) output tmds_data1_TX_OE,
  (* syn_peri_port = 0 *) output [9:0] tmds_data1_TX_DATA,
  (* syn_peri_port = 0 *) output tmds_data1_TX_RST,
  (* syn_peri_port = 0 *) output tmds_data2_TX_OE,
  (* syn_peri_port = 0 *) output [9:0] tmds_data2_TX_DATA,
  (* syn_peri_port = 0 *) output tmds_data2_TX_RST,
  (* syn_peri_port = 0 *) output P0_lcd_power_en,
  (* syn_peri_port = 0 *) output P0_lcd_rstp,
  (* syn_peri_port = 0 *) output P1_lcd_power_en,
  (* syn_peri_port = 0 *) output P1_o_lcd_rstn,
  (* syn_peri_port = 0 *) output S0_io_cam_scl_OUT,
  (* syn_peri_port = 0 *) output S0_io_cam_scl_OE,
  (* syn_peri_port = 0 *) output S0_io_cam_sda_OUT,
  (* syn_peri_port = 0 *) output S0_io_cam_sda_OE,
  (* syn_peri_port = 0 *) output S0_o_cam_rst_p,
  (* syn_peri_port = 0 *) output S1_io_cam_scl_OUT,
  (* syn_peri_port = 0 *) output S1_io_cam_scl_OE,
  (* syn_peri_port = 0 *) output S1_io_cam_sda_OUT,
  (* syn_peri_port = 0 *) output S1_io_cam_sda_OE,
  (* syn_peri_port = 0 *) output S1_o_cam_rst_p,
  (* syn_peri_port = 0 *) output [3:0] led,
  //mipi rx
  (* syn_peri_port = 0 *) output mipi_rx_ck0_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp00_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp01_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp02_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp03_HS_ENA,

  (* syn_peri_port = 0 *) output mipi_rx_ck0_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp00_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp01_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp02_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp03_HS_TERM,
  
  
  (* syn_peri_port = 0 *) output mipi_rx_dp00_RST,
  (* syn_peri_port = 0 *) output mipi_rx_dp01_RST,
  (* syn_peri_port = 0 *) output mipi_rx_dp02_RST,
  (* syn_peri_port = 0 *) output mipi_rx_dp03_RST,


  (* syn_peri_port = 0 *) output mipi_rx_ck1_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp10_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp11_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp12_HS_ENA,
  (* syn_peri_port = 0 *) output mipi_rx_dp13_HS_ENA,

  (* syn_peri_port = 0 *) output mipi_rx_ck1_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp10_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp11_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp12_HS_TERM,
  (* syn_peri_port = 0 *) output mipi_rx_dp13_HS_TERM,
  
  
  (* syn_peri_port = 0 *) output mipi_rx_dp10_RST,
  (* syn_peri_port = 0 *) output mipi_rx_dp11_RST,
  (* syn_peri_port = 0 *) output mipi_rx_dp12_RST,
  (* syn_peri_port = 0 *) output mipi_rx_dp13_RST,

  (* syn_peri_port = 0 *) input mipi_rx_ck0_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_ck0_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp00_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp00_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp01_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp01_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp02_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp02_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp03_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp03_LP_P_IN,

  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp00_HS_IN,
  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp01_HS_IN,
  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp02_HS_IN,
  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp03_HS_IN,

  (* syn_peri_port = 0 *) output mipi_rx_dp00_FIFO_RD,
  (* syn_peri_port = 0 *) output mipi_rx_dp01_FIFO_RD,
  (* syn_peri_port = 0 *) output mipi_rx_dp02_FIFO_RD,
  (* syn_peri_port = 0 *) output mipi_rx_dp03_FIFO_RD,
  (* syn_peri_port = 0 *) input mipi_rx_dp00_FIFO_EMPTY,
  (* syn_peri_port = 0 *) input mipi_rx_dp01_FIFO_EMPTY,
  (* syn_peri_port = 0 *) input mipi_rx_dp02_FIFO_EMPTY,
  (* syn_peri_port = 0 *) input mipi_rx_dp03_FIFO_EMPTY,

    (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp10_HS_IN,
  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp11_HS_IN,
  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp12_HS_IN,
  (* syn_peri_port = 0 *) input [7:0] mipi_rx_dp13_HS_IN,

  (* syn_peri_port = 0 *) input mipi_rx_ck1_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_ck1_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp10_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp10_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp11_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp11_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp12_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp12_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp13_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_rx_dp13_LP_P_IN,



  (* syn_peri_port = 0 *) output mipi_rx_dp10_FIFO_RD,
  (* syn_peri_port = 0 *) output mipi_rx_dp11_FIFO_RD,
  (* syn_peri_port = 0 *) output mipi_rx_dp12_FIFO_RD,
  (* syn_peri_port = 0 *) output mipi_rx_dp13_FIFO_RD,
  (* syn_peri_port = 0 *) input mipi_rx_dp10_FIFO_EMPTY,
  (* syn_peri_port = 0 *) input mipi_rx_dp11_FIFO_EMPTY,
  (* syn_peri_port = 0 *) input mipi_rx_dp12_FIFO_EMPTY,
  (* syn_peri_port = 0 *) input mipi_rx_dp13_FIFO_EMPTY,


  (* syn_peri_port = 0 *) output mipi_tx_ck0_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_ck0_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_ck0_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_ck0_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_ck0_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_ck0_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_ck0_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp00_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp00_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp00_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp00_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp00_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp00_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp00_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp01_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp01_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp01_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp01_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp01_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp01_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp01_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp02_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp02_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp02_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp02_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp02_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp02_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp02_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp03_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp03_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp03_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp03_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp03_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp03_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp03_RST,
  (* syn_peri_port = 0 *) output mipi_tx_ck1_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_ck1_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_ck1_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_ck1_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_ck1_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_ck1_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_ck1_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp10_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp10_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp10_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp10_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp10_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp10_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp10_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp11_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp11_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp11_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp11_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp11_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp11_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp11_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp12_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp12_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp12_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp12_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp12_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp12_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp12_RST,
  (* syn_peri_port = 0 *) output mipi_tx_dp13_HS_OE,
  (* syn_peri_port = 0 *) output [7:0] mipi_tx_dp13_HS_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp13_LP_N_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp13_LP_N_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp13_LP_P_OE,
  (* syn_peri_port = 0 *) output mipi_tx_dp13_LP_P_OUT,
  (* syn_peri_port = 0 *) output mipi_tx_dp13_RST,
  (* syn_peri_port = 0 *) input mipi_tx_dp00_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_tx_dp00_LP_P_IN,
  (* syn_peri_port = 0 *) input mipi_tx_dp10_LP_N_IN,
  (* syn_peri_port = 0 *) input mipi_tx_dp10_LP_P_IN

);



/////////////////////////////////////////////////////////////////////////////
//ddr4 config
localparam [1:0]    IDLE        = 2'b00,
                    CFG_START   = 2'b01,
                    CFG_DONE    = 2'b11;

reg [1:0]   cfg_st, cfg_next;
reg [7:0]   cfg_count;
//Reset and PLL
wire        arst_n;
wire ddr_cfg_ok;


// Slave Interface Write Address Ports
wire [AXI_ID_WIDTH-1:0]           s_axi_awid;
wire [AXI_ADDR_WIDTH-1:0]         s_axi_awaddr;
wire [7:0]                        s_axi_awlen;
wire [2:0]                        s_axi_awsize;
wire [1:0]                        s_axi_awburst;
wire [0:0]                        s_axi_awlock;
wire [3:0]                        s_axi_awcache;
wire [2:0]                        s_axi_awprot;
wire                              s_axi_awvalid;
wire                              s_axi_awready;
// Slave Interface Write Data Ports
wire [AXI_DATA_WIDTH-1:0]         s_axi_wdata;
wire [(AXI_DATA_WIDTH/8)-1:0]     s_axi_wstrb;
wire                              s_axi_wlast;
wire                              s_axi_wvalid;
wire                              s_axi_wready;
// Slave Interface Write Response Ports
wire                              s_axi_bready;
wire [AXI_ID_WIDTH-1:0]           s_axi_bid;
wire [1:0]                        s_axi_bresp;
wire                              s_axi_bvalid;
// Slave Interface Read Address Ports
wire [AXI_ID_WIDTH-1:0]           s_axi_arid;
wire [AXI_ADDR_WIDTH-1:0]         s_axi_araddr;
wire [7:0]                        s_axi_arlen;
wire [2:0]                        s_axi_arsize;
wire [1:0]                        s_axi_arburst;
wire [0:0]                        s_axi_arlock;
wire [3:0]                        s_axi_arcache;
wire [2:0]                        s_axi_arprot;
wire                              s_axi_arvalid;
wire                              s_axi_arready;
// Slave Interface Read Data Ports
wire                              s_axi_rready;
wire [AXI_ID_WIDTH-1:0]           s_axi_rid;
wire [AXI_DATA_WIDTH-1:0]         s_axi_rdata;
wire [1:0]                        s_axi_rresp;
wire                              s_axi_rlast;
wire                              s_axi_rvalid;

wire  [AXI_ID_WIDTH-1:0]   		m0_axi_awid      ; 
  wire  [AXI_ADDR_WIDTH-1:0]   	m0_axi_awaddr    ; 
  wire  [    7:0]   			m0_axi_awlen     ; 
  wire  [    2:0]   			m0_axi_awsize    ; 
  wire  [    1:0]   			m0_axi_awburst   ; 
  wire  [    1:0]   			m0_axi_awlock    ; 
  wire              			m0_axi_awvalid   ; 
  wire              			m0_axi_awready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m0_axi_arid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0]   m0_axi_araddr    ; 
  wire  [    7:0]   			m0_axi_arlen     ; 
  wire  [    2:0]   			m0_axi_arsize    ; 
  wire  [    1:0]   			m0_axi_arburst   ; 
  wire  [    1:0]   			m0_axi_arlock    ; 
  wire              			m0_axi_arvalid   ; 
  wire              			m0_axi_arready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m0_axi_wid       ; 
  wire  [(AXI_DATA_WIDTH/8)-1:0]   m0_axi_wstrb     ; 
  wire              			m0_axi_wlast     ; 
  wire              			m0_axi_wvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m0_axi_wdata     ; 
  wire              			m0_axi_wready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m0_axi_bid       ; 
  wire              			m0_axi_bvalid    ; 
  wire              			m0_axi_bready    ; 
  wire              			m0_axi_rready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m0_axi_rid       ; 
  wire  [    1:0]   			m0_axi_rresp     ; 
  wire              			m0_axi_rlast     ; 
  wire              			m0_axi_rvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m0_axi_rdata     ; 
  ///////////// AXI MASTER 1   
  wire  [AXI_ID_WIDTH-1:0]   	m1_axi_awid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0]   m1_axi_awaddr    ; 
  wire  [    7:0]   			m1_axi_awlen     ; 
  wire  [    2:0]   			m1_axi_awsize    ; 
  wire  [    1:0]   			m1_axi_awburst   ; 
  wire  [    1:0]   			m1_axi_awlock    ; 
  wire              			m1_axi_awvalid   ; 
  wire              			m1_axi_awready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m1_axi_arid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0]   m1_axi_araddr    ; 
  wire  [    7:0]   			m1_axi_arlen     ; 
  wire  [    2:0]   			m1_axi_arsize    ; 
  wire  [    1:0]   			m1_axi_arburst   ; 
  wire  [    1:0]   			m1_axi_arlock    ; 
  wire              			m1_axi_arvalid   ; 
  wire              			m1_axi_arready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m1_axi_wid       ; 
  wire  [(AXI_DATA_WIDTH/8)-1:0]   m1_axi_wstrb     ; 
  wire              			m1_axi_wlast     ; 
  wire              			m1_axi_wvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m1_axi_wdata     ; 
  wire              			m1_axi_wready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m1_axi_bid       ; 
  wire              			m1_axi_bvalid    ; 
  wire              			m1_axi_bready    ;  
  wire              			m1_axi_rready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m1_axi_rid       ; 
  wire  [    1:0]   			m1_axi_rresp     ; 
  wire              			m1_axi_rlast     ; 
  wire              			m1_axi_rvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m1_axi_rdata     ;   

  ///////////// AXI MASTER 2   
  wire  [AXI_ID_WIDTH-1:0]   	m2_axi_awid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0] m2_axi_awaddr    ; 
  wire  [    7:0]   			m2_axi_awlen     ; 
  wire  [    2:0]   			m2_axi_awsize    ; 
  wire  [    1:0]   			m2_axi_awburst   ; 
  wire  [    1:0]   			m2_axi_awlock    ; 
  wire              			m2_axi_awvalid   ; 
  wire              			m2_axi_awready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m2_axi_arid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0] m2_axi_araddr    ; 
  wire  [    7:0]   			m2_axi_arlen     ; 
  wire  [    2:0]   			m2_axi_arsize    ; 
  wire  [    1:0]   			m2_axi_arburst   ; 
  wire  [    1:0]   			m2_axi_arlock    ; 
  wire              			m2_axi_arvalid   ; 
  wire              			m2_axi_arready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m2_axi_wid       ; 
  wire  [(AXI_DATA_WIDTH/8)-1:0]m2_axi_wstrb     ; 
  wire              			m2_axi_wlast     ; 
  wire              			m2_axi_wvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m2_axi_wdata     ; 
  wire              			m2_axi_wready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m2_axi_bid       ; 
  wire              			m2_axi_bvalid    ; 
  wire              			m2_axi_bready    ;  
  wire              			m2_axi_rready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m2_axi_rid       ; 
  wire  [    1:0]   			m2_axi_rresp     ; 
  wire              			m2_axi_rlast     ; 
  wire              			m2_axi_rvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m2_axi_rdata     ; 

  ///////////// AXI MASTER 3   
  wire  [AXI_ID_WIDTH-1:0]   	m3_axi_awid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0] m3_axi_awaddr    ; 
  wire  [    7:0]   			m3_axi_awlen     ; 
  wire  [    2:0]   			m3_axi_awsize    ; 
  wire  [    1:0]   			m3_axi_awburst   ; 
  wire  [    1:0]   			m3_axi_awlock    ; 
  wire              			m3_axi_awvalid   ; 
  wire              			m3_axi_awready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m3_axi_arid      ; 
  wire  [   AXI_ADDR_WIDTH-1:0] m3_axi_araddr    ; 
  wire  [    7:0]   			m3_axi_arlen     ; 
  wire  [    2:0]   			m3_axi_arsize    ; 
  wire  [    1:0]   			m3_axi_arburst   ; 
  wire  [    1:0]   			m3_axi_arlock    ; 
  wire              			m3_axi_arvalid   ; 
  wire              			m3_axi_arready   ; 
  wire  [AXI_ID_WIDTH-1:0]   	m3_axi_wid       ; 
  wire  [(AXI_DATA_WIDTH/8)-1:0]m3_axi_wstrb     ; 
  wire              			m3_axi_wlast     ; 
  wire              			m3_axi_wvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m3_axi_wdata     ; 
  wire              			m3_axi_wready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m3_axi_bid       ; 
  wire              			m3_axi_bvalid    ; 
  wire              			m3_axi_bready    ;  
  wire              			m3_axi_rready    ; 
  wire  [AXI_ID_WIDTH-1:0]   	m3_axi_rid       ; 
  wire  [    1:0]   			m3_axi_rresp     ; 
  wire              			m3_axi_rlast     ; 
  wire              			m3_axi_rvalid    ; 
  wire  [AXI_DATA_WIDTH-1:0]   	m3_axi_rdata     ; 


  wire    [S_COUNT*AXI_ID_WIDTH-1:0]  axi_m_awid;        //
  wire    [S_COUNT*AXI_ADDR_WIDTH-1:0]axi_m_awaddr;
  wire    [S_COUNT*8-1:0]         		axi_m_awlen;
  wire    [S_COUNT*3-1:0]         		axi_m_awsize;
  wire    [S_COUNT*2-1:0]         		axi_m_awburst;
  wire    [S_COUNT-1:0]           		axi_m_awlock;
  wire    [S_COUNT*4-1:0]         		axi_m_awcache;
  wire    [S_COUNT*3-1:0]         		axi_m_awprot;
  wire    [S_COUNT-1:0]           		axi_m_awvalid;
  wire    [S_COUNT-1:0]           		axi_m_awready;
  wire		[S_COUNT*AXI_ID_WIDTH-1:0]	axi_m_wid;
  wire    [S_COUNT*AXI_DATA_WIDTH-1:0]  axi_m_wdata;
  wire    [S_COUNT*(AXI_DATA_WIDTH/8)-1:0]  axi_m_wstrb;
  wire    [S_COUNT-1:0]           		axi_m_wlast;
  wire    [S_COUNT-1:0]           		axi_m_wvalid;
  wire    [S_COUNT-1:0]           		axi_m_wready;
  wire    [S_COUNT*AXI_ID_WIDTH-1:0]  axi_m_bid;
  wire    [S_COUNT*2-1:0]         		axi_m_bresp;
  wire    [S_COUNT-1:0]           		axi_m_bvalid;
  wire    [S_COUNT-1:0]           		axi_m_bready;
  wire    [S_COUNT*AXI_ID_WIDTH-1:0]  axi_m_arid;
  wire    [S_COUNT*AXI_ADDR_WIDTH-1:0]axi_m_araddr;
  wire    [S_COUNT*8-1:0]         		axi_m_arlen;
  wire    [S_COUNT*3-1:0]         		axi_m_arsize;
  wire    [S_COUNT*2-1:0]         		axi_m_arburst;
  wire    [S_COUNT-1:0]           		axi_m_arlock;
  wire    [S_COUNT-1:0]           		axi_m_arvalid;
  wire    [S_COUNT-1:0]           		axi_m_arready;
  wire    [S_COUNT*AXI_ID_WIDTH-1:0]  axi_m_rid;
  wire    [S_COUNT*AXI_DATA_WIDTH-1:0]axi_m_rdata;
  wire    [S_COUNT*2-1:0]         		axi_m_rresp;
  wire    [S_COUNT-1:0]           		axi_m_rlast;
  wire    [S_COUNT-1:0]           		axi_m_rvalid;
  wire    [S_COUNT-1:0]          			axi_m_rready;// 
  localparam AXI_FRAME_BURST_WIDTH = $clog2(AXI_DATA_WIDTH/8);
  wire [AXI_FRAME_BURST_WIDTH-1:0] ch0_axi_awlen;
  wire [AXI_FRAME_BURST_WIDTH-1:0] ch0_axi_arlen;
  wire [AXI_FRAME_BURST_WIDTH-1:0] ch1_axi_awlen;
  wire [AXI_FRAME_BURST_WIDTH-1:0] ch1_axi_arlen;
  wire [3:0] ch0_axi_awcache_unused;
  wire       ch0_axi_awcobuf_unused;
  wire       ch0_axi_awapcmd_unused;
  wire       ch0_axi_awallstrb_unused;
  wire       ch0_axi_awqos_unused;
  wire       ch0_axi_arapcmd_unused;
  wire       ch0_axi_arqos_unused;
  wire [3:0] ch1_axi_awcache_unused;
  wire       ch1_axi_awcobuf_unused;
  wire       ch1_axi_awapcmd_unused;
  wire       ch1_axi_awallstrb_unused;
  wire       ch1_axi_awqos_unused;
  wire       ch1_axi_arapcmd_unused;
  wire       ch1_axi_arqos_unused;
  reg [5:0] vs_cnt ;
  reg  out_sync;
  localparam [12:0] HDMI_H_FRONT_PORCH = HFP >> 1;
  localparam [12:0] HDMI_H_SYNC        = HSP >> 1;
  localparam [12:0] HDMI_H_VALID       = HACT >> 1;
  localparam [12:0] HDMI_H_BACK_PORCH  = HBP >> 1;
  localparam [12:0] HDMI_V_FRONT_PORCH = VFP;
  localparam [12:0] HDMI_V_SYNC        = VSP;
  localparam [12:0] HDMI_V_VALID       = VACT;
  localparam [12:0] HDMI_V_BACK_PORCH  = VBP;
  localparam        CH0_BAYER_SWAP_PIXELS = 1'b1;
  localparam        CH1_BAYER_SWAP_PIXELS = 1'b1;
  localparam [5:0]  CSI_RAW10_DATATYPE    = 6'h2B;

  function [31:0] raw10_4pix_to_raw8_4pix;
    input [39:0] raw10_4pix;
    begin
      // CSI RX outputs 4 RAW10 pixels per clock. Keep each pixel's MSB 8 bits.
      raw10_4pix_to_raw8_4pix = {
        raw10_4pix[39:32],
        raw10_4pix[29:22],
        raw10_4pix[19:12],
        raw10_4pix[9:2]
      };
    end
  endfunction
//=========================================================================
//signal define
//=========================================================================

/////////////////////////////////////////////////////////////////////////////
//Reset and PLL
assign sys_pll_rstn 	= i_sw[0];
assign ddr_pll_rstn     = i_sw[0];
assign MIPI_TX_PLL_RSTN = i_sw[0];
assign pll_byteclk_rstn = i_sw[0];
assign jtag_inst1_TDO   = 1'b0;
assign jtag_inst2_TDO   = 1'b0;
assign led[0] = ~ddr_cfg_ok; //D14
assign led[1] = vs_cnt[5];   //D15

assign axi_m_awlen[7:0]   = {{(8-AXI_FRAME_BURST_WIDTH){1'b0}}, ch0_axi_awlen};
assign axi_m_awlen[15:8]  = {{(8-AXI_FRAME_BURST_WIDTH){1'b0}}, ch1_axi_awlen};
assign axi_m_arlen[7:0]   = {{(8-AXI_FRAME_BURST_WIDTH){1'b0}}, ch0_axi_arlen};
assign axi_m_arlen[15:8]  = {{(8-AXI_FRAME_BURST_WIDTH){1'b0}}, ch1_axi_arlen};
assign axi_m_awcache      = {S_COUNT*4{1'b0}};
assign axi_m_awprot       = {S_COUNT*3{1'b0}};

assign axi0_ARAPCMD       = 1'b0;
assign axi0_ARQOS         = 1'b0;
assign axi0_AWALLSTRB     = 1'b0;
assign axi0_AWAPCMD       = 1'b0;
assign axi0_AWCOBUF       = 1'b0;
assign axi0_AWQOS         = 1'b0;

assign axi1_ARADDR        = {AXI_ADDR_WIDTH{1'b0}};
assign axi1_ARAPCMD       = 1'b0;
assign axi1_ARBURST       = 2'b01;
assign axi1_ARID          = {AXI_ID_WIDTH{1'b0}};
assign axi1_ARLEN         = 8'd0;
assign axi1_ARLOCK        = 1'b0;
assign axi1_ARQOS         = 1'b0;
assign axi1_ARSIZE        = 3'd0;
assign axi1_ARVALID       = 1'b0;
assign axi1_AWADDR        = {AXI_ADDR_WIDTH{1'b0}};
assign axi1_AWALLSTRB     = 1'b0;
assign axi1_AWAPCMD       = 1'b0;
assign axi1_AWBURST       = 2'b01;
assign axi1_AWCACHE       = 4'd0;
assign axi1_AWCOBUF       = 1'b0;
assign axi1_AWID          = {AXI_ID_WIDTH{1'b0}};
assign axi1_AWLEN         = 8'd0;
assign axi1_AWLOCK        = 1'b0;
assign axi1_AWQOS         = 1'b0;
assign axi1_AWSIZE        = 3'd0;
assign axi1_AWVALID       = 1'b0;
assign axi1_BREADY        = 1'b0;
assign axi1_RREADY        = 1'b0;
assign axi1_WDATA         = {AXI_DATA_WIDTH{1'b0}};
assign axi1_WLAST         = 1'b0;
assign axi1_WSTRB         = {(AXI_DATA_WIDTH/8){1'b0}};
assign axi1_WVALID        = 1'b0;

assign arst_n = sys_pll_lock & ddr_pll_lock & pll_byteclk_locked; // MIPI_TX_PLL removed: DSI TX decommissioned
reg [20:0] rst_cnt = 'd0;
always@( posedge i_fb_clk or negedge arst_n )
begin
    if( !arst_n )
        rst_cnt <= 'd0;
    else 
        rst_cnt <= rst_cnt[20] ? rst_cnt : rst_cnt + 1'b1;
end 
wire rst_n = rst_cnt[20];
//ddr4 config
always@(posedge i_fb_clk or negedge rst_n)
begin
   
    if(!rst_n)
    begin
        cfg_st <= IDLE;
        cfg_count <= 'h0;
    end 
    else
    begin
        cfg_st <= cfg_next;

        if (cfg_st == IDLE)
            cfg_count <= cfg_count + 1'b1;
        else 
            cfg_count <= 'h0;
    end 
        
end

always@(*)
begin
    cfg_next = cfg_st;
    case(cfg_st)
    IDLE:
    begin
        if(cfg_count == 'hff)
            cfg_next = CFG_START;
        else
            cfg_next = IDLE;
    end
    CFG_START:
    begin
        if(ddr_inst_CFG_DONE)
            cfg_next = CFG_DONE;
        else
            cfg_next = CFG_START;
    end
    CFG_DONE:
        cfg_next = CFG_DONE;
    default:
        cfg_next = IDLE;
    endcase
end

assign ddr_inst_CFG_START    = (cfg_st != IDLE);
assign ddr_cfg_ok   = (cfg_st == CFG_DONE);
assign ddr_inst_CFG_RST    = (cfg_st == IDLE);
assign ddr_inst_CFG_SEL      = 1'b0;

assign axi0_ARESETn = ddr_cfg_ok;
assign axi1_ARESETn = ddr_cfg_ok;
wire sys_rst_n = ddr_cfg_ok;

// pixel_data_en: locally generated reset-delay, replaces DSI vid_rst_n
wire pixel_data_en;
reg [26:0] vid_dly_cnt;
always @(posedge i_sysclk_div2 or negedge sys_rst_n) begin
    if (!sys_rst_n)
        vid_dly_cnt <= 'd0;
    else if (!vid_dly_cnt[26])
        vid_dly_cnt <= vid_dly_cnt + 1'b1;
end
assign pixel_data_en = vid_dly_cnt[26];

//============================================================================================
//
//============================================================================================





// wire        hs;
// wire        vs;
// wire        de;
// wire [ 7:0] r_data;
// wire [ 7:0] g_data;
// wire [ 7:0] b_data;
// wire [12:0] hact;
// wire [12:0] vact;
    //     color_bar_rgb #(
	// 		.HS_POLORY 		(1'b0	    ),
	// 		.VS_POLORY 		(1'b0	    ),
            
    //         .TEST_MODE 		(2'b00		)
	// )u_color_bar_rgb(
	// /*i*/.clk	(i_sys_clk),
	// /*i*/.rst_n	(pixel_data_en),//(sys_rst_n ),
    //      .H_VALID 		(HACT/2	    ),
    //     .V_VALID 		(VACT	    ),
    //     .H_FRONT_PORCH 	(HFP/2	    ),
    //     .H_SYNC 		(HSP	    ),
    //     .H_BACK_PORCH 	(HBP/2	    ),
    //     .V_FRONT_PORCH 	(VFP		),
    //     .V_SYNC 		(VSP		),
    //     .V_BACK_PORCH 	(VBP		),
   
	// /*o*/.hs	(hs),
	// /*o*/.vs	(vs),
	// /*o*/.de	(de),
	// /*O*/.h_cnt (h_cnt),
	// /*O*/.v_cnt (v_cnt),
	// /*o*/.rgb_r	(r_data),    //像素数据、红色分量
	// /*o*/.rgb_g	(g_data),    //像素数据、绿色分量
	// /*o*/.rgb_b (b_data)    //像素数据、蓝色分量
	
	// );

//========================================================================== 
// csi 
//========================================================================== 
    wire		       w_mipi_rx_vs1;
    wire		       w_mipi_rx_hs1;
    wire	         w_mipi_rx_de1;
    wire	[63:0]	 w_mipi_rx_data1	;
    reg rx_out_vs_r;

    
    
    always @( posedge i_sysclk_div2 )
    begin 
        rx_out_vs_r <= rx_out_vs;
        if( {rx_out_vs_r,rx_out_vs} == 2'b01)
            vs_cnt <= vs_cnt + 1'b1;
    end 

wire rx_out_de;
wire rx_out_hs;
wire rx_out_vs;
wire [PACK_BIT-1:0] rx_out_data;
wire [5:0] rx_out_datatype;
wire [3:0] rx_out_pixel_per_clk;

wire rx_out_de1;
wire rx_out_hs1;
wire rx_out_vs1;
wire [PACK_BIT-1:0] rx_out_data1;
wire [5:0] rx_out_datatype1;
wire [3:0] rx_out_pixel_per_clk1;
wire [31:0] ch0_raw8_4pix;
wire [31:0] ch1_raw8_4pix;
wire [15:0] ch0_bayer_2pix;
wire [15:0] ch1_bayer_2pix;
wire ch0_csi_format_ok;
wire ch1_csi_format_ok;

assign ch0_raw8_4pix = raw10_4pix_to_raw8_4pix(rx_out_data[39:0]);
assign ch1_raw8_4pix = raw10_4pix_to_raw8_4pix(rx_out_data1[39:0]);
assign ch0_csi_format_ok = (rx_out_datatype == CSI_RAW10_DATATYPE) & (rx_out_pixel_per_clk == 4'd4);
assign ch1_csi_format_ok = (rx_out_datatype1 == CSI_RAW10_DATATYPE) & (rx_out_pixel_per_clk1 == 4'd4);


  soft_mipi_rx_top # (
    .PACK_BIT(PACK_BIT)
  )
  soft_mipi_rx_top_inst (
    .mipi_clk                   (   mipi_clk                   ),
    .i_sysclk_div2              (   i_sysclk_div2              ),
    .arst_n                     (   arst_n                     ),
    .mipi_rx_ck0_CLKOUT         (   mipi_rx_ck0_CLKOUT         ),
    .io_cam_scl_IN              (   S0_io_cam_scl_IN           ),
    .io_cam_sda_IN              (   S0_io_cam_sda_IN           ),
    .io_cam_scl_OUT             (   S0_io_cam_scl_OUT          ),
    .io_cam_scl_OE              (   S0_io_cam_scl_OE           ),
    .io_cam_sda_OUT             (   S0_io_cam_sda_OUT          ),
    .io_cam_sda_OE              (   S0_io_cam_sda_OE           ),
    .o_cam_rst_p                (   S0_o_cam_rst_p             ),
    .mipi_rx_ck0_HS_ENA         (   mipi_rx_ck0_HS_ENA         ),
    .mipi_rx_dp00_HS_ENA        (   mipi_rx_dp00_HS_ENA        ),
    .mipi_rx_dp01_HS_ENA        (   mipi_rx_dp01_HS_ENA        ),
    .mipi_rx_dp02_HS_ENA        (   mipi_rx_dp02_HS_ENA        ),
    .mipi_rx_dp03_HS_ENA        (   mipi_rx_dp03_HS_ENA        ),
    .mipi_rx_ck0_HS_TERM        (   mipi_rx_ck0_HS_TERM        ),
    .mipi_rx_dp00_HS_TERM       (   mipi_rx_dp00_HS_TERM       ),
    .mipi_rx_dp01_HS_TERM       (   mipi_rx_dp01_HS_TERM       ),
    .mipi_rx_dp02_HS_TERM       (   mipi_rx_dp02_HS_TERM       ),
    .mipi_rx_dp03_HS_TERM       (   mipi_rx_dp03_HS_TERM       ),
    .mipi_rx_dp00_RST           (   mipi_rx_dp00_RST           ),
    .mipi_rx_dp01_RST           (   mipi_rx_dp01_RST           ),
    .mipi_rx_dp02_RST           (   mipi_rx_dp02_RST           ),
    .mipi_rx_dp03_RST           (   mipi_rx_dp03_RST           ),
    .mipi_rx_ck0_LP_N_IN        (   mipi_rx_ck0_LP_N_IN        ),
    .mipi_rx_ck0_LP_P_IN        (   mipi_rx_ck0_LP_P_IN        ),
    .mipi_rx_dp00_LP_N_IN       (   mipi_rx_dp00_LP_N_IN       ),
    .mipi_rx_dp00_LP_P_IN       (   mipi_rx_dp00_LP_P_IN       ),
    .mipi_rx_dp01_LP_N_IN       (   mipi_rx_dp01_LP_N_IN       ),
    .mipi_rx_dp01_LP_P_IN       (   mipi_rx_dp01_LP_P_IN       ),
    .mipi_rx_dp02_LP_N_IN       (   mipi_rx_dp02_LP_N_IN       ),
    .mipi_rx_dp02_LP_P_IN       (   mipi_rx_dp02_LP_P_IN       ),
    .mipi_rx_dp03_LP_N_IN       (   mipi_rx_dp03_LP_N_IN       ),
    .mipi_rx_dp03_LP_P_IN       (   mipi_rx_dp03_LP_P_IN       ),
    .mipi_rx_dp00_HS_IN         (   mipi_rx_dp00_HS_IN         ),
    .mipi_rx_dp01_HS_IN         (   mipi_rx_dp01_HS_IN         ),
    .mipi_rx_dp02_HS_IN         (   mipi_rx_dp02_HS_IN         ),
    .mipi_rx_dp03_HS_IN         (   mipi_rx_dp03_HS_IN         ),
    .mipi_rx_dp00_FIFO_RD       (   mipi_rx_dp00_FIFO_RD       ),
    .mipi_rx_dp01_FIFO_RD       (   mipi_rx_dp01_FIFO_RD       ),
    .mipi_rx_dp02_FIFO_RD       (   mipi_rx_dp02_FIFO_RD       ),
    .mipi_rx_dp03_FIFO_RD       (   mipi_rx_dp03_FIFO_RD       ),
    .mipi_rx_dp00_FIFO_EMPTY    (   mipi_rx_dp00_FIFO_EMPTY    ),
    .mipi_rx_dp01_FIFO_EMPTY    (   mipi_rx_dp01_FIFO_EMPTY    ),
    .mipi_rx_dp02_FIFO_EMPTY    (   mipi_rx_dp02_FIFO_EMPTY    ),
    .mipi_rx_dp03_FIFO_EMPTY    (   mipi_rx_dp03_FIFO_EMPTY    ),
    .rx_out_de                  (   rx_out_de                  ),
    .rx_out_hs                  (   rx_out_hs                  ),
    .rx_out_vs                  (   rx_out_vs                  ),
    .rx_out_data                (   rx_out_data                ),
    .rx_out_datatype            (   rx_out_datatype            ),
    .rx_out_pixel_per_clk       (   rx_out_pixel_per_clk       )
  );


  soft_mipi_rx_top # (
    .PACK_BIT(PACK_BIT)
  )
  soft_mipi_rx_top_inst1 (
    .mipi_clk                   (   mipi_clk                   ),
    .i_sysclk_div2              (   i_sysclk_div2              ),
    .arst_n                     (   arst_n                     ),
    
    .io_cam_scl_IN              (   S1_io_cam_scl_IN           ),
    .io_cam_sda_IN              (   S1_io_cam_sda_IN           ),
    .io_cam_scl_OUT             (   S1_io_cam_scl_OUT          ),
    .io_cam_scl_OE              (   S1_io_cam_scl_OE           ),
    .io_cam_sda_OUT             (   S1_io_cam_sda_OUT          ),
    .io_cam_sda_OE              (   S1_io_cam_sda_OE           ),
    .o_cam_rst_p                (   S1_o_cam_rst_p             ),

    .mipi_rx_ck0_CLKOUT         (   mipi_rx_ck1_CLKOUT         ),
    .mipi_rx_ck0_HS_ENA         (   mipi_rx_ck1_HS_ENA         ),
    .mipi_rx_ck0_HS_TERM        (   mipi_rx_ck1_HS_TERM        ),
    .mipi_rx_ck0_LP_N_IN        (   mipi_rx_ck1_LP_N_IN        ),
    .mipi_rx_ck0_LP_P_IN        (   mipi_rx_ck1_LP_P_IN        ),

    .mipi_rx_dp00_HS_ENA        (   mipi_rx_dp10_HS_ENA        ),
    .mipi_rx_dp01_HS_ENA        (   mipi_rx_dp11_HS_ENA        ),
    .mipi_rx_dp02_HS_ENA        (   mipi_rx_dp12_HS_ENA        ),
    .mipi_rx_dp03_HS_ENA        (   mipi_rx_dp13_HS_ENA        ),
    
    .mipi_rx_dp00_HS_TERM       (   mipi_rx_dp10_HS_TERM       ),
    .mipi_rx_dp01_HS_TERM       (   mipi_rx_dp11_HS_TERM       ),
    .mipi_rx_dp02_HS_TERM       (   mipi_rx_dp12_HS_TERM       ),
    .mipi_rx_dp03_HS_TERM       (   mipi_rx_dp13_HS_TERM       ),
    .mipi_rx_dp00_RST           (   mipi_rx_dp10_RST           ),
    .mipi_rx_dp01_RST           (   mipi_rx_dp11_RST           ),
    .mipi_rx_dp02_RST           (   mipi_rx_dp12_RST           ),
    .mipi_rx_dp03_RST           (   mipi_rx_dp13_RST           ),
    .mipi_rx_dp00_LP_N_IN       (   mipi_rx_dp10_LP_N_IN       ),
    .mipi_rx_dp00_LP_P_IN       (   mipi_rx_dp10_LP_P_IN       ),
    .mipi_rx_dp01_LP_N_IN       (   mipi_rx_dp11_LP_N_IN       ),
    .mipi_rx_dp01_LP_P_IN       (   mipi_rx_dp11_LP_P_IN       ),
    .mipi_rx_dp02_LP_N_IN       (   mipi_rx_dp12_LP_N_IN       ),
    .mipi_rx_dp02_LP_P_IN       (   mipi_rx_dp12_LP_P_IN       ),
    .mipi_rx_dp03_LP_N_IN       (   mipi_rx_dp13_LP_N_IN       ),
    .mipi_rx_dp03_LP_P_IN       (   mipi_rx_dp13_LP_P_IN       ),
    .mipi_rx_dp00_HS_IN         (   mipi_rx_dp10_HS_IN         ),
    .mipi_rx_dp01_HS_IN         (   mipi_rx_dp11_HS_IN         ),
    .mipi_rx_dp02_HS_IN         (   mipi_rx_dp12_HS_IN         ),
    .mipi_rx_dp03_HS_IN         (   mipi_rx_dp13_HS_IN         ),
    .mipi_rx_dp00_FIFO_RD       (   mipi_rx_dp10_FIFO_RD       ),
    .mipi_rx_dp01_FIFO_RD       (   mipi_rx_dp11_FIFO_RD       ),
    .mipi_rx_dp02_FIFO_RD       (   mipi_rx_dp12_FIFO_RD       ),
    .mipi_rx_dp03_FIFO_RD       (   mipi_rx_dp13_FIFO_RD       ),
    .mipi_rx_dp00_FIFO_EMPTY    (   mipi_rx_dp10_FIFO_EMPTY    ),
    .mipi_rx_dp01_FIFO_EMPTY    (   mipi_rx_dp11_FIFO_EMPTY    ),
    .mipi_rx_dp02_FIFO_EMPTY    (   mipi_rx_dp12_FIFO_EMPTY    ),
    .mipi_rx_dp03_FIFO_EMPTY    (   mipi_rx_dp13_FIFO_EMPTY    ),
    .rx_out_de                  (   rx_out_de1                  ),
    .rx_out_hs                  (   rx_out_hs1                  ),
    .rx_out_vs                  (   rx_out_vs1                  ),
    .rx_out_data                (   rx_out_data1                ),
    .rx_out_datatype            (   rx_out_datatype1            ),
    .rx_out_pixel_per_clk       (   rx_out_pixel_per_clk1       )
  );
`ifdef  FRAME_BUFFER

//============================================================================================ 
//frame_buffer 0 
//============================================================================================	
  
  wire [7:0] 	ch0_r;
  wire [7:0]    ch0_g;
  wire [7:0]    ch0_b;
  wire ch0_vs;
  wire ch0_hs;
  wire ch0_de;
  wire ch0_frame_ready;
  wire ch0_fifo_underflow;
  assign ch0_bayer_2pix = CH0_BAYER_SWAP_PIXELS ? {ch0_b, ch0_g} : {ch0_g, ch0_b};
frame_buffer #(
.AXI_DATA_WIDTH ( AXI_DATA_WIDTH	),
.I_VID_WIDTH    ( I_VID_WIDTH       ),
.O_VID_WIDTH    ( O_VID_WIDTH       ),
.FB_NUM         ( FB_NUM            ),
.BURST_LEN      ( BURST_LEN         ),
.MAX_VID_WIDTH 	( MAX_VID_WIDTH     ),
.MAX_VID_HIGHT 	( MAX_VID_HIGHT     ),
.AXI_ADDR_WIDTH ( AXI_ADDR_WIDTH	),
.WR_FIFO_DEPTH	( WR_FIFO_DEPTH		),    
.RD_FIFO_DEPTH 	( RD_FIFO_DEPTH 	),
.START_ADDR		(	START_ADDR		)
)u_frame_buffer(
    .axi_clk(axi0_ACLK),
    .rst_n(pixel_data_en),//(sys_rst_n),
    // .i_clk  (i_sys_clk) ,
    // .i_vs   (vs) , 
    // .i_de   (de) , 
    // .vin   ({r_data,g_data,b_data}) ,// ({24'habcdef}),//

/*i*/.i_clk			(i_sysclk_div2      ),
/*i*/.i_vs			(rx_out_vs	),
/*i*/.i_hs			(rx_out_hs	),
/*i*/.i_de			(rx_out_de 	),
/*i*/.vin 			(ch0_raw8_4pix	),

    .o_clk  (i_sysclk_div2) ,
    // .o_hs   (fb_ch0_hs) ,
    // .o_vs   (fb_ch0_vs) ,
    // .o_de   (fb_ch0_de) ,
    // .vout   ({fb_ch0_dout}) ,

    /*i*/.o_hs    		(ch0_hs		),			
/*i*/.o_vs    		(ch0_vs		),			
/*i*/.o_de    		(ch0_de		),			
/*i*/.vout    		({ch0_g,ch0_b}	),//ch0_r,
    .frame_ready        (ch0_frame_ready),
    .fifo_rd_underflow  (ch0_fifo_underflow),

    .H_FRONT_PORCH 	(HDMI_H_FRONT_PORCH),
    .H_SYNC 		(HDMI_H_SYNC       ),
    .H_VALID 		(HDMI_H_VALID      ),
    .H_BACK_PORCH 	(HDMI_H_BACK_PORCH ),
    .V_FRONT_PORCH 	(HDMI_V_FRONT_PORCH),
    .V_SYNC 		(HDMI_V_SYNC       ),
    .V_VALID 		(HDMI_V_VALID      ),
    .V_BACK_PORCH 	(HDMI_V_BACK_PORCH ),
    .out_sync         (out_sync),
    .awid   (axi_m_awid		  [1*AXI_ID_WIDTH-1   : 0]		),//(m0_axi_awid      ),//(AXI_MUX_EN ? : axi0_AWID     ),
.awaddr     (axi_m_awaddr	  [1*AXI_ADDR_WIDTH-1 : 0]		),//(m0_axi_awaddr    ),//(AXI_MUX_EN ? : axi0_AWADDR   ),
.awlen      (ch0_axi_awlen					),//(m0_axi_awlen     ),//(AXI_MUX_EN ? : axi0_AWLEN    ),
.awsize     (axi_m_awsize	  [1*3-1			 :0]		),//(m0_axi_awsize    ),//(AXI_MUX_EN ? : axi0_AWSIZE   ),
.awburst    (axi_m_awburst	[1*2-1      : 0]				),//(m0_axi_awburst   ),//(AXI_MUX_EN ? : axi0_AWBURST  ),
.awcache    (ch0_axi_awcache_unused),//(m0_axi_awcache   ),//(AXI_MUX_EN ? : axi0_AWCACHE  ),
.awlock     (axi_m_awlock 	[1*1-1      : 0]				),//(m0_axi_awlock    ),//(AXI_MUX_EN ? : axi0_AWLOCK   ),
.awvalid    (axi_m_awvalid	[1*1-1      : 0]				),//(m0_axi_awvalid   ),//(AXI_MUX_EN ? : axi0_AWVALID  ),
.awcobuf    (ch0_axi_awcobuf_unused),//(axi_m_wid			[1*AXI_ID_WIDTH-1:0]		),//(m0_axi_awcobuf   ),//(AXI_MUX_EN ? : axi0_AWCOBUF  ),
.awapcmd    (ch0_axi_awapcmd_unused),//(m0_axi_awapcmd   ),//(AXI_MUX_EN ? : axi0_AWAPCMD  ),
.awallstrb  (ch0_axi_awallstrb_unused),//(m0_axi_awallstrb ),//(AXI_MUX_EN ? : axi0_AWALLSTRB),
.awready    (axi_m_awready	[1*1-1      : 0]				),//(m0_axi_awready   ),//(AXI_MUX_EN ? : axi0_AWREADY  ),
.awqos      (ch0_axi_awqos_unused),//(axi_m_bresp		[1*2-1      : 0]			),//(m0_axi_awqos     ),//(AXI_MUX_EN ? : axi0_AWQOS    ),
.arid       (axi_m_arid		  [1*AXI_ID_WIDTH-1   : 0]		),//(m0_axi_arid      ),//(AXI_MUX_EN ? : axi0_ARID     ),
.araddr     (axi_m_araddr	  [1*AXI_ADDR_WIDTH-1 : 0]		),//(m0_axi_araddr    ),//(AXI_MUX_EN ? : axi0_ARADDR   ),
.arlen      (ch0_axi_arlen   					),//(m0_axi_arlen     ),//(AXI_MUX_EN ? : axi0_ARLEN    ),
.arsize     (axi_m_arsize 	[1*3-1      : 0]   				),//(m0_axi_arsize    ),//(AXI_MUX_EN ? : axi0_ARSIZE   ),
.arburst    (axi_m_arburst	[1*2-1      : 0]   				),//(m0_axi_arburst   ),//(AXI_MUX_EN ? : axi0_ARBURST  ),
.arlock     (axi_m_arlock	  [1*1-1      : 0]   			),//(m0_axi_arlock    ),//(AXI_MUX_EN ? : axi0_ARLOCK   ),
.arvalid    (axi_m_arvalid	[1*1-1      : 0]				),//(m0_axi_arvalid   ),//(AXI_MUX_EN ? : axi0_ARVALID  ),
.arapcmd    (ch0_axi_arapcmd_unused),//(m0_axi_arapcmd   ),//(AXI_MUX_EN ? : axi0_ARAPCMD  ),
.arready    (axi_m_arready	[1*1-1      : 0]				),//(m0_axi_arready   ),//(AXI_MUX_EN ? : axi0_ARREADY  ),
.arqos      (ch0_axi_arqos_unused),//(m0_axi_arqos     ),//(AXI_MUX_EN ? : axi0_ARQOS    ),
.wdata      (axi_m_wdata		[1*AXI_DATA_WIDTH-1 : 0]	),//(m0_axi_wdata     ),//(AXI_MUX_EN ? : axi0_WDATA    ),
.wstrb      (axi_m_wstrb		[1*(AXI_DATA_WIDTH/8)-1 : 0]			),//(m0_axi_wstrb     ),//(AXI_MUX_EN ? : axi0_WSTRB    ),
.wlast      (axi_m_wlast		[1*1-1      : 0]			),//(m0_axi_wlast     ),//(AXI_MUX_EN ? : axi0_WLAST    ),
.wvalid     (axi_m_wvalid	  [1*1-1      : 0]				),//(m0_axi_wvalid    ),//(AXI_MUX_EN ? : axi0_WVALID   ),
.wready     (axi_m_wready	  [1*1-1      : 0]				),//(m0_axi_wready    ),//(AXI_MUX_EN ? : axi0_WREADY   ),
.rid        (axi_m_rid			[1*AXI_ID_WIDTH-1      : 0]   			),//(m0_axi_rid       ),//(AXI_MUX_EN ? : axi0_RID      ),
.rdata      (axi_m_rdata		[1*AXI_DATA_WIDTH-1 : 0]	),//(m0_axi_rdata     ),//(AXI_MUX_EN ? : axi0_RDATA    ),
.rlast      (axi_m_rlast		[1*1-1      : 0]   			),//(m0_axi_rlast     ),//(AXI_MUX_EN ? : axi0_RLAST    ),
.rvalid     (axi_m_rvalid	  [1*1-1      : 0]   			),//(m0_axi_rvalid    ),//(AXI_MUX_EN ? : axi0_RVALID   ),
.rready     (axi_m_rready	  [1*1-1      : 0]   			),//(m0_axi_rready    ),//(AXI_MUX_EN ? : axi0_RREADY   ),
.rresp      (axi_m_rresp		[1*2-1      : 0]   			),//(m0_axi_rresp     ),//(AXI_MUX_EN ? : axi0_RRESP    ),
.bid        (axi_m_bid			[1*AXI_ID_WIDTH-1      : 0]			),//(m0_axi_bid       ),//(AXI_MUX_EN ? : axi0_BID      ),
.bvalid     (axi_m_bvalid	  [1*1-1      : 0]				),//(m0_axi_bvalid    ),//(AXI_MUX_EN ? : axi0_BVALID   ),
.bready     (axi_m_bready	  [1*1-1      : 0]				)//(m0_axi_bready    ) //(AXI_MUX_EN ? : axi0_BREADY   )
);



// color_bar_checker  #(
//     .DATA_WIDTH (O_VID_WIDTH)
// )u_color_bar_checker (
//     .clk(i_sysclk_div2),
//     .rst_n(sys_rst_n),
//     .i_hs(fb_ch0_hs),
//     .i_vs(fb_ch0_vs),
//     .i_de(fb_ch0_de),
//     .vin(fb_ch0_dout),
//     .check_fail(check_fail)
//   );

//============================================================================================ 
//framebuffer 1
//============================================================================================	
// wire fb_ch1_hs;
// wire fb_ch1_vs;
// wire fb_ch1_de;
// wire [48-1:0] fb_ch1_dout;
wire        hs1;
wire        vs1;
wire        de1;
wire [ 7:0] r_data1;
wire [ 7:0] g_data1;
wire [ 7:0] b_data1;
wire [12:0] hact1;
wire [12:0] vact1;
   

    
// 	color_bar_rgb # (
//     .DYN_EN(1'b0),
//     .HS_POLORY(1'b0),
//     .VS_POLORY(1'b0),
//     .SYMBOL_WIDTH(8),
//     .SYMBOL_NUM(3),
//     .PAR_PIXEL_NUM(1),
//     .HFP(HFP),
//     .HST(HSP),
//     .HACT(3840),
//     .HBP(HBP),
//     .VFP(VFP),
//     .VST(VSP),
//     .VACT(2160),
//     .VBP(VBP),
//     .TEST_MODE(2'd1)
//   )
//   color_bar_rgb_inst (
//     .clk(clk_54m),
//     .rst_n(1'b1),
//     .i_cfg_vid(i_cfg_vid),
//     .h_cnt(h_cnt1),
//     .v_cnt(v_cnt1),
//     .hs(hs1),
//     .vs(vs1),
//     .de(de1),
//     .o_vid_data({r_data1,g_data1,b_data1})
//   );
	

wire [7:0] 	ch1_r;
wire [7:0]  ch1_g;
wire [7:0]  ch1_b;
wire ch1_vs;
wire ch1_hs;
wire ch1_de;
wire ch1_frame_ready;
wire ch1_fifo_underflow;
assign ch1_bayer_2pix = CH1_BAYER_SWAP_PIXELS ? {ch1_b, ch1_g} : {ch1_g, ch1_b};
frame_buffer #(
.AXI_DATA_WIDTH ( AXI_DATA_WIDTH	),
.I_VID_WIDTH    ( I_VID_WIDTH       ),
.O_VID_WIDTH    ( O_VID_WIDTH       ),
.FB_NUM         ( FB_NUM            ),
.BURST_LEN      ( BURST_LEN         ),
.MAX_VID_WIDTH 	( 1920     ),
.MAX_VID_HIGHT 	( 1080     ),
.AXI_ADDR_WIDTH ( AXI_ADDR_WIDTH	),
.WR_FIFO_DEPTH	( WR_FIFO_DEPTH		),    
.RD_FIFO_DEPTH 	( RD_FIFO_DEPTH 	),
.START_ADDR		( 33'h0_0180_0000	)
)u_frame_buffer1(
  .axi_clk(axi0_ACLK),
  .rst_n(pixel_data_en),//(sys_rst_n),
//   .i_clk  (i_sysclk_div2) ,
//   .i_vs   (vs1) , 
//   .i_de   (de1) , 
//   .vin   ({24'd0,r_data1,g_data1,b_data1}) ,// ({24'habcdef}),//

/*i*/.i_clk			(i_sysclk_div2      ),
/*i*/.i_vs			(rx_out_vs1	),
/*i*/.i_hs			(rx_out_hs1	),
/*i*/.i_de			(rx_out_de1 	),
/*i*/.vin 			(ch1_raw8_4pix	),

/*i*/.o_clk       (i_sysclk_div2  ) ,
/*i*/.o_hs    		(ch1_hs	    ),			
/*i*/.o_vs    		(ch1_vs	    ),			
/*i*/.o_de    		(ch1_de	    ),			
/*i*/.vout    		({ch1_g,ch1_b}	),//ch0_r,
   .frame_ready       (ch1_frame_ready),
   .fifo_rd_underflow (ch1_fifo_underflow),

   .H_FRONT_PORCH 	(HDMI_H_FRONT_PORCH),
    .H_SYNC 		(HDMI_H_SYNC       ),
    .H_VALID 		(HDMI_H_VALID      ),
    .H_BACK_PORCH 	(HDMI_H_BACK_PORCH ),
    .V_FRONT_PORCH 	(HDMI_V_FRONT_PORCH),
    .V_SYNC 		(HDMI_V_SYNC       ),
    .V_VALID 		(HDMI_V_VALID      ),
    .V_BACK_PORCH 	(HDMI_V_BACK_PORCH ),
  .out_sync         (out_sync       ),
.awid       (axi_m_awid		 [2*AXI_ID_WIDTH-1   : 1*AXI_ID_WIDTH]		    ),//(m0_axi_awid      ),//(AXI_MUX_EN ? : axi0_AWID     ),
.awaddr     (axi_m_awaddr	 [2*AXI_ADDR_WIDTH-1 : 1*AXI_ADDR_WIDTH]		),//(m0_axi_awaddr    ),//(AXI_MUX_EN ? : axi0_AWADDR   ),
.awlen      (ch1_axi_awlen			    ),//(m0_axi_awlen     ),//(AXI_MUX_EN ? : axi0_AWLEN    ),
.awsize     (axi_m_awsize	 [2*3-1		 : 1*3]		        ),//(m0_axi_awsize    ),//(AXI_MUX_EN ? : axi0_AWSIZE   ),
.awburst    (axi_m_awburst	 [2*2-1      : 1*2]				),//(m0_axi_awburst   ),//(AXI_MUX_EN ? : axi0_AWBURST  ),
.awcache    (ch1_axi_awcache_unused),
.awlock     (axi_m_awlock 	 [2*1-1      : 1*1]				),//(m0_axi_awlock    ),//(AXI_MUX_EN ? : axi0_AWLOCK   ),
.awvalid    (axi_m_awvalid	 [2*1-1      : 1*1]				),//(m0_axi_awvalid   ),//(AXI_MUX_EN ? : axi0_AWVALID  ),
.awcobuf    (ch1_axi_awcobuf_unused),
.awapcmd    (ch1_axi_awapcmd_unused),
.awallstrb  (ch1_axi_awallstrb_unused),
.awready    (axi_m_awready	 [2*1-1      : 1*1]				),//(m0_axi_awready   ),//(AXI_MUX_EN ? : axi0_AWREADY  ),
.awqos      (ch1_axi_awqos_unused),
.arid       (axi_m_arid		 [2*AXI_ID_WIDTH-1   : 1*AXI_ID_WIDTH]		    ),//(m0_axi_arid      ),//(AXI_MUX_EN ? : axi0_ARID     ),
.araddr     (axi_m_araddr	 [2*AXI_ADDR_WIDTH-1 : 1*AXI_ADDR_WIDTH]		),//(m0_axi_araddr    ),//(AXI_MUX_EN ? : axi0_ARADDR   ),
.arlen      (ch1_axi_arlen   			),//(m0_axi_arlen     ),//(AXI_MUX_EN ? : axi0_ARLEN    ),
.arsize     (axi_m_arsize 	 [2*3-1      : 1*3]   			),//(m0_axi_arsize    ),//(AXI_MUX_EN ? : axi0_ARSIZE   ),
.arburst    (axi_m_arburst	 [2*2-1      : 1*2]   			),//(m0_axi_arburst   ),//(AXI_MUX_EN ? : axi0_ARBURST  ),
.arlock     (axi_m_arlock	 [2*1-1      : 1*1]   			),//(m0_axi_arlock    ),//(AXI_MUX_EN ? : axi0_ARLOCK   ),
.arvalid    (axi_m_arvalid	 [2*1-1      : 1*1]				),//(m0_axi_arvalid   ),//(AXI_MUX_EN ? : axi0_ARVALID  ),
.arapcmd    (ch1_axi_arapcmd_unused),
.arready    (axi_m_arready	 [2*1-1      : 1*1]				),//(m0_axi_arready   ),//(AXI_MUX_EN ? : axi0_ARREADY  ),
.arqos      (ch1_axi_arqos_unused),
.wdata      (axi_m_wdata	 [2*AXI_DATA_WIDTH-1 : 1*AXI_DATA_WIDTH]	        ),//(m0_axi_wdata     ),//(AXI_MUX_EN ? : axi0_WDATA    ),
.wstrb      (axi_m_wstrb	 [2*(AXI_DATA_WIDTH/8)-1 : 1*(AXI_DATA_WIDTH/8)]	),//(m0_axi_wstrb     ),//(AXI_MUX_EN ? : axi0_WSTRB    ),
.wlast      (axi_m_wlast	 [2*1-1      : 1*1]			    ),//(m0_axi_wlast     ),//(AXI_MUX_EN ? : axi0_WLAST    ),
.wvalid     (axi_m_wvalid	 [2*1-1      : 1*1]				),//(m0_axi_wvalid    ),//(AXI_MUX_EN ? : axi0_WVALID   ),
.wready     (axi_m_wready	 [2*1-1      : 1*1]				),//(m0_axi_wready    ),//(AXI_MUX_EN ? : axi0_WREADY   ),
.rid        (axi_m_rid		 [2*AXI_ID_WIDTH-1      : 1*AXI_ID_WIDTH]   			),//(m0_axi_rid       ),//(AXI_MUX_EN ? : axi0_RID      ),
.rdata      (axi_m_rdata	 [2*AXI_DATA_WIDTH-1 : 1*AXI_DATA_WIDTH]	        ),//(m0_axi_rdata     ),//(AXI_MUX_EN ? : axi0_RDATA    ),
.rlast      (axi_m_rlast	 [2*1-1      : 1*1]   			),//(m0_axi_rlast     ),//(AXI_MUX_EN ? : axi0_RLAST    ),
.rvalid     (axi_m_rvalid	 [2*1-1      : 1*1]   			),//(m0_axi_rvalid    ),//(AXI_MUX_EN ? : axi0_RVALID   ),
.rready     (axi_m_rready	 [2*1-1      : 1*1]   			),//(m0_axi_rready    ),//(AXI_MUX_EN ? : axi0_RREADY   ),
.rresp      (axi_m_rresp	 [2*2-1      : 1*2]   			),//(m0_axi_rresp     ),//(AXI_MUX_EN ? : axi0_RRESP    ),
.bid        (axi_m_bid		 [2*AXI_ID_WIDTH-1      : 1*AXI_ID_WIDTH]			    ),//(m0_axi_bid       ),//(AXI_MUX_EN ? : axi0_BID      ),
.bvalid     (axi_m_bvalid	 [2*1-1      : 1*1]				),//(m0_axi_bvalid    ),//(AXI_MUX_EN ? : axi0_BVALID   ),
.bready     (axi_m_bready	 [2*1-1      : 1*1]				)//(m0_axi_bready    ) //(AXI_MUX_EN ? : axi0_BREADY   )

);



// color_bar_checker  #(
//     .DATA_WIDTH (48)
// )u_color_bar_checker (
//     .clk(i_sysclk_div2),
//     .rst_n(sys_rst_n),
//     .i_hs(fb_ch1_hs),
//     .i_vs(fb_ch1_vs),
//     .i_de(fb_ch1_de),
//     .vin(fb_ch1_dout),
//     .check_fail(check_fail)
//   );

//======================================================================================================== 
// axi_interconnect
//======================================================================================================== 
axi_interconnect #
(
    .S_COUNT                            (S_COUNT                            ),
    .M_COUNT                            (M_COUNT                            ),
    .DATA_WIDTH                         (AXI_DATA_WIDTH                     ),
    .ADDR_WIDTH                         (AXI_ADDR_WIDTH                     ),
    .ID_WIDTH                           (AXI_ID_WIDTH                       )
)
uw_axi_interconnect
(
    .clk                                (axi0_ACLK                            ),
    .rst                                (~pixel_data_en                         ),
//AXI slave interfaces
    .s_axi_awid                         (axi_m_awid	  [S_COUNT*AXI_ID_WIDTH-1   : 0]    ),// 
    .s_axi_awaddr                       (axi_m_awaddr [S_COUNT*AXI_ADDR_WIDTH-1 : 0]   	), 
    .s_axi_awlen                        (axi_m_awlen  [S_COUNT*8-1      : 0]   			), 
    .s_axi_awsize                       (axi_m_awsize [S_COUNT*3-1		: 0]	 		), 
    .s_axi_awburst                      (axi_m_awburst[S_COUNT*2-1      : 0]   			), 
    .s_axi_awlock                       (axi_m_awlock [S_COUNT*1-1      : 0]   			), 
    .s_axi_awcache                      (axi_m_awcache[S_COUNT*4-1      : 0]   			), 
    .s_axi_awprot                       (axi_m_awprot [S_COUNT*3-1      : 0]   			), 
    .s_axi_awqos                        ({S_COUNT*4{1'b0}}                  ),
    .s_axi_awuser                       ({S_COUNT{1'b0}}                    ),
    .s_axi_awvalid                      (axi_m_awvalid[S_COUNT*1-1      : 0]   			), 
    .s_axi_awready                      (axi_m_awready[S_COUNT*1-1      : 0]   			),

    .s_axi_wdata                        (axi_m_wdata  [S_COUNT*AXI_DATA_WIDTH-1 : 0]   	), 
    .s_axi_wstrb                        (axi_m_wstrb  [S_COUNT*(AXI_DATA_WIDTH/8)-1 : 0]), 
    .s_axi_wlast                        (axi_m_wlast  [S_COUNT*1-1      : 0]   			), 
    .s_axi_wuser                        ({S_COUNT{1'b0}}                    ),
    .s_axi_wvalid                       (axi_m_wvalid [S_COUNT*1-1      : 0]   			), 
    .s_axi_wready                       (axi_m_wready [S_COUNT*1-1      : 0]   			),
    .s_axi_bid                          (axi_m_bid    [S_COUNT*AXI_ID_WIDTH-1      : 0]   			),
    .s_axi_bresp                        (axi_m_bresp  [S_COUNT*2-1      : 0]   			), 
    .s_axi_buser                        (                                  ),
    .s_axi_bvalid                       (axi_m_bvalid [S_COUNT*1-1      : 0]   			), 
    .s_axi_bready                       (axi_m_bready [S_COUNT*1-1      : 0]   			),
    .s_axi_arid                         ({S_COUNT*AXI_ID_WIDTH{1'b0}}       ),
    .s_axi_araddr                       ({S_COUNT*AXI_ADDR_WIDTH{1'b0}}     ),
    .s_axi_arlen                        ({S_COUNT*8{1'b0}}                  ),
    .s_axi_arsize                       ({S_COUNT*3{1'b0}}                  ),
    .s_axi_arburst                      ({S_COUNT*2{1'b0}}                  ),
    .s_axi_arlock                       ({S_COUNT{1'b0}}                    ),
    .s_axi_arcache                      ({S_COUNT*4{1'b0}}                  ),
    .s_axi_arprot                       ({S_COUNT*3{1'b0}}                  ),
    .s_axi_arqos                        ({S_COUNT*4{1'b0}}                  ),
    .s_axi_aruser                       ({S_COUNT{1'b0}}                    ),
    .s_axi_arvalid                      ({S_COUNT{1'b0}}                    ),
    .s_axi_arready                      (                                  ),
    .s_axi_rid                          (                                  ),
    .s_axi_rdata                        (                                  ),
    .s_axi_rresp                        (                                  ),
    .s_axi_rlast                        (                                  ),
    .s_axi_ruser                        (                                  ),
    .s_axi_rvalid                       (                                  ),
    .s_axi_rready                       ({S_COUNT{1'b0}}                    ),
//AXI master interfaces
    .m_axi_awid                         (axi0_AWID       ), //(axi_m1_awid      ),
    .m_axi_awaddr                       (axi0_AWADDR     ), //(axi_m1_awaddr   	), 
    .m_axi_awlen                        (axi0_AWLEN      ), //(axi_m1_awlen     ), 
    .m_axi_awsize                       (axi0_AWSIZE     ), //(axi_m1_awsize    ), 
    .m_axi_awburst                      (axi0_AWBURST    ), //(axi_m1_awburst   ), 
    .m_axi_awlock                       (axi0_AWLOCK     ), //(axi_m1_awlock    ), 
    .m_axi_awcache                      (axi0_AWCACHE   ),
    .m_axi_awprot                       (                ),
    .m_axi_awqos                        (                ),
    .m_axi_awregion                     (                ),
    .m_axi_awuser                       (                ),
    .m_axi_awvalid                      (axi0_AWVALID    ), //(axi_m1_awvalid   ), 
    .m_axi_awready                      (axi0_AWREADY    ), //(axi_m1_awready   ), 
    .m_axi_wdata                        (axi0_WDATA      ), //(axi_m1_wdata     ), 
    .m_axi_wstrb                        (axi0_WSTRB      ), //(axi_m1_wstrb     ), 
    .m_axi_wlast                        (axi0_WLAST      ), //(axi_m1_wlast     ), 
    .m_axi_wuser                        (                ),
    .m_axi_wvalid                       (axi0_WVALID     ), //(axi_m1_wvalid    ), 
    .m_axi_wready                       (axi0_WREADY     ), //(axi_m1_wready    ),
    .m_axi_bid                          (axi0_BID        ), //(axi_m1_bid       ),
    .m_axi_bresp                        (axi0_BRESP     ),
    .m_axi_buser                        ({1'b0}          ),
    .m_axi_bvalid                       (axi0_BVALID     ), //(axi_m1_bvalid    ), 
    .m_axi_bready                       (axi0_BREADY     )//, //(axi_m1_bready    ),
);

axi_interconnect #
(
    .S_COUNT                            (S_COUNT                            ),
    .M_COUNT                            (M_COUNT                            ),
    .DATA_WIDTH                         (AXI_DATA_WIDTH                     ),
    .ADDR_WIDTH                         (AXI_ADDR_WIDTH                     ),
    .ID_WIDTH                           (AXI_ID_WIDTH                       )
)
ur_axi_interconnect
(
    .clk                                (axi0_ACLK                            ),
    .rst                                (~pixel_data_en                         ),
//AXI slave interfaces
    .s_axi_awid                         ({S_COUNT*AXI_ID_WIDTH{1'b0}}       ),
    .s_axi_awaddr                       ({S_COUNT*AXI_ADDR_WIDTH{1'b0}}     ),
    .s_axi_awlen                        ({S_COUNT*8{1'b0}}                  ),
    .s_axi_awsize                       ({S_COUNT*3{1'b0}}                  ),
    .s_axi_awburst                      ({S_COUNT*2{1'b0}}                  ),
    .s_axi_awlock                       ({S_COUNT{1'b0}}                    ),
    .s_axi_awcache                      ({S_COUNT*4{1'b0}}                  ),
    .s_axi_awprot                       ({S_COUNT*3{1'b0}}                  ),
    .s_axi_awqos                        ({S_COUNT*4{1'b0}}                  ),
    .s_axi_awuser                       ({S_COUNT{1'b0}}                    ),
    .s_axi_awvalid                      ({S_COUNT{1'b0}}                    ),
    .s_axi_awready                      (                                  ),
    .s_axi_wdata                        ({S_COUNT*AXI_DATA_WIDTH{1'b0}}     ),
    .s_axi_wstrb                        ({S_COUNT*(AXI_DATA_WIDTH/8){1'b0}} ),
    .s_axi_wlast                        ({S_COUNT{1'b0}}                    ),
    .s_axi_wuser                        ({S_COUNT{1'b0}}                    ),
    .s_axi_wvalid                       ({S_COUNT{1'b0}}                    ),
    .s_axi_wready                       (                                  ),
    .s_axi_bid                          (                                  ),
    .s_axi_bresp                        (                                  ),
    .s_axi_buser                        (                                  ),
    .s_axi_bvalid                       (                                  ),
    .s_axi_bready                       ({S_COUNT{1'b0}}                    ),
    .s_axi_arid                         (axi_m_arid   [S_COUNT*AXI_ID_WIDTH-1   : 0]   	),
    .s_axi_araddr                       (axi_m_araddr [S_COUNT*AXI_ADDR_WIDTH-1 : 0]   	), 
    .s_axi_arlen                        (axi_m_arlen  [S_COUNT*8-1      : 0]   			), 
    .s_axi_arsize                       (axi_m_arsize [S_COUNT*3-1      : 0]   			), 
    .s_axi_arburst                      (axi_m_arburst[S_COUNT*2-1      : 0]   			), 
    .s_axi_arlock                       (axi_m_arlock [S_COUNT*1-1      : 0]   			), 
    .s_axi_arcache                      ({S_COUNT*4{1'b0}}                  ),
    .s_axi_arprot                       ({S_COUNT*3{1'b0}}                  ),
    .s_axi_arqos                        ({S_COUNT*4{1'b0}}                  ),
    .s_axi_aruser                       ({S_COUNT{1'b0}}                    ),
    .s_axi_arvalid                      (axi_m_arvalid[S_COUNT*1-1      : 0]   			), 
    .s_axi_arready                      (axi_m_arready[S_COUNT*1-1      : 0]   			),
    .s_axi_rid                          (axi_m_rid    [S_COUNT*AXI_ID_WIDTH-1      : 0]   			),
    .s_axi_rdata                        (axi_m_rdata  [S_COUNT*AXI_DATA_WIDTH-1 : 0]   	), 
    .s_axi_rresp                        (axi_m_rresp  [S_COUNT*2-1      : 0]   			), 
    .s_axi_rlast                        (axi_m_rlast  [S_COUNT*1-1      : 0]   			), 
    .s_axi_ruser                        (                                  ),
    .s_axi_rvalid                       (axi_m_rvalid [S_COUNT*1-1      : 0]   			), 
    .s_axi_rready                       (axi_m_rready [S_COUNT*1-1      : 0]   			),
//AXI master interfaces
   
    .m_axi_arid                         (axi0_ARID       ), //(axi_m1_arid      ),
    .m_axi_araddr                       (axi0_ARADDR     ), //(axi_m1_araddr    ), 
    .m_axi_arlen                        (axi0_ARLEN      ),  //(axi_m1_arlen     ), 
    .m_axi_arsize                       (axi0_ARSIZE     ), //(axi_m1_arsize    ), 
    .m_axi_arburst                      (axi0_ARBURST    ), //(axi_m1_arburst   ), 
    .m_axi_arlock                       (axi0_ARLOCK     ), //(axi_m1_arlock    ), 
    .m_axi_arcache                      (                ),
    .m_axi_arprot                       (                ),
    .m_axi_arqos                        (                ),
    .m_axi_arregion                     (                ),
    .m_axi_aruser                       (                ),
    .m_axi_arvalid                      (axi0_ARVALID    ), //(axi_m1_arvalid   ), 
    .m_axi_arready                      (axi0_ARREADY    ), //(axi_m1_arready   ),
    .m_axi_rid                          (axi0_RID        ), //(axi_m1_rid       ),
    .m_axi_rdata                        (axi0_RDATA      ), //(axi_m1_rdata     ), 
    .m_axi_rresp                        (axi0_RRESP      ), //(axi_m1_rresp     ), 
    .m_axi_rlast                        (axi0_RLAST      ), //(axi_m1_rlast     ), 
    .m_axi_ruser                        ({1'b0}          ),
    .m_axi_rvalid                       (axi0_RVALID     ), //(axi_m1_rvalid    ), 
    .m_axi_rready                       (axi0_RREADY     )  //(axi_m1_rready    )
);
`endif 


//***************************************************************************
// debayer
//***************************************************************************

reg [26:0] sw_cnt;
always @(posedge i_sysclk_div2 or negedge pixel_data_en)
begin
    if (!pixel_data_en) begin
        sw_cnt   <= 'd0;
        out_sync <= 1'b0;
    end else if (!out_sync) begin
        sw_cnt <= sw_cnt + 1'b1;
        if (sw_cnt[25])
        out_sync <= 1'b1;
    end
end



  wire        rgb_vs;
  wire        rgb_hs;
  wire        rgb_de;
  wire        rgb_valid;
  wire [47:0] rgb_datax2;
  wire        rgb1_vs;
  wire        rgb1_hs;
  wire        rgb1_de;
  wire        rgb1_valid;
  wire [47:0] rgb1_datax2;
  
  debayer_top_2to1 debayer_top
  (
      .in_pclk		  (i_sysclk_div2),//(i_mipi_rx_pclk ),
      .in_rstn		  (pixel_data_en	),
      
      .raw_vs_i		  (ch0_vs		  ),//(ch1_vs	     ),//
      .raw_hs_i		  (ch0_hs		  ),//(ch1_hs	     ),//	 
      .raw_de_i		  (ch0_de		  ),//(ch1_de	     ),//	
      .raw_valid_i	  (ch0_de	      ),//(ch1_de	     ),//	
      .raw_datax4_i	  (ch0_bayer_2pix ),//
      .r_gain         (3'd7           ),
      .g_gain         (3'd4           ),
      .b_gain         (3'd4           ),
      
      .rgb_vs_o		  (rgb_vs         ),
      .rgb_hs_o		  (rgb_hs         ),
      .rgb_de_o		  (rgb_de         ),
      .rgb_valid_o	  (rgb_valid      ),
      .rgb_bout       (               ),
      .rgb_gout       (               ),
      .rgb_rout       (               ),
      .rgb_datax2_o   (rgb_datax2     )//b,g,r,b,g,r
  );
  


  debayer_top_2to1 debayer_top1
  (
      .in_pclk		  (i_sysclk_div2),//(i_mipi_rx_pclk ),
      .in_rstn		  (pixel_data_en	),
      
      .raw_vs_i		  (ch1_vs		  ),//(ch1_vs	     ),//
      .raw_hs_i		  (ch1_hs		  ),//(ch1_hs	     ),//	 
      .raw_de_i		  (ch1_de		  ),//(ch1_de	     ),//	
      .raw_valid_i	  (ch1_de	      ),//(ch1_de	     ),//	
      .raw_datax4_i	  (ch1_bayer_2pix ),//
      .r_gain         (3'd7           ),
      .g_gain         (3'd4           ),
      .b_gain         (3'd4           ),
      
      .rgb_vs_o		  (rgb1_vs         ),
      .rgb_hs_o		  (rgb1_hs         ),
      .rgb_de_o		  (rgb1_de         ),
      .rgb_valid_o	  (rgb1_valid      ),
      .rgb_bout       (                ),
      .rgb_gout       (                ),
      .rgb_rout       (                ),
      .rgb_datax2_o   (rgb1_datax2     )//b,g,r,b,g,r
  );

//============================================================================= 
//mipi dsi
//=============================================================================
wire            wb0_hs_out;
wire            wb0_vs_out;
wire            wb0_de_out;
wire [47:0]     wb0_data_out;

wire            wb1_hs_out;
wire            wb1_vs_out;
wire            wb1_de_out;
wire [47:0]     wb1_data_out;
localparam HDMI_BYPASS_WHITE_BALANCE = 1'b1;
wire [47:0] rgb0_data_rgb = {rgb_datax2[31:24],  rgb_datax2[39:32],  rgb_datax2[47:40],
                             rgb_datax2[7:0],    rgb_datax2[15:8],   rgb_datax2[23:16]};
wire [47:0] rgb1_data_rgb = {rgb1_datax2[31:24], rgb1_datax2[39:32], rgb1_datax2[47:40],
                             rgb1_datax2[7:0],   rgb1_datax2[15:8],  rgb1_datax2[23:16]};
wire            hdmi0_hs_out   = HDMI_BYPASS_WHITE_BALANCE ? rgb_hs     : wb0_hs_out;
wire            hdmi0_vs_out   = HDMI_BYPASS_WHITE_BALANCE ? rgb_vs     : wb0_vs_out;
wire            hdmi0_de_out   = HDMI_BYPASS_WHITE_BALANCE ? rgb_de     : wb0_de_out;
wire [47:0]     hdmi0_data_out = HDMI_BYPASS_WHITE_BALANCE ? rgb0_data_rgb : wb0_data_out;
wire            hdmi1_hs_out   = HDMI_BYPASS_WHITE_BALANCE ? rgb1_hs     : wb1_hs_out;
wire            hdmi1_vs_out   = HDMI_BYPASS_WHITE_BALANCE ? rgb1_vs     : wb1_vs_out;
wire            hdmi1_de_out   = HDMI_BYPASS_WHITE_BALANCE ? rgb1_de     : wb1_de_out;
wire [47:0]     hdmi1_data_out = HDMI_BYPASS_WHITE_BALANCE ? rgb1_data_rgb : wb1_data_out;
white_balance u0_white_balance (
    .clk            (i_sysclk_div2),
    .rst_n          (pixel_data_en      ),
    .hs_in          (rgb_hs         ),
    .vs_in          (rgb_vs         ),
    .de_in          (rgb_de         ),
    .data_in        (rgb0_data_rgb  ),
    .hs_out         (wb0_hs_out      ),
    .vs_out         (wb0_vs_out      ),
    .de_out         (wb0_de_out      ),
    .data_out       (wb0_data_out    )
  );

white_balance u1_white_balance (
    .clk            (i_sysclk_div2),
    .rst_n          (pixel_data_en   ),
    .hs_in          (rgb1_hs         ),
    .vs_in          (rgb1_vs         ),
    .de_in          (rgb1_de         ),
    .data_in        (rgb1_data_rgb   ),
    .hs_out         (wb1_hs_out      ),
    .vs_out         (wb1_vs_out      ),
    .de_out         (wb1_de_out      ),
    .data_out       (wb1_data_out    )
  );

//=============================================================================
//DSI / MIPI TX port tie-off (DSI lane not used; P0/P1 LCD off)
// To restore DSI: re-instantiate inst_tx_byteclk_rst + color_bar_rgb_inst
// + dsi_tx_top_inst1 and delete this block.
//=============================================================================
assign P0_lcd_power_en = 1'b0;
assign P0_lcd_rstp     = 1'b0;
assign P1_lcd_power_en = 1'b0;
assign P1_o_lcd_rstn   = 1'b0;



// ch0 MIPI TX lane tie-off (safe Hi-Z)
assign mipi_tx_ck0_HS_OE    = 1'b0;
assign mipi_tx_ck0_HS_OUT   = 8'd0;
assign mipi_tx_ck0_LP_N_OE  = 1'b0;
assign mipi_tx_ck0_LP_N_OUT = 1'b0;
assign mipi_tx_ck0_LP_P_OE  = 1'b0;
assign mipi_tx_ck0_LP_P_OUT = 1'b0;
assign mipi_tx_ck0_RST      = 1'b1;
assign mipi_tx_dp00_HS_OE    = 1'b0;
assign mipi_tx_dp00_HS_OUT   = 8'd0;
assign mipi_tx_dp00_LP_N_OE  = 1'b0;
assign mipi_tx_dp00_LP_N_OUT = 1'b0;
assign mipi_tx_dp00_LP_P_OE  = 1'b0;
assign mipi_tx_dp00_LP_P_OUT = 1'b0;
assign mipi_tx_dp00_RST      = 1'b1;
assign mipi_tx_dp01_HS_OE    = 1'b0;
assign mipi_tx_dp01_HS_OUT   = 8'd0;
assign mipi_tx_dp01_LP_N_OE  = 1'b0;
assign mipi_tx_dp01_LP_N_OUT = 1'b0;
assign mipi_tx_dp01_LP_P_OE  = 1'b0;
assign mipi_tx_dp01_LP_P_OUT = 1'b0;
assign mipi_tx_dp01_RST      = 1'b1;
assign mipi_tx_dp02_HS_OE    = 1'b0;
assign mipi_tx_dp02_HS_OUT   = 8'd0;
assign mipi_tx_dp02_LP_N_OE  = 1'b0;
assign mipi_tx_dp02_LP_N_OUT = 1'b0;
assign mipi_tx_dp02_LP_P_OE  = 1'b0;
assign mipi_tx_dp02_LP_P_OUT = 1'b0;
assign mipi_tx_dp02_RST      = 1'b1;
assign mipi_tx_dp03_HS_OE    = 1'b0;
assign mipi_tx_dp03_HS_OUT   = 8'd0;
assign mipi_tx_dp03_LP_N_OE  = 1'b0;
assign mipi_tx_dp03_LP_N_OUT = 1'b0;
assign mipi_tx_dp03_LP_P_OE  = 1'b0;
assign mipi_tx_dp03_LP_P_OUT = 1'b0;
assign mipi_tx_dp03_RST      = 1'b1;
// ch1 MIPI TX lane tie-off (was DSI)
assign mipi_tx_ck1_HS_OE    = 1'b0;
assign mipi_tx_ck1_HS_OUT   = 8'd0;
assign mipi_tx_ck1_LP_N_OE  = 1'b0;
assign mipi_tx_ck1_LP_N_OUT = 1'b0;
assign mipi_tx_ck1_LP_P_OE  = 1'b0;
assign mipi_tx_ck1_LP_P_OUT = 1'b0;
assign mipi_tx_ck1_RST      = 1'b1;
assign mipi_tx_dp10_HS_OE    = 1'b0;
assign mipi_tx_dp10_HS_OUT   = 8'd0;
assign mipi_tx_dp10_LP_N_OE  = 1'b0;
assign mipi_tx_dp10_LP_N_OUT = 1'b0;
assign mipi_tx_dp10_LP_P_OE  = 1'b0;
assign mipi_tx_dp10_LP_P_OUT = 1'b0;
assign mipi_tx_dp10_RST      = 1'b1;
assign mipi_tx_dp11_HS_OE    = 1'b0;
assign mipi_tx_dp11_HS_OUT   = 8'd0;
assign mipi_tx_dp11_LP_N_OE  = 1'b0;
assign mipi_tx_dp11_LP_N_OUT = 1'b0;
assign mipi_tx_dp11_LP_P_OE  = 1'b0;
assign mipi_tx_dp11_LP_P_OUT = 1'b0;
assign mipi_tx_dp11_RST      = 1'b1;
assign mipi_tx_dp12_HS_OE    = 1'b0;
assign mipi_tx_dp12_HS_OUT   = 8'd0;
assign mipi_tx_dp12_LP_N_OE  = 1'b0;
assign mipi_tx_dp12_LP_N_OUT = 1'b0;
assign mipi_tx_dp12_LP_P_OE  = 1'b0;
assign mipi_tx_dp12_LP_P_OUT = 1'b0;
assign mipi_tx_dp12_RST      = 1'b1;
assign mipi_tx_dp13_HS_OE    = 1'b0;
assign mipi_tx_dp13_HS_OUT   = 8'd0;
assign mipi_tx_dp13_LP_N_OE  = 1'b0;
assign mipi_tx_dp13_LP_N_OUT = 1'b0;
assign mipi_tx_dp13_LP_P_OE  = 1'b0;
assign mipi_tx_dp13_LP_P_OUT = 1'b0;
assign mipi_tx_dp13_RST      = 1'b1;


//==================================================================================
//hdmi_top - SW4 (touch button i_sw[1]) toggles camera channel.
// RTL led[2] is the channel indicator; board silk observed as LED20.
//==================================================================================
  `ifdef  HDMI_OUT_EN
  reg rgb_vs_r;
  reg rgb_hs_r;
  reg rgb_de_r;
  reg [23:0] rgb_datax1;
  reg [1:0] hdmi_video_ready_sync;
  wire hdmi_video_ready = hdmi_video_ready_sync[1];
  wire selected_frame_ready;
  wire selected_fifo_underflow;
  reg [2:0] selected_frame_ready_cdc;
  reg [1:0] selected_fifo_underflow_cdc;
  reg [1:0] ch0_csi_format_ok_cdc;
  reg [1:0] ch1_csi_format_ok_cdc;
  wire hdmi_input_stable;
  wire selected_frame_ok_hdmi;
  wire selected_csi_format_ok;
  wire selected_bridge_active;
  wire selected_bridge_underflow;

  // SW4 touch-button debounce + toggle (active-low button, default high)
  reg  [1:0] sw4_sync;
  reg [19:0] sw4_cnt;
  reg        sw4_stable;
  reg        sw4_stable_r;
  reg        channel_sel;
  reg        channel_sel_toggle;
  wire       selected_hdmi_vs;
  wire       selected_hdmi_hs;
  wire       selected_hdmi_de;
  wire [23:0] selected_hdmi_data;
  wire       hdmi0_bridge_vs;
  wire       hdmi0_bridge_hs;
  wire       hdmi0_bridge_de;
  wire [23:0] hdmi0_bridge_data;
  wire       hdmi0_bridge_active;
  wire       hdmi0_bridge_underflow;
  wire       hdmi1_bridge_vs;
  wire       hdmi1_bridge_hs;
  wire       hdmi1_bridge_de;
  wire [23:0] hdmi1_bridge_data;
  wire       hdmi1_bridge_active;
  wire       hdmi1_bridge_underflow;

  video_2pix_to_1pix_cdc #(
      .FIFO_DEPTH(1024),
      .START_LEVEL(16)
  ) u_hdmi0_video_cdc (
      .wr_clk(i_sysclk_div2),
      .rd_clk(hdmi_tx_slow_clk),
      .rst_n(pixel_data_en),
      .i_vs(hdmi0_vs_out),
      .i_hs(hdmi0_hs_out),
      .i_de(hdmi0_de_out),
      .i_data(hdmi0_data_out),
      .o_vs(hdmi0_bridge_vs),
      .o_hs(hdmi0_bridge_hs),
      .o_de(hdmi0_bridge_de),
      .o_data(hdmi0_bridge_data),
      .o_active(hdmi0_bridge_active),
      .o_underflow(hdmi0_bridge_underflow)
  );

  video_2pix_to_1pix_cdc #(
      .FIFO_DEPTH(1024),
      .START_LEVEL(16)
  ) u_hdmi1_video_cdc (
      .wr_clk(i_sysclk_div2),
      .rd_clk(hdmi_tx_slow_clk),
      .rst_n(pixel_data_en),
      .i_vs(hdmi1_vs_out),
      .i_hs(hdmi1_hs_out),
      .i_de(hdmi1_de_out),
      .i_data(hdmi1_data_out),
      .o_vs(hdmi1_bridge_vs),
      .o_hs(hdmi1_bridge_hs),
      .o_de(hdmi1_bridge_de),
      .o_data(hdmi1_bridge_data),
      .o_active(hdmi1_bridge_active),
      .o_underflow(hdmi1_bridge_underflow)
  );

  assign selected_hdmi_vs   = channel_sel ? hdmi1_bridge_vs   : hdmi0_bridge_vs;
  assign selected_hdmi_hs   = channel_sel ? hdmi1_bridge_hs   : hdmi0_bridge_hs;
  assign selected_hdmi_de   = channel_sel ? hdmi1_bridge_de   : hdmi0_bridge_de;
  assign selected_hdmi_data = channel_sel ? hdmi1_bridge_data : hdmi0_bridge_data;
  assign selected_bridge_active = channel_sel ? hdmi1_bridge_active : hdmi0_bridge_active;
  assign selected_bridge_underflow = channel_sel ? hdmi1_bridge_underflow : hdmi0_bridge_underflow;

  always @(posedge hdmi_tx_slow_clk or negedge sys_rst_n) begin
      if (!sys_rst_n) begin
          sw4_sync     <= 2'b11;
          sw4_cnt      <= 20'd0;
          sw4_stable   <= 1'b1;
          sw4_stable_r <= 1'b1;
          channel_sel  <= 1'b0;
          channel_sel_toggle <= 1'b0;
      end else begin
          channel_sel_toggle <= 1'b0;
          sw4_sync <= {sw4_sync[0], i_sw[1]};
          // ~7.5ms debounce @ 140MHz hdmi_tx_slow_clk
          if (sw4_sync[1] != sw4_stable) begin
              sw4_cnt    <= 20'd0;
              sw4_stable <= sw4_sync[1];
          end else if (!sw4_cnt[19]) begin
              sw4_cnt <= sw4_cnt + 1'b1;
          end
          // falling-edge toggle on stable low
          sw4_stable_r <= sw4_stable;
          if (sw4_stable_r && !sw4_stable) begin
              channel_sel <= ~channel_sel;
              channel_sel_toggle <= 1'b1;
          end
      end
  end

  always @(posedge hdmi_tx_slow_clk or negedge sys_rst_n) begin
      if (!sys_rst_n) begin
          rgb_vs_r   <= 1'b0;
          rgb_hs_r   <= 1'b0;
          rgb_de_r   <= 1'b0;
          rgb_datax1 <= 24'd0;
          hdmi_video_ready_sync <= 2'b00;
          selected_frame_ready_cdc <= 3'b000;
          selected_fifo_underflow_cdc <= 2'b00;
          ch0_csi_format_ok_cdc <= 2'b00;
          ch1_csi_format_ok_cdc <= 2'b00;
      end else begin
          selected_frame_ready_cdc <= {selected_frame_ready_cdc[1:0], selected_frame_ready};
          selected_fifo_underflow_cdc <= {selected_fifo_underflow_cdc[0], selected_fifo_underflow};
          ch0_csi_format_ok_cdc <= {ch0_csi_format_ok_cdc[0], ch0_csi_format_ok};
          ch1_csi_format_ok_cdc <= {ch1_csi_format_ok_cdc[0], ch1_csi_format_ok};

          hdmi_video_ready_sync <= channel_sel_toggle ? 2'b00 :
              {hdmi_video_ready_sync[0], selected_frame_ok_hdmi & selected_bridge_active & ~selected_bridge_underflow};

          rgb_vs_r   <= selected_hdmi_vs;
          rgb_hs_r   <= selected_hdmi_hs;
          rgb_de_r   <= selected_hdmi_de;
          rgb_datax1 <= selected_hdmi_de ? selected_hdmi_data : 24'd0;
      end
  end

assign led[2] = channel_sel;       // RTL led[2], observed board LED20: ch1 indicator
assign selected_frame_ready = channel_sel ? ch1_frame_ready : ch0_frame_ready;
assign selected_fifo_underflow = channel_sel ? ch1_fifo_underflow : ch0_fifo_underflow;
assign selected_csi_format_ok = channel_sel ? ch1_csi_format_ok_cdc[1] : ch0_csi_format_ok_cdc[1];
assign selected_frame_ok_hdmi = selected_frame_ready_cdc[2] & ~selected_fifo_underflow_cdc[1];
assign led[3] = hdmi_input_stable & selected_csi_format_ok;  // selected CSI is RAW10/4ppc and HDMI timing is accepted
  hdmi_top #(
    .USE_INPUT_STABLE_GATE(1'b1)
  ) hdmi_top_inst (
    .hdmi_tx_locked(1'b1),
    .i_video_ready(hdmi_video_ready),
    .i_hs(rgb_hs_r),
    .i_vs(rgb_vs_r),
    .i_de(rgb_de_r),
    .i_rdata(rgb_datax1[23:16]),
    .i_gdata(rgb_datax1[15:8]),
    .i_bdata(rgb_datax1[7:0]), 
    .hdmi_tx_slow_clk(hdmi_tx_slow_clk),
    .tmds_data0_o(tmds_data0_TX_DATA),
    .tmds_data1_o(tmds_data1_TX_DATA),
    .tmds_data2_o(tmds_data2_TX_DATA),
    .tmds_clk_o(tmds_clk_TX_DATA),
    .tmds_data0_TX_OE(tmds_data0_TX_OE),
    .tmds_data1_TX_OE(tmds_data1_TX_OE),
    .tmds_data2_TX_OE(tmds_data2_TX_OE),
    .tmds_clk_TX_OE(tmds_clk_TX_OE),
    .tmds_data0_TX_RST(tmds_data0_TX_RST),
    .tmds_data1_TX_RST(tmds_data1_TX_RST),
    .tmds_data2_TX_RST(tmds_data2_TX_RST),
    .tmds_clk_TX_RST(tmds_clk_TX_RST),
    .o_input_stable(hdmi_input_stable)
  );
`endif 
endmodule
