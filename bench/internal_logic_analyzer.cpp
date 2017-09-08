#include <verilated.h>          // Defines common routines
#include "Vinternal_logic_analyzer_top.h"

#include "verilated_vcd_c.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>

// HALF_48MHz_PERIOD = 10416.7ps or 10.4167ns is half of clock period for 48MHz
#define HALF_48MHz_PERIOD 10416.672
#define DEBUG_ENABLED   1/*(num_of_clk_passed >= 2 && num_of_clk_passed <= 6) || (num_of_clk_passed  >= 1020 && num_of_clk_passed <= 1038)*/
#define debug_time	(num_of_clk_passed <= 10)
#define	TRACE_POSEDGE	if ((tfp != NULL) && debug_time) tfp->dump(time_ps*10)
#define	TRACE_NEGEDGE	if ((tfp != NULL) && debug_time) tfp->dump(time_ps*10)
#define	TRACE_FLUSH	if ((tfp != NULL) && debug_time) tfp->flush()
#define	TRACE_CLOSE	if (tfp != NULL) tfp->close()

Vinternal_logic_analyzer_top *uut;                     // Instantiation of module
VerilatedVcdC* tfp = NULL;

vluint64_t time_ps = 0;       // Current simulation time

static const unsigned int BUFFER_SIZE = 1024;		// size of circular buffer
static const unsigned int USER_HOLDOFF = 0;	// same value as i_holdoff
static const unsigned int ALIGNMENT_DELAY = 3;	// due to data stream alignment with i_trigger signal
static const unsigned int RESET_TIMING = 30; 	// number of clock cycles between complete stopping of previous logic sampling process and assertion of reset signal
static const bool NOT_TEST_PRIMED_CONDITION = true; // used to observe how the code fails in satisfying primed condition for i_trigger.  (false = test primed condition)

static unsigned int counter = 1;  // this is the test signal we are going to record into circular buffer
static int num_of_clk_passed = 0;  // for checking buffer counter data correctness
static bool assert_trigger_now = false;	// a signal that syncs with uut->i_trigger
 
double sc_time_stamp () {       // Called by $time in Verilog
    return time_ps;           // converts to double, to match
    // what SystemC does
}

void update_clk(void) {
	cout << "Start of next clock cycle !" << endl;
	uut->clk = 0; 
	uut->eval(); 
	TRACE_NEGEDGE; 

	time_ps = time_ps + HALF_48MHz_PERIOD/2;  

	/*******************************************/

	uut->clk = 1; 
	uut->eval(); 
	TRACE_POSEDGE; 

	time_ps = time_ps + HALF_48MHz_PERIOD; 

	/*******************************************/

	uut->clk = 0; 
	uut->eval(); 
	TRACE_NEGEDGE; 

	time_ps = time_ps + HALF_48MHz_PERIOD/2;

	TRACE_FLUSH;

	num_of_clk_passed++;
	cout << "Clock Updated !" << endl;
}

void cout_debug_msg() 
{
    cout << "uut->o_data = " << (int)uut->o_data << " , num_of_clk_passed = " << num_of_clk_passed << endl;  
    cout << "counter = " << counter << endl;
    cout << "uut->o_primed = " << (int)uut->o_primed << endl; 
    cout << "triggered = " << (int)uut->internal_logic_analyzer_top__DOT__st__DOT__triggered << endl;
    cout << "stopped = " << (int)uut->internal_logic_analyzer_top__DOT__stopped << endl;
    cout << "waddr = " << (int)uut->internal_logic_analyzer_top__DOT__waddr << endl;
    cout << "raddr = " << (int)uut->internal_logic_analyzer_top__DOT__rd__DOT__raddr << endl;
    cout << "this_addr = " << (int)uut->internal_logic_analyzer_top__DOT__rd__DOT__this_addr << endl;
}

// for debugging/development purpose only
void print_buffer_data()
{
    const unsigned int ENTRIES_PER_LINE = 25;
    cout << "Printing buffer data !" << endl;
    for (int k=0; k<BUFFER_SIZE; k++)
    {
	if (k==ENTRIES_PER_LINE) cout << endl;
	cout << uut->internal_logic_analyzer_top__DOT____Vcellout__wr__memory[k] << " ";
    }
    cout << endl;
}


bool test_buffer()
{
    if ((num_of_clk_passed > BUFFER_SIZE + USER_HOLDOFF + ALIGNMENT_DELAY + 1) && (num_of_clk_passed <= 2*BUFFER_SIZE + USER_HOLDOFF + ALIGNMENT_DELAY)) {
    	// test for counter data correctness in the circular buffer
    	if (((int)uut->o_data + BUFFER_SIZE + ALIGNMENT_DELAY) != (num_of_clk_passed-1)) {	
	    cout << "Error: Data in the circular buffer is not correct for buffer items at " << (fmod(num_of_clk_passed , (BUFFER_SIZE+1))) << " at clock cycle " << num_of_clk_passed << endl;
	    print_buffer_data(); // for debugging/development purpose only
	    return false;
    	}    
    }

    if (num_of_clk_passed <= BUFFER_SIZE + ALIGNMENT_DELAY + 1) {  
        // test for primed condition
    	if (!((int)uut->o_primed) && (assert_trigger_now) && (USER_HOLDOFF == 0)) {
	    cout << "Error: Memory is not yet fully initialized. Scope could not stop recording at clock cycle " << num_of_clk_passed << endl;
	    print_buffer_data(); // for debugging/development purpose only
	    return false;
	}
    }

    return true;
}

int main(int argc, char** argv)
{
    // turn on trace or not?
    bool vcdTrace = true;

    Verilated::commandArgs(argc, argv);   // Remember args
    uut = new Vinternal_logic_analyzer_top;   // Create instance

    if (vcdTrace)
    {
        Verilated::traceEverOn(true);

        tfp = new VerilatedVcdC;
        uut->trace(tfp, 99);

        std::string vcdname = argv[0];
        vcdname += ".vcd";
        std::cout << vcdname << std::endl;
        tfp->open(vcdname.c_str());
    }

    unsigned int num_of_reset_done = 0;

    uut->i_holdoff = USER_HOLDOFF;
    uut->i_data = counter;
    uut->reset = 0;
    uut->clk = 0;

    while (!Verilated::gotFinish())
    { 
	update_clk();            // Time passes...

	cout_debug_msg();

	counter = counter + 1;
	uut->i_data = counter;

	assert_trigger_now = (num_of_clk_passed == BUFFER_SIZE + NOT_TEST_PRIMED_CONDITION) ? 1 : 0;
	cout << "i_trigger = " << assert_trigger_now << endl;	
	uut->i_trigger = assert_trigger_now; 

	// test for correct recording operation by reading out the data in circular buffer
	bool buffer_is_ok = test_buffer();

	if (!(buffer_is_ok) || (num_of_reset_done==3)) break;	

	if (DEBUG_ENABLED) print_buffer_data();
	
	if (num_of_clk_passed == 2*BUFFER_SIZE + USER_HOLDOFF + ALIGNMENT_DELAY + RESET_TIMING) {
	    counter = 1;
	    num_of_clk_passed = 0;
	    uut->i_data = counter;

	    cout << "uut->reset = 1" << endl;
	    uut->reset = 1;	// test for reset signal
	    update_clk();       // Time passes...
	    uut->reset = 0;	// assert reset signal for one clock cycle only
	    cout << "uut->reset = 0" << endl;
	    num_of_reset_done++;

	    cout_debug_msg();
	    print_buffer_data(); // for debugging/development purpose only

	    counter = counter + 1;
	    uut->i_data = counter;
	}
    }

    uut->final();               // Done simulating

    if (tfp != NULL)
    {
        tfp->close();
        delete tfp;
    }

    delete uut;

    return 0;
}
