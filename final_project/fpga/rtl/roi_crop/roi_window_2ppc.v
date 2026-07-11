module roi_window_2ppc (
    input             i_enable,
    input      [15:0] i_roi_x0,
    input      [15:0] i_roi_y0,
    input      [15:0] i_roi_x1,
    input      [15:0] i_roi_y1,
    input             i_pixel_valid0,
    input             i_pixel_valid1,
    input      [15:0] i_x0,
    input      [15:0] i_x1,
    input      [15:0] i_y,
    output            o_roi_hit0,
    output            o_roi_hit1,
    output            o_roi_invalid
);

    wire roi_valid = (i_roi_x1 > i_roi_x0) && (i_roi_y1 > i_roi_y0);

    assign o_roi_invalid = i_enable && !roi_valid;
    assign o_roi_hit0 = i_enable && roi_valid && i_pixel_valid0 &&
                        (i_x0 >= i_roi_x0) && (i_x0 < i_roi_x1) &&
                        (i_y >= i_roi_y0) && (i_y < i_roi_y1);
    assign o_roi_hit1 = i_enable && roi_valid && i_pixel_valid1 &&
                        (i_x1 >= i_roi_x0) && (i_x1 < i_roi_x1) &&
                        (i_y >= i_roi_y0) && (i_y < i_roi_y1);

endmodule
