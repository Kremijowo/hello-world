#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <iostream>
#include <conio.h>		
#include<assert.h>
#include<string.h>
#include<time.h>

using namespace std;

//Constant variables.
/*
*	The maximum number of blocks a file can use.
*	Used in struct inode.
*/
const unsigned int NADDR = 6;
/*
*	Size of a block.
*/
const unsigned short BLOCK_SIZE = 512;
/*
*	The maximum size of a file.
*/
const unsigned int FILE_SIZE_MAX = (NADDR - 2) * BLOCK_SIZE + BLOCK_SIZE / sizeof(int) * BLOCK_SIZE;
/*
*	The maximum number of data blocks.
*/
const unsigned short BLOCK_NUM = 512;
/*
*	Size of an inode.
*/
const unsigned short INODE_SIZE = 128;
/*
*	The maximum number of inodes.
*/
const unsigned short INODE_NUM = 256;
/*
*	The start position of inode chain.
*	First four blocks are used for loader sector(empty in this program), super block, inode bitmap and block bitmap.
*/
const unsigned int INODE_START = 3 * BLOCK_SIZE;
/*
*	The start position of data blocks.
*/
const unsigned int DATA_START = INODE_START + INODE_NUM * INODE_SIZE;
/*
*	The maximum number of the file system users.
*/
const unsigned int ACCOUNT_NUM = 10;
/*
*	The maximum number of sub-files and sub-directories in one directory.
*/
const unsigned int DIRECTORY_NUM = 16;
/*
*	The maximum length of a file name.
*/
const unsigned short FILE_NAME_LENGTH = 14;
/*
*	The maximum length of a user's name.
*/
const unsigned short USER_NAME_LENGTH = 14;
/*
*	The maximum length of a accouting password.
*/
const unsigned short USER_PASSWORD_LENGTH = 14;
/*
*	The maximum permission of a file.
*/
const unsigned short MAX_PERMISSION = 511;
/*
*	The maximum permission of a file.
*/
const unsigned short MAX_OWNER_PERMISSION = 448;
/*
*	Permission
*/
const unsigned short ELSE_E = 1;
const unsigned short ELSE_W = 1 << 1;
const unsigned short ELSE_R = 1 << 2;
const unsigned short GRP_E = 1 << 3;
const unsigned short GRP_W = 1 << 4;
const unsigned short GRP_R = 1 << 5;
const unsigned short OWN_E = 1 << 6;
const unsigned short OWN_W = 1 << 7;
const unsigned short OWN_R = 1 << 8;
//Data structures.
/*
*	inode(128B)
*/
struct inode
{
	unsigned int i_ino;			//Identification of the inode.
	unsigned int di_addr[NADDR];//Number of data blocks where the file stored.
	unsigned short di_number;	//Number of associated files.
	unsigned short di_mode;		//0 stands for a directory, 1 stands for a file.
	unsigned short icount;		//link number
	unsigned short permission;	//file permission
	unsigned short di_uid;		//File's user id.
	unsigned short di_grp;		//File's group id
	unsigned short di_size;		//File size.
	char time[83];
};

/*
*	Super block
*/
struct filsys
{
	unsigned short s_num_inode;			//Total number of inodes.
	unsigned short s_num_finode;		//Total number of free inodes.
	unsigned short s_size_inode;		//Size of an inode.

	unsigned short s_num_block;			//Total number of blocks.
	unsigned short s_num_fblock;		//Total number of free blocks.
	unsigned short s_size_block;		//Size of a block.

	unsigned int special_stack[50];
	int special_free;
};

/*
*	Directory file(216B)
*/
struct directory
{
	char fileName[20][FILE_NAME_LENGTH];
	unsigned int inodeID[DIRECTORY_NUM];
};

/*
*	Accouting file(320B)
*/
struct userPsw
{
	unsigned short userID[ACCOUNT_NUM];
	char userName[ACCOUNT_NUM][USER_NAME_LENGTH];
	char password[ACCOUNT_NUM][USER_PASSWORD_LENGTH];
	unsigned short groupID[ACCOUNT_NUM];
};

/*
*	Function declaritions.
*/
void CommParser(inode*&);
void Help();
void Sys_start();
//Globle varibles.
/*
*	A descriptor of a file where the file system is emulated.
*/
FILE* fd = NULL;
/*
*	Super block.
*/
filsys superBlock;
/*
*	Bitmaps for inodes and blocks. Element 1 stands for 'uesd', 0 for 'free'.
*/
unsigned short inode_bitmap[INODE_NUM];
/*
*	Accouting information.
*/
userPsw users;
/*
*	current user ID.
*/
unsigned short userID = ACCOUNT_NUM;
/*
*	current user name. used in command line.
*/
char userName[USER_NAME_LENGTH + 6];
/*
*	current directory.
*/
directory currentDirectory;
/*
*	current directory stack.
*/
char ab_dir[100][14];
unsigned short dir_pointer;

//find free block
void find_free_block(unsigned int &inode_number)
{
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(filsys), 1, fd);
	if (superBlock.special_free == 0)
	{
		if (superBlock.special_stack[0] == 0)
		{
			printf("No value block!\n");
			return;
		}
		unsigned int stack[51];

		for (int i = 0; i < 50; i++)
		{
			stack[i] = superBlock.special_stack[i];
		}
		stack[50] = superBlock.special_free;
		fseek(fd, DATA_START + (superBlock.special_stack[0] - 50) * BLOCK_SIZE, SEEK_SET);
		fwrite(stack, sizeof(stack), 1, fd);

		fseek(fd, DATA_START + superBlock.special_stack[0] * BLOCK_SIZE, SEEK_SET);
		fread(stack, sizeof(stack), 1, fd);
		for (int i = 0; i < 50; i++)
		{
			superBlock.special_stack[i] = stack[i];
		}
		superBlock.special_free = stack[50];
	}
	inode_number = superBlock.special_stack[superBlock.special_free];
	superBlock.special_free--;
	superBlock.s_num_fblock--;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
}

//recycle block
void recycle_block(unsigned int &inode_number)
{
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(filsys), 1, fd);
	if (superBlock.special_free == 49)
	{
		unsigned int block_num;
		unsigned int stack[51];
		if (superBlock.special_stack[0] == 0)
			block_num = 499;
		else
			block_num = superBlock.special_stack[0] - 50;
		for (int i = 0; i < 50; i++)
		{
			stack[i] = superBlock.special_stack[i];
		}
		stack[50] = superBlock.special_free;
		fseek(fd, DATA_START + block_num*BLOCK_SIZE, SEEK_SET);
		fwrite(stack, sizeof(stack), 1, fd);
		block_num -= 50;
		fseek(fd, DATA_START + block_num*BLOCK_SIZE, SEEK_SET);
		fread(stack, sizeof(stack), 1, fd);
		for (int i = 0; i < 50; i++)
		{
			superBlock.special_stack[i] = stack[i];
		}
		superBlock.special_free = stack[50];
	}
	superBlock.special_free++;
	superBlock.s_num_fblock++;
	superBlock.special_stack[superBlock.special_free] = inode_number;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
}

/*
*	Formatting function of the file system, including the establishment
*	of superblock, inode chain, root directory, password file and so on.
*
*	return: the function return true only the file system is initialized
*			successfully.
*/
bool Format()
{
	/*
	*	1. Create a empty file to emulate the file system.
	*/
	FILE* fd = fopen("./fs.unix", "wb+");
	if (fd == NULL)
	{
		printf("Fail to initialize the file system!\n");
		return false;
	}

	/*
	*	2. Initialize super block.
	*/
	filsys superBlock;
	superBlock.s_num_inode = INODE_NUM;
	superBlock.s_num_block = BLOCK_NUM + 3 + 64; //3代表0空闲块、1超级块、2Inode位示图表,64块存inode
	superBlock.s_size_inode = INODE_SIZE;
	superBlock.s_size_block = BLOCK_SIZE;
	//Root directory and accounting file will use some inodes and blocks.
	superBlock.s_num_fblock = BLOCK_NUM - 2;
	superBlock.s_num_finode = INODE_NUM - 2;
	superBlock.special_stack[0] = 99;
	for (int i = 1; i < 50; i++)
	{
		superBlock.special_stack[i] = 49 - i;
	}
	superBlock.special_free = 47;
	//Write super block into file.
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(filsys), 1, fd);

	/*
	*	3. Initialize inode and block bitmaps.
	*/
	unsigned short inode_bitmap[INODE_NUM];
	//Root directory and accounting file will use some inodes and blocks.
	memset(inode_bitmap, 0, INODE_NUM);
	inode_bitmap[0] = 1;
	inode_bitmap[1] = 1;
	//Write bitmaps into file.
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	//成组连接
	unsigned int stack[51];
	for (int i = 0; i < BLOCK_NUM / 50; i++)
	{
		memset(stack, 0, sizeof(stack));
		for (unsigned int j = 0; j < 50; j++)
		{
			stack[j] = (49 + i * 50) - j;
		}
		stack[0] = 49 + (i + 1) * 50;
		stack[50] = 49;
		fseek(fd, DATA_START + (49 + i * 50)*BLOCK_SIZE, SEEK_SET);
		fwrite(stack, sizeof(unsigned int) * 51, 1, fd);
	}
	memset(stack, 0, sizeof(stack));
	for (int i = 0; i < 12; i++)
	{
		stack[i] = 511 - i;
	}
	stack[0] = 0;
	stack[50] = 11;
	fseek(fd, DATA_START + 511 * BLOCK_SIZE, SEEK_SET);
	fwrite(stack, sizeof(unsigned int) * 51, 1, fd);

	fseek(fd, DATA_START + 49 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 99 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 149 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 199 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 249 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 299 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 349 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 399 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 449 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 499 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);
	fseek(fd, DATA_START + 511 * BLOCK_SIZE, SEEK_SET);
	fread(stack, sizeof(unsigned int) * 51, 1, fd);


	/*
	*	4. Create root directory.
	*/
	//Create inode
	//Now root directory contain 1 accounting file.
	inode iroot_tmp;
	iroot_tmp.i_ino = 0;					//Identification
	iroot_tmp.di_number = 2;				//Associations: itself and accouting file
	iroot_tmp.di_mode = 0;					//0 stands for directory
	iroot_tmp.di_size = 0;					//"For directories, the value is 0."
	memset(iroot_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	iroot_tmp.di_addr[0] = 0;				//Root directory is stored on 1st block. FFFFFF means empty.
	iroot_tmp.permission = MAX_OWNER_PERMISSION;
	iroot_tmp.di_grp = 1;
	iroot_tmp.di_uid = 0;					//Root user id.
	iroot_tmp.icount = 0;
	time_t t = time(0);
	strftime(iroot_tmp.time, sizeof(iroot_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	iroot_tmp.time[64] = 0;
	fseek(fd, INODE_START, SEEK_SET);
	fwrite(&iroot_tmp, sizeof(inode), 1, fd);

	//Create directory file.
	directory droot_tmp;
	memset(droot_tmp.fileName, 0, sizeof(char) * DIRECTORY_NUM * FILE_NAME_LENGTH);
	memset(droot_tmp.inodeID, -1, sizeof(unsigned int) * DIRECTORY_NUM);
	strcpy(droot_tmp.fileName[0], ".");
	droot_tmp.inodeID[0] = 0;
	strcpy(droot_tmp.fileName[1], "..");
	droot_tmp.inodeID[1] = 0;
	//A sub directory for accounting files
	strcpy(droot_tmp.fileName[2], "pw");
	droot_tmp.inodeID[2] = 1;

	//Write
	fseek(fd, DATA_START, SEEK_SET);
	fwrite(&droot_tmp, sizeof(directory), 1, fd);

	/*
	*	5. Create accouting file.
	*/
	//Create inode
	inode iaccouting_tmp;
	iaccouting_tmp.i_ino = 1;					//Identification
	iaccouting_tmp.di_number = 1;				//Associations
	iaccouting_tmp.di_mode = 1;					//1 stands for file
	iaccouting_tmp.di_size = sizeof(userPsw);	//File size
	memset(iaccouting_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	iaccouting_tmp.di_addr[0] = 1;				//Root directory is stored on 1st block.
	iaccouting_tmp.di_uid = 0;					//Root user id.
	iaccouting_tmp.di_grp = 1;
	iaccouting_tmp.permission = 320;
	iaccouting_tmp.icount = 0;
	t = time(0);
	strftime(iaccouting_tmp.time, sizeof(iaccouting_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	iaccouting_tmp.time[64] = 0;
	fseek(fd, INODE_START + INODE_SIZE, SEEK_SET);
	fwrite(&iaccouting_tmp, sizeof(inode), 1, fd);

	//Create accouting file.
	userPsw paccouting_tmp;
	memset(paccouting_tmp.userName, 0, sizeof(char) * USER_NAME_LENGTH * ACCOUNT_NUM);
	memset(paccouting_tmp.password, 0, sizeof(char) * USER_PASSWORD_LENGTH * ACCOUNT_NUM);
	//Only default user 'admin' is registered. Default password is 'admin'.
	strcpy(paccouting_tmp.userName[0], "admin");
	strcpy(paccouting_tmp.userName[1], "guest");
	strcpy(paccouting_tmp.password[0], "admin");
	strcpy(paccouting_tmp.password[1], "123456");
	//0 stands for super user. Other IDs are only used to identify users.
	for (unsigned short i = 0; i < ACCOUNT_NUM; i++)
	{
		paccouting_tmp.userID[i] = i;
	}
	paccouting_tmp.groupID[0] = 1;
	paccouting_tmp.groupID[1] = 2;
	//Write
	fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
	fwrite(&paccouting_tmp, sizeof(userPsw), 1, fd);

	//Close file.
	fclose(fd);

	return true;
};

/*
*	Initialization function of the file system. Open an existing file system
*	from 'fs.unix'.
*
*	return: the function return true only when the file system has been
*			formatted and is complete.
*/
bool Mount()
{
	/*
	*	1. Open the emulation file where the file system is installed.
	*/
	fd = fopen("./fs.unix", "rb+");
	if (fd == NULL)
	{
		printf("Error: File system not found!\n");
		return false;
	}

	/*
	*	2. Read superblock, bitmaps, accouting file, current directory (root)
	*/
	//Read superblock
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fread(&superBlock, sizeof(superBlock), 1, fd);

	//Read inode bitmap
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fread(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	//Read current directory, namely root directory
	fseek(fd, DATA_START, SEEK_SET);
	fread(&currentDirectory, sizeof(directory), 1, fd);

	//Read accouting file
	fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
	fread(&users, sizeof(userPsw), 1, fd);

	return true;
};

/*
*	Log in function. Update user information by checking log in inputs.
*
*	return: the function return true only when log in process succeed.
*/
bool Login(const char* user, const char* password)
{
	//parameters check
	if (user == NULL || password == NULL)
	{
		printf("Error: User name or password illegal!\n\n");
		return false;
	}
	if (strlen(user) > USER_NAME_LENGTH || strlen(password) > USER_PASSWORD_LENGTH)
	{
		printf("Error: User name or password illegal!\n");
		return false;
	}

	//have logged in?
	if (userID != ACCOUNT_NUM)
	{
		printf("Login failed: User has been logged in. Please log out first.\n");
		return false;
	}

	//search the user in accouting file
	for (int i = 0; i < ACCOUNT_NUM; i++)
	{
		if (strcmp(users.userName[i], user) == 0)
		{
			//find the user and check password
			if (strcmp(users.password[i], password) == 0)
			{
				//Login successfully
				printf("Login successfully.\n");
				userID = users.userID[i];
				//make user's name, root user is special
				memset(userName, 0, USER_NAME_LENGTH + 6);
				if (userID == 0)
				{
					strcat(userName, "root ");
					strcat(userName, users.userName[i]);
					strcat(userName, "$");
				}
				else
				{
					strcat(userName, users.userName[i]);
					strcat(userName, "#");
				}

				return true;
			}
			else
			{
				//Password wrong
				printf("Login failed: Wrong password.\n");
				return false;
			}
		}
	}

	//User not found
	printf("Login failed: User not found.\n");
	return false;

};

/*
*	Log out function. Remove user's states.
*/
void Logout()
{
	//remove user's states
	userID = ACCOUNT_NUM;
	memset(&users, 0, sizeof(users));
	memset(userName, 0, 6 + USER_NAME_LENGTH);
	Mount();
};


int main()
{
	memset(ab_dir, 0, sizeof(ab_dir));
	dir_pointer = 0;
	//Check file system has been formatted or not.
	FILE* fs_test = fopen("fs.unix", "r");
	if (fs_test == NULL)
	{
		printf("File system not found... Format file system first...\n\n");
		Format();
	}
	Sys_start();
	ab_dir[dir_pointer][0] = 'r';ab_dir[dir_pointer][1] = 'o';ab_dir[dir_pointer][2] = 'o';ab_dir[dir_pointer][3] = 't';ab_dir[dir_pointer][4] = '\0';
	dir_pointer++;
	//First log in
	char tmp_userName[USER_NAME_LENGTH];
	char tmp_userPassword[USER_PASSWORD_LENGTH * 5];

	do {
		memset(tmp_userName, 0, USER_NAME_LENGTH);
		memset(tmp_userPassword, 0, USER_PASSWORD_LENGTH * 5);

		printf("User log in\n\n");
		printf("User name: ");
		scanf("%s", tmp_userName);
		printf("Password : ");
		char c;
		scanf("%c", &c);
		int i = 0;
		while (1) {
			char ch;
			ch = getch();
			if (ch == '\b') {
				if (i != 0) {
					printf("\b \b");
					i--;
				}
				else {
					tmp_userPassword[i] = '\0';
				}
			}
			else if (ch == '\r') {
				tmp_userPassword[i] = '\0';
				printf("\n\n");
				break;
			}
			else {
				putchar('*');
				tmp_userPassword[i++] = ch;
			}
		}

		//scanf("%s", tmp_userPassword);
	} while (Login(tmp_userName, tmp_userPassword) != true);

	//User's mode of file system
	inode* currentInode = new inode;

	CommParser(currentInode);

	return 0;
}
//system start
void Sys_start() {
	//Install file system
	Mount();

	printf("****************************************************************\n");
	printf("*                      UNIX FILESYSTEM                         *\n");
	printf("*--------------------------------------------------------------*\n");
	printf("*                                             by:              *\n");
	printf("*                                                 R. Ariyuda   *\n");
	printf("*                                                 H. Ka Chon   *\n");
	printf("*                                               RK. Agustian   *\n");
	printf("****************************************************************\n");
}
/*
*	Recieve commands from console and call functions accordingly.
*
*	para cuurentInode: a global inode used for 'open' and 'write'
*/
void CommParser(inode*& currentInode)
{
	char para1[11];
	char para2[1024];
	bool flag = false;
	//Recieve commands
	while (true)
	{
		unsigned int f1[BLOCK_SIZE / sizeof(unsigned int)] = { 0 };
		fseek(fd, DATA_START + 8 * BLOCK_SIZE, SEEK_SET);
		fread(f1, sizeof(f1), 1, fd);
		memset(para1, 0, 11);
		memset(para2, 0, 1024);

		printf("%s>", userName);
		scanf("%s", para1);
		para1[10] = 0;			//security protection
	}
};

/*
*	Print all commands help information when 'help' is
*	called or input error occurs.
*/
void Help(){
	printf("System command:\n");
	printf("\t01.Exit system....................................(exit)\n");
	printf("\t02.Show help information..........................(help)\n");
	printf("\t03.Show current directory..........................(pwd)\n");
	printf("\t04.File list of current directory...................(ls)\n");
	printf("\t05.Enter the specified directory..........(cd + dirname)\n");
	printf("\t06.Return last directory.........................(cd ..)\n");
	printf("\t07.Create a new directory..............(mkdir + dirname)\n");
	printf("\t08.Delete the directory................(rmdir + dirname)\n");
	printf("\t09.Create a new file....................(create + fname)\n");
	printf("\t10.Open a file............................(open + fname)\n");
	printf("\t11.Read the file...................................(cat)\n");
	printf("\t12.Write the file....................(write + <content>)\n");
	printf("\t13.Copy a file..............................(cp + fname)\n");
	printf("\t14.Delete a file............................(rm + fname)\n");
	printf("\t15.System information view........................(info)\n");
	printf("\t16.Close the current user.......................(logout)\n");
	printf("\t17.Change the current user...............(su + username)\n");
	printf("\t18.Change the mode of a file.............(chmod + fname)\n");
	printf("\t19.Change the user of a file.............(chown + fname)\n");
	printf("\t20.Change the group of a file............(chgrp + fname)\n");
	printf("\t21.Rename a file............................(mv + fname)\n");
	printf("\t22.link.....................................(ln + fname)\n");
	printf("\t23.Change password..............................(passwd) \n");
	printf("\t24.User Management Menu..........................(Muser)\n");
};
