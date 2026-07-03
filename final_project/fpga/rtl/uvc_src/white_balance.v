module white_balance #(
    parameter DATA_WIDTH = 8,                 // RGB each channel bit width
    parameter GAIN_WIDTH = 16,                // Q8.8 fixed-point gain width
    parameter GAIN_ONE   = 16'd256,           // 1.0 in Q8.8
    parameter GAIN_MIN   = 16'd128,           // 0.5
    parameter GAIN_MAX   = 16'd512,           // 2.0
    parameter CNT_WIDTH  = 24
)(
    input  wire                        clk,
    input  wire                        rst_n,
    input  wire                        hs_in,
    input  wire                        vs_in,
    input  wire                        de_in,
    input  wire [DATA_WIDTH*6-1:0]     data_in,  // [47:24]=odd pixel, [23:0]=even pixel
    output reg                         hs_out,
    output reg                         vs_out,
    output reg                         de_out,
    output reg  [DATA_WIDTH*6-1:0]     data_out
);

    localparam SUM_WIDTH = CNT_WIDTH + DATA_WIDTH;
    localparam MUL_WIDTH = DATA_WIDTH + GAIN_WIDTH;

    reg de_d;
    reg vs_d;
    wire de_fall = de_d & (~de_in);
    wire vs_fall = vs_d &( ~vs_in);
    reg [SUM_WIDTH-1:0] sum_r, sum_g, sum_b;
    reg [CNT_WIDTH-1:0] pixel_cnt;
    reg [DATA_WIDTH-1:0] avg_r, avg_g, avg_b;
    reg [GAIN_WIDTH-1:0] gain_r, gain_b;

    reg [MUL_WIDTH-1:0] odd_r_mul, odd_b_mul;
    reg [MUL_WIDTH-1:0] even_r_mul, even_b_mul;


    wire [DATA_WIDTH-1:0] odd_r_in  = data_in[DATA_WIDTH*6-1:DATA_WIDTH*5];
    wire [DATA_WIDTH-1:0] odd_g_in  = data_in[DATA_WIDTH*5-1:DATA_WIDTH*4];
    wire [DATA_WIDTH-1:0] odd_b_in  = data_in[DATA_WIDTH*4-1:DATA_WIDTH*3];
    wire [DATA_WIDTH-1:0] even_r_in = data_in[DATA_WIDTH*3-1:DATA_WIDTH*2];
    wire [DATA_WIDTH-1:0] even_g_in = data_in[DATA_WIDTH*2-1:DATA_WIDTH];
    wire [DATA_WIDTH-1:0] even_b_in = data_in[DATA_WIDTH-1:0];

    function [GAIN_WIDTH-1:0] gain_clip;
        input [GAIN_WIDTH-1:0] gain_in;
        begin
            if (gain_in < GAIN_MIN) begin
                gain_clip = GAIN_MIN;
            end else if (gain_in > GAIN_MAX) begin
                gain_clip = GAIN_MAX;
            end else begin
                gain_clip = gain_in;
            end
        end
    endfunction

    function [DATA_WIDTH-1:0] sat_q8_8_to_u8;
        input [MUL_WIDTH-1:0] val_q8_8;
        begin
            if (|val_q8_8[MUL_WIDTH-1:DATA_WIDTH+8]) begin
                sat_q8_8_to_u8 = {DATA_WIDTH{1'b1}};
            end else begin
                sat_q8_8_to_u8 = val_q8_8[DATA_WIDTH+7:8];
            end
        end
    endfunction

    // Statistics and gain update: update once per de falling edge.
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            de_d      <= 1'b0;
            sum_r     <= {SUM_WIDTH{1'b0}};
            sum_g     <= {SUM_WIDTH{1'b0}};
            sum_b     <= {SUM_WIDTH{1'b0}};
            pixel_cnt <= {CNT_WIDTH{1'b0}};

        end else begin
            de_d <= de_in;
            if( vs_fall ) begin 
                pixel_cnt <= 'd0;
                sum_r <= 'd0;
                sum_g <= 'd0;
                sum_b <= 'd0;
            end else if (de_in) begin
                sum_r     <= sum_r + odd_r_in + even_r_in;
                sum_g     <= sum_g + odd_g_in + even_g_in;
                sum_b     <= sum_b + odd_b_in + even_b_in;
                pixel_cnt <= pixel_cnt + 2'd2;
            end
        end
    end
    wire [DATA_WIDTH-1:0] avg_r_calc = sum_r / pixel_cnt;
    wire [DATA_WIDTH-1:0] avg_g_calc = sum_g / pixel_cnt;
    wire [DATA_WIDTH-1:0] avg_b_calc = sum_b / pixel_cnt;

     always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            avg_r     <= {DATA_WIDTH{1'b1}} >> 1;
            avg_g     <= {DATA_WIDTH{1'b1}} >> 1;
            avg_b     <= {DATA_WIDTH{1'b1}} >> 1;
            gain_r    <= GAIN_ONE;
            gain_b    <= GAIN_ONE;
        end else begin

            if (vs_fall && (pixel_cnt != {CNT_WIDTH{1'b0}})) begin
                avg_r <= avg_r_calc;
                avg_g <= avg_g_calc;
                avg_b <= avg_b_calc;

                if (avg_r_calc == {DATA_WIDTH{1'b0}}) begin
                    gain_r <= GAIN_ONE;
                end else begin
                    gain_r <= gain_clip((avg_g_calc * GAIN_ONE) / avg_r_calc);
                end

                if (avg_b_calc == {DATA_WIDTH{1'b0}}) begin
                    gain_b <= GAIN_ONE;
                end else begin
                    gain_b <= gain_clip((avg_g_calc * GAIN_ONE) / avg_b_calc);
                end

            end
        end
    end

    // Pixel white balance correction for two pixels per clock.
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            odd_r_mul  <= {MUL_WIDTH{1'b0}};
            odd_b_mul  <= {MUL_WIDTH{1'b0}};
            even_r_mul <= {MUL_WIDTH{1'b0}};
            even_b_mul <= {MUL_WIDTH{1'b0}};
            hs_out <= 1'b0;
            vs_out <= 1'b0;
            de_out <= 1'b0;
            data_out <= {(DATA_WIDTH*6){1'b0}};
        end else if (de_in) begin
            hs_out <= hs_in;
            vs_out <= vs_in;
            de_out <= de_in;
            odd_r_mul  <= odd_r_in  * gain_r;
            odd_b_mul  <= odd_b_in  * gain_b;
            even_r_mul <= even_r_in * gain_r;
            even_b_mul <= even_b_in * gain_b;

            data_out <= {
                sat_q8_8_to_u8(odd_r_in * gain_r),
                odd_g_in,
                sat_q8_8_to_u8(odd_b_in * gain_b),
                sat_q8_8_to_u8(even_r_in * gain_r),
                even_g_in,
                sat_q8_8_to_u8(even_b_in * gain_b)
            };
        end else begin
            odd_r_mul  <= {MUL_WIDTH{1'b0}};
            odd_b_mul  <= {MUL_WIDTH{1'b0}};
            even_r_mul <= {MUL_WIDTH{1'b0}};
            even_b_mul <= {MUL_WIDTH{1'b0}};
            hs_out <= hs_in;
            vs_out <= vs_in;
            de_out <= de_in;
            data_out <= {(DATA_WIDTH*6){1'b0}};
        end
    end

endmodule
