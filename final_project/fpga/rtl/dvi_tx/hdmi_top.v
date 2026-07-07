//=================================================================
//
//  Copyright (C) 2022 Persion All rights reserved.
//  文件名称：top.v
//  创 建 者：Ramsey Wang
//  创建日期：2022.04.01
//  描    述：hdmi_top
//
//=================================================================


module hdmi_top
(


    ////////////////////////    CLOCK & PLL     ////////////////////////
	input hdmi_tx_locked,
    input hdmi_tx_slow_clk,
  input         i_video_ready,
  input 		i_hs,
  input 		i_vs,
  input 		i_de,
  input [7:0] i_rdata,
  input [7:0] i_gdata,
  input [7:0] i_bdata,

    output [9:0]tmds_data0_o ,
    output [9:0]tmds_data1_o ,
    output [9:0]tmds_data2_o ,
    output [9:0]tmds_clk_o   ,

    output tmds_data0_TX_OE,
    output tmds_data1_TX_OE,
    output tmds_data2_TX_OE,
    output tmds_clk_TX_OE,
	output tmds_data0_TX_RST,
	output tmds_data1_TX_RST,
	output tmds_data2_TX_RST,
	output tmds_clk_TX_RST,
  output        o_input_stable

);

//=====================================================================================
//localpram
//=====================================================================================
    // Keep fallback active until the selected camera path has stable timing.
    parameter   USE_INPUT_STABLE_GATE = 1'b1;
    parameter   INPUT_STABLE_FRAME_COUNT = 4'd4;
	parameter	MAX_HRES		= 12'd960;   // was 1920; halved to match reduced framebuffer H_VALID
	parameter	MAX_VRES		= 12'd1080;
	parameter	HSP				= 8'd4;
	parameter	HBP				= 8'd88;
	parameter	HFP				= 8'd120;
	parameter	VSP				= 8'd2;
	parameter	VBP				= 8'd20;
	parameter	VFP				= 8'd20;


  
//=====================================================================================
//hdmi demo
//=====================================================================================
wire                            video_hs;
wire                            video_vs;
wire                            video_de;
wire[7:0]                       video_r;
wire[7:0]                       video_g;
wire[7:0]                       video_b;
wire   							sys_rst_n;
wire [23:0]                      i_cfg_vid;
wire [15:0]                      h_cnt;
wire [15:0]                      v_cnt;
assign i_cfg_vid = 24'd0;
//=====================================================================================
//rest-n
//=====================================================================================

 reset
	#(
		.IN_RST_ACTIVE	("LOW"),
		.OUT_RST_ACTIVE	("LOW"),
		.CYCLE			(3)
	)
	inst_rx_byteclk_rst
	(
		.i_arst	(hdmi_tx_locked),
		.i_clk	(hdmi_tx_slow_clk),
		.o_srst	(sys_rst_n)
	);

//=====================================================================================
//hdmi demo
//=====================================================================================
wire [9:0] tmds_data0;
wire [9:0] tmds_data1;
wire [9:0] tmds_data2;
wire [9:0] tmds_clk ;


assign tmds_data0_TX_OE = 1'b1;
assign tmds_data1_TX_OE = 1'b1;
assign tmds_data2_TX_OE = 1'b1;
assign tmds_clk_TX_OE   = 1'b1;

assign tmds_data0_TX_RST = 1'b0;
assign tmds_data1_TX_RST = 1'b0;
assign tmds_data2_TX_RST = 1'b0;
assign tmds_clk_TX_RST   = 1'b0;

wire i_stable;
wire input_neg_vs_sync;
wire [13:0] input_h_act;
wire [13:0] input_v_act;
wire input_h_active_error;
wire input_v_active_error;
wire input_v_total_error;
wire input_h_total_error;
wire input_h_sync_error;
wire video_path_ready = sys_rst_n & i_video_ready;
wire input_timing_size_ok = (input_h_act == MAX_HRES) & (input_v_act == MAX_VRES);
wire input_stable_qualified = i_stable &
                              input_timing_size_ok &
                              ~input_h_active_error &
                              ~input_v_active_error &
                              ~input_v_total_error &
                              ~input_h_total_error &
                              ~input_h_sync_error;
reg  input_vs_d = 1'b0;
wire input_pos_vs = ~input_vs_d & i_vs;
wire input_neg_vs = input_vs_d & ~i_vs;
wire input_frame_end = input_neg_vs_sync ? input_neg_vs : input_pos_vs;
reg  [3:0] input_good_frame_cnt = 4'd0;
reg  input_stable_seen = 1'b0;
wire use_input_video = video_path_ready & (!USE_INPUT_STABLE_GATE || input_stable_seen);
assign o_input_stable = video_path_ready & input_stable_seen;

always @(posedge hdmi_tx_slow_clk or negedge sys_rst_n) begin
    if (!sys_rst_n) begin
        input_vs_d <= 1'b0;
        input_good_frame_cnt <= 4'd0;
        input_stable_seen <= 1'b0;
    end else begin
        input_vs_d <= i_vs;
        if (!video_path_ready || !input_stable_qualified) begin
            input_good_frame_cnt <= 4'd0;
            input_stable_seen <= 1'b0;
        end else if (input_frame_end) begin
            if (input_good_frame_cnt < INPUT_STABLE_FRAME_COUNT)
                input_good_frame_cnt <= input_good_frame_cnt + 1'b1;
            if (input_good_frame_cnt >= (INPUT_STABLE_FRAME_COUNT - 1'b1))
                input_stable_seen <= 1'b1;
        end
    end
end

vid_info_det vid_info_det_inst (
    .clk(hdmi_tx_slow_clk),
    .rst_n(video_path_ready),
    .i_vs(i_vs),
    .i_hs(i_hs),
    .i_de(i_de),
    .frame_cnt_o(),
    .frame_stable(i_stable),
    .neg_vs_sync(input_neg_vs_sync),
    .neg_hs_sync(),
    .o_h_act(input_h_act),
    .h_active_error(input_h_active_error),
    .o_v_act(input_v_act),
    .v_active_error(input_v_active_error),
    .o_v_total(),
    .v_total_error(input_v_total_error),
    .o_h_total(),
    .h_total_error(input_h_total_error),
    .h_sync_error(input_h_sync_error)
  );


		color_bar_rgb # (
    .DYN_EN(1'b0),
    .HS_POLORY(1'b1),
    .VS_POLORY(1'b1),
    .SYMBOL_WIDTH(8),
    .SYMBOL_NUM(3),
    .PAR_PIXEL_NUM(1),
    .HFP(HFP),
    .HST(HSP),
    .HACT(MAX_HRES),
    .HBP(HBP),
    .VFP(VFP),
    .VST(VSP),
    .VACT(MAX_VRES),
    .VBP(VBP),
    .TEST_MODE(2'd2)
  )
  color_bar_rgb_inst (
    .clk(hdmi_tx_slow_clk),
    .rst_n(sys_rst_n),
    .i_cfg_vid(i_cfg_vid),
    .h_cnt(h_cnt),
    .v_cnt(v_cnt),
    .hs(video_hs),
    .vs(video_vs),
    .de(video_de),
    .o_vid_data({video_r,video_g,video_b})
  );

dvi_encoder dvi_encoder_m0
(
	.pixelclk      (hdmi_tx_slow_clk        ),// system clock
	.rstin         (~sys_rst_n         ),// reset
	//hdmi tx
	.blue_din      (use_input_video ? i_bdata : video_b   ),//(video_b ),    //
	.green_din     (use_input_video ? i_gdata : video_g   ),//(video_g ),    //
	.red_din       (use_input_video ? i_rdata : video_r   ),//(video_r ),    //
	.hsync         (use_input_video ? i_hs    : video_hs  ),// (video_hs ),    //
	.vsync         (use_input_video ? i_vs    : video_vs  ),// (video_vs ),    //
	.de            (use_input_video ? i_de    : video_de  ),// (video_de ),    //
	
    .tmds_data0    (tmds_data0),
    .tmds_data1    (tmds_data1),
    .tmds_data2    (tmds_data2),
    .tmds_clk      (tmds_clk  )
);
   
assign tmds_clk_o =  tmds_clk;
assign tmds_data0_o = tmds_data0;
assign tmds_data1_o = tmds_data1;
assign tmds_data2_o = tmds_data2;


  
 
endmodule
