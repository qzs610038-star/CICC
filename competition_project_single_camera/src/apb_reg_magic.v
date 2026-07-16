module apb_reg_magic #(
    parameter [31:0] REG_MAGIC = 32'h375A_0001
) (
    input  wire [11:0] paddr,
    input  wire        penable,
    output wire [31:0] prdata,
    output wire        pready,
    input  wire        psel,
    output wire        pslverror,
    input  wire        pwrite
);

wire access_valid = psel && penable;
wire address_valid = (paddr == 12'h000);

assign pready = 1'b1;
assign prdata = (!pwrite && address_valid) ? REG_MAGIC : 32'h0000_0000;
assign pslverror = access_valid && (pwrite || !address_valid);

endmodule
