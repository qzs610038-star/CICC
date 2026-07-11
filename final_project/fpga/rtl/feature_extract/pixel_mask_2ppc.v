module pixel_mask_2ppc (
    input       [7:0] i_r0,
    input       [7:0] i_g0,
    input       [7:0] i_b0,
    input       [7:0] i_r1,
    input       [7:0] i_g1,
    input       [7:0] i_b1,
    input             i_roi_hit0,
    input             i_roi_hit1,
    input       [7:0] i_bg_r,
    input       [7:0] i_bg_g,
    input       [7:0] i_bg_b,
    input       [7:0] i_fg_diff_min,
    input       [9:0] i_luma_min,
    input       [9:0] i_luma_max,
    input       [7:0] i_red_rg_min,
    input       [7:0] i_red_rb_min,
    input       [7:0] i_blue_bg_min,
    input       [7:0] i_blue_br_min,
    input       [7:0] i_yel_rb_min,
    input       [7:0] i_yel_gb_min,
    input       [7:0] i_yel_rg_delta_max,
    output            o_red0,
    output            o_blue0,
    output            o_yellow0,
    output            o_foreground0,
    output            o_red1,
    output            o_blue1,
    output            o_yellow1,
    output            o_foreground1,
    output      [9:0] o_luma0,
    output      [9:0] o_luma1
);

    function [7:0] abs_diff8;
        input [7:0] a;
        input [7:0] b;
        begin
            abs_diff8 = (a >= b) ? (a - b) : (b - a);
        end
    endfunction

    wire [9:0] luma0 = {2'b0, i_r0} + {2'b0, i_g0} + {2'b0, i_b0};
    wire [9:0] luma1 = {2'b0, i_r1} + {2'b0, i_g1} + {2'b0, i_b1};
    wire luma_ok0 = (luma0 >= i_luma_min) && (luma0 <= i_luma_max);
    wire luma_ok1 = (luma1 >= i_luma_min) && (luma1 <= i_luma_max);

    wire red0 = (i_r0 >= i_g0) && (i_r0 >= i_b0) &&
                (abs_diff8(i_r0, i_g0) >= i_red_rg_min) &&
                (abs_diff8(i_r0, i_b0) >= i_red_rb_min);
    wire red1 = (i_r1 >= i_g1) && (i_r1 >= i_b1) &&
                (abs_diff8(i_r1, i_g1) >= i_red_rg_min) &&
                (abs_diff8(i_r1, i_b1) >= i_red_rb_min);
    wire blue0 = (i_b0 >= i_g0) && (i_b0 >= i_r0) &&
                 (abs_diff8(i_b0, i_g0) >= i_blue_bg_min) &&
                 (abs_diff8(i_b0, i_r0) >= i_blue_br_min);
    wire blue1 = (i_b1 >= i_g1) && (i_b1 >= i_r1) &&
                 (abs_diff8(i_b1, i_g1) >= i_blue_bg_min) &&
                 (abs_diff8(i_b1, i_r1) >= i_blue_br_min);
    wire yellow0 = (i_r0 >= i_b0) && (i_g0 >= i_b0) &&
                   (abs_diff8(i_r0, i_b0) >= i_yel_rb_min) &&
                   (abs_diff8(i_g0, i_b0) >= i_yel_gb_min) &&
                   (abs_diff8(i_r0, i_g0) <= i_yel_rg_delta_max);
    wire yellow1 = (i_r1 >= i_b1) && (i_g1 >= i_b1) &&
                   (abs_diff8(i_r1, i_b1) >= i_yel_rb_min) &&
                   (abs_diff8(i_g1, i_b1) >= i_yel_gb_min) &&
                   (abs_diff8(i_r1, i_g1) <= i_yel_rg_delta_max);
    wire foreground0 = (abs_diff8(i_r0, i_bg_r) >= i_fg_diff_min) ||
                       (abs_diff8(i_g0, i_bg_g) >= i_fg_diff_min) ||
                       (abs_diff8(i_b0, i_bg_b) >= i_fg_diff_min);
    wire foreground1 = (abs_diff8(i_r1, i_bg_r) >= i_fg_diff_min) ||
                       (abs_diff8(i_g1, i_bg_g) >= i_fg_diff_min) ||
                       (abs_diff8(i_b1, i_bg_b) >= i_fg_diff_min);

    assign o_red0        = i_roi_hit0 && luma_ok0 && red0;
    assign o_blue0       = i_roi_hit0 && luma_ok0 && blue0;
    assign o_yellow0     = i_roi_hit0 && luma_ok0 && yellow0;
    assign o_foreground0 = i_roi_hit0 && luma_ok0 && foreground0;
    assign o_red1        = i_roi_hit1 && luma_ok1 && red1;
    assign o_blue1       = i_roi_hit1 && luma_ok1 && blue1;
    assign o_yellow1     = i_roi_hit1 && luma_ok1 && yellow1;
    assign o_foreground1 = i_roi_hit1 && luma_ok1 && foreground1;
    assign o_luma0 = luma0;
    assign o_luma1 = luma1;

endmodule
