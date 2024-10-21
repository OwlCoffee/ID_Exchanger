#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <fstream>
#include <queue>
#include <mutex>
#include <sstream>
#include <vector>

using namespace std;

void *Connection(void *client_socket);
string Dequeue();
void Enqueue(string str);
int DelimiterCounter(string str, char delimiter);
vector<string> Split(string str, char delimiter);
string StringCutter(string str, int count, char delimiter);

void error_handling(const char *message)
{
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}

queue<string> q; // Received data queue

pthread_mutex_t m; // Mutex
pthread_mutex_t m_print; // Mutex for print text
pthread_mutex_t m_conn; // Mutex for connection

#define MESSAGE_COUNT 5
#define MAX_THREAD 1000
pthread_t connection_thread[MAX_THREAD]; // Thread

ofstream fout;

int main(int argc, char **argv)
{
   pid_t pid;

   /* 새로운 프로세스 생성 */
   pid = fork();
   if(pid == -1)
       return -1;
    else if (pid != 0)
       exit(EXIT_SUCCESS);

   /* 새로운 세션과 프로세스 그룹을 생성, 세션 리더로 만듬*/
   if(setsid() == -1)
       return -1;

   /* 작업 디렉토리를 루트 디렉토리로 변경*/
   if(chdir("/") == -1)
      return -1;

   /* 모든 파일 디스크립터(fd)를 닫음*/
   for(int i=0; i<NR_OPEN; i++)
      close(i);

   /* 표준 입력, 출력, 에러 파일을 /dev/null 로 리다이렉션*/
   open("/dev/null", O_RDWR);
   dup(0);
   dup(0);

   int serv_sock;
   int clnt_sock;
   struct sockaddr_in serv_addr;
   struct sockaddr_in clnt_addr;
   socklen_t clnt_addr_size;

   if(argc!=2)
   {
       printf("Usage : %s <port>\n", argv[0]);
       exit(1);
   }

   try
   {
   serv_sock = socket(PF_INET, SOCK_STREAM, 0); // Server Socket

   if(serv_sock == -1)
        error_handling("socket() error");

   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(atoi(argv[1]));

   timeval tv;
   tv.tv_sec=10;
   tv.tv_usec=0;

   setsockopt(serv_sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&tv,sizeof(timeval)); // Timeout
   
   if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) // Bind
        error_handling("bind() error");

   if(listen(serv_sock, SOMAXCONN) == -1) // Listen
        error_handling("listen() error");
   
   
   fout.open("log_server.txt"); // Open File Stream

   for(int thread_count=0;thread_count<MAX_THREAD;thread_count++)
   {
      pthread_mutex_lock(&m_conn); // Mutex Lock

      clnt_addr_size = sizeof(clnt_addr);
      clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &clnt_addr_size); // Accept

      if(clnt_sock == -1)
           error_handling("accept() error");

      pthread_create(&connection_thread[thread_count],NULL,Connection,(void*)&clnt_sock);

      pthread_mutex_lock(&m_print); // Mutex Lock
      cout<<"Accepted Count: "<<thread_count<<"\n";
      pthread_mutex_unlock(&m_print); // Mutex Unlock
   }

   for (int thread_count = 0; thread_count < MAX_THREAD; thread_count++)
   {
      pthread_join(connection_thread[thread_count],NULL);
      pthread_mutex_lock(&m_print); // Mutex Lock
      cout<<"종료된 쓰레드: "<<thread_count<<"\n";
      pthread_mutex_unlock(&m_print); // Mutex Unlock
   }
   
   fout.close(); // Close File Stream
   cout<<"Close Log\n";

   close(clnt_sock);
   }
   catch(exception& ex)
   {
      cout<<ex.what()<<"\n";
      fout<<ex.what()<<"\n";
      fout.close();
      cout<<"Close Log\n";
      close(clnt_sock);
      close(serv_sock);
   }

   return 0;
}

void *Connection(void *client_socket)
{
   int cli_sock=*(int *)client_socket;

   pthread_mutex_unlock(&m_conn); // Mutex Unlock

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

      read(cli_sock,buffer,sizeof(buffer)-1);
      if(buffer==NULL)break;
      mergedBuf+=buffer;

      delimiter_cnt=DelimiterCounter(mergedBuf,'#');

      //mergedBuf=StringCutter(mergedBuf,delimiter_cnt,'#');
   }

   for(int id_count=0;id_count<MESSAGE_COUNT;id_count++)
   {
      string s="";

      while(s=="")
      {
         s=Dequeue();
         pthread_mutex_unlock(&m); // Mutex Unlock
      }

      char const* message=(s+"#").c_str();
      write(cli_sock, message, sizeof(message));
      //fout<<message<<"\n"; // Logging
   }
   write(cli_sock, NULL, 0);

   return nullptr;
}

string Dequeue()
{
   pthread_mutex_lock(&m); // Mutex Lock

   if(!q.empty()) // Exists data
   {
      string value=q.front(); // Return front value
      q.pop(); // Dequeue

      return value;
   }
   else // Empty
   {
      return "";
   }
}

void Enqueue(string str)
{
   pthread_mutex_lock(&m); // Mutex Lock
   vector<string> splitted=Split(str,'#');
   for(int i=0;i<splitted.size();i++)
   {
      q.push(splitted[i]); // Enqueue
      fout<<splitted[i]<<"\n"; // Logging
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