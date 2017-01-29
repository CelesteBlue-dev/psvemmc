#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <array>
#include <iomanip>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "EmmcClient.h"
#include "SceMbr.h"

//defalut size of sector for SD MMC protocol
#define SD_DEFAULT_SECTOR_SIZE 0x200

int emmc_ping(SOCKET socket)
{
   command_0_request cmd0;
   cmd0.command = PSVEMMC_COMMAND_PING;

   int iResult = send(socket, (const char*)&cmd0, sizeof(command_0_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_0_response resp0;
   iResult = recv(socket, (char*)&resp0, sizeof(command_0_response), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   if(resp0.base.command != PSVEMMC_COMMAND_PING || resp0.base.vita_err < 0 || resp0.base.proxy_err < 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   if(strncmp(resp0.data, "emmcproxy", 10) != 0)
      return -1;

   return 0;
}

int emmc_init(SOCKET socket, int bytesPerSector, int sectorsPerCluster)
{
   command_2_request cmd2;
   cmd2.command = PSVEMMC_COMMAND_INIT;
   cmd2.bytesPerSector = bytesPerSector;
   cmd2.sectorsPerCluster = sectorsPerCluster;

   int iResult = send(socket, (const char*)&cmd2, sizeof(command_2_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_2_response resp2;
   iResult = recv(socket, (char*)&resp2, sizeof(command_2_response), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   if(resp2.base.command != PSVEMMC_COMMAND_INIT || resp2.base.vita_err < 0 || resp2.base.proxy_err < 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   return 0;
}

int emmc_deinit(SOCKET socket)
{
   command_3_request cmd3;
   cmd3.command = PSVEMMC_COMMAND_DEINIT;

   int iResult = send(socket, (const char*)&cmd3, sizeof(command_3_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_3_response resp3;
   iResult = recv(socket, (char*)&resp3, sizeof(command_3_response), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   if(resp3.base.command != PSVEMMC_COMMAND_DEINIT || resp3.base.vita_err < 0 || resp3.base.proxy_err < 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   return 0;
}

int emmc_read_sector(SOCKET socket, int sector, std::array<char, SD_DEFAULT_SECTOR_SIZE>& result)
{
   command_4_request cmd4;
   cmd4.command = PSVEMMC_COMMAND_READ_SECTOR;
   cmd4.sector = sector;

   int iResult = send(socket, (const char*)&cmd4, sizeof(command_4_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_4_response resp4;

   int bytesToReceive = sizeof(command_4_response);
   int bytesWereReceived = 0;
   command_4_response* respcpy = &resp4;

   while(bytesToReceive != bytesWereReceived)
   {
      int iResult = recv(socket, ((char*)respcpy) + bytesWereReceived, bytesToReceive - bytesWereReceived, 0);
      if (iResult == SOCKET_ERROR) 
      {
         std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
         closesocket(socket);
         WSACleanup();
         return -1;
      }

      bytesWereReceived = bytesWereReceived + iResult;
   }

   if(resp4.base.command != PSVEMMC_COMMAND_READ_SECTOR || resp4.base.vita_err < 0 || resp4.base.proxy_err != 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   memcpy(result.data(), resp4.data, SD_DEFAULT_SECTOR_SIZE);

   return 0;
}

int emmc_read_cluster(SOCKET socket, int cluster, int expectedSize, char* data)
{
   command_5_request cmd5;
   cmd5.command = PSVEMMC_COMMAND_READ_CLUSTER;
   cmd5.cluster = cluster;

   int iResult = send(socket, (const char*)&cmd5, sizeof(command_5_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_5_response resp5;

   iResult = recv(socket, (char*)&resp5, sizeof(command_5_response), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   int bytesToReceive = expectedSize;
   int bytesWereReceived = 0;
   char* respcpy = data;

   while(bytesToReceive != bytesWereReceived)
   {
      int iResult = recv(socket, ((char*)respcpy) + bytesWereReceived, bytesToReceive - bytesWereReceived, 0);
      if (iResult == SOCKET_ERROR) 
      {
         std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
         closesocket(socket);
         WSACleanup();
         return -1;
      }

      bytesWereReceived = bytesWereReceived + iResult;
   }

   if(resp5.base.command != PSVEMMC_COMMAND_READ_CLUSTER || resp5.base.vita_err < 0 || resp5.base.proxy_err != 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   return 0;
}

int emmc_read_sector_ms(SOCKET socket, int sector, std::array<char, SD_DEFAULT_SECTOR_SIZE>& result)
{
   command_8_request cmd8;
   cmd8.command = PSVEMMC_COMMAND_READ_SECTOR_MS;
   cmd8.sector = sector;

   int iResult = send(socket, (const char*)&cmd8, sizeof(command_8_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_8_response resp8;

   int bytesToReceive = sizeof(command_8_response);
   int bytesWereReceived = 0;
   command_8_response* respcpy = &resp8;

   while(bytesToReceive != bytesWereReceived)
   {
      int iResult = recv(socket, ((char*)respcpy) + bytesWereReceived, bytesToReceive - bytesWereReceived, 0);
      if (iResult == SOCKET_ERROR) 
      {
         std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
         closesocket(socket);
         WSACleanup();
         return -1;
      }

      bytesWereReceived = bytesWereReceived + iResult;
   }

   if(resp8.base.command != PSVEMMC_COMMAND_READ_SECTOR_MS || resp8.base.vita_err < 0 || resp8.base.proxy_err != 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   memcpy(result.data(), resp8.data, SD_DEFAULT_SECTOR_SIZE);

   return 0;
}

int emmc_read_cluster_ms(SOCKET socket, int cluster, int expectedSize, char* data)
{
   command_9_request cmd9;
   cmd9.command = PSVEMMC_COMMAND_READ_CLUSTER_MS;
   cmd9.cluster = cluster;

   int iResult = send(socket, (const char*)&cmd9, sizeof(command_9_request), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   command_9_response resp9;

   iResult = recv(socket, (char*)&resp9, sizeof(command_9_response), 0);
   if (iResult == SOCKET_ERROR) 
   {
      std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   int bytesToReceive = expectedSize;
   int bytesWereReceived = 0;
   char* respcpy = data;

   while(bytesToReceive != bytesWereReceived)
   {
      int iResult = recv(socket, ((char*)respcpy) + bytesWereReceived, bytesToReceive - bytesWereReceived, 0);
      if (iResult == SOCKET_ERROR) 
      {
         std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
         closesocket(socket);
         WSACleanup();
         return -1;
      }

      bytesWereReceived = bytesWereReceived + iResult;
   }

   if(resp9.base.command != PSVEMMC_COMMAND_READ_CLUSTER_MS || resp9.base.vita_err < 0 || resp9.base.proxy_err != 0)
   {
      closesocket(socket);
      WSACleanup();
      return -1;
   }

   return 0;
}

int read_sector(SOCKET socket, int dumpMode, int sector, std::array<char, SD_DEFAULT_SECTOR_SIZE>& result)
{
   switch(dumpMode)
   {
   case 0:
      return emmc_read_sector(socket, sector, result);
   case 1:
      return emmc_read_sector_ms(socket, sector, result);
   default:
      return -1;
   }
}

int read_cluster(SOCKET socket, int dumpMode, int cluster, int expectedSize, char* data)
{
   switch(dumpMode)
   {
   case 0:
      return emmc_read_cluster(socket, cluster, expectedSize, data) < 0;
   case 1:
      return emmc_read_cluster_ms(socket, cluster, expectedSize, data) < 0;
   default:
      return -1;
   }
}

int dump_partition(SOCKET emmc_socket, int dumpMode, const PartitionEntry& partition, boost::filesystem::path dumpFilePath)
{
   //TODO: valid approach is to take sectorsPerCluster value from partition VBR (both fat16 and exfat have this info)

   const int sectorsPerCluster = 8;

   if(emmc_init(emmc_socket, SD_DEFAULT_SECTOR_SIZE, sectorsPerCluster) < 0)
      return -1;

   int nClustersToRead = partition.partitionSize / sectorsPerCluster;
   int nSectorsToRead = partition.partitionSize % sectorsPerCluster;

   int clusterOffset = partition.partitionOffset / sectorsPerCluster;
   int tailOffset =  partition.partitionOffset + nClustersToRead * sectorsPerCluster;

   std::ofstream outputFile(dumpFilePath.generic_string().c_str(), std::ios::out | std::ios::trunc | std::ios::binary);

   std::vector<char> clusterData(SD_DEFAULT_SECTOR_SIZE * sectorsPerCluster);
   for(size_t i = 0; i < nClustersToRead; i++)
   {
      if(read_cluster(emmc_socket, dumpMode, clusterOffset + i, clusterData.size(), clusterData.data()) < 0)
         return -1;

      outputFile.write(clusterData.data(), clusterData.size());

      std::cout << "cluster " << (i + 1) << " out of " << nClustersToRead << std::endl;
   }

   std::array<char, SD_DEFAULT_SECTOR_SIZE> sectorData;
   for(size_t i = 0; i < nSectorsToRead; i++)
   {
      if(read_sector(emmc_socket, dumpMode, tailOffset + i, sectorData) < 0)
         return -1;

      outputFile.write(sectorData.data(), sectorData.size());
   }

   outputFile.close();

   if(emmc_deinit(emmc_socket) < 0)
      return -1;
}

int parse_sce_mbr(SOCKET emmc_socket, int dumpMode, MBR& mbr)
{
   std::array<char, SD_DEFAULT_SECTOR_SIZE> mbrSector;

   if(read_sector(emmc_socket, dumpMode, 0, mbrSector) < 0)
      return -1;

   memcpy(&mbr, mbrSector.data(), mbrSector.size());
   if(validateSceMbr(mbr) < 0)
      return -1;

   std::cout << "Available partitions: " << std::endl;

   for(size_t i = 0; i < NPartitions; i++)
   {
      const PartitionEntry& partition = mbr.partitions[i];

      if(partition.partitionType == empty_t)
         break;

      std::cout << std::hex << std::setfill('0') << std::setw(8) << partition.partitionOffset << " " 
                << std::hex << std::setfill('0') << std::setw(8) << partition.partitionSize << " "
                << PartitionTypeToString(partition.partitionType) << " " 
                << partitionCodeToString(partition.partitionCode) << std::endl;
   }
}

int dump_device(SOCKET emmc_socket, int dumpMode, int dumpPartitionIndex, boost::filesystem::path dumpFilePath)
{
   if(emmc_ping(emmc_socket) < 0)
      return -1;

   MBR mbr;
   parse_sce_mbr(emmc_socket, dumpMode,  mbr);

   if(mbr.partitions[dumpPartitionIndex].partitionType == empty_t)
   {
      std::cout << "trying to dump empty partition" << std::endl;
      return -1;
   }

   dump_partition(emmc_socket, dumpMode, mbr.partitions[dumpPartitionIndex], dumpFilePath);

   return 0;
}

int parseArgs(int argc, char* argv[], int& dumpMode, int& dumpPartitionIndex, boost::filesystem::path& dumpFilePath)
{
   //TODO: args should be parsed with boost
   if(argc < 4)
   {
      std::cout << "Invalid number of arguments" << std::endl;
      std::cout << "Usage: dumpMode dumpPartitionIndex dumpFilePath" << std::endl;
      return - 1;
   }

   dumpMode = dumpPartitionIndex = boost::lexical_cast<int, std::string>(std::string(argv[1]));
   dumpPartitionIndex = boost::lexical_cast<int, std::string>(std::string(argv[2]));
   dumpFilePath = boost::filesystem::path (argv[3]);

   if(dumpMode >= 2)
   {
      std::cout << "dump mode index is invalid" << std::endl;
      return -1;
   }

   if(dumpPartitionIndex >= NPartitions)
   {
      std::cout << "partition index is invalid" << std::endl;
      return -1;
   }

   return 0;
}

int main(int argc, char* argv[])
{
   int dumpMode;
   int dumpPartitionIndex;
   boost::filesystem::path dumpFilePath;

   if(parseArgs(argc, argv, dumpMode, dumpPartitionIndex, dumpFilePath) < 0)
      return 1;

   SOCKET emmc_socket = 0;
   if(initialize_emmc_proxy_connection(emmc_socket) < 0)
      return 1;

   dump_device(emmc_socket, dumpMode, dumpPartitionIndex, dumpFilePath);

   if(deinitialize_emmc_proxy_connection(emmc_socket) < 0)
      return 1;

	return 0;
}
