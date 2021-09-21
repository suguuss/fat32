/*===========================================================================*=
   CFPT - Projet : FAT32
   Auteur        : Nicolas Albanesi
   Date creation : 26.04.2021
  =============================================================================
   Descriptif: Librairie FAT32
=*===========================================================================*/

#include <reg51f380.h>

#ifndef	__FAT32_H__
#define __FAT32_H__

#define SUCCESS 1
#define FAILED 0


// CARTE SD
#define TOKEN_RW 0xFE
#define NB_BYTES_SECTOR 512


// FAT32
// BOOT SECTOR
#define BYTSPERSEC_OFFSET 		0x0B // 11 
#define SECPERCLUS_OFFSET 		0x0D // 13
#define RSVDSECCNT_OFFSET 		0x0E // 14
#define NUMFATS_OFFSET    		0x10 // 16
#define FATSz32_OFFSET    		0x24 // 36
#define ROOTCLUS_OFFSET   		0x2C // 44

// DIR ENTRY
#define NAME_OFFSET          	0x00 // 00
#define ATTR_OFFSET          	0x0B // 11
#define CRTTIMETENTH_OFFSET  	0x0D // 13
#define CRTTIME_OFFSET       	0x0E // 14
#define CRTDATE_OFFSET       	0x10 // 16
#define LSTACCDATE_OFFSET    	0x12 // 18
#define FSTCLUSHI_OFFSET     	0x14 // 20
#define WRTTIME_OFFSET       	0x16 // 22
#define WRTDATE_OFFSET       	0x18 // 24
#define FSTCLUSLO_OFFSET     	0x1A // 26
#define FILESIZE_OFFSET      	0x1C // 28


// FILE SEEK
#define SEEK_SET 0
#define SEEK_CUR 1


#define END_OF_FILE_MARK 0x0FFFFFFF

// MACROS
#define PARSE_INFO_INT(structure, info, buffer, offset)  memcpy(&structure.info, buffer+offset, sizeof(structure.info)); SwapEndianINT(&structure.info);
#define PARSE_INFO_LONG(structure, info, buffer, offset) memcpy(&structure.info, buffer+offset, sizeof(structure.info)); SwapEndianLONG(&structure.info);
#define PARSE_INFO_CHAR(structure, info, buffer, offset) memcpy(&structure.info, buffer+offset, sizeof(structure.info));

sbit DEBUG_3 = P2^6;

// Informations utiles du Boot sector 
// Taille de la struct : 16 bytes
typedef struct 
{
	unsigned int  BytsPerSec;
	unsigned char SecPerClus;
	unsigned int  RsvdSecCnt;
	unsigned char NumFATs;
	unsigned long FATSz32;
	unsigned long RootClus;
	unsigned int  RootDirSector;  // Not really in the boot sector
} BootSector;


// Structure d'un fichier (sans le nom long)
// Taille de la struct : 19 bytes
typedef struct
{
	unsigned char Name[11];
	unsigned int  FstClusHi;
	unsigned int  FstClusLO;
	unsigned long fileSize;
	//unsigned char Attr;
	//unsigned char CrtTimeTenth;
	//unsigned int  CrtTime;
	//unsigned int  CrtDate;
	//unsigned int  LstAccDate;
	//unsigned int  WrtTime;
	//unsigned int  WrtDate;
} FileEntry;

// Structure qui permet de connaitre l'emplacement dans le fichier
typedef struct 
{
   unsigned long baseCluster;
   unsigned long currentCluster;
   unsigned long Offset;
   unsigned long fileSize;
   unsigned char currentSector;
} FileInfo;

// Fonction d'abstraction
extern bit SD_ReadBlock(unsigned char token, unsigned char *buf, unsigned int nbBytes, unsigned long sectorAddr);
extern bit SD_WriteBlock(unsigned char token, unsigned char *buf, unsigned int nbBytes, unsigned long blkAddr);

// Swap endian
void SwapEndianINT(unsigned int *val);
void SwapEndianLONG(unsigned long *val);


FileInfo OpenFile(BootSector *bs, unsigned char *buf, unsigned long secteurDepart, FileEntry *fe, char *filename);
unsigned int ReadFile(BootSector *bs, unsigned char *buf, unsigned char *output, FileInfo *fi, unsigned int length);
bit FileSeek(BootSector *bs, unsigned char *buf, FileInfo *fi, unsigned long offset, bit mode);
void WriteFile(BootSector *bs, unsigned char *buf, FileInfo *fi, FileEntry *fe, unsigned char *texte, unsigned int length);


unsigned char CleanFilename(char *filename);
unsigned long GetNextClusterValue(BootSector *bs, unsigned char *buf, unsigned long clusterNumber);
void SetNextClusterValue(BootSector *bs, unsigned char *buf, unsigned long clusterNumber, unsigned long nextClusterNumber);
unsigned long GetSectorFromCluster(BootSector *bs, unsigned long cluster);
unsigned long FindFreeCluster(BootSector *bs, unsigned char *buf);
unsigned int FindFileEntry(BootSector *bs, char *buf, unsigned long secteurDepart, char *filename);
void ListFilesDirectory(BootSector *bs, unsigned char *buf, unsigned char *texte, unsigned long secteurDepart);


BootSector ParseBootSector(unsigned char *buf);
FileEntry ReadFileEntry(unsigned char *buf, unsigned int offset);

#endif














