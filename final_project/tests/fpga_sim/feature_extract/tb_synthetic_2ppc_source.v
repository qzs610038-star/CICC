`timescale 1ns / 1ps

module tb_synthetic_2ppc_source;
    reg clk = 1'b0;
    reg rst_n = 1'b0;
    wire vs, hs, de, valid;
    wire [47:0] rgb_2ppc;
    integer active_beats;
    integer frame_count;
    reg [47:0] object_beat;
    reg [47:0] expected_object_beat;
    reg vs_d;

    always #5 clk = ~clk;

    // Small geometry makes the full five-color cycle practical in simulation.
    synthetic_2ppc_source #(
        .FRAME_WIDTH(12), .FRAME_HEIGHT(6),
        .H_SYNC(1), .H_BACK(1), .H_FRONT(1),
        .V_SYNC(1), .V_BACK(1), .V_FRONT(1),
        .FRAMES_PER_COLOR(1)
    ) dut (
        .i_clk(clk), .i_rst_n(rst_n), .o_vs(vs), .o_hs(hs),
        .o_de(de), .o_valid(valid), .o_rgb_2ppc(rgb_2ppc)
    );

    always @(posedge clk) begin
        if (vs && !vs_d) begin
            frame_count = frame_count + 1;
            active_beats = 0;
        end
        if (de || valid) begin
            if (!(de && valid)) begin
                $display("FAIL de/valid mismatch");
                $finish(1);
            end
            active_beats = active_beats + 1;
            // For 12x6, the first object beat is active beat 15. Check one
            // frame of each color in the HDMI CDC order {R1,G1,B1,R0,G0,B0}.
            if (active_beats == 15) begin
                case (frame_count)
                    1: expected_object_beat = 48'hFF_00_00_FF_00_00;
                    2: expected_object_beat = 48'h00_00_FF_00_00_FF;
                    3: expected_object_beat = 48'hFF_FF_00_FF_FF_00;
                    4: expected_object_beat = 48'hFF_FF_FF_FF_FF_FF;
                    5: expected_object_beat = 48'h00_00_00_00_00_00;
                    default: expected_object_beat = 48'hxx_xx_xx_xx_xx_xx;
                endcase
                if (frame_count <= 5 && rgb_2ppc !== expected_object_beat) begin
                    $display("FAIL color frame=%0d got=%h expected=%h",
                             frame_count, rgb_2ppc, expected_object_beat);
                    $finish(1);
                end
                object_beat = rgb_2ppc;
            end
        end
        if ((frame_count == 6) && vs && !vs_d) begin
            $display("PASS tb_synthetic_2ppc_source");
            $finish(0);
        end
        vs_d = vs;
    end

    initial begin
        active_beats = 0;
        frame_count = 0;
        object_beat = 48'd0;
        expected_object_beat = 48'd0;
        vs_d = 1'b0;
        repeat (3) @(negedge clk);
        rst_n <= 1'b1;
    end
endmodule
