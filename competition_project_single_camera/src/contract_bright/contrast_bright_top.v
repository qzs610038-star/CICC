
module contrast_bright_top(
	
	input		wire				ch0_clk,
	input		wire				ch1_clk,
	input		wire				rst_n,
	input		wire	[7:0]	ch0_contrast,
	input		wire	[7:0]	ch0_lumi_r	,
	input		wire	[7:0]	ch0_lumi_g	,				
	input		wire	[7:0]	ch0_lumi_b	, 
	input		wire [31:0]	ch0_invert_frame_size,     
	input		wire	[7:0]	ch1_contrast,       
	input		wire	[7:0]	ch1_lumi_r	,       
	input		wire	[7:0]	ch1_lumi_g	,
	input		wire	[7:0]	ch1_lumi_b	, 
	input		wire [31:0]	ch1_invert_frame_size,
	            
	input		wire	[1:0]	ch0_h_sync_i,
	input		wire	[1:0]	ch1_h_sync_i,
	
	input		wire	[1:0]	ch0_v_sync_i,
	input		wire	[1:0]	ch1_v_sync_i,
	
	input		wire	[1:0]	ch0_de_i,
	input		wire	[1:0]	ch1_de_i,
	
	input		wire [47:0] ch0_vid_i,
	input		wire [47:0] ch1_vid_i,
	
	output	wire	[1:0]	ch0_h_sync_o,  
	output	wire	[1:0]	ch1_h_sync_o,  
	output	wire	[1:0]	ch0_v_sync_o,  
	output	wire	[1:0]	ch1_v_sync_o,  
	output	wire	[1:0]	ch0_de_o,      
	output	wire	[1:0]	ch1_de_o,          
	output	wire [47:0] ch0_vid_o,       
	output	wire [47:0] ch1_vid_o    


);

reg		[6:0] contrast0 = 0;
reg					contrast_sign0 = 1'b0;  
reg		[6:0] contrast1 = 0;        
reg					contrast_sign1 = 1'b0; 


always @( posedge ch0_clk )
begin
		contrast_sign0 <= ch0_contrast[7];
		if( ch0_contrast[7] ) begin
		   	contrast0 <= ch0_contrast[6:0];        	
		end else begin
				contrast0 <= ~ch0_contrast[6:0];
		end
end 
always @( posedge ch1_clk )
begin
		contrast_sign1 <= ch1_contrast[7];
		if( ch1_contrast[7] ) begin
		   	contrast1 <= ch1_contrast[6:0];        	
		end else begin
				contrast1 <= ~ch1_contrast[6:0];
		end
end 


// channel 0
 Contrast_Adj #(
	.COLOR_PLANE_WIDTH (8), 
	.COLOR_PLANE_NUM  (3),  
   .PIXCEL_NUM  (2)        
)u0_contrast_adj(
.vid_clk(ch0_clk),          
.rst_n(rst_n),           
.vid_h_sync_i(ch0_h_sync_i),         
.vid_v_sync_i(ch0_v_sync_i),         
.vid_de_i(ch0_de_i	),            
.vid_data_i(ch0_vid_i),     
.CONTRAST_SIG(contrast_sign0), 
.CONTRAST({1'b0,contrast0}), 
.invert_frame_size(ch0_invert_frame_size),
.lumi_r(ch0_lumi_r),
.lumi_g(ch0_lumi_g),
.lumi_b(ch0_lumi_b), 
.vid_h_sync_o(ch0_h_sync_o),       
.vid_v_sync_o(ch0_v_sync_o),       
.vid_de_o(ch0_de_o),          
.vid_data_o (ch0_vid_o)
);  
// channel 1
Contrast_Adj #(
	.COLOR_PLANE_WIDTH (8), 
	.COLOR_PLANE_NUM  (3),  
   .PIXCEL_NUM  (2)        
)u1_contrast_adj(
.vid_clk(ch1_clk),          
.rst_n(rst_n),           
.vid_h_sync_i(ch1_h_sync_i),         
.vid_v_sync_i(ch1_v_sync_i),         
.vid_de_i(ch1_de_i	),            
.vid_data_i(ch1_vid_i), 
.invert_frame_size(ch1_invert_frame_size),    
.CONTRAST_SIG(contrast_sign1), 
.CONTRAST({1'b0,contrast1}), 
.lumi_r(ch1_lumi_r),
.lumi_g(ch1_lumi_g),
.lumi_b(ch1_lumi_b),   
.vid_h_sync_o(ch1_h_sync_o),       
.vid_v_sync_o(ch1_v_sync_o),       
.vid_de_o(ch1_de_o),          
.vid_data_o (ch1_vid_o)
);  


endmodule
