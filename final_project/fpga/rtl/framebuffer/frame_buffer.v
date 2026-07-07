
`timescale 1ps/1ps
module frame_buffer #(
parameter AXI_DATA_WIDTH	= 512,
parameter I_VID_WIDTH = 32,
parameter O_VID_WIDTH = 32,

parameter MAX_VID_WIDTH		=	1920 ,//video width 
parameter MAX_VID_HIGHT		=	1080 ,//wideo height
parameter AXI_ADDR_WIDTH 	= 33,
parameter  VID_MAX_WIDTH = 1920,
parameter VID_MAX_HIGHT = 1080,
parameter	WR_FIFO_DEPTH		= 1024,    
parameter	RD_FIFO_DEPTH 	= 1024,
parameter BURST_LEN       = 15,
parameter FB_NUM			= 	3,//2 buffer ,3 buffer   
parameter AXI_STRB_WIDTH 	= AXI_DATA_WIDTH/8,
parameter AXSIZE_WTH 			= $clog2(AXI_DATA_WIDTH/8),
parameter AXI_BURST_WIDTH = $clog2(AXI_DATA_WIDTH/8),
parameter WR_USEDW_WITH 	= $clog2(WR_FIFO_DEPTH),
parameter RD_USEDW_WITH 	= $clog2(RD_FIFO_DEPTH),
parameter START_ADDR = 33'd0

) (

input 												axi_clk,
input 												rst_n,
input	wire								i_clk	,
input	wire								i_vs, //active hgih
input	wire								i_hs,
input	wire								i_de, //active high
input	wire	[I_VID_WIDTH-1:0] 			vin ,
input	wire								o_clk	,
output	wire								o_hs , //active high
output	wire								o_vs , //active hgih
output	wire								o_de , //active high
output	wire	[O_VID_WIDTH-1:0] 			vout ,
output  wire                                frame_ready,
output  wire                                fifo_rd_underflow,
output  wire                                dbg_fifo_rd_period,
output  wire                                dbg_ddr_rd_seen,
output  wire                                dbg_ddr_read_gap,
output  wire                                dbg_frame_stable,
output  wire                                dbg_wr_frame_done,
output  wire                                dbg_rd_frame_available,
output  wire                                dbg_frame_start_seen,
output  wire                                dbg_wr_fifo_wren_seen,
output  wire                                dbg_wr_start_seen,
output  wire                                dbg_awvalid_seen,
output  wire                                dbg_wr_frame_done_seen,
output  wire                                dbg_rd_start_seen,
output  wire                                dbg_arvalid_seen,
input  wire               out_sync,

input	wire 	[12:0]						H_FRONT_PORCH 	,
input	wire 	[12:0]						H_SYNC 			,
input	wire 	[12:0]						H_VALID 		,
input	wire 	[12:0]						H_BACK_PORCH 	,
input	wire 	[12:0]						V_FRONT_PORCH 	,
input	wire 	[12:0]						V_SYNC 			,
input	wire 	[12:0]						V_VALID 		,
input	wire 	[12:0]						V_BACK_PORCH 	,


		
output 	[5:0] 								awid,
output  [AXI_ADDR_WIDTH-1:0] 	awaddr,
output  [AXI_BURST_WIDTH-1:0] awlen,
output  [2:0] 								awsize,
output  [1:0] 								awburst,
output  [3:0] 								awcache,
output  									awlock,
output  									awvalid,
output  									awcobuf,
output  									awapcmd,
output  									awallstrb,
output  									awqos,
input 										awready,

output [5:0] 									arid,
output  [AXI_ADDR_WIDTH-1:0] 	    araddr,
output  [AXI_BURST_WIDTH-1:0] 		arlen,
output  [2:0] 								    arsize,
output  [1:0] 								    arburst,
output  									    arlock,
output  									    arvalid,
output  									    arapcmd,
output 										    arqos,
input 										    arready,

output  [AXI_DATA_WIDTH-1:0] 	            wdata,  
output  [AXI_STRB_WIDTH-1:0] 	            wstrb,
output  									wlast,
output  									wvalid,
input 										wready,

input [5:0] 							    rid,
input [AXI_DATA_WIDTH-1:0] 		            rdata,
input 										rlast,
input 										rvalid,
output  									rready,
input [1:0] 							    rresp,

input [5:0] 							    bid,
input 										bvalid,
output  									bready
);
//=============================================================  
//wire define
//=============================================================
wire												wr_fifo_wren		;
wire												wr_fifo_wrfull	;
wire [AXI_DATA_WIDTH-1:0]		                    wr_fifo_wrdata	; 
wire                                                wr_fifo_rst_p;
wire [WR_USEDW_WITH:0]                              wr_fifo_wrusedw;


wire [AXI_DATA_WIDTH-1:0] 	rd_fifo_rddata	;
wire 												rd_fifo_rdempty	;
wire                                                rd_fifo_wr_full;
wire [AXI_DATA_WIDTH-1:0]                           ddr_rd_data;
wire                                                ddr_rd_valid;
//=============================================================
//reg define 
//=============================================================   
wire	[31:0]									    ddr_frame_len ;
wire                                                frame_start;
wire                                                frame_stable;
wire                                                wr_frame_done;
wire                                                wr_start;
reg     [12:0]                                      h_front_porch  = 13'd100; 
reg     [12:0]                                      h_sync 	 		= 13'd100; 
reg     [12:0]                                      h_valid 	 	 	= 13'd100; 
reg     [12:0]                                      h_back_porch = 13'd100; 
reg     [12:0]                                      v_front_porch  = 13'd100; 
reg     [12:0]                                      v_sync 	 		= 13'd100; 
reg     [12:0]                                      v_valid 	 	  = 13'd100; 
reg     [12:0]                                      v_back_porch = 13'd100; 
wire                                                fifo_rd_period;
wire                                                 rd_start ;
wire                                                frame_en ;
wire                                                axi_clk_rst_n;
wire                                                o_clk_rst_n;
wire                                                tx_fifo_rd_underflow;
//=============================================================  
//RTL                                                     
//=============================================================  
rst_n_piple # (
    .DLY(3)
  )
  rst_n_piple_inst (
    .clk(axi_clk),
    .rst_n_i(rst_n),
    .rst_n_o(axi_clk_rst_n)
  );
  rst_n_piple # (
    .DLY(3)
  )
  rst_n_o_clk (
    .clk(o_clk),
    .rst_n_i(rst_n),
    .rst_n_o(o_clk_rst_n)
  );

vid_rx_align_v1 #(
	.I_VID_WIDTH   (I_VID_WIDTH		),
	.AXI_DDR_WIDTH (AXI_DATA_WIDTH )
)u_vid_rx_align(
/*i*/.clk			          (i_clk  		    ),  
/*i*/.rst_n			        (rst_n	        ),
/*i*/.i_vs			        (i_vs			      ), //active hgih
/*i*/.i_hs			        (i_hs			      ),
/*i*/.i_de			        (i_de			      ), //active high
/*i*/.vin			          (vin			      ),
/*o*/.fifo_wr_en	      (wr_fifo_wren	  ),
/*o*/.fifo_wr_data	    (wr_fifo_wrdata	),
/*o*/.frame_start	      (frame_start	  ),
/*O*/.fifo_rst_p		    (wr_fifo_rst_p	),
/*o*/.frame_pix_num   	(	 	            ),
/*O*/.frame_stable      (frame_stable   ),
/*O*/.ddr_frame_len     (ddr_frame_len  )
);



ddr_buffer #(
.AXI_DATA_WIDTH ( AXI_DATA_WIDTH	),
.AXI_ADDR_WIDTH ( AXI_ADDR_WIDTH	),
.MAX_VID_WIDTH 	( MAX_VID_WIDTH   ),
.MAX_VID_HIGHT 	( MAX_VID_HIGHT   ),
.WR_FIFO_DEPTH	( WR_FIFO_DEPTH		),    
.RD_FIFO_DEPTH 	( RD_FIFO_DEPTH 	),
.START_ADDR     ( START_ADDR      ),
.BURST_LEN      ( BURST_LEN       ),
.FB_NUM					( FB_NUM          ),
.I_VID_WIDTH    ( I_VID_WIDTH     )
)u_ddr_buffer(
    .axi_clk		    (axi_clk	),
    .axi_rst_n				(axi_clk_rst_n 	    ),
    
//write interface 
/*i*/.wr_start	      (wr_start ),
     .wr_busrt_len    (ddr_frame_len  ),
/*i*/.wr_fifo_wrclk	  (i_clk			    ),
/*i*/.wr_fifo_rst_p   (wr_fifo_rst_p  ),
/*i*/.wr_fifo_wren	  (wr_fifo_wren	  ),
/*o*/.wr_fifo_wrfull  (wr_fifo_wrfull ),
/*i*/.wr_fifo_wrdata  (wr_fifo_wrdata ), 
/*o*/.wr_fifo_wrusedw (wr_fifo_wrusedw), 
//read interface
/*i*/.rd_start	      ( rd_start      ),
    .rd_burst_len     (ddr_frame_len  ),
    .ddr_rd_valid     (ddr_rd_valid   ),
    .ddr_rd_data      (ddr_rd_data    ),
    .wr_frame_done    (wr_frame_done  ),
    .rd_fifo_wr_full  (rd_fifo_wr_full),

    .awid			  ( awid			),
    .awaddr			( awaddr		),
    .awlen			( awlen			),
    .awsize			( awsize		),
    .awburst		( awburst		),
    .awcache		( awcache		),
    .awlock			( awlock		),
    .awvalid		( awvalid		),
    .awcobuf		( awcobuf		),
    .awapcmd		( awapcmd		),
    .awallstrb	( awallstrb	),
    .awready		( awready		),
    .awqos			( awqos			),
    .arid			  ( arid			),
    .araddr			( araddr		),
    .arlen			( arlen			),
    .arsize			( arsize		),
    .arburst		( arburst		),
    .arlock			( arlock		),
    .arvalid		( arvalid		),
    .arapcmd		( arapcmd		),
    .arready		( arready		),
    .arqos			( arqos			),
    .wdata			( wdata			),
    .wstrb			( wstrb			),
    .wlast			( wlast			),
    .wvalid			( wvalid		),
    .wready			( wready		),
    .rid			  ( rid			  ),
    .rdata			( rdata			),
    .rlast			( rlast			),
    .rvalid			( rvalid		),
    .rready			( rready		),
    .rresp			( rresp			),
    .bid			  ( bid			  ),
    .bvalid			( bvalid		),
    .bready			( bready		));

    wire                  rd_fifo_rden1;
    wire                 rd_fifo_rdempty1;
    wire [127:0]         rd_fifo_rddata1;
    reg                  rd_fifo_rdvalid1;

    fifo_d512t128 # (
        .AXI_DATA_WIDTH(AXI_DATA_WIDTH),
        .RD_FIFO_DEPTH(RD_FIFO_DEPTH)
      )
      fifo_d512t128_inst (
        .axi_clk(axi_clk),
        .o_clk(o_clk),
        .rst_n(rst_n),
        .fifo_rd_period(fifo_rd_period),
        .ddr_rd_valid(ddr_rd_valid),
        .rd_fifo_wr_full(rd_fifo_wr_full),
        .ddr_rd_data     (ddr_rd_data),
        .rd_fifo_rden1   (rd_fifo_rden1),
        .rd_fifo_rdempty1(rd_fifo_rdempty1),
        .rd_fifo_rddata1 (rd_fifo_rddata1)
      );

always @(posedge o_clk or negedge o_clk_rst_n)
begin
    if (!o_clk_rst_n)
        rd_fifo_rdvalid1 <= 1'b0;
    else
        rd_fifo_rdvalid1 <= rd_fifo_rden1;
end

wire tx_valid;
wire [O_VID_WIDTH-1:0] tx_vin;
wire   tx_almost_full;
    par2ser_parse#(
      .VID_WIDTH 		( O_VID_WIDTH  ),
      .AXI_DATA_WIDTH   (128)
      
    )u_par2ser_parse(
    /*i*/.clk               (o_clk              ),
    /*i*/.rst_n             (o_clk_rst_n              ),
    /*i*/.frame_period      (fifo_rd_period     ),
    /*i*/.rd_fifo_rdvalid   (rd_fifo_rdvalid1   ),
    /*i*/.rd_fifo_rddata    (rd_fifo_rddata1    ),
    /*i*/.rd_fifo_rdempty   (rd_fifo_rdempty1   ),
    /*o*/.rd_fifo_rden      (rd_fifo_rden1      ),

    /*o*/.tx_fifo_wrdata    (tx_vin             ),
    /*o*/.tx_fifo_valid     (tx_valid           ),
    /*I*/.tx_fifo_full      (tx_almost_full     )

);
always @( posedge o_clk )
begin
    h_front_porch   <=  H_FRONT_PORCH   ;
    h_sync 	 		    <=  H_SYNC 			;
    h_valid 	 	    <=  H_VALID 		;
    h_back_porch    <=  H_BACK_PORCH    ;
    v_front_porch   <=  V_FRONT_PORCH   ;
    v_sync 	 		    <=  V_SYNC 			;
    v_valid 	 	    <=  V_VALID 		;
    v_back_porch    <=  V_BACK_PORCH 	;
end

reg [1:0] rd_frame_en_r = 'd0;
reg [1:0] fifo_rd_period_r = 'd0;
reg       rd_frame_available_axi = 1'b0;
reg [1:0] rd_frame_available_o = 2'b00;
wire      rd_frame_available;
reg       fifo_rd_underflow_latched = 1'b0;
reg       fifo_rd_period_d = 1'b0;
wire      fifo_rd_frame_end;
reg       dbg_ddr_rd_seen_r = 1'b0;
reg       dbg_ddr_read_gap_r = 1'b0;
reg [9:0] dbg_ddr_no_valid_cnt = 10'd0;
reg       dbg_frame_start_seen_r = 1'b0;
reg       dbg_wr_fifo_wren_seen_r = 1'b0;
reg       dbg_wr_start_seen_r = 1'b0;
reg       dbg_awvalid_seen_r = 1'b0;
reg       dbg_wr_frame_done_seen_r = 1'b0;
reg       dbg_rd_start_seen_r = 1'b0;
reg       dbg_arvalid_seen_r = 1'b0;

always @(posedge axi_clk or negedge axi_clk_rst_n)
begin
    if (!axi_clk_rst_n)
        rd_frame_available_axi <= 1'b0;
    else if (wr_frame_done)
        rd_frame_available_axi <= 1'b1;
end

always @(posedge o_clk or negedge o_clk_rst_n)
begin
    if (!o_clk_rst_n)
        rd_frame_available_o <= 2'b00;
    else
        rd_frame_available_o <= {rd_frame_available_o[0], rd_frame_available_axi};
end

assign rd_frame_available = rd_frame_available_o[1];
assign wr_start = frame_start & frame_stable;
assign frame_ready = frame_en & out_sync & ~fifo_rd_underflow_latched;
assign fifo_rd_underflow = fifo_rd_underflow_latched;
assign fifo_rd_frame_end = fifo_rd_period_d & ~fifo_rd_period;
assign dbg_fifo_rd_period = fifo_rd_period;
assign dbg_ddr_rd_seen = dbg_ddr_rd_seen_r;
assign dbg_ddr_read_gap = dbg_ddr_read_gap_r;
assign dbg_frame_stable = frame_stable;
assign dbg_wr_frame_done = wr_frame_done;
assign dbg_rd_frame_available = rd_frame_available;
assign dbg_frame_start_seen = dbg_frame_start_seen_r;
assign dbg_wr_fifo_wren_seen = dbg_wr_fifo_wren_seen_r;
assign dbg_wr_start_seen = dbg_wr_start_seen_r;
assign dbg_awvalid_seen = dbg_awvalid_seen_r;
assign dbg_wr_frame_done_seen = dbg_wr_frame_done_seen_r;
assign dbg_rd_start_seen = dbg_rd_start_seen_r;
assign dbg_arvalid_seen = dbg_arvalid_seen_r;

always @(posedge i_clk or negedge rst_n)
begin
    if (!rst_n) begin
        dbg_frame_start_seen_r <= 1'b0;
        dbg_wr_fifo_wren_seen_r <= 1'b0;
        dbg_wr_start_seen_r <= 1'b0;
    end else begin
        if (frame_start)
            dbg_frame_start_seen_r <= 1'b1;
        if (wr_fifo_wren)
            dbg_wr_fifo_wren_seen_r <= 1'b1;
        if (wr_start)
            dbg_wr_start_seen_r <= 1'b1;
    end
end

always @(posedge axi_clk or negedge axi_clk_rst_n)
begin
    if (!axi_clk_rst_n) begin
        dbg_awvalid_seen_r <= 1'b0;
        dbg_wr_frame_done_seen_r <= 1'b0;
        dbg_rd_start_seen_r <= 1'b0;
        dbg_arvalid_seen_r <= 1'b0;
    end else begin
        if (awvalid)
            dbg_awvalid_seen_r <= 1'b1;
        if (wr_frame_done)
            dbg_wr_frame_done_seen_r <= 1'b1;
        if (rd_start)
            dbg_rd_start_seen_r <= 1'b1;
        if (arvalid)
            dbg_arvalid_seen_r <= 1'b1;
    end
end

always @(posedge axi_clk or negedge axi_clk_rst_n)
begin
    if (!axi_clk_rst_n) begin
        dbg_ddr_rd_seen_r <= 1'b0;
        dbg_ddr_read_gap_r <= 1'b0;
        dbg_ddr_no_valid_cnt <= 10'd0;
    end else if (!rd_start) begin
        dbg_ddr_no_valid_cnt <= 10'd0;
    end else if (ddr_rd_valid) begin
        dbg_ddr_rd_seen_r <= 1'b1;
        dbg_ddr_no_valid_cnt <= 10'd0;
    end else if (dbg_ddr_no_valid_cnt == 10'h3ff) begin
        dbg_ddr_read_gap_r <= 1'b1;
    end else begin
        dbg_ddr_no_valid_cnt <= dbg_ddr_no_valid_cnt + 1'b1;
    end
end

always @(posedge o_clk or negedge o_clk_rst_n)
begin
    if (!o_clk_rst_n)
        fifo_rd_period_d <= 1'b0;
    else
        fifo_rd_period_d <= fifo_rd_period;
end

always @(posedge o_clk or negedge o_clk_rst_n)
begin
    if (!o_clk_rst_n)
        fifo_rd_underflow_latched <= 1'b0;
    else if (!frame_en || fifo_rd_frame_end)
        fifo_rd_underflow_latched <= 1'b0;
    else if (tx_fifo_rd_underflow)
        fifo_rd_underflow_latched <= 1'b1;
end

always @( posedge o_clk or negedge o_clk_rst_n ) //two pipeline
begin
    if( !o_clk_rst_n ) begin
      rd_frame_en_r <= 'b0;
    end else begin
      rd_frame_en_r <= {rd_frame_en_r[0],frame_stable};
    end
end
always @( posedge axi_clk or negedge rst_n )
begin
    if( !rst_n ) begin 
        fifo_rd_period_r <= 'd0;
    end else begin 
        fifo_rd_period_r <= {fifo_rd_period_r[0],fifo_rd_period};
    end 
end
assign rd_start = fifo_rd_period_r[1] & rd_frame_available_axi;
assign frame_en = rd_frame_en_r[1] & rd_frame_available;
data_tx #(
	.VID_WIDTH 				( O_VID_WIDTH  ),
	.FIFO_DIPTH  			( 1024 				 ),
	.FIFO_ALMOST_FULL	( 960          )
	)u_data_tx(
	/*i*/.clk					        (o_clk			    ),
	/*i*/.rst_n				        (o_clk_rst_n    ),
	/*i*/.H_FRONT_PORCH       (h_front_porch  ),
	/*i*/.H_SYNC 	 		        (h_sync 	 	    ),
	/*i*/.H_VALID 	 	        (h_valid 	 	    ),
	/*i*/.H_BACK_PORCH        (h_back_porch   ),
	/*i*/.V_FRONT_PORCH       (v_front_porch  ),
	/*i*/.V_SYNC 	 		        (v_sync 	 	    ),
	/*i*/.V_VALID 	 	        (v_valid 	 	    ),
	/*i*/.V_BACK_PORCH        (v_back_porch   ),
	/*i*/.frame_en		        (frame_en & out_sync ),
	/*i*/.fifo_wr_data        (tx_vin			    ),
	/*i*/.fifo_wr_en	        (tx_valid		    ),
	/*o*/.fifo_wr_almost_full	(tx_almost_full	),
	/*o*/.fifo_rd_period      (fifo_rd_period ),
	/*o*/.fifo_rd_underflow   (tx_fifo_rd_underflow),
	/*o*/.vout		            (vout		        ),
	/*o*/.o_hs		            (o_hs		        ),
	/*o*/.o_vs		            (o_vs		        ),
	/*o*/.o_de		            (o_de		        )
	
	);


endmodule
