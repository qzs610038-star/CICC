module vision_preprocess_channel (
    input              i_clk,
    input              i_rst_n,
    input              i_vs,
    input              i_hs,
    input              i_de,
    input              i_valid,
    input      [47:0]  i_rgb_2ppc,
    input              i_cfg_enable,
    input      [15:0]  i_cfg_roi_x0,
    input      [15:0]  i_cfg_roi_y0,
    input      [15:0]  i_cfg_roi_x1,
    input      [15:0]  i_cfg_roi_y1,
    input      [7:0]   i_cfg_bg_r,
    input      [7:0]   i_cfg_bg_g,
    input      [7:0]   i_cfg_bg_b,
    input      [7:0]   i_cfg_fg_diff_min,
    input      [9:0]   i_cfg_luma_min,
    input      [9:0]   i_cfg_luma_max,
    input      [7:0]   i_cfg_red_rg_min,
    input      [7:0]   i_cfg_red_rb_min,
    input      [7:0]   i_cfg_blue_bg_min,
    input      [7:0]   i_cfg_blue_br_min,
    input      [7:0]   i_cfg_yel_rb_min,
    input      [7:0]   i_cfg_yel_gb_min,
    input      [7:0]   i_cfg_yel_rg_delta_max,
    input              i_snapshot_ack,
    output             o_snapshot_valid,
    output     [15:0]  o_frame_id,
    output     [31:0]  o_roi_pixel_count,
    output     [31:0]  o_sum_r,
    output     [31:0]  o_sum_g,
    output     [31:0]  o_sum_b,
    output     [31:0]  o_sum_y,
    output     [31:0]  o_red_area,
    output     [31:0]  o_blue_area,
    output     [31:0]  o_yellow_area,
    output     [31:0]  o_fg_area,
    output     [31:0]  o_bbox_min,
    output     [31:0]  o_bbox_max,
    output     [31:0]  o_center,
    output     [31:0]  o_status,
    output     [31:0]  o_dropped_frames
);

    reg        cfg_enable;
    reg [15:0] cfg_roi_x0, cfg_roi_y0, cfg_roi_x1, cfg_roi_y1;
    reg [7:0]  cfg_bg_r, cfg_bg_g, cfg_bg_b, cfg_fg_diff_min;
    reg [9:0]  cfg_luma_min, cfg_luma_max;
    reg [7:0]  cfg_red_rg_min, cfg_red_rb_min;
    reg [7:0]  cfg_blue_bg_min, cfg_blue_br_min;
    reg [7:0]  cfg_yel_rb_min, cfg_yel_gb_min, cfg_yel_rg_delta_max;

    wire frame_start;
    wire pixel_valid0, pixel_valid1;
    wire [7:0] r0, g0, b0, r1, g1, b1;
    wire [15:0] x0, x1, y;
    wire roi_hit0, roi_hit1, roi_invalid;
    wire red0, blue0, yellow0, foreground0;
    wire red1, blue1, yellow1, foreground1;
    wire [9:0] luma0, luma1;
    wire frame_complete, frame_seen, bbox_valid, roi_invalid_seen;
    wire [31:0] roi_pixel_count, sum_r, sum_g, sum_b, sum_y;
    wire [31:0] red_area, blue_area, yellow_area, fg_area;
    wire [15:0] bbox_x_min, bbox_y_min, bbox_x_max, bbox_y_max;

    // Direct configuration is shadowed at the frame boundary. APB CDC is added later.
    always @(posedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            cfg_enable           <= 1'b0;
            cfg_roi_x0           <= 16'd0;
            cfg_roi_y0           <= 16'd0;
            cfg_roi_x1           <= 16'd0;
            cfg_roi_y1           <= 16'd0;
            cfg_bg_r             <= 8'd0;
            cfg_bg_g             <= 8'd0;
            cfg_bg_b             <= 8'd0;
            cfg_fg_diff_min      <= 8'd0;
            cfg_luma_min         <= 10'd0;
            cfg_luma_max         <= 10'd1023;
            cfg_red_rg_min       <= 8'd0;
            cfg_red_rb_min       <= 8'd0;
            cfg_blue_bg_min      <= 8'd0;
            cfg_blue_br_min      <= 8'd0;
            cfg_yel_rb_min       <= 8'd0;
            cfg_yel_gb_min       <= 8'd0;
            cfg_yel_rg_delta_max <= 8'hff;
        end else if (frame_start) begin
            cfg_enable           <= i_cfg_enable;
            cfg_roi_x0           <= i_cfg_roi_x0;
            cfg_roi_y0           <= i_cfg_roi_y0;
            cfg_roi_x1           <= i_cfg_roi_x1;
            cfg_roi_y1           <= i_cfg_roi_y1;
            cfg_bg_r             <= i_cfg_bg_r;
            cfg_bg_g             <= i_cfg_bg_g;
            cfg_bg_b             <= i_cfg_bg_b;
            cfg_fg_diff_min      <= i_cfg_fg_diff_min;
            cfg_luma_min         <= i_cfg_luma_min;
            cfg_luma_max         <= i_cfg_luma_max;
            cfg_red_rg_min       <= i_cfg_red_rg_min;
            cfg_red_rb_min       <= i_cfg_red_rb_min;
            cfg_blue_bg_min      <= i_cfg_blue_bg_min;
            cfg_blue_br_min      <= i_cfg_blue_br_min;
            cfg_yel_rb_min       <= i_cfg_yel_rb_min;
            cfg_yel_gb_min       <= i_cfg_yel_gb_min;
            cfg_yel_rg_delta_max <= i_cfg_yel_rg_delta_max;
        end
    end

    vision_stream_adapter_2ppc u_adapter (
        .i_clk(i_clk), .i_rst_n(i_rst_n), .i_vs(i_vs), .i_hs(i_hs),
        .i_de(i_de), .i_valid(i_valid), .i_rgb_2ppc(i_rgb_2ppc),
        .o_frame_start(frame_start), .o_pixel_valid0(pixel_valid0),
        .o_pixel_valid1(pixel_valid1), .o_r0(r0), .o_g0(g0), .o_b0(b0),
        .o_r1(r1), .o_g1(g1), .o_b1(b1), .o_x0(x0), .o_x1(x1), .o_y(y)
    );

    roi_window_2ppc u_roi (
        .i_enable(cfg_enable), .i_roi_x0(cfg_roi_x0), .i_roi_y0(cfg_roi_y0),
        .i_roi_x1(cfg_roi_x1), .i_roi_y1(cfg_roi_y1),
        .i_pixel_valid0(pixel_valid0), .i_pixel_valid1(pixel_valid1),
        .i_x0(x0), .i_x1(x1), .i_y(y), .o_roi_hit0(roi_hit0),
        .o_roi_hit1(roi_hit1), .o_roi_invalid(roi_invalid)
    );

    pixel_mask_2ppc u_mask (
        .i_r0(r0), .i_g0(g0), .i_b0(b0), .i_r1(r1), .i_g1(g1), .i_b1(b1),
        .i_roi_hit0(roi_hit0), .i_roi_hit1(roi_hit1),
        .i_bg_r(cfg_bg_r), .i_bg_g(cfg_bg_g), .i_bg_b(cfg_bg_b),
        .i_fg_diff_min(cfg_fg_diff_min), .i_luma_min(cfg_luma_min),
        .i_luma_max(cfg_luma_max), .i_red_rg_min(cfg_red_rg_min),
        .i_red_rb_min(cfg_red_rb_min), .i_blue_bg_min(cfg_blue_bg_min),
        .i_blue_br_min(cfg_blue_br_min), .i_yel_rb_min(cfg_yel_rb_min),
        .i_yel_gb_min(cfg_yel_gb_min), .i_yel_rg_delta_max(cfg_yel_rg_delta_max),
        .o_red0(red0), .o_blue0(blue0), .o_yellow0(yellow0),
        .o_foreground0(foreground0), .o_red1(red1), .o_blue1(blue1),
        .o_yellow1(yellow1), .o_foreground1(foreground1),
        .o_luma0(luma0), .o_luma1(luma1)
    );

    feature_accumulator_2ppc u_accumulator (
        .i_clk(i_clk), .i_rst_n(i_rst_n), .i_frame_start(frame_start),
        .i_roi_invalid(roi_invalid), .i_pixel_valid0(pixel_valid0),
        .i_pixel_valid1(pixel_valid1), .i_roi_hit0(roi_hit0), .i_roi_hit1(roi_hit1),
        .i_r0(r0), .i_g0(g0), .i_b0(b0), .i_r1(r1), .i_g1(g1), .i_b1(b1),
        .i_luma0(luma0), .i_luma1(luma1), .i_red0(red0), .i_blue0(blue0),
        .i_yellow0(yellow0), .i_foreground0(foreground0), .i_red1(red1),
        .i_blue1(blue1), .i_yellow1(yellow1), .i_foreground1(foreground1),
        .i_x0(x0), .i_x1(x1), .i_y(y), .o_frame_complete(frame_complete),
        .o_frame_seen(frame_seen), .o_bbox_valid(bbox_valid),
        .o_roi_invalid_seen(roi_invalid_seen), .o_roi_pixel_count(roi_pixel_count),
        .o_sum_r(sum_r), .o_sum_g(sum_g), .o_sum_b(sum_b), .o_sum_y(sum_y),
        .o_red_area(red_area), .o_blue_area(blue_area), .o_yellow_area(yellow_area),
        .o_fg_area(fg_area), .o_bbox_x_min(bbox_x_min), .o_bbox_y_min(bbox_y_min),
        .o_bbox_x_max(bbox_x_max), .o_bbox_y_max(bbox_y_max)
    );

    feature_snapshot u_snapshot (
        .i_clk(i_clk), .i_rst_n(i_rst_n), .i_frame_complete(frame_complete),
        .i_ack(i_snapshot_ack), .i_bbox_valid(bbox_valid),
        .i_roi_invalid(roi_invalid_seen), .i_roi_pixel_count(roi_pixel_count),
        .i_sum_r(sum_r), .i_sum_g(sum_g), .i_sum_b(sum_b), .i_sum_y(sum_y),
        .i_red_area(red_area), .i_blue_area(blue_area), .i_yellow_area(yellow_area),
        .i_fg_area(fg_area), .i_bbox_x_min(bbox_x_min), .i_bbox_y_min(bbox_y_min),
        .i_bbox_x_max(bbox_x_max), .i_bbox_y_max(bbox_y_max),
        .o_snapshot_valid(o_snapshot_valid), .o_frame_id(o_frame_id),
        .o_roi_pixel_count(o_roi_pixel_count), .o_sum_r(o_sum_r), .o_sum_g(o_sum_g),
        .o_sum_b(o_sum_b), .o_sum_y(o_sum_y), .o_red_area(o_red_area),
        .o_blue_area(o_blue_area), .o_yellow_area(o_yellow_area), .o_fg_area(o_fg_area),
        .o_bbox_min(o_bbox_min), .o_bbox_max(o_bbox_max), .o_center(o_center),
        .o_status(o_status), .o_dropped_frames(o_dropped_frames)
    );

endmodule
