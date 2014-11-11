#include "procsim.hpp"

// Top-level processing function
// call sub-functions for each pipeline stages and update class-scope variables e.g. proc_complete
proc_result_t ProcSim::instProc(int cycle) {
	proc_result_t result;
	result.retired_inst = stateUpdate(cycle);
	result.fired_inst = instExecute(cycle);
	instSchedule(cycle);
	instDispatch(cycle);
	instFetch(cycle);
	result.disp_size = dispQ.size();
	proc_complete = dispQ.empty() && schedQMap.empty();
	return result;
}



/***************************************** State Update (SU) ******************************************/
unsigned int ProcSim::stateUpdate(int cycle) {
	// Completed instructions in FUs broadcast to free result buses, free FU
	while (!funcUnits.empty() && resultBuses.size() != r) {
		procResults[funcUnits.front().instNo - 1][4] = cycle;
#if DEBUGMODE
		std::cout << "Retire " << funcUnits.front().instNo << std::endl;
#endif
		resultBuses.push_back(ResultBusEntry(funcUnits.front().instNo, funcUnits.front().Reg, funcUnits.front().Tag));
		++fu_left[funcUnits.front().funcUnit];
		funcUnits.pop();
	}

	// Update Register File via result buses
	for (size_t i = 0; i != resultBuses.size(); ++i) {
		if (resultBuses[i].Reg != -1 && regFile[resultBuses[i].Reg].Tag == resultBuses[i].Tag)
			regFile[resultBuses[i].Reg].Ready = true;
	}
#if DEBUGFU
	std::cout << "FU available: " << fu_left[0] << ", " << fu_left[1] << ", " << fu_left[2] << std::endl;
#endif
	return resultBuses.size();
}



/***************************************** Execute (EX) ***********************************************/
unsigned int ProcSim::instExecute(int cycle) {
	// If(!RS.Fired && RS.Ready) Fire to FUs if available, copy Destination Register and Tag, mark fired
	unsigned int fired = 0;
	for (size_t RS = 0; RS != schedQMap.size(); ++RS) { // for each valid RS (in tag order)
#if DEBUGFU
		schedQ[schedQMap[RS]].display();
#endif
		if (!schedQ[schedQMap[RS]].Fired && schedQ[schedQMap[RS]].Ready) { // instruction ready to fire
			if (fu_left[schedQ[schedQMap[RS]].funcUnit] > 0) { // FU available
				procResults[schedQ[schedQMap[RS]].instNo - 1][3] = cycle;
#if DEBUGMODE
				std::cout << "Fire " << schedQ[schedQMap[RS]].instNo << std::endl;
#endif
				++fired;
				--fu_left[schedQ[schedQMap[RS]].funcUnit];
				funcUnits.push(FuncUnitEntry(schedQ[schedQMap[RS]].instNo, schedQ[schedQMap[RS]].funcUnit,
					schedQ[schedQMap[RS]].destReg, schedQ[schedQMap[RS]].destTag));
				schedQ[schedQMap[RS]].Fired = true;
			}
		}
		if (fu_left[0] == 0 && fu_left[1] == 0 && fu_left[2] == 0) break; // save some effort :p
	}
#if DEBUGFU
	std::cout << "FU available: " << fu_left[0] << ", " << fu_left[1] << ", " << fu_left[2] << std::endl;
#endif
	return fired;
}



/***************************************** Schedule(SH) ***********************************************/
void ProcSim::instSchedule(int cycle) {
	// If(!RS.Filed) Update via result buses, update ready bit
	for (size_t RS = 0; RS != schedQ.size(); ++RS) { // for each RS in scheduling queue
		if (schedQ[RS].instNo != 0 && !schedQ[RS].Fired) { // instruction not fired yet
			for (size_t i = 0; i != 2; ++i) {
				if (!schedQ[RS].srcReady[i]) { // read result bus
					for (size_t bus = 0; bus != resultBuses.size(); ++bus) {
						if (schedQ[RS].srcTag[i] == resultBuses[bus].Tag) { // value broadcast on the bus
							schedQ[RS].srcReady[i] = true;
							break;
						}
					}
				}
			}
			schedQ[RS].Ready = schedQ[RS].srcReady[0] && schedQ[RS].srcReady[1];
		}
	}
}



/***************************************** Dispatch (DP) **********************************************/
void ProcSim::instDispatch(int cycle) {
	// Read Register File and dispatch to Schedule Queue, update Register File
	proc_inst_t *dispInst;
	size_t index = 0;
	while (schedQMap.size() != schedQ_capacity && !dispQ.empty()) {
		procResults[dispQ.front().first - 1][2] = cycle + 1;
#if DEBUGMODE
		std::cout << "Schedule " << dispQ.front().first << " in next cycle" << std::endl;
#endif
		while (index != schedQ.size() && schedQ[index].instNo != 0) ++index;
		schedQMap.push_back(index); // reserve slot
		schedQ[index].instNo = dispQ.front().first;
		dispInst = &dispQ.front().second;
		schedQ[index].funcUnit = dispInst->op_code == -1 ? 1 : dispInst->op_code;
		for (size_t i = 0; i != 2; ++i) {
			if (dispInst->src_reg[i] == -1 || regFile[dispInst->src_reg[i]].Ready)
				schedQ[index].srcReady[i] = true;
			else {
				schedQ[index].srcReady[i] = false;
				schedQ[index].srcTag[i] = regFile[dispInst->src_reg[i]].Tag;
			}
		}
		schedQ[index].destReg = dispInst->dest_reg;
		schedQ[index].destTag = schedQ[index].instNo - 1;
		if (schedQ[index].destReg != -1) {
			regFile[schedQ[index].destReg].Tag = schedQ[index].destTag;
			regFile[schedQ[index].destReg].Ready = false;
		}
		dispQ.pop();
	}

	// Delete retired RSs(see result buses), free result buses
	for (size_t i = 0; i != resultBuses.size(); ++i) {
		auto beg = schedQMap.begin();
		while (beg != schedQMap.end() && schedQ[*beg].instNo != resultBuses[i].instNo) ++beg;
#if DEBUGFU
		std::cout << "Remove " << schedQ[*beg].instNo << " from schedule queue" << std::endl;
#endif
		schedQ[*beg].instNo = 0;
		schedQ[*beg].Ready = 0;
		schedQ[*beg].Fired = 0; // REMEMBER TO RESET!!!!!!!!!!!!!
		schedQMap.erase(beg);
	}
	resultBuses.clear();
}



/******************************************** Fetch (IF) **********************************************/
void ProcSim::instFetch(int cycle) {
	// Fetch next instructions into Dispatch Queue
	proc_inst_t *p_inst = new proc_inst_t;
	for (size_t i = 0; !fetch_complete && i != f; ++i) {
		if (read_instruction(p_inst, inFile)) {
			dispQ.push(make_pair(++inst_count, *p_inst));
			procResults.push_back(vector<int> {cycle, cycle + 1, 0, 0, 0});
		}
		else
			fetch_complete = true;
	}
#if DEBUGFU
	std::cout << "Size of schedule queue: " << schedQMap.size() << std::endl;
	std::cout << "Size of dispatch queue: " << dispQ.size() << std::endl;
#endif
}

void ProcSim::resultDisplay() {
	printf("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\n");
	for (size_t i = 0; i != procResults.size(); ++i) {
		printf("%d", i + 1);
		for (size_t j = 0; j != 5; ++j)
			printf("\t%d", procResults[i][j]);
		printf("\n");
	}
	printf("\n");
}


// Global object that simulates the processor
ProcSim procSim;

/**
 * Subroutine for initializing the processor. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @r ROB size
 * @k0 Number of k0 FUs
 * @k1 Number of k1 FUs
 * @k2 Number of k2 FUs
 * @f Number of instructions to fetch
 */
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
	procSim = ProcSim(k0, k1, k2, r, f);
}

/**
 * Subroutine that simulates the processor.
 *   The processor should fetch instructions as appropriate, until all instructions have executed
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */

// bool read_instruction(proc_inst_t* p_inst)
void run_proc(proc_stats_t* p_stats)
{
	proc_result_t result;
	while (!procSim.proc_complete) {
		++p_stats->cycle_count;
#if DEBUGMODE
		std::cout << p_stats->cycle_count << std::endl;
#endif
		result = procSim.instProc(p_stats->cycle_count);
		p_stats->retired_instruction += result.retired_inst;
		p_stats->fired_instruction += result.fired_inst;
		p_stats->total_disp_size += result.disp_size;
		if (result.disp_size > p_stats->max_disp_size) p_stats->max_disp_size = result.disp_size;
	}
}

/**
 * Subroutine for cleaning up any outstanding instructions and calculating overall statistics
 * such as average IPC, average fire rate etc.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_proc(proc_stats_t *p_stats) 
{
	p_stats->avg_inst_retired = (float) p_stats->retired_instruction / p_stats->cycle_count;
	p_stats->avg_inst_fired = (float) p_stats->fired_instruction / p_stats->cycle_count;
	p_stats->avg_disp_size = (float) p_stats->total_disp_size / p_stats->cycle_count;
	procSim.resultDisplay();
}
