/*
 * threaded.c -- A simple multi-threaded FastCGI application.
 */

#include <fcgi_config.h>

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcgiapp.h>
#include <string.h>
#include <sys/wait.h>

#include "tinycamd.h"

#define THREAD_COUNT 20

static pthread_mutex_t counts_mutex = PTHREAD_MUTEX_INITIALIZER;
static int counts[THREAD_COUNT];
static int sock;

static void do_status_request( FCGX_Request *req, int thread_id)
{
    char *server_name;
    pid_t pid = getpid();
    int i;

    server_name = FCGX_GetParam("SERVER_NAME", req->envp);
    
    FCGX_FPrintF(req->out,
		 "Content-type: text/html\r\n"
		 "\r\n"
		 "<title>FastCGI Hello! (multi-threaded C, fcgiapp library)</title>"
		 "<h1>FastCGI Hello! (multi-threaded C, fcgiapp library)</h1>"
		 "Thread %d, Process %ld<p>"
		 "Request counts for %d threads running on host <i>%s</i><p><code>",
		 thread_id, pid, THREAD_COUNT, server_name ? server_name : "?");
    
    pthread_mutex_lock(&counts_mutex);
    ++counts[thread_id];
    for (i = 0; i < THREAD_COUNT; i++)
	FCGX_FPrintF(req->out, "%5d " , counts[i]);
    pthread_mutex_unlock(&counts_mutex);
}

static void put_image(const struct chunk *c, void *arg)
{
    FCGX_Request *req = (FCGX_Request *)arg;

    FCGX_FPrintF(req->out, "Content-type: image/jpeg\r\n"
		 "\r\n");
    while( c->data) {
	FCGX_PutStr( c->data, c->length, req->out);
	c++;
    }
}

static void *handle_requests(void *a)
{
    int rc, thread_id = (int)a;
    FCGX_Request request;
    char *path_info;

    FCGX_InitRequest(&request, sock, 0);

    for (;;)
    {
        static pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;

        /* Some platforms require accept() serialization, some don't.. */
        pthread_mutex_lock(&accept_mutex);
        rc = FCGX_Accept_r(&request);
        pthread_mutex_unlock(&accept_mutex);

        if (rc < 0)
            break;

	path_info = FCGX_GetParam("PATH_INFO", request.envp);

	if ( strcmp(path_info,"/status")==0) {
	    do_status_request(&request, thread_id);
	} else {
	    with_current_frame( &put_image, &request);
	}

        FCGX_Finish_r(&request);
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int i;
    pthread_t id[THREAD_COUNT];
    pthread_t captureThread;

    do_options(argc, argv);

    open_device();
    init_device();
    start_capturing();

    pthread_create( &captureThread, NULL, main_loop, NULL);

    FCGX_Init();
    sock = FCGX_OpenSocket(":3636",20);

    for (i = 1; i < THREAD_COUNT; i++)
        pthread_create(&id[i], NULL, handle_requests, (void*)i );

    for(;;) sleep(100);

    close_device();

    return 0;
}
