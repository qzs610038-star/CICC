// Reference SC431HAI controller path with passive debug observation only.
module i2c_master_ctrl_top #(
    parameter DATA_LENGTH = 162,
    parameter I2C_REG_ADDR_WIDTH = 16,
    parameter I2C_DATA_WIDTH = 8,
    parameter I2C_DEVICE_ADDR = 8'h60,
    parameter CLK_DIV = 16'd499
)(
    input wire clk,
    input wire rst_n,
    input wire scl_pad_i,
    output wire scl_pad_o,
    output wire scl_padoen_o,
    input wire sda_pad_i,
    output wire sda_pad_o,
    output wire sda_padoen_o,
    output wire dbg_init_done,
    output wire dbg_wr_en,
    output wire dbg_wr_done,
    output wire dbg_cfg_done,
    output wire dbg_last_index_seen,
    output wire dbg_stream_on_index_reached,
    output wire dbg_stream_on_seen,
    output wire dbg_stream_on_done,
    output wire dbg_stream_on_clean,
    output wire dbg_stream_on_error,
    output wire dbg_i2c_status_sample_seen,
    output wire dbg_i2c_status_rxack_seen,
    output wire dbg_i2c_status_busy_seen,
    output wire dbg_i2c_status_al_seen,
    output wire dbg_i2c_status_tip_seen,
    output wire dbg_i2c_status_rxack_prebyte_seen,
    output wire dbg_i2c_status_rxack_devaddr_seen,
    output wire dbg_i2c_status_rxack_reg_high_seen,
    output wire dbg_i2c_status_rxack_reg_low_seen,
    output wire dbg_i2c_status_rxack_data_seen,
    output wire dbg_stream_on_rxack_devaddr_seen,
    output wire dbg_stream_on_rxack_reg_high_seen,
    output wire dbg_stream_on_rxack_reg_low_seen,
    output wire dbg_stream_on_rxack_data_seen,
    output wire [7:0] dbg_i2c_last_status
);

wire init_done;
wire wr_done;
wire rd_done;
wire wr_en;
wire rd_en;
wire [I2C_REG_ADDR_WIDTH-1:0] addr;
wire [I2C_DATA_WIDTH-1:0] set_data;
wire [I2C_DATA_WIDTH-1:0] get_data;
wire get_valid;
wire [7:0] dev_addr;
wire [2:0] i2c_addr;
wire i2c_waitrequest;
wire [7:0] i2c_readdata;
wire [7:0] i2c_writedata;
wire i2c_write;
wire i2c_chipselect;
wire tx_i2c_irq;
wire dbg_rd_nack_devaddr_w;
wire dbg_rd_nack_reg_high_w;
wire dbg_rd_nack_reg_low_w;
wire dbg_rd_nack_read_addr_w;

i2c_master_reg_set #(
    .DATA_LENGTH(DATA_LENGTH),
    .I2C_REG_ADDR_WIDTH(I2C_REG_ADDR_WIDTH),
    .I2C_DATA_WIDTH(I2C_DATA_WIDTH),
    .I2C_DEVICE_ADDR(I2C_DEVICE_ADDR)
) u_reg_set (
    .clk(clk),
    .rst_n(rst_n),
    .init_done(init_done),
    .rd_done(rd_done),
    .wr_done(wr_done),
    .wr_en(wr_en),
    .rd_en(rd_en),
    .addr(addr),
    .dout(set_data),
    .dev_addr(dev_addr)
);

i2c_16addr_8data #(
    .CLK_DIV(CLK_DIV),
    .IRQ_EN(1'b0),
    .I2C_EN(1'b1)
) u_i2c_ctrl (
    .clk(clk),
    .rst_n(rst_n),
    .init_done(init_done),
    .rd_done(rd_done),
    .wr_done(wr_done),
    .wr_en(wr_en),
    .rd_en(rd_en),
    .addr(addr),
    .dev_addr(dev_addr),
    .din(set_data),
    .dout(get_data),
    .dout_valid(get_valid),
    .dbg_rd_nack_devaddr(dbg_rd_nack_devaddr_w),
    .dbg_rd_nack_reg_high(dbg_rd_nack_reg_high_w),
    .dbg_rd_nack_reg_low(dbg_rd_nack_reg_low_w),
    .dbg_rd_nack_read_addr(dbg_rd_nack_read_addr_w),
    .i2c_address(i2c_addr),
    .i2c_write(i2c_write),
    .i2c_readdata(i2c_readdata),
    .i2c_writedata(i2c_writedata),
    .i2c_chipselect(i2c_chipselect),
    .i2c_waitrequest(1'b0)
);

i2c_master_top u_i2c_master (
    .arst_i(1'b1),
    .scl_pad_i(scl_pad_i),
    .scl_pad_o(scl_pad_o),
    .scl_padoen_o(scl_padoen_o),
    .sda_pad_i(sda_pad_i),
    .sda_pad_o(sda_pad_o),
    .sda_padoen_o(sda_padoen_o),
    .wb_ack_o(i2c_waitrequest),
    .wb_adr_i(i2c_addr),
    .wb_clk_i(clk),
    .wb_dat_i(i2c_writedata),
    .wb_dat_o(i2c_readdata),
    .wb_rst_i(~rst_n),
    .wb_stb_i(i2c_chipselect),
    .wb_we_i(i2c_write),
    .wb_inta_o(tx_i2c_irq)
);

wire stream_on_access = (addr == 16'h0100) && (set_data == 8'h01);
wire last_access = (addr == 16'h3213) && (set_data == 8'h05);
reg stream_on_pending = 1'b0;
reg stream_on_seen_r = 1'b0;
reg stream_on_done_r = 1'b0;
reg last_index_seen_r = 1'b0;
reg cfg_done_r = 1'b0;
reg readback_done_r = 1'b0;
reg [7:0] readback_data_r = 8'd0;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        stream_on_pending <= 1'b0;
        stream_on_seen_r <= 1'b0;
        stream_on_done_r <= 1'b0;
        last_index_seen_r <= 1'b0;
        cfg_done_r <= 1'b0;
        readback_done_r <= 1'b0;
        readback_data_r <= 8'd0;
    end else begin
        if (wr_en && stream_on_access) begin
            stream_on_pending <= 1'b1;
            stream_on_seen_r <= 1'b1;
        end
        if (wr_done && stream_on_pending) begin
            stream_on_pending <= 1'b0;
            stream_on_done_r <= 1'b1;
        end
        if (wr_en && last_access)
            last_index_seen_r <= 1'b1;
        if (wr_done && last_index_seen_r)
            cfg_done_r <= 1'b1;
        if (rd_done && (addr == 16'h0100)) begin
            readback_done_r <= 1'b1;
            readback_data_r <= get_data;
            cfg_done_r <= 1'b1;
        end
    end
end

assign dbg_init_done = init_done;
assign dbg_wr_en = wr_en;
assign dbg_wr_done = wr_done;
assign dbg_cfg_done = cfg_done_r;
assign dbg_last_index_seen = last_index_seen_r;
assign dbg_stream_on_index_reached = stream_on_seen_r;
assign dbg_stream_on_seen = stream_on_seen_r;
assign dbg_stream_on_done = stream_on_done_r;
assign dbg_stream_on_clean = stream_on_done_r;
assign dbg_stream_on_error = 1'b0;
assign dbg_i2c_status_sample_seen = readback_done_r;
assign dbg_i2c_status_rxack_seen = readback_done_r & (readback_data_r == 8'h01);
assign dbg_i2c_status_busy_seen = readback_done_r & (readback_data_r != 8'h00) &
                                 (readback_data_r != 8'h01);
assign dbg_i2c_status_al_seen = 1'b0;
assign dbg_i2c_status_tip_seen = 1'b0;
assign dbg_i2c_status_rxack_prebyte_seen = dbg_rd_nack_devaddr_w |
                                           dbg_rd_nack_reg_high_w |
                                           dbg_rd_nack_reg_low_w |
                                           dbg_rd_nack_read_addr_w;
assign dbg_i2c_status_rxack_devaddr_seen = dbg_rd_nack_devaddr_w;
assign dbg_i2c_status_rxack_reg_high_seen = dbg_rd_nack_reg_high_w;
assign dbg_i2c_status_rxack_reg_low_seen = dbg_rd_nack_reg_low_w;
assign dbg_i2c_status_rxack_data_seen = dbg_rd_nack_read_addr_w;
assign dbg_stream_on_rxack_devaddr_seen = 1'b0;
assign dbg_stream_on_rxack_reg_high_seen = 1'b0;
assign dbg_stream_on_rxack_reg_low_seen = 1'b0;
assign dbg_stream_on_rxack_data_seen = 1'b0;
assign dbg_i2c_last_status = readback_data_r;

endmodule
