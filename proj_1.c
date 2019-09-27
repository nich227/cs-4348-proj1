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

int main(int argc, char *argv[])
{
	//Filename and interrupt not specified
	if (argc != 3)
	{
		printf("ERROR: This program requires 2 arguments to operate.\n1. Input program filename\n2. Timer interrupt value\n");
		exit(1);
	}

	//Filename and interrupt
	char *filename = argv[1];
	int interrupt = *argv[2];

	//Creating pipes for sending data from CPU to memory and vice versa
	int cpuToMem[2]; //Pipe for cpu to memory
	int memToCpu[2]; //Pipe for memory to cpu

	if (pipe(cpuToMem) < 0 || pipe(memToCpu) < 0)
	{
		printf("ERROR: Piping failed!\n");
		exit(1);
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
		int sys_call = 1000;

		//Read in file line by line
		FILE *file = fopen(filename, "r");
		char *f_line;
		size_t f_len = 0;
		ssize_t f_read;

		//File doesn't exist
		if (!file)
		{
			printf("\nERROR: %s not found!\n", filename);
			return 1;
		}

		//Read each line from file and extract the commands/jumps
		int cur_ptr = usr_prgm;
		while (f_read = getline(&f_line, &f_len, file) > 0)
		{

			int cur_char = 0;

			char tmp_command[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
			int jmp_to_address = -1;

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
			if (tmp_command[0] != '\0')
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

		cur_ptr = usr_prgm;

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
			if (input[0] == 'w')
			{
				char write_val[5];
				read(cpuToMem[0], &write_val, 4);
				mem_arr[atoi(input)] = atoi(write_val);
			}
		}
	}

	//CPU
	else
	{
		//Registers
		int pc = 0;
		int sp = 999;
		int ir;
		int ac;
		int x;
		int y;

		//Stack starting points
		int usr_stack_top = 999;
		int sys_stack_top = 1999;

		//Interrupts
		int timer = 0;
		int inst_counter = 0;
		bool usr = true;
		bool intr = false;

		char inst[5] = {'\0', '\0', '\0', '\0', '\0'};
		//Executing instructions
		while (true)
		{
			for (int i = 0; i < 5; i++)
				inst[i] = '\0';

			//Check for interrupt
			if (!intr && inst_counter > 0 && (inst_counter % timer) == 0)
			{
				usr = false;
				intr = true;
			}

			//Get next instruction from memory
			char tmp_buffer[10];
			snprintf(tmp_buffer, 10, "r%d", pc);
			write(cpuToMem[1], &tmp_buffer, 5);
			read(memToCpu[0], &inst, 4);

			//Check if jump instruction is used
			bool hasJumped = false;

			if (inst[0] != '\0')
			{
				//Load val
				if (atoi(inst) == 1)
				{
					char read_mem[5];
					pc++;
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);
					ac = atoi(read_mem);
				}

				//Load addr
				if (atoi(inst) == 2)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Load addr into ac
					snprintf(tmp_buffer, 10, "r%d", atoi(read_mem));
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);
					ac = atoi(read_mem);
				}

				//LoadIdxX addr
				if (atoi(inst) == 4)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Load x + addr into ac
					snprintf(tmp_buffer, 10, "r%d", x + atoi(read_mem));
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);
					ac = atoi(read_mem);
				}

				//LoadIdxY addr
				if(atoi(inst) == 5)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					//Load y + addr into ac
					snprintf(tmp_buffer, 10, "r%d", y + atoi(read_mem));
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);
					ac = atoi(read_mem);
				}

				//Put port
				if (atoi(inst) == 9)
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
				if(atoi(inst) == 10)
					ac += x;

				//AddY
				if(atoi(inst) == 11)
					ac += y;

				//CopyToX
				if (atoi(inst) == 14)
					x = ac;

				//CopyToY
				if(atoi(inst) == 16)
					y = ac;

				//Jump addr
				if(atoi(inst) == 20)
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
				if (atoi(inst) == 21)
				{
					char read_mem[5];
					pc++;

					//Read next line for addr
					snprintf(tmp_buffer, 10, "r%d", pc);
					write(cpuToMem[1], &tmp_buffer, 5);
					read(memToCpu[0], &read_mem, 4);

					if (ac == 0)
					{
						pc = atoi(read_mem);
						hasJumped = true;
					}
				}

				//IncX
				if(atoi(inst) == 25)
					x++;

				//End
				if(atoi(inst) == 50)
					return 0;

				//Advance to next instruction if no jumps done
				if (!hasJumped)
					pc++;
			}
		}
	}
}