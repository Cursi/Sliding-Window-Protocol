#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "link_emulator/lib.h"
#include "payload.h"

#define HOST "127.0.0.1"
#define PORT 10001

// Loads the payload into the buffer from your struct
void LoadPayload(msg *t, Payload p)
{
  memset(t->payload, 0, sizeof(t->payload));
  memcpy(t->payload, &p, sizeof(p));
}

// Unloads the payload from the buffer from your struct to a local payload
void UnloadPayload(msg r, Payload *p)
{
  memset(p, 0, sizeof(Payload));
  memcpy(p, r.payload, sizeof(*p));
}

// Gets the index of the first unreceived package
int GetFirstUnreceivedIndex(int *received, int numberOfPackages)
{
  int i;

  // Iterates through packets
  for(i = 0; i < numberOfPackages; i++)
  {
    // Checks for the first unreceived index and returns it
    if(received[i] == 0) return i;
  }
  
  // Returns -1 if all packages were received
  return -1;
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

int main(int argc, char **argv)
{
  // Define and initialize needed variables such as windowSize, timeOutTime etc. 
  msg r, t;
  init(HOST, PORT);

  Payload currentPayload;
  int fileDescriptor, numberOfPackages;
  char fileName[REAL_MESSAGE_SIZE];

  int *received;
  Payload *receivedPayloads;

  // Infinite loop that will break when everything is "Done"
  while(1)
  {
    // Receive a message
    recv_message(&r);
    // Unloads the payload to a local payload
    UnloadPayload(r, &currentPayload);
    
    // Saves the givenHash
    int givenHash = currentPayload.hash;
    
    // Sets the hash to 0 because this is how it was hashed
    currentPayload.hash = 0;
    // Loads it with the 0 hash
    LoadPayload(&r, currentPayload);
    // Computes the hash
    int computedHash = GetSumOfBytes(r.payload, sizeof(Payload));
    
    // Compares hashes and if they differ
    if(givenHash != computedHash)
    {
      // Send a "-200" meaning that the message was corrupted
      sprintf(t.payload, "%d", -200);
      t.len = strlen(t.payload);
      send_message(&t);
    }
    else
    {
      // Otherwise it's correct so we check it's type
      switch(currentPayload.type)
      {
        // File name message type
        case 1:
        {
          // Stores the fileName
          strcpy(fileName, "recv_");
          strcat(fileName, currentPayload.message);
          // Opens the output file
          fileDescriptor = open(fileName, O_CREAT | O_WRONLY | O_TRUNC, 0660);
          break;
        }
        // Number of packages message type
        case 2:
        {
          // Stores the numberOfPackages
          numberOfPackages = atoi(currentPayload.message);
          // Initializes 2 arrays, received with the same purpouse as in Sender
          // And receivedPayloads which contains the messages that will be written
          received = (int*)calloc(numberOfPackages, sizeof(int));
          receivedPayloads = (Payload*)calloc(numberOfPackages, sizeof(Payload));
          break;
        }
        // Effective data message type
        case 3:
        {
          // Stores the length of the message marking it as received this way too
          received[currentPayload.index] = r.len;
          // Stores the received message on it's correct position into the array
          receivedPayloads[currentPayload.index] = currentPayload;
          break;
        }
        default: break;
      }

      // If all messages are received correctly
      if(received != NULL && GetFirstUnreceivedIndex(received, numberOfPackages) == -1)
      {
        // Iterates through packages
        for(int i = 0; i < numberOfPackages; i++)
        {
          // Writes packages to the output file
          write(fileDescriptor, receivedPayloads[i].message, received[i]);          
        }

        // Closes the output file
        close(fileDescriptor);
        // Sends the "Done" singal to the Sender
        strcpy(t.payload, "Done");
        t.len = strlen(t.payload);
        send_message(&t);
        break;
      }

      // Sends the index of the received package back to Sender to be marked
      sprintf(t.payload, "%d", currentPayload.index);
      t.len = strlen(t.payload);
      send_message(&t);
    }
  }

  return 0;
}