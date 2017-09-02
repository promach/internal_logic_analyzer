`include "define.v"

module write_mem (clk, reset, write_enable, data, waddr, memory, primed);

input clk, reset, write_enable;
input [(`DATA_WIDTH-1) : 0] data;

output reg primed = 0;	// to signal whether *all* memory locations have been initialized (written to before) or not
output reg [(`ADDR_WIDTH-1) : 0] waddr;	// address for memory(buffer) writing
output reg [(`DATA_WIDTH-1) : 0] memory [(`MEMORY_SIZE-1) : 0];

always @(posedge clk)
begin
    if (reset) 	
	waddr <= 0;
    else begin
	waddr <= waddr + `ADDR_WIDTH'(write_enable);   // if write is enabled, then waddr++
	$display("waddr = %d" , waddr);
    end
end

always @(posedge clk)
begin
    $display("data = %d" , data);
    if (write_enable)	
	memory[waddr] <= data;
end

always @(posedge clk)
begin
    if (reset)	
	primed <= 1'b0;		// memory is not initialized
    else if (!primed) begin 
	primed <= &waddr;	// the very *first*(!primed) wrapping around (&waddr) of address space
	$display("primed = ", primed); 
    end
end

endmodule
