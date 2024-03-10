#include <iostream>
#include <fstream>
#include <libdeflate.h>
#include <cstring>
#include <string>

#define OUTPUT_BUFFER_SIZE  1048576

using namespace std;

/*
class DeflateOgzstream {
	public:
		DeflateOgzstream(const std::string& filename) : m_outputFile(filename, std::ios::out | std::ios::binary) {
			m_compressor = libdeflate_alloc_compressor(6);
		}

		~DeflateOgzstream() {
			libdeflate_free_compressor(m_compressor);
			m_outputFile.close();
		}

		bool compressDataWrite(const void* input, size_t inputSize) {
			if (!m_compressor || !input || inputSize == 0) {
				return false;
			}

			size_t compressedSize = libdeflate_gzip_compress(m_compressor, input, inputSize, m_outputBuffer, OUTPUT_BUFFER_SIZE);
			if (compressedSize == 0) {
				return false;
			}

			m_outputFile.write((const char*)m_outputBuffer, compressedSize);

			return true;
		}

		bool compressData(const void* input, size_t inputSize, uint8_t * m_outputBuffer, size_t & compressedSize ) {
			if (!m_compressor || !input || inputSize == 0) {
				return false;
			}
			compressedSize = libdeflate_gzip_compress(m_compressor, input, inputSize, m_outputBuffer, OUTPUT_BUFFER_SIZE);
			if (compressedSize == 0) {
				return false;
			}
			else {
				return true;
			}
		}

		void writeGZIO ( uint8_t * m_outputBuffer, size_t & compressedSize ) {
			m_outputFile.write((const char*)m_outputBuffer, compressedSize);
		}

		void writeGZIO (string  outInfo) {
			int Site=outInfo.length();
			int Start=0;
			while(Site>-1) {
				if (Site>OUTPUT_BUFFER_SIZE) {
					string AA=outInfo.substr(Start,OUTPUT_BUFFER_SIZE);
					compressDataWrite(AA.c_str(),OUTPUT_BUFFER_SIZE);
					Start+=OUTPUT_BUFFER_SIZE;
				}
				else {
					string AA=outInfo.substr(Start);
					compressDataWrite(AA.c_str(),AA.length());
				}
				Site=Site-OUTPUT_BUFFER_SIZE;
			}
		}

	private:
		libdeflate_compressor* m_compressor;
		std::ofstream m_outputFile;
		uint8_t m_outputBuffer[OUTPUT_BUFFER_SIZE];
};

*/ 

class DeflateCompress
{
	public:
		DeflateCompress( ) 
		{
			m_compressor = libdeflate_alloc_compressor(6);
		}

		~DeflateCompress()
		{
			libdeflate_free_compressor(m_compressor);
		}

		bool compressData(const void* input, size_t inputSize, uint8_t  ** m_outputBuffer, size_t & compressedSize, size_t & OUT_BUFFER_SIZE,int &  Thread) 
		{
			if (OUT_BUFFER_SIZE<0.8*inputSize)  //  压缩率一般高于80%的  
			{
				OUT_BUFFER_SIZE=inputSize;
				delete [] m_outputBuffer[Thread];
				m_outputBuffer[Thread]=new uint8_t[OUT_BUFFER_SIZE];
			}
			compressedSize = libdeflate_gzip_compress(m_compressor, input, inputSize, m_outputBuffer[Thread], OUT_BUFFER_SIZE);
			if (compressedSize == 0)
			{
				return false;
			}
			else
			{
				return true;
			}
		}

		bool compressData(const void* input, size_t inputSize, uint8_t  * m_outputBuffer, size_t & compressedSize) 
		{
			compressedSize = libdeflate_gzip_compress(m_compressor, input, inputSize, m_outputBuffer, OUTPUT_BUFFER_SIZE );
			if (compressedSize == 0) 
			{
				return false;
			}
			else
			{
				return true;
			}
		}

	private:
		libdeflate_compressor* m_compressor;
};

