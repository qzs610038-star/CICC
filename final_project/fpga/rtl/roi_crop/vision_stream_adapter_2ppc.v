module vision_stream_adapter_2ppc (
    input              i_clk,
    input              i_rst_n,
    input              i_vs,
    input              i_hs,
    input              i_de,
    input              i_valid,
    input      [47:0]  i_rgb_2ppc,

    output             o_frame_start,
    output             o_pixel_valid0,
    output             o_pixel_valid1,
    output     [7:0]   o_r0,
    output     [7:0]   o_g0,
    output     [7:0]   o_b0,
    output     [7:0]   o_r1,
    output     [7:0]   o_g1,
    output     [7:0]   o_b1,
    output     [15:0]  o_x0,
    output     [15:0]  o_x1,
    output     [15:0]  o_y
);

    reg        vs_d;
    reg        hs_d;
    reg        de_d;
    reg        frame_started;
    reg [15:0] x_count;
    reg [15:0] y_count;

    wire pixel_active = i_de && i_valid;
    // DE is the primary line delimiter. A simultaneous or preceding HS edge
    // is also accepted so the same module can cross-check the Debayer timing.
    wire line_start = pixel_active && (!de_d || (i_hs && !hs_d));
    wire frame_start = i_vs && !vs_d;
    wire [15:0] current_y = frame_start ? 16'd0 :
                              ((line_start && frame_started) ? y_count + 16'd1 : y_count);

    // The Debayer output packs adjacent pixels as {B1,G1,R1,B0,G0,R0}.
    assign o_r0 = i_rgb_2ppc[7:0];
    assign o_g0 = i_rgb_2ppc[15:8];
    assign o_b0 = i_rgb_2ppc[23:16];
    assign o_r1 = i_rgb_2ppc[31:24];
    assign o_g1 = i_rgb_2ppc[39:32];
    assign o_b1 = i_rgb_2ppc[47:40];

    assign o_frame_start = frame_start;
    assign o_pixel_valid0 = pixel_active;
    assign o_pixel_valid1 = pixel_active;
    assign o_x0 = line_start ? 16'd0 : x_count;
    assign o_x1 = (line_start ? 16'd0 : x_count) + 16'd1;
    assign o_y = current_y;

    always @(posedge i_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            vs_d          <= 1'b0;
            hs_d          <= 1'b0;
            de_d          <= 1'b0;
            frame_started <= 1'b0;
            x_count       <= 16'd0;
            y_count       <= 16'd0;
        end else begin
            vs_d <= i_vs;
            hs_d <= i_hs;
            de_d <= pixel_active;

            if (frame_start) begin
                frame_started <= 1'b0;
                x_count       <= 16'd0;
                y_count       <= 16'd0;
            end else if (pixel_active) begin
                if (line_start) begin
                    x_count <= 16'd2;
                    if (frame_started)
                        y_count <= y_count + 16'd1;
                    else
                        frame_started <= 1'b1;
                end else begin
                    x_count <= x_count + 16'd2;
                end
            end else if (de_d) begin
                x_count <= 16'd0;
            end
        end
    end

endmodule
