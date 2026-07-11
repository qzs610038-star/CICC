`timescale 1ns / 1ps

module tb_vision_preprocess_channel;
    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg vs = 1'b0;
    reg hs = 1'b0;
    reg de = 1'b0;
    reg valid = 1'b0;
    reg [47:0] rgb_2ppc = 48'd0;
    reg cfg_enable = 1'b1;
    reg [15:0] cfg_roi_x0 = 16'd1;
    reg [15:0] cfg_roi_y0 = 16'd0;
    reg [15:0] cfg_roi_x1 = 16'd5;
    reg [15:0] cfg_roi_y1 = 16'd2;
    reg [7:0] cfg_bg_r = 8'd10;
    reg [7:0] cfg_bg_g = 8'd20;
    reg [7:0] cfg_bg_b = 8'd30;
    reg [7:0] cfg_fg_diff_min = 8'd40;
    reg [9:0] cfg_luma_min = 10'd0;
    reg [9:0] cfg_luma_max = 10'd765;
    reg [7:0] cfg_red_rg_min = 8'd80;
    reg [7:0] cfg_red_rb_min = 8'd80;
    reg [7:0] cfg_blue_bg_min = 8'd80;
    reg [7:0] cfg_blue_br_min = 8'd80;
    reg [7:0] cfg_yel_rb_min = 8'd100;
    reg [7:0] cfg_yel_gb_min = 8'd100;
    reg [7:0] cfg_yel_rg_delta_max = 8'd20;
    reg snapshot_ack = 1'b0;

    wire snapshot_valid;
    wire [15:0] frame_id;
    wire [31:0] roi_pixel_count, sum_r, sum_g, sum_b, sum_y;
    wire [31:0] red_area, blue_area, yellow_area, fg_area;
    wire [31:0] bbox_min, bbox_max, center, status, dropped_frames;

    always #5 clk = ~clk;

    vision_preprocess_channel dut (
        .i_clk(clk), .i_rst_n(rst_n), .i_vs(vs), .i_hs(hs), .i_de(de),
        .i_valid(valid), .i_rgb_2ppc(rgb_2ppc), .i_cfg_enable(cfg_enable),
        .i_cfg_roi_x0(cfg_roi_x0), .i_cfg_roi_y0(cfg_roi_y0),
        .i_cfg_roi_x1(cfg_roi_x1), .i_cfg_roi_y1(cfg_roi_y1),
        .i_cfg_bg_r(cfg_bg_r), .i_cfg_bg_g(cfg_bg_g), .i_cfg_bg_b(cfg_bg_b),
        .i_cfg_fg_diff_min(cfg_fg_diff_min), .i_cfg_luma_min(cfg_luma_min),
        .i_cfg_luma_max(cfg_luma_max), .i_cfg_red_rg_min(cfg_red_rg_min),
        .i_cfg_red_rb_min(cfg_red_rb_min), .i_cfg_blue_bg_min(cfg_blue_bg_min),
        .i_cfg_blue_br_min(cfg_blue_br_min), .i_cfg_yel_rb_min(cfg_yel_rb_min),
        .i_cfg_yel_gb_min(cfg_yel_gb_min), .i_cfg_yel_rg_delta_max(cfg_yel_rg_delta_max),
        .i_snapshot_ack(snapshot_ack), .o_snapshot_valid(snapshot_valid),
        .o_frame_id(frame_id), .o_roi_pixel_count(roi_pixel_count), .o_sum_r(sum_r),
        .o_sum_g(sum_g), .o_sum_b(sum_b), .o_sum_y(sum_y), .o_red_area(red_area),
        .o_blue_area(blue_area), .o_yellow_area(yellow_area), .o_fg_area(fg_area),
        .o_bbox_min(bbox_min), .o_bbox_max(bbox_max), .o_center(center),
        .o_status(status), .o_dropped_frames(dropped_frames)
    );

    task begin_frame;
        begin
            @(negedge clk);
            vs <= 1'b1;
            @(negedge clk);
            vs <= 1'b0;
        end
    endtask

    task send_line;
        begin
            @(negedge clk);
            hs <= 1'b1;
            de <= 1'b1;
            valid <= 1'b1;
            // x=0/1: background and red (or background on the second line).
            rgb_2ppc <= {8'd20, 8'd20, 8'd200, 8'd30, 8'd20, 8'd10};
            @(negedge clk);
            // x=2/3: blue and yellow (or black and red on the second line).
            rgb_2ppc <= {8'd10, 8'd190, 8'd200, 8'd200, 8'd20, 8'd10};
            @(negedge clk);
            // x=4/5: white and background.
            rgb_2ppc <= {8'd30, 8'd20, 8'd10, 8'd240, 8'd240, 8'd240};
            @(negedge clk);
            de <= 1'b0;
            valid <= 1'b0;
            hs <= 1'b0;
        end
    endtask

    task send_second_line;
        begin
            @(negedge clk);
            hs <= 1'b1;
            de <= 1'b1;
            valid <= 1'b1;
            rgb_2ppc <= {8'd30, 8'd20, 8'd10, 8'd30, 8'd20, 8'd10};
            @(negedge clk);
            rgb_2ppc <= {8'd20, 8'd20, 8'd210, 8'd0, 8'd0, 8'd0};
            @(negedge clk);
            rgb_2ppc <= {8'd30, 8'd20, 8'd10, 8'd30, 8'd20, 8'd10};
            @(negedge clk);
            de <= 1'b0;
            valid <= 1'b0;
            hs <= 1'b0;
        end
    endtask

    task send_test_frame;
        begin
            begin_frame;
            send_line;
            send_second_line;
        end
    endtask

    task expect32;
        input [8*24-1:0] name;
        input [31:0] got;
        input [31:0] expected;
        begin
            if (got !== expected) begin
                $display("FAIL %0s got=%0d expected=%0d", name, got, expected);
                $finish(1);
            end
        end
    endtask

    initial begin
        repeat (3) @(negedge clk);
        rst_n <= 1'b1;

        // Frame 1 accumulates. Frame 2 start atomically publishes its snapshot.
        send_test_frame;
        begin_frame;
        #1;
        if (!snapshot_valid) begin
            $display("FAIL snapshot was not published");
            $finish(1);
        end
        expect32("frame_id", {16'd0, frame_id}, 32'd1);
        expect32("roi_pixel_count", roi_pixel_count, 32'd8);
        expect32("sum_r", sum_r, 32'd880);
        expect32("sum_g", sum_g, 32'd530);
        expect32("sum_b", sum_b, 32'd550);
        expect32("sum_y", sum_y, 32'd1960);
        expect32("red_area", red_area, 32'd2);
        expect32("blue_area", blue_area, 32'd1);
        expect32("yellow_area", yellow_area, 32'd1);
        expect32("fg_area", fg_area, 32'd5);
        expect32("bbox_min", bbox_min, 32'h0000_0001);
        expect32("bbox_max", bbox_max, 32'h0001_0004);
        expect32("center", center, 32'h0000_0002);
        expect32("status", status, 32'd0);

        // Without ack, the next complete frame must not tear the existing snapshot.
        send_test_frame;
        begin_frame;
        #1;
        expect32("dropped_frames", dropped_frames, 32'd1);
        expect32("held_frame_id", {16'd0, frame_id}, 32'd1);

        // Ack clears the snapshot. A valid but empty ROI frame must still publish.
        @(negedge clk);
        snapshot_ack <= 1'b1;
        @(negedge clk);
        snapshot_ack <= 1'b0;
        cfg_roi_x0 <= 16'd6;
        cfg_roi_x1 <= 16'd7;
        send_test_frame;
        begin_frame;
        #1;
        if (!snapshot_valid) begin
            $display("FAIL empty ROI frame was not published");
            $finish(1);
        end
        expect32("empty_roi_count", roi_pixel_count, 32'd0);
        expect32("empty_fg_area", fg_area, 32'd0);
        expect32("empty_bbox_min", bbox_min, 32'd0);
        expect32("empty_status", status, 32'd1);

        $display("PASS tb_vision_preprocess_channel");
        $finish(0);
    end
endmodule
