/*
 * This was copied from the Ice40 Nextpnr example. As Nextpnr is under the ISC license,
 * this likely also is:
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
module top(input clk, input [7:0] btn, output [5:0] led);
    localparam ctr_width = 24;
    localparam ctr_max = 2**ctr_width - 1;
    reg [ctr_width-1:0] ctr = 0;
    reg [9:0] pwm_ctr = 0;
    reg dir = 0;

    always@(posedge clk) begin
    ctr <= dir ? ctr - 1'b1 - btn[7]: ctr + 1'b1 + btn[7];
        if (ctr[ctr_width-1 : ctr_width-3] == 0 && dir == 1)
            dir <= 1'b0;
        else if (ctr[ctr_width-1 : ctr_width-3] == 7 && dir == 0)
            dir <= 1'b1;
        pwm_ctr <= pwm_ctr + 1'b1;
    end

    reg [9:0] brightness [0:7];
    localparam bright_max = 2**10 - 1;
    reg [7:0] led_reg;

    genvar i;
    generate
    for (i = 0; i < 8; i=i+1) begin
       always @ (posedge clk) begin
            if (ctr[ctr_width-1 : ctr_width-3] == i)
                brightness[i] <= bright_max;
            else if (ctr[ctr_width-1 : ctr_width-3] == (i - 1))
                brightness[i] <= ctr[ctr_width-4:ctr_width-13];
             else if (ctr[ctr_width-1 : ctr_width-3] == (i + 1))
                 brightness[i] <= bright_max - ctr[ctr_width-4:ctr_width-13];
            else
                brightness[i] <= 0;
            led_reg[i] <= pwm_ctr < brightness[i];
       end
    end
    endgenerate

    assign led = led_reg[6:1];
endmodule