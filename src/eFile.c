// eFile.c
// Runs on either TM4C123 or MSP432
// High-level implementation of the file system implementation.
// Daniel and Jonathan Valvano
// August 29, 2016
#include <stdint.h>
#include <string.h>
#include "eDisk.h"

uint8_t Buff[512]; // temporary buffer used during file I/O
uint8_t Directory[256], FAT[256];
int32_t bDirectoryLoaded =0; // 0 means disk on ROM is complete, 1 means RAM version active

// Return the larger of two integers.
int16_t max(int16_t a, int16_t b){
  if(a > b){
    return a;
  }
  return b;
}
//*****MountDirectory******
// if directory and FAT are not loaded in RAM,
// bring it into RAM from disk
void MountDirectory(void){ 
// if bDirectoryLoaded is 0, 
//    read disk sector 255 and populate Directory and FAT
//    set bDirectoryLoaded=1
// if bDirectoryLoaded is 1, simply return

  if (bDirectoryLoaded == 0)
  {
    // init eDisk
    eDisk_Init(0);

    // read disk sector 255
    eDisk_ReadSector(Buff, 255);

    // copy first 256 bytes to directory 
    memcpy(Directory, &Buff[0], sizeof(Directory));

    // copy last 256 bytes to FAT
    memcpy(FAT, &Buff[256], sizeof(FAT));

    // disk is mounted
    bDirectoryLoaded = 1;
  }
}

// Return the index of the last sector in the file
// associated with a given starting sector.
// Note: This function will loop forever without returning
// if the file has no end (i.e. the FAT is corrupted).
uint8_t lastsector(uint8_t start){

  uint8_t result;

  if (start == 255)
  {
    result = 255;
  }
  else
  {
    uint8_t  next_sector = start;
		result 							 = start;
    while (next_sector != 255)
    {
			result = next_sector;
      next_sector = FAT[next_sector];      
    }
  }
	
  return result; // replace this line
}

// Return the index of the first free sector.
// Note: This function will loop forever without returning
// if a file has no end or if (Directory[255] != 255)
// (i.e. the FAT is corrupted).
uint8_t findfreesector(void){
// **write this function**
  
  // find the last sector of first file;
  uint8_t filenum    = 0;
  int     freesector = -1;
  uint8_t ls         = 0;

  // find the last free sector of the first file
  ls = lastsector(Directory[filenum]);

  // loop terminate if the next file doesn't exist
  while (ls != 255)
  {
    // store the largest known last sector
    freesector = max(freesector, ls);
    
    // search the next file
    filenum++;

    // find the last free sector of the current file
    ls = lastsector(Directory[filenum]);
  }

  // free sector is the last sector of all files + 1 or 
  // the 1st sector if disk is formatted
  freesector = freesector + 1;

  return freesector; // replace this line
}

// Append a sector index 'n' at the end of file 'num'.
// This helper function is part of OS_File_Append(), which
// should have already verified that there is free space,
// so it always returns 0 (successful).
// Note: This function will loop forever without returning
// if the file has no end (i.e. the FAT is corrupted).
uint8_t appendfat(uint8_t num, uint8_t n){  
  // find the last sector of the file
  uint8_t last = lastsector(Directory[num]);

  if (last == 255)
  {
    // first file ever, write to directory
    Directory[num] = n;
  }
  else
  {
    // append the file to the FAT by updating the tail of file 
    FAT[last] = n;
  }
  return 0;
}

//********OS_File_New*************
// Returns a file number of a new file for writing
// Inputs: none
// Outputs: number of a new file
// Errors: return 255 on failure or disk full
uint8_t OS_File_New(void){

  uint8_t result = 255;

  if (!bDirectoryLoaded)
  {
    MountDirectory();
  }

  // search the directory for the next available free file, aka 255
  for (uint32_t file_id = 0; file_id < sizeof(Directory); file_id++)
  {
    if (255 == Directory[file_id])
    {
      result = file_id;
      break;
    }
  }
	
  return result;
}

//********OS_File_Size*************
// Check the size of this file
// Inputs:  num, 8-bit file number, 0 to 254
// Outputs: 0 if empty, otherwise the number of sectors
// Errors:  none
uint8_t OS_File_Size(uint8_t num){
  
  uint8_t size = 0;
  uint8_t start = Directory[num];
  
  if (start == 255)
  {
    // empty
    size = 0;
  }
  else
  {
    uint8_t  next_sector = start;
    while (next_sector != 255)
    {
      size++;
      next_sector = FAT[next_sector];
    }
  }
	
  return size;
}

//********OS_File_Append*************
// Save 512 bytes into the file
// Inputs:  num, 8-bit file number, 0 to 254
//          buf, pointer to 512 bytes of data
// Outputs: 0 if successful
// Errors:  255 on failure or disk full
uint8_t OS_File_Append(uint8_t num, uint8_t buf[512]){
  
  if (!bDirectoryLoaded)
  {
    MountDirectory();
  }

  uint8_t target_sector = findfreesector();

  if (target_sector == 255)
  {
    return 255;
  }
  
  // write data to NVM
  if (RES_OK == eDisk_WriteSector(buf, target_sector))
  {
    // update the FAT
    appendfat(num, target_sector);

    return 0;
  }
  else
  {
    return 255;
  }
}

//********OS_File_Read*************
// Read 512 bytes from the file
// Inputs:  num, 8-bit file number, 0 to 254
//          location, logical address, 0 to 254
//          buf, pointer to 512 empty spaces in RAM
// Outputs: 0 if successful
// Errors:  255 on failure because no data
uint8_t OS_File_Read(uint8_t num, uint8_t location,
                     uint8_t buf[512]){
  if (!bDirectoryLoaded)
  {
    MountDirectory();
  }

  uint8_t result = 255;

  // get the start sector for a file
  uint8_t target = Directory[num];

  if (target < 255)
  {
    // find the sector in the file corresponding to the location
    uint8_t target_sector = target;

    for (uint8_t i = 0; i < location; i++)
    {
      // go to the next sector
      target_sector = FAT[target_sector];
    }

    eDisk_ReadSector(buf, target_sector);
  }


  return result; // replace this line
}

//********OS_File_Flush*************
// Update working buffers onto the disk
// Power can be removed after calling flush
// Inputs:  none
// Outputs: 0 if success
// Errors:  255 on disk write failure
uint8_t OS_File_Flush(void){
  // erase sector 255
  memcpy(&Buff[0], Directory, sizeof(Directory));

  memcpy(&Buff[256], FAT, sizeof(FAT));
  
  // write the directory and FAT to sector 255
  if (RES_OK == eDisk_WriteSector(Buff, 255))
  { 
    return 0;
  }
  else
  {
    return 255;
  }
}

//********OS_File_Format*************
// Erase all files and all data
// Inputs:  none
// Outputs: 0 if success
// Errors:  255 on disk write failure
uint8_t OS_File_Format(void){
// call eDiskFormat
// clear bDirectoryLoaded to zero

  if (RES_OK == eDisk_Format())
  {
    bDirectoryLoaded = 0;
    
    return 0;
  }
  else
  {
    return 255;
  }
}
