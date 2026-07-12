// Synthetic RGB888 2ppc video source for preprocessing verification only.
//
// This module intentionally has the same timing/data contract as the Debayer
// tap: i_vs/i_hs/i_de/i_valid plus {B1,G1,R1,B0,G0,R0}. It does not connect
// to HDMI, MIPI, DDR, CPU, or any external pin. The object color advances at
// frame boundaries so the downstream statistics path can be exercised without
// a stable camera stream.
module synthetic_2ppc_source #(
    // Horizontal values are 2ppc beats. The defaults expand through the CDC
    // to hdmi_top's 960x1080, HSP=4, HBP=88, HFP=120 input timing.
    parameter FRAME_WIDTH  = 960,
    parameter FRAME_HEIGHT = 1080,
    parameter H_SYNC       = 2,
    parameter H_BACK       = 44,
    parameter H_FRONT      = 60,
    parameter V_SYNC       = 2,
    parameter V_BACK       = 20,
    parameter V_FRONT      = 20,
    // At the 70 MHz source clock, 60 frames is approximately one second for
    // the 960x1080 verification raster used by top.v.
    parameter FRAMES_PER_COLOR = 60
) (
    input               i_clk,
    input               i_rst_n,
    output reg          o_vs,
    output reg          o_hs,
    output reg          o_de,
    output reg          o_valid,
    output reg [47:0]   o_rgb_2ppc
);

    localparam [7:0] BG_R = 8'd128;
    localparam [7:0] BG_G = 8'd128;
    localparam [7:0] BG_B = 8'd128;

    localparam [2:0] COLOR_RED    = 3'd0;
    localparam [2:0] COLOR_BLUE   = 3'd1;
    localparam [2:0] COLOR_YELLOW = 3'd2;
    localparam [2:0] COLOR_WHITE  = 3'd3;
    localparam [2:0] COLOR_BLACK  = 3'd4;

    // Each source beat carries two horizontal pixels. FRAME_WIDTH must be even.
    localparam PAIRS_PER_LINE = FRAME_WIDTH / 2;
    localparam H_ACTIVE_START = H_SYNC + H_BACK;
    localparam H_TOTAL = H_SYNC + H_BACK + PAIRS_PER_LINE + H_FRONT;
    localparam V_ACTIVE_START = V_SYNC + V_BACK;
    localparam V_TOTAL = V_SYNC + V_BACK + FRAME_HEIGHT + V_FRONT;
    // Derive both axes from one side length.  The old one-third calculation
    // produced 320x360 at 960x1080, which is a rectangle rather than the
    // intended centered square.
    localparam OBJECT_SIDE = FRAME_WIDTH / 3;
    localparam OBJECT_X0 = (FRAME_WIDTH - OBJECT_SIDE) / 2;
    localparam OBJECT_X1 = OBJECT_X0 + OBJECT_SIDE;
    localparam OBJECT_Y0 = (FRAME_HEIGHT - OBJECT_SIDE) / 2;
    localparam OBJECT_Y1 = OBJECT_Y0 + OBJECT_SIDE;

    reg [15:0] h_count;
    reg [15:0] v_count;
    reg [2:0]  color_id;
    reg [15:0] color_frame_count;
    reg [23:0] pixel0_rgb;
    reg [23:0] pixel1_rgb;

    // Return RGB in {R,G,B}. Use full-scale canonical values for the five
    // competition colors so downstream threshold tests have unambiguous input.
    function [23:0] object_rgb;
        input [2:0] color_id;
        begin
            case (color_id)
                COLOR_RED:    object_rgb = {8'hFF, 8'h00, 8'h00};
                COLOR_BLUE:   object_rgb = {8'h00, 8'h00, 8'hFF};
                COLOR_YELLOW: object_rgb = {8'hFF, 8'hFF, 8'h00};
                COLOR_WHITE:  object_rgb = {8'hFF, 8'hFF, 8'hFF};
                default:      object_rgb = {8'h00, 8'h00, 8'h00};
            endcase
        end
    endfunction

    // Keep the source intentionally simple: a centered square on a neutral
    // background. The fixed geometry makes bbox and area expectations stable.
    function [23:0] pixel_rgb;
        input [15:0] pixel_x;
        input [15:0] line_y;
        input [2:0]  color_id;
        begin
            if ((pixel_x >= OBJECT_X0) && (pixel_x < OBJECT_X1) &&
                (line_y  >= OBJECT_Y0) && (line_y  < OBJECT_Y1)) begin
                pixel_rgb = object_rgb(color_id);
            end else begin
                pixel_rgb = {BG_R, BG_G, BG_B};
            end
        end
    endfunction

    always @(posedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            o_vs                <= 1'b0;
            o_hs                <= 1'b0;
            o_de                <= 1'b0;
            o_valid             <= 1'b0;
            o_rgb_2ppc          <= 48'd0;
            h_count             <= 16'd0;
            v_count             <= 16'd0;
            color_id            <= COLOR_RED;
            color_frame_count   <= 16'd0;
        end else begin
            // Generate a full blanking/sync raster, not just DE. The existing
            // HDMI timing detector can therefore validate the synthetic path.
            o_vs    <= (v_count < V_SYNC);
            o_hs    <= (h_count < H_SYNC);
            o_de    <= (h_count >= H_ACTIVE_START) &&
                       (h_count < H_ACTIVE_START + PAIRS_PER_LINE) &&
                       (v_count >= V_ACTIVE_START) &&
                       (v_count < V_ACTIVE_START + FRAME_HEIGHT);
            o_valid <= (h_count >= H_ACTIVE_START) &&
                       (h_count < H_ACTIVE_START + PAIRS_PER_LINE) &&
                       (v_count >= V_ACTIVE_START) &&
                       (v_count < V_ACTIVE_START + FRAME_HEIGHT);

            if ((h_count >= H_ACTIVE_START) &&
                (h_count < H_ACTIVE_START + PAIRS_PER_LINE) &&
                (v_count >= V_ACTIVE_START) &&
                (v_count < V_ACTIVE_START + FRAME_HEIGHT)) begin
                pixel0_rgb = pixel_rgb((h_count - H_ACTIVE_START) << 1,
                                        v_count - V_ACTIVE_START, color_id);
                pixel1_rgb = pixel_rgb(((h_count - H_ACTIVE_START) << 1) + 16'd1,
                                        v_count - V_ACTIVE_START, color_id);
                // The HDMI CDC emits i_data[23:0] before i_data[47:24]. Keep
                // each pixel as {R,G,B}; reversing it makes yellow appear cyan.
                o_rgb_2ppc <= {
                    pixel1_rgb,
                    pixel0_rgb
                };

            end else begin
                o_rgb_2ppc <= 48'd0;
            end

            if (h_count == H_TOTAL - 1) begin
                h_count <= 16'd0;
                if (v_count == V_TOTAL - 1) begin
                    v_count <= 16'd0;
                    if (color_frame_count == FRAMES_PER_COLOR - 1) begin
                        color_frame_count <= 16'd0;
                        if (color_id == COLOR_BLACK)
                            color_id <= COLOR_RED;
                        else
                            color_id <= color_id + 1'b1;
                    end else begin
                        color_frame_count <= color_frame_count + 1'b1;
                    end
                end else begin
                    v_count <= v_count + 1'b1;
                end
            end else begin
                h_count <= h_count + 1'b1;
            end
        end
    end

endmodule
