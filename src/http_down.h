// http下载类的申明，包含http下载的相关接口
// 作者：龚一帆
// 创建日期：2017年11月7日

#ifndef HTTP_DOWN_H
	#define HTTP_DOWN_H
	#include <iostream>
	#include <cstdio>
	#include <getopt.h>
	#include <fstream>
	#include <pthread.h>//多线程
	#include <string.h>//字符串处理
	#include <sys/socket.h>//套接字
	#include <arpa/inet.h>//ip地址处理
	#include <fcntl.h>//open系统调用
	#include <unistd.h>//write系统调用
	#include <netdb.h>//查询DNS
	#include <stdlib.h>//exit函数
	#include <sys/stat.h>//stat系统调用获取文件大小
	#include <sys/time.h>//获取下载时间
	using namespace std;

	struct HTTP_RES_HEADER//储存相应头信息
	{
	    int status_code;//HTTP/1.1 '200' OK
	    char content_type[128];//Content-Type: application/gzip
	    long content_length;//Content-Length: 11683079
	    long range_start;
	    long range_end;
	};

	class HttpDown
	{
	public:
		HttpDown(const char *url,const char * dir);
		~HttpDown();

		int Connent(long range_start,long range_end);//建立连接,返回http状态码
		int Download(long & FileLength);//开始下载
		void CloseConnect(int code);//关闭链接
		void ParseURL();//解析url链接
		void HttpCode(int status_code);//分析httpcode
		void SetSocket(struct sockaddr_in &addr);
		void ReadSocket();//读socket
		void GetHttpHead(long range_start,long range_end);//获取发送端的http头
		void GetIPAddr();//获取IP地址
		long GetFileSize(const char * dir_name);//获取已下载文件的大小
		struct HTTP_RES_HEADER ParseHeader();//解析http头
		long GetFileSumLength(){return _file_sum_length;};
		long GetFileLength(){return _file_length;};
		long GetHasRecieve(){return _hasrecieve;};
		char * GetFileName(){return _file_name;};
		char * GetLastModified(){return _lastModified;};
		void SetDir(const char *filename){ strcpy(_dir_file, filename); }//设置文件保存的地址
	private:
    	int  _port;//远程主机端口, http默认80端口
    	long _file_length;//本次需下载文件的大小
    	long _file_sum_length;//文件总大小
    	long _hasrecieve;//已下载文件大小
    	int  _client_socket;//存储套接字
    	static const int _mem_size = 4096;
    	char _lastModified[30];//Last_Modified
		char _url[2048];//下载链接URL
    	char _host[64];//远程主机地址
    	char _ip_addr[16];//远程主机IP地址
    	char _file_name[256];//下载文件名
    	char _dir_file[256];//文件保存地址
    	char _header[2048];//http头
    	char * _response;//读http头缓冲区
    	char * _buf;//写文件缓冲区

	};

#endif