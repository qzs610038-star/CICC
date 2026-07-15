`timescale 1ns/1ps

module feature_stats_tap_tb;
    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg capture_enable = 1'b1;
    reg frame_stable = 1'b1;
    reg diag_active = 1'b0;
    reg vs = 1'b0;
    reg de = 1'b0;
    reg [47:0] rgb_data = 48'd0;
    reg [2:0] roi_x0 = 0, roi_x1 = 3, roi_y0 = 0, roi_y1 = 0;
    reg [7:0] bg_r = 0, bg_g = 0, bg_b = 0;
    reg [9:0] foreground_delta = 10;
    reg [7:0] red_min = 20, blue_min = 20, yellow_min = 20, color_delta = 10;
    reg [15:0] config_seq = 16'h22;
    reg ack_valid = 0;
    reg [15:0] ack_frame_id = 0;
    wire snapshot_valid;
    wire [15:0] frame_id, snapshot_config_seq;
    wire [2:0] red_area, blue_area, yellow_area, foreground_area, roi_pixel_count;
    wire [11:0] sum_luma;
    wire [2:0] bbox_width, bbox_height;
    wire [7:0] source_flags;

    always #5 clk = ~clk;

    feature_stats_tap #(
        .X_WIDTH(3), .Y_WIDTH(3), .COUNT_WIDTH(3), .LUMA_WIDTH(12),
        .FRAME_WIDTH(8), .FRAME_HEIGHT(2)
    ) dut (
        .i_clk(clk), .i_rst_n(rst_n), .i_capture_enable(capture_enable),
        .i_frame_stable(frame_stable), .i_diag_active(diag_active), .i_vs(vs),
        .i_de(de), .i_rgb_data(rgb_data), .i_roi_x0(roi_x0), .i_roi_x1(roi_x1),
        .i_roi_y0(roi_y0), .i_roi_y1(roi_y1), .i_bg_r(bg_r), .i_bg_g(bg_g),
        .i_bg_b(bg_b), .i_foreground_delta(foreground_delta), .i_red_min(red_min),
        .i_blue_min(blue_min), .i_yellow_min(yellow_min), .i_color_delta(color_delta),
        .i_config_seq(config_seq), .i_ack_valid(ack_valid), .i_ack_frame_id(ack_frame_id),
        .o_snapshot_valid(snapshot_valid), .o_frame_id(frame_id),
        .o_config_seq(snapshot_config_seq), .o_red_area(red_area), .o_blue_area(blue_area),
        .o_yellow_area(yellow_area), .o_foreground_area(foreground_area),
        .o_roi_pixel_count(roi_pixel_count), .o_sum_luma(sum_luma),
        .o_bbox_width(bbox_width), .o_bbox_height(bbox_height), .o_source_flags(source_flags)
    );

    task line_two_pairs;
        input [23:0] p0;
        input [23:0] p1;
        input [23:0] p2;
        input [23:0] p3;
        begin
            rgb_data = {p0, p1}; de = 1'b1; @(posedge clk);
            rgb_data = {p2, p3}; @(posedge clk);
            de = 1'b0; @(posedge clk);
        end
    endtask

    task frame_begin;
        begin vs = 1'b1; @(posedge clk); end
    endtask

    task frame_end;
        begin vs = 1'b0; @(posedge clk); #1; end
    endtask

    task expect;
        input condition;
        input [8*48-1:0] message;
        begin
            if (!condition) begin
                $display("FAIL: %0s", message);
                $finish;
            end
        end
    endtask

    initial begin
        repeat (2) @(posedge clk);
        rst_n = 1'b1;

        // One 4-pixel line: red, blue, yellow, and neutral foreground.
        frame_begin;
        line_two_pairs(24'hc80000, 24'h0000c8, 24'hc8c800, 24'h202020);
        frame_end;
        expect(snapshot_valid, "valid snapshot");
        expect(frame_id == 16'd1 && snapshot_config_seq == 16'h22, "frame/config id");
        expect(roi_pixel_count == 4 && red_area == 1 && blue_area == 1 && yellow_area == 1,
               "color counts");
        expect(foreground_area == 4 && bbox_width == 4 && bbox_height == 1, "bbox counts");
        expect(source_flags == 8'h47, "valid source flags");

        // Wrong ACK must retain the snapshot; matching ACK releases it.
        ack_frame_id = 16'h55; ack_valid = 1'b1; @(posedge clk); ack_valid = 1'b0;
        expect(snapshot_valid, "wrong ack retained");
        ack_frame_id = frame_id; ack_valid = 1'b1; @(posedge clk); ack_valid = 1'b0;
        expect(!snapshot_valid, "matching ack releases");

        // Diagnostic and counter overflow both fail closed.
        diag_active = 1'b1; frame_begin;
        line_two_pairs(24'hc80000, 24'h0000c8, 24'hc80000, 24'h0000c8); frame_end;
        expect(!snapshot_valid && source_flags[3], "diagnostic rejected");
        diag_active = 1'b0;
        foreground_delta = 0; roi_x1 = 7; roi_y1 = 1; frame_begin;
        line_two_pairs(24'hc80000, 24'hc80000, 24'hc80000, 24'hc80000);
        line_two_pairs(24'hc80000, 24'hc80000, 24'hc80000, 24'hc80000);
        frame_end;
        expect(!snapshot_valid && source_flags[4], "overflow rejected");

        $display("PASS feature_stats_tap_tb");
        $finish;
    end
endmodule
