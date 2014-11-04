#ifndef PROCSIM_HPP
#define PROCSIM_HPP

#include <cstdint>
#include <cstdio>

#include <iostream>

#include <vector>
#include <queue>

using std::vector;
using std::queue;
using std::pair;
using std::make_pair;

#ifndef DEBUGMODE
#define DEBUGMODE 0
#endif

#ifndef DEBUGFU
#define DEBUGFU 0
#endif

#define DEFAULT_K0 1
#define DEFAULT_K1 2
#define DEFAULT_K2 3
#define DEFAULT_R 8
#define DEFAULT_F 4

#define REG_NUMBER 128

typedef struct _proc_inst_t
{
	uint32_t instruction_address;
	int32_t op_code;
	int32_t src_reg[2];
	int32_t dest_reg;
} proc_inst_t;

typedef struct _proc_stats_t
{
	unsigned long retired_instruction;
	unsigned long fired_instruction;
	unsigned long total_disp_size;
	float avg_inst_retired;
	float avg_inst_fired;
	float avg_disp_size;
	unsigned long max_disp_size;
	unsigned long cycle_count;
} proc_stats_t;

// struct to store processing result from each cycle
struct proc_result_t {
	proc_result_t() : retired_inst(0), fired_inst(0), disp_size(0) {}
	int retired_inst;
	int fired_inst;
	unsigned long disp_size;
};

class ProcSim {
public:
	ProcSim() : k0(DEFAULT_K0), k1(DEFAULT_K1), k2(DEFAULT_K2), r(DEFAULT_R), f(DEFAULT_F),
		schedQ_capacity(2 * (k0 + k1 + k2)), inst_count(0),
		regFile(vector<RegFileEntry>(REG_NUMBER, RegFileEntry())) {}
	ProcSim(uint64_t k0, uint64_t k1, uint64_t k2, uint64_t r, uint64_t f) :
		k0(k0), k1(k1), k2(k2), r(r), f(f),
		inst_count(0),
		schedQ_capacity(2 * (k0 + k1 + k2)), /*schedQ_left(schedQ_capacity),*/
		proc_complete(false), fetch_complete(false),
		schedQ(vector<SchedQEntry>(schedQ_capacity, SchedQEntry())),
		regFile(vector<RegFileEntry>(REG_NUMBER, RegFileEntry())) {}
	proc_result_t instProc();
	void setFU(int k0, int k1, int k2) { fu_left[0] = k0; fu_left[1] = k1; fu_left[2] = k2; }
	bool proc_complete;
	unsigned long getInstCount() { return inst_count; }
private:
	uint64_t k0, k1, k2, r, f;
	int fu_left[3]; // available function units
	int schedQ_capacity; // 2 * (k0 + k1 + k2)
	unsigned long inst_count; // initialize to 0, valid from 1
	bool fetch_complete;
	
	// Dispatch Queue
	queue<pair<int, proc_inst_t>> dispQ; // In-order dispatcher

	// Scheduling Queue
	struct SchedQEntry {
		SchedQEntry() : instNo(0), Ready(false), Fired(false) {}
		unsigned long instNo; // 0 indicates empty slot
		int32_t funcUnit;
		int32_t destReg;
		unsigned long destTag;
		bool srcReady[2];
		unsigned long srcTag[2];
		bool Ready;
		bool Fired;
		void display() {
			std::cout << instNo << " " << funcUnit << " " << destReg << " " << destReg << " "
				<< srcReady[0] << " " << srcReady[1] << " " << Ready << " " << Fired << std::endl;
		}
	};
	vector<SchedQEntry> schedQ; // Out-of-order scheduler
	vector<int> schedQMap;
	
	// Register File
	struct RegFileEntry {
		RegFileEntry() : Ready(true), Tag(0) {}
		bool Ready;
		unsigned long Tag;
	};
	vector<RegFileEntry> regFile; // fixex size
	
	// Function Units
	struct FuncUnitEntry {
		FuncUnitEntry() : instNo(0), funcUnit(0), Reg(0), Tag(0) {}
		FuncUnitEntry(unsigned long inst_no, int32_t func_unit, int32_t inst_reg, unsigned long inst_tag)
		: instNo(inst_no), funcUnit(func_unit), Reg(inst_reg), Tag(inst_tag) {}
		unsigned long instNo;
		int32_t funcUnit;
		int32_t Reg;
		unsigned long Tag;
	};
	queue<FuncUnitEntry> funcUnits; // FIFO

	// Result Buses
	struct ResultBusEntry {
		ResultBusEntry() : instNo(0), Reg(-1), Tag(0) {}
		ResultBusEntry(unsigned long inst_no, int32_t inst_reg, unsigned long inst_tag)
		: instNo(inst_no), Reg(inst_reg), Tag(inst_tag) {}
		unsigned long instNo;
		int32_t Reg;
		unsigned long Tag;
	};
	vector<ResultBusEntry> resultBuses;

	int stateUpdate(); // return # retired instructions
	int instExecute(); // return # fired instructions
	void instSchedule();
	void instDispatch();
	void instFetch();
};


bool read_instruction(proc_inst_t* p_inst, FILE* fin);
extern FILE* inFile;

void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

#endif /* PROCSIM_HPP */
