// http下载类的实现，包含http下载的相关接口
// 作者：龚一帆
// 创建日期：2017年11月7日

#include "http_down.h"

HttpDown::HttpDown(const char * url,const char * dir)
{
	strcpy(_url, url);
	strcpy(_dir_file, dir);

	_buf = (char *) malloc( 2 * _mem_size * sizeof(char) + 8);
    _response = (char *) malloc(_mem_size * sizeof(char));
}

HttpDown::~HttpDown()
{
	free(_response);
	free(_buf);
}

void HttpDown::ParseURL()
{
    /*通过url解析出域名, 端口, 以及文件名*/
    int j = 0;
    int start = 0;
    bool flag = false;
    string patterns[2] = {"http://","https://"};
    for (int i = 0; i<2; i++)//分离下载地址中的http协议
    {
        if (strncmp(_url, patterns[i].c_str(), patterns[i].length()) == 0)
        {
        	start = patterns[i].length();
        	flag = true;
        }
    }
    if(!flag)
    {
    	cout<<"这不是一个合法的url！"<<endl;
        exit(0);
    }
    //解析域名, 这里处理时域名后面的端口号会保留
    for (int i = start; _url[i] != '/' && _url[i] != '\0'; i++, j++)
        _host[j] = _url[i];
    _host[j] = '\0';

    //解析端口号, 如果没有, 那么设置端口为80
    char *pos = strstr(_host, ":");
    if (pos)
        sscanf(pos,":%d", &_port);
    else
    	_port = 80;
    //删除域名端口号
    int length = (int)strlen(_host);
    for (int i = 0; i < length; i++)
    {
        if (_host[i] == ':')
        {
            _host[i] = '\0';
            break;
        }
    }

    //获取下载文件名
    j = 0;
    for (int i = start; _url[i] != '\0'; i++)
    {
        if (_url[i] == '/')
        {
            if (i !=  strlen(_url) - 1)
                j = 0;
            continue;
        }
        else
            _file_name[j++] = _url[i];
    }
    _file_name[j] = '\0';
}

void HttpDown::GetIPAddr()
{
    /*通过域名得到相应的ip地址*/
    struct hostent *host = gethostbyname(_host);//此函数将会访问DNS服务器
    if (!host)
    {
        cout<<"找不到IP地址，请检查DNS服务器!"<<endl;
        exit(0);
    }

    for (int i = 0; host->h_addr_list[i]; i++)
    {
        strcpy(_ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        break;
    }

    if (strlen(_ip_addr) == 0)
    {
        printf("错误: 无法获取到远程服务器的IP地址, 请检查下载地址的有效性\n");
        exit(0);
    }
}

void GetDate(char * response,char * LastModified)
{
	char *pos = strstr(response, "Last-Modified:");
    if (pos)//获取Last-Modified
	{
		strncpy( LastModified , pos+15 , 29);
	}
}

struct HTTP_RES_HEADER HttpDown::ParseHeader()
{
    /*获取响应头的信息*/
    struct HTTP_RES_HEADER resp;

    char *pos = strstr(_response, "HTTP/");
    if (pos)//获取返回代码
        sscanf(pos, "%*s %d", &resp.status_code);

    pos = strstr(_response, "Content-Type:");
    if (pos)//获取返回文档类型
        sscanf(pos, "%*s %s", resp.content_type);

    pos = strstr(_response, "Content-Length:");
    if (pos)//获取返回文档长度
        sscanf(pos, "%*s %ld", &resp.content_length);

    pos = strstr(_response, "Content-Range:");
    if (pos)//获取返回文件区间
        sscanf(pos, "%*s %*s %ld-%ld/%ld", 
        	&resp.range_start,&resp.range_end,&resp.content_length);

    GetDate(_response,_lastModified);

    return resp;
}

void HttpDown::SetSocket(struct sockaddr_in &addr)
{
	_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_client_socket < 0)
    {
        printf("套接字创建失败: %d\n", _client_socket);
        exit(-1);
    }
    //创建IP地址结构体
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(_ip_addr);
    addr.sin_port = htons(_port);
}

long HttpDown::GetFileSize(const char * dir_name)
{
	//通过系统调用直接得到文件的大小
    struct stat buf;
    if (stat(dir_name, &buf) < 0)
        return 0;
    return (unsigned long) buf.st_size;
}

void HttpDown::HttpCode(int status_code)
{
	switch(status_code)
    {
    	case 416:
    	case 200:
    	case 206: break;
    	default :
    	{
    		cout<<"文件无法下载, 远程主机返回: "<<status_code<<endl;
        	exit(0);
    	}
    }
}

void HttpDown::GetHttpHead(long range_start,long range_end)
{
	//设置http请求头信息
	int Max = max(range_start,range_end);//为了解决请求报文长度异常的bug
	int Min = min(range_start,range_end);

    sprintf(_header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s\r\n"\
            "Connection: keep-alive\r\n"\
            "Range: bytes=%ld-%ld\r\n"\
            "\r\n"\
        , _url, _host, Min,Max);
}

void HttpDown::ReadSocket()
{
	int mem_size = _mem_size;
    int length = 0;
    int len;
	while ((len = read(_client_socket, _buf, 1)) != 0)
    {
        if (length + len > mem_size)
        {
            //动态内存申请, 因为无法确定响应头内容长度
            mem_size *= 2;
            char * temp = (char *) realloc(_response, sizeof(char) * mem_size);
            if (temp == NULL)
            {
                printf("动态内存申请失败\n");
                exit(-1);
            }
            free(_response);
            _response = temp;
        }

        _buf[len] = '\0';
        strcat(_response, _buf);

        //找到响应头的头部信息
        int flag = 0;
        for (int i = strlen(_response) - 1; _response[i] == '\n' || _response[i] == '\r'; i--, flag++);
        if (flag == 4)//连续两个换行和回车表示已经到达响应头的头尾, 即将出现的就是需要下载的内容
            break;

        length += len;
    }
}

int HttpDown::Download(long & FileLength)
{
	long last_length = GetFileSize(_dir_file);

	FileLength = last_length;

	//cout<<"6: 开始正式下载..."<<endl;

    //开始正式下载
    _hasrecieve = last_length;//记录已经下载的长度
    int mem_size = 8192;//缓冲区大小8K
    int buf_len = mem_size;//理想状态每次读取8K大小的字节流
    int len;
    //创建文件描述符，准备写入文件
    int fd = open(_dir_file, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd < 0)
    {
        cout<<"文件创建失败!"<<endl;
        exit(0);
    }

    lseek(fd, last_length, SEEK_SET);

    struct timeval t_start, t_end;//记录一次读取的时间起点和终点
    long diff = 0;
    long prelen = 0;
    double speed = 0;
    int cnt;//计数，如果多次速度小于10 kb 就杀了线程连接重连

    while (_hasrecieve < _file_length)
    {
    	gettimeofday(&t_start, NULL ); //获取开始时间
      	//socket读取
        len = read(_client_socket, _buf, buf_len);
        //写入文件
        if(write(fd, _buf, len) == -1)
        {
        	perror("write fail：");
        	exit(0);
        }
        gettimeofday(&t_end, NULL ); //获取结束时间


        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us

        if (diff >= 1000000)//当一个时间段大于1s=1000000us时, 计算一次速度
        {
            speed = (double)(_hasrecieve - prelen) / (double)diff * (1000000.0 / 1024.0);
            if(speed < 10)
            {
            	cnt++;
            }
            else if(speed > 50)
            {
            	cnt++;
            }
            prelen = _hasrecieve;
            diff = 0;//清零时间段长度
        }

        if(cnt >= 5)
        {
        	return 1;
        }

        _hasrecieve += len;//更新已经下载的长度
        FileLength = _hasrecieve;
        
        if (_hasrecieve == _file_length)
            return 0;
    }
    
    if(_hasrecieve == _file_length)
    {
        return 0;
    }
    
}

int HttpDown::Connent(long range_start,long range_end)
{

	//cout<<"1: 正在获取远程服务器IP地址..."<<endl;
	ParseURL();//从url中分析出主机名, 端口号, 文件名
	GetIPAddr();//调用函数同访问DNS服务器获取远程主机的IP

	/*
	cout<<"\t>>>>下载地址解析成功<<<<"<<endl;
    cout<<"\t远程主机: "             <<_host<<endl;
    cout<<"\tIP 地 址: "             <<_ip_addr<<endl;
    cout<<"\t主机PORT: "             <<_port<<endl;
    cout<<"\t文件名 : "              <<_file_name<<endl;
    */
	//cout<<"2: 创建网络套接字..."<<endl;
    struct sockaddr_in addr;
	SetSocket(addr);

	//连接远程主机
    //cout<<"3: 正在连接远程主机..."<<endl;
    int res = connect(_client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1)
    {
        cout<<"连接远程主机失败, error: "<<res<<endl;
        exit(-1);
    }

    //cout<<"4: 正在发送http下载请求..."<<endl;
    long last_length = GetFileSize(_dir_file);
    GetHttpHead(range_start + last_length,range_end);//获取http头
    write(_client_socket, _header, strlen(_header));//write系统调用, 将请求header发送给服务器

    //每次单个字符读取响应头信息
    //cout<<"5: 正在解析http响应头..."<<endl;
    ReadSocket();//读取socket
    struct HTTP_RES_HEADER resp = ParseHeader();
    /*
    cout<<"http响应头解析成功!"<<endl;
    cout<<"HTTP响应代码: "<<resp.status_code<<endl;
    cout<<"HTTP文档类型: "<<resp.content_type<<endl;
    cout<<"HTTP主体长度: "<<resp.content_length<<"字节"<<endl;
    */
    _file_sum_length = resp.content_length;
    _file_length = range_end - range_start +1;

}

void HttpDown::CloseConnect(int code)
{
	//cout<<"7: 关闭套接字"<<endl;
	if(!code)
	{
		shutdown(_client_socket, 2);//关闭套接字的接收和发送
		return;
	}
    if (_file_length == GetFileSize(_dir_file))
        ;//cout<<"文件下载成功!"<<endl;
    else
    {
        remove(_dir_file);
        cout<<"\n文件下载中有字节缺失, 下载失败, 请重试!"<<endl;
    }
    shutdown(_client_socket, 2);//关闭套接字的接收和发送
}