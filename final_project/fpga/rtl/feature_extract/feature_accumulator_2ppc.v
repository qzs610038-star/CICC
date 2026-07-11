module feature_accumulator_2ppc (
    input             i_clk,
    input             i_rst_n,
    input             i_frame_start,
    input             i_roi_invalid,
    input             i_pixel_valid0,
    input             i_pixel_valid1,
    input             i_roi_hit0,
    input             i_roi_hit1,
    input       [7:0] i_r0,
    input       [7:0] i_g0,
    input       [7:0] i_b0,
    input       [7:0] i_r1,
    input       [7:0] i_g1,
    input       [7:0] i_b1,
    input       [9:0] i_luma0,
    input       [9:0] i_luma1,
    input             i_red0,
    input             i_blue0,
    input             i_yellow0,
    input             i_foreground0,
    input             i_red1,
    input             i_blue1,
    input             i_yellow1,
    input             i_foreground1,
    input      [15:0] i_x0,
    input      [15:0] i_x1,
    input      [15:0] i_y,
    output            o_frame_complete,
    output            o_frame_seen,
    output            o_bbox_valid,
    output            o_roi_invalid_seen,
    output reg [31:0] o_roi_pixel_count,
    output reg [31:0] o_sum_r,
    output reg [31:0] o_sum_g,
    output reg [31:0] o_sum_b,
    output reg [31:0] o_sum_y,
    output reg [31:0] o_red_area,
    output reg [31:0] o_blue_area,
    output reg [31:0] o_yellow_area,
    output reg [31:0] o_fg_area,
    output reg [15:0] o_bbox_x_min,
    output reg [15:0] o_bbox_y_min,
    output reg [15:0] o_bbox_x_max,
    output reg [15:0] o_bbox_y_max
);

    reg frame_seen;
    reg bbox_valid;
    reg roi_invalid_seen;

    wire fg0 = i_roi_hit0 && i_foreground0;
    wire fg1 = i_roi_hit1 && i_foreground1;
    wire [15:0] fg_x_min = fg0 ? i_x0 : i_x1;
    wire [15:0] fg_x_max = fg1 ? i_x1 : i_x0;

    assign o_frame_complete = i_frame_start && frame_seen;
    assign o_frame_seen = frame_seen;
    assign o_bbox_valid = bbox_valid;
    assign o_roi_invalid_seen = roi_invalid_seen;

    always @(posedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            frame_seen          <= 1'b0;
            bbox_valid          <= 1'b0;
            roi_invalid_seen    <= 1'b0;
            o_roi_pixel_count   <= 32'd0;
            o_sum_r             <= 32'd0;
            o_sum_g             <= 32'd0;
            o_sum_b             <= 32'd0;
            o_sum_y             <= 32'd0;
            o_red_area          <= 32'd0;
            o_blue_area         <= 32'd0;
            o_yellow_area       <= 32'd0;
            o_fg_area           <= 32'd0;
            o_bbox_x_min        <= 16'd0;
            o_bbox_y_min        <= 16'd0;
            o_bbox_x_max        <= 16'd0;
            o_bbox_y_max        <= 16'd0;
        end else if (i_frame_start) begin
            frame_seen          <= 1'b0;
            bbox_valid          <= 1'b0;
            roi_invalid_seen    <= i_roi_invalid;
            o_roi_pixel_count   <= 32'd0;
            o_sum_r             <= 32'd0;
            o_sum_g             <= 32'd0;
            o_sum_b             <= 32'd0;
            o_sum_y             <= 32'd0;
            o_red_area          <= 32'd0;
            o_blue_area         <= 32'd0;
            o_yellow_area       <= 32'd0;
            o_fg_area           <= 32'd0;
            o_bbox_x_min        <= 16'd0;
            o_bbox_y_min        <= 16'd0;
            o_bbox_x_max        <= 16'd0;
            o_bbox_y_max        <= 16'd0;
        end else begin
            if (i_roi_invalid)
                roi_invalid_seen <= 1'b1;

            if (i_pixel_valid0 || i_pixel_valid1)
                frame_seen <= 1'b1;

            if (i_roi_hit0 || i_roi_hit1) begin
                o_roi_pixel_count <= o_roi_pixel_count + i_roi_hit0 + i_roi_hit1;
                o_sum_r           <= o_sum_r + (i_roi_hit0 ? i_r0 : 8'd0) +
                                     (i_roi_hit1 ? i_r1 : 8'd0);
                o_sum_g           <= o_sum_g + (i_roi_hit0 ? i_g0 : 8'd0) +
                                     (i_roi_hit1 ? i_g1 : 8'd0);
                o_sum_b           <= o_sum_b + (i_roi_hit0 ? i_b0 : 8'd0) +
                                     (i_roi_hit1 ? i_b1 : 8'd0);
                o_sum_y           <= o_sum_y + (i_roi_hit0 ? i_luma0 : 10'd0) +
                                     (i_roi_hit1 ? i_luma1 : 10'd0);
                o_red_area        <= o_red_area + i_red0 + i_red1;
                o_blue_area       <= o_blue_area + i_blue0 + i_blue1;
                o_yellow_area     <= o_yellow_area + i_yellow0 + i_yellow1;
                o_fg_area         <= o_fg_area + fg0 + fg1;
            end

            if (fg0 || fg1) begin
                if (!bbox_valid) begin
                    bbox_valid   <= 1'b1;
                    o_bbox_x_min <= fg_x_min;
                    o_bbox_x_max <= fg_x_max;
                    o_bbox_y_min <= i_y;
                    o_bbox_y_max <= i_y;
                end else begin
                    if (fg_x_min < o_bbox_x_min)
                        o_bbox_x_min <= fg_x_min;
                    if (fg_x_max > o_bbox_x_max)
                        o_bbox_x_max <= fg_x_max;
                    if (i_y < o_bbox_y_min)
                        o_bbox_y_min <= i_y;
                    if (i_y > o_bbox_y_max)
                        o_bbox_y_max <= i_y;
                end
            end
        end
    end

endmodule
