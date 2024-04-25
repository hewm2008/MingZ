#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <regex>
#include <cmath>
#include <thread>
#include <algorithm>
#include <unistd.h>
#include "gzstream.C"
#include "DeflateOgzstream.cpp"

using namespace std;


int  mingz_usage() 
{
	cout <<""
		"\tUsage: mingz  *.fq \n"
		"\tOptions:\n"
		"\t\t-i   <str>   input of text file or pipe\n"
		"\t\t-o   <str>   output of text file\n"
		"\n"
		"\t\t-t   <int>   number of threads [8]\n"
		"\t\t-c           write on standard output\n"
		"\t\t-d           decompress(no threads)\n"
		"\t\t-h           show help hewm2008[v1.12]\n"
		"\n";
	return 1;
}

inline string add_Asuffix (string path) 
{
	string ext =path.substr(path.rfind('.') ==string::npos ? path.length() : path.rfind('.') + 1);
	if (ext != "gz") 
	{
		path=path+".gz" ;
	}
	return path ;
}

int n_thread=8;

class Para_A24 
{
	public:
		vector <string> InPutList ;
		vector <string> OutPutList ;
		int Infile_count ;
		int Outfile_count ;
		bool compress;
		bool std;
		Para_A24()
		{
			Infile_count=0;
			Outfile_count=0;
			compress=true;
			std=false;
		}
};

inline void  LogLackArg(string flag) 
{
	cerr << "Error: Lack Argument for [ -"<<flag<<" ]"<<endl;
}


bool  GzipTrue( string filepath )
{
	FILE   *infile;
	unsigned char buf[3] ={'\0'};
	infile = fopen(filepath.c_str(), "rb");
	int rc = fread(buf,sizeof(unsigned char), 2,infile);
	fclose(infile);
	if  (buf[0] == 31 &&  buf[1] == 139)
	{
		return true ;
	}
	else
	{
		return false ;
	}
}



string & replace_all(string & str, const string & pattern, const string & replacement) 
{
	while(true) 
	{
		string::size_type  pos(0);
		if((pos=str.find(pattern))!=string::npos)
		{
			str.replace(pos,pattern.length(),replacement);
		}
		else
		{
			break;
		}
	}
	return str;
}

int mingz_cmd(int argc, char **argv, Para_A24 * P2In)
{
	// if (argc <= 1) {mingz_usage(); return 1;}

	for(int i = 1; i < argc; i++)
	{
		if(argv[i][0] != '-')
		{
			string A=argv[i];
			if ( access(argv[i], 0) != 0 )
			{
				cerr << "Error: Can't find input File \t"<<A<< endl;
				return 1;
			}
			(P2In->Infile_count)++;
			(P2In->InPutList).push_back(A);
			continue ;
		}

		string flag=argv[i] ;
		flag=replace_all(flag,"-","");

		//input and output options
		if (flag == "i" ) 
		{
			if(i + 1 == argc) {LogLackArg(flag); return 0;}
			i++;
			string A=argv[i];
			if ( access(argv[i], 0) != 0 )
			{
				cerr << "Error: Can't find input File \t"<<A<< endl;
				return 1;
			}
			(P2In->Infile_count)++;
			(P2In->InPutList).push_back(A);
		}
		else if (flag == "o" ) 
		{
			if(i + 1 == argc) {LogLackArg(flag); return 1;}
			i++;
			string A=argv[i];
			(P2In->Outfile_count)++;
			(P2In->OutPutList).push_back(A);
		}
		//Other options
		else if (flag  ==  "t") 
		{
			if(i + 1 == argc) {LogLackArg(flag) ; return 1;}
			i++;
			n_thread=atoi(argv[i]);
		}
		else if (flag  == "stdout" || flag  == "c") 
		{
			P2In->std=true;
		}
		else if (flag  == "decompress" || flag  == "d") 
		{
			P2In->compress=false;
		}
		else if (flag  == "dc" || flag  == "cd") 
		{
			P2In->compress=false;
			P2In->std=true;
		}

		else if (flag  == "help" || flag  == "h") 
		{
			mingz_usage(); return 1;
		}
		else 
		{
			cerr << "Error: UnKnow argument -"<<flag<<endl;
			return 1;
		}
	}

	if ( (P2In->Infile_count) <1   &&  (P2In->Outfile_count)<1)
	{
		P2In->std=true;
	}
	for (int i=0 ; i<(P2In->Infile_count) ;i++)
	{
		if (i>=(P2In->Outfile_count))
		{
			(P2In->OutPutList).push_back(P2In->InPutList[i]);
		}
	}
	return  0;
}



void ComPreThread(bool & pass_, char * InData ,uint8_t * ComPresseData,  size_t &  inputSize , size_t & CompressedSize  )
{

	if (inputSize <1)
	{
		pass_ = false;
	}
	else 
	{
		DeflateCompress  GZData ;
		pass_=GZData.compressData(InData, inputSize, ComPresseData , CompressedSize);		
	}
}


void RemoveFile(const std::string&  infile , const std::string&  outfile)
{
	std::string command = "touch -r " + infile + " "+ outfile;
	int result = std::system(command.c_str());
	if (result != 0)
	{
		std::cerr << "Error: touch no done ,check : "<<outfile<< std::endl;
	}
	else 
	{
		remove(infile.c_str());
	}
}


int UnGzipMain(Para_A24 * P2In,string input_file, string & output_file )
{

	igzstream IN (input_file.c_str(),ifstream::in); // ifstream  + gz
	if(!IN.good())
	{
		cerr << "open IN File error: "<<input_file<<endl;
		exit(1);
	}


	std::ostream * OUT = &std::cout; // 初始化 Info 指针指向 std::cout
	std::ofstream fout;


	if (!(P2In->std))
	{
		output_file=add_Asuffix(output_file);
		output_file=output_file.substr(0,output_file.length()-3);

		if ( input_file == output_file)
		{
			cerr<<"OutPut File can't be the same with InPut File:\t"<<input_file<<endl;
			return  -1;
		}

		fout.open(output_file.c_str());

		if(!fout.good())
		{
			cerr << "open OUT File error: "<<output_file<<endl;
			exit(1);
		}
		OUT = &fout;
	}

	std::ios::sync_with_stdio(false);
	std::cin.tie(0);


	IN.rdbuf()->pubsetbuf(nullptr, OUTPUT_BUFFER_SIZE);
	(*OUT).rdbuf()->pubsetbuf(nullptr,OUTPUT_BUFFER_SIZE);





	string line;
	while(!IN.eof())
	{
		getline(IN,line);
		if (line.length()<=0)  { continue  ; }
		*OUT<<line<<"\n";
	}

	IN.close();

	if (!(P2In->std))
	{
		fout.close(); 
		//OUT.close();		remove(input_file.c_str());
		RemoveFile(input_file,output_file);
	}

	return 0;
}


int GzipFile(Para_A24 * P2In,string &InFilePath ,string & OutFilePath) 
{

	std::ios::sync_with_stdio(false);
	std::cin.tie(0);

	if (GzipTrue(InFilePath))
	{
		cerr << "Error: The input file is already in the gzip format : " << InFilePath << endl;
		return  -1;
	}

	ifstream inFile(InFilePath.c_str(), std::ios::binary);

	if (inFile.fail()) 
	{
		cerr << "Error: Can't open input file: " << InFilePath << endl;
		return  -1;
	}

	//open out file


	OutFilePath=add_Asuffix(OutFilePath);

	if ( InFilePath  == OutFilePath)
	{
		cerr<<"OutPut File can't be the same with InPut File"<<InFilePath<<endl;
		return  -1;
	}

	//DeflateOgzstream OUTH(OutFilePath);

	ofstream OUTH(OutFilePath, std::ios::out | std::ios::binary);
	vector<thread> threads;




	bool *PASS =new bool [n_thread];
	uint8_t ** ComPresseData = new uint8_t*[n_thread];
	char ** InData = new char* [n_thread];
	size_t * CompressedSize =new size_t [n_thread];
	size_t * InSize =new size_t [n_thread];
	for (int i = 0; i < n_thread; i++)
	{
		ComPresseData[i] = new uint8_t[OUTPUT_BUFFER_SIZE+2];
		InData[i] = new char[OUTPUT_BUFFER_SIZE+2];
		CompressedSize[i]=OUTPUT_BUFFER_SIZE;
		InSize[i]=OUTPUT_BUFFER_SIZE;
	}

	int Flag=0;

	while(!inFile.eof()) 
	{
		inFile.read(InData[Flag], OUTPUT_BUFFER_SIZE);
		InSize[Flag]=inFile.gcount();
		threads.push_back(std::thread(ComPreThread,ref(PASS[Flag]),ref(InData[Flag]),ref(ComPresseData[Flag]),ref(InSize[Flag]),ref(CompressedSize[Flag])));

		Flag++;

		if (Flag==n_thread)
		{
			for (auto& thread : threads) 
			{
				thread.join();
			}
			threads.clear();

			for (int i = 0; i < n_thread; i++)
			{
				if (PASS[i])
				{
					OUTH.write((const char*) ComPresseData[i] , CompressedSize[i] );
				}
			}
			Flag=0;
		}
	}


	if (Flag!=0) 
	{
		for (auto& thread : threads) 
		{
			thread.join();
		}
		threads.clear();
		for (int i = 0; i < Flag; i++)
		{
			if (PASS[i])
			{
				OUTH.write((const char*)ComPresseData[i] , CompressedSize[i] );
			}
		}
		Flag=0;
	}


	OUTH.close();
	for (int i = 0; i < n_thread; i++) 
	{
		delete[]  ComPresseData[i] ;
		delete[]  InData[i] ;
	}

	delete [] CompressedSize ;
	delete [] ComPresseData ;
	delete [] InData ;
	delete [] PASS;

	inFile.close();

	string output_file=OutFilePath.substr(0,OutFilePath.length()-3);
	if(output_file==InFilePath)
	{
		//remove(InFilePath.c_str());
		RemoveFile(InFilePath,OutFilePath);
	}
	return 0;
}





int GzipFileInPipeOutFile(Para_A24 * P2In,string & OutFilePath) 
{

	std::ios::sync_with_stdio(false);
	std::cin.tie(0);

	OutFilePath=add_Asuffix(OutFilePath);

	ofstream OUTH(OutFilePath, std::ios::out | std::ios::binary);
	vector<thread> threads;


	bool *PASS =new bool [n_thread];
	uint8_t ** ComPresseData = new uint8_t*[n_thread];
	char ** InData = new char* [n_thread];
	size_t * CompressedSize =new size_t [n_thread];
	size_t * InSize =new size_t [n_thread];
	for (int i = 0; i < n_thread; i++)
	{
		ComPresseData[i] = new uint8_t[OUTPUT_BUFFER_SIZE+2];
		InData[i] = new char[OUTPUT_BUFFER_SIZE+2];
		CompressedSize[i]=OUTPUT_BUFFER_SIZE;
		InSize[i]=OUTPUT_BUFFER_SIZE;
	}

	int Flag=0;
	while (std::cin.peek() != EOF) 
	{
		std::cin.read(InData[Flag], OUTPUT_BUFFER_SIZE);
		InSize[Flag]=std::cin.gcount();
		threads.push_back(std::thread(ComPreThread,ref(PASS[Flag]),ref(InData[Flag]),ref(ComPresseData[Flag]),ref(InSize[Flag]),ref(CompressedSize[Flag])));
		Flag++;
		if (Flag==n_thread)
		{
			for (auto& thread : threads) 
			{
				thread.join();
			}
			threads.clear();

			for (int i = 0; i < n_thread; i++)
			{
				if (PASS[i])
				{
					OUTH.write((const char*) ComPresseData[i] , CompressedSize[i] );
				}
			}
			Flag=0;
		}
	}


	if (Flag!=0) 
	{
		for (auto& thread : threads) 
		{
			thread.join();
		}
		threads.clear();
		for (int i = 0; i < Flag; i++)
		{
			if (PASS[i])
			{
				OUTH.write((const char*)ComPresseData[i] , CompressedSize[i] );
			}
		}
		Flag=0;
	}


	OUTH.close();
	for (int i = 0; i < n_thread; i++) 
	{
		delete[]  ComPresseData[i] ;
		delete[]  InData[i] ;
	}

	delete [] CompressedSize ;
	delete [] ComPresseData ;
	delete [] InData ;
	delete [] PASS;

	return 0;
}



int GzipFileOutPipe(Para_A24 * P2In,string &InFilePath) 
{

	std::ios::sync_with_stdio(false);
	std::cin.tie(0);

	if (GzipTrue(InFilePath))
	{
		cerr << "Error: The input file is already in the gzip format " << InFilePath << endl;
		return  -1;
	}

	ifstream inFile(InFilePath.c_str(), std::ios::binary);

	if (inFile.fail()) 
	{
		cerr << "Error: Can't open input file: " << InFilePath << endl;
		return  -1;
	}

	//open out file



	vector<thread> threads;




	bool *PASS =new bool [n_thread];
	uint8_t ** ComPresseData = new uint8_t*[n_thread];
	char ** InData = new char* [n_thread];
	size_t * CompressedSize =new size_t [n_thread];
	size_t * InSize =new size_t [n_thread];
	for (int i = 0; i < n_thread; i++)
	{
		ComPresseData[i] = new uint8_t[OUTPUT_BUFFER_SIZE+2];
		InData[i] = new char[OUTPUT_BUFFER_SIZE+2];
		CompressedSize[i]=OUTPUT_BUFFER_SIZE;
		InSize[i]=OUTPUT_BUFFER_SIZE;
	}

	int Flag=0;

	while(!inFile.eof()) 
	{
		inFile.read(InData[Flag], OUTPUT_BUFFER_SIZE);
		InSize[Flag]=inFile.gcount();
		threads.push_back(std::thread(ComPreThread,ref(PASS[Flag]),ref(InData[Flag]),ref(ComPresseData[Flag]),ref(InSize[Flag]),ref(CompressedSize[Flag])));

		Flag++;

		if (Flag==n_thread)
		{
			for (auto& thread : threads) 
			{
				thread.join();
			}
			threads.clear();

			for (int i = 0; i < n_thread; i++)
			{
				if (PASS[i])
				{
					std::cout.write((const char*)ComPresseData[i] , CompressedSize[i] );
				}
			}
			Flag=0;
		}
	}


	if (Flag!=0) 
	{
		for (auto& thread : threads) 
		{
			thread.join();
		}
		threads.clear();
		for (int i = 0; i < Flag; i++)
		{
			if (PASS[i])
			{
				std::cout.write((const char*) ComPresseData[i] , CompressedSize[i] );
			}
		}
		Flag=0;
	}


	std::cout.flush();


	for (int i = 0; i < n_thread; i++) 
	{
		delete[]  ComPresseData[i] ;
		delete[]  InData[i] ;
	}

	delete [] CompressedSize ;
	delete [] ComPresseData ;
	delete [] InData ;
	delete [] PASS;
	inFile.close();

	return 0;
}




int GzipFileInOutPipe(Para_A24 * P2In) 
{

	vector<thread> threads;

	bool *PASS =new bool [n_thread];
	uint8_t ** ComPresseData = new uint8_t*[n_thread];
	char ** InData = new char* [n_thread];
	size_t * CompressedSize =new size_t [n_thread];
	size_t * InSize =new size_t [n_thread];
	for (int i = 0; i < n_thread; i++)
	{
		ComPresseData[i] = new uint8_t[OUTPUT_BUFFER_SIZE+2];
		InData[i] = new char[OUTPUT_BUFFER_SIZE+2];
		CompressedSize[i]=OUTPUT_BUFFER_SIZE;
		InSize[i]=OUTPUT_BUFFER_SIZE;
	}

	int Flag=0;
	while (std::cin.peek() != EOF) 
	{
		std::cin.read(InData[Flag], OUTPUT_BUFFER_SIZE);
		InSize[Flag]=std::cin.gcount();
		threads.push_back(std::thread(ComPreThread,ref(PASS[Flag]),ref(InData[Flag]),ref(ComPresseData[Flag]),ref(InSize[Flag]),ref(CompressedSize[Flag])));
		Flag++;
		if (Flag==n_thread)
		{
			for (auto& thread : threads) 
			{
				thread.join();
			}
			threads.clear();

			for (int i = 0; i < n_thread; i++)
			{
				if (PASS[i])
				{
					std::cout.write((const char*)ComPresseData[i] , CompressedSize[i] );
				}
			}
			Flag=0;
		}
	}


	if (Flag!=0) 
	{
		for (auto& thread : threads) 
		{
			thread.join();
		}
		threads.clear();
		for (int i = 0; i < Flag; i++)
		{
			if (PASS[i])
			{
				std::cout.write((const char*) ComPresseData[i] , CompressedSize[i] );
			}
		}
		Flag=0;
	}

	std::cout.flush();


	for (int i = 0; i < n_thread; i++) 
	{
		delete[]  ComPresseData[i] ;
		delete[]  InData[i] ;
	}

	delete [] CompressedSize ;
	delete [] ComPresseData ;
	delete [] InData ;
	delete [] PASS;


	return 0;
}





//////////////////main///////////////////
int main (int argc, char *argv[ ])
{
	Para_A24 * P2In = new Para_A24;
	int Inflag=0;
	Inflag=mingz_cmd(argc, argv, P2In);
	if(Inflag==1) 
	{
		delete P2In ;
		return 1 ;
	}

	if (P2In->compress)
	{
		if  ((P2In->std) )
		{
			if (P2In->Infile_count>0)
			{
				if  ( P2In->Infile_count>1 )
				{
					cerr<<"Warning: Muti InFile but one standard output"<<endl;
				}

				for (int i=0 ; i<(P2In->Infile_count) ;i++)
				{
					GzipFileOutPipe(P2In,(P2In->InPutList)[i]);
				}
			}
			else
			{
				if (!isatty(STDIN_FILENO)) 
				{
					GzipFileInOutPipe(P2In);
				}
				else
				{
					mingz_usage();
					//cerr<<"\t\tError: No any input [ PiPe ] or  [Files] "<<endl;
					delete P2In ;
					return 1 ;
				}
			}
		}
		else
		{
			if (P2In->Infile_count>0)
			{
				for (int i=0 ; i<(P2In->Infile_count) ;i++)
				{
					GzipFile(P2In,(P2In->InPutList)[i],(P2In->OutPutList)[i]);
				}
			}
			else
			{
				if (!isatty(STDIN_FILENO)) 
				{
					GzipFileInPipeOutFile(P2In,(P2In->OutPutList)[0]);
				}
				else
				{
					mingz_usage();
					//cerr<<"\t\tError: No any input [ PiPe ] or  [Files] "<<endl;
					delete P2In ;
					return 1 ;
				}
			}
		}
	}
	else
	{
		for (int i=0 ; i<(P2In->Infile_count) ;i++)
		{
			UnGzipMain(P2In,(P2In->InPutList)[i],(P2In->OutPutList)[i]);
		}
	}
	delete P2In ;
	return 0;
}

