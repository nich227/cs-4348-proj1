/**
 * Project 1
 * Name: Kevin Chen
 * NetID: nkc160130
 * Coded in: C
 * Due: 09/28/19
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

int main(int argc, char *argv[])
{
	//Filename and interrupt not specified
	if (argc != 3)
	{
		printf("ERROR: This program requires 2 arguments to operate.\n1. Input program filename\n2. Timer interrupt value\n");
		exit(1);
	}

	//Seed for random number
	srand(time(0));

	//Creating pipes for sending data from CPU to memory and vice versa
	int cpuToMem[2]; //Pipe for cpu to memory
	int memToCpu[2]; //Pipe for memory to cpu

	if (pipe(cpuToMem) < 0 || pipe(memToCpu) < 0)
	{
		printf("ERROR: Piping failed!\n");
		exit(1);
	}

	//Get filename from arguments
	char *filename = argv[1];

	//Open file
	FILE *file = fopen(filename, "r");
	
	//File doesn't exist
	if (!file)
	{
		printf("\nERROR: %s not found!\n", filename);
		return 1;
	}

	//Forking process into CPU and memory
	int pid = fork();

	//Invalid process
	if (pid < 0)
	{
		printf("ERROR: Invalid process!\n");
		exit(1);
	}

	//Memory
	else if (pid == 0)
	{
		//Integer entries
		int mem_arr[2000];

		//User and system program
		int usr_prgm = 0;
		int sys_prgm = 1000;

		//Read in file line by line
		char *f_line;
		size_t f_len = 0;
		ssize_t f_read;

		//Read each line from file and extract the commands/jumps
		int cur_ptr = usr_prgm;
		while ((f_read = getline(&f_line, &f_len, file)) > 0)
		{

			int cur_char = 0;

			char tmp_command[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};

			//If it's a jump to address
			if (f_line[0] == '.')
			{
				tmp_command[0] = '.';
				cur_char++;
			}

			//Retrieve all numbers
			while (isdigit(f_line[cur_char]))
			{
				tmp_command[cur_char] = f_line[cur_char];
				cur_char++;
			}

			//If the input line needs to be stored in memory
			if (tmp_command[0] == '.' || isdigit(tmp_command[0]))
			{
				//If it's a jump to memory location
				if (tmp_command[0] == '.')
				{
					//Remove the initial . from the string
					for (int i = 0; i < 5; i++)
						tmp_command[i] = tmp_command[i + 1];
					cur_ptr = atoi(tmp_command);
				}

				//It's a user command
				else
				{
					mem_arr[cur_ptr] = atoi(tmp_command);
					cur_ptr++;
				}
			}
		}

		//Listen for CPU read/write requests
		while (true)
		{
			char input[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};

			//Get instruction from CPU
			read(cpuToMem[0], &input, 5);

			char command = input[0];

			for (int i = 0; i < 5; i++)
				input[i] = input[i + 1];

			//Do read operation
			if (command == 'r')
			{
				char convertToStr[5];
				snprintf(convertToStr, 5, "%d", mem_arr[atoi(input)]);
				write(memToCpu[1], &convertToStr, 4);
			}

			//Do write operation
			if (command == 'w')
			{
				char write_val[5];
				read(cpuToMem[0], &write_val, 4);

				mem_arr[atoi(input)] = atoi(write_val);
			}

			//Exit
			if (command == 'e')
				exit(0);
		}
	}

	//CPU
	else
	{
		//Get timer from arguments
		int timer = atoi(argv[2]);

		//Registers
		int pc;
		int sp;
		int ir;
		int ac;
		int x;
		int y;

		//Stack starting points
		int usr_stack_top = 999;
		int sys_stack_top = 1999;

		//Interrupts
		int inst_counter = 0;
		bool usr = true;
		bool intr = false;
		int num_intr_waiting = 0;

		//Default values
		pc = 0;
		sp = usr_stack_top;

		//Executing instructions
		while (true)
		{
			//Instruction buffer
			char inst[5] = {'\0', '\0', '\0', '\0', '\0'};

			//Buffer used for sending read or write commands to memory with addr
			char tmp_buffer[10] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};

			//Check if jump instruction is used
			bool hasJumped = false;

			//Check if there is another interrupt scheduled while this interrupt is going on
			if (intr && inst_counter > 0 && (inst_counter % timer) == 0)
				num_intr_waiting++;

			//Check for timer interrupt
			if ((!intr && inst_counter > 0 && (inst_counter % timer) == 0) || (!intr && num_intr_waiting))
			{
				//Set mode to interrupt mode
				usr = false;
				intr = true;

				//Reduce number of interrupts waiting
				num_intr_waiting--;

				//User values for pc and sp
				int usr_sp = sp;
				int usr_pc = pc;

				//Set sp to system stack top and pc to 1000 and write original values onto stack
				sp = sys_stack_top;
				pc = 1000;
				hasJumped = true;

				//Write user sp into memory at sp
				snprintf(tmp_buffer, 10, "w%d", sp);
				write(cpuToMem[1], &tmp_buffer, 5);
				snprintf(tmp_buffer, 10, "%d", usr_sp);
				write(cpuToMem[1], &tmp_buffer, 4);
				sp--;

				//Write user pc into memory at sp
				snprintf(tmp_buffer, 10, "w%d", sp);
				write(cpuToMem[1], &tmp_buffer, 5);
				snprintf(tmp_buffer, 10, "%d", usr_pc);
				write(cpuToMem[1], &tmp_buffer, 4);
				sp--;

				continue;
			}

			//Get next instruction from memory (Fetch)
			snprintf(tmp_buffer, 10, "r%d", pc);
			write(cpuToMem[1], &tmp_buffer, 5);
			read(memToCpu[0], &inst, 4);
			inst_counter++;

			//If the instruction does not return empty (Execute)
			if (inst[0] != '\0')
			{
				//Load instruction into ir
				ir = atoi(inst);

				//Load val
				if (ir == 1)
				{
					char read_mem[5];
					pc++;
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);
					ac = atoi(read_mem);
				}

				//Load addr
				else if (ir == 2)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Attempting to load from system memory
					if (atoi(read_mem) > 999 && usr)
						printf("Memory violation: accessing system address %d in user mode.\n", atoi(read_mem));
					else
					{
						//Load addr into ac
						snprintf(tmp_buffer, 10, "r%d", atoi(read_mem));
						write(cpuToMem[1], &tmp_buffer, 5);
						read(memToCpu[0], &read_mem, 4);
						ac = atoi(read_mem);
					}
				}

				//LoadInd addr
				else if (ir == 3)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Attempting to load from system memory
					if (atoi(read_mem) > 999 && usr)
						printf("Memory violation: accessing system address %d in user mode.\n", atoi(read_mem));
					else
					{
						//Load addr into ac
						snprintf(tmp_buffer, 10, "r%d", atoi(read_mem));
						write(cpuToMem[1], &tmp_buffer, 5);
						read(memToCpu[0], &read_mem, 4);
						ac = atoi(read_mem);

						//Attempting to load from system memory
						if (atoi(read_mem) > 999 && usr)
							printf("Memory violation: accessing system address %d in user mode.\n", atoi(read_mem));
						else
						{
							//Load ac address into ac
							snprintf(tmp_buffer, 10, "r%d", ac);
							write(cpuToMem[1], &tmp_buffer, 5);
							read(memToCpu[0], &read_mem, 4);
							ac = atoi(read_mem);
						}
					}
				}

				//LoadIdxX addr
				else if (ir == 4)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Attempting to load from system memory
					if (x + atoi(read_mem) > 999 && usr)
						printf("Memory violation: accessing system address %d in user mode.\n", atoi(read_mem));
					else
					{
						//Load x + addr into ac
						snprintf(tmp_buffer, 10, "r%d", x + atoi(read_mem));
						write(cpuToMem[1], &tmp_buffer, 5);
						read(memToCpu[0], &read_mem, 4);
						ac = atoi(read_mem);
					}
				}

				//LoadIdxY addr
				else if (ir == 5)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Attempting to load from system memory
					if (y + atoi(read_mem) > 999 && usr)
						printf("Memory violation: accessing system address %d in user mode.\n", atoi(read_mem));
					else
					{
						//Load y + addr into ac
						snprintf(tmp_buffer, 10, "r%d", y + atoi(read_mem));
						write(cpuToMem[1], &tmp_buffer, 5);
						read(memToCpu[0], &read_mem, 4);
						ac = atoi(read_mem);
					}
				}

				//LoadSpX
				else if (ir == 6)
				{
					char read_mem[5];

					//Attempting to load from system memory
					if (x + sp > 999 && !intr)
						printf("Memory violation: accessing system address %d in user mode.\n", atoi(read_mem));
					else
					{
						//Load sp + x into ac
						snprintf(tmp_buffer, 10, "r%d", sp + 1 + x);
						write(cpuToMem[1], &tmp_buffer, 5);
						read(memToCpu[0], &read_mem, 4);
						ac = atoi(read_mem);
					}
				}

				//Store addr
				else if (ir == 7)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Write ac into memory at location addr
					snprintf(tmp_buffer, 10, "w%d", atoi(read_mem));
					write(cpuToMem[1], &tmp_buffer, 5);
					snprintf(tmp_buffer, 10, "%d", ac);
					write(cpuToMem[1], &tmp_buffer, 4);
				}

				//Get
				else if (ir == 8)
					ac = (rand() % 100) + 1;

				//Put port
				else if (ir == 9)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Check value of port
					if (atoi(read_mem) == 1)
						printf("%d", ac);

					if (atoi(read_mem) == 2)
						printf("%c", ac);
				}

				//AddX
				else if (ir == 10)
					ac += x;

				//AddY
				else if (ir == 11)
					ac += y;

				//SubX
				else if (ir == 12)
					ac -= x;

				//SubY
				else if (ir == 13)
					ac -= y;

				//CopyToX
				else if (ir == 14)
					x = ac;

				//CopyFromX
				else if (ir == 15)
					ac = x;

				//CopyToY
				else if (ir == 16)
					y = ac;

				//CopyFromY
				else if (ir == 17)
					ac = y;

				//CopyToSp
				else if (ir == 18)
					sp = ac;

				//CopyFromSp
				else if (ir == 19)
					ac = sp;

				//Jump addr
				else if (ir == 20)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					pc = atoi(read_mem);
					hasJumped = true;
				}

				//JumpIfEqual
				else if (ir == 21)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Change value of pc (jump) if ac = 0
					if (ac == 0)
					{
						pc = atoi(read_mem);
						hasJumped = true;
					}
				}

				//JumpIfNotEqual addr
				else if (ir == 22)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Change value of pc (jump) if ac != 0
					if (ac != 0)
					{
						pc = atoi(read_mem);
						hasJumped = true;
					}
				}

				//Call addr
				else if (ir == 23)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Write return addr into stack at location sp
					snprintf(tmp_buffer, 10, "w%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					snprintf(tmp_buffer, 10, "%d", pc + 1);
					write(cpuToMem[1], &tmp_buffer, 4);
					sp--;

					pc = atoi(read_mem);
					hasJumped = true;
				}

				//Ret
				else if (ir == 24)
				{
					char read_mem[5];
					sp++;

					//Read top of stack
					snprintf(tmp_buffer, 10, "r%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Read into pc (jump)
					pc = atoi(read_mem);
					hasJumped = true;
				}

				//IncX
				else if (ir == 25)
					x++;

				//DecX
				else if (ir == 26)
					x--;

				//Push
				else if (ir == 27)
				{
					//Write ac into stack at location sp
					snprintf(tmp_buffer, 10, "w%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					snprintf(tmp_buffer, 10, "%d", ac);
					write(cpuToMem[1], &tmp_buffer, 4);
					sp--;
				}

				//Pop
				else if (ir == 28)
				{
					char read_mem[5];
					sp++;

					//Read top of stack
					snprintf(tmp_buffer, 10, "r%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Read into ac
					ac = atoi(read_mem);
				}

				//Int
				else if (ir == 29 && !intr)
				{
					//Set mode to interrupt mode
					usr = false;
					intr = true;

					//User values for pc and sp
					int usr_sp = sp;
					int usr_pc = pc + 1;

					//Set sp to system stack top and pc to 1500 and write original values onto stack
					sp = sys_stack_top;
					pc = 1500;
					hasJumped = true;

					//Write user sp into memory at sp
					snprintf(tmp_buffer, 10, "w%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					snprintf(tmp_buffer, 10, "%d", usr_sp);
					write(cpuToMem[1], &tmp_buffer, 4);
					sp--;

					//Write user pc into memory at sp
					snprintf(tmp_buffer, 10, "w%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					snprintf(tmp_buffer, 10, "%d", usr_pc);
					write(cpuToMem[1], &tmp_buffer, 4);
					sp--;
				}

				//IRet
				else if (ir == 30)
				{
					usr = true;
					intr = false;

					char read_mem[5];
					sp++;

					//Read top of stack
					snprintf(tmp_buffer, 10, "r%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Read into pc (jump)
					pc = atoi(read_mem);
					hasJumped = true;

					sp++;

					//Read top of stack
					snprintf(tmp_buffer, 10, "r%d", sp);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Read into sp (jump)
					sp = atoi(read_mem);
				}

				//End
				else if (ir == 50)
				{
					//Signal memory to exit
					write(cpuToMem[1], "e", 5);

					//Close all pipes
					close(cpuToMem[0]);
					close(cpuToMem[1]);
					close(memToCpu[0]);
					close(memToCpu[1]);

					//Exit
					return 0;
				}

				//Advance to next instruction if no jumps done
				if (!hasJumped)
					pc++;
			}
		}
	}
}