`timescale 1ns/1ps

module tb_apb_reg_magic;

reg  [11:0] paddr;
reg         penable;
wire [31:0] prdata;
wire        pready;
reg         psel;
wire        pslverror;
reg         pwrite;

apb_reg_magic dut (
    .paddr      (paddr),
    .penable    (penable),
    .prdata     (prdata),
    .pready     (pready),
    .psel       (psel),
    .pslverror  (pslverror),
    .pwrite     (pwrite)
);

task expect;
    input [31:0] expected_data;
    input        expected_error;
    begin
        #1;
        if (pready !== 1'b1 || prdata !== expected_data ||
            pslverror !== expected_error) begin
            $display("FAIL paddr=%h psel=%b penable=%b pwrite=%b prdata=%h pready=%b pslverror=%b",
                     paddr, psel, penable, pwrite, prdata, pready, pslverror);
            $finish;
        end
    end
endtask

initial begin
    paddr = 12'h000;
    penable = 1'b0;
    psel = 1'b0;
    pwrite = 1'b0;
    expect(32'h375A_0001, 1'b0);

    psel = 1'b1;
    expect(32'h375A_0001, 1'b0);

    penable = 1'b1;
    expect(32'h375A_0001, 1'b0);

    pwrite = 1'b1;
    expect(32'h0000_0000, 1'b1);

    pwrite = 1'b0;
    paddr = 12'h004;
    expect(32'h0000_0000, 1'b1);

    penable = 1'b0;
    expect(32'h0000_0000, 1'b0);

    $display("APB_REG_MAGIC_TB=PASS");
    $finish;
end

endmodule
