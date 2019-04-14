#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "link_emulator/lib.h"
#include "payload.h"

#define HOST "127.0.0.1"
#define PORT 10000

// I defined min function here
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

// Gets the input file size
int GetFileSize(char *fileName)
{
  // Opens the input file
  int fileDescriptor = open(fileName, O_RDONLY);
  // Moves the cursor and stores number of bytes which is the size
  int fileSize = lseek(fileDescriptor, 0L, SEEK_END);

  // Closes the input file
  close(fileDescriptor);
  return fileSize;
}

// Gets a new Payload
Payload GetPayload(int type, int index)
{
  return (Payload){type, index, 0, ""};
}

// Loads the payload into the buffer from your struct
void LoadPayload(msg *t, Payload p)
{
  memset(t->payload, 0, sizeof(t->payload));
  memcpy(t->payload, &p, sizeof(p));
}

// Hashes the bytes as a simple sum
int GetSumOfBytes(char *message, int length)
{
  int sumOfBytes = 0, i;

  // Iterates through bytes and computes the sum
  for (i = 0; i < length; i++)
  {
    sumOfBytes += (unsigned char)message[i];
  }

  return sumOfBytes;
}

// Gets the index of the first unreceived package
int GetFirstUnreceivedIndex(int *received, int numberOfPackages)
{
  int i;

  // Iterates through packets
  for(i = 0; i < numberOfPackages; i++)
  {

    // Checks for the first unreceived index
    if(received[i] == 0)
    {
      // Marks it as 2 meaning "sent to fullfill the window before it gets empty" 
      received[i] = 2;
      // Returns the first unreceived index
      return i;
    } 
  }
  
  // Returns -1 if all packages were received
  return -1;
}

// Sends windowSize packages to the receiver
void FillWindow(msg * messages, int *received, int windowSize, int numberOfPackages)
{
  int currentWindowSize = 0, i;

  // Iterates through packages
  for(i = 0; i < numberOfPackages; i++)
  {
    // If fulled out the window then we stop 
    if(currentWindowSize == min(windowSize, numberOfPackages)) break;
    // Otherwise check for unreceived indexes
    else if(received[i] == 0)
    {
      currentWindowSize++;
      // Send the desired package 
      send_message(&messages[i]);
    }
  }
}

int main(int argc, char **argv)
{
  // Define and initialize needed variables such as windowSize, timeOutTime etc. 
  init(HOST, PORT);
  msg r, t;
  int i;

  int BDP = atoi(argv[2]) * atoi(argv[3]) * 1024;
  int windowSize = BDP / (sizeof(msg) * 8);

  char *fileName = argv[1];
  int fileSize = GetFileSize(fileName);
  int timeOutTime = atoi(argv[3]) * DELAY_RATE + 1;

  Payload currentPayload;
  // Compute the number of packages based on fileSize and the maximum capacity for a package
  int numberOfPackages = fileSize / REAL_MESSAGE_SIZE + 1;
  int received[numberOfPackages];
  msg messages[numberOfPackages];

  // Initialize the received array with 0 because there is no received package at this time
  for (i = 0; i < numberOfPackages; i++)
  {
    received[i] = 0;
  }

  // Opens the input file for reading purpouses 
  int fileDescriptor = open(fileName, O_RDONLY);

  // Iterates through packages and converts them into messages stores into messages array
  for (i = 0; i < numberOfPackages; i++)
  {
    // Creates a payload with type, index, hash initialized at 0 and the message itself
    currentPayload = GetPayload(3, i);
    // Sets the message length to the number of bytes read
    messages[i].len = read(fileDescriptor, currentPayload.message, REAL_MESSAGE_SIZE);

    // Computes hash
    LoadPayload(&messages[i], currentPayload);
    currentPayload.hash = GetSumOfBytes(messages[i].payload, sizeof(Payload));
    LoadPayload(&messages[i], currentPayload);
  }

  close(fileDescriptor);

  // Sends fileName header until it is correctly received
  do
  {
    // Creates a payload with type, index, hash initialized at 0 and the message itself
    currentPayload = GetPayload(1, -1);
    strcpy(currentPayload.message, fileName);

    // Computes hash
    LoadPayload(&t, currentPayload);
    currentPayload.hash = GetSumOfBytes(t.payload, sizeof(Payload));
    LoadPayload(&t, currentPayload);

    send_message(&t);
  } while (recv_message_timeout(&r, timeOutTime) < 0);

  // Sends numberOfPackages header until it is correctly received
  do
  {
    // Creates a payload with type, index, hash initialized at 0 and the message itself
    currentPayload = GetPayload(2, -1);
    char numberOfPackagesAsString[REAL_MESSAGE_SIZE];
    sprintf(numberOfPackagesAsString, "%d", numberOfPackages);
    strcpy(currentPayload.message, numberOfPackagesAsString);

    // Computes hash
    LoadPayload(&t, currentPayload);
    currentPayload.hash = GetSumOfBytes(t.payload, sizeof(Payload));
    LoadPayload(&t, currentPayload);

    send_message(&t);
  } while (recv_message_timeout(&r, timeOutTime) < 0);

  // Sends the first windowSize messages
  FillWindow(messages, received, windowSize, numberOfPackages);

  // Infinite loop that will break when everything is "Done"
  while (1)
  {
    // printf("%s\n", r.payload);

    // If there was a timeout
    if (recv_message_timeout(&r, timeOutTime) < 0)
    {
      // Reset all the packages from 2 to 0
      for (i = 0; i < numberOfPackages; i++)
      {
        if (received[i] == 2) received[i] = 0;
      }

      // Sends the upcoming windowSize messages
      FillWindow(messages, received, windowSize, numberOfPackages);
    }
    else
    {
      // If receiver sent "Done" then the file was sent, written and closed at the back-end.
      if(strcmp(r.payload, "Done") == 0) break;
      else
      {
        // "-200" means that the receiver got an uncorrupted package markable as 1
        if(strcmp(r.payload, "-200") != 0) received[atoi(r.payload)] = 1;
        // Get the first unreceived index, send the package and mark the package as 2
        int f = GetFirstUnreceivedIndex(received, numberOfPackages);
        if(f != -1) send_message(&messages[f]);
      }
    }
  }

  return 0;
}