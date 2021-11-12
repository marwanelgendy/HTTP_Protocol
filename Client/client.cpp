#include <iostream>
#include <ws2tcpip.h>
#include <fstream>
#include <vector>
#include<string.h>
#include<string>
#include <time.h>
#include <unistd.h>

using namespace std;


string ipAddress = "127.0.0.1";  // ip address of the server
SOCKET sock ;   // client socket

// struct to store parts of request
struct request{
    string method;
    string hostname;
    int port;
    string file_path;
    string body;
};



request commandParser(string command){
    request req;
    vector<string> tokens ;

    //split command into parts 
    const char *str = command.c_str();
    char *token = strtok( (char *)str ," ");

    while (token != NULL)
    {
        tokens.push_back((string) token);

        token = strtok(NULL," ");
    }

    // construct a request
    req.method = tokens[0];
    req.file_path = tokens[1];
    req.hostname = tokens[2];

    if(tokens[0] == "GET") //GET
    {
        if(tokens.size() == 4)
            req.port = atoi(tokens[3].c_str());
        else
            req.port = 80;
    }
    else // POST
    {
        if(tokens.size() == 4)
            req.port = atoi(tokens[3].c_str());
        else
            req.port = 80;

        // read data from file
        ifstream in_file;
        in_file.open(req.file_path.substr(1,req.file_path.size()), ios::binary | std::ios::in);
        in_file.seekg(0, std::ios::end);
        int file_size = in_file.tellg();
        
        vector<char> d;
        d.resize(file_size);
       
        char *file_data = new char[file_size];
        in_file.seekg(0, ios::beg);
        in_file.read(&d[0],file_size);
        
        req.body = string(d.begin(),d.end());
    }

    return req;
}

vector<request> parseFile(string f_name){
    fstream file(f_name);
    string command;
    vector<request> commands;

    while (getline(file , command))  // read client commands
    {
        commands.push_back(commandParser(command)) ; // parse each command
    }

    file.close();
    return commands ;
}

bool connectToServer(int port){
    sock = socket(AF_INET , SOCK_STREAM , 0);   // create client socket
    if(sock == INVALID_SOCKET)
    {
        cerr << "Can't create socket , Error #" << WSAGetLastError() << endl;
        WSACleanup();
        return false ;
    }


    // Fill in hint structure
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    inet_pton(AF_INET , ipAddress.c_str() , &hint.sin_addr);

    cout<< port << endl;
    // connect to server
    int connResult = connect(sock , (sockaddr*)&hint , sizeof(hint));
    if(connResult == SOCKET_ERROR)
    {
        cerr << "Can't connext to server , Err #" << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return false;
    }

    return true;
}

string GetReq(string request){
    send(sock,request.c_str(),request.size(),0);
        
    char buf[2048];
    ssize_t received = 0;
    ZeroMemory(buf , 2048);
        
    received = recv(sock,buf,sizeof buf,0);
    string response = buf;
   
    ZeroMemory(buf , 2048);
    received = recv(sock,buf,sizeof buf,0);
    response += buf;

    /*if(received = 1024){
        ZeroMemory(buf , 2048);
        printf("recv\n");
        //usleep(50000);
        while(recv(sock,buf,sizeof buf,0) == 1024){
            response = response.substr(0,response.size()-1);
            response += buf;
            //usleep(50000);
            ZeroMemory(buf , 2048);
        }
        response += buf;
    }*/
    return response;
}

string PostReq(string request){
    
    send(sock , request.c_str() , request.size() ,0);

    char buf[2048];
    ZeroMemory(buf , 2048);
    int bytesReceived = recv(sock , buf,sizeof(buf) , 0);

    if(bytesReceived > 0)
        return buf;

    return "Error" ;
}



void executeCommand(request req , int indx){
    if(!connectToServer(req.port)){
        return;
    }
    string request = "";  // construct request message
    if(req.method == "GET")
        request += "GET " ;
    else if(req.method == "POST")
        request += "POST " ;

    request += req.file_path;

    request += " HTTP/1.1\r\n" ;

    if(req.method == "POST"){
        request += "content-length: " + to_string(req.body.size()) + "\r\n\r\n";
        request += req.body;
    }
    else{
        request += "\r\n\r\n";
    }
    string response ;
    if(req.method == "GET"){   
         response = GetReq(request);  // send request and receive response
    }
    else{
        response = PostReq(request); // send request and receive response
    }
    // write response to file
    string f_name = "response"+to_string(indx)+".txt" ;
    ofstream file(f_name,ios::binary);

    file.write(response.c_str() , response.size()) ;
    
}

void sendRequest(string f_name)
{
    vector<request> commands = parseFile(f_name);  // read all commands and parse them
    
    int indx = 0;
    for(request req : commands){
        executeCommand(req , indx);    // execute command
        indx++ ;
    }

    closesocket(sock);
    WSACleanup();
}


int main()
{
    // initialize winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2,2);

    int wsOK = WSAStartup(ver , &wsData);
    if(wsOK != 0){
        cerr << "can't initialize winsock" << endl;
       
        return 0;
    }

   sendRequest("mytxt.txt");

  
    return 0;
}