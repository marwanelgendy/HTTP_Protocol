#include <iostream>
#include <ws2tcpip.h>
#include <fstream>
#include <vector>
#include<string.h>
#include<string>
#include <time.h>
#include <unistd.h>
#include <map>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/types.h>
//#include <boost/algorithm/string.hpp>

using namespace std;

struct client_info{
    SOCKET socket_fd;      //socket to respond with to the client.
    clock_t time;       //time of the client's last request.
};

//struct to use it with interputer thread
struct wrapper{
    void *p;
    int max;
};


SOCKET socket_fd;   // server socket
SOCKET new_socket_fd;   // use to accept connection
int queueSize;          // max num of connections
vector<client_info> connected_clients;  // connected clients
int client_num;  // number of connected clients





bool bindOnSocket(char* portNumber) {

    int port = atoi(portNumber) ;  //specified port number

    // Fill in hint structure
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    
    // create socket for server
    socket_fd = socket(AF_INET , SOCK_STREAM ,0);

    if(socket_fd == INVALID_SOCKET){          // check create socket succsuccessfully
        perror("server: socket");
    }

    int status;
    if(status = bind(socket_fd , (sockaddr*)&hint , sizeof(hint) ) == -1){   // bind IP Address and Port Number to server socket
        perror("bind");
        cout<< "can't bind "<<gai_strerror(status)<<"\n";
        return false;
    }
   
    cout<<"binding successfull" <<endl;
    return true;
}

void start_listening(int quSize) {
    queueSize = quSize;  // max number of client that can connect
    if(listen(socket_fd,quSize) == -1){
        perror("listen");
    }
    cout<< "start listening successful\n";
}


SOCKET acceptConnection() {

    sockaddr_in client_addr;
    int size_addr = sizeof(client_addr);

    new_socket_fd = accept(socket_fd, (sockaddr*) &client_addr,&size_addr); // accept connection with the client

    if(new_socket_fd == INVALID_SOCKET){
        perror("accept");
        return -1;
    }

    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(client_addr.sin_addr),str,INET_ADDRSTRLEN);  // get info about client

    cout << "connection from " << str<< " port "
         << ntohs(client_addr.sin_port) << endl;

    return new_socket_fd;
}

//intrupt thread that monitering the clients and close connection with
//idle clients.
void* interupt(void* arg){
    wrapper *w = (wrapper*)arg;
    vector<client_info>* vec = (vector<client_info>*)w->p;
    int max = w->max;
    int defaultTimeOut = 10;
    while(1){
        for(int i = 0;i < vec->size();i++){
            if((clock() - vec->at(i).time)/CLOCKS_PER_SEC > (defaultTimeOut - (vec->size()/max)*3)){
                close(vec->at(i).socket_fd);
                cout<<"timeout for : "<< vec->at(i).socket_fd<<endl;
                vec->erase(vec->begin() + i);
            }
        }
    }
}


bool file_exist(string file_name) {
    if(file_name == ""){
        return false;
    }

    /*if(file_name == "/"){
        ifstream f("index.html");
        return f.good();
    }*/

    ifstream f(file_name.substr(1,file_name.size()).c_str());
    return f.good();
}

vector<string> requestParser(string req,int *body_length,int* body_id) {
    vector<string> req_vec;

    //split request to header and data
    const char *str = req.c_str();
    char *token = strtok( (char *)str ,"\r\n");

    while (token != NULL)
    {
        req_vec.push_back((string) token);

        token = strtok(NULL,"\r\n");
    }

    vector<string> result;

    for(string str : req_vec){
        if(str != "") {
            result.push_back(str);
        }
    }

    // split request header to get method and file name
    vector<string> first_line;
    
    const char *str1 = result[0].c_str();
    char *token1 = strtok( (char *)str1 ," ");

    while (token1 != NULL)
    {
        first_line.push_back((string) token1);

        token1 = strtok(NULL," ");
    }

    if(first_line[0] == "POST"){  // if method == POST get data and length of data

        vector<string> second_line;
       
        const char *str2 = result[1].c_str();
        char *token2 = strtok( (char *)str2 ," ");

        while (token2 != NULL)
        {
            second_line.push_back((string) token2);

            token2 = strtok(NULL," ");
        }

        *body_length = atoi(second_line[second_line.size()-1].c_str());

        string body = "";

        for(int i = 2;i<result.size();i++){
            body += result[i];
        }

        *body_id = result[0].size() + result[1].size() + 6 ;

        first_line.push_back(body);
    }

    return first_line;
}

vector<char> readfile(string file_name) {

    string file;

    // remove the slash
    file = file_name.substr(1,file_name.size());
   

    //get file length
    ifstream in_file;
    in_file.open(file, ios::binary | std::ios::in);
    in_file.seekg(0, std::ios::end);
    int file_size = in_file.tellg();

    vector<char> d;
    d.resize(file_size);
   
    in_file.seekg(0, ios::beg);
    in_file.read(&d[0],file_size);

    return d;
}

void handleGET(string file, SOCKET sock) {

    if(!file_exist(file))
    {
        send(sock,"HTTP/1.1 404 Not Found\r\n\r\n",26,0);
    }
    else
    {
        string data_header = "HTTP/1.1 200 OK\r\n";
        send(sock,data_header.c_str(),data_header.size(),0);

        vector<char> d = readfile(file);
        send(sock,&d[0],d.size(),0);
    }
}

void handlePOST(string file_name, SOCKET sock, char *buffer,int recived) {
   
    string file = file_name.substr(1,file_name.size());
    FILE * fp = fopen(file.c_str(),"w");

    fwrite(buffer, sizeof(char),recived,fp);
    //printf("hey post buffer");
    send(sock,"HTTP/1.1 200 OK\r\n",17,0);
    fflush(fp);
    fclose(fp);
}


void *executeRequest(void *arg) {

    //extract args
    client_info *client = (client_info*) arg;

    SOCKET sock = client->socket_fd;
    
  
    //loop of requests from client
    while(1){

        char buffer[2048];
        ZeroMemory(buffer , 2048) ;

        int bytesRecevied;

        client->time = clock();

        bytesRecevied = recv(sock,buffer,sizeof buffer,0);

        if(bytesRecevied == 0 || bytesRecevied < 3){
            //recieve wrong format request.
            close(sock);
            break;
        }

        string req = string(buffer, buffer+bytesRecevied);

        cout<<"log : socket "<<sock<<" \n"<<buffer<<endl;

        vector<string> vec;
        int content_length = 0;
        int body_id = 0;

        try {
            vec = requestParser(req, &content_length, &body_id);
        }catch (int e){
            cout<<"log : socket "<<sock<<" "<<"error parsing"<<endl;
            break;
        }

        try{
            if(vec[0] == "POST"){

                string body = vec[vec.size()-1];

                char request_body[content_length];

                ZeroMemory(request_body , content_length);

                for(int i = body_id;i < bytesRecevied;i++){
                    request_body[i-body_id] = buffer[i];
                }

                if(content_length > bytesRecevied){
                    int length = bytesRecevied-body_id;
                    recv(sock,request_body+length,content_length-length,0);
                }

                handlePOST(vec[1],sock,request_body,content_length);

            }else if(vec[0] == "GET"){

                handleGET(vec[1] , sock);
            }
        }catch (int e){
            break;
        }

    }

}


void startServer() {
    pthread_t connections[queueSize];  // create threads equal to max number of conncetions
    client_num = 0;

    //temp struct to use it with interputer thread
    wrapper arg;
    arg.p = &connected_clients;
    arg.max = queueSize;

    //start the interupter thread.
    pthread_t interputer;
    pthread_create(&interputer,NULL,interupt,&arg);

    while(1){
        client_info new_client;
        //block until new connection accepted.
        new_client.socket_fd = acceptConnection();   // accept client's connection
        if(new_client.socket_fd == INVALID_SOCKET){
            continue;
        }
        printf("new connection on socket: %d\n",new_client.socket_fd);

        new_client.time = clock();
       
        connected_clients.push_back(new_client);  //add to connected client

        int creation = pthread_create(&connections[client_num++],NULL,executeRequest,&new_client);  // creata thread to serv client's connection

        if(creation != 0)
            printf("creation of new thread faild");

        if(client_num >= queueSize){
            for(int i = 0;i < queueSize;i++){
                pthread_join(connections[i],NULL);
            }
            client_num = 0;
        }
    }
}


int main(int argc , char *argv[])
{
    int port = 54000 ;

    // initialize winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2,2);

    int wsOK = WSAStartup(ver , &wsData);
    if(wsOK != 0){
        cerr << "can't initialize winsock" << endl;
        return 0;
    }

    
   if(argc < 2)
   {
       cout << "not specified port number" << endl;
       return 0 ;
   }

   int connection_num = 5;

   bindOnSocket(argv[1]);

   start_listening(connection_num);

   startServer();


    return 0;
}