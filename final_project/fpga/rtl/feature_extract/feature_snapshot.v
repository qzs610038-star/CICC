module feature_snapshot (
    input             i_clk,
    input             i_rst_n,
    input             i_frame_complete,
    input             i_ack,
    input             i_bbox_valid,
    input             i_roi_invalid,
    input      [31:0] i_roi_pixel_count,
    input      [31:0] i_sum_r,
    input      [31:0] i_sum_g,
    input      [31:0] i_sum_b,
    input      [31:0] i_sum_y,
    input      [31:0] i_red_area,
    input      [31:0] i_blue_area,
    input      [31:0] i_yellow_area,
    input      [31:0] i_fg_area,
    input      [15:0] i_bbox_x_min,
    input      [15:0] i_bbox_y_min,
    input      [15:0] i_bbox_x_max,
    input      [15:0] i_bbox_y_max,
    output reg        o_snapshot_valid,
    output reg [15:0] o_frame_id,
    output reg [31:0] o_roi_pixel_count,
    output reg [31:0] o_sum_r,
    output reg [31:0] o_sum_g,
    output reg [31:0] o_sum_b,
    output reg [31:0] o_sum_y,
    output reg [31:0] o_red_area,
    output reg [31:0] o_blue_area,
    output reg [31:0] o_yellow_area,
    output reg [31:0] o_fg_area,
    output reg [31:0] o_bbox_min,
    output reg [31:0] o_bbox_max,
    output reg [31:0] o_center,
    output reg [31:0] o_status,
    output reg [31:0] o_dropped_frames
);

    wire [15:0] center_x = (i_bbox_x_min + i_bbox_x_max) >> 1;
    wire [15:0] center_y = (i_bbox_y_min + i_bbox_y_max) >> 1;

    always @(posedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            o_snapshot_valid  <= 1'b0;
            o_frame_id        <= 16'd0;
            o_roi_pixel_count <= 32'd0;
            o_sum_r           <= 32'd0;
            o_sum_g           <= 32'd0;
            o_sum_b           <= 32'd0;
            o_sum_y           <= 32'd0;
            o_red_area        <= 32'd0;
            o_blue_area       <= 32'd0;
            o_yellow_area     <= 32'd0;
            o_fg_area         <= 32'd0;
            o_bbox_min        <= 32'd0;
            o_bbox_max        <= 32'd0;
            o_center          <= 32'd0;
            o_status          <= 32'd0;
            o_dropped_frames  <= 32'd0;
        end else begin
            if (i_ack)
                o_snapshot_valid <= 1'b0;

            if (i_frame_complete) begin
                if (!o_snapshot_valid || i_ack) begin
                    o_snapshot_valid  <= 1'b1;
                    o_frame_id        <= o_frame_id + 16'd1;
                    o_roi_pixel_count <= i_roi_pixel_count;
                    o_sum_r           <= i_sum_r;
                    o_sum_g           <= i_sum_g;
                    o_sum_b           <= i_sum_b;
                    o_sum_y           <= i_sum_y;
                    o_red_area        <= i_red_area;
                    o_blue_area       <= i_blue_area;
                    o_yellow_area     <= i_yellow_area;
                    o_fg_area         <= i_fg_area;
                    o_bbox_min        <= i_bbox_valid ? {i_bbox_y_min, i_bbox_x_min} : 32'd0;
                    o_bbox_max        <= i_bbox_valid ? {i_bbox_y_max, i_bbox_x_max} : 32'd0;
                    o_center          <= i_bbox_valid ? {center_y, center_x} : 32'd0;
                    o_status          <= {30'd0, i_roi_invalid, !i_bbox_valid};
                end else begin
                    o_dropped_frames <= o_dropped_frames + 32'd1;
                end
            end
        end
    end

endmodule
