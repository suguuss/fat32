/*===========================================================================*=
   Projet        : FAT32
   Auteur        : suguuss
   Date creation : 26.04.2021
  =============================================================================
   Descriptif: Librairie FAT32
=*===========================================================================*/

#include <string.h>
#include <stdio.h>
#include "fat32.h"

/*---------------------------------------------------------------------------*-
   SwapEndianINT ()
  -----------------------------------------------------------------------------
   Descriptif: Permet de convertir une valeur (2 bytes) en little endian a une 
               valeur en big endian ou inversement 

   Entrée    : valeur sur laquelle il faut faire la conversion
   Sortie    : --
-*---------------------------------------------------------------------------*/
void SwapEndianINT(unsigned int *val)
{
   *val = (*val << 8) | (*val >> 8);
}

/*---------------------------------------------------------------------------*-
   SwapEndianLONG ()
  -----------------------------------------------------------------------------
   Descriptif: Permet de convertir une valeur (4 bytes) en little endian a une 
               valeur en big endian ou inversement 

   Entrée    : valeur sur laquelle il faut faire la conversion
   Sortie    : --
-*---------------------------------------------------------------------------*/
void SwapEndianLONG(unsigned long *val)
{
   *val = (*val << 24) | ((*val << 8) & 0x00ff0000) | ((*val >> 8) & 0x0000ff00) | (*val >> 24);
}


/*---------------------------------------------------------------------------*-
   OpenFile ()
  -----------------------------------------------------------------------------
   Descriptif: "ouvre" un fichier et retourne des informations sur un fichier
               telle que le nom et retourne également des informations sur la
               position dans le fichier (clusteur, secteur, offset) pour la lecture

   Entrée    : bs : Struct boot sector
               buf : Buffer pour écrire le contenu du secteur
               secteurDepart : Commence à chercher à ce secteur puis continu sur 1 cluster
               fe : struct FileEntry pour retourner les informations
               filename : nom du fichier à chercher
   Sortie    : Struct FileInfo qui contient la position dans le fichier
-*---------------------------------------------------------------------------*/
FileInfo OpenFile(BootSector *bs, unsigned char *buf, unsigned long secteurDepart, FileEntry *fe, char *filename)
{
   unsigned int xdata offset = 0;
   FileInfo xdata fi = {0,0,0,0,0};
   
   offset = FindFileEntry(bs, buf, secteurDepart, filename);
   
   // Attention ne marche probablement plus si le secteurDepart
   // N'est pas la racine
   if (offset != 0)
   {
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, secteurDepart + (offset / 512) );
      *fe = ReadFileEntry(buf, offset % 512);
      
      fi.currentCluster = fe->FstClusHi << 16 | fe->FstClusLO;
      fi.baseCluster = fi.currentCluster;
      fi.Offset = 0;
      fi.currentSector = 0;
      fi.fileSize = fe->fileSize;
      return fi;
   }

   return fi;
}

/*---------------------------------------------------------------------------*-
   ReadFile ()
  -----------------------------------------------------------------------------
   Descriptif: Lis un fichier depuis l'offset dans la structure FileInfo

   Entrée    : bs : Struct boot sector
               buf : Buffer pour écrire le contenu du secteur
               output : Pointeur ou les caractères lu seront stockés
               fi : FileInfo struct, contient la position dans le fichier 
               length : Nombre de caractères à lire
   Sortie    : Nombre de caractères lu
-*---------------------------------------------------------------------------*/
unsigned int ReadFile(BootSector *bs, unsigned char *buf, unsigned char *output, FileInfo *fi, unsigned int length)
{
   unsigned int xdata cpt = 0;
   
   // Pointeur vers le début du buffer
   unsigned char *basePtrBuf = buf;
   
   // Si le fichié est fini on quitte la fonction
   if (fi->Offset >= fi->fileSize)
   {
      return cpt;
   }
   
   // Lire le contenu du secteur
   SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, GetSectorFromCluster(bs, fi->currentCluster) + fi->currentSector);
   buf += fi->Offset % bs->BytsPerSec;
   
   
   for (cpt = 0; cpt < length; cpt++)
   {
      *output++ = *buf++; // Lis le byte et incrémente les addresses
      fi->Offset++; // Incrémente la position dans le fichier
      
      if (fi->Offset >= fi->fileSize) // Check fin du fichier
      {
         // Si on atteind la fin du fichier, on retourne
         // Le nombre de bytes lu
         *output = 0;
         return cpt;
      }
      
      if ((fi->Offset % bs->BytsPerSec) == 0) // Check fin de secteur
      {
         if (++fi->currentSector == bs->SecPerClus) // Check fin de cluster
         {
            // Lire la table fat 
            buf = basePtrBuf;
            fi->currentCluster = GetNextClusterValue(bs, buf, fi->currentCluster);
            fi->currentSector = 0;
            SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, GetSectorFromCluster(bs, fi->currentCluster) + fi->currentSector);
         }
         else
         {
            buf = basePtrBuf;
            SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, GetSectorFromCluster(bs, fi->currentCluster) + fi->currentSector);
         }
      }
   }
   
   *output = 0;
   return cpt;
}

/*---------------------------------------------------------------------------*-
   WriteFile ()
  -----------------------------------------------------------------------------
   Descriptif: Ecris dans un fichier en mode append

   Entrée    : bs : Struct boot sector
               buf : Buffer pour écrire le contenu du secteur
               fe : FileEntry struct, contient l'entrée du fichier
               fi : FileInfo struct, contient la position dans le fichier 
               texte : Contenu à écrire
               length : Nombre de caractères à lire
   Sortie    : --
-*---------------------------------------------------------------------------*/
void WriteFile(BootSector *bs, unsigned char *buf, FileInfo *fi, FileEntry *fe, unsigned char *texte, unsigned int length)
{
   unsigned char name[12];
   unsigned int offset = 0, x = 0;
   unsigned long cluster;
   unsigned long secteur = 0;
   
   FileSeek(bs, buf, fi, fi->fileSize, SEEK_SET);
   
   
   
   while (length != 0)
   {
      // Lecture du secteur
      secteur = GetSectorFromCluster(bs, fi->currentCluster) + fi->currentSector;
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, secteur);
      
      for (x = (fi->Offset % 512); x < 512; x++)
      {
         buf[x] = *texte++;
         fi->fileSize++;
         fi->Offset++;
         length--;
         if (length == 0) break;
      }
      // Ecriture des modifications
      
      DEBUG_3 = SD_WriteBlock(TOKEN_RW, buf, bs->BytsPerSec, secteur);
      
      
      // Si fin du secteur
      if ((fi->Offset % 512) == 0) 
      {
         fi->currentSector++;
      }
      
      // Fin du cluster
      if (fi->currentSector >= bs->SecPerClus)
      {
         // Allocation d'un nouveau cluster
         cluster = FindFreeCluster(bs, buf);
         SetNextClusterValue(bs, buf, fi->currentCluster, cluster);
         
         fi->currentCluster = cluster;
         fi->currentSector = 0;
         
         // Vide le cluster
         memset(buf, 0, 512);
         for (x = 0; x < 8; x++)
         {
            SD_WriteBlock(TOKEN_RW, buf, bs->BytsPerSec, GetSectorFromCluster(bs, fi->currentCluster) + x);
         }
      }
   }
   
   // Change la taille du fichier dans l'entrée
   SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RootDirSector);
   
   strncpy(name, fe->Name, 11);
   CleanFilename(name);
   offset = FindFileEntry(bs, buf, bs->RootDirSector, name);
   
   SwapEndianLONG(&fi->fileSize);
   memcpy(buf + offset + FILESIZE_OFFSET, &fi->fileSize, 4);
   SwapEndianLONG(&fi->fileSize);
   
   SD_WriteBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RootDirSector);
}


/*---------------------------------------------------------------------------*-
   FileSeek ()
  -----------------------------------------------------------------------------
   Descriptif: Déplace le curseur du fichier

   Entrée    : bs : Struct boot sector
               buf : Buffer pour écrire le contenu du secteur
               fi : FileInfo struct, contient la position dans le fichier 
               offset : Nombre de byte à se déplacer
               mode : SEEK_CUR : Se déplace depuis le curseur
                      SEEK_SET : Se déplace depuis le début du fichier
   Sortie    : SUCCESS (1) ou FAILED (0)
-*---------------------------------------------------------------------------*/
bit FileSeek(BootSector *bs, unsigned char *buf, FileInfo *fi, unsigned long offset, bit mode)
{
   unsigned char xdata nbSec = 0, nbClus = 0;
   
   
   if (mode == SEEK_CUR)
   {
      // Si offset plus loin que fin du fichier
      if (offset > fi->fileSize) return FAILED;
      
      nbSec = (offset + fi->Offset) / bs->BytsPerSec;
      nbClus = nbSec / bs->SecPerClus;
      
      for (nbClus; nbClus > 0; nbClus--)
      {
         fi->currentCluster = GetNextClusterValue(bs, buf, fi->currentCluster);
      }
      
      fi->currentSector = nbSec % bs->SecPerClus;
      fi->Offset = offset + fi->Offset;
   }
   else // SEEK_SET
   {
      // Si offset plus loin que fin du fichier
      if (offset > fi->fileSize) return FAILED;
      
      nbSec = offset / bs->BytsPerSec;
      nbClus = nbSec / bs->SecPerClus;
      
      //if (nbClus == 0) fi->currentCluster = fi->baseCluster;
      fi->currentCluster = fi->baseCluster;
      
      for (nbClus; nbClus > 0; nbClus--)
      {
         fi->currentCluster = GetNextClusterValue(bs, buf, fi->currentCluster);
      }
      
      fi->currentSector = nbSec % bs->SecPerClus;
      fi->Offset = offset;
   }
   
   return SUCCESS;
}



/*---------------------------------------------------------------------------*-
   CleanFilename ()
  -----------------------------------------------------------------------------
   Descriptif: Formate correctement un nom de fichier lu sur la carte

   Entrée    : filename : nom du fichier lu
   Sortie    : Taille du nom formaté
-*---------------------------------------------------------------------------*/
unsigned char CleanFilename(char *filename)
{
   unsigned char xdata cleanCharCnt = 0;
   unsigned char xdata charCnt = 0;
   
   // Ajoute tous les charactères jusqu'a ce que ce soit un espace
   while(filename[charCnt] != ' ' && charCnt < 8)
   {
      filename[cleanCharCnt] = filename[charCnt];
      
      // Si le charactere est une lettre en majuscule, la mettre en minuscule
      if ( (filename[cleanCharCnt]>='A') && (filename[cleanCharCnt]<='Z') )
      {
         filename[cleanCharCnt] += 0x20;
      }
      
      charCnt++;
      cleanCharCnt++;
   }
   
   if (filename[8] != 0x20)
   {
      // Ajoute le point
      filename[cleanCharCnt] = '.';
      cleanCharCnt++;
      
      // Ajoute l'extension et le Zero Terminal
      for (charCnt = 8; charCnt < 11; charCnt++)
      {
         filename[cleanCharCnt] = filename[charCnt];
         
         // Si le charactere est une lettre en majuscule, la mettre en minuscule
         if ( (filename[cleanCharCnt]>='A') && (filename[cleanCharCnt]<='Z') )
         {
            filename[cleanCharCnt] += 0x20;
         }
         
         cleanCharCnt++;
      }
   }
   
   filename[cleanCharCnt] = 0;
   
   return cleanCharCnt;
}


/*---------------------------------------------------------------------------*-
   GetNextClusterValue ()
  -----------------------------------------------------------------------------
   Descriptif: Lis dans la table FAT la valeur du prochain cluster 

   Entrée    : clusterNumber : Valeur du cluster actuel
   Sortie    : Valeur du cluster suivant
-*---------------------------------------------------------------------------*/
unsigned long GetNextClusterValue(BootSector *bs, unsigned char *buf, unsigned long clusterNumber)
{
   unsigned long xdata FATOffset = clusterNumber * 4;
   unsigned long xdata nextClusterNumber = 0;
   // Lis le bloc ou se trouve la valeur
   SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RsvdSecCnt + (FATOffset / bs->BytsPerSec));
   // Lis la valeur
   memcpy(&nextClusterNumber, buf+(FATOffset % bs->BytsPerSec), 4);
   // Change l'endiannes
   SwapEndianLONG(&nextClusterNumber);
   
   return nextClusterNumber;
}

/*---------------------------------------------------------------------------*-
   SetNextClusterValue ()
  -----------------------------------------------------------------------------
   Descriptif: Lis dans la table FAT la valeur du prochain cluster 

   Entrée    : clusterNumber : Valeur du cluster actuel
               nextClusterNumber : Valeur du cluster suivant
   Sortie    : --
-*---------------------------------------------------------------------------*/
void SetNextClusterValue(BootSector *bs, unsigned char *buf, unsigned long clusterNumber, unsigned long nextClusterNumber)
{
   unsigned long xdata FATOffset = 0;
   unsigned long EOFMark = END_OF_FILE_MARK;
   unsigned char x = 0;
   
   SwapEndianLONG(&EOFMark);
   
   for (x = 0; x < bs->NumFATs; x++)
   {
      FATOffset = clusterNumber * 4;
      // POINTE ANCIEN CLUSTER AU NOUVEAU CLUSTER
      // Lis le bloc ou se trouve la valeur
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RsvdSecCnt + (FATOffset / bs->BytsPerSec) + (x * bs->FATSz32));
      // Change l'endiannes
      SwapEndianLONG(&nextClusterNumber);
      // Stocke la valeur
      memcpy(buf+(FATOffset % bs->BytsPerSec), &nextClusterNumber, 4);
      SD_WriteBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RsvdSecCnt + (FATOffset / bs->BytsPerSec) + (x * bs->FATSz32));
      
      // INDIQUE FIN DU FICHIER SUR NOUVEAU CLUSTER
      SwapEndianLONG(&nextClusterNumber);
      FATOffset = nextClusterNumber * 4;
      // Lis le bloc ou se trouve le nouveau cluster
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RsvdSecCnt + (FATOffset / bs->BytsPerSec) + (x * bs->FATSz32));
      // Stocke la valeur End Of File
      memcpy(buf+(FATOffset % bs->BytsPerSec), &EOFMark, 4);
      SD_WriteBlock(TOKEN_RW, buf, bs->BytsPerSec, bs->RsvdSecCnt + (FATOffset / bs->BytsPerSec) + (x * bs->FATSz32));
   }
}

/*---------------------------------------------------------------------------*-
   GetSectorFromCluster ()
  -----------------------------------------------------------------------------
   Descriptif: Retourne la valeur du cluster suivant

   Entrée    : Numéro de cluster actuel
   Sortie    : Numéro de cluster suivant
-*---------------------------------------------------------------------------*/
unsigned long GetSectorFromCluster(BootSector *bs, unsigned long cluster)
{
   return (cluster - 2) * bs->SecPerClus + bs->RootDirSector;
}

/*---------------------------------------------------------------------------*-
   FindFreeCluster ()
  -----------------------------------------------------------------------------
   Descriptif: Retourne la valeur du cluster suivant

   Entrée    : bs : Contenu du boot sector
               buf : Buffer pour stocker le contenu du secteur
   Sortie    : Offset du cluster vide dans la table FAT
-*---------------------------------------------------------------------------*/
unsigned long FindFreeCluster(BootSector *bs, unsigned char *buf)
{
   unsigned long sector = bs->RsvdSecCnt;
   unsigned long clusterValue = 0;
   unsigned char x = 0;

   while (sector != (sector + bs->FATSz32))
   {
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, sector);

      for (x = 0; x < (bs->BytsPerSec/4); x++)
      {
         memcpy(&clusterValue, buf+(x*4), 4);
         if (clusterValue == 0)
         {
            return ((sector - bs->RsvdSecCnt) * 128 + x);
         }
      }

      sector++;
   }

   return 0xFFFFFFFF; // PAS DE CLUSTER VIDE
}


/*---------------------------------------------------------------------------*-
   FindFileEntry ()
  -----------------------------------------------------------------------------
   Descriptif: Cherche dans un dossier l'entrée d'un fichier et retourne l'offset  

   Entrée    : bs : Struct boot sector
               buf : Buffer pour écrire le contenu du secteur
               secteurDepart : Commence à chercher à ce secteur puis continu sur 1 cluster
               filename : nom du fichier à chercher
   Sortie    : Offset du fichier depuis le début du cluster
-*---------------------------------------------------------------------------*/
unsigned int FindFileEntry(BootSector *bs, char *buf, unsigned long secteurDepart, char *filename)
{
   unsigned char xdata name[12];
   unsigned int xdata offset = 0;
   unsigned char xdata secteur = 0;
   
   do
   {
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, secteurDepart + secteur);
      
      for (offset = 0; offset < 512; offset+=32)
      {
         memcpy(name, buf+offset, 11);
         
         if (name[0] == 0) break; // Fin des entrées
         
         // Skip les noms long et les fichiers supprimés
         if (name[2] != 0 && name[0] != 0xE5)
         {
            CleanFilename(name);
            if (strncmp(name, filename, strlen(filename)) == 0)
            {
               return offset + (secteur * 512);
            }
         }
      }
      
      secteur++;
      if (secteur >= bs->SecPerClus) break;
      
   } while (name[0] != 0);
   
   return 0;
}


/*---------------------------------------------------------------------------*-
   ListFilesDirectory ()
  -----------------------------------------------------------------------------
   Descriptif: Liste tous les fichiers présents dans un dossier 

   Entrée    : Structure bootsector, secteur de départ
   Sortie    : --

   info : La fonction cherche des fichiers seulement sur 1 cluster
   pour l'instant

   Uitlisation de la variable globale buffer et texte
-*---------------------------------------------------------------------------*/
void ListFilesDirectory(BootSector *bs, unsigned char *buf, unsigned char *texte, unsigned long secteurDepart)
{
   unsigned char x = 0, fileNameSize = 0;
   unsigned int entryOffset = 0, listOffset = 0;
   xdata FileEntry tempFe;
   
   for (x = 0; x < bs->SecPerClus; x++) // Check all sectors in the cluster
   {
      SD_ReadBlock(TOKEN_RW, buf, bs->BytsPerSec, secteurDepart + x);
      
      if (secteurDepart + x == bs->RootDirSector){entryOffset = 32;}
      else {entryOffset = 0;}
      
      for (entryOffset; entryOffset < bs->BytsPerSec; entryOffset+=32)
      {
         tempFe = ReadFileEntry(buf, entryOffset);
         if (tempFe.Name[0] == 0x00) return; // Fin des entrées, quitte la fonction
         else if (tempFe.Name[0] == 0xE5);   // Fichier supprimé
         else if (tempFe.Name[2] == 0x00);   // Long file entry not supported
         else if (!strncmp(tempFe.Name, "SYSTEM~", 7));
         else                                // Fichier valide
         {
            fileNameSize = CleanFilename(tempFe.Name);
            strncpy(texte+listOffset, tempFe.Name, fileNameSize);
            listOffset += fileNameSize;
            texte[listOffset] = ';';
            listOffset++;
         }
      }
   }
}

/*---------------------------------------------------------------------------*-
   ParseBootSector ()
  -----------------------------------------------------------------------------
   Descriptif: Lis les informations utiles du Boot Sector pour trouver le 
               Root directory

   Entrée    : Secteur lu
   Sortie    : Struct BootSector, contient toutes les infos lues
-*---------------------------------------------------------------------------*/
BootSector ParseBootSector(unsigned char *buf)
{
   BootSector xdata bootSector;
   SD_ReadBlock(TOKEN_RW, buf, NB_BYTES_SECTOR, 0);
   
   PARSE_INFO_INT (bootSector, BytsPerSec, buf, BYTSPERSEC_OFFSET)
   PARSE_INFO_CHAR(bootSector, SecPerClus, buf, SECPERCLUS_OFFSET)
   PARSE_INFO_INT (bootSector, RsvdSecCnt, buf, RSVDSECCNT_OFFSET)
   PARSE_INFO_CHAR(bootSector, NumFATs   , buf, NUMFATS_OFFSET)
   PARSE_INFO_LONG(bootSector, FATSz32   , buf, FATSz32_OFFSET)
   PARSE_INFO_LONG(bootSector, RootClus  , buf, ROOTCLUS_OFFSET)
   
   bootSector.RootDirSector = bootSector.RsvdSecCnt + (bootSector.NumFATs * bootSector.FATSz32);
   
   return bootSector;
}

/*---------------------------------------------------------------------------*-
   ReadFileEntry ()
  -----------------------------------------------------------------------------
   Descriptif: Lis une entrée de fichier et la stocke dans une structure
               FileEntry

   Entrée    : buf : Buffer qui contient le secteur
               offset : Offset auquel se trouve le fichier
   Sortie    : Structure contenant les informations
-*---------------------------------------------------------------------------*/
FileEntry ReadFileEntry(unsigned char *buf, unsigned int offset)
{
   FileEntry xdata tempFile;
   
   
   //Récupérer les infos et les mettre dans la structure
   PARSE_INFO_CHAR(tempFile, Name,         buf, NAME_OFFSET + offset)
//   PARSE_INFO_CHAR(tempFile, Attr,         buf, ATTR_OFFSET + offset)
//   PARSE_INFO_CHAR(tempFile, CrtTimeTenth, buf, CRTTIMETENTH_OFFSET + offset)
//   PARSE_INFO_INT (tempFile, CrtTime,      buf, CRTTIME_OFFSET + offset)
//   PARSE_INFO_INT (tempFile, CrtDate,      buf, CRTDATE_OFFSET + offset)
//   PARSE_INFO_INT (tempFile, LstAccDate,   buf, LSTACCDATE_OFFSET + offset)
   PARSE_INFO_INT (tempFile, FstClusHi,    buf, FSTCLUSHI_OFFSET + offset)
//   PARSE_INFO_INT (tempFile, WrtTime,      buf, WRTTIME_OFFSET + offset)
//   PARSE_INFO_INT (tempFile, WrtDate,      buf, WRTDATE_OFFSET + offset)
   PARSE_INFO_INT (tempFile, FstClusLO,    buf, FSTCLUSLO_OFFSET + offset)
   PARSE_INFO_LONG(tempFile, fileSize,     buf, FILESIZE_OFFSET + offset)
   
   return tempFile;
}
