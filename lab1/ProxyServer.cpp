#include "ProxyServer.h"
#include <iostream>

#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶� 
#define HTTP_PORT 80 //http �������˿�
bool cacheFlag = false, needCache = true;


ProxyServer::ProxyServer() {
    // ��ʼ��Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // ��ʼ������������׽���
    proxyServer = socket(AF_INET, SOCK_STREAM, 0);

    proxyServerAddr.sin_family = AF_INET;  // IPv4
    proxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY; // �����ַ
    proxyServerAddr.sin_port = htons(proxyServerPort); // �˿ڣ�ת��Ϊ�����ֽ���
}

ProxyServer::~ProxyServer() {
    // �رմ���������׽���
    closesocket(proxyServer);

    // ����Winsock
    WSACleanup();
}


int ProxyServer::run() {
    std::cout << "���ڳ�ʼ���׽���..." << std::endl;
    if (!initSocket()) {
        std::cerr << "��ʼ���׽���ʧ�ܣ�" << std::endl;
        return 0;
    }
    std::cout << "��ʼ���׽��ֳɹ���" << std::endl;
    std::cout << "���ڼ����˿�" << proxyServerPort << "..." << std::endl;
    SOCKET accepetSocket = INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE hThread;
    DWORD dwThreadId;
    while (true) {
        cacheFlag = false;
        needCache = true;
        accepetSocket = accept(proxyServer, NULL, NULL); // ��������
        lpProxyParam = new ProxyParam;  // Ϊ�̲߳��������ڴ�
        if (lpProxyParam == NULL) {
            std::cerr << "�����̲߳���ʧ�ܣ�" << std::endl;
            continue;
        }
        lpProxyParam->setClientSocket(accepetSocket);
        hThread = (HANDLE)_beginthreadex(NULL, 0,
                                         &ProxyServer::proxyThread, (LPVOID)lpProxyParam, 0, 0);
                                        //  _beginthreadex()�������ڴ���һ���̣߳��ú����ĵ�һ������Ϊ�̰߳�ȫ���ԣ��ڶ�������Ϊ��ջ��С������������Ϊ�̺߳��������ĸ�����Ϊ���ݸ��̺߳����Ĳ��������������Ϊ�̴߳�����ĳ�ʼ״̬������������Ϊ�߳�ID
                                        // �õ��˿⺯��process.h
        CloseHandle(hThread);           // �ر��߳̾��
        Sleep(200);
    }
    closesocket(proxyServer);
    WSACleanup();
    return 0;
}

bool ProxyServer::initSocket() {
    // �󶨴���������׽���
    if (bind(proxyServer, (SOCKADDR *)&proxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket." << std::endl;
        return false;
    }

    // ��������������׽���
    if (listen(proxyServer, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen socket." << std::endl;
        return false;
    }

    return true;
}

void ProxyServer::getRedirect(char *buffer, const char *url) {
    // ȡ��buffer ��һ�� ��һ���ո�֮ǰ������
    char *p = strstr(buffer, " ");
    char temp[MAXSIZE];
    const char *content = "HTTP/1.1 302 Moved Temporarily\r\n";
    ZeroMemory(temp, sizeof(temp));
    // content -> temp
    memcpy(temp, content, strlen(content));
    const char *content2 = "Connection:keep-alive\r\n";
    strcat(temp, content2);
    const char *content3 = "Cache-Control:max-age=0\r\n";
    strcat(temp, content3);
    const char *content4 = "Location: ";
    strcat(temp, content4);
    strcat(temp, url);
    strcat(temp, "\r\n\r\n");
    ZeroMemory(buffer, strlen(buffer));
    memcpy(buffer, temp, strlen(temp));   
    printf("��������Ӧͷ��\n----------------\n%s\n----------------\n", buffer);
}


unsigned int __stdcall ProxyServer::proxyThread(LPVOID lpParameter) {

    char buffer[MAXSIZE], fileBuffer[MAXSIZE];
    char *redirectBuffer;
    char *cacheBuffer, *dateBuffer;
    char fileName[1024];
    char hostname[1024];
    bool redirectFlag = false;


    memset(buffer, 0, sizeof(buffer));
    memset(fileBuffer, 0, sizeof(fileBuffer));
    
    int recvSize;
    int ret;
    recvSize = recv(((ProxyParam *)lpParameter)->getClientSocket(), buffer, MAXSIZE, 0); // ��������
    HttpHeader *httpHeader = new HttpHeader();
    cacheBuffer = new char[recvSize + 1];
    memset(cacheBuffer, 0, recvSize + 1);
    memcpy(cacheBuffer, buffer, recvSize);
    parseHttpHead(cacheBuffer, httpHeader);
    if (!strlen(httpHeader->getMethod())) {
        Sleep(200);
        closesocket(((ProxyParam *)lpParameter)->getClientSocket());
        closesocket(((ProxyParam *)lpParameter)->getServerSocket());
        delete (ProxyParam *)lpParameter;
        _endthreadex(0);
        return 0;
    }
    dateBuffer = new char[recvSize + 1];
    memset(dateBuffer, 0, strlen(buffer) + 1);
    memcpy(dateBuffer, buffer, strlen(buffer) + 1);

    memset(fileName, 0, sizeof(fileName));
    
    if (!strcmp(httpHeader->getHost(), "jwes.hit.edu.cn")) {
        printf("\n=====================================\n\n");
        printf("����ɹ�������ǰ������վ�ѱ�������http://jwts.hit.edu.cn\n");
        httpHeader->setHost("jwts.hit.edu.cn", 22);
        httpHeader->setUrl("http://jwts.hit.edu.cn", 22);
        redirectFlag = true;
    }

    getFilename(httpHeader->getUrl(), fileName, sizeof(fileName)); // ��ȡ�ļ���
    printf("�ļ�����%s\n", fileName);

    int ret_new = gethostname(hostname, sizeof(hostname));         // ��ȡ������
    printf("��������%s\n----------------\n", hostname);

    HOSTENT *hostent = gethostbyname(hostname);                 // ��ȡ������Ϣ
    char *ip = inet_ntoa(*(in_addr *)*hostent->h_addr_list);    // ��ȡIP��ַ
    if (!strcmp(ip, "127.0.0.1")) {
        printf("���ط���\n��������������\n");
        goto error;
    }

    if (strstr(httpHeader->getUrl(), "gov.cn")) {
        printf("���ط���%s\n��������������\n", httpHeader->getUrl());
        goto error;
    }

    FILE *in;
    char date[50];
    memset(date, 0, sizeof(date));
    if (fopen_s(&in, fileName, "rb") == 0) {
        printf("��������\n");
        fread(fileBuffer, sizeof(char), MAXSIZE, in);
        fclose(in);
        getDate(fileBuffer, date);
        printf("����ʱ�䣺%s\n", date);
        changeHTTP(buffer, date);
        // printf("����������ͷ��\n----------------\n%s\n----------------\n", buffer);
        cacheFlag = true;
        goto success;
    }

    delete cacheBuffer;

success:
    SOCKET serverSockett;
    serverSockett = ((ProxyParam *)lpParameter)->getServerSocket();
    if (strlen(httpHeader->getHost()) == 0)
        goto error;
    if (!connectToServer(&serverSockett, httpHeader->getHost())) {
        printf("���ӷ�����ʧ�ܣ�%s \n", httpHeader->getHost());
        goto error;
    }
    ((ProxyParam *)lpParameter)->setServerSocket(serverSockett);
    if (!redirectFlag) 
        printf("���ӷ������ɹ���\n������: \n%s\n", buffer);
    ret = send(((ProxyParam *)lpParameter)->getServerSocket(), buffer, strlen(buffer) + 1, 0); // ��������
    if (ret == -1) {
        int errCode = WSAGetLastError();
        printf("��������ʧ�ܣ�������룺%d\n", errCode);
    }
    recvSize = recv(((ProxyParam *)lpParameter)->getServerSocket(), buffer, MAXSIZE, 0); // ��������
    if (recvSize <= 0) {
        printf("��������ʧ�ܣ�\n");
        goto error;
    }
    // if (redirectFlag) {
    //     redirectBuffer = new char[recvSize + 1]; 
    //     memset(redirectBuffer, 0, strlen(buffer) + 1);
    //     memcpy(redirectBuffer, buffer, strlen(buffer) + 1);
    //     getRedirect(redirectBuffer, httpHeader->getUrl());
    //     ret = send(((ProxyParam *)lpParameter)->getClientSocket(), redirectBuffer, strlen(redirectBuffer) + 1, 0); // ��������
    //     goto error;
    // }
    if (cacheFlag) {
        getCache(buffer, fileName);
    }
    if (!redirectFlag)
        printf("��Ӧ����: \n%s\n---------------\n", buffer);
    if (needCache) {
        saveCache(buffer, httpHeader->getUrl());
    }
    std::cout << "clientSocket: " << ((ProxyParam *)lpParameter)->getClientSocket() << std::endl;
    ret = send(((ProxyParam *)lpParameter)->getClientSocket(), buffer, sizeof(buffer), 0); // ��������

error:
    Sleep(200);
    closesocket(((ProxyParam *)lpParameter)->getClientSocket());
    closesocket(((ProxyParam *)lpParameter)->getServerSocket());
    delete (ProxyParam *)lpParameter;
    _endthreadex(0);
    return 0;
}

void ProxyServer::parseHttpHead(char *buffer, HttpHeader *httpHeader) {
    char *p;
    char *ptr;
    const char *split = "\r\n";
    p = strtok_s(buffer, split, &ptr); // �ָ��ַ���
    // strtok_s()�������ڷָ��ַ�������һ������ΪҪ�ָ���ַ������ڶ�������Ϊ�ָ��ַ���������������Ϊ����ָ�����ָ��
    if (p[0] == 'G') { // GET 
        httpHeader->setMethod("GET");
        httpHeader->setUrl(p + 4, strlen(p) - 13);
    }
    else if (p[0] == 'P') { // POST
        httpHeader->setMethod("POST");
        httpHeader->setUrl(p + 5, strlen(p) - 14);
    }
    else {
        // printf("�ݲ�֧�ָ÷�����%s\n", p);
        return;
    }
    p = strtok_s(NULL, split, &ptr); // NULL��ʾʹ����һ�εķָ���
    while (p) {
        if (p[0] == 'H') { // Host
            httpHeader->setHost(p + 6, strlen(p) - 6);
        }
        else if (p[0] == 'C') { // Cookie
            if (strlen(p) > 8) {
                char header[8];
                ZeroMemory(header, sizeof(header)); // zeromemory�������ڽ��ڴ������, ��memset��������ǰ����ϵͳ����, ������C����
                memcpy(header, p, 6);
                if (strcmp(header, "Cookie") == 0) {
                    httpHeader->setCookie(p + 8, strlen(p) - 8);
                }
            }
        }
        p = strtok_s(NULL, split, &ptr);
    }
}

bool ProxyServer::connectToServer(SOCKET *serverSocket, const char *host) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT); // �˿ڣ�ת��Ϊ�����ֽ���
    HOSTENT *hostent = gethostbyname(host); // ��ȡ������Ϣ
    if (!hostent) {
        printf("��ȡ������Ϣʧ�ܣ�\n�������Ϊ%d\n", WSAGetLastError());
        return false;
    }
    in_addr Inaddr = *((in_addr *)*hostent->h_addr_list); // ��ȡ������ַ
    serverAddr.sin_addr.S_un.S_addr = inet_addr(inet_ntoa(Inaddr)); // ���ʮ����ת��Ϊ�����ֽ���
    *serverSocket = socket(AF_INET, SOCK_STREAM, 0); // �����׽���
    if (INVALID_SOCKET == *serverSocket) {
        printf("�����׽���ʧ�ܣ�\n�������Ϊ%d\n", WSAGetLastError());
        return false;
    }
    if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) { // ���ӷ�����
        printf("���ӷ�����ʧ�ܣ�\n�������Ϊ%d\n", WSAGetLastError());
        return false;
    }
    return true;
}

void ProxyServer::getFilename(const char *url, char *filename, size_t maxLen) {
    size_t len = strlen(url);
    size_t copyLen = (len < maxLen) ? len : maxLen - 1; // ��������߽磬����һ��λ�ø��ַ�����β��'\0'
    size_t filenameIndex = 0;

    for (size_t i = 0; i < copyLen; i++) {
        if (url[i] != '/' && url[i] != ':' && url[i] != '.') { // ���˵��Ƿ��ַ�
            filename[filenameIndex++] = url[i];
        }
    }

    filename[filenameIndex] = '\0'; // ����ַ�����β��־
}

bool ProxyServer::getDate(char *buffer, char *date) {
    char *p;
    char *ptr;
    const char *split = "\r\n";
    p = strtok_s(buffer, split, &ptr); // �ָ��ַ���
    while (p) {
        if (p[0] == 'D') { // Date
            if (strlen(p) > 6) {
                char header[6];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 4);
                if (strcmp(header, "Date") == 0) {
                    memcpy(date, p + 6, strlen(p) - 6);
                    return true;
                }
            }
        }
        p = strtok_s(NULL, split, &ptr);
    }
    return false;
}

void ProxyServer::changeHTTP(char *buffer, char *value) {
    const char* fieldToReplace = "Host";
    const char* newField = "If-Modified-Since: ";
    char* pos = strstr(buffer, fieldToReplace);
    char *check = strstr(buffer, newField);
    char *pos1 = strstr(pos, "\r\n");
    if (check != NULL) {
        char temp[MAXSIZE];
        strncpy(temp, buffer, pos - buffer);
        strncat(temp, pos, pos1 - pos);
        char* oldHostEnd = strstr(pos + strlen(fieldToReplace), "\r\n");
        if (oldHostEnd != NULL) {
            // Ҫ��host���浽newFieldǰ������ݲ���
            strncat(temp, oldHostEnd, check - oldHostEnd);
        }
        strcat(temp, newField);
        strcat(temp, value);
        strcat(temp, "\r\n");
        // Ҫ�� newField ��������ݲ���
        char *oldHostEnd1 = strstr(check + strlen(newField), "\r\n");
        if (oldHostEnd1 != NULL) {
            strcat(temp, oldHostEnd1);
        }
        ZeroMemory(buffer, strlen(buffer));
        memcpy(buffer, temp, strlen(temp));
    }
    else if (pos != NULL) {
        char temp[MAXSIZE];
        strncpy(temp, buffer, pos - buffer);
        strcat(temp, newField);
        strcat(temp, value);
        strcat(temp, "\r\n");
        // Ҫ�� host ���в���
        strncat(temp, pos, pos1 - pos);

        char* oldHostEnd = strstr(pos + strlen(fieldToReplace), "\r\n");
        if (oldHostEnd != NULL) {
            strcat(temp, oldHostEnd);
        }
        ZeroMemory(buffer, strlen(buffer));
        memcpy(buffer, temp, strlen(temp));
    }
}

bool getFileContents(char *filename, char *buffer, size_t bufferSize) {
    FILE *file;
    if (fopen_s(&file, filename, "rb") == 0) {
        size_t bytesRead = fread(buffer, sizeof(char), bufferSize, file);
        fclose(file);
        return bytesRead > 0;
    }
    return false;
}

void ProxyServer::getCache(char *buffer, char *filename) {
    const char *split = "\r\n";
    char *p, *ptr, status[4];
    char tempBuffer[MAXSIZE + 1];

    memset(status, 0, sizeof(status));
    memset(tempBuffer, 0, sizeof(tempBuffer));
    strncpy(tempBuffer, buffer, sizeof(tempBuffer) - 1);

    p = strtok_s(tempBuffer, split, &ptr); // ��ȡ��һ��
    strncpy(status, p + 9, sizeof(status) - 1);
    printf("��ȡ״̬�룺%s\n", status);

    if (strcmp(status, "304") == 0) {
        printf("��ȡ���ػ��棡\n");
        if (getFileContents(filename, buffer, MAXSIZE)) {
            // �����ļ����ݳɹ�
            needCache = false;
        }
    }
}

bool writeFile(const char *filename, const char *data) {
    FILE *file;
    if (fopen_s(&file, filename, "wb") == 0) {  // wb �Զ�����д��
        size_t bytesWritten = fwrite(data, sizeof(char), strlen(data), file); // д���ļ�
        fclose(file);
        return bytesWritten > 0;
    }
    return false;
}
void ProxyServer::saveCache(char *buffer, const char *url) {
    const char *split = "\r\n";
    char *p, *ptr, status[4];
    char tempBuffer[MAXSIZE + 1];

    memset(status, 0, sizeof(status));
    memset(tempBuffer, 0, sizeof(tempBuffer));
    strncpy(tempBuffer, buffer, sizeof(tempBuffer) - 1);

    p = strtok_s(tempBuffer, split, &ptr); // ��ȡ��һ��
    strncpy(status, p + 9, sizeof(status) - 1); // ��ȡ״̬��

    if (strcmp(status, "200") == 0) {
        char filename[100];
        memset(filename, 0, sizeof(filename));
        getFilename(url, filename, sizeof(filename)); // ��ȡ�ļ���
        if (writeFile(filename, buffer)) {
            printf("����ɹ���\n");
        }
        else {
            printf("����ʧ�ܣ��޷�д���ļ���\n");
        }
    }
}