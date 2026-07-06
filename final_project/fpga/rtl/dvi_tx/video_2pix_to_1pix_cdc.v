module video_2pix_to_1pix_cdc #(
    parameter FIFO_DEPTH = 1024,
    parameter START_LEVEL = 16,
    parameter COUNT_WIDTH = $clog2(FIFO_DEPTH) + 1
) (
    input  wire        wr_clk,
    input  wire        rd_clk,
    input  wire        rst_n,
    input  wire        i_vs,
    input  wire        i_hs,
    input  wire        i_de,
    input  wire [47:0] i_data,
    output reg         o_vs,
    output reg         o_hs,
    output reg         o_de,
    output reg  [23:0] o_data,
    output reg         o_active,
    output reg         o_underflow
);

    wire [COUNT_WIDTH-1:0] fifo_wr_usedw;
    wire [COUNT_WIDTH-1:0] fifo_rd_usedw;
    wire                  fifo_full;
    wire                  fifo_empty;
    wire [50:0]           fifo_q;
    wire                  fifo_rd_en;
    wire                  fifo_reset = ~rst_n;
    reg                   fifo_rd_valid = 1'b0;
    reg [50:0]            active_word = 51'd0;
    reg [1:0]             phase = 2'd0;
    localparam [1:0]      PH_REQUEST = 2'd0;
    localparam [1:0]      PH_LOW     = 2'd1;
    localparam [1:0]      PH_HIGH    = 2'd2;
    localparam [COUNT_WIDTH-1:0] START_LEVEL_W = START_LEVEL;
    wire                  enough_fifo_level = fifo_rd_usedw >= START_LEVEL_W;

    DC_FIFO #(
        .DATA_WIDTH(51),
        .FIFO_DEPTH(FIFO_DEPTH)
    ) u_video_cdc_fifo (
        .Reset(fifo_reset),
        .WrClk(wr_clk),
        .WrEn(1'b1),
        .WrDNum(fifo_wr_usedw),
        .WrFull(fifo_full),
        .WrData({i_vs, i_hs, i_de, i_data}),
        .RdClk(rd_clk),
        .RdEn(fifo_rd_en),
        .RdDNum(fifo_rd_usedw),
        .RdEmpty(fifo_empty),
        .DataVal(),
        .RdData(fifo_q)
    );

    assign fifo_rd_en = o_active & ~fifo_empty &
                        ((phase == PH_REQUEST) | (phase == PH_HIGH));

    always @(posedge rd_clk or negedge rst_n) begin
        if (!rst_n) begin
            phase <= PH_REQUEST;
            fifo_rd_valid <= 1'b0;
            active_word <= 51'd0;
            o_vs <= 1'b0;
            o_hs <= 1'b0;
            o_de <= 1'b0;
            o_data <= 24'd0;
            o_active <= 1'b0;
            o_underflow <= 1'b0;
        end else begin
            fifo_rd_valid <= fifo_rd_en;
            o_underflow <= 1'b0;

            if (!o_active) begin
                phase <= PH_REQUEST;
                fifo_rd_valid <= 1'b0;
                o_vs <= 1'b0;
                o_hs <= 1'b0;
                o_de <= 1'b0;
                o_data <= 24'd0;
                if (enough_fifo_level)
                    o_active <= 1'b1;
            end else begin
                case (phase)
                    PH_REQUEST: begin
                        o_vs <= 1'b0;
                        o_hs <= 1'b0;
                        o_de <= 1'b0;
                        o_data <= 24'd0;
                        if (fifo_empty) begin
                            o_active <= 1'b0;
                            o_underflow <= 1'b1;
                        end else begin
                            phase <= PH_LOW;
                        end
                    end

                    PH_LOW: begin
                        if (fifo_rd_valid) begin
                            active_word <= fifo_q;
                            o_vs <= fifo_q[50];
                            o_hs <= fifo_q[49];
                            o_de <= fifo_q[48];
                            o_data <= fifo_q[48] ? fifo_q[23:0] : 24'd0;
                            phase <= PH_HIGH;
                        end else begin
                            o_vs <= 1'b0;
                            o_hs <= 1'b0;
                            o_de <= 1'b0;
                            o_data <= 24'd0;
                            phase <= PH_REQUEST;
                        end
                    end

                    PH_HIGH: begin
                        o_vs <= active_word[50];
                        o_hs <= active_word[49];
                        o_de <= active_word[48];
                        o_data <= active_word[48] ? active_word[47:24] : 24'd0;
                        phase <= fifo_empty ? PH_REQUEST : PH_LOW;
                    end

                    default: begin
                        phase <= PH_REQUEST;
                    end
                endcase
            end
        end
    end

endmodule
