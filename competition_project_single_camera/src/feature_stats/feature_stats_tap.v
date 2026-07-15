// Read-only ch0 RGB feature accumulator. It never drives video-path signals.
module feature_stats_tap #(
    parameter X_WIDTH = 11,
    parameter Y_WIDTH = 11,
    parameter COUNT_WIDTH = 21,
    parameter LUMA_WIDTH = 31,
    parameter FRAME_WIDTH = 1920,
    parameter FRAME_HEIGHT = 1080
) (
    input  wire                   i_clk,
    input  wire                   i_rst_n,
    input  wire                   i_capture_enable,
    input  wire                   i_frame_stable,
    input  wire                   i_diag_active,
    input  wire                   i_vs,
    input  wire                   i_de,
    input  wire [47:0]            i_rgb_data,

    input  wire [X_WIDTH-1:0]     i_roi_x0,
    input  wire [X_WIDTH-1:0]     i_roi_x1,
    input  wire [Y_WIDTH-1:0]     i_roi_y0,
    input  wire [Y_WIDTH-1:0]     i_roi_y1,
    input  wire [7:0]             i_bg_r,
    input  wire [7:0]             i_bg_g,
    input  wire [7:0]             i_bg_b,
    input  wire [9:0]             i_foreground_delta,
    input  wire [7:0]             i_red_min,
    input  wire [7:0]             i_blue_min,
    input  wire [7:0]             i_yellow_min,
    input  wire [7:0]             i_color_delta,
    input  wire [15:0]            i_config_seq,

    input  wire                   i_ack_valid,
    input  wire [15:0]            i_ack_frame_id,

    output reg                    o_snapshot_valid,
    output reg [15:0]             o_frame_id,
    output reg [15:0]             o_config_seq,
    output reg [COUNT_WIDTH-1:0] o_red_area,
    output reg [COUNT_WIDTH-1:0] o_blue_area,
    output reg [COUNT_WIDTH-1:0] o_yellow_area,
    output reg [COUNT_WIDTH-1:0] o_foreground_area,
    output reg [COUNT_WIDTH-1:0] o_roi_pixel_count,
    output reg [LUMA_WIDTH-1:0]  o_sum_luma,
    output reg [X_WIDTH-1:0]     o_bbox_width,
    output reg [Y_WIDTH-1:0]     o_bbox_height,
    output reg [7:0]             o_source_flags
);

    localparam [7:0] FLAG_FRAME_STABLE     = 8'h01;
    localparam [7:0] FLAG_ROI_VALID        = 8'h02;
    localparam [7:0] FLAG_STATS_VALID      = 8'h04;
    localparam [7:0] FLAG_DIAG_ACTIVE      = 8'h08;
    localparam [7:0] FLAG_COUNTER_OVERFLOW = 8'h10;
    localparam [7:0] FLAG_SNAPSHOT_OVERRUN = 8'h20;
    localparam [7:0] FLAG_SOURCE_CH0       = 8'h40;

    reg vs_d;
    reg de_d;
    reg in_frame;
    reg seen_line;
    reg [X_WIDTH-1:0] x_next;
    reg [Y_WIDTH-1:0] y_current;
    reg [15:0] work_config_seq;
    reg work_overflow;
    reg [COUNT_WIDTH-1:0] work_red_area;
    reg [COUNT_WIDTH-1:0] work_blue_area;
    reg [COUNT_WIDTH-1:0] work_yellow_area;
    reg [COUNT_WIDTH-1:0] work_foreground_area;
    reg [COUNT_WIDTH-1:0] work_roi_pixel_count;
    reg [LUMA_WIDTH-1:0] work_sum_luma;
    reg [X_WIDTH-1:0] work_x_min;
    reg [X_WIDTH-1:0] work_x_max;
    reg [Y_WIDTH-1:0] work_y_min;
    reg [Y_WIDTH-1:0] work_y_max;

    wire frame_start = ~vs_d & i_vs;
    wire frame_end = vs_d & ~i_vs;
    wire roi_cfg_valid = i_roi_x0 <= i_roi_x1 && i_roi_y0 <= i_roi_y1 &&
                         i_roi_x1 < FRAME_WIDTH && i_roi_y1 < FRAME_HEIGHT;
    wire [X_WIDTH-1:0] pixel_x = de_d ? x_next : {X_WIDTH{1'b0}};
    wire [Y_WIDTH-1:0] pixel_y = de_d ? y_current :
                                  (seen_line ? y_current + 1'b1 : {Y_WIDTH{1'b0}});
    wire pixel0_in_roi = roi_cfg_valid && pixel_x >= i_roi_x0 && pixel_x <= i_roi_x1 &&
                         pixel_y >= i_roi_y0 && pixel_y <= i_roi_y1;
    wire pixel1_in_roi = roi_cfg_valid && (pixel_x + 1'b1) >= i_roi_x0 &&
                         (pixel_x + 1'b1) <= i_roi_x1 && pixel_y >= i_roi_y0 &&
                         pixel_y <= i_roi_y1;

    wire [7:0] p0_r = i_rgb_data[47:40];
    wire [7:0] p0_g = i_rgb_data[39:32];
    wire [7:0] p0_b = i_rgb_data[31:24];
    wire [7:0] p1_r = i_rgb_data[23:16];
    wire [7:0] p1_g = i_rgb_data[15:8];
    wire [7:0] p1_b = i_rgb_data[7:0];

    function [8:0] abs_diff;
        input [7:0] a;
        input [7:0] b;
        begin
            abs_diff = a >= b ? a - b : b - a;
        end
    endfunction

    function is_red;
        input [7:0] r;
        input [7:0] g;
        input [7:0] b;
        reg [8:0] g_limit;
        reg [8:0] b_limit;
        begin
            g_limit = {1'b0, g} + i_color_delta;
            b_limit = {1'b0, b} + i_color_delta;
            is_red = r >= i_red_min && {1'b0, r} >= g_limit &&
                     {1'b0, r} >= b_limit;
        end
    endfunction

    function is_blue;
        input [7:0] r;
        input [7:0] g;
        input [7:0] b;
        reg [8:0] r_limit;
        reg [8:0] g_limit;
        begin
            r_limit = {1'b0, r} + i_color_delta;
            g_limit = {1'b0, g} + i_color_delta;
            is_blue = b >= i_blue_min && {1'b0, b} >= r_limit &&
                      {1'b0, b} >= g_limit;
        end
    endfunction

    function is_yellow;
        input [7:0] r;
        input [7:0] g;
        input [7:0] b;
        reg [7:0] rg_min;
        reg [8:0] b_limit;
        begin
            rg_min = r < g ? r : g;
            b_limit = {1'b0, b} + i_color_delta;
            is_yellow = r >= i_yellow_min && g >= i_yellow_min &&
                        abs_diff(r, g) <= i_color_delta &&
                        {1'b0, rg_min} >= b_limit;
        end
    endfunction

    function is_foreground;
        input [7:0] r;
        input [7:0] g;
        input [7:0] b;
        reg [10:0] delta;
        begin
            delta = {2'd0, abs_diff(r, i_bg_r)} +
                    {2'd0, abs_diff(g, i_bg_g)} +
                    {2'd0, abs_diff(b, i_bg_b)};
            is_foreground = delta >= i_foreground_delta;
        end
    endfunction

    wire p0_red = pixel0_in_roi && is_red(p0_r, p0_g, p0_b);
    wire p1_red = pixel1_in_roi && is_red(p1_r, p1_g, p1_b);
    wire p0_blue = pixel0_in_roi && is_blue(p0_r, p0_g, p0_b);
    wire p1_blue = pixel1_in_roi && is_blue(p1_r, p1_g, p1_b);
    wire p0_yellow = pixel0_in_roi && is_yellow(p0_r, p0_g, p0_b);
    wire p1_yellow = pixel1_in_roi && is_yellow(p1_r, p1_g, p1_b);
    wire p0_foreground = pixel0_in_roi && is_foreground(p0_r, p0_g, p0_b);
    wire p1_foreground = pixel1_in_roi && is_foreground(p1_r, p1_g, p1_b);

    wire [1:0] roi_increment = pixel0_in_roi + pixel1_in_roi;
    wire [1:0] red_increment = p0_red + p1_red;
    wire [1:0] blue_increment = p0_blue + p1_blue;
    wire [1:0] yellow_increment = p0_yellow + p1_yellow;
    wire [1:0] foreground_increment = p0_foreground + p1_foreground;
    wire [10:0] luma_increment = (pixel0_in_roi ? ({3'd0, p0_r} + {3'd0, p0_g} + {3'd0, p0_b}) : 11'd0) +
                                 (pixel1_in_roi ? ({3'd0, p1_r} + {3'd0, p1_g} + {3'd0, p1_b}) : 11'd0);

    task clear_work;
        begin
            work_overflow <= 1'b0;
            work_red_area <= {COUNT_WIDTH{1'b0}};
            work_blue_area <= {COUNT_WIDTH{1'b0}};
            work_yellow_area <= {COUNT_WIDTH{1'b0}};
            work_foreground_area <= {COUNT_WIDTH{1'b0}};
            work_roi_pixel_count <= {COUNT_WIDTH{1'b0}};
            work_sum_luma <= {LUMA_WIDTH{1'b0}};
            work_x_min <= {X_WIDTH{1'b1}};
            work_x_max <= {X_WIDTH{1'b0}};
            work_y_min <= {Y_WIDTH{1'b1}};
            work_y_max <= {Y_WIDTH{1'b0}};
        end
    endtask

    always @(posedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            vs_d <= 1'b0;
            de_d <= 1'b0;
            in_frame <= 1'b0;
            seen_line <= 1'b0;
            x_next <= {X_WIDTH{1'b0}};
            y_current <= {Y_WIDTH{1'b0}};
            work_config_seq <= 16'd0;
            clear_work();
            o_snapshot_valid <= 1'b0;
            o_frame_id <= 16'd0;
            o_config_seq <= 16'd0;
            o_red_area <= {COUNT_WIDTH{1'b0}};
            o_blue_area <= {COUNT_WIDTH{1'b0}};
            o_yellow_area <= {COUNT_WIDTH{1'b0}};
            o_foreground_area <= {COUNT_WIDTH{1'b0}};
            o_roi_pixel_count <= {COUNT_WIDTH{1'b0}};
            o_sum_luma <= {LUMA_WIDTH{1'b0}};
            o_bbox_width <= {X_WIDTH{1'b0}};
            o_bbox_height <= {Y_WIDTH{1'b0}};
            o_source_flags <= 8'd0;
        end else begin
            vs_d <= i_vs;
            de_d <= i_de;

            if (i_ack_valid && o_snapshot_valid && i_ack_frame_id == o_frame_id)
                o_snapshot_valid <= 1'b0;

            if (frame_start) begin
                in_frame <= 1'b1;
                seen_line <= 1'b0;
                x_next <= {X_WIDTH{1'b0}};
                y_current <= {Y_WIDTH{1'b0}};
                work_config_seq <= i_config_seq;
                clear_work();
            end else if (in_frame && i_de) begin
                if (!de_d) begin
                    x_next <= 2'd2;
                    if (seen_line)
                        y_current <= y_current + 1'b1;
                    else
                        seen_line <= 1'b1;
                end else begin
                    x_next <= x_next + 2'd2;
                end

                if (roi_increment > ({COUNT_WIDTH{1'b1}} - work_roi_pixel_count))
                    work_overflow <= 1'b1;
                else
                    work_roi_pixel_count <= work_roi_pixel_count + roi_increment;
                if (red_increment > ({COUNT_WIDTH{1'b1}} - work_red_area))
                    work_overflow <= 1'b1;
                else
                    work_red_area <= work_red_area + red_increment;
                if (blue_increment > ({COUNT_WIDTH{1'b1}} - work_blue_area))
                    work_overflow <= 1'b1;
                else
                    work_blue_area <= work_blue_area + blue_increment;
                if (yellow_increment > ({COUNT_WIDTH{1'b1}} - work_yellow_area))
                    work_overflow <= 1'b1;
                else
                    work_yellow_area <= work_yellow_area + yellow_increment;
                if (foreground_increment > ({COUNT_WIDTH{1'b1}} - work_foreground_area))
                    work_overflow <= 1'b1;
                else
                    work_foreground_area <= work_foreground_area + foreground_increment;
                if (luma_increment > ({LUMA_WIDTH{1'b1}} - work_sum_luma))
                    work_overflow <= 1'b1;
                else
                    work_sum_luma <= work_sum_luma + luma_increment;

                if (p0_foreground || p1_foreground) begin
                    if (work_foreground_area == {COUNT_WIDTH{1'b0}}) begin
                        work_x_min <= p0_foreground ? pixel_x : pixel_x + 1'b1;
                        work_x_max <= p1_foreground ? pixel_x + 1'b1 : pixel_x;
                        work_y_min <= pixel_y;
                        work_y_max <= pixel_y;
                    end else begin
                        if (p0_foreground && pixel_x < work_x_min)
                            work_x_min <= pixel_x;
                        if (p1_foreground && (pixel_x + 1'b1) < work_x_min)
                            work_x_min <= pixel_x + 1'b1;
                        if (p1_foreground)
                            work_x_max <= pixel_x + 1'b1;
                        else if (p0_foreground)
                            work_x_max <= pixel_x;
                        if (pixel_y < work_y_min)
                            work_y_min <= pixel_y;
                        if (pixel_y > work_y_max)
                            work_y_max <= pixel_y;
                    end
                end
            end

            if (frame_end) begin
                in_frame <= 1'b0;
                if (o_snapshot_valid) begin
                    o_snapshot_valid <= 1'b0;
                    o_source_flags <= FLAG_SNAPSHOT_OVERRUN | FLAG_SOURCE_CH0;
                end else begin
                    o_frame_id <= o_frame_id + 1'b1;
                    o_config_seq <= work_config_seq;
                    o_red_area <= work_red_area;
                    o_blue_area <= work_blue_area;
                    o_yellow_area <= work_yellow_area;
                    o_foreground_area <= work_foreground_area;
                    o_roi_pixel_count <= work_roi_pixel_count;
                    o_sum_luma <= work_sum_luma;
                    o_bbox_width <= work_foreground_area == {COUNT_WIDTH{1'b0}} ? {X_WIDTH{1'b0}} :
                                    work_x_max - work_x_min + 1'b1;
                    o_bbox_height <= work_foreground_area == {COUNT_WIDTH{1'b0}} ? {Y_WIDTH{1'b0}} :
                                     work_y_max - work_y_min + 1'b1;
                    o_source_flags <= (i_frame_stable ? FLAG_FRAME_STABLE : 8'd0) |
                                      (roi_cfg_valid && work_roi_pixel_count != {COUNT_WIDTH{1'b0}} ? FLAG_ROI_VALID : 8'd0) |
                                      (i_diag_active ? FLAG_DIAG_ACTIVE : 8'd0) |
                                      (work_overflow ? FLAG_COUNTER_OVERFLOW : 8'd0) |
                                      FLAG_SOURCE_CH0;
                    if (i_capture_enable && i_frame_stable && roi_cfg_valid &&
                        work_roi_pixel_count != {COUNT_WIDTH{1'b0}} && !i_diag_active && !work_overflow) begin
                        o_snapshot_valid <= 1'b1;
                        o_source_flags <= (i_frame_stable ? FLAG_FRAME_STABLE : 8'd0) |
                                          FLAG_ROI_VALID | FLAG_STATS_VALID | FLAG_SOURCE_CH0;
                    end else begin
                        o_snapshot_valid <= 1'b0;
                    end
                end
            end
        end
    end
endmodule
