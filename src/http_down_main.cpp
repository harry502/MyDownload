// http下载工具
// 作者：龚一帆
// 创建日期：2017年11月7日

#include <iostream>
#include "http_down.h"
using namespace std;

struct thpread_pack
{
	int t_num;
	long range_start;
	long range_end;
	char * FileDir;
};
long gSumLength;
long gFileLength[20];
char gURL[2048];
const int gMaxPthreadNum = 20;
int gpthreadnum=5;//初始化线程数量
char gPart_Name[gMaxPthreadNum][256];//各个部分文件的名称
char gdir_name[256] = "";
char gConfDir[256];

void * Download(void * pack)
{
	int tnum = ((thpread_pack *)pack)->t_num;
	long rangestart = ((thpread_pack *)pack)->range_start;
	long rangeend = ((thpread_pack *)pack)->range_end;
	char filedir[256];
	strcpy(filedir,((thpread_pack *)pack)->FileDir);

	//cout<<tnum<<' '<<1<<endl;
	HttpDown * http = new HttpDown(gURL,filedir);
	http -> Connent(rangestart,rangeend);
	int res = http -> Download(gFileLength[tnum]);
	//cout<<res<<endl;
	while(res)
	{
		//重连接
		http -> CloseConnect(0);
		delete http;
		http = new HttpDown(gURL,filedir);
		http -> Connent(rangestart,rangeend);
		//cout<<rangestart<<' '<<rangeend<<endl;
		res = http -> Download(gFileLength[tnum]);

		//cout<<tnum<<' '<<endl;
		//cout<<res<<endl;
	}
	http -> CloseConnect(1);

	delete http;
}

void ProgressBar(long cur_size, long total_size, double speed)
{
    /*用于显示下载进度条*/
    float percent = (float) cur_size / total_size;
    const int numTotal = 50;
    int numShow = (int)(numTotal * percent);

    if (numShow == 0)
        numShow = 1;

    if (numShow > numTotal)
        numShow = numTotal;

    char sign[51] = {0};
    memset(sign, '=', numTotal);

    printf("\r%.2f%%[%-*.*s] %.2f/%.2fMB %4.0fkb/s", percent * 100, numTotal, numShow, sign, cur_size / 1024.0 / 1024.0, total_size / 1024.0 / 1024.0, speed);
    fflush(stdout);

    if (numShow == numTotal)
        printf("\n");
}

long GetFileSize(const char * gdir_name)
{
	//通过系统调用直接得到文件的大小
    struct stat buf;
    if (stat(gdir_name, &buf) < 0)
        return 0;
    return (unsigned long) buf.st_size;
}

void CombinePart()
{
	int mem_size = 8192;//缓冲区大小8K
	char buff[mem_size];
	//打开第一个文件
    int fd = open(gPart_Name[0], O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd < 0)
    {
        cout<<gPart_Name[0]<<" 文件打开失败!"<<endl;
        exit(0);
    }

    lseek(fd, 0, SEEK_END);

    //开始写文件
	for(int i=1;i<gpthreadnum;i++)
	{
		int nextfd = open(gPart_Name[i], O_RDONLY, S_IRWXG | S_IRWXO | S_IRWXU);
	    if (fd < 0)
	    {
	        cout<<gPart_Name[i]<<" 文件打开失败!"<<endl;
	        exit(0);
	    }

	    //记录已经写完的数据
	    long hasread = 0;
	    long file_size = GetFileSize(gPart_Name[i]);
	    long len = 0;

	    while(hasread < file_size)
	    {
	    	//写8k数据进buff
	    	len = read(nextfd,buff,mem_size);
	    	//写8k数据进第一个文件
	    	if(write(fd, buff, len) == -1)
	        {
	        	perror("write fail：");
	        	exit(0);
	        }
	    	hasread += len;
	    }

	    if(hasread == file_size)
	    {
	    	remove(gPart_Name[i]);
	    }
	    else
	    {
	    	cout<<"文件合并失败"<<endl;
	    }
	}


	//文件改名
	rename(gPart_Name[0],gdir_name);
}

void SetConf(const char * LastModified)
{
	strcpy(gConfDir,gdir_name);
	strcat(gConfDir,".downconf");
	int fd;

	//如果文件存在，就检查数据是否正确
	if( access(gConfDir, F_OK) != -1 )
	{
		char temp[30];
		long templen;
		int t_num;

	    ifstream in(gConfDir);

		in>>templen;
		in.getline(temp,1);
		in.getline(temp,30);
		in>>t_num;

		//如果数据和从服务器获取的不一致
		if(templen != gSumLength || (strcmp(temp,LastModified)!=0))
		{
			cout<<"服务器数据变动，重新下载该数据..."<<endl;
			remove(gConfDir);

			for(int i=0;i<t_num;i++)
			{
				strcpy(gPart_Name[i],gdir_name);
				strcat(gPart_Name[i],"_part_");
				char str[10];
				sprintf(str,"%d",i);
				strcat(gPart_Name[i],str);
				remove(gPart_Name[i]);
			}
			//把配置文件删完之后再跑一次这个函数
			SetConf(LastModified);
			return;
		}
		if(t_num != gpthreadnum)
		{
			cout<<"该链接在进行"<<t_num<<"线程下载,自动识别并继续按该线程数下载"<<endl;
			gpthreadnum = t_num;
		}
	}
	//不存在则创建
	else
	{
		ofstream out(gConfDir);
		out<<gSumLength<<endl;
		out<<LastModified<<endl;
		out<<gpthreadnum<<endl;
	}
}

int main(int argc, char *argv[])
{

    pthread_t tids[gMaxPthreadNum];//存储各个线程的tid
    struct thpread_pack pack[gMaxPthreadNum];

    int ch;

    while((ch = getopt(argc, argv, "t:u:d:bh")) != EOF)
    {
    	switch(ch)
    	{
    		case 'u':
    		{
    			strcpy(gURL,optarg);
    			break;
    		}
    		case 'd':
    		{
    			cout<<"您已经将下载文件名指定为: "<<optarg<<endl;
    			strcpy(gdir_name,optarg);
    			break;
    		}
    		case 'b':
    		{
    			daemon(1,0);
    			break;
    		}
    		case 'h':
    		{
    			cout<<"\t\t使用方法："<<endl;
    			cout<<"\t-u 设置目标URL"<<endl;
    			cout<<"\t-d 设置保存文件的具体地址"<<endl;
    			cout<<"\t-b 后台下载"<<endl;
    			cout<<"\t-h 查看参数详情"<<endl;
    			cout<<"\t-t 设置线程数量，最多不能超过20"<<endl;
    			cout<<"示例： ./http_down_main -u";
    			cout<<"https://nodejs.org/dist/v4.2.3/node-v4.2.3-linux-x64.tar.gz ";
    			cout<<"-d ../download/node.tar.gz -t 10"<<endl;

    			exit(0);
    			break;
    		}
    		case 't':
    		{
    			gpthreadnum = atoi(optarg);
    			if(gpthreadnum > 20)
    			{
    				cout<<"线程数不能超过20！"<<endl;
    				exit(0);
    			}
    		}
    	}
    }

    /*第一次http连接，获取文件相关属性*/
    char LastModified[30];
	HttpDown * first =new HttpDown(gURL,gdir_name);
	int code = first -> Connent(0,0);
	gSumLength = first -> GetFileSumLength();
	strcpy(LastModified,first->GetLastModified());
	first -> CloseConnect(0);

	/* 配置文件相关操作 */
	SetConf(LastModified);

	if(code == 200 && gpthreadnum != 1)
	{
		cout<<"该服务器不支持多线程下载，自动转换为单线程下载..."<<endl;
		gpthreadnum = 1;
	}

	if( access(gdir_name, F_OK) != -1 && gSumLength == GetFileSize(gdir_name))
	{
		cout<<"文件已存在！"<<endl;
		exit(0);
	}

	if(strcmp(gdir_name,"")==0)
	{
		strcpy(gdir_name,first -> GetFileName());
	}

    //开始多线程下载
    cout<<"开始多线程下载，请您稍等..."<<endl;
    for(int i=0;i<gpthreadnum;i++)
    {
    	//设置分段的文件名
    	if(gpthreadnum != 1)
    	{
    		strcpy(gPart_Name[i],gdir_name);
			strcat(gPart_Name[i],"_part_");
			char str[10];
			sprintf(str,"%d",i);
			strcat(gPart_Name[i],str);
    	}
    	else
    	{
    		strcpy(gPart_Name[i],gdir_name);
    	}

		//配置线程包
		pack[i].t_num = i;
		pack[i].range_start = i * (gSumLength/(double)gpthreadnum) ;
		if( i != gpthreadnum - 1)
			pack[i].range_end = (i+1) * (gSumLength/(double)gpthreadnum) - 1;
		else
			pack[i].range_end = gSumLength - 1;

		pack[i].FileDir = gPart_Name[i];

    	pthread_create(&tids[i], NULL, Download, (void*)&pack[i]);
    }

    //计算进度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    long hasrecsum = 0;
    long lastrecsum = 0;
    long diff = 0;
    double speed = 0;

    for(int i=0;i<gpthreadnum;i++)
    		hasrecsum += gFileLength[i];
    ProgressBar(hasrecsum, gSumLength, speed);

    while(hasrecsum < gSumLength )
    {
    	gettimeofday(&t_start, NULL ); //获取开始时间

    	hasrecsum = 0;
    	for(int i=0;i<gpthreadnum;i++)
    		hasrecsum += gFileLength[i];

    	gettimeofday(&t_end, NULL ); //获取结束时间
    	if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us

        if (diff >= 500000)//当一个时间段大于1s=1000000us时, 计算一次速度
        {
            speed = (double)(hasrecsum - lastrecsum) / (double)diff * (500000.0 / 1024.0);
            lastrecsum = hasrecsum;//清零下载量
            diff = 0;//清零时间段长度
            ProgressBar(hasrecsum, gSumLength, speed);
            //cout<<gFileLength[0]<<' '<<gFileLength[1]<<endl;
        }
    }
    ProgressBar(hasrecsum, gSumLength, speed);


    //确认线程全部关闭
    for(int i=0;i<gpthreadnum;i++)
    {
    	pthread_join(tids[i], NULL);
    }

    //开始文件合并
    cout<<"开始文件合并，请您稍等..."<<endl;
    CombinePart();
    cout<<"下载完成！"<<endl;
    remove(gConfDir);

    return 0;
}
//15548902