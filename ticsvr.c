/*
Title: Tic Tac Toe server
Author: Jimmy Yu
Date: Aug 2020

Citations:
    Author: Alan J Rosenthal
    Date: December 2000
    Title: muffinman.c
    Availability: https://www.teach.cs.toronto.edu/~ajr/209/a4/muffinman.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct node{
    int clientfd;
    struct in_addr ipaddr;
    struct node *next;
    int xoPlayer; // -1 for viewer, 0 for x player, 1 for o player
};

int port = 3000;
int listen_soc;
char board[9];
struct node *head = NULL;
int listSize = 1;
int xoTurn = 0;

int main(int argc, char **argv)
{
    // Get options flag
    extern void showboard(int fd);
    extern void activity(struct node *p);
    extern void newconnection();
    extern void bind_and_listen();
    extern int game_is_over();
    extern void broadcast();
    extern void initialiseboard();
    extern void broadcastboard();
    int option, status = 0;
    while((option=getopt(argc, argv, "p:")) != EOF)
    {
        switch(option)
        {
            case 'p':
                port = atoi(optarg);
                break;
            case '?':
            default:
                status = 1;
                break;
        }
    }
    
    if(status || optind < argc)
    {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        return(1);
    }
    
    // Setup server
    bind_and_listen();

    // Initialise the board to 1 through 9
    initialiseboard();

    struct node *ptr = NULL;
    while(1)
    {
        fd_set fds;
        int maxfd = listen_soc;
        FD_ZERO(&fds);
        FD_SET(listen_soc, &fds);
        for(ptr = head; ptr; ptr = ptr->next)
        {
            FD_SET(ptr->clientfd, &fds);
            if(ptr->clientfd > maxfd)
                maxfd = ptr->clientfd;
        }
        if (select(maxfd +1, &fds, NULL, NULL, NULL) < 0)
        {
            perror("select");
        }
        else
        {
            for(ptr = head; ptr; ptr = ptr->next)
            {
                if(FD_ISSET(ptr->clientfd, &fds))
                {
                    break;
                }
            }
            if(ptr)
            {
                activity(ptr);
            }
            if(FD_ISSET(listen_soc, &fds))
            {
                newconnection();
            }
            if(game_is_over() != 0 && game_is_over() != ' ')
            {
                broadcast("Game is over!\r\n", 15);
                broadcastboard();
                if(game_is_over() == 'x')
                {
                    broadcast("x wins.\r\n", 9); 
                }
                else
                {
                    broadcast("o wins.\r\n", 9);
                }
                broadcast("Let's play again!\r\n", 19);
                // Re-initialise the board
                initialiseboard();
                if(xoTurn % 2 == 0)
                    broadcast("x goes first!\r\n", 15);
                else
                    broadcast("o goes first!\r\n", 15);
            }
            else if(game_is_over() == ' ')
            {
                broadcast("Game is over!\r\n", 15);
                broadcastboard();
                broadcast("It is a draw.\r\n", 15);
                broadcast("Let's play again!\r\n", 19);
                initialiseboard();
                if(xoTurn % 2 == 0)
                    broadcast("x goes first!\r\n", 15);
                else
                    broadcast("o goes first!\r\n", 15);
            }
        }
    }
    return(0);
}

void activity(struct node *p)
{
    int len;
    int bytes_in_buf = 0;
    char buf[1024];
    extern void removeclient(int fd);
    extern void showboard(int fd);
    extern void broadcast();
    extern void broadcastboard();

    while(buf[bytes_in_buf-1] != '\n')
    {
        if((len=read(p->clientfd, buf + bytes_in_buf, sizeof buf - bytes_in_buf - 1)) < 0)
        {
            perror("read");
            break;
        }
        else if(len == 0)
        {
            printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
            removeclient(p->clientfd);
            return;
        }
        bytes_in_buf += len;
    }
    buf[bytes_in_buf] = '\0';

    // strlen(buf) == 2 (one for the digit, one for the newline)
    if((strlen(buf) == 2) && (p->xoPlayer == 0) && (xoTurn % 2 == 0))
    {
        int move = buf[0] - '0';
        if(board[move-1] != 'x' && board[move-1] != 'o')
        {
            board[move-1] = "xo"[p->xoPlayer];
            xoTurn++;
            printf("%s makes move %d\n", inet_ntoa(p->ipaddr), move);
            broadcastboard();
            broadcast("It is o's turn\r\n", 16);
        }
        else
            broadcast("Invalid move!\r\n", 15); 
    }
    else if((strlen(buf) == 2) && (p->xoPlayer == 1) && (xoTurn % 2 == 1))
    {
        int move = buf[0] - '0';
        if(board[move-1] != 'x' && board[move-1] != 'o')
        {
            board[move-1] = "xo"[p->xoPlayer];
            xoTurn++;
            printf("%s makes move %d\n", inet_ntoa(p->ipaddr), move);
            broadcastboard();
            broadcast("It is x's turn\r\n", 16);
        }
        else
            broadcast("Invalid move!\r\n", 15);
    }
    else
    {
        //broadcast(buf, sizeof buf);
        struct node *ptr = NULL;
        for(ptr = head; ptr; ptr=ptr->next)
        {
            if(ptr->clientfd != p->clientfd)
            {
                if(write(ptr->clientfd, buf, strlen(buf)) != strlen(buf))
                {
                    perror("write");
                }
            }
        }
        printf("chat message: %s", buf);
    }
}

void newconnection()
{
    extern void addclient(int fd, struct in_addr addr);
    extern void showboard(int fd);
    int fd;
    struct sockaddr_in r;
    socklen_t size = sizeof r;

    if((fd=accept(listen_soc, (struct sockaddr *)&r, &size)) < 0)
    {
        perror("accept");
    }
    else
    {
        showboard(fd);
        printf("connection from %s\n", inet_ntoa(r.sin_addr));
        addclient(fd, r.sin_addr);
    }
}

void bind_and_listen()
{
    // Set up server
    listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_soc == -1)
    {
        perror("socket");
        exit(1);
    }

    // Bind and listen
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(addr.sin_zero), 0, 8);

    if(bind(listen_soc, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        close(listen_soc);
        exit(1);
    }

    if (listen(listen_soc, 5) < 0)
    {
        perror("listen");
        exit(1);
    }
 
}

void addclient(int fd, struct in_addr addr)
{
    extern void assignPlayers();
    struct node *newNode = malloc(sizeof(struct node));
    if(newNode == NULL)
    {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }
    printf("Adding client %s\n", inet_ntoa(addr));
    fflush(stdout);
    // Initialize the node and add to head of linked list
    newNode->clientfd = fd;
    newNode->ipaddr = addr;
    newNode->next = head;
    newNode->xoPlayer = -1;
    head = newNode;
    listSize++;
    printf("Number of clients: %d\n", listSize);
    assignPlayers();
}

void removeclient(int fd)
{
    extern void assignPlayers();
    struct node **pp;
    for(pp = &head; *pp && (*pp)->clientfd != fd; pp = &(*pp)->next)
        ;

    if(*pp)
    {
        struct node *p = (*pp)->next;
        printf("Removing client %s\n", inet_ntoa((*pp)->ipaddr));
        fflush(stdout);
        free(*pp);
        *pp = p;
        listSize--;
    }
    else
    {
        fprintf(stderr, "Removing node with fd %d failed\n", fd);
        fflush(stderr);
    }
    printf("Number of clients: %d\n", listSize);
    assignPlayers();
}

void assignPlayers()
{
    struct node *p = NULL;
    int xplayercount = 0;
    int oplayercount = 0;
    for(p = head; p; p = p->next)
    {
        if(p->xoPlayer == 0)
        {
            xplayercount++;
        }
        else if(p->xoPlayer == 1)
        {
            oplayercount++;
        }
    }
    
    if(xplayercount == 0)
    {
        for(p = head; p && p->xoPlayer == -1 && p->xoPlayer != 1; p = p->next)
        {
            p->xoPlayer = 0;
            printf("Assigned x player\n");
            if(write(p->clientfd, "You now get to play! You are now x\r\n", 36) != 36)
            {
                perror("write");
            }
            break;
        }
    }

    if(oplayercount == 0)
    {
        for(p = head; p && p->xoPlayer == -1 && p->xoPlayer != 0; p = p->next)
        {
            p->xoPlayer = 1;
            printf("Assigned o player\n");
            if(write(p->clientfd, "You now get to play! You are now o\r\n", 36) != 36)
            {
                perror("write");
            }
            break;
        }
    }
}

void initialiseboard()
{
    // Initialise the board to 1 through 9
    for(int i = 1; i <= 9; i++)
        board[i-1] = i + '0';
}

void broadcastboard()
{
    extern void showboard(int fd);
    struct node *ptr = NULL;
    for(ptr = head; ptr; ptr=ptr->next)
        showboard(ptr->clientfd);
}

void broadcast(char *s, int size)
{
    struct node *p;
    for(p = head; p; p = p->next)
    {
        if(write(p->clientfd, s, size) != size)
        {
            perror("write");
        }
    }
}

void showboard(int fd)
{
    char buf[100], *bufp, *boardp;
    int col, row;
    //struct client *p;

    for (bufp = buf, col = 0, boardp = board; col < 3; col++) {
        for (row = 0; row < 3; row++, bufp += 4)
            sprintf(bufp, " %c |", *boardp++);
        bufp -= 2;  // kill last " |"
        strcpy(bufp, "\r\n---+---+---\r\n");
        bufp = strchr(bufp, '\0');
    }
    if (write(fd, buf, bufp - buf) != bufp-buf)
        perror("write");
}

int game_is_over()  /* returns winner, or ' ' for draw, or 0 for not over */
{
    int i, c;
    extern int allthree(int start, int offset);
    extern int isfull();

    for (i = 0; i < 3; i++)
        if ((c = allthree(i, 3)) || (c = allthree(i * 3, 1)))
            return(c);
    if ((c = allthree(0, 4)) || (c = allthree(2, 2)))
        return(c);
    if (isfull())
        return(' ');
    return(0);
}

int allthree(int start, int offset)
{
    if (board[start] > '9' && board[start] == board[start + offset]
            && board[start] == board[start + offset * 2])
        return(board[start]);
    return(0);
}

int isfull()
{
    int i;
    for (i = 0; i < 9; i++)
        if (board[i] < 'a')
            return(0);
    return(1);
}

