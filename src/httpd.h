#ifndef HTTPD_IS_IN
#define HTTPD_IS_IN

#include <pthread.h>

typedef struct http_request *HTTPD_Request;

pthread_t HTTPD_Start(const char *bindName, void (*func)(HTTPD_Request req, const char *method, const char *url) );

void HTTPD_Send_Status( HTTPD_Request req, int status, const char *text);      // optional, will be sent as 200 if you try to skip it
void HTTPD_Add_Header( HTTPD_Request req, const char *h);  // optional
void HTTPD_Send_Body( HTTPD_Request req, const void *data, int length);
void HTTPD_Send_Body_Chunk(HTTPD_Request req, const void *data, int length);
void HTTPD_Push( HTTPD_Request req);

const char *HTTPD_Get_Authorization( HTTPD_Request req);  // NULL if none given

#endif
