#include "Grafana_Interface.h"


/********** GLOBAL VARIABLES **********/

// Override of SIGINT signal to stop the execution of the program
static volatile int Running = 1;





/********** PROTOTYPE DEFINITIONS **********/

// Catch SIGINT Signal (CTRL+C)
void CatchSignal(int signo)
{
    Running = 0;
}


// Initialization of RX Socket Network (1 per topic)
// TopicIPAddr: IP Address of the Topic to listen
// Return:      ERROR (-1) / Reception Socket File Descriptor
int NetworkInitRXSocket(void)
{
    // RX Socket Configuration
    int RXSocket;
    SOCKADDR_IN RXConfig;

    // MulticastGroup (IP Multicast Address of the Group, Local IP Address of Interface)
    struct ip_mreq MulticastGroup = {inet_addr(MULTICAST_IP), htonl(INADDR_ANY)};

    // Multicast Timeout (Timeout in Second, Timeout in USecond)
    struct timeval MulticastTimeout = {MULTICAST_TIMEOUT, 0};

    // Create Reception Socket
    RXSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (RXSocket < 0)
        return -1;

    // Configuration of RXSocket
    memset(&RXConfig, 0, sizeof(RXConfig));
    RXConfig.sin_family = AF_INET;
    RXConfig.sin_port = htons(MULTICAST_PORT);
    RXConfig.sin_addr.s_addr = htonl(INADDR_ANY);

    // Join the MulticastGroup
    if (setsockopt(RXSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &MulticastGroup, sizeof(MulticastGroup)) < 0)
        return -1;

    // Set Multicast Receive Timeout
    if (setsockopt(RXSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &MulticastTimeout, sizeof(MulticastTimeout)) < 0)
        return -1;

    // Bind RXSocket
    if (bind(RXSocket, (struct sockaddr*) &RXConfig, sizeof(RXConfig)) < 0)
        return -1;

    return RXSocket;   
}


// Initialization of Network 
// Return:  ERROR (-1) / Transmission Socket File Descriptor
int NetworkInitTXSocket(void)
{    
    // TX Socket Configuration
    int TXSocket;

    // Multicast TTL Configuration
    u_char TTL = MULTICAST_TTL;

    // Multicast Timeout (Timeout in Second, Timeout in USecond)
    struct timeval MulticastTimeout = {MULTICAST_TIMEOUT, 0};
    
    // Create Transmission Socket
    TXSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (TXSocket < 0)
        return -1;

    // Configure Multicast TTL
    if (setsockopt(TXSocket, IPPROTO_IP, IP_MULTICAST_TTL, &TTL, sizeof(TTL)) < 0)
        return -1;

    // Set Multicast Transmit Timeout
    if (setsockopt(TXSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &MulticastTimeout, sizeof(MulticastTimeout)) < 0)
        return -1;
    return TXSocket;
}


// Receive Data from Network
// RXSocket:    Reception Socket File Descriptor
// Return:      No Data Available (-1) / Data Available
int NetworkReceive(int RXSocket)
{
    SOCKADDR_IN ClientConfig;
    socklen_t Length = sizeof(ClientConfig);

    char MyCMD[DATA_FORMATING_LENGTH] = {0};

    // Parse MySerialTX
    char MySerialTX[DATA_FORMATING_LENGTH] = {0};
    char *Parser = MySerialTX;

    Transaction MyTX;

    if (recvfrom(RXSocket, &MySerialTX, sizeof(MySerialTX), 0, (struct sockaddr*) &ClientConfig, &Length) != -1)
    {
        // Sensor Topic
        if ((MySerialTX[0] == 'S') && (MySerialTX[1] == *DATA_DELIMITER))
        {
            // Find Publisher
            Parser = MySerialTX+2;
            while( (*Parser != 0) && (*Parser != *DATA_DELIMITER) )
                Parser++;

            // Set End of First Data Part
            if (*Parser == *DATA_DELIMITER)
                *Parser = 0;
            else
                return -1;

            snprintf(MyCMD, DATA_FORMATING_LENGTH, "./MySQLAccess.sh 1 %s", MySerialTX+2);
            system(MyCMD);
        }

        else
        {
            Parser = strtok(MySerialTX, DATA_DELIMITER);
            MyTX.Subscriber = Parser;

            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.Publisher = Parser;
                
            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.SmartContract = Parser;

            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.Price = atoi(Parser);

            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.Time = atoi(Parser);

            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.PrevState = Parser;

            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.DCoin = atoi(Parser);

            Parser = strtok(NULL, DATA_DELIMITER);
            MyTX.Nonce = atoi(Parser);

            // Check Light PoW
            if (Check_Light_PoW(MyTX) == 0)
            {
                // Add New Buying Request
                if (MyTX.Subscriber == MyTX.Publisher)
                    snprintf(MyCMD, DATA_FORMATING_LENGTH, "./MySQLAccess.sh 2 %s %s %s %d %d %s %d %d", MyTX.Subscriber.c_str(), MyTX.Publisher.c_str(), MyTX.SmartContract.c_str(), MyTX.Price, MyTX.Time, MyTX.PrevState.c_str(), MyTX.DCoin, MyTX.Nonce);

                // Add New Consensus Response
                else
                    snprintf(MyCMD, DATA_FORMATING_LENGTH, "./MySQLAccess.sh 3 %s %s %s %d %d %s %d %d", MyTX.Subscriber.c_str(), MyTX.Publisher.c_str(), MyTX.SmartContract.c_str(), MyTX.Price, MyTX.Time, MyTX.PrevState.c_str(), MyTX.DCoin, MyTX.Nonce);

                system(MyCMD);
            }
        }
        return 0;
    }
    else
        return -1;
}


// Send Request Transaction to the Network
// TXSocket:    Transmission Socket File Descriptor
// MyCMD:       Request Transaction to transmit (string format)
void NetworkSend_RequestTransaction(int TXSocket, char* MyCMD)
{
    // TXSocket Configuration
    SOCKADDR_IN TXConfig;
    memset(&TXConfig, 0, sizeof(TXConfig));
    TXConfig.sin_family = AF_INET;
    TXConfig.sin_port = htons(MULTICAST_PORT);
    TXConfig.sin_addr.s_addr = inet_addr(MULTICAST_IP);

    // Request Transaction
    Transaction MyTX;

    // Parse MyCMD
    char *Token;

    Token = strtok(MyCMD, DATA_DELIMITER);
    MyTX.SmartContract = Token;

    Token = strtok(NULL, DATA_DELIMITER);
    MyTX.Price = atoi(Token);
        
    Token = strtok(NULL, DATA_DELIMITER);
    MyTX.Subscriber = Token;
    MyTX.Publisher = Token;

    Token = strtok(NULL, DATA_DELIMITER);
    MyTX.PrevState = Token;
        
    MyTX.Time = time(NULL);
    MyTX.DCoin = -1;
    MyTX.Nonce = Compute_Light_PoW(MyTX);

    // Serialize Transaction
    char MySerialTX[DATA_FORMATING_LENGTH];
    snprintf(MySerialTX, DATA_FORMATING_LENGTH, "%s%s%s%s%s%s%d%s%d%s%s%s%d%s%d", \
            MyTX.Subscriber.c_str(), DATA_DELIMITER, MyTX.Publisher.c_str(), DATA_DELIMITER, MyTX.SmartContract.c_str(), DATA_DELIMITER, MyTX.Price, DATA_DELIMITER, MyTX.Time, DATA_DELIMITER, MyTX.PrevState.c_str(), DATA_DELIMITER, MyTX.DCoin, DATA_DELIMITER, MyTX.Nonce);

    // Send Data to the Network
    sendto(TXSocket, MySerialTX, strlen(MySerialTX), 0, (struct sockaddr*) &TXConfig, sizeof(TXConfig));
}



/********** MAIN PART **********/

int main (int argc, char **argv)
{
    // Network
    int RXSocket;
    int TXSocket;

    char MyCMD[DATA_FORMATING_LENGTH];

    // Reading Pipes: Web Interface -> Grafana Interface
    FILE* Pipe_Web_to_Grafana;
    fclose(fopen(PIPE_WEB_INTERFACE_TO_GRAFANA, "w"));
    Pipe_Web_to_Grafana = fopen(PIPE_WEB_INTERFACE_TO_GRAFANA, "r");
    if (Pipe_Web_to_Grafana == NULL)
    {
        printf("ERROR OPENNING PIPE WEB INTERFACE TO GRAFANA : %s\n", strerror(errno));
        return -1;
    }

    // Init Network
    RXSocket = NetworkInitRXSocket();
    TXSocket = NetworkInitTXSocket();

    if ( (RXSocket == -1) || (TXSocket == -1) )
        return -1;

    // Init Majoritychain Database & Tables
    system("./MySQLAccess.sh 0");

    signal(SIGINT, CatchSignal);
    while (Running)
    {
        // Read Pipe: Data from Grafana Interface to send on network
        while (fgets(MyCMD, sizeof(MyCMD), Pipe_Web_to_Grafana) != NULL)
        {
            NetworkSend_RequestTransaction(TXSocket, MyCMD);
        }

        // Listen Network
        while (NetworkReceive(RXSocket) != -1);

        // Consensus
        system("./MySQLAccess.sh 4");
    }

    fclose(Pipe_Web_to_Grafana);
    close(RXSocket);
    close(TXSocket);

    // Clear Grafana Database & Tables
    system("./MySQLAccess.sh 6");

    printf("By Grafana Interface\n");
    return 0;
}