#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <stdlib.h>
#include <fstream>
#include <queue>
#include <mutex>
#include <sstream>
#include <vector>

using namespace std;

#define MESSAGE_COUNT 5
#define MAX_THREAD 1000
pthread_t client_thread[MAX_THREAD];
int stat_array[MAX_THREAD]={0,};

pthread_mutex_t m;
pthread_mutex_t m_print; // Mutex for print text
queue<string> q;

ofstream fout;
ofstream stat_fout;

void* send_messages(void* arg);
void Enqueue(string str);
int DelimiterCounter(string str, char delimiter);
vector<string> Split(string str, char delimiter);
string StringCutter(string str, int count, char delimiter);

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

char* ipAddress;
int port;

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    ipAddress=argv[1];
    port=atoi(argv[2]);

    try
    {
    for (int thread_count = 0; thread_count < MAX_THREAD; thread_count++)
    {
        int* id = new int(thread_count);
        if (pthread_create(&client_thread[thread_count], NULL, send_messages, (void*)id) != 0)
        {
            error_handling("pthread_create() error");
        }
    }

    for (int thread_count = 0; thread_count < MAX_THREAD; thread_count++)
    {
        pthread_join(client_thread[thread_count],NULL);
        pthread_mutex_lock(&m_print); // Mutex Lock
        cout<<"종료된 쓰레드: "<<thread_count<<"\n";
        pthread_mutex_unlock(&m_print); // Mutex Unlock
    }

    fout.open("queue_data.txt");
    stat_fout.open("stat.txt");

    pthread_mutex_lock(&m); // Mutex Lock
    while(!q.empty())
    {
        cout<<q.front()<<"\n";
        fout<<q.front()<<"\n";
        stat_array[stoi(q.front())]++;
        q.pop();
    }
    pthread_mutex_unlock(&m); // Mutex Unlock

    stat_fout<<"ID Count\n";
    for(int i=0;i<MAX_THREAD;i++) stat_fout<<i<<" "<<stat_array[i]<<"\n";

    fout.close();
    stat_fout.close();
    cout<<"Close Log\n";
    }
    catch(exception& ex)
    {
        cout<<ex.what()<<"\n";
        fout<<ex.what()<<"\n";
        fout.close();
        stat_fout.close();
        cout<<"Close Log\n";
    }

    return 0;
}

void* send_messages(void* arg)
{
    int id = *((int*)arg);

    char const* message=(to_string(id)+"#").c_str(); // Add delimeter '#'
    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ipAddress);
    serv_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) // Connect
        error_handling("connect() error");
    else
    {
        pthread_mutex_lock(&m_print); // Mutex Lock
        cout<<"Connection Count: "<<id<<"\n";
        pthread_mutex_unlock(&m_print); // Mutex Unlock
    }

    for (int i = 0; i < MESSAGE_COUNT; i++)
    {
        write(sock, message, sizeof(message));
    }
    write(sock, NULL, 0);

    string mergedBuf;

    char buffer[100];

    int delimiter_cnt=0;

    while(true)
    {
        if(delimiter_cnt>=MESSAGE_COUNT)
        {
            Enqueue(mergedBuf);
            break;
        }

        read(sock, buffer,sizeof(buffer)-1);
        if(buffer==NULL)break;
        mergedBuf+=buffer;

        delimiter_cnt=DelimiterCounter(mergedBuf,'#');

        //mergedBuf=StringCutter(mergedBuf,delimiter_cnt,'#');
    }

    close(sock);

    return nullptr;
}

void Enqueue(string str)
{
   pthread_mutex_lock(&m); // Mutex Lock
   vector<string> splitted=Split(str,'#');
   for(int i=0;i<splitted.size();i++)
   {
      q.push(splitted[i]); // Enqueue
   }
   pthread_mutex_unlock(&m); // Mutex Unlock
}

int DelimiterCounter(string str, char delimiter)
{
   int count = 0;

   for(int index=0; index<str.length(); index++)
   {
      if(str[index]==delimiter) count++;
   }

   return count;
}

string StringCutter(string str, int count, char delimiter)
{
   int cnt=0;
   string result="";

   for(int i=0;i<str.length();i++)
   {
      if(str[i]==delimiter) cnt++;

      if(cnt==count)
      {
         result=str.substr(i+1);
         break;
      }
   }

   return result;
}

vector<string> Split(string str, char delimiter)
{
   vector<string> result;

   istringstream iss(str);
   string buffer;

   while (getline(iss, buffer, delimiter))
   {
      result.push_back(buffer);
   }
 
   return result;
}