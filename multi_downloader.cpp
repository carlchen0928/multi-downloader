#include "curlev.cpp"
#include <errno.h>

#define THREAD_NUM 8
#define CHUNK_SIZE (4 * 1024 * 1024)

pthread_t download_threads[THREAD_NUM];

extern void *downloader(void *thread_id);


void mkfifo_with_id(long thread_id)
{
    struct stat st;
    char fifo[64];
    sprintf(fifo, "hiper.pipe-%ld", thread_id);
    fprintf(MSG_OUT, "Creating named pipe \"%s\"\n", fifo);
    // <lstat> will follow symbol link file to the real file,
    // but <stat> will not.
    if ( lstat (fifo, &st) == 0 )
    {
        // if this file is a regular file (S_IFREG)
        if ( (st.st_mode & S_IFMT) == S_IFREG )
        {
            errno = EEXIST;
            perror("lstat");
            exit (1);
        }
    }
    unlink(fifo);
    if ( mkfifo (fifo, 0600) == -1 )
    {
        perror("mkfifo");
        exit (1);
    }
}

void init_threads()
{
    fprintf(MSG_OUT, "start init threads...\n");
    int rc;
    for (long i = 0; i < THREAD_NUM; ++i) {
        fprintf(MSG_OUT, "init threads %ld...\n", i);

        mkfifo_with_id(i);

        rc = pthread_create(&download_threads[i], NULL, downloader, (void *)i);
        if (rc) {
            fprintf(MSG_OUT, "create pthread error %d", rc);
            exit(-1);
        }
    }
    fprintf(MSG_OUT, "init threads finish.\n");
}

void create_file(const char* url)
{
    FILE* fp = fopen(get_filename(url), "w");
    if (!fp) {
        fprintf(MSG_OUT, "file create error.\n");
        exit (-1);
    }
    fclose(fp);
}

int start_download(const char* url)
{
    fprintf(MSG_OUT, "start push task into pipe.\n");

    create_file(url);

    CURL *handler = curl_easy_init();
    if (!handler) {
        fprintf(MSG_OUT, "create easy handler failed.\n");
        return CURLE_FAILED_INIT;
    }
    
    double downloadFileLength = 0.0;
    
    curl_easy_setopt(handler, CURLOPT_URL, url);
    curl_easy_setopt(handler, CURLOPT_HEADER, 1);
    curl_easy_setopt(handler, CURLOPT_NOBODY, 1);
    
    if (curl_easy_perform(handler) == CURLE_OK) {

        curl_easy_getinfo(handler, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &downloadFileLength);
        fprintf(MSG_OUT, "file length is: %lf\n", downloadFileLength);

        int needThread = downloadFileLength / CHUNK_SIZE;
        needThread = needThread < THREAD_NUM ? needThread : THREAD_NUM;
        double newChunk = downloadFileLength / needThread;
        
        char pipe_name[64];
        int pipe;
        for (long i = 0; i < needThread; ++i) {
            sprintf(pipe_name, "hiper.pipe-%ld", i);

            fprintf(MSG_OUT, "start write into pipe: %s\n", pipe_name);
            // FIXME: should do after thread have create fifo
            pipe = open(pipe_name, O_RDWR);

            FILE *file = fdopen(pipe, "w");
            if ( !file )
            {
                fprintf(MSG_OUT, "create pipe file failed.\n");
                exit (-1);
            }
            fprintf(MSG_OUT, "create pipe file success.\n");

            TaskInfo taskInfo;
            sprintf(taskInfo.url, "%s", url);
            taskInfo.start = i * newChunk;
            taskInfo.end = (i + 1) * newChunk > downloadFileLength ?
                            downloadFileLength : (i + 1) * newChunk;
            
            int rc;
 /*           if ((rc = fwrite(&taskInfo, sizeof(TaskInfo), 1, file)) <= 0) {
                fprintf(MSG_OUT, "write into pipe error， errno is %d, return value is: %d\n", errno, rc);
                exit(-1);
            }*/

            if ((rc = write(pipe, &taskInfo, sizeof(taskInfo))) <= 0) {
                fprintf(MSG_OUT, "write into pipe error， errno is %d, return value is: %d\n", errno, rc);
                exit(-1);
            }

            fprintf(MSG_OUT, "write into pipe size: %d\n", rc);
        }
    }
    else {
        fprintf(MSG_OUT, "error init easy curl\n");
        return -1;
    }
    
    return 0;
}

int main()
{
    fprintf(MSG_OUT, "start running...\n");

    init_threads();

    start_download("http://cdn.mysql.com/Downloads/MySQL-5.6/mysql-5.6.24-osx10.9-x86_64.tar.gz");

    for (int i = 0; i < THREAD_NUM; ++i)
    {
        pthread_join(download_threads[i], NULL);
    }
    return 0;
}



