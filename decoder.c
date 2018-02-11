#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "endian.h"
#include "crc.c"

#define INTSIZE 0x01000000    //16,777,216 bytes
#define COMPSIZE 0x02000000   //33,554,432 bytes
#define DECOMPSIZE 0x04000000 //67,108,864 bytes

uint32_t* inROM;
uint32_t* outROM;
uint32_t* inTable;
uint32_t* outTable;

typedef struct t_offsets
{
	uint32_t startV;   //Start Virtual Address
	uint32_t endV;     //End Virtual Address
	uint32_t startP;   //Start Physical Address
	uint32_t endP;     //End Phycical Address
}
Offsets;

void decode(uint8_t*, uint8_t*, int32_t);
Offsets findOffsets(uint32_t);
void setOffsets(uint32_t, Offsets);
void loadROM(char*);
int32_t findTable();

int main(int argc, char** argv)
{
	FILE* outFile;
	int32_t tableOffset, tableSize, tableCount, size, i;
	Offsets offsets, curFile;

	inROM = malloc(DECOMPSIZE);
	outROM = malloc(DECOMPSIZE);

	//Load the ROM into inROM and outROM
	loadROM(argv[1]);

	//Find table offsets
	tableOffset = findTable();
	inTable = (void*)inROM + tableOffset;
	outTable = (void*)outROM + tableOffset;
	offsets = findOffsets(2);
	tableSize = offsets.endV - offsets.startV;
	tableCount = tableSize / 16;
	i = tableCount - 1;

	//Change everything past the table in outROM to 0
	memset((uint8_t*)(outROM) + offsets.endV, 0, DECOMPSIZE - offsets.endV);

	while(i >= 0)
	{
		curFile = findOffsets(i);
		size = curFile.endV - curFile.startV;

		//Don't do anything to this one for some reason. Not sure why, but it breaks otherwise
		if(i == 2)
		{
			i--;
			continue;
		}

		//Copy if decoded, decode if encoded
		if(curFile.endP == 0x00000000)
			memcpy((void*)outROM + curFile.startV, (void*)inROM + curFile.startP, size);
		else
			decode((void*)inROM + curFile.startP, (void*)outROM + curFile.startV, size);

		//Clean up outROM's table
		curFile.startP = curFile.startV;
		curFile.endP = 0x00000000;
		setOffsets(i, curFile);
		i--;
	}

	//Write the new ROM
	outFile = fopen(argv[2], "wb");
	fwrite(outROM, sizeof(uint32_t), INTSIZE, outFile);
	fclose(outFile);

	//I have no idea what's going on with this. I think it's just Nintendo magic
	fix_crc(argv[2]);

	return(0);
}

/*
	Function: findTable
	Description: Finds the table offset for the inROM (the encoded one)
	Paramaters: N/A (Uses some globals)
	Returns: Offset of table
*/
int32_t findTable()
{
	int32_t i = 0;

	while(i+4 < INTSIZE)
	{
		//Thsese values mark the begining of the file table
		if(htobe32(inROM[i]) == 0x7A656C64)
		{
			if(htobe32(inROM[i+1]) == 0x61407372)
			{
				if((htobe32(inROM[i+2]) & 0xFF000000) == 0x64000000)
				{
					//Search for the begining of the filetable
					while(htobe32(inROM[i]) != 0x00001060)
						i += 4;
					return((i-4) * sizeof(uint32_t));
				}
			}
		}

		i += 4;
	}

	fprintf(stderr, "Error: Couldn't find table\n");
	exit(1);
}

/*
	Function: loadROM
	Description: Loads the ROM from a file into two arrays
	Paramaters: Name of the file to open (Uses some globals)
	Returns: N/A
*/
void loadROM(char* name)
{
	uint32_t size;
	FILE* romFile;
	
	//Open file, make sure it exists
	romFile = fopen(name, "rb");
	perror(name);
	if(romFile == NULL)
		exit(1);
	
	//Find size of file
	fseek(romFile, 0, SEEK_END);
	size = ftell(romFile);
	fseek(romFile, 0, SEEK_SET);

	//If it's not the right size, exit
	if(size != COMPSIZE)
	{
		fprintf(stderr, "Error, %s is not the correct size", name);
		exit(1);
	}

	//Read to inROM, close romFile, and copy to outROM
	fread(inROM, sizeof(char), size, romFile);
	fclose(romFile);
	memcpy(outROM, inROM, size);
}

/*
	Function: findOffsets
	Description: Finds a given entry in the inTable array
	Paramaters: Number of the entry to find (Uses some globals)
	Returns: Offsets struct containing table info
*/
Offsets findOffsets(uint32_t i)
{
	Offsets offsets;

	//First 32 bytes are VROM start address, next 32 are VROM end address
	//Next 32 bytes are Physical start address, last 32 are Physical end address
	offsets.startV = htobe32(inTable[i*4]);
	offsets.endV   = htobe32(inTable[(i*4)+1]);
	offsets.startP = htobe32(inTable[(i*4)+2]);
	offsets.endP   = htobe32(inTable[(i*4)+3]);

	return(offsets);
}

/*
	Function: setOffsets
	Description: Sets the table vales in outTable 
	Paramaters: Number of the entry to set, Offset struct containing table info (Uses some globals)
	Returns: N/A
*/
void setOffsets(uint32_t i, Offsets offsets)
{
	//First 32 bytes are VROM start address, next 32 are VROM end address
	//Next 32 bytes are Physical start address, last 32 are Physical end address
	outTable[i*4]     = htobe32(offsets.startV);
	outTable[(i*4)+1] = htobe32(offsets.endV);
	outTable[(i*4)+2] = htobe32(offsets.startP);
	outTable[(i*4)+3] = htobe32(offsets.endP);
}

/*
	Function: decode
	Description: Decodes/decompresses portions of data using Yaz0 decoding
	Paramaters: Source array, Destination array, Size of decompressed data
	Returns: N/A
*/
void decode(uint8_t* source, uint8_t* decomp, int32_t decompSize)
{
	uint32_t srcPlace = 0, dstPlace = 0;
	uint32_t i, dist, copyPlace, numBytes;
	uint8_t codeByte, byte1, byte2;
	uint8_t bitCount = 0;

	source += 0x10;
	while(dstPlace < decompSize)
	{
		//If there are no more bits to test, get a new byte
		if(!bitCount)
		{
			codeByte = source[srcPlace++];
			bitCount = 8;
		}

		//If bit 7 is a 1, just copy 1 byte from source to destination
		//Else do some decoding
		if(codeByte & 0x80)
		{
			decomp[dstPlace++] = source[srcPlace++];
		}
		else
		{
			//Get 2 bytes from source
			byte1 = source[srcPlace++];
			byte2 = source[srcPlace++];

			//Calculate distance to move in destination
			//And the number of bytes to copy
			dist = ((byte1 & 0xF) << 8) | byte2;
			copyPlace = dstPlace - (dist + 1);
			numBytes = byte1 >> 4;

			//Do more calculations on the number of bytes to copy
			if(!numBytes)
				numBytes = source[srcPlace++] + 0x12;
			else
				numBytes += 2;

			//Copy data from a previous point in destination
			//to current point in destination
			for(i = 0; i < numBytes; i++)
				decomp[dstPlace++] = decomp[copyPlace++];
		}

		//Set up for the next read cycle
		codeByte = codeByte << 1;
		bitCount--;
	}
}