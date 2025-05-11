#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>
#include<sys/types.h>
#include<sys/socket.h>

#define MAX_MSG_LENGTH 1024
#define MAX_IP_LENGTH 15
#define MAX_ID_LENGTH 32

//--------------------------- PRIVATE TYPES -------------------------------

pthread_mutex_t mutex;

// ClientNode
typedef struct ClientNode {
	char* id;
	char* ip;
	int port;
	struct ClientNode* next;
} ClientNode;

// Node
typedef ClientNode* Node;

// newNode()
// Constructor for private Node type
Node newNode(char* id, char* ip, int port) {
	Node N = malloc(sizeof(ClientNode));
	assert(N!=NULL);
	N->id = id;
	N->ip = ip;
	N->port = port;
	N->next = NULL;
	return(N);
}

// freeNode()
// Destructor for private Node type
void freeNode(Node* pN) {
	if(pN!=NULL && *pN!=NULL) {
		free(*pN);
		*pN = NULL;
	}
}

// ClientList
typedef struct ClientList {
	Node top;
	int numClients;
} ClientList;

// List
typedef struct ClientList* List;

//--------------------------- PUBLIC FUNCTIONS -----------------------------

// newClientList()
// Constructor for the List type
List newClientList(void) {
	List L = malloc(sizeof(ClientList));
	assert(L!=NULL);
	L->top = NULL;
	L->numClients = 0;
	return(L);
}

// isEmpty()
// Check if List is empty
// Returns 1 (true) if L is empty, 0 (false) otherwise
int isEmpty(List L) {
	return(L->numClients==0);
}

// makeEmpty()
// Resets L to the empty state
void makeEmpty(List L) {

	// If no items in List, throw an exception
	if( isEmpty(L) ) {
		perror("Calling makeEmpty() on empty List\n");
		exit(EXIT_FAILURE);
	}

	// Dereference and free each node
	while(L->numClients > 0) {
		Node N;
		N = L->top;
		L->top = L->top->next;
		L->numClients--;
		freeNode(&N);
	}
}

// freeClientList()
// Destructor for the List type
void freeClientList(List* pL) {
	if( pL!=NULL && *pL!=NULL ) {
		if( !isEmpty(*pL) ) makeEmpty(*pL);
		free(*pL);
		*pL = NULL; 
	}
}

// size()
// Returns the number of clients in List
int size(List L) {
	return(L->numClients);
}

// lookup()
// Check if specified client is in the List
// returns ID, or NULL if no such ID exists
char* lookup(List L, char* id) {
	for(Node N=L->top; N!=NULL; N=N->next) {
		if(strcmp(id, N->id)==0) return N->id;
	}
	return(NULL);
}

// insert()
// Insert new client (ID, IP, PORT) truple in L
int insertToList(List L, char* id, char* ip, int port) {

	pthread_mutex_lock(&mutex);

	// If ID exists in List, throw an exception
	if ( lookup(L, id)!=NULL ) {
		perror("ID collision\n");
		return(-1);
	}

	// If List has no clients, create one
	else if( L->top==NULL ) L->top = newNode(id, ip, port);
	
	// If List has clients, insert client at end of List
	else {
		Node N = L->top;
		while( (N->next)!= NULL ) N = N->next;
		N->next = newNode(id, ip, port);
	}

	pthread_mutex_unlock(&mutex);

	// Add 1 to number of clients
	L->numClients++;
	
	return(1);
}

// remove()
// Deletes client node with specified ID
void removeFromList(List L, char* id) {

	pthread_mutex_lock(&mutex);

	// If ID doesn't exist in the List, throw an exception
	if( lookup(L, id)==NULL ) {
		perror("ID not found\n");
		exit(-1);
	}

	// If ID is the 1st node of List, alter head to point at 2nd node
	else if(strcmp((L->top)->id, id)==0) {
		Node N = L->top;
		L->top = (L->top)->next;
		freeNode(&N);
	}

	// If ID is the nth item of List, alter pointer at specified ID
	// node to point at the next node
	else {
		for(Node N = L->top; (N->next)!=NULL; N=N->next) {
			if(strcmp((N->next)->id, id)==0) {
				Node M = N->next;
				N->next = N->next->next;
				freeNode(&M);
				break;
			}
		}
	}

	pthread_mutex_unlock(&mutex);

	// Subtract 1 from number of clients
	L->numClients--;
}

int sendClientList(List L, int sockfd) {

	pthread_mutex_lock(&mutex);

	char buffer[MAX_ID_LENGTH];
	for(Node N=L->top; N!=NULL; N=N->next) {
		memset(buffer, 0, MAX_ID_LENGTH);
		strcpy(buffer, N->id);
		if(send(sockfd, buffer, MAX_ID_LENGTH, 0) < 0) {
			pthread_mutex_unlock(&mutex);
			perror("Fail to send to client");
			return(-1);
		}
	}
	if(send(sockfd, "\n", 1, 0) < 0)
	{
			pthread_mutex_unlock(&mutex);
			perror("Fail to send to client\n");
			return(-1);		
	}

	pthread_mutex_unlock(&mutex);
	return(1);
}

int sendClientInfo(List L, int sockfd, char* id) {

	pthread_mutex_lock(&mutex);

	char buffer[MAX_MSG_LENGTH];
	memset(buffer, 0, MAX_MSG_LENGTH);

	// if ID in L, send info to client
	for(Node N=L->top; N!=NULL; N=N->next) {
		if(strcmp(id, N->id)==0) {
			sprintf(buffer, "%s %s", N->ip, N->port);
			if(send(sockfd, buffer, MAX_MSG_LENGTH, 0) < 0) {
				perror("Fail to send to client");
				pthread_mutex_unlock(&mutex);
				return(-1);
			}
			pthread_mutex_unlock(&mutex);
			removeFromList(L, id);
			return(1);
		}
	}

	// if ID not in L, send "DNE" to client
	strcpy(buffer, "DNE");
	if(send(sockfd, buffer, MAX_MSG_LENGTH, 0) < 0) {
		perror("Fail to send to client");
		pthread_mutex_unlock(&mutex);
		return(-1);
	}

	pthread_mutex_unlock(&mutex);
	return(-1);
}
