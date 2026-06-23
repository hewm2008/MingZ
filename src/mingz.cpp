#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libdeflate.h>
#include "GzPipeline.h"

using namespace std;

#define OUTPUT_BUFFER_SIZE 2097152
int g_level = 9;
size_t g_block = OUTPUT_BUFFER_SIZE;


int  mingz_usage() 
{
	cout <<""
		"\tUsage: mingz  *.fq \n"
		"\tOptions:\n"
		"\t\t-i   <str>   input of text file or pipe\n"
		"\t\t-o   <str>   output of text file\n"
		"\n"
		"\t\t-t   <int>   number of threads [8]\n"
		"\t\t-l   <int>   compression level 1-12 [9]\n"
		"\t\t-b   <int>   block size in MB [2]\n"
		"\t\t-c           write on standard output\n"
		"\t\t-d           decompress(no threads)\n"
		"\t\t-h           show help hewm2008[v1.14]\n"
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


string & replace_all(string & str, const string & pattern, const string & replacement) 
{
	while (str.size() > 0 && str[0] == '-') str.erase(0, 1);
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
			if (n_thread<1)   n_thread=1;
			if (n_thread>256) n_thread=256;
		}
		else if (flag  == "l")
		{
			if(i + 1 == argc) {LogLackArg(flag) ; return 1;}
			i++;
			g_level=atoi(argv[i]);
			if (g_level<1)  g_level=1;
			if (g_level>12) g_level=12;
		}
		else if (flag  == "b")
		{
			if(i + 1 == argc) {LogLackArg(flag) ; return 1;}
			i++;
			int mb=atoi(argv[i]);
			if (mb<1) mb=1;
			g_block=(size_t)mb*1024*1024;
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



void RemoveFile(const std::string&  infile , const std::string&  outfile)
{
	struct stat st;
	if (stat(outfile.c_str(), &st) == 0)
	{
		struct timespec ts[2];
		ts[0] = st.st_atim;
		ts[1] = st.st_mtim;
		utimensat(AT_FDCWD, outfile.c_str(), ts, 0);
	}
	remove(infile.c_str());
}


struct MapHandle
{
	int fd = -1;
	void * addr = NULL;
	size_t size = 0;
	bool mapped = false;
	std::vector<uint8_t> buf;
};

// Provide read-only access to a whole input as a contiguous byte range.
// Regular files are mmap'd (zero-copy); pipes/FIFOs/other are read into memory.
// Returns true on success (size==0 is valid for an empty input).
bool MapFile(const std::string & path, MapHandle & mh)
{
	mh.fd = open(path.c_str(), O_RDONLY);
	if (mh.fd < 0) return false;
	struct stat st;
	if (fstat(mh.fd, &st) != 0) { close(mh.fd); mh.fd = -1; return false; }

	if (S_ISREG(st.st_mode) && st.st_size > 0)
	{
		mh.size = (size_t)st.st_size;
		mh.addr = mmap(NULL, mh.size, PROT_READ, MAP_PRIVATE, mh.fd, 0);
		if (mh.addr == MAP_FAILED) { close(mh.fd); mh.fd = -1; mh.addr = NULL; return false; }
		mh.mapped = true;
		madvise(mh.addr, mh.size, MADV_SEQUENTIAL);
		return true;
	}

	// non-regular (pipe/fifo) or empty: read everything into memory
	const size_t CHUNK = 1 << 20;
	mh.buf.reserve(CHUNK);
	while (true)
	{
		size_t old = mh.buf.size();
		if (mh.buf.capacity() < old + CHUNK)
		{
			size_t newCap = mh.buf.capacity() * 2;
			if (newCap < old + CHUNK) newCap = old + CHUNK;
			mh.buf.reserve(newCap);
		}
		mh.buf.resize(old + CHUNK);
		ssize_t got = read(mh.fd, mh.buf.data() + old, CHUNK);
		if (got < 0) { close(mh.fd); mh.fd = -1; return false; }
		mh.buf.resize(old + (size_t)got);
		if (got == 0) break;
	}
	close(mh.fd); mh.fd = -1;
	mh.addr = mh.buf.empty() ? NULL : (void *) mh.buf.data();
	mh.size = mh.buf.size();
	mh.mapped = false;
	return true;
}

void UnMapFile(MapHandle & mh)
{
	if (mh.mapped && mh.addr != NULL && mh.size > 0) munmap(mh.addr, mh.size);
	if (mh.fd >= 0) close(mh.fd);
	mh.addr = NULL; mh.fd = -1; mh.size = 0; mh.mapped = false;
	mh.buf.clear(); mh.buf.shrink_to_fit();
}


int UnGzipMain(Para_A24 * P2In,string input_file, string & output_file )
{
	MapHandle mh;
	if (!MapFile(input_file, mh))
	{
		cerr << "open IN File error: "<<input_file<<endl;
		exit(1);
	}

	std::ostream * OUT = &std::cout;
	std::ofstream fout;

	if (!(P2In->std))
	{
		if (input_file == "/dev/stdin")
		{
			P2In->std = true;
		}
		else
		{
			if (output_file.size() > 3 && output_file.substr(output_file.size()-3) == ".gz")
				output_file.resize(output_file.size()-3);

			if ( input_file == output_file)
			{
				cerr<<"OutPut File can't be the same with InPut File:\t"<<input_file<<endl;
				UnMapFile(mh);
				return  -1;
			}

			fout.open(output_file.c_str(), std::ios::out | std::ios::binary);

			if(!fout.good())
			{
				cerr << "open OUT File error: "<<output_file<<endl;
				UnMapFile(mh);
				exit(1);
			}
			OUT = &fout;
		}
	}

	const uint8_t * in = (const uint8_t *) mh.addr;
	size_t inSize = mh.size;
	size_t pos = 0;

	libdeflate_decompressor * dec = libdeflate_alloc_decompressor();
	if (dec == NULL)
	{
		cerr << "Error: libdeflate_alloc_decompressor failed"<<endl;
		UnMapFile(mh);
		exit(1);
	}

	size_t outCap = 64 * 1024 * 1024;
	std::vector<uint8_t> outBuf(outCap);

	bool fail = false;
	while (pos < inSize)
	{
		size_t inUsed = 0, outUsed = 0;
		enum libdeflate_result r = libdeflate_gzip_decompress_ex(
			dec, in + pos, inSize - pos,
			outBuf.data(), outBuf.size(),
			&inUsed, &outUsed);

		if (r == LIBDEFLATE_SUCCESS)
		{
			if (outUsed > 0) OUT->write((const char *) outBuf.data(), (std::streamsize) outUsed);
			if (inUsed == 0) break; // safety: no progress
			pos += inUsed;
		}
		else if (r == LIBDEFLATE_INSUFFICIENT_SPACE)
		{
			outCap *= 2;
			outBuf.resize(outCap);
		}
		else // LIBDEFLATE_BAD_DATA / LIBDEFLATE_SHORT_OUTPUT
		{
			// tolerate trailing padding of NUL bytes (as gunzip does)
			bool onlyZeros = true;
			for (size_t k = pos; k < inSize; k++)
			{
				if (in[k] != 0) { onlyZeros = false; break; }
			}
			if (!onlyZeros)
			{
				cerr << "Error: not a valid gzip stream: "<<input_file<<endl;
				fail = true;
			}
			break;
		}
	}

	libdeflate_free_decompressor(dec);
	OUT->flush();
	UnMapFile(mh);

	if (!(P2In->std))
	{
		fout.close();
		if (!fail) RemoveFile(input_file,output_file);
	}

	return fail ? -1 : 0;
}



int GzipFile(Para_A24 * P2In,string &InFilePath ,string & OutFilePath) 
{
	MapHandle mh;
	if (!MapFile(InFilePath, mh))
	{
		cerr << "Error: Can't open input file: " << InFilePath << endl;
		return  -1;
	}

	if (mh.size >= 2 && ((const uint8_t*)mh.addr)[0] == 31 && ((const uint8_t*)mh.addr)[1] == 139)
	{
		cerr << "Error: The input file is already in the gzip format : " << InFilePath << endl;
		UnMapFile(mh);
		return  -1;
	}

	//open out file


	OutFilePath=add_Asuffix(OutFilePath);

	if ( InFilePath  == OutFilePath)
	{
		cerr<<"OutPut File can't be the same with InPut File"<<InFilePath<<endl;
		UnMapFile(mh);
		return  -1;
	}

	ofstream OUTH(OutFilePath, std::ios::out | std::ios::binary);

	GzPipe pipe((const uint8_t*) mh.addr, mh.size, OUTH, n_thread, g_block, g_level);
	bool ok = pipe.run();

	OUTH.close();
	UnMapFile(mh);

	if (!ok)
	{
		cerr << "Error: compression failed: " << InFilePath << endl;
		return -1;
	}

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

	OutFilePath=add_Asuffix(OutFilePath);

	ofstream OUTH(OutFilePath, std::ios::out | std::ios::binary);

	GzPipe pipe(std::cin, OUTH, n_thread, g_block, g_level);
	bool ok = pipe.run();

	OUTH.close();

	if (!ok) { cerr << "Error: compression failed (stdin)" << endl; return -1; }

	return 0;
}



int GzipFileOutPipe(Para_A24 * P2In,string &InFilePath) 
{

	MapHandle mh;
	if (!MapFile(InFilePath, mh))
	{
		cerr << "Error: Can't open input file: " << InFilePath << endl;
		return  -1;
	}

	if (mh.size >= 2 && ((const uint8_t*)mh.addr)[0] == 31 && ((const uint8_t*)mh.addr)[1] == 139)
	{
		cerr << "Error: The input file is already in the gzip format " << InFilePath << endl;
		UnMapFile(mh);
		return  -1;
	}

	//open out file



	GzPipe pipe((const uint8_t*) mh.addr, mh.size, std::cout, n_thread, g_block, g_level);
	bool ok = pipe.run();

	std::cout.flush();
	UnMapFile(mh);

	if (!ok)
	{
		cerr << "Error: compression failed: " << InFilePath << endl;
		return -1;
	}

	return 0;
}




int GzipFileInOutPipe(Para_A24 * P2In) 
{
	GzPipe pipe(std::cin, std::cout, n_thread, g_block, g_level);
	bool ok = pipe.run();

	std::cout.flush();

	if (!ok) { cerr << "Error: compression failed (stdin)" << endl; return -1; }

	return 0;
}





//////////////////main///////////////////
int main (int argc, char *argv[ ])
{
	std::ios::sync_with_stdio(false);
	std::cin.tie(0);

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

