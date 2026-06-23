#ifndef GZPIPELINE_H
#define GZPIPELINE_H

#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <libdeflate.h>

// Persistent thread-pool pipeline for gzip compression.
//   1 reader  : hands out fixed-size blocks in order (with backpressure)
//   N workers : reused threads, each owns ONE libdeflate compressor
//   1 writer  : writes compressed members in input order
// Two input modes share the same worker/writer machinery:
//   - stream mode : reader copies bytes from an istream into block buffers
//   - mmap mode   : reader hands workers pointers straight into mapped memory
//                   (zero-copy; no input buffers, no reader I/O)

class GzPipe
{
	public:
		// stream mode
		GzPipe(std::istream & in, std::ostream & out, int threads,
		       size_t blockSize, int level)
			: IN(&in), OUT(out), nThreads(threads < 1 ? 1 : threads),
			  BLK(blockSize), Level(level),
			  mapData(NULL), mapSize(0), useMmap(false) {}

		// mmap mode
		GzPipe(const uint8_t * data, size_t size, std::ostream & out,
		       int threads, size_t blockSize, int level)
			: IN(NULL), OUT(out), nThreads(threads < 1 ? 1 : threads),
			  BLK(blockSize), Level(level),
			  mapData(data), mapSize(size), useMmap(true) {}

		bool run()
		{
			const size_t bound = libdeflate_gzip_compress_bound(NULL, BLK);
			const int poolSize = nThreads * 4 + 2;

			pool.resize(poolSize);
			for (int i = 0; i < poolSize; i++)
			{
				if (!useMmap) pool[i].in.resize(BLK);
				pool[i].out.resize(bound);
				freeQ.push(&pool[i]);
			}

			if (useMmap)
			{
				useVec = true;
				size_t nBlk = (mapSize + BLK - 1) / BLK;
				doneVec.assign(nBlk, NULL);
			}

			std::vector<std::thread> workers;
			workers.reserve(nThreads);
			for (int i = 0; i < nThreads; i++)
			{
				workers.emplace_back(&GzPipe::worker, this);
			}
			std::thread writer(&GzPipe::writeLoop, this);

			if (useMmap) readMmap();
			else         readStream();

			{ std::lock_guard<std::mutex> lk(mWork); noMoreWork = true; }
			cvWork.notify_all();
			cvDone.notify_all();

			for (auto & t : workers) t.join();
			writer.join();

			if (totalBlocks.load() == 0)
			{
				libdeflate_compressor *c = libdeflate_alloc_compressor(Level);
				if (c)
				{
					uint8_t buf[32];
					size_t n = libdeflate_gzip_compress(c, buf, 0, buf, sizeof(buf));
					OUT.write((const char*)buf, (std::streamsize)n);
					libdeflate_free_compressor(c);
				}
			}

			OUT.flush();
			return !error.load();
		}

	private:
		struct Block
		{
			size_t index = 0;
			const char * src = NULL;   // bytes to compress (points into in[] or mmap)
			std::vector<char> in;      // used only in stream mode
			size_t inSize = 0;
			std::vector<uint8_t> out;
			size_t outSize = 0;
		};

		std::istream * IN;
		std::ostream & OUT;
		int nThreads;
		size_t BLK;
		int Level;
		const uint8_t * mapData;
		size_t mapSize;
		bool useMmap;

		std::vector<Block> pool;

		std::queue<Block *> freeQ;
		std::mutex mFree;
		std::condition_variable cvFree;

		std::queue<Block *> workQ;
		std::mutex mWork;
		std::condition_variable cvWork;
		bool noMoreWork = false;

		// For stream mode (unknown totalBlocks) — map for ordering by index.
		std::map<size_t, Block *> doneM;
		// For mmap mode (totalBlocks known up front) — O(1) vector, no heap per entry.
		std::vector<Block *> doneVec;
		bool useVec = false;
		std::mutex mDone;
		std::condition_variable cvDone;

		std::atomic<bool> readDone{false};
		std::atomic<bool> error{false};
		std::atomic<size_t> totalBlocks{0};

		Block * popFree()
		{
			std::unique_lock<std::mutex> lk(mFree);
			cvFree.wait(lk, [&]{ return !freeQ.empty(); });
			Block * b = freeQ.front();
			freeQ.pop();
			return b;
		}
		void pushFree(Block * b)
		{
			{ std::lock_guard<std::mutex> lk(mFree); freeQ.push(b); }
			cvFree.notify_one();
		}

		void pushWork(Block * b)
		{
			{ std::lock_guard<std::mutex> lk(mWork); workQ.push(b); }
			cvWork.notify_one();
		}
		Block * popWork()
		{
			std::unique_lock<std::mutex> lk(mWork);
			cvWork.wait(lk, [&]{ return !workQ.empty() || noMoreWork; });
			if (workQ.empty()) return NULL;
			Block * b = workQ.front();
			workQ.pop();
			return b;
		}

		void pushDone(Block * b)
		{
			std::lock_guard<std::mutex> lk(mDone);
			if (useVec) doneVec[b->index] = b;
			else        doneM[b->index] = b;
			cvDone.notify_one();
		}

		void readStream()
		{
			size_t idx = 0;
			while (true)
			{
				Block * b = popFree();
				IN->read(b->in.data(), (std::streamsize)BLK);
				b->inSize = (size_t)IN->gcount();
				if (b->inSize == 0) { pushFree(b); break; }
				b->src = b->in.data();
				b->index = idx++;
				pushWork(b);
			}
			totalBlocks.store(idx);
			readDone.store(true);
		}

		void readMmap()
		{
			size_t idx = 0;
			size_t off = 0;
			while (off < mapSize)
			{
				size_t len = mapSize - off;
				if (len > BLK) len = BLK;
				Block * b = popFree();
				b->src = (const char *)(mapData + off);
				b->inSize = len;
				b->index = idx++;
				pushWork(b);
				off += len;
			}
			totalBlocks.store(idx);
			readDone.store(true);
		}

		void worker()
		{
			libdeflate_compressor * c = libdeflate_alloc_compressor(Level);
			while (true)
			{
				Block * b = popWork();
				if (b == NULL) break;
				size_t n = libdeflate_gzip_compress(c, b->src, b->inSize,
				                                    b->out.data(), b->out.size());
				if (n == 0) error.store(true);
				b->outSize = n;
				pushDone(b);
			}
			libdeflate_free_compressor(c);
		}

		void writeLoop()
		{
			size_t expect = 0;
			while (true)
			{
				Block * b = NULL;
				{
					std::unique_lock<std::mutex> lk(mDone);
					cvDone.wait(lk, [&]{
						if (useVec)
							return (expect < doneVec.size() && doneVec[expect] != NULL) ||
							       (readDone.load() && expect >= totalBlocks.load());
						return doneM.count(expect) > 0 ||
						       (readDone.load() && expect >= totalBlocks.load());
					});
					if (useVec)
					{
						if (expect >= doneVec.size() || doneVec[expect] == NULL) break;
						b = doneVec[expect];
						doneVec[expect] = NULL;
					}
					else
					{
						auto it = doneM.find(expect);
						if (it == doneM.end()) break;
						b = it->second;
						doneM.erase(it);
					}
				}
				if (b->outSize > 0)
				{
					OUT.write((const char *)b->out.data(), (std::streamsize)b->outSize);
				}
				expect++;
				pushFree(b);
			}
		}
};

#endif
